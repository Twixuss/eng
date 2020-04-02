#pragma once
#include "common.h"
struct Input {
	struct KeyState {
		bool keys[256];
		bool mouse[8];
	};
	KeyState current, previous;
	v2s mousePosition, mouseDelta;
	s32 mouseWheel;

	bool keyHeld(u8 k) const { return current.keys[k]; }
	bool keyDown(u8 k) const { return current.keys[k] && !previous.keys[k]; }
	bool keyUp(u8 k) const { return !current.keys[k] && previous.keys[k]; }
	bool mouseHeld(u8 k) const { return current.mouse[k]; }
	bool mouseDown(u8 k) const { return current.mouse[k] && !previous.mouse[k]; }
	bool mouseUp(u8 k) const { return !current.mouse[k] && previous.mouse[k]; }

	
	void swap() {
		previous = current;
		mouseDelta = {};
		mouseWheel = {};
	}
	void reset() {
		memset(&current, 0, sizeof(current));
	}
};