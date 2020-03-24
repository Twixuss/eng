#pragma once

#include "window.h"

#pragma warning(push, 0)
#include <Windows.h>
#pragma warning(pop)

namespace Window {

extern ENG_API HWND hwnd;
extern ENG_API v2u clientSize;
extern ENG_API bool open;
extern ENG_API bool resized;
extern ENG_API bool lostFocus;
extern ENG_API bool fullscreen;
extern ENG_API i64 lastPerfCounter;

ENG_API void init(HINSTANCE instance, WindowCreationInfo);
ENG_API void shutdown();
ENG_API void processMessages();
ENG_API void finalizeFrame();

}
