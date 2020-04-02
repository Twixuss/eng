#pragma once
#define _CRT_SECURE_NO_WARNINGS

#define PRINT_AND_THROW(string) \
	puts(string);               \
	DEBUG_BREAK();              \
	throw std::runtime_error(string)

#define ASSERTION_FAILURE(causeString, expression, ...)                                                        \
	PRINT_AND_THROW(causeString "\nFile: " __FILE__ "\nLine: " STRINGIZE(__LINE__) "\nExpression: " expression \
																				   "\nMessage: " __VA_ARGS__)
#include "../dep/tl/include/tl/common.h"
#include "../dep/tl/include/tl/math.h"
using namespace TL;

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
5045 /* spectre */
// clang-format on
#pragma warning(disable : DISABLED_WARNINGS)

#if defined BUILD_ENG
#define ENG_API	 __declspec(dllexport)
#define GAME_API __declspec(dllimport)
#elif defined BUILD_GAME
#define ENG_API	 __declspec(dllimport)
#define GAME_API __declspec(dllexport)
#else
#define ENG_API	 __declspec(dllimport)
#define GAME_API __declspec(dllimport)
#endif

#if COMPILER_MSVC
#pragma warning(push, 0)
#pragma warning(disable : 4710)
#pragma warning(disable : 4711)
#else
#endif

#include <atomic>
#include <optional>
#include <thread>
#include <chrono>
#include <utility>
#include <mutex>

#if COMPILER_MSVC
#pragma warning(pop)
#else
#endif

// single producer, single consumer
namespace SPSC {
template <class T, size_t capacity>
struct CircularQueue {
	CircularQueue() = default;
	CircularQueue(CircularQueue const&) = delete;
#define EMPLACE(name, param, assign) \
	void name(param) {               \
		getSpace();                  \
		assign;                      \
		++poppableCount;             \
		increment(end);              \
	}
	template <class... Args>
	EMPLACE(emplace, Args&&... args, new (std::addressof(end->val)) T(std::forward<Args>(args)...));
	EMPLACE(push, T&& val, new (std::addressof(end->val)) T(std::move(val)));
	EMPLACE(push, T const& val, new (std::addressof(end->val)) T(val));
#undef EMPLACE
	std::optional<T> pop() {
		std::optional<T> result{};
		if (poppableCount) {
			--poppableCount;
			result.emplace(std::move(begin->val));
			begin->val.~T();
			++pushableCount;
			increment(begin);
		}
		return result;
	}
	size_t size() const { return poppableCount; }

private:
	union Entry {
		T val;
#pragma warning(suppress : 4582)
		Entry() {}
#pragma warning(suppress : 4583)
		~Entry() {}
#pragma warning(suppress : 4625)
	};
	Entry entries[capacity];
	Entry* begin = entries;
	Entry* end = begin;
	std::atomic<size_t> poppableCount = 0;
	std::atomic<size_t> pushableCount = capacity;
	void increment(Entry*& e) {
		if (++e == entries + capacity) {
			e = entries;
		}
	}
	void getSpace() {
		while (pushableCount == 0) {
#if !COMPILER_GCC
			std::this_thread::sleep_for(std::chrono::microseconds(1));
#endif
		}
		--pushableCount;
	}
};

void circularQueueTest() {
#if !COMPILER_GCC
	{
		struct Test {
			Test() { puts("con"); }
			Test(Test const&) { puts("cop"); }
			Test(Test&&) { puts("mov"); }
			~Test() { puts("des"); }
		};
		SPSC::CircularQueue<Test, 1> q;
		puts("emplace");
		q.emplace();
		puts("pop");
		q.pop();
		{
			puts("push &");
			Test t;
			q.push(t);
			puts("pop");
			q.pop();
		}
		puts("push &&");
		q.push({});
		puts("pop");
		q.pop();
	}

	CircularQueue<u32, 128> queue;
	bool done;
	auto popperWork = [&]() {
		u32 counter = 0;
		while (!done || queue.size()) {
			if (auto v = queue.pop(); v) {
				ASSERT(v.value() == counter++);
				// Sleep((DWORD)rand() & 1);
			}
		}
	};
	for (int j = 0; j < 2; ++j) {
		done = false;
		std::thread popper{popperWork};
		for (u32 i = 0; i < 1024 * 1024; ++i) {
			queue.push(i);
			// Sleep((DWORD)rand() & 1);
		}
		done = true;
		popper.join();
	}
#endif
}

} // namespace SPSC

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
	inline static Ret getMilliseconds(s64 begin, s64 end) {
		return getMilliseconds<Ret>(begin - end);
	}
	template <class Ret = f32>
	inline static Ret getMicroseconds(s64 begin, s64 end) {
		return getMicroseconds<Ret>(begin - end);
	}
	template <class Ret = f32>
	inline Ret getMilliseconds() {
		return getMilliseconds<Ret>(getElapsedCounter());
	}
	template <class Ret = f32>
	inline Ret getMicroseconds() {
		return getMicroseconds<Ret>(getElapsedCounter());
	}

private:
	s64 begin;
};
#else
#error no timer
#endif

enum class FilePoint : u32 {
	begin = 0,
	cursor = 1,
	end = 2,
};

ENG_API s64 setFilePointer(void* file, s64 offset, FilePoint startPoint = FilePoint::begin);
ENG_API bool readFile(void* handle, void* buffer, size_t size);
ENG_API bool writeFile(void* handle, void const* buffer, size_t size);

struct ReadFileResult {
	StringSpan data;
	bool valid() const { return data.begin() != 0; }
	void release() {
		free(data.begin());
		data = {};
	}
};
ENG_API ReadFileResult readFile(wchar const* path);
ENG_API ReadFileResult readFile(char const* path);

ENG_API void* copyBuffer(void const* data, size_t size);
ENG_API void freeCopiedBuffer(void* data);

namespace Profiler {
struct Entry {
	s64 begin, end;
	char* name;
};
ENG_API void init();
ENG_API void shutdown();
template <class... T>
static Entry begin(char const* format, T const&... ts) {
	Entry result;
	char tempBuffer[256];
	int len = sprintf_s(tempBuffer, format, ts...);
	ASSERT(len >= 0);
	result.name = (char*)malloc((size_t)len + 1);
	strcpy(result.name, tempBuffer);
	result.begin = PerfTimer::getCounter();
	return result;
}
ENG_API void push(Entry e);

struct Scope : Entry {
	template <class... T>
	Scope(char const* format, T const&... ts) : Entry(Profiler::begin(format, ts...)) {}
	~Scope() { push(*this); }
};
}; // namespace Profiler

#if ENABLE_PROFILER
struct ProfilerScope {
	inline ProfilerScope() { Profiler::init(); }
	inline ~ProfilerScope() { Profiler::shutdown(); }
};
#define CREATE_PROFILER					  ProfilerScope _profiler_scope
#define PROFILE_BEGIN(name, message, ...) auto CONCAT(_profiler_, name) = Profiler::begin(message, __VA_ARGS__)
#define PROFILE_END(name)				  Profiler::push(CONCAT(_profiler_, name))
#define PROFILE_SCOPE(message, ...)		  Profiler::Scope CONCAT(_profiler_scope_, __LINE__)(message, __VA_ARGS__)
#define PROFILE_FUNCTION				  PROFILE_SCOPE("%s", __FUNCTION__)
#else
#define CREATE_PROFILER
#define PROFILE_BEGIN(name, message, ...)
#define PROFILE_END(name)
#define PROFILE_SCOPE(message, ...)
#define PROFILE_FUNCTION
#endif

#define DATA  "../data/"
#define LDATA CONCAT(L, DATA)
