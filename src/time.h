#pragma once
#include "common.h"
struct Time {
	u32 frameCount;
	f32 targetDelta;
	f32 delta;
	f32 time;
};
