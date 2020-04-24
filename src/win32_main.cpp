#include "common.h"
#include "game.h"
#include "input.h"
#include "renderer.h"

#include "win32_common.h"

#pragma warning(disable : 4191)

char const *winMessageToString(UINT msg) {
#define C(x) \
	case x: return #x;
	switch (msg) {
		C(WM_NULL);
		C(WM_CREATE);
		C(WM_DESTROY);
		C(WM_MOVE);
		C(WM_SIZE);
		C(WM_ACTIVATE);
		C(WM_SETFOCUS);
		C(WM_KILLFOCUS);
		C(WM_ENABLE);
		C(WM_SETREDRAW);
		C(WM_SETTEXT);
		C(WM_GETTEXT);
		C(WM_GETTEXTLENGTH);
		C(WM_PAINT);
		C(WM_CLOSE);
		C(WM_QUERYENDSESSION);
		C(WM_QUIT);
		C(WM_QUERYOPEN);
		C(WM_ERASEBKGND);
		C(WM_SYSCOLORCHANGE);
		C(WM_ENDSESSION);
		C(WM_SHOWWINDOW);
		// C(WM_CTLCOLOR);
		C(WM_WININICHANGE);
		C(WM_DEVMODECHANGE);
		C(WM_ACTIVATEAPP);
		C(WM_FONTCHANGE);
		C(WM_TIMECHANGE);
		C(WM_CANCELMODE);
		C(WM_SETCURSOR);
		C(WM_MOUSEACTIVATE);
		C(WM_CHILDACTIVATE);
		C(WM_QUEUESYNC);
		C(WM_GETMINMAXINFO);
		C(WM_PAINTICON);
		C(WM_ICONERASEBKGND);
		C(WM_NEXTDLGCTL);
		C(WM_SPOOLERSTATUS);
		C(WM_DRAWITEM);
		C(WM_MEASUREITEM);
		C(WM_DELETEITEM);
		C(WM_VKEYTOITEM);
		C(WM_CHARTOITEM);
		C(WM_SETFONT);
		C(WM_GETFONT);
		C(WM_SETHOTKEY);
		C(WM_GETHOTKEY);
		C(WM_QUERYDRAGICON);
		C(WM_COMPAREITEM);
		C(WM_GETOBJECT);
		C(WM_COMPACTING);
		C(WM_COMMNOTIFY);
		C(WM_WINDOWPOSCHANGING);
		C(WM_WINDOWPOSCHANGED);
		C(WM_POWER);
		// C(WM_COPYGLOBALDATA);
		C(WM_COPYDATA);
		C(WM_CANCELJOURNAL);
		C(WM_NOTIFY);
		C(WM_INPUTLANGCHANGEREQUEST);
		C(WM_INPUTLANGCHANGE);
		C(WM_TCARD);
		C(WM_HELP);
		C(WM_USERCHANGED);
		C(WM_NOTIFYFORMAT);
		C(WM_CONTEXTMENU);
		C(WM_STYLECHANGING);
		C(WM_STYLECHANGED);
		C(WM_DISPLAYCHANGE);
		C(WM_GETICON);
		C(WM_SETICON);
		C(WM_NCCREATE);
		C(WM_NCDESTROY);
		C(WM_NCCALCSIZE);
		C(WM_NCHITTEST);
		C(WM_NCPAINT);
		C(WM_NCACTIVATE);
		C(WM_GETDLGCODE);
		C(WM_SYNCPAINT);
		C(WM_NCMOUSEMOVE);
		C(WM_NCLBUTTONDOWN);
		C(WM_NCLBUTTONUP);
		C(WM_NCLBUTTONDBLCLK);
		C(WM_NCRBUTTONDOWN);
		C(WM_NCRBUTTONUP);
		C(WM_NCRBUTTONDBLCLK);
		C(WM_NCMBUTTONDOWN);
		C(WM_NCMBUTTONUP);
		C(WM_NCMBUTTONDBLCLK);
		C(WM_NCXBUTTONDOWN);
		C(WM_NCXBUTTONUP);
		C(WM_NCXBUTTONDBLCLK);
		C(WM_INPUT);
		C(WM_KEYDOWN);
		C(WM_KEYUP);
		C(WM_CHAR);
		C(WM_DEADCHAR);
		C(WM_SYSKEYDOWN);
		C(WM_SYSKEYUP);
		C(WM_SYSCHAR);
		C(WM_SYSDEADCHAR);
		C(WM_UNICHAR);
		// C(WM_WNT_CONVERTREQUESTEX);
		// C(WM_CONVERTREQUEST);
		// C(WM_CONVERTRESULT);
		// C(WM_INTERIM);
		C(WM_IME_STARTCOMPOSITION);
		C(WM_IME_ENDCOMPOSITION);
		C(WM_IME_COMPOSITION);
		C(WM_INITDIALOG);
		C(WM_COMMAND);
		C(WM_SYSCOMMAND);
		C(WM_TIMER);
		C(WM_HSCROLL);
		C(WM_VSCROLL);
		C(WM_INITMENU);
		C(WM_INITMENUPOPUP);
		// C(WM_SYSTIMER);
		C(WM_MENUSELECT);
		C(WM_MENUCHAR);
		C(WM_ENTERIDLE);
		C(WM_MENURBUTTONUP);
		C(WM_MENUDRAG);
		C(WM_MENUGETOBJECT);
		C(WM_UNINITMENUPOPUP);
		C(WM_MENUCOMMAND);
		C(WM_CHANGEUISTATE);
		C(WM_UPDATEUISTATE);
		C(WM_QUERYUISTATE);
		C(WM_CTLCOLORMSGBOX);
		C(WM_CTLCOLOREDIT);
		C(WM_CTLCOLORLISTBOX);
		C(WM_CTLCOLORBTN);
		C(WM_CTLCOLORDLG);
		C(WM_CTLCOLORSCROLLBAR);
		C(WM_CTLCOLORSTATIC);
		C(WM_MOUSEMOVE);
		C(WM_LBUTTONDOWN);
		C(WM_LBUTTONUP);
		C(WM_LBUTTONDBLCLK);
		C(WM_RBUTTONDOWN);
		C(WM_RBUTTONUP);
		C(WM_RBUTTONDBLCLK);
		C(WM_MBUTTONDOWN);
		C(WM_MBUTTONUP);
		C(WM_MBUTTONDBLCLK);
		C(WM_MOUSELAST);
		C(WM_MOUSEWHEEL);
		C(WM_XBUTTONDOWN);
		C(WM_XBUTTONUP);
		C(WM_XBUTTONDBLCLK);
		C(WM_PARENTNOTIFY);
		C(WM_ENTERMENULOOP);
		C(WM_EXITMENULOOP);
		C(WM_NEXTMENU);
		C(WM_SIZING);
		C(WM_CAPTURECHANGED);
		C(WM_MOVING);
		C(WM_POWERBROADCAST);
		C(WM_DEVICECHANGE);
		C(WM_MDICREATE);
		C(WM_MDIDESTROY);
		C(WM_MDIACTIVATE);
		C(WM_MDIRESTORE);
		C(WM_MDINEXT);
		C(WM_MDIMAXIMIZE);
		C(WM_MDITILE);
		C(WM_MDICASCADE);
		C(WM_MDIICONARRANGE);
		C(WM_MDIGETACTIVE);
		C(WM_MDISETMENU);
		C(WM_ENTERSIZEMOVE);
		C(WM_EXITSIZEMOVE);
		C(WM_DROPFILES);
		C(WM_MDIREFRESHMENU);
		// C(WM_IME_REPORT);
		C(WM_IME_SETCONTEXT);
		C(WM_IME_NOTIFY);
		C(WM_IME_CONTROL);
		C(WM_IME_COMPOSITIONFULL);
		C(WM_IME_SELECT);
		C(WM_IME_CHAR);
		C(WM_IME_REQUEST);
		// C(WM_IMEKEYDOWN);
		C(WM_IME_KEYDOWN);
		// C(WM_IMEKEYUP);
		C(WM_IME_KEYUP);
		C(WM_NCMOUSEHOVER);
		C(WM_MOUSEHOVER);
		C(WM_NCMOUSELEAVE);
		C(WM_MOUSELEAVE);
		C(WM_CUT);
		C(WM_COPY);
		C(WM_PASTE);
		C(WM_CLEAR);
		C(WM_UNDO);
		C(WM_RENDERFORMAT);
		C(WM_RENDERALLFORMATS);
		C(WM_DESTROYCLIPBOARD);
		C(WM_DRAWCLIPBOARD);
		C(WM_PAINTCLIPBOARD);
		C(WM_VSCROLLCLIPBOARD);
		C(WM_SIZECLIPBOARD);
		C(WM_ASKCBFORMATNAME);
		C(WM_CHANGECBCHAIN);
		C(WM_HSCROLLCLIPBOARD);
		C(WM_QUERYNEWPALETTE);
		C(WM_PALETTEISCHANGING);
		C(WM_PALETTECHANGED);
		C(WM_HOTKEY);
		C(WM_PRINT);
		C(WM_PRINTCLIENT);
		C(WM_APPCOMMAND);
		C(WM_HANDHELDFIRST);
		C(WM_HANDHELDLAST);
		C(WM_AFXFIRST);
		C(WM_AFXLAST);
		C(WM_PENWINFIRST);
		// C(WM_RCRESULT);
		// C(WM_HOOKRCRESULT);
		// C(WM_GLOBALRCCHANGE);
		// C(WM_PENMISCINFO);
		// C(WM_SKB);
		// C(WM_HEDITCTL);
		// C(WM_PENCTL);
		// C(WM_PENMISC);
		// C(WM_CTLINIT);
		// C(WM_PENEVENT);
		C(WM_PENWINLAST);
		C(WM_USER);
		C(WM_CHOOSEFONT_GETLOGFONT);
		C(WM_PSD_MINMARGINRECT);
		C(WM_PSD_MARGINRECT);
		C(WM_PSD_GREEKTEXTRECT);
		C(WM_PSD_ENVSTAMPRECT);
		C(WM_PSD_YAFULLPAGERECT);
		// C(WM_CAP_UNICODE_START);
		C(WM_CHOOSEFONT_SETLOGFONT);
		// C(WM_CAP_SET_CALLBACK_ERRORW);
		C(WM_CHOOSEFONT_SETFLAGS);
		// C(WM_CAP_SET_CALLBACK_STATUSW);
		// C(WM_CAP_DRIVER_GET_NAMEW);
		// C(WM_CAP_DRIVER_GET_VERSIONW);
		// C(WM_CAP_FILE_SET_CAPTURE_FILEW);
		// C(WM_CAP_FILE_GET_CAPTURE_FILEW);
		// C(WM_CAP_FILE_SAVEASW);
		// C(WM_CAP_FILE_SAVEDIBW);
		// C(WM_CAP_SET_MCI_DEVICEW);
		// C(WM_CAP_GET_MCI_DEVICEW);
		// C(WM_CAP_PAL_OPENW);
		// C(WM_CAP_PAL_SAVEW);
		// C(WM_CPL_LAUNCH);
		// C(WM_CPL_LAUNCHED);
		C(WM_APP);
		// C(WM_RASDIALEVENT);
	}
#undef C
	return "";
}
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
		if (msg == WM_CREATE) {
			SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTA *)lp)->lpCreateParams);
			return 0;
		}
		Win32Window *window = (Win32Window *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
		if (window) {
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
				case WM_EXITSIZEMOVE: {
					window->exitSizeMove = true;
				}
			}
			// puts(winMessageToString(msg));
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
struct Win32Audio {
	static constexpr u32 numChannels = 2;
	static constexpr u32 bytesPerSample = sizeof(s16) * numChannels;
	Audio audio;
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

		if (Region1SampleCount) GameAPI::fillSoundBuffer(game, audio, (s16*)Region1, Region1SampleCount);
		if (Region2SampleCount) GameAPI::fillSoundBuffer(game, audio, (s16*)Region2, Region2SampleCount);

		runningSampleIndex += Region1SampleCount + Region2SampleCount;

		DHR(soundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size));
	}
	void fillFrame(GameState& game) {
		auto latencySampleCount = audio.sampleRate / 12;
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
	result.audio.sampleRate = 48000;

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
	wfx.nSamplesPerSec = result.audio.sampleRate;
	wfx.wBitsPerSample = result.bytesPerSample * 8u / result.numChannels;
	wfx.nBlockAlign = result.bytesPerSample;
	wfx.nAvgBytesPerSec = result.bytesPerSample * result.audio.sampleRate;
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

Renderer createRenderer(char const *rendererName, Window &window, u32 sampleCount) {
	PROFILE_FUNCTION;
	char rendererLibName[256];
	sprintf(rendererLibName, "r_%s.dll", rendererName);

	HMODULE renderModule = LoadLibraryA(rendererLibName);
	if (!renderModule) {
		INVALID_CODE_PATH("%s not found", rendererLibName);
	}

	auto exportedFunctions = (Renderer::ExportedFunctions *)GetProcAddress(renderModule, "exportedFunctions");
	if (!exportedFunctions) {
		INVALID_CODE_PATH("'exportedFunctions' not found in %s", rendererLibName);
	}

	bool foundAllProcs = true;
	Renderer renderer;
	renderer.window = &window;
	renderer.drawCalls = 0;
	for (auto &entry : *exportedFunctions) {
		auto proc = (void *)GetProcAddress(renderModule, entry.name);
		if (!proc) {
			foundAllProcs = false;
			Log::print("Funcion '%s' not found in %s", entry.name, rendererLibName);
		}
		(&renderer.functions)[entry.memberOffset] = proc;
	}
	if (!foundAllProcs)
		INVALID_CODE_PATH("Not all procs found in %s", rendererLibName);
		
	renderer.init(sampleCount);

	return renderer;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
	try {
		Profiler::reset();
		PROFILE_BEGIN("mainStartup");

		PROFILE_BEGIN("AllocConsole");
		AllocConsole();
		freopen("CONOUT$", "w", stdout);
		PROFILE_END;

		// testAllocator();
		// mathTest();
		// SPSC::circularQueueTest();

		StartInfo startInfo{};
		startInfo.clientSize = {1600, 900};
		startInfo.resizeable = true;
		startInfo.sampleCount = 1;
		startInfo.workerThreadCount = cpuInfo.logicalProcessorCount;

		GameAPI::fillStartInfo(startInfo);

		if(startInfo.workerThreadCount > cpuInfo.logicalProcessorCount) {
			startInfo.workerThreadCount = cpuInfo.logicalProcessorCount;
			Log::warn("WARNING: Clamping 'startInfo.workerThreadCount' to number of CPU cores (%u)", cpuInfo.logicalProcessorCount);
		}

		initWorkerThreads(startInfo.workerThreadCount);

		Win32Window window;
		initWindow(window, instance, startInfo.clientSize, startInfo.resizeable);
		DEFER { destroyWindow(window); };

		Renderer renderer;
		pushWork("createRenderer", [&] {
			renderer = createRenderer("d3d11", window, startInfo.sampleCount);
		});
		DEFER { renderer.shutdown(); };

		Win32Audio win32Audio;
		pushWork("createAudio", [&] {
			win32Audio = createAudio((HWND)window.handle);
		});

		Input input;
		pushWork("createInput", [&] {
			input = createInput();
		});

		completeAllWork();

		PROFILE_END;

		GameState *game = GameAPI::start(window, renderer);
		if (!game)
			return -1;
		DEFER { GameAPI::shutdown(*game); };
		
		win32Audio.fillSoundBuffer(*game, 0, win32Audio.soundBufferSize);
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

			renderer.present();

			win32Audio.fillFrame(*game);

			finalizeFrame(window, lastPerfCounter, time);

			
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
