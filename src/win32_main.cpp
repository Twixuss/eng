#include "common.h"
#include "game.h"
#include "input.h"
#include "renderer.h"

#if OS_WINDOWS
#include "win32_input.h"
#include "win32_window.h"
#endif

#pragma warning(push, 0)
#include <Windows.h>
#pragma warning(pop)

#pragma warning(disable : 4191)

void initWindow(Window& window, HINSTANCE instance, v2u clientSize, bool resizeable) {
	PROFILE_FUNCTION;

	window = {};

	window.open = true;

	if (clientSize == V2u(0)) {
		window.clientSize = {800, 600};
	} else {
		window.clientSize = clientSize;
	}

	WNDCLASSEXA wndClass{};
	wndClass.cbSize = sizeof(wndClass);
	wndClass.hCursor = LoadCursorA(0, IDC_ARROW);
	wndClass.hInstance = instance;
	wndClass.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
		if(msg == WM_CREATE) {
			SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTA*)lp)->lpCreateParams);
			return 0;
		}
		Window* window = (Window*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
		if(window){
			switch (msg) {
				case WM_DESTROY: {
					window->open = false;
					return 0;
				}
				case WM_SIZE: {
					if (!LOWORD(lp) || !HIWORD(lp))
						break;
					window->resized = true;
					window->clientSize.x = LOWORD(lp);
					window->clientSize.y = HIWORD(lp);
					return 0;
				}
				case WM_KILLFOCUS: {
					window->lostFocus = true;
					return 0;
				}
			}
		}
		return DefWindowProcA(hwnd, msg, wp, lp);
	};
	wndClass.lpszClassName = "myClass";
	RegisterClassExA(&wndClass);

	u32 windowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
	if (!resizeable) {
		windowStyle &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
	}

	RECT windowRect{0, 0, (LONG)window.clientSize.x, (LONG)window.clientSize.y};
	AdjustWindowRect(&windowRect, windowStyle, 0);

	window.handle = CreateWindowExA(0, wndClass.lpszClassName, "window name", windowStyle, CW_USEDEFAULT, CW_USEDEFAULT,
						   windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, 0, 0, instance, &window);
	ASSERT(window.handle);

	timeBeginPeriod(1);
}
void destroyWindow(Window& window) { timeEndPeriod(1); }
void processMessages(Window& window, Input& input) {
	PROFILE_FUNCTION;
	input.swap();
	MSG msg;
	while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
		switch (msg.message) {
			case WM_KEYUP:
			case WM_KEYDOWN:
			case WM_SYSKEYUP:
			case WM_SYSKEYDOWN: {
				auto code = (u32)msg.wParam;
				[[maybe_unused]]bool extended = msg.lParam & u32(1 << 24);
				bool alt = msg.lParam & u32(1 << 29);
				bool isRepeated = msg.lParam & u32(1 << 30);
				bool wentUp = msg.lParam & u32(1 << 31);
				if (code == VK_F4 && alt) {
					DestroyWindow((HWND)window.handle);
				}
				if (isRepeated == wentUp) { // Don't handle repeated
					input.current.keys[code] = !isRepeated;
				}
				continue;
			}
			case WM_INPUT: {
				RAWINPUT rawInput;
				if (UINT rawInputSize = sizeof(rawInput);
					GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, &rawInput, &rawInputSize,
									sizeof(RAWINPUTHEADER)) == -1) {
					INVALID_CODE_PATH("Error: GetRawInputData");
				}
				if (rawInput.header.dwType == RIM_TYPEMOUSE) {
					auto& mouse = rawInput.data.mouse;
					input.mouseDelta += {mouse.lLastX, mouse.lLastY};
					if (mouse.usButtonFlags & RI_MOUSE_WHEEL)
						input.mouseWheel += (s16)mouse.usButtonData;
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN)
						input.current.mouse[0] = true;
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP)
						input.current.mouse[0] = false;
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN)
						input.current.mouse[1] = true;
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP)
						input.current.mouse[1] = false;
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN)
						input.current.mouse[2] = true;
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP)
						input.current.mouse[2] = false;
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
						input.current.mouse[3] = true;
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)
						input.current.mouse[3] = false;
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
						input.current.mouse[4] = true;
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)
						input.current.mouse[4] = false;
				}
				continue;
			}
		}
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
	if (window.lostFocus) {
		input.reset();
	}
	POINT p;
	GetCursorPos(&p);
	ScreenToClient((HWND)window.handle, &p);
	input.mousePosition.x = p.x;
	input.mousePosition.y = p.y;
}
void finalizeFrame(Window& window, s64& lastPerfCounter, Time& time) {
	window.lostFocus = false;
	window.resized = false;
	if (!window.fullscreen) {
		auto secondsElapsed = PerfTimer::getSeconds(lastPerfCounter, PerfTimer::getCounter());
		if (secondsElapsed < time.targetFrameTime) {
			s32 msToSleep = (s32)((time.targetFrameTime - secondsElapsed) * 1000.0f);
			if (msToSleep > 0) {
				Sleep((DWORD)msToSleep);
			}
			auto targetCounter = lastPerfCounter + s64(time.targetFrameTime * PerfTimer::frequency);
			while (PerfTimer::getCounter() < targetCounter)
				;
		}
	}
	auto endCounter = PerfTimer::getCounter();
	time.delta = PerfTimer::getSeconds(lastPerfCounter, endCounter);
	lastPerfCounter = endCounter;
	time.time += time.delta;
	++time.frameCount;
}

Input createInput() {
	PROFILE_FUNCTION;
	RAWINPUTDEVICE mouse = {};
	mouse.usUsagePage = 0x01;
	mouse.usUsage = 0x02;

	if (!RegisterRawInputDevices(&mouse, 1, sizeof(RAWINPUTDEVICE))) {
		ASSERT(0);
	}

	Input input{};
	return input;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
	try {
		AllocConsole();
		freopen("CONOUT$", "w", stdout);

		// testAllocator();
		// mathTest();
		// SPSC::circularQueueTest();

		CREATE_PROFILER;

		StartupInfo startupInfo{};
		startupInfo.clientSize = {1280, 720};
		startupInfo.resizeable = true;
		startupInfo.sampleCount = 1;
		GameAPI::fillStartupInfo(startupInfo);

		Window window;
		initWindow(window, instance, startupInfo.clientSize, startupInfo.resizeable);
		DEFER { destroyWindow(window); };

		char const* rendererName = "d3d11";
		char rendererLibName[256];
		sprintf(rendererLibName, "r_%s.dll", rendererName);

		HMODULE renderModule = LoadLibraryA(rendererLibName);
		if(!renderModule){
			printf("%s not found\n", rendererLibName);
			INVALID_CODE_PATH("renderer not loaded");
		}

		auto exportedFunctions = (Renderer::ExportedFunctions*)GetProcAddress(renderModule, "exportedFunctions");
		if(!exportedFunctions) {
			printf("'exportedFunctions' not found in %s", rendererLibName);
			INVALID_CODE_PATH("renderer not loaded");
		}

		bool foundAllProcs = true;
		Renderer renderer;
		renderer.window = &window;
		renderer.drawCalls = 0;
		for (auto& entry : *exportedFunctions) {
			auto proc = (void*)GetProcAddress(renderModule, entry.name);
			if(!proc) {
				foundAllProcs = false;
				printf("Funcion '%s' not found in %s\n", entry.name, rendererLibName);
			}
			(&renderer.functions)[entry.memberOffset] = proc;
		}
		if(!foundAllProcs)
			INVALID_CODE_PATH("got not all procs");

		renderer.init(startupInfo.sampleCount);
		DEFER { renderer.shutdown(); };

		Input input = createInput();

		Game* game = GameAPI::start(window, renderer);
		if (!game)
			return -1;
		DEFER { GameAPI::shutdown(*game); };
		
		Time time;
		
		time.frameCount = 0;
		time.targetFrameTime = 0.016666666f;
		time.delta = time.targetFrameTime;
		time.time = 0.f;

		s64 lastPerfCounter = PerfTimer::getCounter();
		while (window.open) {
			PROFILE_SCOPE("Frame #%llu", time.frameCount);

			processMessages(window, input);
			if (window.resized) {
				input.reset();
				renderer.resize(window.clientSize);
			}
			renderer.prepare();

			GameAPI::update(*game, window, renderer, input, time);

			renderer.render();

			finalizeFrame(window, lastPerfCounter, time);
		}
		return 0;
	} catch (std::runtime_error& e) {
		MessageBoxA(0, e.what(), "std::runtime_error", MB_OK);
	}
	return -1;
}
