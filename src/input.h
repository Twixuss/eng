#pragma once
#include "common.h"
enum { 
	JoyAxis_x,
	JoyAxis_y,
	JoyAxis_z,
	JoyAxis_u,
	JoyAxis_v,
	JoyAxis_w,
	JoyAxis_count
};
struct Input {
	struct JoyState {
		f32 axis[6];
		bool buttons[32];
	};
	struct State {
		bool keys[256];
		bool mouse[8];
		JoyState joystick;
	};
	State current, previous;
	v2s mousePosition, mouseDelta;
	s32 mouseWheel;
	
	bool keyHeld(Key k) const { return current.keys[k]; }
	bool keyDown(Key k) const { return current.keys[k] && !previous.keys[k]; }
	bool keyUp(Key k) const { return !current.keys[k] && previous.keys[k]; }
	bool mouseHeld(u32 k) const { return current.mouse[k]; }
	bool mouseDown(u32 k) const { return current.mouse[k] && !previous.mouse[k]; }
	bool mouseUp(u32 k) const { return !current.mouse[k] && previous.mouse[k]; }

	f32 joyAxis(u32 axis) const { return current.joystick.axis[axis]; }
	bool joyButtonHeld(u32 k) const { return current.joystick.buttons[k]; }
	bool joyButtonDown(u32 k) const { return current.joystick.buttons[k] && !previous.joystick.buttons[k]; }
	bool joyButtonUp(u32 k) const { return !current.joystick.buttons[k] && previous.joystick.buttons[k]; }
};
