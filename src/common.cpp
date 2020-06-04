#include "common.h"
#include "common.h"
#include "common_internal.h"
#include "../dep/tl/include/tl/thread.h"

#include <unordered_map>

#pragma warning(push, 0)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#pragma push_macro("OS_WINDOWS")
#include <Shlwapi.h>
#pragma pop_macro("OS_WINDOWS")
#include <strsafe.h>

#include <array>
#include <queue>
#include <stack>

#pragma comment(lib, "shlwapi")
#pragma warning(pop)

static void (*pushWorkImpl)(WorkQueue *queue, void (*fn)(void *), void *param);
static void (*waitForWorkCompletionImpl)(WorkQueue *);

static void doWork(void (*fn)(void *), void *param, WorkQueue *queue) {
	fn(param);
	InterlockedDecrement(&queue->workToDo);
}

struct WorkEntry {
	WorkQueue *queue;
	void (*function)(void *param);
	void *param;
};

struct SharedWorkQueue {
	std::queue<WorkEntry> queue;
	std::mutex mutex;
	void push(WorkEntry &&val) {
		mutex.lock();
		queue.push(std::move(val));
		mutex.unlock();
	}
	auto try_pop() {
		std::optional<WorkEntry> entry;
		mutex.lock();
		if (queue.size()) {
			entry.emplace(std::move(queue.front()));
			queue.pop();
		}
		mutex.unlock();
		return entry;
	}
};

static SharedWorkQueue sharedWorkQueue;
static u32 workerCount = 0;
static bool volatile stopWork = false;
static u32 volatile deadWorkers = 0;
static u32 volatile initializedWorkers = 0;
static std::unordered_map<DWORD, u32> threadIdMap;
static bool tryDoWork() {
	if (auto entry = sharedWorkQueue.try_pop()) {
		doWork(entry->function, entry->param, entry->queue);
		return true;
	}
	return false;
}
void initWorkerThreads(u32 threadCount) {
	workerCount = threadCount;
	threadIdMap[GetCurrentThreadId()] = 0;
	if (threadCount == 0) {
		pushWorkImpl = [](WorkQueue *queue, void (*fn)(void *), void *param) { doWork(fn, param, queue); };
		waitForWorkCompletionImpl = [](WorkQueue *queue) {};
	} else {
		waitForWorkCompletionImpl = [](WorkQueue *queue) { 
			waitUntil([queue] { 
				tryDoWork();
				return queue->completed(); 
			}); 
		};
		std::mutex threadIdMapMutex;
		for (u32 threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
			std::thread([&threadIdMapMutex, threadIndex]() {
				threadIdMapMutex.lock();
				threadIdMap[GetCurrentThreadId()] = threadIndex + 1;
				threadIdMapMutex.unlock();
				InterlockedIncrement(&initializedWorkers);
				for (;;) {
					waitUntil([]{ return tryDoWork() || stopWork; });
					if (stopWork)
						break;
				}
				InterlockedIncrement(&deadWorkers);
			}).detach();
		}
		pushWorkImpl = [](WorkQueue *queue, void (*fn)(void *), void *param) {
			sharedWorkQueue.push({queue, fn, param});
			InterlockedIncrement(&queue->workToDo);
		};
	}
	waitUntil([threadCount] { return initializedWorkers == threadCount; });
}
void shutdownWorkerThreads() {
	stopWork = true;
	waitUntil([] { return deadWorkers == workerCount; });
}
u32 getWorkerThreadCount() {
	return workerCount;
}
void WorkQueue::push_(void (*fn)(void *), void *param) { return pushWorkImpl(this, fn, param); }
void WorkQueue::completeAllWork() { return waitForWorkCompletionImpl(this); }
bool WorkQueue::completed() {
	return workToDo == 0;
}

struct CPUID {
	s32 eax;
	s32 ebx;
	s32 ecx;
	s32 edx;
	
	CPUID(s32 func) {
		__cpuid(data(), func);
	}

	CPUID(s32 func, s32 subfunc) {
		__cpuidex(data(), func, subfunc);
	}

	s32 *data() { return &eax; }
};

CacheType cvt(PROCESSOR_CACHE_TYPE v) {
	switch (v) {
		case PROCESSOR_CACHE_TYPE::CacheUnified:	 return CacheType::unified;
		case PROCESSOR_CACHE_TYPE::CacheInstruction: return CacheType::instruction;
		case PROCESSOR_CACHE_TYPE::CacheData:		 return CacheType::data;
		case PROCESSOR_CACHE_TYPE::CacheTrace:		 return CacheType::trace;
		default: INVALID_CODE_PATH();
	}
}

char const *toString(CacheType v) {
#define CASE(name) case CacheType::name: return #name;
	switch (v) {
		CASE(data)
		CASE(instruction)
		CASE(trace)
		CASE(unified)
		default: return "Unknown";
	}
#undef CASE
}

static CpuInfo getCpuInfo() {
	CpuInfo result{};

	DWORD processorInfoLength = 0;
	if (!GetLogicalProcessorInformation(0, &processorInfoLength)) {
		DWORD err = GetLastError();
		ASSERT(err == ERROR_INSUFFICIENT_BUFFER, "GetLastError(): %u", err);
	}

	auto buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)malloc(processorInfoLength);
	DEFER { free(buffer); };

	if (!GetLogicalProcessorInformation(buffer, &processorInfoLength))
		INVALID_CODE_PATH("GetLogicalProcessorInformation: %u", GetLastError());

	ASSERT(processorInfoLength % sizeof(buffer[0]) == 0);
	for (auto &info : Span{buffer, processorInfoLength / sizeof(buffer[0])}) {
		switch (info.Relationship) {
			case RelationNumaNode:
			case RelationProcessorPackage: break;
			case RelationProcessorCore: result.logicalProcessorCount += countBits(info.ProcessorMask); break;
			case RelationCache: {
				auto &cache = result.cache[info.Cache.Level - 1][(u8)cvt(info.Cache.Type)];
				cache.size += info.Cache.Size; 
				++cache.count;
				result.cacheLineSize = info.Cache.LineSize;
				break;
			}
			default: INVALID_CODE_PATH("Error: Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value.");
		}
	}

	s32 ecx1 = 0;
	s32 edx1 = 0;
	s32 ebx7 = 0;
	s32 ecx7 = 0;
	s32 ecx1ex = 0;
	s32 edx1ex = 0;
	StaticList<CPUID, 64> data;
	StaticList<CPUID, 64> dataEx;

	// Calling __cpuid with 0x0 as the function_id argument
	// gets the number of the highest valid function ID.
	s32 nIds = CPUID(0).eax;
	//data.reserve((umm)nIds);

	for (s32 i = 0; i <= nIds; ++i) {
		data.push_back(CPUID(i, 0));
	}

	ASSERT(data.size() > 0);

	char vendorName[24];
	((s32 *)vendorName)[0] = data[0].ebx;
	((s32 *)vendorName)[1] = data[0].edx;
	((s32 *)vendorName)[2] = data[0].ecx;
	if (startsWith(vendorName, 24, "GenuineIntel")) {
		result.vendor = CpuVendor::Intel;
	} else if (startsWith(vendorName, 24, "AuthenticAMD")) {
		result.vendor = CpuVendor::AMD;
	}

	// load bitset with flags for function 0x00000001
	if (nIds >= 1) {
		ecx1 = data[1].ecx;
		edx1 = data[1].edx;
	}

	// load bitset with flags for function 0x00000007
	if (nIds >= 7) {
		ebx7 = data[7].ebx;
		ecx7 = data[7].ecx;
	}

	// Calling __cpuid with 0x80000000 as the function_id argument
	// gets the number of the highest valid extended ID.
	s32 nExIds_ = CPUID(0x80000000).eax;

	for (s32 i = 0x80000000; i <= nExIds_; ++i) {
		dataEx.push_back(CPUID(i, 0));
	}

	// load bitset with flags for function 0x80000001
	if (nExIds_ >= 0x80000001) {
		ecx1ex = dataEx[1].ecx;
		edx1ex = dataEx[1].edx;
	}

	// Interpret CPU brand string if reported
	if (nExIds_ >= 0x80000004) {
		result.brand[48] = 0;
		memcpy(result.brand + sizeof(CPUID) * 0, dataEx[2].data(), sizeof(CPUID));
		memcpy(result.brand + sizeof(CPUID) * 1, dataEx[3].data(), sizeof(CPUID));
		memcpy(result.brand + sizeof(CPUID) * 2, dataEx[4].data(), sizeof(CPUID));
	} else {
		result.brand[0] = 0;
	}

	auto set = [&](ProcessorFeature feature, bool value) {
		CpuInfo::FeatureIndex index = cpuInfo.getFeaturePos(feature);
		result.features[index.slot] |= (u32)value << index.bit;
	};

	// clang-format off
	set(ProcessorFeature::SSE3,			(ecx1	& (1 << 0 )));
    set(ProcessorFeature::PCLMULQDQ,	(ecx1	& (1 << 1 )));
    set(ProcessorFeature::MONITOR,		(ecx1	& (1 << 3 )));
    set(ProcessorFeature::SSSE3,		(ecx1	& (1 << 9 )));
    set(ProcessorFeature::FMA,			(ecx1	& (1 << 12)));
    set(ProcessorFeature::CMPXCHG16B,	(ecx1	& (1 << 13)));
    set(ProcessorFeature::SSE41,		(ecx1	& (1 << 19)));
    set(ProcessorFeature::SSE42,		(ecx1	& (1 << 20)));
    set(ProcessorFeature::MOVBE,		(ecx1	& (1 << 22)));
    set(ProcessorFeature::POPCNT,		(ecx1	& (1 << 23)));
    set(ProcessorFeature::AES,			(ecx1	& (1 << 25)));
    set(ProcessorFeature::XSAVE,		(ecx1	& (1 << 26)));
    set(ProcessorFeature::OSXSAVE,		(ecx1	& (1 << 27)));
    set(ProcessorFeature::AVX,			(ecx1	& (1 << 28)));
    set(ProcessorFeature::F16C,			(ecx1	& (1 << 29)));
    set(ProcessorFeature::RDRAND,		(ecx1	& (1 << 30)));
    set(ProcessorFeature::MSR,			(edx1	& (1 << 5 )));
    set(ProcessorFeature::CX8,			(edx1	& (1 << 8 )));
    set(ProcessorFeature::SEP,			(edx1	& (1 << 11)));
    set(ProcessorFeature::CMOV,			(edx1	& (1 << 15)));
    set(ProcessorFeature::CLFSH,		(edx1	& (1 << 19)));
    set(ProcessorFeature::MMX,			(edx1	& (1 << 23)));
    set(ProcessorFeature::FXSR,			(edx1	& (1 << 24)));
    set(ProcessorFeature::SSE,			(edx1	& (1 << 25)));
    set(ProcessorFeature::SSE2,			(edx1	& (1 << 26)));
    set(ProcessorFeature::FSGSBASE,		(ebx7	& (1 << 0 )));
    set(ProcessorFeature::BMI1,			(ebx7	& (1 << 3 )));
    set(ProcessorFeature::HLE,			(ebx7	& (1 << 4 )) && result.vendor == CpuVendor::Intel);
    set(ProcessorFeature::AVX2,			(ebx7	& (1 << 5 )));
    set(ProcessorFeature::BMI2,			(ebx7	& (1 << 8 )));
    set(ProcessorFeature::ERMS,			(ebx7	& (1 << 9 )));
    set(ProcessorFeature::INVPCID,		(ebx7	& (1 << 10)));
    set(ProcessorFeature::RTM,			(ebx7	& (1 << 11)) && result.vendor == CpuVendor::Intel);
    set(ProcessorFeature::AVX512F,		(ebx7	& (1 << 16)));
    set(ProcessorFeature::RDSEED,		(ebx7	& (1 << 18)));
    set(ProcessorFeature::ADX,			(ebx7	& (1 << 19)));
    set(ProcessorFeature::AVX512PF,		(ebx7	& (1 << 26)));
    set(ProcessorFeature::AVX512ER,		(ebx7	& (1 << 27)));
    set(ProcessorFeature::AVX512CD,		(ebx7	& (1 << 28)));
    set(ProcessorFeature::SHA,			(ebx7	& (1 << 29)));
    set(ProcessorFeature::PREFETCHWT1,  (ecx7	& (1 << 0 )));
    set(ProcessorFeature::LAHF,			(ecx1ex	& (1 << 0 )));
    set(ProcessorFeature::LZCNT,		(ecx1ex	& (1 << 5 )) && result.vendor == CpuVendor::Intel);
    set(ProcessorFeature::ABM,			(ecx1ex	& (1 << 5 )) && result.vendor == CpuVendor::AMD);
    set(ProcessorFeature::SSE4a,		(ecx1ex	& (1 << 6 )) && result.vendor == CpuVendor::AMD);
    set(ProcessorFeature::XOP,			(ecx1ex	& (1 << 11)) && result.vendor == CpuVendor::AMD);
    set(ProcessorFeature::TBM,			(ecx1ex	& (1 << 21)) && result.vendor == CpuVendor::AMD);
    set(ProcessorFeature::SYSCALL,		(edx1ex	& (1 << 11)) && result.vendor == CpuVendor::Intel);
    set(ProcessorFeature::MMXEXT,		(edx1ex	& (1 << 22)) && result.vendor == CpuVendor::AMD);
    set(ProcessorFeature::RDTSCP,		(edx1ex	& (1 << 27)) && result.vendor == CpuVendor::Intel);
    set(ProcessorFeature::_3DNOWEXT,	(edx1ex	& (1 << 30)) && result.vendor == CpuVendor::AMD);
    set(ProcessorFeature::_3DNOW,		(edx1ex	& (1 << 31)) && result.vendor == CpuVendor::AMD);
	// clang-format on

	return result;
}

CpuInfo const cpuInfo = getCpuInfo();

char const *toString(ProcessorFeature f) {
#define CASE(name) \
	case ProcessorFeature::name: return #name;
	switch (f) {
		CASE(_3DNOW)
		CASE(_3DNOWEXT)
		CASE(ABM)
		CASE(ADX)
		CASE(AES)
		CASE(AVX)
		CASE(AVX2)
		CASE(AVX512CD)
		CASE(AVX512ER)
		CASE(AVX512F)
		CASE(AVX512PF)
		CASE(BMI1)
		CASE(BMI2)
		CASE(CLFSH)
		CASE(CMOV)
		CASE(CMPXCHG16B)
		CASE(CX8)
		CASE(ERMS)
		CASE(F16C)
		CASE(FMA)
		CASE(FSGSBASE)
		CASE(FXSR)
		CASE(HLE)
		CASE(INVPCID)
		CASE(LAHF)
		CASE(LZCNT)
		CASE(MMX)
		CASE(MMXEXT)
		CASE(MONITOR)
		CASE(MOVBE)
		CASE(MSR)
		CASE(OSXSAVE)
		CASE(PCLMULQDQ)
		CASE(POPCNT)
		CASE(PREFETCHWT1)
		CASE(RDRAND)
		CASE(RDSEED)
		CASE(RDTSCP)
		CASE(RTM)
		CASE(SEP)
		CASE(SHA)
		CASE(SSE)
		CASE(SSE2)
		CASE(SSE3)
		CASE(SSE41)
		CASE(SSE42)
		CASE(SSE4a)
		CASE(SSSE3)
		CASE(SYSCALL)
		CASE(TBM)
		CASE(XOP)
		CASE(XSAVE)
	}
	return "Unknown";
}
char const *toString(CpuVendor vendor) {
	switch (vendor) {
		case CpuVendor::Intel: return "Intel";
		case CpuVendor::AMD: return "AMD";
		default: return "Unknown";
	}
}

#if OS_WINDOWS
s64 const PerfTimer::frequency = []() {
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	return freq.QuadPart;
}();
s64 PerfTimer::getCounter() {
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	return counter.QuadPart;
}
s64 PerfTimer::syncWithTime(s64 beginPC, f32 duration) {
	s64 endCounter = PerfTimer::getCounter();
	auto secondsElapsed = PerfTimer::getSeconds(beginPC, endCounter);
	if (secondsElapsed < duration) {
		s32 msToSleep = (s32)((duration - secondsElapsed) * 1000.0f);
		if (msToSleep > 0) {
			Sleep((DWORD)msToSleep);
		}
		auto targetCounter = beginPC + s64(duration * PerfTimer::frequency);
		while (1) {
			endCounter = PerfTimer::getCounter();
			if (endCounter >= targetCounter)
				break;
		}
	}
	return endCounter;
}
#else
#error no timer
#endif

s64 FileHandle::setPointer(s64 distance, SeekFrom startPoint) {
	LARGE_INTEGER result, dist;
	dist.QuadPart = distance;
	if (!SetFilePointerEx(handle, dist, &result, (DWORD)startPoint)) {
		return -1;
	}
	return result.QuadPart;
}

s64 FileHandle::getPointer() {
	LARGE_INTEGER result;
	if (!SetFilePointerEx(handle, {}, &result, SEEK_CUR)) {
		return -1;
	}
	return result.QuadPart;
}

bool FileHandle::read(void *buffer, size_t size) {
	DWORD const maxSize = 0xFFFFFFFF;
	for (;;) {
		DWORD bytesRead = 0;
		if (size > maxSize) {
			if (!ReadFile(handle, buffer, maxSize, &bytesRead, 0)) {
				return false;
			}
			buffer = (u8 *)buffer + maxSize;
			size -= maxSize;
		} else {
			if (!ReadFile(handle, buffer, (DWORD)size, &bytesRead, 0)) {
				return false;
			}
			break;
		}
	}
	return true;
}
bool FileHandle::write(void const *buffer, size_t size) {
	DWORD const maxSize = 0xFFFFFFFF;
	for (;;) {
		DWORD bytesWritten = 0;
		if (size > maxSize) {
			if (!WriteFile(handle, buffer, maxSize, &bytesWritten, 0)) {
				return false;
			}
			buffer = (u8 *)buffer + maxSize;
			size -= maxSize;
		} else {
			if (!WriteFile(handle, buffer, (DWORD)size, &bytesWritten, 0)) {
				return false;
			}
			break;
		}
	}
	return true;
}

StringSpan readEntireFile(FileHandle file) {
	LARGE_INTEGER endPtr{};
	if (!SetFilePointerEx(file.handle, {}, &endPtr, FILE_END)) {
		return {};
	}
	if (!SetFilePointerEx(file.handle, {}, 0, FILE_BEGIN)) {
		return {};
	}
	size_t size = (size_t)endPtr.QuadPart;
	auto data = malloc(size);
	if (!file.read(data, size)) {
		free(data);
		return {};
	}
	return {(char *)data, size};
}

#define READ_FILE(char, CreateFileA)                                                                                  \
	StringSpan readEntireFile(char const *path) {                                                                     \
		HANDLE handle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0); \
		if (handle == INVALID_HANDLE_VALUE) {                                                                         \
			return {};                                                                                                \
		}                                                                                                             \
		DEFER { CloseHandle(handle); };                                                                               \
		return readEntireFile(FileHandle{handle});                                                                    \
	}

READ_FILE(char, CreateFileA);
READ_FILE(wchar, CreateFileW);

#undef READ_FILE

void freeEntireFile(StringSpan file) { free(file.begin()); }

bool fileExists(char const* path) {
	return PathFileExistsA(path);
}

namespace Profiler {
#define MAX_PROFILER_ENTRY_COUNT 1024
static Stats stats;
struct ThreadCallStack {
	List<Stat> calls;
};
static List<ThreadCallStack> threadStacks;
static std::mutex mutex;
void init(u32 totalThreadCount) {
	threadStacks.resize(totalThreadCount);
	for (auto &stack : threadStacks)
		stack.calls.reserve(512);
	stats.entries.resize(totalThreadCount);
	for (auto &l : stats.entries)
		l.reserve(256);
	stats.startUs = 
	stats.totalUs = (u64)PerfTimer::getCounter();
}
u32 getThreadId() {
	auto it = threadIdMap.find(GetCurrentThreadId());
	if (it == threadIdMap.end())
		return ~0u;
	return it->second;
}
void start(char const *name) {
	u32 threadId = getThreadId();
	if (threadId == ~0)
		return;

	Stat result;
	result.depth = (u16)threadStacks[threadId].calls.size();
	result.name = name;
	result.selfUs = 
	result.startUs =
	result.totalUs = (u64)PerfTimer::getCounter();

	std::scoped_lock _l(mutex);
	threadStacks[threadId].calls.push_back(std::move(result));
}
void stop() {
	u32 threadId = getThreadId();
	if (threadId == ~0)
		return;
	std::scoped_lock _l(mutex);

	Stat result = std::move(threadStacks[threadId].calls.back());
	threadStacks[threadId].calls.pop_back();
	auto nowNs = (u64)PerfTimer::getCounter();
	result.selfUs = nowNs - result.selfUs;
	result.totalUs = nowNs - result.totalUs;
	for (auto &call : threadStacks[threadId].calls) {
		call.selfUs += result.selfUs;
	}
	stats.entries[threadId].push_back(std::move(result));
}
Stats getStats() {
	for (auto &l : stats.entries) {
		for (auto &stat : l) {
			stat.startUs = (u64)PerfTimer::getMicroseconds<f64>((s64)stat.startUs);
			stat.totalUs = (u64)PerfTimer::getMicroseconds<f64>((s64)stat.totalUs);
			stat.selfUs = (u64)PerfTimer::getMicroseconds<f64> ((s64)stat.selfUs);
		}
	}
	stats.startUs = (u64)PerfTimer::getMicroseconds<f64>((s64)stats.startUs);
	stats.totalUs = (u64)PerfTimer::getMicroseconds<f64>((s64)stats.totalUs, PerfTimer::getCounter());
	return stats;
}
void reset() {
	for (auto &l : stats.entries)
		l.clear();
	for (auto &l : threadStacks)
		l.calls.clear();
	stats.startUs = 
	stats.totalUs = (u64)PerfTimer::getCounter();
}
} // namespace Profiler

void setCursorVisibility(bool visible) {
	if (visible)
		while (ShowCursor(visible) < 0);
	else
		while (ShowCursor(visible) >= 0);
}

namespace Log {
void print(char const *msg) { puts(msg); }
} // namespace Log
