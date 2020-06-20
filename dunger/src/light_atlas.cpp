#include "../../src/optimize.h"
#include "light_atlas.h"

OPTIMIZE_EXPORT UPDATE_LIGHT_ATLAS(updateLightAtlas) {
	Atomic<u32> totalRaysCast = 0;
	Atomic<u32> totalVolumeChecks = 0;

	constexpr s32xm sampleOffsetsx = []() {
		if constexpr (LightAtlas::simdElementCount == 4)
			return s32x4{0,1,2,3};
		else if constexpr (LightAtlas::simdElementCount == 8)
			return s32x8{0, 1, 2, 3, 4, 5, 6, 7};
		else
			static_assert(false, "can't initialize 'sampleOffsetsx'");
	}();

#define CAST_NO_SIMD 0
#define CAST_N_RAYS_OVER_1_TILE 1

#define CAST_METHOD CAST_N_RAYS_OVER_1_TILE

#define RESTRICT_NONE 0
#define RESTRICT_ROW  1
#define RESTRICT_CELL 2

#define RESTRICT_METHOD RESTRICT_ROW

	auto cast = [&](s32 voxelY) {
		PROFILE_SCOPE("raycast");

		f32 maxRayLength = length((v2f)atlas.size);
		
#if RESTRICT_METHOD == RESTRICT_ROW
		StaticList<LightTile, 1024> tilesToTest;
		v2f rayBoxRaduis = V2f(maxRayLength);
		auto testRegion = boxMinMax(v2f{0, (f32)voxelY} - rayBoxRaduis, v2f{(f32)atlas.size.x, (f32)voxelY} + rayBoxRaduis);
		for (auto const &tile : allRaycastTargets) {
			if (intersects(testRegion, boxMinMax(tile.boxMin, tile.boxMax))) {
				tilesToTest.push_back(tile);
			}
		}
#elif RESTRICT_METHOD == RESTRICT_NONE
		auto &tilesToTest = allRaycastTargets;
#endif
		
		v3f vox[atlas.maxSampleCount];
		for (s32 voxelX : Range((s32)atlas.size.x)) {
			if (enableCheckerboard && ((voxelY & 1) ^ (voxelX & 1) ^ inverseCheckerboard))
				continue;

#if RESTRICT_METHOD == RESTRICT_CELL
			StaticList<LightTile, 1024> tilesToTest;
			v2f rayBoxRaduis = V2f(maxRayLength);
			v2f voxelP = {(f32)voxelX, (f32)voxelY};
			auto testRegion = boxMinMax(voxelP - rayBoxRaduis, voxelP + rayBoxRaduis);
			for (auto const &tile : allRaycastTargets) {
				if (intersects(testRegion, boxMinMax(tile.boxMin, tile.boxMax))) {
					tilesToTest.push_back(tile);
				}
			}
#endif

			v2f rayBegin = V2f((f32)voxelX, (f32)voxelY) + atlas.center - (v2f)(atlas.size / 2);
			memset(vox, 0, atlas.sampleCount * sizeof(vox[0]));
#if CAST_METHOD == CAST_N_RAYS_OVER_1_TILE
			v2fxm rayBeginX = V2fxm(rayBegin);
			s32 sampleCountX = (s32)(atlas.sampleCount / atlas.simdElementCount);
			for (s32 sampleIndex : Range(sampleCountX)) {
				//constexpr f32 maxRayLength = size / 2 * sqrt2;

				v2fxm dir;
				if constexpr (0 /*lightSampleCount > maxRayLength * 2 * pi*/) {
					s32xm offsets = sampleIndex * (s32)atlas.simdElementCount + sampleOffsetsx;
					gather(dir.x, &atlas.samplingCircle[0].x, offsets * sizeof(f32) * 2);
					gather(dir.y, &atlas.samplingCircle[0].y, offsets * sizeof(f32) * 2);
				} else {
					v2fxm dir0, dir1;
					s32xm offsets0 = sampleIndex * (s32)atlas.simdElementCount + sampleOffsetsx;
					s32xm offsets1 = ((sampleIndex + 1) % sampleCountX) * (s32)atlas.simdElementCount + sampleOffsetsx;
					gather(dir0.x, &atlas.samplingCircle[0].x, offsets0 * sizeof(f32) * 2);
					gather(dir0.y, &atlas.samplingCircle[0].y, offsets0 * sizeof(f32) * 2);
					gather(dir1.x, &atlas.samplingCircle[0].x, offsets1 * sizeof(f32) * 2);
					gather(dir1.y, &atlas.samplingCircle[0].y, offsets1 * sizeof(f32) * 2);
					dir = lerp(dir0, dir1, atlas.random.f32());
					dir = normalize(dir);
				}


				f32xm closestDistanceX = F32xm(pow2(maxRayLength));
				v2fxm rayEnd = rayBeginX + dir * maxRayLength;

				v2fxm tMin, tMax;
				minmax(rayBeginX, rayEnd, tMin, tMax);
				auto checkBox = boxMinMax(tMin, tMax);
				u32 raycasts = 0;
				v2fxm point, normal;
				v3fxm hitColor{};
				for (auto const &tile : tilesToTest) {
					auto boxMin = V2fxm(tile.boxMin);
					auto boxMax = V2fxm(tile.boxMax);
					if (anyTrue(intersects(boxMinMax(boxMin, boxMax), checkBox))) {
						auto raycastMask = raycastAABB(rayBeginX, rayEnd, boxMin, boxMax, point, normal);

						f32xm dist = distanceSqr(rayBeginX, point);
						auto mask = raycastMask && dist < closestDistanceX;
						closestDistanceX = select(mask, dist, closestDistanceX);
						hitColor = select(mask, V3fxm(tile.color), hitColor);
						// rayEnd = rayBeginX + dir * sqrt(closestDistanceX);
						// minmax(rayBeginX, rayEnd, tMin, tMax);
						++raycasts;
					}
				}
				totalVolumeChecks += (u32)tilesToTest.size();
				totalRaysCast += raycasts * atlas.simdElementCount;
				((v3fxm *)vox)[sampleIndex] += hitColor * 10;
			}
			for (v3fxm &v : Span((v3fxm *)vox, (umm)sampleCountX)) {
				v = unpack(v);
			}
			v3f *dest = atlas.getVoxel(voxelY, voxelX);
			for (s32 i = 0; i < (s32)atlas.sampleCount; ++i) {
				dest[i] = lerp(dest[i], vox[i], min(timeDelta * atlas.accumulationRate, 1));
			}
#elif CAST_METHOD == CAST_NO_SIMD
			for (u32 i = 0; i < atlas.sampleCount; ++i) {
				v2f dir = atlas.samplingCircle[i];
				v2f rayEnd = rayBegin + dir * maxRayLength;

				v2f tMin, tMax;
				minmax(rayBegin, rayEnd, tMin, tMax);

				f32 closestDistance = INFINITY;
				v2f point, normal;
				v3f hitColor{};

				u32 raycasts = 0;
				for (auto const &tile : tilesToTest) {
					if (intersects(boxMinMax(tMin, tMax), boxMinMax(tile.boxMin, tile.boxMax))) {
						if (raycastAABB(rayBegin, rayEnd, tile.boxMin, tile.boxMax, point, normal)) {
							f32 dist = distanceSqr(rayBegin, point);
							if (dist < closestDistance) {
								closestDistance = dist;
								hitColor = tile.color;
							}
							++raycasts;
						}
					}
				}
				totalRaysCast += raycasts;
				totalVolumeChecks += (u32)tilesToTest.size();
				vox[i] += hitColor * 10; //(10000.0f - pow2(distanceSqr(rayBegin, point))) * 0.001f * hitColor;
			}
			v3f *dest = atlas.getVoxel(voxelY, voxelX);
			for (s32 i = 0; i < (s32)atlas.sampleCount; ++i) {
				dest[i] = lerp(dest[i], vox[i], min(timeDelta * atlas.accumulationRate, 1));
			}
#else
#error invalid raycast method
#endif
		}
	};
	if (timeDelta) {
		if (threaded) {
			WorkQueue queue{};
			for (s32 ySlice : Range((s32)atlas.size.y)) {
				queue.push(cast, ySlice);
			}
			queue.completeAllWork();
		} else {
			for (s32 ySlice : Range((s32)atlas.size.y)) {
				cast(ySlice);
			}
		}
	}
	atlas.totalRaysCast = totalRaysCast;
	atlas.totalVolumeChecks = totalVolumeChecks;
}
