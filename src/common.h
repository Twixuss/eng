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

namespace Log {
ENG_API void print(char const *msg);
template <class... T>
void print(char const *format, T const &... args);
template <class... T>
void warn(char const *format, T const &... args);
template <class... T>
void error(char const *format, T const &... args);
} // namespace Log

#include <stdexcept>
#include <functional>
#define PRINT_AND_THROW(string, ...)              \
	Log::error(string "\nMessage: " __VA_ARGS__); \
	DEBUG_BREAK();                                \
	throw std::runtime_error(string)

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

#include <atomic>
#include <optional>
#include <thread>
#include <chrono>
#include <utility>
#include <mutex>
#include <tuple>

#if COMPILER_MSVC
#pragma warning(pop)
#else
#endif

namespace Detail {
template <class Tuple, size_t... indices>
static void invoke(void *rawVals) noexcept {
	Tuple *fnVals((Tuple *)(rawVals));
	Tuple &tup = *fnVals;
	std::invoke(std::move(std::get<indices>(tup))...);
	delete fnVals;
}

template <class Tuple, size_t... indices>
static constexpr auto getInvoke(std::index_sequence<indices...>) noexcept {
	return &invoke<Tuple, indices...>;
}
} // namespace Detail

struct ENG_API WorkQueue {
	u32 volatile workToDo = 0;

	template <class Fn, class... Args>
	void push(Fn &&fn, Args &&... args) {
		using Tuple = std::tuple<std::decay_t<Fn>, std::decay_t<Args>...>;
		auto fnParams = new Tuple(std::forward<Fn>(fn), std::forward<Args>(args)...);
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
	PerfTimer() : begin(getCounter()) {}
	inline s64 getElapsedCounter() { return getCounter() - begin; }
	inline void reset() { begin = getCounter(); }

	static s64 const frequency;
	static s64 getCounter();
	template <class Ret = f32>
	inline static Ret getSeconds(s64 begin, s64 end) {
		return (Ret)(end - begin) / (Ret)frequency;
	}
	template <class Ret = f32>
	inline static Ret getMilliseconds(s64 elapsed) {
		return (Ret)(elapsed * 1000) / (Ret)frequency;
	}
	template <class Ret = f32>
	inline static Ret getMicroseconds(s64 elapsed) {
		return (Ret)(elapsed * 1000000) / (Ret)frequency;
	}
	template <class Ret = f32>
	inline static Ret getNanoseconds(s64 elapsed) {
		return (Ret)(elapsed * 1000000000) / (Ret)frequency;
	}
	template <class Ret = f32>
	inline static Ret getMilliseconds(s64 begin, s64 end) {
		return getMilliseconds<Ret>(end - begin);
	}
	template <class Ret = f32>
	inline static Ret getMicroseconds(s64 begin, s64 end) {
		return getMicroseconds<Ret>(end - begin);
	}
	template <class Ret = f32>
	inline static Ret getNanoseconds(s64 begin, s64 end) {
		return getNanoseconds<Ret>(end - begin);
	}
	template <class Ret = f32>
	inline Ret getMilliseconds() {
		return getMilliseconds<Ret>(getElapsedCounter());
	}
	template <class Ret = f32>
	inline Ret getMicroseconds() {
		return getMicroseconds<Ret>(getElapsedCounter());
	}
	template <class Ret = f32>
	inline Ret getNanoseconds() {
		return getNanoseconds<Ret>(getElapsedCounter());
	}

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

struct ENG_API FileHandle {
	void *handle;

	s64 getPointer();
	s64 setPointer(s64 offset, SeekFrom startPoint = SeekFrom::begin);
	bool read(void *buffer, size_t size);
	bool write(void const *buffer, size_t size);
};

ENG_API StringSpan readEntireFile(wchar const *path);
ENG_API StringSpan readEntireFile(char const *path);
ENG_API void freeEntireFile(StringSpan file);

ENG_API bool fileExists(char const *path);

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

namespace Log {
ENG_API void print(char const *msg);
template <class... T>
void print(char const *format, T const &... args) {
	char buffer[1024];
	sprintf(buffer, format, args...);
	print(buffer);
}
template <class... T>
void warn(char const *format, T const &... args) {
	char buffer[1024];
	char *t = buffer;
	t += sprintf(t, "WARNING: ");
	t += sprintf(t, format, args...);
	print(buffer);
}
template <class... T>
void error(char const *format, T const &... args) {
	char buffer[1024];
	char *t = buffer;
	t += sprintf(t, "ERROR: ");
	t += sprintf(t, format, args...);
	print(buffer);
}
} // namespace Log

#define DATA  "../data/"
#define LDATA CONCAT(L, DATA)

#if OS_WINDOWS

enum {
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
