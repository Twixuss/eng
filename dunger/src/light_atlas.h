#pragma once
#include "../../src/game.h"
#include "../../src/optimize.h"
#include "random.h"

struct LightAtlas;
struct LightTile;

#define UPDATE_LIGHT_ATLAS(name) void name(LightAtlas &atlas, bool enableCheckerboard, b32 inverseCheckerboard, f32 timeDelta, Span<LightTile> allRaycastTargets, bool threaded)
typedef UPDATE_LIGHT_ATLAS(UpdateLightAtlas);

#define ROUGH_SAMPLE_COUNT 16

struct LightTile {
	v2f boxMin;
	v2f boxMax;
	v3f color;
};
struct LightAtlas {
	static constexpr u32 simdElementCount = TL::simdElementCount<f32>;
	static constexpr u32 maxSampleCount = 128;

	UpdateLightAtlas *optimizedUpdate = 0;

	v2f center{};
	v2f oldCenter{};
	v3f *voxels = 0;
	v2f *samplingCircle = 0;
	u32 sampleCount;
	f32 accumulationRate;
	::Random random;
	v2u size;
	
	
	u32 totalRaysCast;
	u32 totalVolumeChecks;

	v3f *getVoxel(u32 y, u32 x) {
		return voxels + (y * size.x + x) * sampleCount;
	}
	v3f *getRow(u32 y) {
		return voxels + y * size.x * sampleCount;
	}

	v3f *getVoxel(s32 y, s32 x) { return getVoxel((u32)y, (u32)x); }
	v3f *getRow(s32 y) { return getRow((u32)y); }

	u32 probeSize() { return sampleCount * sizeof(voxels[0]); }

	void resize(v2u newSize) {
		size = newSize;
		free(voxels);
		umm dataSize = probeSize() * size.x * size.y;
		voxels = (v3f *)malloc(dataSize);
		memset(voxels, 0, dataSize);
	}
	void init(u32 newSampleCount, float newAccumulationRate) {
		accumulationRate = newAccumulationRate;
		sampleCount = newSampleCount;
		samplingCircle = (v2f *)malloc(sampleCount * sizeof(samplingCircle[0]));
		for (u32 i = 0; i < sampleCount; ++i) {
			f32 angle = (f32)i / sampleCount * (pi * 2);
			sincos(angle, samplingCircle[i].x, samplingCircle[i].y);
		}
	}
	bool move(v2f newCenter) {
		center = newCenter;
		if (center != oldCenter) {
			v2s const d = clamp((v2s)(center - oldCenter), -(v2s)size, (v2s)size);
			v2u const ad = (v2u)absolute(d);

			if (d.x > 0) {
				for (u32 voxelY = 0; voxelY < size.y; ++voxelY) {
					for (u32 voxelX = 0; voxelX < size.x - ad.x; ++voxelX) {
						memcpy(getVoxel(voxelY, voxelX), getVoxel(voxelY, voxelX + ad.x), probeSize());
					}
					for (u32 voxelX = size.x - ad.x; voxelX < size.x; ++voxelX) {
						memset(getVoxel(voxelY, voxelX), 0, probeSize());
					}
				}
			} else if (d.x < 0) {
				for (u32 voxelY = 0; voxelY < size.y; ++voxelY) {
					for (u32 voxelX = size.x - 1; voxelX >= ad.x; --voxelX) {
						memcpy(getVoxel(voxelY, voxelX), getVoxel(voxelY, voxelX - ad.x), probeSize());
					}
					for (u32 voxelX = 0; voxelX < ad.x; ++voxelX) {
						memset(getVoxel(voxelY, voxelX), 0, probeSize());
					}
				}
			}
			if (d.y > 0) {
				for (u32 voxelY = 0; voxelY < size.y - ad.y; ++voxelY) {
					memcpy(getRow(voxelY), getRow(voxelY + ad.y), size.x * probeSize());
				}
				for (u32 voxelY = size.y - ad.y; voxelY < size.y; ++voxelY) {
					memset(getRow(voxelY), 0, size.x * probeSize());
				}
			} else if (d.y < 0) {
				for (u32 voxelY = size.y - 1; voxelY >= ad.y; --voxelY) {
					memcpy(getRow(voxelY), getRow(voxelY - ad.y), size.x * probeSize());
				}
				for (u32 voxelY = 0; voxelY < ad.y; ++voxelY) {
					memset(getRow(voxelY), 0, size.x * probeSize());
				}
			}
		}
		DEFER { oldCenter = center; };
		return center != oldCenter;
	}
	void update(bool enableCheckerboard, b32 inverseCheckerboard, f32 timeDelta, Span<LightTile> allRaycastTargets, bool threaded = true) {
		PROFILE_FUNCTION;
		optimizedUpdate(*this, enableCheckerboard, inverseCheckerboard, timeDelta, allRaycastTargets, threaded);
	}
};

bool estimateLightPerformance(LightAtlas &lightAtlas, u32 &estimatedAtlasHeight, bool &enableCheckerboard, bool &skipOneFrame) {
	float const baseAccumulationRate = 5;
#if BUILD_DEBUG
	lightAtlas.init(16, baseAccumulationRate);
	estimatedAtlasHeight = 12;
	lightAtlas.resize({16, estimatedAtlasHeight});
	enableCheckerboard = true;
#else
	PROFILE_FUNCTION;
	LightAtlas testAtlas;
	testAtlas.init(LightAtlas::maxSampleCount, baseAccumulationRate);
	testAtlas.resize({16, 16});
	testAtlas.optimizedUpdate = lightAtlas.optimizedUpdate;

	StaticList<LightTile, 1024> testTargets;

	::Random random;
	for (u32 i = 0; i < testTargets.capacity(); ++i) {
		LightTile t;
		v2f pos = v2f{random.f32(), random.f32()} * 32;
		v2f size = v2f{random.f32(), random.f32()} + V2f(1);
		t.boxMin = pos - size;
		t.boxMax = pos + size;
		t.color = {random.f32(), random.f32(), random.f32()};
		testTargets.push_back(t);
	}

	auto update = [&] {
		testAtlas.update(false, false, 1.0f / 60.0f, testTargets, false);
	};

	update(); // warmup
	PerfTimer timer;
	update();
	//std::this_thread::sleep_for(std::chrono::milliseconds(500));
	f32 ms = timer.getMilliseconds() / (getWorkerThreadCount() + 1);
	Log::print("base time: {}ms", ms);

	u32 estimatedSampleCount = LightAtlas::maxSampleCount;
	estimatedAtlasHeight = 16;
	
	if (ms > 15) { ms *= 0.5f; estimatedSampleCount /= 2; }
	if (ms > 15) { ms *= 0.5f; enableCheckerboard = true; }
	if (ms > 15) { ms *= 0.5f; skipOneFrame = true; }
	if (ms > 15) { ms *= 0.5f; estimatedAtlasHeight = 12; }
	if (ms > 15) { return false; }

	Log::print("estimatedSampleCount: {}", estimatedSampleCount);
	Log::print("enableCheckerboard: {}", enableCheckerboard);
	Log::print("skipOneFrame: {}", skipOneFrame);
	Log::print("estimatedAtlasHeight: {}", estimatedAtlasHeight);
	
	lightAtlas.init(estimatedSampleCount, skipOneFrame ? baseAccumulationRate * 2 : baseAccumulationRate);
#endif
	return true;
}
