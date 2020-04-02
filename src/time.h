#pragma once
#include "common.h"
struct Time {
	u32 frameCount;
	f32 targetFrameTime;
	f32 delta;
	f32 time;
};
