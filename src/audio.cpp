#include "audio.h"

#pragma pack(push, 1)
struct WavSubchunk {
	u32 id;
	u32 size;
	void *data() {
		return this + 1;
	}
	WavSubchunk *next() {
		return (WavSubchunk *)((u8 *)data() + size);
	}
};
struct WavHeader {
	u32 chunkId;
	u32 chunkSize;
	u32 format;
	u32 subChunk1Id;
	u32 subChunk1Size;
	u16 audioFormat;
	u16 numChannels;
	u32 sampleRate;
	u32 byteRate;
	u16 blockAlign;
	u16 bitsPerSample;
	WavSubchunk subChunk2;
};
#pragma pack(pop)

SoundBuffer loadWaveFile(char const *path) {
	PROFILE_FUNCTION;
	
	StaticList<WavSubchunk*, 16> subChunks;
	
	SoundBuffer result{};

	auto file = readEntireFile(path);
	if (!file.size())
		goto finish;
		
	auto &header = *(WavHeader *)file.data();

	if (memcmp(&header.chunkId, "RIFF", 4) != 0)
		goto finish;
	if (memcmp(&header.format, "WAVE", 4) != 0)
		goto finish;
	if (memcmp(&header.subChunk1Id, "fmt ", 4) != 0)
		goto finish;
	if (header.audioFormat != 1)
		goto finish;
	if (header.subChunk1Size != 16)
		goto finish;

	for (WavSubchunk *subChunk = &header.subChunk2; (char *)subChunk < file.end(); subChunk = subChunk->next()) {
		subChunks.push_back(subChunk);
	}
	
	WavSubchunk **it = subChunks.find_if([](WavSubchunk *subChunk){ return memcmp(&subChunk->id, "data", 4) == 0; });

	if (it == subChunks.end()) {
		goto finish;
	}
	WavSubchunk *dataSubchunk = *it;

	result._alloc = file.data();
	result.data = dataSubchunk->data();
	result.sampleRate = header.sampleRate;
	result.sampleCount = dataSubchunk->size / header.blockAlign;
	result.numChannels = header.numChannels;
	result.bitsPerSample = header.bitsPerSample;
	
finish:
	if (result._alloc != file.data()) {
		freeEntireFile(file);
	}
	return result;
}
ENG_API void freeSoundBuffer(SoundBuffer buffer) {
	freeEntireFile({(char *)buffer._alloc, (umm)0});
}
