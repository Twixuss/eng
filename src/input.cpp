#include "input.h"

#include "input_internal.cpp"

namespace Input {

bool keyHeld(u8 k) { return current.keys[k]; }
bool keyDown(u8 k) { return current.keys[k] && !previous.keys[k]; }
bool keyUp(u8 k) { return !current.keys[k] && previous.keys[k]; }
bool mouseHeld(u8 k) { return current.mouse[k]; }
bool mouseDown(u8 k) { return current.mouse[k] && !previous.mouse[k]; }
bool mouseUp(u8 k) { return !current.mouse[k] && previous.mouse[k]; }
v2i mousePosition() { return current.mousePosition; }
v2i mouseDelta() { return current.mouseDelta; }

} // namespace Input