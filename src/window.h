#pragma once

#include "input.h"
#include "time.h"

struct WindowCreationInfo {
	bool resizeable;
	v2u clientSize;
};

namespace Window {
ENG_API void* getHandle();
ENG_API v2u getClientSize();
ENG_API bool isFullscreen();
ENG_API bool isOpen();
ENG_API bool isResized();
}; // namespace Window