#pragma once
#include "common.h"

struct Audio {
	u32 sampleRate;
	u32 syncIntervalMS;
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
