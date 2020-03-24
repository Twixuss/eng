#include "window.h"

#include "window_internal.cpp"

namespace Window {
void* getHandle() { return hwnd; }
v2u getClientSize() { return clientSize; }
bool isFullscreen() { return fullscreen; }
bool isOpen() { return open; }
bool isResized() { return resized; }
} // namespace Window