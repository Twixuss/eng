#pragma once
#include "common.h"
#include "input.h"
#include "renderer.h"
#include "time.h"
#include "window.h"

struct StartInfo {
	v2u clientSize;
	u32 workerThreadCount;
	u8 sampleCount;
	bool resizeable;
};

struct Audio {
	u32 sampleRate;
};

struct SoundBuffer {
	void *data;
	u32 sampleCount;
	u32 sampleRate;
	u16 numChannels;
	u16 bitsPerSample;
	void *_alloc;
};

ENG_API SoundBuffer loadWaveFile(char const *path);
ENG_API void freeSoundBuffer(SoundBuffer);
