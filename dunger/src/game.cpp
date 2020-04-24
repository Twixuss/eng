#include "../../src/game.h"
#include <array>
#include <queue>
#include <random>

#if 0
template <class T>
void erase(List<T> &vec, T *val) {
	vec.erase(vec.begin() + (val - vec.data()));
}
template <class T>
void erase(List<T> &vec, typename List<T>::iterator val) {
	vec.erase(val);
}
template <class T>
void erase(List<T> &vec, T &val) {
	erase(vec, &val);
}
#endif
template <class T>
u32 indexOf(T const *val, UnorderedList<T> const &list) {
	ASSERT(list.begin() <= val && val < list.end(), "value is not in range");
	return (u32)(val - list.begin());
}
template <class T>
u32 indexOf(T const *val, View<T> list) {
	ASSERT(list.begin() <= val && val < list.end(), "value is not in range");
	return (u32)(val - list.begin());
}

#define CHUNK_WIDTH 32
using TileStorage = std::array<bool, CHUNK_WIDTH * CHUNK_WIDTH>;
void setTile(TileStorage &tiles, u32 x, u32 y, bool value) {
#if BUILD_DEBUG
	ASSERT(x < CHUNK_WIDTH);
	ASSERT(y < CHUNK_WIDTH);
#endif
	tiles[x * CHUNK_WIDTH + y] = value;
}
bool getTile(TileStorage const &tiles, u32 x, u32 y) {
#if BUILD_DEBUG
	ASSERT(x < CHUNK_WIDTH);
	ASSERT(y < CHUNK_WIDTH);
#endif
	return tiles[x * CHUNK_WIDTH + y];
}
TileStorage genTiles() {
	TileStorage tiles;
	for (u32 x = 0; x < CHUNK_WIDTH; ++x) {
		setTile(tiles, x, 0, true);
		setTile(tiles, x, CHUNK_WIDTH - 1, true);
	}
	for (u32 y = 0; y < CHUNK_WIDTH; ++y) {
		setTile(tiles, 0, y, true);
		setTile(tiles, CHUNK_WIDTH - 1, y, true);
	}
	for (u32 x = 1; x < CHUNK_WIDTH - 1; ++x) {
		for (u32 y = 1; y < CHUNK_WIDTH - 1; ++y) {
			setTile(tiles, x, y, voronoi((v2s)V2u(x, y), CHUNK_WIDTH / 8) < 0.15f);
		}
	}
	for (u32 x = 1; x < 3; ++x) {
		for (u32 y = 1; y < 3; ++y) {
			setTile(tiles, x, y, false);
			setTile(tiles, CHUNK_WIDTH - 1 - x, y, false);
			setTile(tiles, CHUNK_WIDTH - 1 - x, CHUNK_WIDTH - 1 - y, false);
			setTile(tiles, x, CHUNK_WIDTH - 1 - y, false);
		}
	}
	s32 midRadius = 5;
	for (s32 x = -midRadius; x < midRadius; ++x) {
		for (s32 y = -midRadius; y < midRadius; ++y) {
			if (abs(x) + abs(y) <= midRadius)
				setTile(tiles, (u32)(x + CHUNK_WIDTH / 2), (u32)(y + CHUNK_WIDTH / 2), false);
		}
	}
	return tiles;
}

v2 makeValidPosition(v2 p) {
	if (isnan(p.x))
		p.x = 0;
	if (isnan(p.y))
		p.y = 0;
	return clamp(p, v2{}, V2f(CHUNK_WIDTH - 1));
}
v2 movePlayer(TileStorage const &tiles, Time const &time, v2 from, v2 velocity, f32 r) {
	for (s32 i = 0; i < 4; ++i) {
		if (lengthSqr(velocity) == 0) {
			break;
		}
		v2 a = from;
		v2 b = a + velocity * time.delta;
		bool hit = false;
		v2s tMin = max(roundToInt(min(a, b) - (r + 0.5f)), V2s(0));
		v2s tMax = min(roundToInt(max(a, b) + (r + 0.5f)) + 1, V2s(CHUNK_WIDTH));
		for (s32 tx = tMin.x; tx < tMax.x; ++tx) {
			for (s32 ty = tMin.y; ty < tMax.y; ++ty) {
				if (!getTile(tiles, (u32)tx, (u32)ty))
					continue;
				v2 tilef = (v2f)v2s{tx, ty};

				v2 tileMin = tilef - 0.5f;
				v2 tileMax = tilef + 0.5f;

				v2 normal{};
				if (tileMin.x < b.x && b.x < tileMax.x) {
					if (tilef.y < b.y && b.y < tilef.y + 0.5f + r) {
						from.y = tilef.y + 0.5f + r;
						normal = {0, 1};
					} else if (tilef.y - 0.5f - r < b.y && b.y < tilef.y) {
						from.y = tilef.y - 0.5f - r;
						normal = {0, -1};
					}
				} else if (tileMin.y < b.y && b.y < tileMax.y) {
					if (tilef.x < b.x && b.x < tilef.x + 0.5f + r) {
						from.x = tilef.x + 0.5f + r;
						normal = {1, 0};
					} else if (tilef.x - 0.5f - r < b.x && b.x < tilef.x) {
						from.x = tilef.x - 0.5f - r;
						normal = {-1, 0};
					}
				} else {
					auto doCorner = [&](v2 corner) {
						if (lengthSqr(b - corner) < r * r) {
							normal = normalize(b - corner);
							from = corner + normal * r;
						}
					};
					if (b.x > tileMax.x && b.y > tileMax.y)
						doCorner(tileMax);
					else if (b.x < tileMin.x && b.y < tileMin.y)
						doCorner(tileMin);
					else if (b.x < tileMin.x && b.y > tileMax.y)
						doCorner(v2{tileMin.x, tileMax.y});
					else if (b.x > tileMax.x && b.y < tileMin.y)
						doCorner(v2{tileMax.x, tileMin.y});
				}
				hit = normal != v2{};
				velocity -= normal * dot(velocity, normal);
				if (hit)
					break;
			}
			if (hit)
				break;
		}
	}
	return makeValidPosition(from + velocity);
}

#define MAX_TILES  (1024 * 256)
#define MAX_LIGHTS (1024 * 256)

#define ATLAS_SIZE		 V2f(8)
#define ATLAS_ENTRY_SIZE (1.0f / ATLAS_SIZE)

v2 offsetAtlasTile(v2f v) { return v * ATLAS_ENTRY_SIZE; }
v2 offsetAtlasTile(f32 x, f32 y) { return offsetAtlasTile({x, y}); }

struct Bullet {
	v2 position;
	v2 velocity;
	f32 remainingLifetime = 5.0f;
	f32 rotation;
	u32 ignoredLayers;
};
struct Explosion {
	v2 position;
	f32 remainingLifeTime;
	f32 maxLifeTime;
	f32 rotationOffset;
};
struct Ember {
	v2 position;
	v2 velocity;
	f32 remainingLifeTime;
	f32 maxLifeTime;
	f32 rotationOffset;
};
struct Tile {
	v4 color;

	v2 position;
	v2 size;

	v2 uv0;
	v2 uv1;

	v2 uvScale;
	f32 uvMix;
	f32 rotation;
};
struct FrameUvs {
	v2f uv0;
	v2f uv1;
	f32 uvMix;
};
struct Light {
	v3 color;
	f32 radius;
	v2 position;
};
struct Bot {
	v2 position;
	v2 velocity;
	f32 radius;
	f32 fireTimer;
};
struct Random {
	s32 seed;
	s32 s32() { return seed = randomize(seed); }
	u32 u32() { return (::u32)s32(); }
	f32 f32() { return (u32() >> 8) * (1.0f / 0x1000000); }
	bool u1() { return u32() & 0x10; }
	s32x4 s32x4() { return {s32(), s32(), s32(), s32()}; }
	s32x8 s32x8() { return {s32(), s32(), s32(), s32(), s32(), s32(), s32(), s32()}; }
	u32x4 u32x4() { return (::u32x4)s32x4(); }
	u32x8 u32x8() { return (::u32x8)s32x8(); }
	f32x4 f32x4() { return (::f32x4)(u32x4() >> 8) * (1.0f / 0x1000000); }
	f32x8 f32x8() { return (::f32x8)(u32x8() >> 8) * (1.0f / 0x1000000); }
	v4 v4() { return V4f(f32x4()); }
	v3 v3() { return V3f(f32(), f32(), f32()); }
};
struct Label {
	v2 p;
	std::string text;
	v4f color = V4f(1);
};
struct Rect {
	v2s pos;
	v2s size;
	v4 color;
};
enum class RaycastResultType { noHit, tile, bot, player };
struct RaycastResult {
	RaycastResultType type;
	u32 target;
};
enum RaycastLayer { player = 0x1, bot = 0x2 };
RaycastResult raycastBullet(TileStorage const *tiles, View<Bot> bots, View<v2f> players, v2 a, v2 b, v2 &point,
							v2 &normal) {
	if (tiles) {
		v2 tMin, tMax;
		minmax(a, b, tMin, tMax);
		v2s tMini = max(roundToInt(tMin), V2s(0));
		v2s tMaxi = min(roundToInt(tMax) + 1, V2s(CHUNK_WIDTH));
		for (s32 tx = tMini.x; tx < tMaxi.x; ++tx) {
			for (s32 ty = tMini.y; ty < tMaxi.y; ++ty) {
				if (!getTile(*tiles, (u32)tx, (u32)ty))
					continue;
				if (raycastRect(a, b, (v2f)v2s{tx, ty}, V2f(0.5f), point, normal)) {
					return {RaycastResultType::tile, (u32)(tx | (ty << 16))};
				}
			}
		}
	}
	if (bots.size()) {
		for (auto &bot : bots) {
			if (raycastRect(a, moveAway(b, a, max(dot(a - b, bot.velocity), 0)), bot.position, V2f(bot.radius), point,
							normal)) {
				return {RaycastResultType::bot, indexOf(&bot, bots)};
			}
		}
	}
	if (players.size()) {
		for (auto &player : players) {
			// TODO: adjust ray length by player's velocity so bullets dont go through
			if (raycastRect(a, b, player, V2f(0.4f), point, normal)) {
				return {RaycastResultType::player, indexOf(&player, players)};
			}
		}
	}
	return {};
}
FrameUvs getFrameUvs(f32 time, u32 totalFrames, u32 frameAtlasWidth, v2f frameOffset, f32 frameSize,
					 bool looping = true) {
	FrameUvs result;
	f32 mtime = time * (totalFrames - !looping);
	u32 utime = (u32)mtime;
	u32 frame0 = utime % totalFrames;
	u32 frame1 = (utime + 1) % totalFrames;
	result.uv0 = offsetAtlasTile(frameOffset.x + (f32)(frame0 % frameAtlasWidth) * frameSize,
								 frameOffset.y + (f32)(frame0 / frameAtlasWidth) * frameSize);
	result.uv1 = offsetAtlasTile(frameOffset.x + (f32)(frame1 % frameAtlasWidth) * frameSize,
								 frameOffset.y + (f32)(frame1 / frameAtlasWidth) * frameSize);
	result.uvMix = frac(mtime);
	return result;
}

#if BUILD_DEBUG
#define VOXEL_MAP_WIDTH 12
#else
#define VOXEL_MAP_WIDTH 32
#endif

v2 screenToWorld(v2 p, v2 clientSize, v2 cameraP, f32 cameraZoom) {
	return cameraP + (p * 2 - clientSize) / (clientSize.y * cameraZoom);
}

struct LightTile {
	v2 pos;
	f32 size;
	v3 color;
};
struct LightAtlas {
	static constexpr u32 simdElementCount = TL::simdElementCount<sizeof(f32)>;
	static constexpr u32 lightSampleCount = BUILD_DEBUG ? 16 : 64;
	static constexpr u32 lightSampleCountX = lightSampleCount / simdElementCount;
	static constexpr u32 lightSampleRandomCount = 64;

	v2 center{};
	v2 oldCenter{};
	v3 voxels[VOXEL_MAP_WIDTH][VOXEL_MAP_WIDTH]{};
	v2 samplingCircle[lightSampleCount * lightSampleRandomCount];
	::Random random;
};
void initLightAtlas(LightAtlas &atlas) {
	u32 samplingCircleSize = _countof(atlas.samplingCircle);
	for (u32 i = 0; i < samplingCircleSize; ++i) {
		f32 angle = (f32)i / samplingCircleSize * (pi * 2);
		sincos(angle, atlas.samplingCircle[i].x, atlas.samplingCircle[i].y);
	}
}
void moveLightAtlas(LightAtlas &atlas, v2 newCenter) {
	atlas.center = newCenter;
	if (atlas.center != atlas.oldCenter) {
		v2s const d = clamp((v2s)(atlas.center - atlas.oldCenter), V2s(-VOXEL_MAP_WIDTH), V2s(VOXEL_MAP_WIDTH));
		v2s const ad = abs(d);

		if (d.x > 0) {
			for (s32 voxelY = 0; voxelY < VOXEL_MAP_WIDTH; ++voxelY) {
				for (s32 voxelX = 0; voxelX < VOXEL_MAP_WIDTH - ad.x; ++voxelX) {
					atlas.voxels[voxelY][voxelX] = atlas.voxels[voxelY][voxelX + ad.x];
				}
				for (s32 voxelX = VOXEL_MAP_WIDTH - ad.x; voxelX < VOXEL_MAP_WIDTH; ++voxelX) {
					atlas.voxels[voxelY][voxelX] = {};
				}
			}
		} else if (d.x < 0) {
			for (s32 voxelY = 0; voxelY < VOXEL_MAP_WIDTH; ++voxelY) {
				for (s32 voxelX = VOXEL_MAP_WIDTH - 1; voxelX >= ad.x; --voxelX) {
					atlas.voxels[voxelY][voxelX] = atlas.voxels[voxelY][voxelX - ad.x];
				}
				for (s32 voxelX = 0; voxelX < ad.x; ++voxelX) {
					atlas.voxels[voxelY][voxelX] = {};
				}
			}
		}
		if (d.y > 0) {
			for (s32 voxelY = 0; voxelY < VOXEL_MAP_WIDTH - ad.y; ++voxelY) {
				memcpy(atlas.voxels[voxelY], atlas.voxels[voxelY + ad.y], VOXEL_MAP_WIDTH * sizeof(atlas.voxels[0][0]));
			}
			for (s32 voxelY = VOXEL_MAP_WIDTH - ad.y; voxelY < VOXEL_MAP_WIDTH; ++voxelY) {
				memset(atlas.voxels[voxelY], 0, VOXEL_MAP_WIDTH * sizeof(atlas.voxels[0][0]));
			}
		} else if (d.y < 0) {
			for (s32 voxelY = VOXEL_MAP_WIDTH - 1; voxelY >= ad.y; --voxelY) {
				memcpy(atlas.voxels[voxelY], atlas.voxels[voxelY - ad.y], VOXEL_MAP_WIDTH * sizeof(atlas.voxels[0][0]));
			}
			for (s32 voxelY = 0; voxelY < ad.y; ++voxelY) {
				memset(atlas.voxels[voxelY], 0, VOXEL_MAP_WIDTH * sizeof(atlas.voxels[0][0]));
			}
		}
	}
	atlas.oldCenter = atlas.center;
}
u32 updateLightAtlas(LightAtlas &atlas, Renderer &renderer, bool inverseCheckerboard, f32 timeDelta,
					 View<LightTile> allRaycastTargets) {
	PROFILE_FUNCTION;
	// static auto samplingCircleX = [&] {
	//	std::array<v2x, lightSampleCountX * lightSampleRandomCount> r;
	//	for (u32 i = 0; i < r.size(); ++i) {
	//		f32x<> angle;
	//		for (u32 j = 0; j < simdElementCount; ++j) {
	//			angle[j] = i / simdElementCount / (f32)r.size() + j / (f32)simdElementCount +
	//					   (atlas.random.f32() * 2 - 1) * 0.01f;
	//		}
	//		sincos(angle * pi * 2, r[i].x, r[i].y);
	//	}
	//	return r;
	//}();

	std::atomic_uint32_t totalRaysCast = 0;

	auto cast = [&](u32 voxelY) {
		for (u32 voxelX = 0; voxelX < VOXEL_MAP_WIDTH; ++voxelX) {
			bool skip = (voxelY & 1) ^ (voxelX & 1);
			if (inverseCheckerboard)
				skip = !skip;
			if (skip)
				continue;
			atlas.random.seed >>= 1;
			v2 rayBegin = V2f((f32)voxelX, (f32)voxelY) + atlas.center - VOXEL_MAP_WIDTH * 0.5f;
			v3 vox{};
#if 1
			v2x rayBeginX = V2fx(rayBegin);
			v3x voxX{};
			for (u32 sampleIndex = 0; sampleIndex < atlas.lightSampleCountX; ++sampleIndex) {
				v2x dir;
				// dir = samplingCircleX[sampleIndex * lightSampleRandomCount + random.u32() %
				// lightSampleRandomCount];
				for (u32 i = 0; i < atlas.simdElementCount; ++i)
					((v2 *)&dir)[i] =
						atlas.samplingCircle[(sampleIndex * atlas.simdElementCount + i) * atlas.lightSampleRandomCount +
											 atlas.random.u32() % atlas.lightSampleRandomCount];
				dir = pack(dir);

				v2x rayEnd = rayBeginX + dir * (VOXEL_MAP_WIDTH / 2);

				v2x tMin, tMax;
				minmax(rayBeginX, rayEnd, tMin, tMax);

				LightTile tilesToTest[1024];
				u32 tileCount = 0;
				for (auto const &tile : allRaycastTargets) {
					if (anyTrue(inBounds(V2fx(tile.pos), tMin - tile.size, tMax + tile.size))) {
						tilesToTest[tileCount++] = tile;
					}
				}

				f32x closestDistanceX = F32x(INFINITY);
				v2x point, normal;
				v3x hitColor{};
				for (u32 i = 0; i < tileCount; ++i) {
					LightTile *tile = tilesToTest + i;
					auto raycastMask = raycastRect(rayBeginX, rayEnd, V2fx(tile->pos), V2fx(tile->size), point, normal);

					f32x dist = distanceSqr(rayBeginX, point);
					auto mask = raycastMask && dist < closestDistanceX;
					closestDistanceX = select(mask, dist, closestDistanceX);
					hitColor = select(mask, V3fx(tile->color), hitColor);
				}
				totalRaysCast += tileCount * atlas.simdElementCount;
				voxX += hitColor * 10;
			}
			voxX = unpack(voxX);
			for (u32 i = 0; i < atlas.simdElementCount; ++i) {
				vox += ((v3 *)&voxX)[i];
			}
#else
			for (u32 i = 0; i < lightSampleCount; ++i) {
				v2 dir = samplingCircle[i * lightSampleRandomCount + atlas.random.u32() % lightSampleRandomCount];
				v2 rayEnd = rayBegin + dir * (VOXEL_MAP_WIDTH / 2);

				v2 tMin, tMax;
				minmax(rayBegin, rayEnd, tMin, tMax);

				f32 closestDistance = INFINITY;
				v2 point, normal;
				v3 hitColor{};

				LightTile tilesToTest[64];
				LightTile *tileToTest = tilesToTest;
				for (auto const &tile : allRaycastTargets) {
					if (tMin.x - tile.size < tile.pos.x && tile.pos.x < tMax.x + tile.size &&
						tMin.y - tile.size < tile.pos.y && tile.pos.y < tMax.y + tile.size) {
						*tileToTest++ = tile;
					}
				}
				for (LightTile *tile = tilesToTest; tile < tileToTest; ++tile) {
					if (raycastRect(rayBegin, rayEnd, tile->pos, V2f(tile->size), point, normal)) {
						f32 dist = distanceSqr(rayBegin, point);
						if (dist < closestDistance) {
							closestDistance = dist;
							hitColor = tile->color;
						}
					}
				}
				totalRaysCast += (u32)(tileToTest - tilesToTest);
				vox += hitColor * 10; //(10000.0f - pow2(distanceSqr(rayBegin, point))) * 0.001f * hitColor;
			}
#endif
			atlas.voxels[voxelY][voxelX] = lerp(atlas.voxels[voxelY][voxelX], vox / atlas.lightSampleCount,
												min(timeDelta * 6.0f, 1));
		}
	};
	if (timeDelta) {
		for (u32 i = 0; i < VOXEL_MAP_WIDTH; ++i) {
			pushWork("raycast", cast, i);
		}
		completeAllWork();
	}
	return totalRaysCast;
}

void addSoundSample(u32 targetSampleRate, s16 *subsample, SoundBuffer const &sound, f64 &playCursor, f32 volume[2],
					f32 pitch = 1.0f, bool looping = false) {
	u32 sampleOffset = (u32)map(playCursor, 0.0f, targetSampleRate, 0.0f, sound.sampleRate);

	if (looping)
		sampleOffset %= sound.sampleCount;
	else
		sampleOffset = min(sampleOffset, sound.sampleCount - 1);

	s16 *musicSubsample = (s16 *)sound.data + sampleOffset * sound.numChannels;
	if (sound.numChannels == 2) {
		for (u32 channelIndex = 0; channelIndex < 2; ++channelIndex) {
			*subsample++ += (s16)((*musicSubsample++ >> 6) * volume[channelIndex]);
		}
	} else if (sound.numChannels == 1) {
		s16 result = *musicSubsample++ >> 6;
		for (u32 channelIndex = 0; channelIndex < 2; ++channelIndex) {
			*subsample++ += (s16)(result * volume[channelIndex]);
		}
	} else {
		INVALID_CODE_PATH("audio channel count mismatch");
	}

	playCursor += pitch;
	if (looping && playCursor > sound.sampleCount) {
		playCursor -= sound.sampleCount;
	}
}

struct GameState {
	v2 playerP = V2f((CHUNK_WIDTH - 1) * 0.5f);
	v2 playerV{};

	f32 const playerR = 0.4f;

	v2 cameraP = playerP;
	f32 targetCamZoom = 1.0f / 8;
	f32 camZoom = targetCamZoom;
	f32 fireTimer = 0.0f;
	f32 fireDelta = 0.5f;

	v2 pixelsInMeter;

	TileStorage tiles = genTiles();

	UnorderedList<Bullet> bullets;
	UnorderedList<Explosion> explosions;
	UnorderedList<Ember> embers;

	::Random random;

	RenderTargetId msRenderTarget, diffuseRt, lightMapRt;
	ShaderId tileShader, solidShader, msaaShader, lightShader, mergeShader;
	TextureId atlas, fontTexture, voxelsTex;
	BufferId tilesBuffer, lightsBuffer;

	List<Tile> tilesToDraw;
	List<Light> lightsToDraw;

	LightAtlas lightAtlas;

	f32 botSpawnTimer = 0.0f;
	f32 botSpawnDelta = 1.23456789f;
	UnorderedList<Bot> bots;
	u32 botsKilled = 0;

	s32 shotsAtOnce = 1;
	u32 ultimateAttackPercent = 0;
	float auraSize = 0.0f;
	u32 health = 100;
	bool hasUltimateAttack() { return ultimateAttackPercent == 100; }

	SoundBuffer music, shotSound, explosionSound, ultimateSound;

	struct PlayingSound {
		SoundBuffer &sound;
		v2f position;
		f64 cursor;
		f32 volume[2];
	};
	UnorderedList<PlayingSound> playingSounds;

	enum class State { game, menu };

	State currentState{};
	f32 timeScale = 1.0f;

	struct DebugProfile {
		u64 raycastCy;
		u64 totalRaysCast;
		u8 mode;
	};
	DebugProfile debugProfile{};

	RaycastResult raycastBullet(u32 ignoredLayers, v2 a, v2 b, v2 &point, v2 &normal) {
		View players = {&playerP, 1};
		return ::raycastBullet(&tiles, (ignoredLayers & RaycastLayer::bot) ? View<Bot>{} : bots,
							   (ignoredLayers & RaycastLayer::player) ? View<v2f>{} : players, a, b, point, normal);
	}
	void spawnExplosion(v2 position, v2 incoming, v2 normal) {
		Explosion ex;
		ex.position = position;
		ex.maxLifeTime = ex.remainingLifeTime = 0.75f + random.f32() * 0.5f;
		ex.rotationOffset = random.f32() * (2 * pi);
		explosions.push_back(ex);

		v2 nin = normalize(incoming);
		f32 range = -dot(nin, normal);
		v2 rin = reflect(nin, normal);
		f32 rinAngle = atan2f(rin.x, rin.y);

		Ember e;
		e.position = position;
		for (u32 i = 0; i < 16; ++i) {
			f32x4 rnd = random.f32x4();
			e.maxLifeTime = e.remainingLifeTime = 0.5f + rnd[0] * 0.5f;
			f32 angle = rinAngle + (rnd[1] * 2 - 1) * (pi * 0.5f) * range;
			sincos(angle, e.velocity.x, e.velocity.y);
			e.velocity *= rnd[2] * 10.0f;
			e.rotationOffset = rnd[3] * (2 * pi);
			embers.push_back(e);
		}
		playingSounds.push_back({explosionSound, ex.position});
	}
	void spawnBullet(u32 ignoredLayers, v2 position, f32 rotation, f32 velocity) {
		Bullet b;
		b.ignoredLayers = ignoredLayers;
		b.position = position;
		b.rotation = rotation;
		sincos(rotation, b.velocity.y, b.velocity.x);
		b.velocity *= velocity;
		bullets.push_back(b);
	}
	void spawnBullet(u32 ignoredLayers, v2 position, v2 velocity) {
		Bullet b;
		b.ignoredLayers = ignoredLayers;
		b.position = position;
		b.velocity = velocity;
		b.rotation = atan2(normalize(velocity));
		bullets.push_back(b);
	}

	Tile createTile(v2 position, v2 uv0, v2 uv1, f32 uvMix, v2 uvScale, v2 size = V2f(1.0f), f32 rotation = 0.0f,
					v4 color = V4f(1)) {
		Tile t{};
		t.position = position;
		t.uv0 = uv0;
		t.uv1 = uv1;
		t.uvMix = uvMix;
		t.uvScale = uvScale;
		t.rotation = rotation;
		t.size = size;
		t.color = color;
		return t;
	}
	Tile createTile(v2 position, v2 uv0, v2 uvScale, v2 size = V2f(1.0f), f32 rotation = 0.0f, v4 color = V4f(1)) {
		return createTile(position, uv0, {}, 0, uvScale, size, rotation, color);
	}
	Light createLight(v3 color, v2 position, f32 radius, v2 clientSize) {
		Light l;
		l.color = color;
		l.radius = camZoom * radius * 2;
		l.position = (position - cameraP) * camZoom;
		l.position.x *= clientSize.y / clientSize.x;
		return l;
	}

	void drawText(Renderer &renderer, View<Label> labels, v2 clientSize) {
		v2 textScale{11, 22};

		tilesToDraw.clear();
		for (auto &l : labels) {
			v2 p = l.p / textScale;
			p.y *= -1;
			f32 row = 0.0f;
			f32 column = 0.0f;
			for (s8 c : l.text) {
				if (c == '\n') {
					row -= 1.0f;
					column = 0.0f;
					continue;
				}
				f32 u = ((u8)c % 16) * 0.0625f;
				f32 v = ((u8)c / 16) * 0.0625f;
				tilesToDraw.push_back(
					createTile(p + v2{column + 0.5f, row - 0.5f}, {u, v}, V2f(1.0f / 16.0f), V2f(1.0f), 0, l.color));
				column += 1.0f;
			}
		}
		renderer.updateBuffer(tilesBuffer, tilesToDraw.data(), (u32)tilesToDraw.size() * sizeof(tilesToDraw[0]));

		renderer.setMatrix(0, m4::translation(-1, 1, 0) * m4::scaling(2 * textScale / clientSize, 0));

		renderer.setBlend(Blend::srcAlpha, Blend::invSrcAlpha, BlendOp::add);
		renderer.bindShader(tileShader);
		renderer.bindBuffer(tilesBuffer, Stage::vs, 0);
		renderer.bindTexture(fontTexture, Stage::ps, 0);
		renderer.draw((u32)tilesToDraw.size() * 6, 0);
	}
	void drawRect(Renderer &renderer, View<Rect> rects, v2 clientSize) {
		tilesToDraw.clear();
		for (auto r : rects) {
			r.pos.y = (s32)clientSize.y - r.pos.y;
			// printf("%i\n", r.pos.y);
			tilesToDraw.push_back(
				createTile((v2)r.pos + (v2)r.size * V2f(0.5f, -0.5f), {}, {}, (v2)r.size, 0, r.color));
		}
		renderer.updateBuffer(tilesBuffer, tilesToDraw.data(), (u32)tilesToDraw.size() * sizeof(tilesToDraw[0]));

		renderer.setMatrix(0, m4::translation(-1, -1, 0) * m4::scaling(2.0f / clientSize, 0));

		/*
		m4 m = m4::scaling(1.0f / clientSize, 0);
		for (auto &r : rects) {
			v4 p = V4f((m * V4f((v2)r.pos, 0, 1)).xy, (m * V4f((v2)r.size, 0, 1)).xy);
			printf("%.3f %.3f %.3f %.3f\n", p.x, p.y, p.z, p.w);
		}
		*/

		renderer.setBlend(Blend::srcAlpha, Blend::invSrcAlpha, BlendOp::add);
		renderer.bindShader(solidShader);
		renderer.bindBuffer(tilesBuffer, Stage::vs, 0);
		renderer.draw((u32)tilesToDraw.size() * 6, 0);
	}

	void init(Window &window, Renderer &renderer) {
		PROFILE_FUNCTION;
		tileShader = renderer.createShader(DATA "shaders/tile");
		solidShader = renderer.createShader(DATA "shaders/solid");
		lightShader = renderer.createShader(DATA "shaders/light");
		mergeShader = renderer.createShader(DATA "shaders/merge");

		tilesBuffer = renderer.createBuffer(0, sizeof(Tile), MAX_TILES);
		lightsBuffer = renderer.createBuffer(0, sizeof(Light), MAX_LIGHTS);

		atlas = renderer.createTexture(DATA "textures/atlas.png", Address::clamp, Filter::point_point);
		fontTexture = renderer.createTexture(DATA "textures/font.png", Address::clamp, Filter::point_point);
		voxelsTex = renderer.createTexture(Format::F_RGB32, VOXEL_MAP_WIDTH, VOXEL_MAP_WIDTH, Address::border,
										   Filter::linear_linear, v4{});
		initLightAtlas(lightAtlas);

		music = loadWaveFile(DATA "audio/music.wav");
		shotSound = loadWaveFile(DATA "audio/shot.wav");
		explosionSound = loadWaveFile(DATA "audio/explosion.wav");
		ultimateSound = loadWaveFile(DATA "audio/ultimate.wav");
	}
	void resize(Window &window, Renderer &renderer) {
		PROFILE_FUNCTION;

		u32 sampleCount = 4;

		if (msRenderTarget.valid())
			renderer.release(msRenderTarget);
		msRenderTarget = renderer.createRenderTarget(window.clientSize, sampleCount);

		if (diffuseRt.valid())
			renderer.release(diffuseRt);
		diffuseRt = renderer.createRenderTarget(window.clientSize);

		if (lightMapRt.valid())
			renderer.release(lightMapRt);
		lightMapRt = renderer.createRenderTarget(window.clientSize, Format::F_RGBA16);

		if (msaaShader.valid())
			renderer.release(msaaShader);
		char msaaPrefix[1024];
		sprintf(msaaPrefix, R"(
#define MSAA_SAMPLE_COUNT %u
)",
				sampleCount);
		msaaShader = renderer.createShader(DATA "shaders/msaa", StringView(msaaPrefix, strlen(msaaPrefix)));

		pixelsInMeter = (v2)window.clientSize / (f32)window.clientSize.y * camZoom;
	}
	void update(Window &window, Renderer &renderer, Input const &input, Time &time) {
		// time.targetFrameTime = 0.0f;
		PROFILE_FUNCTION;

#if 0
		u32 seed = 0;
		u32 test = seed;
		u32 count = 0;
		do {
			test = randomize(test);
			++count;
			printf("%x\n", test);
			if(test == seed){
				if(count) {
					printf("Unique values: %u\n", count);
				}
				ASSERT(!count);
			}
		} while(count);
#endif

		if (window.resized) {
			resize(window, renderer);
		}

		switch (currentState) {
			case State::menu:
				if (input.keyDown(Key_escape)) {
					currentState = State::game;
					break;
				}
				break;
			case State::game:
				if (input.keyDown(Key_escape)) {
					currentState = State::menu;
					break;
				}
				break;
			default: break;
		}

		timeScale = moveTowards(timeScale, (f32)(currentState == State::game), time.delta);

		f32 scaledDelta = time.delta * timeScale;

		v2 move{};
		if (currentState == State::game) {
			move.x += input.keyHeld('D') - input.keyHeld('A');
			move.y += input.keyHeld('W') - input.keyHeld('S');
		}

		v2 targetPlayerV = normalize(move, {});

		playerV = moveTowards(playerV, targetPlayerV, scaledDelta * 10);

		playerP = movePlayer(tiles, time, playerP, playerV * scaledDelta * 5, playerR);

		cameraP = lerp(cameraP, playerP, min(scaledDelta * 10, 1));

		auraSize = lerp(auraSize, hasUltimateAttack(), scaledDelta * 5.0f);

		for (auto &bot : bots) {
			v2f diff = playerP - bot.position;
			v2f velocity = normalize(diff, {1});
			if (lengthSqr(diff) < 4.0f)
				velocity *= -1;

			bot.velocity = lerp(bot.velocity, velocity, scaledDelta * 10.0f);
			bot.position = movePlayer(tiles, time, bot.position, bot.velocity * scaledDelta * 3, playerR);

			if (bot.fireTimer >= 1.0f) {
				v2f point, normal;
				if (::raycastBullet(&tiles, {}, {&playerP, 1}, bot.position, playerP, point, normal).type ==
					RaycastResultType::player) {
					bot.fireTimer -= 1.0f;
					spawnBullet(RaycastLayer::bot, bot.position, normalize(playerP - bot.position, {1}));
					playingSounds.push_back({shotSound, bot.position});
				}
			} else {
				bot.fireTimer += scaledDelta;
			}
			// if(distanceSqr(bot.position, playerP) < playerR * playerR * 2) {}
		}
#if 1
		botSpawnTimer += scaledDelta;
		if (botSpawnTimer >= botSpawnDelta) {
			botSpawnTimer -= botSpawnDelta;
			Bot newBot{};
			do {
				newBot.position = v2f{random.f32(), random.f32()} * CHUNK_WIDTH;
			} while (getTile(tiles, (u32)newBot.position.x, (u32)newBot.position.y));
			newBot.radius = playerR;
			bots.push_back(newBot);
		}
#endif
		PROFILE_BEGIN("bullets");
		for (u32 i = 0; i < bullets.size(); ++i) {
			auto &b = bullets[i];
			b.remainingLifetime -= scaledDelta;
			if (b.remainingLifetime <= 0) {
				erase(bullets, b);
				--i;
				continue;
			}
			v2 nextPos = b.position + b.velocity * scaledDelta * 10;
			v2 hitPoint, hitNormal;
			auto raycastResult = raycastBullet(b.ignoredLayers, b.position, nextPos, hitPoint, hitNormal);
			if (raycastResult.type == RaycastResultType::bot) {
				bots.erase(bots[raycastResult.target]);
				++botsKilled;
				if (shotsAtOnce < 5 && botsKilled % 25 == 0) {
					++shotsAtOnce;
				}
				if (!hasUltimateAttack()) {
					ultimateAttackPercent += 2;
				}
			} else if (raycastResult.type == RaycastResultType::player) {
				--health;
			}
			if (raycastResult.type != RaycastResultType::noHit) {
				spawnExplosion(hitPoint, normalize(b.velocity), hitNormal);
				erase(bullets, b);
				--i;
				continue;
			}
			b.position = nextPos;
		}
		PROFILE_END;
		PROFILE_BEGIN("explosions");
		for (u32 i = 0; i < explosions.size(); ++i) {
			auto &e = explosions[i];
			e.remainingLifeTime -= scaledDelta;
			if (e.remainingLifeTime <= 0) {
				erase(explosions, e);
				--i;
				continue;
			}
		}
		PROFILE_END;
		PROFILE_BEGIN("embers");
		for (auto e = embers.begin(); e != embers.end();) {
			e->remainingLifeTime -= scaledDelta;
			if (e->remainingLifeTime <= 0) {
				erase(embers, e);
				continue;
			}
			e->position += e->velocity * scaledDelta;
			e->velocity = moveTowards(e->velocity, {}, scaledDelta * 10);
			++e;
		}
		PROFILE_END;

		v2s mousePos = input.mousePosition;
		mousePos.y = (s32)window.clientSize.y - mousePos.y;
		v2f mouseWorldPos = screenToWorld((v2)mousePos, (v2)window.clientSize, cameraP, camZoom);

		if (currentState == State::game) {
			if (fireTimer > 0) {
				fireTimer -= scaledDelta;
			} else {
				if (input.mouseHeld(0)) {
					fireTimer += fireDelta;
					auto createBullet = [&](f32 offset) {
						spawnBullet(RaycastLayer::player, playerP, atan2(normalize(mouseWorldPos - playerP)) + offset,
									1);
					};
					if (shotsAtOnce == 1) {
						createBullet(0);
					} else {
						if (shotsAtOnce & 1) {
							for (s32 i = -shotsAtOnce / 2; i <= shotsAtOnce / 2; ++i) {
								createBullet(f32(i) * 0.1f);
							}
						} else {
							for (s32 i = 0; i < shotsAtOnce; ++i) {
								createBullet(f32(i - shotsAtOnce * 0.5f + 0.5f) * 0.1f);
							}
						}
					}
					playingSounds.push_back({shotSound, playerP});
				}
			}
			if (input.mouseDown(1) && hasUltimateAttack()) {
				ultimateAttackPercent = 0;
				for (u32 i = 0; i < 25; ++i) {
					spawnBullet(RaycastLayer::player, playerP, i * 0.08f * pi, 1);
				}
				playingSounds.push_back({ultimateSound, playerP});
			}
		}

		f32 const zoomFactor = 1.25f;
		if (auto wheel = input.mouseWheel; wheel > 0) {
			targetCamZoom *= zoomFactor;
		} else if (wheel < 0) {
			targetCamZoom /= zoomFactor;
		}
		// targetCamZoom = clamp(
		//	targetCamZoom, 2.0f / (VOXEL_MAP_WIDTH - 1) * max(1.0f, ((f32)window.clientSize.x / window.clientSize.y)),
		//	0.5f);
		camZoom = lerp(camZoom, targetCamZoom, min(time.delta * 15, 1.0f));

		tilesToDraw.clear();
		tilesToDraw.reserve(CHUNK_WIDTH * CHUNK_WIDTH + bullets.size() + embers.size() + explosions.size() + 1);
		lightsToDraw.clear();
		lightsToDraw.reserve(bullets.size() + explosions.size() + 1);

		static UnorderedList<LightTile> allRaycastTargets;
		allRaycastTargets.clear();

		for (u32 x = 0; x < CHUNK_WIDTH; ++x) {
			for (u32 y = 0; y < CHUNK_WIDTH; ++y) {
				v4 color = V4f(1);
				if (getTile(tiles, x, y)) {
					allRaycastTargets.push_back({(v2)v2u{x, y}, 0.5f, 0.0f});
				} else {
					color.xyz *= 0.25f;
				}
				tilesToDraw.push_back(
					createTile((v2)v2u{x, y}, offsetAtlasTile(1, 0), ATLAS_ENTRY_SIZE, V2f(1.0f), 0.0f, color));
			}
		}

		PROFILE_BEGIN("prepare tiles & raycast targets");
		for (auto &b : bullets) {
			u32 frame = (u32)(b.remainingLifetime * 12) % 4;
			v2u framePos{frame % 2, frame / 2};
			tilesToDraw.push_back(createTile(b.position, offsetAtlasTile(0, 1) + (v2)framePos * ATLAS_ENTRY_SIZE * 0.5f,
											 ATLAS_ENTRY_SIZE * 0.5f, V2f(0.5f), b.rotation));
			lightsToDraw.push_back(createLight({.2, .3, 1}, b.position, 1, (v2)window.clientSize));
			allRaycastTargets.push_back({b.position, 0.25f, v3{.2, .3, 1} * 0.5f});
		}
		for (auto &e : embers) {
			f32 t = e.remainingLifeTime / e.maxLifeTime;
			f32 tt = t * t;
			auto uvs = getFrameUvs(1 - t, 16, 4, {1, 1}, 0.25f, false);
			tilesToDraw.push_back(createTile(e.position, uvs.uv0, uvs.uv1, uvs.uvMix, ATLAS_ENTRY_SIZE * 0.25f,
											 V2f(map(tt, 0, 1, 0.1f, 0.25f)), t * 3 + e.rotationOffset));
			//	allRaycastTargets.push_back({e.position, max(0.0f, t - 0.9f) * 5.0f, v3{1, .5, .1} * 10.0f});
			lightsToDraw.push_back(createLight(v3{1, .5, .1} * tt * 5, e.position, .5f, (v2)window.clientSize));
		}
		for (auto &e : explosions) {
			f32 t = e.remainingLifeTime / e.maxLifeTime;

			auto uvs = getFrameUvs(1 - t, 8, 2, {2, 0}, 1, false);

			tilesToDraw.push_back(createTile(e.position, uvs.uv0, uvs.uv1, uvs.uvMix, ATLAS_ENTRY_SIZE,
											 V2f(map(t, 0, 1, 0.75f, 1)), t * 2 + e.rotationOffset));
			allRaycastTargets.push_back({e.position, max(0.0f, t - 0.8f) * 5.0f, v3{1, .5, .1} * 10.0f});
			lightsToDraw.push_back(createLight(v3{1, .5, .1} * t * 5, e.position, 3, (v2)window.clientSize));
		}
		for (auto &bot : bots) {
			tilesToDraw.push_back(createTile(bot.position, offsetAtlasTile(0, 0), ATLAS_ENTRY_SIZE));
		}
		tilesToDraw.push_back(createTile(playerP, offsetAtlasTile(0, 0), ATLAS_ENTRY_SIZE));

		if (hasUltimateAttack()) {
			auto uvs = getFrameUvs(time.time, 8, 2, {4, 0}, 2);
			tilesToDraw.push_back(createTile(playerP, uvs.uv0, uvs.uv1, uvs.uvMix, ATLAS_ENTRY_SIZE * 2,
											 V2f(1 + auraSize), time.time * pi, V4f(V3f(1), auraSize)));
		}

		allRaycastTargets.push_back({playerP, 0.4f, V3f(1.0f)});
		// lightsToDraw.push_back(createLight(V3f(1), playerP, 7, (v2)window.clientSize));
		PROFILE_END;

		renderer.updateBuffer(tilesBuffer, tilesToDraw.data(), (u32)tilesToDraw.size() * sizeof(tilesToDraw[0]));
		renderer.updateBuffer(lightsBuffer, lightsToDraw.data(), (u32)lightsToDraw.size() * sizeof(lightsToDraw[0]));

		// for(auto &bot : bots) {
		//	allRaycastTargets.push_back({bot.position, 0.3f, V3f(0.6f, 0.5f, 0.4f)});
		//}

		moveLightAtlas(lightAtlas, floor(playerP + 1.0f));
		u64 raycastCyBegin = __rdtsc();
		debugProfile.totalRaysCast = updateLightAtlas(lightAtlas, renderer, time.frameCount & 1, scaledDelta,
													  allRaycastTargets);
		debugProfile.raycastCy = __rdtsc() - raycastCyBegin;
		renderer.updateTexture(voxelsTex, lightAtlas.voxels);

		m4 worldMatrix = m4::scaling(camZoom) * m4::scaling((f32)window.clientSize.y / window.clientSize.x, 1, 1) *
						 m4::translation(-cameraP, 0);
		renderer.setMatrix(0, worldMatrix);
		renderer.bindTexture(atlas, Stage::ps, 0);
		renderer.clearRenderTarget(msRenderTarget, V4f(.1));
		renderer.bindRenderTarget(msRenderTarget);
		renderer.bindBuffer(tilesBuffer, Stage::vs, 0);
		renderer.bindShader(tileShader);
		renderer.setBlend(Blend::srcAlpha, Blend::invSrcAlpha, BlendOp::add);
		renderer.draw((u32)tilesToDraw.size() * 6);

		renderer.bindRenderTarget(diffuseRt);
		renderer.bindRenderTargetAsTexture(msRenderTarget, Stage::ps, 0);
		renderer.bindShader(msaaShader);
		renderer.disableBlend();
		renderer.draw(3);

		renderer.bindRenderTarget(lightMapRt);
		renderer.clearRenderTarget(lightMapRt, {});
		renderer.bindShader(lightShader);
		renderer.bindBuffer(lightsBuffer, Stage::vs, 0);
		renderer.setBlend(Blend::one, Blend::one, BlendOp::add);
		renderer.draw((u32)lightsToDraw.size() * 6);

		v2 voxScale = v2{(f32)window.clientSize.x / window.clientSize.y, 1};
		renderer.setMatrix(0, m4::translation((cameraP - lightAtlas.center) / VOXEL_MAP_WIDTH * 2, 0) *
								  m4::translation(V2f(1.0f / VOXEL_MAP_WIDTH), 0) *
								  m4::scaling(voxScale / VOXEL_MAP_WIDTH / camZoom * 2, 1));
		renderer.bindRenderTarget({0});
		renderer.bindRenderTargetAsTexture(diffuseRt, Stage::ps, 0);
		renderer.bindRenderTargetAsTexture(lightMapRt, Stage::ps, 1);
		renderer.bindTexture(voxelsTex, Stage::ps, 2);
		renderer.bindShader(mergeShader);
		renderer.disableBlend();
		renderer.draw(3);

		UnorderedList<Label> labels;
		labels.reserve(4);

		tilesToDraw.clear();
		UnorderedList<Tile> uiTiles;

		f32 menuAlpha = 1.0f - timeScale;
		if (menuAlpha > 0) {
			v4f menuColor = V4f(V3f(0.5f), menuAlpha * 0.5f);

			v2f menuPanelPos = (v2f)window.clientSize * 0.4f;
			v2f menuPanelSize = V2f(11 * 8, 22) + V2f(8);

			v2f rectPos = menuPanelPos;
			rectPos.y = window.clientSize.y - rectPos.y - menuPanelSize.y;
			uiTiles.push_back(createTile(rectPos + menuPanelSize * 0.5f, offsetAtlasTile(1, 2), ATLAS_ENTRY_SIZE / 16,
										 menuPanelSize, 0, menuColor));

			auto contains = [](auto pos, auto size, auto mp) {
				return pos.x <= mp.x && mp.x < pos.x + size.x && pos.y <= mp.y && mp.y < pos.y + size.y;
			};
			auto button = [&](v2f pos, v2f size, char const *text) {
				v2f rectPos = pos;
				rectPos.y = window.clientSize.y - rectPos.y - size.y;
				bool c = contains(rectPos, size, mousePos);
				uiTiles.push_back(createTile(rectPos + size * 0.5f, offsetAtlasTile(1, 2), ATLAS_ENTRY_SIZE / 16, size,
											 0, c ? V4f(1, 1, 1, menuColor.w) : menuColor));
				labels.push_back({pos + 2, text, V4f(1, 1, 1, menuAlpha)});
				return c && input.mouseUp(0);
			};

			if (button(menuPanelPos + 2, V2f(11 * 8, 22) + V2f(4), "Continue")) {
				if (currentState == State::menu) {
					currentState = State::game;
				}
			}
		}

		auto drawUiTiles = [&]() {
			renderer.updateBuffer(tilesBuffer, uiTiles.data(), (u32)uiTiles.size() * sizeof(uiTiles[0]));
			renderer.setBlend(Blend::srcAlpha, Blend::invSrcAlpha, BlendOp::add);
			renderer.setMatrix(0, m4::translation(-1, -1, 0) * m4::scaling(2.0f / (v2f)window.clientSize, 1));
			renderer.bindBuffer(tilesBuffer, Stage::vs, 0);
			renderer.bindShader(tileShader);
			renderer.bindTexture(atlas, Stage::ps, 0);
			renderer.draw((u32)uiTiles.size() * 6);
		};
		drawUiTiles();

		drawText(renderer, labels, (v2)window.clientSize);
		labels.clear();

		uiTiles.clear();
		uiTiles.push_back(createTile((v2f)mousePos, offsetAtlasTile(0, 2), ATLAS_ENTRY_SIZE, V2f(32.0f)));
		drawUiTiles();

		char buf[256];
		sprintf(buf, "Health: %u%%\nULT: %u%%", health, ultimateAttackPercent);

		labels.push_back({V2f(8, 64), buf});

#if 0
		labels.push_back({V2f(8), R"(

 !"#$%&'()*+,-./
0123456789:;<=>?
@ABCDEFGHIJKLMNO
PQRSTUVWXYZ[\]^_
`abcdefghijklmno
pqrstuvwxyz{|}~


        ¨
        ¸
ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏ
ÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞß
àáâãäåæçèéêëìíîï
ðñòóôõö÷øùúûüýþÿ)"});
#endif
		drawText(renderer, labels, (v2)window.clientSize);
	}
	f64 musicPlayCursor = 0;

	void fillSoundBuffer(Audio const &audio, s16 *subsample, u32 sampleCount) {
		for (auto sound = playingSounds.begin(); sound != playingSounds.end();) {
			if (sound->cursor >= shotSound.sampleCount) {
				playingSounds.erase(sound);
				continue;
			}
			++sound;
		}
		for (auto &sound : playingSounds) {
			v2f volume = V2f(1);
			v2f d = normalize(sound.position - cameraP, {0, 1});
			if (d.x > 0) {
				volume.x -= d.x;
			} else {
				volume.y += d.x;
			}
			f32 dist = distanceSqr(sound.position, cameraP);
			volume = lerp(volume, V2f(1.0f), 1.0f / (dist * 0.1f + 1.0f));
			volume *= 1.0f / (dist * 0.02f + 1.0f);
			memcpy(sound.volume, &volume, sizeof(sound.volume));
		}
		while (sampleCount--) {
			s16 subsamples[2]{};

			if (music.data)
				addSoundSample(audio.sampleRate, subsamples, music, musicPlayCursor, V2f(1).data(), 1.0f, true);

			for (auto &sound : playingSounds) {
				if (sound.sound.data)
					addSoundSample(audio.sampleRate, subsamples, sound.sound, sound.cursor, sound.volume, timeScale);
			}

			for (auto s : subsamples)
				*subsample++ = s;
		}
	}
};

namespace GameAPI {
Profiler::Stats startProfile;
::Random random;
s32 threadCount;
u32 sortIndex = 0;
void fillStartInfo(StartInfo &info) {
	--info.workerThreadCount;
	info.sampleCount = 4;
	threadCount = (s32)info.workerThreadCount;
}
GameState *start(Window &window, Renderer &renderer) {
	GameState *game = new GameState;
	game->init(window, renderer);
	startProfile = Profiler::getStats();
	std::sort(startProfile.entries.begin(), startProfile.entries.end(),
			  [](Profiler::Entry a, Profiler::Entry b) { return a.selfCycles > b.selfCycles; });
	return game;
}
void update(GameState &game, Window &window, Renderer &renderer, Input const &input, Time &time) {
	game.update(window, renderer, input, time);

	if (input.keyDown(Key_f1)) {
		game.debugProfile.mode = (u8)((game.debugProfile.mode + 1) % 4);
	}
	if (game.debugProfile.mode == 1) {
		char debugLabel[256];
		static f32 smoothDelta = time.delta;
		sprintf(debugLabel, R"(delta: %3.1f ms (%3.1f FPS)
raycast: %10llu cy per ray %10llu cy total
rays: %llu
draw calls: %u
tile count: %u)",
				smoothDelta * 1000, 1.0f / smoothDelta,
				game.debugProfile.totalRaysCast == 0 ? 0
													 : game.debugProfile.raycastCy / game.debugProfile.totalRaysCast,
				game.debugProfile.raycastCy, game.debugProfile.totalRaysCast, renderer.drawCalls,
				(u32)game.tilesToDraw.size());
		smoothDelta = lerp(smoothDelta, time.delta, 0.05f);

		UnorderedList<Label> labels;
		labels.push_back({V2f(8), debugLabel});
		game.drawText(renderer, labels, (v2)window.clientSize);
	}
	auto printStats = [](Profiler::Stats const &stats, char *buffer) {
		u32 offset = 0;
		for (auto const &stat : stats.entries) {
			offset += sprintf(buffer + offset, "%35s: %4.1f%%, %10llu cy, %7llu us, %4.1f%%, %10llu cy, %7llu us\n",
							  stat.name, (f64)stat.selfCycles / stats.totalCycles * 100, stat.selfCycles,
							  stat.selfMicroseconds, (f64)stat.totalCycles / stats.totalCycles * 100, stat.totalCycles,
							  stat.totalMicroseconds);
		}
		return offset;
	};
	if (game.debugProfile.mode == 2) {
		UnorderedList<Label> labels;
		UnorderedList<Rect> rects;

		v2s letterSize{11, 22};

		char debugLabel[1024 * 16];
		s32 offset = sprintf(debugLabel, "%35s:  Self:                            Total:\n", "Update profile");
		auto &stats = Profiler::getStats();

		auto contains = [](v2s pos, v2s size, v2s mp) {
			return pos.x <= mp.x && mp.x < pos.x + size.x && pos.y <= mp.y && mp.y < pos.y + size.y;
		};

		v2s mp = input.mousePosition;

		auto button = [&](v2s pos, v2s size) {
			bool c = contains(pos, size, mp);
			rects.push_back({pos, size, V4f(c ? 0.75f : 0.5f)});
			return c && input.mouseUp(0);
		};

		u32 oldSortIndex = sortIndex;

		auto columnHeader = [&](v2s pos, v2s size, u32 index) {
			if (button(pos, size)) {
				if ((sortIndex & 0xF) == index) {
					sortIndex ^= 0x10;
				} else {
					sortIndex = index;
				}
			}
		};

		columnHeader(V2s(8), letterSize * v2s{36, 1}, 0);
		columnHeader(V2s(8) + letterSize * v2s{36, 0}, letterSize * v2s{34, 1}, 1);
		columnHeader(V2s(8) + letterSize * v2s{70, 0}, letterSize * v2s{33, 1}, 2);

		//[](Profiler::Entry a, Profiler::Entry b) { return a.selfCycles > b.selfCycles; }
		static bool (**comparers)(Profiler::Entry, Profiler::Entry) = [] {
			u32 const columnCount = 3;
			static bool (*result[columnCount * 2])(Profiler::Entry, Profiler::Entry);
			result[0] = [](Profiler::Entry a, Profiler::Entry b) { return std::string(a.name).compare(b.name) > 0; };
			result[2] = [](Profiler::Entry a, Profiler::Entry b) { return a.selfCycles < b.selfCycles; };
			result[4] = [](Profiler::Entry a, Profiler::Entry b) { return a.totalCycles < b.totalCycles; };

			result[1] = [](Profiler::Entry a, Profiler::Entry b) { return result[0](b, a); };
			result[3] = [](Profiler::Entry a, Profiler::Entry b) { return result[2](b, a); };
			result[5] = [](Profiler::Entry a, Profiler::Entry b) { return result[4](b, a); };

			return result;
		}();

		std::sort(stats.entries.begin(), stats.entries.end(),
				  comparers[(sortIndex & 0xF) * 2 + (bool)(sortIndex & 0x10)]);

		offset += printStats(stats, debugLabel + offset);
		labels.push_back({V2f(8), debugLabel});

		offset = 0;

		v2s threadInfoPos{letterSize.x * 105, 8};
		s32 y = threadInfoPos.y;
		s32 tableWidth = letterSize.x * 10;
		auto &threadStats = getThreadStats();
		s32 barWidth = 300;
		rects.push_back({threadInfoPos + v2s{tableWidth, 0}, {barWidth, letterSize.y * threadCount}, V4f(0.5f)});
		u32 worksDone = 0;
		for (u32 i = 0; i < (u32)threadStats.size(); ++i) {
			offset += sprintf(debugLabel + offset, "Thread %u:\n", i);
			s32 x = threadInfoPos.x + tableWidth;
			for (auto &stat : threadStats[i]) {
				random.seed = (s32)(s64)stat.workName;
				s32 w = (s32)(stat.cycles * (u32)barWidth / stats.totalCycles);
				v2s pos = {x + 1, y + 1};
				v2s size = {w - 2, letterSize.y - 2};
				rects.push_back({pos, size, V4f(map(random.v3(), -1, 1, 0.5f, 1.0f), 1.0f)});
				if (contains(pos, size, mp)) {
					labels.push_back({(v2)mp - v2{0, (f32)letterSize.y}, stat.workName});
				}
				x += w;
			}
			y += letterSize.y;
			worksDone += (u32)threadStats[i].size();
		}
		labels.push_back({(v2)threadInfoPos, debugLabel});

		game.drawRect(renderer, rects, (v2)window.clientSize);
		game.drawText(renderer, labels, (v2)window.clientSize);
	}
	if (game.debugProfile.mode == 3) {
		char debugLabel[1024 * 16];
		s32 offset = sprintf(debugLabel, "%35s:  Self:                            Total: %10llu cy, %7llu us\n",
							 "Start profile", startProfile.totalCycles, startProfile.totalMicroseconds);
		printStats(startProfile, debugLabel + offset);
		UnorderedList<Label> labels;
		labels.push_back({V2f(8), debugLabel});
		game.drawText(renderer, labels, (v2)window.clientSize);
	}
}
void fillSoundBuffer(GameState &game, Audio const &audio, s16 *subsample, u32 sampleCount) {
	game.fillSoundBuffer(audio, subsample, sampleCount);
}
void shutdown(GameState &game) { delete &game; }
} // namespace GameAPI
  //#include "../../src/game_default_interface.h"