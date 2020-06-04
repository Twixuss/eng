#pragma once
#include "common.h"
#include "input.h"
#include "renderer.h"
#include "time.h"
#include "window.h"
#include "audio.h"

struct StartInfo {
	v2u clientSize;
	u32 workerThreadCount;
	RenderingApi renderingApi;
	u8 sampleCount;
	bool resizeable;
};
