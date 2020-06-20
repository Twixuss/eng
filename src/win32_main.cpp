#include "common.h"
#include "common_internal.h"
#include "game.h"
#include "input.h"
#include "input_internal.h"
#include "renderer.h"

#include "win32_common.h"

#pragma warning(disable : 4191)

struct Win32Window final : Window {
	bool exitSizeMove;
};
struct Win32Input final : Input {
	static constexpr u32 invalidId = ~0u;
	u32 joystickId;
};

void initWindow(Win32Window &window, HINSTANCE instance, char const *title, v2u clientSize, bool resizeable) {
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
	if(!RegisterClassExA(&wndClass)) {
		Log::error("GetLastError: %lu", GetLastError());
		INVALID_CODE_PATH("RegisterClass");
	}

	u32 windowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
	if (!resizeable) {
		windowStyle &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
	}

	RECT windowRect{0, 0, (LONG)window.clientSize.x, (LONG)window.clientSize.y};
	if (!AdjustWindowRect(&windowRect, windowStyle, 0)) {
		Log::error("GetLastError: %lu", GetLastError());
		INVALID_CODE_PATH("AdjustWindowRect");
	}

	window.handle = CreateWindowExA(0, wndClass.lpszClassName, title, windowStyle, CW_USEDEFAULT, CW_USEDEFAULT,
									windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, 0, 0,
									instance, &window);
	if (!window.handle) {
		Log::error("GetLastError: %lu", GetLastError());
		INVALID_CODE_PATH("CreateWindow");
	}
}
void destroyWindow(Win32Window &window) {}
struct DeferredKeyboardInput {
	u8 key;
	bool value;
};
struct DeferredMouseInput {
	u8 key;
	bool value;
};
StaticList<DeferredKeyboardInput, 4> deferredKeyboardInput, deferredKeyboardInput2;
StaticList<DeferredMouseInput, 4> deferredMouseInput, deferredMouseInput2;
void processMessages(Win32Window &window, Win32Input &input) {
	PROFILE_FUNCTION;
	swap(input);
	MSG msg;
	bool processedKeys[256]{};
	bool processedMouseKeys[5]{};

	u32 const maxDeferFrames = 25;

	// TODO: i don't know what i am doing
	for (auto &m : deferredKeyboardInput) {
		if (processedKeys[m.key]) {
			deferredKeyboardInput2.push_back_pop_front(m);
			continue;
		}
		input.current.keys[m.key] = m.value;
		processedKeys[m.key] = true;
	}
	deferredKeyboardInput = std::move(deferredKeyboardInput2);

	for (auto &m : deferredMouseInput) {
		if (processedMouseKeys[m.key]) {
			deferredMouseInput2.push_back_pop_front(m);
			continue;
		}
		input.current.mouse[m.key] = m.value;
		processedMouseKeys[m.key] = true;
	}
	deferredMouseInput = std::move(deferredMouseInput2);

	while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
		switch (msg.message) {
			case WM_KEYUP:
			case WM_KEYDOWN:
			case WM_SYSKEYUP:
			case WM_SYSKEYDOWN: {
				auto code = (u8)msg.wParam;
				[[maybe_unused]] bool extended = msg.lParam & u32(1 << 24);
				bool alt = msg.lParam & u32(1 << 29);
				bool isRepeated = msg.lParam & u32(1 << 30);
				bool wentUp = msg.lParam & u32(1 << 31);
				if (code == VK_F4 && alt) {
					DestroyWindow((HWND)window.handle);
				}
				if (isRepeated == wentUp) { // Don't handle repeated
					if (processedKeys[code]) {
						deferredKeyboardInput.push_back_pop_front({code, !wentUp});
						continue;
					}
					input.current.keys[code] = !wentUp;
					processedKeys[code] = true;
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

					u8 key = (u8)~0;
					bool value = false;
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN)	{ key = 0; value = true;  }
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP)		{ key = 0; value = false; }
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN)	{ key = 1; value = true;  }
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP)		{ key = 1; value = false; }
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN)	{ key = 2; value = true;  }
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP)		{ key = 2; value = false; }
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)	{ key = 3; value = true;  }
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)		{ key = 3; value = false; }
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)	{ key = 4; value = true;  }
					if (mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)		{ key = 4; value = false; }
					if (key != (u8)~0) {
						if (processedMouseKeys[key]) {
							deferredMouseInput.push_back_pop_front({key, value});
							continue;
						}
						input.current.mouse[key] = value;
						processedMouseKeys[key] = true;
					}
				}
				continue;
			}
		}
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
	if (input.joystickId != input.invalidId) {
		JOYINFOEX ji{sizeof(JOYINFOEX), JOY_RETURNALL};
		switch (joyGetPosEx(input.joystickId, &ji)) {
			case JOYERR_NOERROR: {
				if (ji.dwXpos == 65535) ji.dwXpos--;
				if (ji.dwYpos == 65535) ji.dwYpos--;
				if (ji.dwZpos == 65535) ji.dwZpos--;
				if (ji.dwRpos == 65535) ji.dwRpos--;
				if (ji.dwUpos == 65535) ji.dwUpos--;
				if (ji.dwVpos == 65535) ji.dwVpos--;
				f32 axis[6] {
					(f32)((s32)ji.dwXpos - 32767),
					(f32)((s32)ji.dwYpos - 32767),
					(f32)((s32)ji.dwZpos - 32767),
					(f32)((s32)ji.dwRpos - 32767),
					(f32)((s32)ji.dwUpos - 32767),
					(f32)((s32)ji.dwVpos - 32767)
				};
				for (u32 i = 0; i < JoyAxis_count; ++i) {
					input.current.joystick.axis[i] = clamp(map(axis[i], -32767.0f, 32767.0f, -1.0f, 1.0f), -1.0f, 1.0f);
				}
				for (int i = 0; i < 32; i++)
					input.current.joystick.buttons[i] = ji.dwButtons & (1 << i);
				break;
			}
			case JOYERR_UNPLUGGED:
			case JOYERR_PARMS:
				input.joystickId = input.invalidId;
				break;
		}
	}
	if (window.lostFocus || window.exitSizeMove) {
		reset(input);
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
	if (window.fullscreen) {
		time.delta = 0.01666666666f;
	} else {
		auto endCounter = PerfTimer::syncWithTime(lastPerfCounter, time.targetDelta);
		time.delta = PerfTimer::getSeconds(lastPerfCounter, endCounter);
		lastPerfCounter = endCounter;
	}
	time.time += time.delta;
	++time.frameCount;
}

Win32Input createInput() {
	PROFILE_FUNCTION;

	RAWINPUTDEVICE mouse = {};
	mouse.usUsagePage = 0x01;
	mouse.usUsage = 0x02;

	if (!RegisterRawInputDevices(&mouse, 1, sizeof(RAWINPUTDEVICE))) {
		ASSERT(0);
	}
	
	Win32Input input{};
	
    u32 numDevs = joyGetNumDevs();
    if (numDevs != 0) {
		JOYINFOEX ji;
		ji.dwSize = sizeof(JOYINFOEX);
		ji.dwFlags = 0;

        bool jc1 = joyGetPosEx(JOYSTICKID1, &ji) == JOYERR_NOERROR;
        bool jc2 = numDevs == 2 && joyGetPosEx(JOYSTICKID2, &ji) == JOYERR_NOERROR;
		
		input.joystickId = jc2 ? JOYSTICKID2 : jc1 ? JOYSTICKID1 : input.invalidId;
    } else {
		input.joystickId = input.invalidId;
	}

	return input;
}

FILETIME getLastWriteTime(char const *filename) {
	WIN32_FIND_DATAA data;
	FindFirstFileA(filename, &data);
	return data.ftLastWriteTime;
}

struct Win32Game {
	HMODULE library;
	std::mutex mutex;
	FILETIME lastWriteTime;
	struct State final : EngState {
		decltype(GameApi::fillStartInfo) *fillStartInfo;
		decltype(GameApi::init) *_init;
		decltype(GameApi::start) *_start;
		decltype(GameApi::debugStart) *_debugStart;
		decltype(GameApi::debugReload) *_debugReload;
		decltype(GameApi::update) *_update;
		decltype(GameApi::debugUpdate) *_debugUpdate;
		decltype(GameApi::fillSoundBuffer) *_fillSoundBuffer;
		decltype(GameApi::shutdown) *_shutdown;

		void init() { _init(*this); }
		void start(Window &window, Renderer &renderer, Input &input, Time &time) { _start(*this, window, renderer, input, time); }
		void update(Window &window, Renderer &renderer, Input &input, Time &time) { _update(*this, window, renderer, input, time); }
		void shutdown() { _shutdown(*this); }
		void debugReload() { _debugReload(*this); }
		void fillSoundBuffer(Audio &audio, s16 *subSample, u32 subSampleCount) { _fillSoundBuffer(*this, audio, subSample, subSampleCount); }
		void debugStart(Window &window, Renderer &renderer, Input &input, Time &time, Profiler::Stats const &stats) { _debugStart(*this, window, renderer, input, time, stats); }
		void debugUpdate(Window &window, Renderer &renderer, Input &input, Time &time, Profiler::Stats const &start, Profiler::Stats const &stats) { _debugUpdate(*this, window, renderer, input, time, start, stats); }
	} state;
	
	String<> gameLibName;

	void init() {
		auto lib = LoadLibraryA("game.dll");
		ASSERT(lib);
		DEFER { FreeLibrary(lib); };

		StringBuilder<OsAllocator> builder;
		builder.append(((decltype(GameApi::getName) *)GetProcAddress(lib, "getName"))());
		builder.append(".dll");
		gameLibName = builder.getTerminated();
	}

	void load() {
		PROFILE_FUNCTION;

		u32 tryCount = 0;
		while (!CopyFileA("game.dll", gameLibName.data(), false)) {
			if (++tryCount == 100) {
				INVALID_CODE_PATH("Failed to copy game.dll");
			}
			Sleep(100);
		}

		lastWriteTime = getLastWriteTime("game.dll");

		auto tryLoad = [&]() {
			library = LoadLibraryA(gameLibName.data());
			return library != 0;
		};

		tryCount = 0;
		while (!tryLoad()) {
			if (++tryCount == 100) {
				INVALID_CODE_PATH("Failed to load {}", gameLibName);
			}
			Sleep(100);
		}

		bool success = true;
		auto getProcAddress = [&](auto &dst, char const *name) {
			auto result = GetProcAddress(library, name);
			if (result) {
				dst = (decltype(dst))result;
			} else {
				success = false;
				Log::error("Function {} not found in {}", name, gameLibName);
			}
		};

		getProcAddress(state.fillStartInfo, "fillStartInfo");
		getProcAddress(state._init, "init");
		getProcAddress(state._start, "start");
		getProcAddress(state._debugStart, "debugStart");
		getProcAddress(state._debugReload, "debugReload");
		getProcAddress(state._update, "update");
		getProcAddress(state._debugUpdate, "debugUpdate");
		getProcAddress(state._fillSoundBuffer, "fillSoundBuffer");
		getProcAddress(state._shutdown, "shutdown");

		if (!success) {
			INVALID_CODE_PATH("game api is missing");
		}
	}
	void unload() {
		FreeLibrary(library);
	}
	void reload() {
		mutex.lock();
		unload();
		load();
		mutex.unlock();
		state.debugReload();
	}
	void checkUpdate() {
		auto newTime = getLastWriteTime("game.dll");
		if (CompareFileTime(&newTime, &lastWriteTime) == 1) {
			reload();
		}
	}
};

#include <dsound.h>
#pragma comment(lib,  "dsound")
struct Win32Audio final : Audio {
	static constexpr u32 numChannels = 2;
	static constexpr u32 bytesPerSample = sizeof(s16) * numChannels;

	bool volatile initialized = false;
	LPDIRECTSOUNDBUFFER soundBuffer;
	u32 runningSampleIndex;
	u32 soundBufferSize;
	DWORD lastPlayCursor;

	void fillSoundBuffer(Win32Game &game, DWORD ByteToLock, DWORD BytesToWrite) {
		void* Region1;
		void* Region2;
		DWORD Region1Size;
		DWORD Region2Size;
		DHR(soundBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0));
		ASSERT(Region1Size % bytesPerSample == 0);
		ASSERT(Region2Size % bytesPerSample == 0);

		DWORD Region1SampleCount = Region1Size / bytesPerSample;
		DWORD Region2SampleCount = Region2Size / bytesPerSample;

		if (Region1SampleCount) game.state.fillSoundBuffer(*this, (s16*)Region1, Region1SampleCount);
		if (Region2SampleCount) game.state.fillSoundBuffer(*this, (s16*)Region2, Region2SampleCount);

		runningSampleIndex += Region1SampleCount + Region2SampleCount;

		DHR(soundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size));
	}
	void fillFrame(Win32Game &game/*, Time const &time*/) {
		PROFILE_FUNCTION;
		auto latencySampleCount = sampleRate * syncIntervalMS / 150;

		u32 ByteToLock = (runningSampleIndex * bytesPerSample) % soundBufferSize;

		//u32 expectedBytesPerFrame = (u32)(sampleRate * bytesPerSample * time.targetFrameTime);
		//f32 secondsUntilFlip = time.targetFrameTime - fromBeginToAudioSeconds;
		//u32 expectedBytesUntilFlip = (u32)((secondsUntilFlip / time.targetFrameTime) * (f32)expectedBytesPerFrame);
		//u32 expectedFrameBoundaryByte = playCursor + expectedBytesUntilFlip;
		//u32 safeWriteCursor = writeCursor;

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

	result.syncIntervalMS = 15;
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

	result.initialized = true;

	return result;
}

char const* getRendererDllName(RenderingApi api) {
	switch (api) {
		case RenderingApi::d3d11:  return "d3d11";
		case RenderingApi::opengl: return "gl";
		default: INVALID_CODE_PATH();
	}
}
Renderer createRenderer(RenderingApi renderingApi, Window &window, u32 sampleCount, Format format) {
	PROFILE_FUNCTION;
	char rendererLibName[256];
	sprintf(rendererLibName, "r_%s.dll", getRendererDllName(renderingApi));

	HMODULE renderModule = LoadLibraryA(rendererLibName);
	if (!renderModule) {
		INVALID_CODE_PATH("%s not found", rendererLibName);
	}

	Renderer renderer;
	
	bool foundAllProcs = true;

	struct RendererFn {
		char const *name;
		void **address;
	};
	StaticList<RendererFn, 64> rendererFunctions;

#define R_DECORATE(type, name, args, params) rendererFunctions.push_back({STRINGIZE(RNAME(name)), (void **)&renderer.RNAME(name)})
	R_ALLFUNS;

	for (auto fn : rendererFunctions) {												
		*fn.address = (void *)GetProcAddress(renderModule, fn.name);	
		if (!*fn.address) {												
			foundAllProcs = false;													
			Log::error("Funcion '%s' not found in %s", fn.name, rendererLibName);		
		}																			
	}

	if (!foundAllProcs)
		INVALID_CODE_PATH("Not all procs found in %s", rendererLibName);
		
	renderer.init(window, sampleCount, format);

	return renderer;
}

void printMemoryUsage() {
	Log::print("Memory usage: {}", cvtBytes(getMemoryUsage()));
}
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
	printMemoryUsage();

	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	Log::setHandle(GetStdHandle(STD_OUTPUT_HANDLE));

	timeBeginPeriod(1);
	DEFER { timeEndPeriod(1); };

	printMemoryUsage();

#if 0
	for (u32 i = 0; i < 1024*1024*16; ++i) {
		*((char *)allocateTemp(1)) = (char)i;
	}
	resetTempStorage();
#endif
	
	try {
		loadOptimizedModule();

		Win32Game game{};
		game.init();

		game.load();
		DEFER { game.unload(); };
	
		printMemoryUsage();

		StartInfo startInfo{};
		startInfo.clientSize = {1600, 900};
		startInfo.resizeable = true;
		startInfo.renderingApi = RenderingApi::d3d11;
		startInfo.backBufferFormat = Format::UN_RGBA8;
		startInfo.backBufferSampleCount = 1;
		startInfo.workerThreadCount = cpuInfo.logicalProcessorCount - 1;

		game.state.fillStartInfo(startInfo);

		printMemoryUsage();

		initWorkerThreads(startInfo.workerThreadCount);
		DEFER { shutdownWorkerThreads(); };
		
		printMemoryUsage();
		
		Profiler::init(startInfo.workerThreadCount + 1);
		PROFILE_BEGIN("mainStartup");
		
		printMemoryUsage();
		
		WorkQueue workQueue{};

		workQueue.push([&]{
			game.state.init();			   
		});

		Win32Input input;
		workQueue.push([&] {
			input = createInput();
		});

		Win32Window window;
		initWindow(window, instance, startInfo.windowTitle, startInfo.clientSize, startInfo.resizeable);
		
		printMemoryUsage();

		Renderer renderer;
		workQueue.push([&] {
			renderer = createRenderer(startInfo.renderingApi, window, startInfo.backBufferSampleCount, startInfo.backBufferFormat);
		});

		SyncPoint playAudioSyncPoint{2};
		
		Win32Audio win32Audio;
		std::thread audioThread([&] {
			win32Audio = createAudio((HWND)window.handle);
			sync(playAudioSyncPoint);
			DHR(win32Audio.soundBuffer->Play(0, 0, DSBPLAY_LOOPING));
			s64 lastPerfCounter = PerfTimer::getCounter();
			while (window.open) {
				game.mutex.lock();
				win32Audio.fillFrame(game);
				game.mutex.unlock();

				DWORD PlayCursor;
				DWORD WriteCursor;
				DHR(win32Audio.soundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor));

				win32Audio.lastPlayCursor = PlayCursor;

				lastPerfCounter = PerfTimer::syncWithTime(lastPerfCounter, win32Audio.syncIntervalMS * 0.001f);
			}
		});
		DEFER { audioThread.join(); };
		
		workQueue.completeAllWork();
		printMemoryUsage();

		SetForegroundWindow((HWND)window.handle);

		DEFER { destroyWindow(window); };
		DEFER { renderer.shutdown(); };

		PROFILE_END;
		
		Time time;

		time.frameCount = 0;
		time.targetDelta = 0.016666666f;
		time.time = 0.f;

		game.state.start(window, renderer, input, time);
		DEFER { game.state.shutdown(); };
		
		Profiler::Stats startStats = Profiler::getStats();
		game.state.debugStart(window, renderer, input, time, startStats);
		
		time.delta = time.targetDelta;

		//win32Audio.fillSoundBuffer(*game, 0, win32Audio.soundBufferSize);

		sync(playAudioSyncPoint);

		s64 lastPerfCounter = PerfTimer::getCounter();
		while (window.open) {
			Profiler::reset();
			resetTempStorage();

			game.checkUpdate();
			if (game.state.forceReload) {
				game.state.forceReload = false;
				game.reload();
			}

			processMessages(window, input);
			if (window.resized) {
				reset(input);
				renderer.resize(window);
			}

			game.state.update(window, renderer, input, time);
			
			finalizeFrame(window, lastPerfCounter, time);

			game.state.debugUpdate(window, renderer, input, time, startStats, Profiler::getStats());
		
			renderer.present(window, time);
		}
		return 0;
	} catch (std::runtime_error &e) {
		MessageBoxA(0, e.what(), "std::runtime_error", MB_OK);
		return -1;
	}
}
