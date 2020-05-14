#include "eng.h"
#include "common.cpp"
#include "renderer.cpp"

#pragma pack(push, 1)
struct WavSubchunk {
	u32 id;
	u32 size;
#pragma warning(suppress : 4200)
	u8 data[];
};
struct WavHeader {
	u32 chunkId;
	u32 chunkSize;
	u32 format;
	u32 subchunk1Id;
	u32 subchunk1Size;
	u16 audioFormat;
	u16 numChannels;
	u32 sampleRate;
	u32 byteRate;
	u16 blockAlign;
	u16 bitsPerSample;
	WavSubchunk subchunk2;
};
#pragma pack(pop)

SoundBuffer loadWaveFile(char const *path) {
	PROFILE_FUNCTION;

	SoundBuffer result{};

	auto file = readEntireFile(path);
	do {
		if (!file.size())
			break;
		
		auto &header = *(WavHeader *)file.data();

		if (memcmp(&header.chunkId, "RIFF", 4) != 0)
			break;
		if (memcmp(&header.format, "WAVE", 4) != 0)
			break;
		if (memcmp(&header.subchunk1Id, "fmt ", 4) != 0)
			break;
		if (header.audioFormat != 1)
			break;
		if (header.subchunk1Size != 16)
			break;

		WavSubchunk *dataSubchunk = &header.subchunk2;

		if (memcmp(&dataSubchunk->id, "data", 4) != 0) {
			dataSubchunk = (WavSubchunk *)((char *)dataSubchunk + dataSubchunk->size + 8);
			if (memcmp(&dataSubchunk->id, "data", 4) != 0)
				break;
		}

		result._alloc = file.data();
		result.data = dataSubchunk->data;
		result.sampleRate = header.sampleRate;
		result.sampleCount = dataSubchunk->size / header.blockAlign;
		result.numChannels = header.numChannels;
		result.bitsPerSample = header.bitsPerSample;

	} while (0);

	if (result._alloc != file.data()) {
		freeEntireFile(file);
	}
	return result;
}
ENG_API void freeSoundBuffer(SoundBuffer buffer) {
	freeEntireFile({(char *)buffer._alloc, (umm)0});
}
