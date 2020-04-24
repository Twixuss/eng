#include "common.h"
#include "../dep/tl/include/tl/thread.h"

#pragma warning(push, 0)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <strsafe.h>

#include <array>
#include <queue>
#include <stack>
#pragma warning(pop)

template <class T, class... Args>
void construct(T &val, Args &&... args) {
	new (&val) decltype(val)(std::forward<Args>(args)...);
}

template <class T>
void destruct(T &val) {
	val.~T();
}

template <class Pred>
void spinlock(Pred pred, bool volatile *stop = 0) {
	u32 miss = 0;
	while (stop ? !*stop : true) {
		if (pred()) {
			break;
		}
		++miss;
		if (miss >= 64) {
			SwitchToThread();
		} else if (miss >= 16) {
			_mm_pause();
		}
	}
}

struct WorkEntry {
	void (*function)(void *param);
	void *param;
	char const *name;
};

struct SPMC_WorkQueue {
	std::queue<WorkEntry> queue;
	std::mutex mutex;
	void push(WorkEntry &&val) {
		mutex.lock();
		queue.push(std::move(val));
		mutex.unlock();
	}
	auto pop() {
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

struct WorkQueue {
	SPSC::CircularQueue<WorkEntry, 1024> single;
	SPMC_WorkQueue multi;
};
static WorkQueue workQueue;
static void (*pushWorkImpl)(void (*fn)(void *), void *param, char const *name);
static void (*waitForWorkCompletionImpl)();

static List<List<ThreadStat>> threadStats;
void resetThreadStats() {
	for (auto &stat : threadStats)
		stat.clear();
}
List<List<ThreadStat>> &getThreadStats() { return threadStats; }
static u32 volatile workToDo = 0;

void doWork(u32 threadIndex, void (*fn)(void *), void *param, char const *name) {
	ThreadStat stat;
	stat.workName = name;
	stat.cycles = __rdtsc();
	fn(param);
	stat.cycles = __rdtsc() - stat.cycles;
	threadStats[threadIndex].push_back(stat);
	InterlockedDecrement(&workToDo);
}

static u32 workerCount = 0;
static bool volatile stopWork = false;
static u32 volatile deadWorkers = 0;
void initWorkerThreads(u32 count) {
	PROFILE_FUNCTION;
	workerCount = count;
	threadStats.resize(max(count, 1u));
	if (count == 0) {
		pushWorkImpl = [](void (*fn)(void *), void *param, char const *name) { doWork(0, fn, param, name); };
		waitForWorkCompletionImpl = []() {};
	} else {
		waitForWorkCompletionImpl = []() { spinlock([] { return workToDo == 0; }); };
		if (count == 1) {
			std::thread([]() {
				for (;;) {
					spinlock(
						[] {
							if (auto entry = workQueue.single.pop()) {
								doWork(0, entry->function, entry->param, entry->name);
								return true;
							}
							return false;
						},
						&stopWork);
					if (stopWork)
						break;
				}
				InterlockedIncrement(&deadWorkers);
			}).detach();
			pushWorkImpl = [](void (*fn)(void *), void *param, char const *name) {
				workQueue.single.push({fn, param, name});
				InterlockedIncrement(&workToDo);
			};
		} else {
			for (u32 i = 0; i < count; ++i) {
				std::thread([i]() {
					for (;;) {
						spinlock(
							[i] {
								if (auto entry = workQueue.multi.pop()) {
									doWork(i, entry->function, entry->param, entry->name);
									return true;
								}
								return false;
							},
							&stopWork);
						if (stopWork)
							break;
					}
					InterlockedIncrement(&deadWorkers);
				}).detach();
			}
			pushWorkImpl = [](void (*fn)(void *), void *param, char const *name) {
				workQueue.multi.push({fn, param, name});
				InterlockedIncrement(&workToDo);
			};
		}
	}
}
void shutdownWorkerThreads() {
	stopWork = true;
	spinlock([] { return deadWorkers == workerCount; });
}
void pushWork_(char const *name, void (*fn)(void *), void *param) { return pushWorkImpl(fn, param, name); }
void completeAllWork() { return waitForWorkCompletionImpl(); }

struct CPUID {
	s32 eax;
	s32 ebx;
	s32 ecx;
	s32 edx;
	operator s32 *() { return &eax; }
};

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

	ASSERT(processorInfoLength % sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) == 0);
	for (auto &info : Span{buffer, processorInfoLength / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)}) {
		switch (info.Relationship) {
			case RelationNumaNode:
			case RelationProcessorPackage: break;
			case RelationProcessorCore: result.logicalProcessorCount += countBits(info.ProcessorMask); break;

			case RelationCache:
				if (info.Cache.Level == 1) {
					result.cacheL1++;
				} else if (info.Cache.Level == 2) {
					result.cacheL2++;
				} else if (info.Cache.Level == 3) {
					result.cacheL3++;
				}
				break;

			default: INVALID_CODE_PATH("Error: Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value.");
		}
	}

	CpuVendor vendor{};
	char vendorName[25];
	s32 ECX_1 = 0;
	s32 EDX_1 = 0;
	s32 EBX_7 = 0;
	s32 ECX_7 = 0;
	s32 ECX_e1 = 0;
	s32 EDX_e1 = 0;
	std::vector<CPUID> data;
	std::vector<CPUID> extdata;

	CPUID cpui{};

	// Calling __cpuid with 0x0 as the function_id argument
	// gets the number of the highest valid function ID.
	__cpuid(cpui, 0);
	int nIds = cpui.eax;
	data.reserve((umm)nIds);

	for (int i = 0; i <= nIds; ++i) {
		__cpuidex(cpui, i, 0);
		data.push_back(cpui);
	}

	ASSERT(data.size() > 0);

	// Capture vendor string
	((int *)vendorName)[0] = data[0][1];
	((int *)vendorName)[1] = data[0][3];
	((int *)vendorName)[2] = data[0][2];
	vendorName[24] = '\0';
	if (strcmp(vendorName, "GenuineIntel") == 0) {
		vendor = CpuVendor::Intel;
	} else if (strcmp(vendorName, "AuthenticAMD") == 0) {
		vendor = CpuVendor::AMD;
	}
	result.vendor = vendor;

	// load bitset with flags for function 0x00000001
	if (nIds >= 1) {
		ECX_1 = data[1][2];
		EDX_1 = data[1][3];
	}

	// load bitset with flags for function 0x00000007
	if (nIds >= 7) {
		EBX_7 = data[7][1];
		ECX_7 = data[7][2];
	}

	// Calling __cpuid with 0x80000000 as the function_id argument
	// gets the number of the highest valid extended ID.
	__cpuid(cpui, 0x80000000);
	int nExIds_ = cpui[0];

	for (int i = 0x80000000; i <= nExIds_; ++i) {
		__cpuidex(cpui, i, 0);
		extdata.push_back(cpui);
	}

	// load bitset with flags for function 0x80000001
	if (nExIds_ >= 0x80000001) {
		ECX_e1 = extdata[1][2];
		EDX_e1 = extdata[1][3];
	}

	// Interpret CPU brand string if reported
	if (nExIds_ >= 0x80000004) {
		char *brand = (char *)malloc(49);
		brand[48] = 0;
		memcpy(brand + sizeof(cpui) * 0, extdata[2], sizeof(cpui));
		memcpy(brand + sizeof(cpui) * 1, extdata[3], sizeof(cpui));
		memcpy(brand + sizeof(cpui) * 2, extdata[4], sizeof(cpui));
		result.brand = brand;
	}

	auto set = [&](ProcessorFeature feature, bool value) {
		u32 index, slot = cpuInfo.getFeaturePos(feature, index);
		result.features[slot] |= (u32)value << index;
	};

	// clang-format off
	set(ProcessorFeature::SSE3,			(ECX_1	& (1 << 0 )));
    set(ProcessorFeature::PCLMULQDQ,	(ECX_1	& (1 << 1 )));
    set(ProcessorFeature::MONITOR,		(ECX_1	& (1 << 3 )));
    set(ProcessorFeature::SSSE3,		(ECX_1	& (1 << 9 )));
    set(ProcessorFeature::FMA,			(ECX_1	& (1 << 12)));
    set(ProcessorFeature::CMPXCHG16B,	(ECX_1	& (1 << 13)));
    set(ProcessorFeature::SSE41,		(ECX_1	& (1 << 19)));
    set(ProcessorFeature::SSE42,		(ECX_1	& (1 << 20)));
    set(ProcessorFeature::MOVBE,		(ECX_1	& (1 << 22)));
    set(ProcessorFeature::POPCNT,		(ECX_1	& (1 << 23)));
    set(ProcessorFeature::AES,			(ECX_1	& (1 << 25)));
    set(ProcessorFeature::XSAVE,		(ECX_1	& (1 << 26)));
    set(ProcessorFeature::OSXSAVE,		(ECX_1	& (1 << 27)));
    set(ProcessorFeature::AVX,			(ECX_1	& (1 << 28)));
    set(ProcessorFeature::F16C,			(ECX_1	& (1 << 29)));
    set(ProcessorFeature::RDRAND,		(ECX_1	& (1 << 30)));
    set(ProcessorFeature::MSR,			(EDX_1	& (1 << 5 )));
    set(ProcessorFeature::CX8,			(EDX_1	& (1 << 8 )));
    set(ProcessorFeature::SEP,			(EDX_1	& (1 << 11)));
    set(ProcessorFeature::CMOV,			(EDX_1	& (1 << 15)));
    set(ProcessorFeature::CLFSH,		(EDX_1	& (1 << 19)));
    set(ProcessorFeature::MMX,			(EDX_1	& (1 << 23)));
    set(ProcessorFeature::FXSR,			(EDX_1	& (1 << 24)));
    set(ProcessorFeature::SSE,			(EDX_1	& (1 << 25)));
    set(ProcessorFeature::SSE2,			(EDX_1	& (1 << 26)));
    set(ProcessorFeature::FSGSBASE,		(EBX_7	& (1 << 0 )));
    set(ProcessorFeature::BMI1,			(EBX_7	& (1 << 3 )));
    set(ProcessorFeature::HLE,			(EBX_7	& (1 << 4 )) && vendor == CpuVendor::Intel);
    set(ProcessorFeature::AVX2,			(EBX_7	& (1 << 5 )));
    set(ProcessorFeature::BMI2,			(EBX_7	& (1 << 8 )));
    set(ProcessorFeature::ERMS,			(EBX_7	& (1 << 9 )));
    set(ProcessorFeature::INVPCID,		(EBX_7	& (1 << 10)));
    set(ProcessorFeature::RTM,			(EBX_7	& (1 << 11)) && vendor == CpuVendor::Intel);
    set(ProcessorFeature::AVX512F,		(EBX_7	& (1 << 16)));
    set(ProcessorFeature::RDSEED,		(EBX_7	& (1 << 18)));
    set(ProcessorFeature::ADX,			(EBX_7	& (1 << 19)));
    set(ProcessorFeature::AVX512PF,		(EBX_7	& (1 << 26)));
    set(ProcessorFeature::AVX512ER,		(EBX_7	& (1 << 27)));
    set(ProcessorFeature::AVX512CD,		(EBX_7	& (1 << 28)));
    set(ProcessorFeature::SHA,			(EBX_7	& (1 << 29)));
    set(ProcessorFeature::PREFETCHWT1,  (ECX_7	& (1 << 0 )));
    set(ProcessorFeature::LAHF,			(ECX_e1	& (1 << 0 )));
    set(ProcessorFeature::LZCNT,		(ECX_e1	& (1 << 5 )) && vendor == CpuVendor::Intel);
    set(ProcessorFeature::ABM,			(ECX_e1	& (1 << 5 )) && vendor == CpuVendor::AMD);
    set(ProcessorFeature::SSE4a,		(ECX_e1	& (1 << 6 )) && vendor == CpuVendor::AMD);
    set(ProcessorFeature::XOP,			(ECX_e1	& (1 << 11)) && vendor == CpuVendor::AMD);
    set(ProcessorFeature::TBM,			(ECX_e1	& (1 << 21)) && vendor == CpuVendor::AMD);
    set(ProcessorFeature::SYSCALL,		(EDX_e1	& (1 << 11)) && vendor == CpuVendor::Intel);
    set(ProcessorFeature::MMXEXT,		(EDX_e1	& (1 << 22)) && vendor == CpuVendor::AMD);
    set(ProcessorFeature::RDTSCP,		(EDX_e1	& (1 << 27)) && vendor == CpuVendor::Intel);
    set(ProcessorFeature::_3DNOWEXT,	(EDX_e1	& (1 << 30)) && vendor == CpuVendor::AMD);
    set(ProcessorFeature::_3DNOW,		(EDX_e1	& (1 << 31)) && vendor == CpuVendor::AMD);
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

#if 0
namespace Profiler {
HANDLE thread;
SPSC::CircularQueue<Entry, 256> entries;
bool volatile stopped;
void push(Entry e) {
	e.end = PerfTimer::getCounter();
	entries.push(e);
}
void init() {
	thread = CreateThread(
		0, 0,
		[](void*) -> DWORD {
			HANDLE file = CreateFileA("profile.json", GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
			ASSERT(file != INVALID_HANDLE_VALUE);
			char const* header = R"({"otherData":{},"traceEvents":[)";
			writeFile(file, header, strlen(header));

			for (;;) {
				if (auto e = entries.pop(); e.has_value()) {
					s64 end = e->end;
					s64 begin = e->begin;
					s64 const usInSecond = 1000000;
					begin = begin * usInSecond / PerfTimer::frequency;
					end = end * usInSecond / PerfTimer::frequency;
					char buffer[256];
					ASSERT(StringCbPrintfA(
							   buffer, _countof(buffer),
							   R"({"cat":"function","dur":%lli,"name":"%s","ph":"X","pid":0,"tid":%u,"ts":%lli},)",
							   end - begin, e->name, GetCurrentThreadId(), begin) == S_OK);
					writeFile(file, buffer, strlen(buffer));
					free(e->name);
				} else if (stopped) {
					break;
				} else {
					Sleep(1);
				}
			}
			char const* footer = "]}";
			setFilePointer(file, -1, SeekFrom::cursor);
			writeFile(file, footer, strlen(footer));
			CloseHandle(file);
			return 0;
		},
		0, 0, 0);
	ASSERT(thread);
}
void shutdown() {
	stopped = true;
	WaitForSingleObjectEx(thread, INFINITE, false);
	CloseHandle(thread);
}
} // namespace Profiler
#else
namespace Profiler {
#define MAX_PROFILER_ENTRY_COUNT 1024
Stats stats;
List<Entry> activeEntries;

ENG_API void createEntry(char const *name) {
	Entry result;
	result.name = name;
	result.totalMicroseconds = (u64)PerfTimer::getCounter();
	result.totalCycles = __rdtsc();
	result.selfMicroseconds = result.totalMicroseconds;
	result.selfCycles = result.totalCycles;
	activeEntries.push_back(result);
}
void addEntry() {
	auto e = activeEntries.back();
	activeEntries.pop_back();
	auto nowNs = (u64)PerfTimer::getCounter();
	e.selfMicroseconds = nowNs - e.selfMicroseconds;
	e.totalMicroseconds = nowNs - e.totalMicroseconds;
	auto nowCy = __rdtsc();
	e.selfCycles = nowCy - e.selfCycles;
	e.totalCycles = nowCy - e.totalCycles;
	for (auto &o : activeEntries) {
		o.selfCycles += e.selfCycles;
		o.selfMicroseconds += e.selfMicroseconds;
	}
	if (auto it = std::find_if(stats.entries.begin(), stats.entries.end(),
							   [&](Entry n) { return strcmp(n.name, e.name) == 0; });
		it != stats.entries.end()) {

		it->selfCycles += e.selfCycles;
		it->selfMicroseconds += e.selfMicroseconds;
		it->totalCycles += e.totalCycles;
		it->totalMicroseconds += e.totalMicroseconds;
	} else {
		stats.entries.push_back(e);
	}
}
Stats &getStats() {
	for (auto &stat : stats.entries) {
		stat.selfMicroseconds = PerfTimer::getMicroseconds<u64>((s64)stat.selfMicroseconds);
		stat.totalMicroseconds = PerfTimer::getMicroseconds<u64>((s64)stat.totalMicroseconds);
	}
	stats.totalMicroseconds = PerfTimer::getMicroseconds<u64>((s64)stats.totalMicroseconds, PerfTimer::getCounter());
	stats.totalCycles = __rdtsc() - stats.totalCycles;
	return stats;
}
void reset() {
	stats.entries.clear();
	stats.entries.reserve(256);
	activeEntries.clear();
	activeEntries.reserve(512);
	stats.totalCycles = __rdtsc();
	stats.totalMicroseconds = (u64)PerfTimer::getCounter();
}
} // namespace Profiler
namespace Log {
void print(char const *msg) { puts(msg); }
} // namespace Log
#endif