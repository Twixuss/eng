#pragma once

#include "input.h"
#include "time.h"

struct Window {
	void* handle;
	v2u clientSize;
	bool open;
	bool resized;
	bool lostFocus;
	bool fullscreen;
}; 