#include "window_internal.h"
#include "input_internal.h"

#pragma warning(push, 0)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#pragma warning(pop)

namespace Window {
HWND hwnd;
v2u clientSize;
bool open = true;
bool resized = false;
bool lostFocus = false;
bool fullscreen = false;
i64 lastPerfCounter;
void init(HINSTANCE instance, WindowCreationInfo info) {
	PROFILE_FUNCTION;

	if (info.clientSize == V2u(0)) {
		clientSize = {800, 600};
	} else {
		clientSize = info.clientSize;
	}

	WNDCLASSEXA wndClass{};
	wndClass.cbSize = sizeof(wndClass);
	wndClass.hCursor = LoadCursorA(0, IDC_ARROW);
	wndClass.hInstance = instance;
	wndClass.lpfnWndProc = [](HWND _hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
		switch (msg) {
			case WM_DESTROY: {
				open = false;
				return 0;
			}
			case WM_SIZE: {
				if (!LOWORD(lp) || !HIWORD(lp))
					break;
				resized = true;
				clientSize.x = LOWORD(lp);
				clientSize.y = HIWORD(lp);
				return 0;
			}
			case WM_KILLFOCUS: {
				lostFocus = true;
				return 0;
			}
		}
		return DefWindowProcA(_hwnd, msg, wp, lp);
	};
	wndClass.lpszClassName = "myClass";
	RegisterClassExA(&wndClass);

	u32 windowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
	if (!info.resizeable) {
		windowStyle &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
	}

	RECT windowRect{0, 0, (LONG)clientSize.x, (LONG)clientSize.y};
	AdjustWindowRect(&windowRect, windowStyle, 0);

	hwnd = CreateWindowExA(0, wndClass.lpszClassName, "window name", windowStyle, CW_USEDEFAULT, CW_USEDEFAULT,
						   windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, 0, 0, instance, 0);
	ASSERT(hwnd);

	timeBeginPeriod(1);

	lastPerfCounter = PerfTimer::getCounter();
}
void shutdown() { timeEndPeriod(1); }
void processMessages() {
	PROFILE_FUNCTION;
	Input::swap();
	if (lostFocus) {
		lostFocus = false;
		Input::reset();
	}
	MSG msg;
	while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
		switch (msg.message) {
			case WM_KEYUP:
			case WM_KEYDOWN:
			case WM_SYSKEYUP:
			case WM_SYSKEYDOWN: {
				auto code = (u32)msg.wParam;
				bool extended = msg.lParam & u32(1 << 24);
				bool alt = msg.lParam & u32(1 << 29);
				bool isRepeated = msg.lParam & u32(1 << 30);
				bool wentUp = msg.lParam & u32(1 << 31);
				if (code == VK_F4 && alt) {
					DestroyWindow(hwnd);
				}
				Input::processKey(code, extended, alt, isRepeated, wentUp);
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
					Input::current.mouseDelta += {mouse.lLastX, mouse.lLastY};
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN)
						Input::setMouseButton(0, 1);
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP)
						Input::setMouseButton(0, 0);
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN)
						Input::setMouseButton(1, 1);
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP)
						Input::setMouseButton(1, 0);
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN)
						Input::setMouseButton(2, 1);
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP)
						Input::setMouseButton(2, 0);
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
						Input::setMouseButton(3, 1);
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)
						Input::setMouseButton(3, 0);
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
						Input::setMouseButton(4, 1);
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)
						Input::setMouseButton(4, 0);
				}
				continue;
			}
		}
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
	POINT p;
	GetCursorPos(&p);
	ScreenToClient(hwnd, &p);
	Input::current.mousePosition.x = p.x;
	Input::current.mousePosition.y = p.y;
}
void finalizeFrame() {
	resized = false;
	if (!fullscreen) {
		auto secondsElapsed = PerfTimer::getSeconds(lastPerfCounter, PerfTimer::getCounter());
		if (secondsElapsed < Time::targetFrameTime) {
			i32 msToSleep = (i32)((Time::targetFrameTime - secondsElapsed) * 1000.0f);
			if (msToSleep > 0) {
				Sleep((DWORD)msToSleep);
			}
			auto targetCounter = lastPerfCounter + i64(Time::targetFrameTime * PerfTimer::frequency);
			while (PerfTimer::getCounter() < targetCounter)
				;
		}
	}
	auto endCounter = PerfTimer::getCounter();
	Time::frameTime = PerfTimer::getSeconds(lastPerfCounter, endCounter);
	lastPerfCounter = endCounter;
	Time::time += Time::frameTime;
	++Time::frameCount;
}
} // namespace Window