#pragma once
#include "common.h"
#include "input.h"
#include "renderer.h"
#include "time.h"
#include "window.h"

struct StartupInfo {
	bool resizeable;
	v2u clientSize;
	u8 sampleCount;
};
