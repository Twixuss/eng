#include "common.h"
#include "game.h"
#include "input.h"
#include "renderer.h"

#include "win32_common.h"

#pragma warning(disable : 4191)

struct Win32Window final : Window {
	bool exitSizeMove;
};
void initWindow(Win32Window &window, HINSTANCE instance, v2u clientSize, bool resizeable) {
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
		static Win32Window *window = 0;
		if (msg == WM_CREATE) {
			window = (Win32Window *)((CREATESTRUCTA *)lp)->lpCreateParams;
			return 0;
		}
		if (window) {
			switch (msg) {
				case WM_DESTROY: {
					window->open = false;
					return 0;
				}
				case WM_SIZE: {
					u32 w = LOWORD(lp);
					u32 h = HIWORD(lp);
					if (w == 0 || h == 0)
						break;
					window->resized = true;
					window->clientSize.x = w;
					window->clientSize.y = h;
					return 0;
				}
				case WM_KILLFOCUS: {
					window->lostFocus = true;
					return 0;
				}
				case WM_EXITSIZEMOVE: {
					window->exitSizeMove = true;
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
									windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, 0, 0,
									instance, &window);
	ASSERT(window.handle);

	timeBeginPeriod(1);
}
void destroyWindow(Win32Window &window) { timeEndPeriod(1); }
void processMessages(Win32Window &window, Input &input) {
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
				[[maybe_unused]] bool extended = msg.lParam & u32(1 << 24);
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
					auto &mouse = rawInput.data.mouse;
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
	if (window.lostFocus || window.exitSizeMove) {
		input.reset();
	}
	POINT p;
	GetCursorPos(&p);
	ScreenToClient((HWND)window.handle, &p);
	input.mousePosition.x = p.x;
	input.mousePosition.y = p.y;
}
void finalizeFrame(Win32Window &window, s64 &lastPerfCounter, Time &time) {
	PROFILE_FUNCTION;
	window.lostFocus = false;
	window.resized = false;
	window.exitSizeMove = false;
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
	RAWINPUTDEVICE mouse = {};
	mouse.usUsagePage = 0x01;
	mouse.usUsage = 0x02;

	if (!RegisterRawInputDevices(&mouse, 1, sizeof(RAWINPUTDEVICE))) {
		ASSERT(0);
	}

	Input input{};
	return input;
}

#include <dsound.h>
#pragma comment(lib,  "dsound")
struct Win32Audio final : Audio {
	static constexpr u32 numChannels = 2;
	static constexpr u32 bytesPerSample = sizeof(s16) * numChannels;

	LPDIRECTSOUNDBUFFER soundBuffer;
	u32 runningSampleIndex;
	u32 soundBufferSize;
	DWORD lastPlayCursor;

	void fillSoundBuffer(GameState& game, DWORD ByteToLock, DWORD BytesToWrite) {
		void* Region1;
		void* Region2;
		DWORD Region1Size;
		DWORD Region2Size;
		DHR(soundBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0));
		ASSERT(Region1Size % bytesPerSample == 0);
		ASSERT(Region2Size % bytesPerSample == 0);

		DWORD Region1SampleCount = Region1Size / bytesPerSample;
		DWORD Region2SampleCount = Region2Size / bytesPerSample;

		if (Region1SampleCount) GameAPI::fillSoundBuffer(game, *this, (s16*)Region1, Region1SampleCount);
		if (Region2SampleCount) GameAPI::fillSoundBuffer(game, *this, (s16*)Region2, Region2SampleCount);

		runningSampleIndex += Region1SampleCount + Region2SampleCount;

		DHR(soundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size));
	}
	void fillFrame(GameState& game) {
		auto latencySampleCount = sampleRate / 12;
		u32 ByteToLock = (runningSampleIndex * bytesPerSample) % soundBufferSize;
		u32 TargetCursor = (lastPlayCursor + latencySampleCount * bytesPerSample) % soundBufferSize;
		u32 BytesToWrite;
		if (ByteToLock > TargetCursor) {
			BytesToWrite = soundBufferSize - ByteToLock + TargetCursor;
		} else {
			BytesToWrite = TargetCursor - ByteToLock;
		}
		if (BytesToWrite) { // TODO: ???
			fillSoundBuffer(game, ByteToLock, BytesToWrite);
		}
	}
};

Win32Audio createAudio(HWND hwnd) {
	PROFILE_FUNCTION;

	Win32Audio result;

	result.lastPlayCursor = 0;
	result.runningSampleIndex = 0;
	result.sampleRate = 48000;

	LPDIRECTSOUND dsound;
	DHR(DirectSoundCreate(0, &dsound, 0));
	DHR(dsound->SetCooperativeLevel(hwnd, DSSCL_PRIORITY));

	DSBUFFERDESC desc = {};
	desc.dwSize = sizeof(desc);
	desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

	LPDIRECTSOUNDBUFFER PrimaryBuffer;
	DHR(dsound->CreateSoundBuffer(&desc, &PrimaryBuffer, 0));

	WAVEFORMATEX wfx;

	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = result.numChannels;
	wfx.nSamplesPerSec = result.sampleRate;
	wfx.wBitsPerSample = result.bytesPerSample * 8u / result.numChannels;
	wfx.nBlockAlign = result.bytesPerSample;
	wfx.nAvgBytesPerSec = result.bytesPerSample * result.sampleRate;
	wfx.cbSize = sizeof(wfx);

	DHR(PrimaryBuffer->SetFormat(&wfx));

	u32 bufferLengthInSeconds = 1;
	result.soundBufferSize = wfx.nAvgBytesPerSec * bufferLengthInSeconds;
	desc.dwBufferBytes = result.soundBufferSize;
	desc.dwFlags = 0;
	desc.lpwfxFormat = &wfx;

	DHR(dsound->CreateSoundBuffer(&desc, &result.soundBuffer, 0));

	return result;
}

char const* getRendererDllName(RenderingApi api) {
	switch (api) {
		case RenderingApi::d3d11:  return "d3d11";
		case RenderingApi::opengl: return "gl";
		default: INVALID_CODE_PATH();
	}
}
Renderer createRenderer(RenderingApi renderingApi, Window &window, u32 sampleCount) {
	PROFILE_FUNCTION;
	char rendererLibName[256];
	sprintf(rendererLibName, "r_%s.dll", getRendererDllName(renderingApi));

	HMODULE renderModule = LoadLibraryA(rendererLibName);
	if (!renderModule) {
		INVALID_CODE_PATH("%s not found", rendererLibName);
	}

	Renderer renderer;
	renderer.window = &window;
	renderer.drawCalls = 0;
	
	bool foundAllProcs = true;

#define R_DECORATE(type, name, args, params)										\
	{																				\
		char const *fn = STRINGIZE(RNAME(name));									\
		*(void **)&renderer.RNAME(name) = (void *)GetProcAddress(renderModule, fn);	\
		if (!renderer.RNAME(name)) {												\
			foundAllProcs = false;													\
			Log::print("Funcion '%s' not found in %s", fn, rendererLibName);		\
		}																			\
	}

	R_ALLFUNS;
	if (!foundAllProcs)
		INVALID_CODE_PATH("Not all procs found in %s", rendererLibName);
		
	renderer.init(sampleCount);

	return renderer;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
	try {
		Profiler::reset();
		resetThreadStats();
		PROFILE_BEGIN("mainStartup");

		StartInfo startInfo{};
		startInfo.clientSize = {1600, 900};
		startInfo.resizeable = true;
		startInfo.renderingApi = RenderingApi::d3d11;
		startInfo.sampleCount = 1;
		startInfo.workerThreadCount = cpuInfo.logicalProcessorCount;

		GameAPI::fillStartInfo(startInfo);

		initWorkerThreads(startInfo.workerThreadCount);

#if 0
		WorkQueue testWork;
		for(u32 i=0;i<1000;++i){
			testWork.push("test", []{
				PROFILE_FUNCTION;
				puts("Hello!");
			});
		}
		testWork.completeAllWork();
#endif

		WorkQueue workQueue{};

		PROFILE_BEGIN("AllocConsole");
		AllocConsole();
		freopen("CONOUT$", "w", stdout);
		PROFILE_END;

		Win32Window window;
		initWindow(window, instance, startInfo.clientSize, startInfo.resizeable);
		DEFER { destroyWindow(window); };
		
		Renderer renderer;
		workQueue.push("createRenderer", [&] {
			renderer = createRenderer(startInfo.renderingApi, window, startInfo.sampleCount);
		});
		DEFER { renderer.shutdown(); };

		Win32Audio win32Audio;
		workQueue.push("createAudio", [&] {
			win32Audio = createAudio((HWND)window.handle);
		});

		Input input;
		workQueue.push("createInput", [&] {
			input = createInput();
		});

		workQueue.completeAllWork();

		PROFILE_END;

		Profiler::prepareStats();

		GameState *game = GameAPI::start(window, renderer);
		if (!game)
			return -1;
		DEFER { GameAPI::shutdown(*game); };
		
		//win32Audio.fillSoundBuffer(*game, 0, win32Audio.soundBufferSize);
		DHR(win32Audio.soundBuffer->Play(0, 0, DSBPLAY_LOOPING));

		ShowCursor(false);

		Time time;

		time.frameCount = 0;
		time.targetFrameTime = 0.016666666f;
		time.delta = time.targetFrameTime;
		time.time = 0.f;

		s64 lastPerfCounter = PerfTimer::getCounter();
		while (window.open) {
			Profiler::reset();
			resetThreadStats();

			processMessages(window, input);
			if (window.resized) {
				input.reset();
				renderer.resize(window.clientSize);
			}
			renderer.prepare();

			GameAPI::update(*game, window, renderer, input, time);
			
			win32Audio.fillFrame(*game);

			finalizeFrame(window, lastPerfCounter, time);

			Profiler::prepareStats();

			GameAPI::endFrame(*game, window, renderer, input, time);
			
			renderer.present();

			DWORD PlayCursor;
			DWORD WriteCursor;
			DHR(win32Audio.soundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor));

			win32Audio.lastPlayCursor = PlayCursor;
		}

		shutdownWorkerThreads();

		return 0;
	} catch (std::runtime_error &e) {
		MessageBoxA(0, e.what(), "std::runtime_error", MB_OK);
	}
	return -1;
}
