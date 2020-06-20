#pragma once
#include "common.h"
#include "input.h"
#include "renderer.h"
#include "time.h"
#include "window.h"
#include "audio.h"

struct StartInfo {
	char const *windowTitle;
	v2u clientSize;
	u32 workerThreadCount;
	RenderingApi renderingApi;
	u8 backBufferSampleCount;
	Format backBufferFormat;
	bool resizeable;
};

struct EngState {
	void *userData;
	bool forceReload;
};
