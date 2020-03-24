#pragma once
#include "common.h"
namespace Input {
	ENG_API bool keyHeld(u8 k);
	ENG_API bool keyDown(u8 k);
	ENG_API bool keyUp(u8 k);
	ENG_API bool mouseHeld(u8 k);
	ENG_API bool mouseDown(u8 k);
	ENG_API bool mouseUp(u8 k);
	ENG_API v2i mousePosition();
	ENG_API v2i mouseDelta();
};