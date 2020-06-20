#pragma once
// clang-format off
#define DISABLED_WARNINGS                                \
4061 /* enumerator in switch is not explicitly handled */\
4100 /* unused formal parameter */                       \
4201 /* unnamed struct */                                \
4251 /* dll interface */                                 \
4514 /* unreferenced inline function has been removed */ \
4587 /* implicit constructor is not called in union*/    \
4626 /* assignment operator was implicitly deleted */    \
4710 /* function not inlined */                          \
4711 /* function inlined */                              \
4774 /* non literal format string */                     \
4820 /* struct padding */                                \
5045 /* spectre */										 \
4625 /* deleted copy */									 \
5026 /* deleted move */									 \
5027 /* deleted assign */
// clang-format on
#pragma warning(disable : DISABLED_WARNINGS)

#define _CRT_SECURE_NO_WARNINGS

#if defined BUILD_ENG
#define ENG_API	 __declspec(dllexport)
#define GAME_API extern "C" __declspec(dllimport)
#elif defined BUILD_GAME
#define ENG_API	 __declspec(dllimport)
#define GAME_API extern "C" __declspec(dllexport)
#else
#define ENG_API	 __declspec(dllimport)
#define GAME_API extern "C" __declspec(dllimport)
#endif

#define FATAL_CODE_PATH(msg) Log::fatal(msg);DEBUG_BREAK;throw 0

#define PRINT_AND_THROW(string, ...) Log::error(string "\nMessage: " __VA_ARGS__); FATAL_CODE_PATH(string)

#define ASSERTION_FAILURE(causeString, expression, ...)                                                     \
	PRINT_AND_THROW(causeString                                                                             \
					"\nFile: " __FILE__                                                                     \
					"\nLine: " STRINGIZE(__LINE__) "\nFunction: " __FUNCTION__ "\nExpression: " expression, \
					__VA_ARGS__)
#include "../dep/tl/include/tl/common.h"
#include "../dep/tl/include/tl/math.h"
#include "../dep/tl/include/tl/thread.h"
using namespace TL;

#if COMPILER_MSVC
#pragma warning(push, 0)
#pragma warning(disable : 4710) // function inlined
#pragma warning(disable : 4711) // function not inlined
#else
#endif

#include <thread>
#include <chrono>
#include <mutex>
#include <tuple>

#if COMPILER_MSVC
#pragma warning(pop)
#else
#endif

template <class StringBuilder>
void _append(char const *fmt, StringBuilder &builder) {
	builder.append(Span{fmt, strlen(fmt)});
}

template <class Arg, class ...Args, class StringBuilder>
void _append(char const *fmt, StringBuilder &builder, Arg const &arg, Args const &...args) {
	char const *c = fmt;
	char const *argP = 0;
	while (c[0] && c[1]) {
		if (c[0] == '{' && c[1] == '}') {
			argP = c;
			break;
		}
		++c;
	}
	ASSERT(argP, "invalid format");
	builder.append(Span{fmt, argP});
	toString(arg, [&] (char const *src, umm length) {
		builder.append(Span{src, length});
		return 0;
	});

	_append(argP + 2, builder, args...);
}

template <class Allocator = TempAllocator, class ...Args>
String<Allocator> format(char const *fmt, Args const &...args) {
	StringBuilder<Allocator> builder;
	_append(fmt, builder, args...);
	return builder.get();
}

template <class Allocator = TempAllocator, class ...Args>
String<Allocator> formatAndTerminate(char const *fmt, Args const &...args) {
	StringBuilder<Allocator> builder;
	_append(fmt, builder, args...);
	builder.append('\0');
	return builder.get();
}

ENG_API Span<char const> _getModuleName(void *);

namespace Log {
enum class Color {
	black,
	darkBlue,
	darkGreen,
	darkCyan,
	darkRed,
	darkMagenta,
	darkYellow,
	darkGray,
	gray,
	blue,
	green,
	cyan,
	red,
	magenta,
	yellow,
	white,
};

extern "C" struct IMAGE_DOS_HEADER __ImageBase;

static Span<char const> _moduleName = _getModuleName(&__ImageBase);

ENG_API void _print(Span<char const> msg, Span<char const> = _moduleName);
ENG_API void setColor(Color);
inline void fatal(Span<char const> msg) {
	setColor(Color::red);
	_print(msg);
}
template <class ...Args>
void error(char const *fmt, Args const &...args) {
	StringBuilder<TempAllocator> builder;
	builder.append("ERROR: ");
	_append(fmt, builder, args...);
	setColor(Color::red);
	_print(builder.get());
}
template <class ...Args>
void warn(char const *fmt, Args const &...args) {
	StringBuilder<TempAllocator> builder;
	builder.append("WARNING: ");
	_append(fmt, builder, args...);
	setColor(Color::yellow);
	_print(builder.get());
}
template <class ...Args>
void print(char const *fmt, Args const &...args) {
	StringBuilder<TempAllocator> builder;
	_append(fmt, builder, args...);
	setColor(Color::white);
	_print(builder.get());
}
} // namespace Log

ENG_API void dummy(void const *);

// NOTE: memory gets freed before each frame
ENG_API void *allocateTemp(u32 size, u32 align = 0);
template <class T>
T *allocateTemp(u32 count = 1) {
	return (T *)allocateTemp(count * sizeof T, alignof T);
}

ENG_API u32 getTempMemoryUsage();

ENG_API u64 getMemoryUsage();

struct TempAllocator {
	static void *allocate(umm size, umm align = 0) { return allocateTemp((u32)size, (u32)align); }
	static void deallocate(void *data) {}
};

namespace Detail {
template <class Tuple, umm... indices>
static void invoke(void *rawVals) noexcept {
	Tuple *fnVals((Tuple *)(rawVals));
	Tuple &tup = *fnVals;
	std::invoke(std::move(std::get<indices>(tup))...);
}

template <class Tuple, umm... indices>
static constexpr auto getInvoke(std::index_sequence<indices...>) noexcept {
	return &invoke<Tuple, indices...>;
}
} // namespace Detail

#define ENG_WORK_USE_TEMP 1

struct ENG_API WorkQueue {
	u32 volatile workToDo = 0;

	// NOTE: time interval between first call to 'push' and call to 'completeAllWork' MUST NOT cross start-frame or frame-frame boundary
	template <class Fn, class... Args>
	void push(Fn &&fn, Args &&... args) {
		using Tuple = std::tuple<std::decay_t<Fn>, std::decay_t<Args>...>;
#if ENG_WORK_USE_TEMP
		auto fnParams = allocateTemp(sizeof Tuple);
#else
		auto fnParams = malloc(sizeof Tuple);
#endif
		new(fnParams) Tuple(std::forward<Fn>(fn), std::forward<Args>(args)...);
		constexpr auto invokerProc = Detail::getInvoke<Tuple>(std::make_index_sequence<1 + sizeof...(Args)>{});

		push_(invokerProc, fnParams);
	}
	void push_(void (*fn)(void *), void *param);
	void completeAllWork();
	bool completed();
};

ENG_API void initWorkerThreads(u32 count);
ENG_API void shutdownWorkerThreads();
ENG_API u32 getWorkerThreadCount();

template <class Pred>
inline void waitUntil(Pred pred) {
	u32 miss = 0;
	while (!pred()) {
		++miss;
		if (miss >= 1024) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		} else if (miss >= 256) {
			switchThread();
		} else if (miss >= 16) {
			_mm_pause();
		}
	}
}

inline void waitUntil(bool const volatile &condition) {
	waitUntil([&]{ return condition; });
}

struct SyncPoint {
	u32 const threadCount;
	u32 volatile counter;
	inline SyncPoint(u32 threadCount) : threadCount(threadCount), counter(0) {}
};

inline void sync(SyncPoint& sp) {
	lockIncrement(&sp.counter);
	waitUntil([&]{return sp.counter==sp.threadCount;});
}

enum class ProcessorFeature {
	_3DNOW,
	_3DNOWEXT,
	ABM,
	ADX,
	AES,
	AVX,
	AVX2,
	AVX512CD,
	AVX512ER,
	AVX512F,
	AVX512PF,
	BMI1,
	BMI2,
	CLFSH,
	CMOV,
	CMPXCHG16B,
	CX8,
	ERMS,
	F16C,
	FMA,
	FSGSBASE,
	FXSR,
	HLE,
	INVPCID,
	LAHF,
	LZCNT,
	MMX,
	MMXEXT,
	MONITOR,
	MOVBE,
	MSR,
	OSXSAVE,
	PCLMULQDQ,
	POPCNT,
	PREFETCHWT1,
	RDRAND,
	RDSEED,
	RDTSCP,
	RTM,
	SEP,
	SHA,
	SSE,
	SSE2,
	SSE3,
	SSE41,
	SSE42,
	SSE4a,
	SSSE3,
	SYSCALL,
	TBM,
	XOP,
	XSAVE,
};

ENG_API char const *toString(ProcessorFeature);

enum class CpuVendor { Unknown, Intel, AMD };
ENG_API char const *toString(CpuVendor);

enum class CacheType : u16 {
	unified, instruction, data, trace, count
};

struct CpuInfo {
	struct FeatureIndex {
		u32 slot;
		u32 bit;
	};
	struct Cache {
		u16 count;
		u32 size;
	};

	u32 logicalProcessorCount;
	Cache cache[3][(u32)CacheType::count];
	u32 cacheLineSize;
	u32 features[4];
	CpuVendor vendor;
	char brand[49];

	inline b32 hasFeature(ProcessorFeature feature) const {
		FeatureIndex index = getFeaturePos(feature);
		return (b32)(features[index.slot] & (1 << index.bit));
	}
	inline FeatureIndex getFeaturePos(ProcessorFeature f) const {
		FeatureIndex result;
		result.slot = (u32)f >> 5;
		result.bit = (u32)f & 0x1F;
		return result;
	}
	u32 totalCacheSize(u32 level) const {
		ASSERT(1 <= level && level <= 3);
		u32 result = 0;
		for (auto &c : cache[level - 1]) {
			result += c.size;
		}
		return result;
	}
};
extern ENG_API CpuInfo const cpuInfo;

#if OS_WINDOWS
struct ENG_API PerfTimer {
	inline PerfTimer() { reset(); }
	inline s64 getElapsedCounter() { return getCounter() - begin; }
	inline void reset() { begin = getCounter(); }

	static s64 const frequency;
	static s64 getCounter();
	template <class Ret = f32> inline static Ret getSeconds(s64 elapsed) { return (Ret)elapsed / (Ret)frequency; }
	template <class Ret = f32> inline static Ret getMilliseconds(s64 elapsed) { return getSeconds<Ret>(elapsed * 1000); }
	template <class Ret = f32> inline static Ret getMicroseconds(s64 elapsed) { return getSeconds<Ret>(elapsed * 1000000); }
	template <class Ret = f32> inline static Ret getNanoseconds (s64 elapsed) { return getSeconds<Ret>(elapsed * 1000000000); }
	template <class Ret = f32> inline static Ret getSeconds(s64 begin, s64 end) { return getSeconds<Ret>(end - begin); }
	template <class Ret = f32> inline static Ret getMilliseconds(s64 begin, s64 end) { return getMilliseconds<Ret>(end - begin); }
	template <class Ret = f32> inline static Ret getMicroseconds(s64 begin, s64 end) { return getMicroseconds<Ret>(end - begin); }
	template <class Ret = f32> inline static Ret getNanoseconds (s64 begin, s64 end) { return getNanoseconds <Ret>(end - begin); }
	template <class Ret = f32> inline Ret getSeconds() { return getSeconds<Ret>(getElapsedCounter()); }
	template <class Ret = f32> inline Ret getMilliseconds() { return getMilliseconds<Ret>(getElapsedCounter()); }
	template <class Ret = f32> inline Ret getMicroseconds() { return getMicroseconds<Ret>(getElapsedCounter()); }
	template <class Ret = f32> inline Ret getNanoseconds() { return getNanoseconds<Ret>(getElapsedCounter()); }

	static s64 syncWithTime(s64 beginPC, f32 duration);

private:
	s64 begin;
};
#else
#error no timer
#endif

enum class SeekFrom : u32 {
	begin = 0,
	cursor = 1,
	end = 2,
};

struct File {
#if OS_WINDOWS
	enum OpenMode : u32 {
		OpenMode_write = 0x40000000,
		OpenMode_read  = 0x80000000,
		OpenMode_keep  = 0x1,
	};
#endif

	void *handle;

	ENG_API File(char const *path, u32 mode);
	ENG_API void close();

	ENG_API bool valid();

	ENG_API s64 getPointer();
	ENG_API s64 setPointer(s64 offset, SeekFrom startPoint = SeekFrom::begin);
	ENG_API bool read(void *buffer, umm size);
	ENG_API bool write(void const *buffer, umm size);
	ENG_API bool writeLine();
	inline bool write(Span<char const> span) { return write(span.data(), span.size()); }
	inline bool write(char const *str) { return write(str, strlen(str)); }
	template <class T>
	inline bool writeLine(T const &val) {
		return write(val) && writeLine();
	}
};

struct EntireFile {
	StringSpan data;
	bool valid;
};

ENG_API EntireFile readEntireFile(char const *path);
ENG_API void freeEntireFile(EntireFile file);

ENG_API bool fileExists(char const *path);
inline bool fileExists(Span<char const> path) { return fileExists(nullTerminate<TempAllocator>(path).data()); }

namespace Profiler {

ENG_API void init(u32 threadCount);

struct Stat {
	u64 startUs;
	u64 totalUs;
	u64 selfUs;
	std::string name;
	u16 depth;
};
ENG_API void start(char const *name);
ENG_API void stop();
ENG_API void reset();

struct Stats {
	List<List<Stat>> entries;
	u64 startUs;
	u64 totalUs;
};

}; // namespace Profiler

#if ENABLE_PROFILER
#define PROFILE_BEGIN(message) Profiler::start(message)
#define PROFILE_END			   Profiler::stop()
#define PROFILE_SCOPE(message) \
	PROFILE_BEGIN(message);    \
	DEFER { PROFILE_END; }
#define PROFILE_FUNCTION PROFILE_SCOPE(__FUNCTION__)
#else
#define PROFILE_BEGIN(message)
#define PROFILE_END
#define PROFILE_SCOPE(message)
#define PROFILE_FUNCTION
#endif

ENG_API void setCursorVisibility(bool);

#define DATA  "../data/"
#define LDATA CONCAT(L, DATA)

#if OS_WINDOWS

enum {
	Key_tab = 0x09,
	Key_enter = 0x0D,
	Key_escape = 0x1B,
	Key_leftArrow = 0x25,
	Key_upArrow = 0x26,
	Key_rightArrow = 0x27,
	Key_downArrow = 0x28,
	Key_f1 = 0x70,
	Key_f2 = 0x71,
	Key_f3 = 0x72,
	Key_f4 = 0x73,
	Key_f5 = 0x74,
	Key_f6 = 0x75,
	Key_f7 = 0x76,
	Key_f8 = 0x77,
	Key_f9 = 0x78,
	Key_f10 = 0x79,
	Key_f11 = 0x7A,
	Key_f12 = 0x7B,
	Key_f13 = 0x7C,
	Key_f14 = 0x7D,
	Key_f15 = 0x7E,
	Key_f16 = 0x7F,
	Key_f17 = 0x80,
	Key_f18 = 0x81,
	Key_f19 = 0x82,
	Key_f20 = 0x83,
	Key_f21 = 0x84,
	Key_f22 = 0x85,
	Key_f23 = 0x86,
	Key_f24 = 0x87,
};
#endif
using Key = u32;

struct ConvertedBytes {
	u16 value;
	u16 unit;
};
ENG_API Span<char const> byteUnitToString(u16 unit);
ENG_API ConvertedBytes cvtBytes(u64 bytes);

template <class CopyFn>
void toString(ConvertedBytes t, CopyFn &&copyFn) {
	char buf[64];
	char *end = buf;
	end += toString(t.value, buf).size();
	*end++ = ' ';
	auto unit = byteUnitToString(t.unit);
	memcpy(end, unit.data(), unit.size());
	end += unit.size();
	copyFn(buf, (umm)(end - buf));
}

struct ConvertedTime {
	u16 value;
	u16 unit;
};

ENG_API Span<char const> timeUnitToString(u16 unit);
ENG_API ConvertedTime cvtNanoseconds(u64 nanoseconds);
ENG_API ConvertedTime cvtMicroseconds(u64 microseconds);
ENG_API ConvertedTime cvtMilliseconds(u64 milliseconds);
ENG_API ConvertedTime cvtSeconds(u64 seconds);
ENG_API ConvertedTime cvtMinutes(u64 minutes);
ENG_API ConvertedTime cvtHours(u64 hours);

template <class CopyFn>
void toString(ConvertedTime t, CopyFn &&copyFn) {
	char buf[64];
	char *end = buf;
	end += toString(t.value, buf).size();
	*end++ = ' ';
	auto unit = timeUnitToString(t.unit);
	memcpy(end, unit.data(), unit.size());
	end += unit.size();
	copyFn(buf, (umm)(end - buf));
}


ENG_API void *getOptimizedProcAddress(char const *name);
ENG_API Span<char const> getOptimizedModuleName();
