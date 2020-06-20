#include "common.h"
#include "common_internal.h"

#include <unordered_map>

#pragma warning(push, 0)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#pragma push_macro("OS_WINDOWS")
#include <Shlwapi.h>
#pragma pop_macro("OS_WINDOWS")
#include <Psapi.h>
#include <strsafe.h>

#include <array>
#include <queue>
#include <stack>

#pragma comment(lib, "shlwapi")
#pragma warning(pop)

void dummy(void const *) {

}

static void (*pushWorkImpl)(WorkQueue *queue, void (*fn)(void *), void *param);
static void (*waitForWorkCompletionImpl)(WorkQueue *);

static void doWork(void (*fn)(void *), void *param, WorkQueue *queue) {
	fn(param);
	InterlockedDecrement(&queue->workToDo);
#if !ENG_WORK_USE_TEMP
	free(param);
#endif
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
		Optional<WorkEntry> entry;
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
					waitUntil([] { return tryDoWork() || stopWork; });
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
u32 getWorkerThreadCount() { return workerCount; }
void WorkQueue::push_(void (*fn)(void *), void *param) { return pushWorkImpl(this, fn, param); }
void WorkQueue::completeAllWork() { return waitForWorkCompletionImpl(this); }
bool WorkQueue::completed() { return workToDo == 0; }

struct CPUID {
	s32 eax;
	s32 ebx;
	s32 ecx;
	s32 edx;

	CPUID(s32 func) { __cpuid(data(), func); }
	CPUID(s32 func, s32 subfunc) { __cpuidex(data(), func, subfunc); }

	s32 *data() { return &eax; }
};

CacheType cvt(PROCESSOR_CACHE_TYPE v) {
	switch (v) {
		case PROCESSOR_CACHE_TYPE::CacheUnified: return CacheType::unified;
		case PROCESSOR_CACHE_TYPE::CacheInstruction: return CacheType::instruction;
		case PROCESSOR_CACHE_TYPE::CacheData: return CacheType::data;
		case PROCESSOR_CACHE_TYPE::CacheTrace: return CacheType::trace;
		default: INVALID_CODE_PATH();
	}
}

char const *toString(CacheType v) {
#define CASE(name) \
	case CacheType::name: return #name;
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
		ASSERT(err == ERROR_INSUFFICIENT_BUFFER, "GetLastError(): {}", err);
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

	s32 highestId = CPUID(0).eax;
	for (s32 i = 0; i <= highestId; ++i) {
		data.push_back(CPUID(i, 0));
	}
	if (data.size() > 0) {
		char vendorName[24];
		((s32 *)vendorName)[0] = data[0].ebx;
		((s32 *)vendorName)[1] = data[0].edx;
		((s32 *)vendorName)[2] = data[0].ecx;
		if (startsWith(vendorName, 24, "GenuineIntel")) {
			result.vendor = CpuVendor::Intel;
		} else if (startsWith(vendorName, 24, "AuthenticAMD")) {
			result.vendor = CpuVendor::AMD;
		}
	}
	if (data.size() > 1) {
		ecx1 = data[1].ecx;
		edx1 = data[1].edx;
	}
	if (data.size() > 7) {
		ebx7 = data[7].ebx;
		ecx7 = data[7].ecx;
	}

	s32 highestExId = CPUID(0x80000000).eax;
	for (s32 i = 0x80000000; i <= highestExId; ++i) {
		dataEx.push_back(CPUID(i, 0));
	}
	if (dataEx.size() > 1) {
		ecx1ex = dataEx[1].ecx;
		edx1ex = dataEx[1].edx;
	}
	if (dataEx.size() > 4) {
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

s64 File::setPointer(s64 distance, SeekFrom startPoint) {
	LARGE_INTEGER result, dist;
	dist.QuadPart = distance;
	if (!SetFilePointerEx(handle, dist, &result, (DWORD)startPoint)) {
		return -1;
	}
	return result.QuadPart;
}

s64 File::getPointer() {
	LARGE_INTEGER result;
	if (!SetFilePointerEx(handle, {}, &result, SEEK_CUR)) {
		return -1;
	}
	return result.QuadPart;
}

bool File::read(void *buffer, umm _size) {
	s64 size = (s64)_size;
	DWORD const maxSize = 0xFFFFFFFF;
	for (;;) {
		DWORD bytesRead = 0;
		if (size > maxSize) {
			if (!ReadFile(handle, buffer, maxSize, &bytesRead, 0)) {
				return false;
			}
			if (bytesRead == maxSize) {
				buffer = (u8 *)buffer + maxSize;
				size -= maxSize;
				continue;
			}
		} else if (size > 0) {
			if (!ReadFile(handle, buffer, (DWORD)size, &bytesRead, 0)) {
				return false;
			}
		}
		break;
	}
	return true;
}
bool File::write(void const *buffer, umm _size) {
	s64 size = (s64)_size;
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
bool File::writeLine() {
#if OS_WINDOWS
	return write("\r\n", 2);
#else
	return write("\n", 1);
#endif
}
File::File(char const* path, u32 mode) {
	handle = INVALID_HANDLE_VALUE;
	DWORD creationDisposition = (DWORD)((mode & File::OpenMode_keep) ? CREATE_NEW : CREATE_ALWAYS);
	if (!(mode & File::OpenMode_write)) {
		if (mode & File::OpenMode_read) {
			creationDisposition = OPEN_EXISTING;
		} else {
			CreateFileA(path, 0, FILE_SHARE_READ, 0, creationDisposition, FILE_ATTRIBUTE_NORMAL, 0);
			return;
		}
	}
	DWORD desiredAccess = mode & (File::OpenMode_read | File::OpenMode_write);
	handle = CreateFileA(path, desiredAccess, FILE_SHARE_READ, 0, creationDisposition, FILE_ATTRIBUTE_NORMAL, 0);
}
void File::close() {
	if (valid()) {
		CloseHandle(handle);
		handle = INVALID_HANDLE_VALUE;
	}
}

bool File::valid() {
	return handle != INVALID_HANDLE_VALUE;
}

EntireFile readEntireFile(File file) {
	EntireFile result{};
	result.valid = true;

	file.setPointer(0, SeekFrom::end);
	umm size = (umm)file.getPointer();
	if (!size) {
		return result;
	}
	file.setPointer(0, SeekFrom::begin);

	auto data = malloc(size);
	if (file.read(data, size)) {
		result.data = {(char *)data, size};
	} else {
		free(data);
		result.valid = false;
	}
	
	return result;
}

EntireFile readEntireFile(char const *path) {
	File file(path, File::OpenMode_read);
	DEFER { file.close(); };
	if (!file.valid()) {
		return {};
	}
	return readEntireFile(file);
}

void freeEntireFile(EntireFile file) { 
	if (file.valid) {
		free(file.data.begin()); 
	}
}

bool fileExists(char const *path) { return PathFileExistsA(path); }

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
	stats.startUs = stats.totalUs = (u64)PerfTimer::getCounter();
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
	result.selfUs = result.startUs = result.totalUs = (u64)PerfTimer::getCounter();

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
			stat.selfUs = (u64)PerfTimer::getMicroseconds<f64>((s64)stat.selfUs);
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
	stats.startUs = stats.totalUs = (u64)PerfTimer::getCounter();
}
} // namespace Profiler

void setCursorVisibility(bool visible) {
	if (visible)
		while (ShowCursor(visible) < 0) {}
	else
		while (ShowCursor(visible) >= 0) {}
}

namespace Log {

HANDLE handle;

void setHandle(void *newHandle) {
	handle = (HANDLE)newHandle;
}

void _print(Span<char const> msg, Span<char const> moduleName) {
	auto result = format<OsAllocator>("[{}] {}\n", moduleName, msg);
	DWORD charsWritten;
	WriteConsoleA(handle, result.data(), (DWORD)result.size(), &charsWritten, 0);
}
void setColor(Color c) {
	SetConsoleTextAttribute(handle, (WORD)c);
}

} // namespace Log

#if 1
static constexpr u32 tempStoragePerThread = 1024 * 1024 * 16;
struct TempStorage {
	u8 *top = data;
	u8 data[tempStoragePerThread];
	TempStorage(){
		memset(data, 'C', sizeof(data));
	}
};

std::unordered_map<DWORD, TempStorage> threadStorages;
std::mutex threadStorageMutex;

void *allocateTemp(u32 size, u32 align) {
	align = max(align, 8u);
	if (!isPowerOf2(align)) {
		FATAL_CODE_PATH("align is not a power of two");
	}

	threadStorageMutex.lock();
	TempStorage &storage = threadStorages[GetCurrentThreadId()];
	threadStorageMutex.unlock();
	
	void *result = ceil(storage.top, align);
	storage.top = (u8 *)result + size;
	if (storage.top > storage.data + tempStoragePerThread) {
		FATAL_CODE_PATH("temp storage overflow");
	}
	return result;
}
void resetTempStorage() { 
	for (auto &[threadId, storage] : threadStorages)
		storage.top = storage.data;
}
u32 getTempMemoryUsage() {
	u32 result = 0;
	for (auto &[threadId, storage] : threadStorages)
		result += (u32)(storage.top - storage.data);
	return result;
}
#else
static constexpr u32 tempStorageSize = 1024 * 1024 * 64;
// TODO: this should be u8 but for some reason trying to write in the middle 
static u8 tempStorage[tempStorageSize];
static u32 volatile tempStorageOffset = 0;

void *allocateTemp(u32 size, u32 align) {
	align = max(align, 8u);
	u32 resultOffset = lockAdd(tempStorageOffset, size);
	ASSERT(resultOffset + size <= tempStorageSize);
	return tempStorage + resultOffset;
}
void resetTempStorage() { 
	tempStorageOffset = 0;
}
u32 getTempMemoryUsage() {
	return tempStorageOffset;
}
#endif

u64 getMemoryUsage() {
	u64 result = 0;

	PROCESS_MEMORY_COUNTERS memoryCounters;
	if (GetProcessMemoryInfo(GetCurrentProcess(), &memoryCounters, sizeof(memoryCounters))) {
		result = memoryCounters.PagefileUsage;
	}
	return result;
}

Span<char const> timeUnitToString(u16 unit) {
	switch (unit) {
		case 0: return {"ns", 2};
		case 1: return {"us", 2};
		case 2: return {"ms", 2};
		case 3: return {"s", 1};
		case 4: return {"m", 1};
		case 5: return {"h", 1};
		case 6: return {"d", 1};
		default: INVALID_CODE_PATH();
	}
}

constexpr u32 timeDivThreshold = 10;
ConvertedTime cvtNanoseconds (u64  nanoseconds){return  nanoseconds >= timeDivThreshold * 1000 ? cvtMicroseconds( nanoseconds / 1000) : ConvertedTime{(u16) nanoseconds, 0}; }
ConvertedTime cvtMicroseconds(u64 microseconds){return microseconds >= timeDivThreshold * 1000 ? cvtMilliseconds(microseconds / 1000) : ConvertedTime{(u16)microseconds, 1}; }
ConvertedTime cvtMilliseconds(u64 milliseconds){return milliseconds >= timeDivThreshold * 1000 ? cvtSeconds     (milliseconds / 1000) : ConvertedTime{(u16)milliseconds, 2}; }
ConvertedTime cvtSeconds     (u64      seconds){return      seconds >= timeDivThreshold *   60 ? cvtMinutes     (     seconds /   60) : ConvertedTime{(u16)     seconds, 3}; }
ConvertedTime cvtMinutes     (u64      minutes){return      minutes >= timeDivThreshold *   60 ? cvtHours       (     minutes /   60) : ConvertedTime{(u16)     minutes, 4}; }
ConvertedTime cvtHours       (u64        hours){return        hours >= timeDivThreshold *   24 ? ConvertedTime{(u16)(hours / 24), 6}  : ConvertedTime{(u16)       hours, 5}; }

Span<char const> byteUnitToString(u16 unit) {
	switch (unit) {
		case 0: return {"B", 1};
		case 1: return {"KB", 2};
		case 2: return {"MB", 2};
		case 3: return {"GB", 2};
		case 4: return {"TB", 2};
		case 5: return {"PB", 2};
		case 6: return {"EB", 2};
		default: INVALID_CODE_PATH();
	}
}

constexpr u32 byteDivThreshold = 10;
ConvertedBytes cvtBytes(u64 bytes) {
	ConvertedBytes result{};
	while (bytes >= byteDivThreshold * 1024) { bytes /= 1024; ++result.unit; }
	result.value = (u16)bytes;
	return result;
}

HMODULE optimizedModule;
void *getOptimizedProcAddress(char const *name) {
	void *result = GetProcAddress(optimizedModule, name);
	ASSERT(result);
	return result;
}
Span<char const> optimizedModuleName;
Span<char const> getOptimizedModuleName() {
	return optimizedModuleName;
}
void loadOptimizedModule() {
	char const *optimizedModuleFileName;
	if (cpuInfo.hasFeature(ProcessorFeature::AVX2)) {
		optimizedModuleFileName = "o_avx2.dll";
		optimizedModuleName = "AVX2";
	} else if (cpuInfo.hasFeature(ProcessorFeature::AVX)) {
		optimizedModuleFileName = "o_avx.dll";
		optimizedModuleName = "AVX";
	} else {
		optimizedModuleFileName = "o_sse.dll";
		optimizedModuleName = "SSE";
	}

	optimizedModule = LoadLibraryA(optimizedModuleFileName);
	ASSERT(optimizedModule, "Failed to load optimized module ({})", optimizedModuleFileName);
}

ENG_API Span<char const> _getModuleName(void *imageBase) {
	HMODULE module = (HMODULE)imageBase;

	char modulePath[256];
	DWORD modulePathSize = GetModuleFileNameA(module, modulePath, sizeof(modulePath));

	char *nameBegin = modulePath + modulePathSize;
	char *nameEnd = nameBegin;
	while (nameBegin > modulePath && *nameBegin != '\\') {
		if (*nameBegin == '.') {
			nameEnd = nameBegin;
			*nameEnd = '\0';
		}
		--nameBegin;
	}
	if (nameBegin != modulePath) {
		++nameBegin;
	}
	auto moduleNameLength = (umm)(nameEnd - nameBegin);
	char *result = (char *)malloc(moduleNameLength);
	memcpy(result, nameBegin, moduleNameLength);
	return Span{result, result + moduleNameLength};
}
