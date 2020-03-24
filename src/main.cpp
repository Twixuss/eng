#include "common.h"
#include "game\game.h"
#include "input.h"
#include "input_internal.h"
#include "renderer.h"
#include "window_internal.h"

#pragma warning(push, 0)
#include <Windows.h>
#pragma warning(pop)

#pragma warning(disable : 4191)

// TEST
#include "allocator.h"

namespace {

inline void testAllocator() {
	BlockAllocator<int, 1024> al;
	u32 testSize = 1024 * 1024;
	std::vector<int*> ptrs;
	ptrs.reserve(testSize);

	PerfTimer timer;
	for (u32 i = 0; i < testSize; ++i) {
		ptrs.push_back(al.allocate(rand()));
	}
	printf("allocate: %f ms; %p\n", PerfTimer::getMilliseconds(timer.getElapsedCounter()), &ptrs);

	timer.reset();
	auto it = al.begin();
	u32 sum = 0;
	for (u32 i = 0; i < testSize; ++i) {
		++it;
		sum += *it;
	}
	printf("advance: %f ms; %u\n", PerfTimer::getMilliseconds(timer.getElapsedCounter()), sum);

	timer.reset();
	sum = 0;
	for (auto i : al) {
		sum += i;
	}
	printf("iterate: %f ms; %u\n", PerfTimer::getMilliseconds(timer.getElapsedCounter()), sum);

	timer.reset();
#if 1
	for (u32 i = 0; i < testSize; ++i) {
		al.deallocate(ptrs.back());
		ptrs.pop_back();
	}
#else
	for (auto ptr : ptrs) {
		al.deallocate(ptr);
	}
#endif
	printf("deallocate: %f ms\n", PerfTimer::getMilliseconds(timer.getElapsedCounter()));
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
	try {
		AllocConsole();
		freopen("CONOUT$", "w", stdout);

		// testAllocator();
		// mathTest();
		// SPSC::circularQueueTest();

		CREATE_PROFILER;

		Window::init(instance, GameAPI::getWindowInfo());
		DEFER { Window::shutdown(); };

		HMODULE renderer = LoadLibraryA("r_d3d11.dll");
		ASSERT(renderer, "r_d3d11.dll not found");

		Renderer::ProcEntries& r_procEntries = *(Renderer::ProcEntries*)GetProcAddress(renderer, "r_procEntries");
		for (auto& entry : r_procEntries) {
			auto proc = (void*)GetProcAddress(renderer, entry.name);
			ASSERT(proc, "Proc not found in r_d3d11.dll");
			*entry.procAddress = proc;
		}

		Renderer::init();
		DEFER { Renderer::shutdown(); };

		Input::init();
		DEFER { Input::shutdown(); };

		Game* game = GameAPI::start();
		if (!game)
			return -1;
		DEFER { GameAPI::shutdown(*game); };

		while (Window::isOpen()) {
			PROFILE_SCOPE("Frame #%llu", Time::frameCount);

			Window::processMessages();
			if (Window::isResized()) {
				Renderer::resize(Window::getClientSize());
				GameAPI::resize(*game);
			}
			Renderer::prepare();

			GameAPI::update(*game);

			Renderer::render();

			Window::finalizeFrame();
		}
		return 0;
	} catch (std::runtime_error& e) {
		MessageBoxA(0, e.what(), "std::runtime_error", MB_OK);
	}
	return -1;
}
