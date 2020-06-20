#include "../../src/game.h"
#include <string>
#include <array>
#include <queue>
#include <random>
#include <unordered_map>

#include <variant>

#pragma warning(disable : 4234) // struct padding
#pragma warning(disable : 4623) // implicitly deleted constructor

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
u32 indexOf(T const *val, Span<T> list) {
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

	for (u32 x = CHUNK_WIDTH / 2 - 1; x < CHUNK_WIDTH / 2 + 1; ++x) {
		for (u32 y = 1; y < 6; ++y) {
			setTile(tiles, x, y, false);
		}
	}
	for (u32 y = 1; y < 4; ++y) {
		setTile(tiles, CHUNK_WIDTH / 2 - 2, y, true);
		setTile(tiles, CHUNK_WIDTH / 2 + 1, y, true);
	}
	return tiles;
}

struct MapGraph {
	struct Node {
		f32 travelCost;
	};
	Node nodes[CHUNK_WIDTH][CHUNK_WIDTH];

	v2u getPos(Node *node) {
		u32 idx = (u32)(node - &nodes[0][0]);
		return { idx / CHUNK_WIDTH, idx % CHUNK_WIDTH };
	}
	List<v2u> getPath(v2u a, v2u b) {
		if (a.x >= CHUNK_WIDTH || a.y >= CHUNK_WIDTH || b.x >= CHUNK_WIDTH || b.y >= CHUNK_WIDTH) {
			return {};
		}
		Node *start = &nodes[a.x][a.y];
		Node *goal = &nodes[b.x][b.y];
		
		struct Stat {
			Node *node;
			f32 cost;
			bool operator>(Stat b) const { return cost > b.cost; }
		};

		std::priority_queue<Stat, std::vector<Stat>, std::greater<Stat>> frontier;
		frontier.push({start, 0});
		std::unordered_map<Node *, Node *> cameFrom;
		std::unordered_map<Node *, f32> costSoFar;
		cameFrom[start] = 0;
		costSoFar[start] = 0;

		Node *tempNeighbors[8];
		Node **lastNeighbor = tempNeighbors;
		while (!frontier.empty()) {
			auto current = frontier.top();
			frontier.pop();

			if (current.node == goal)
				break;

			lastNeighbor = tempNeighbors;
			v2u currentPos = getPos(current.node);
			
			auto pushColumn = [&](u32 x) {
				if (currentPos.y > 0) {
					*lastNeighbor++ = &nodes[x][currentPos.y - 1];
				}
				*lastNeighbor++ = &nodes[x][currentPos.y];
				if (currentPos.y < CHUNK_WIDTH - 1) {
					*lastNeighbor++ = &nodes[x][currentPos.y + 1];
				}
			};

			if (currentPos.x > 0) {
				if (currentPos.y > 0) {
					*lastNeighbor++ = &nodes[currentPos.x - 1][currentPos.y - 1];
				}
				*lastNeighbor++ = &nodes[currentPos.x - 1][currentPos.y];
				if (currentPos.y < CHUNK_WIDTH - 1) {
					*lastNeighbor++ = &nodes[currentPos.x - 1][currentPos.y + 1];
				}
			}
			if (currentPos.y > 0) {
				*lastNeighbor++ = &nodes[currentPos.x][currentPos.y - 1];
			}
			if (currentPos.y < CHUNK_WIDTH - 1) {
				*lastNeighbor++ = &nodes[currentPos.x][currentPos.y + 1];
			}
			if (currentPos.x < CHUNK_WIDTH - 1) {
				if (currentPos.y > 0) {
					*lastNeighbor++ = &nodes[currentPos.x + 1][currentPos.y - 1];
				}
				*lastNeighbor++ = &nodes[currentPos.x + 1][currentPos.y];
				if (currentPos.y < CHUNK_WIDTH - 1) {
					*lastNeighbor++ = &nodes[currentPos.x + 1][currentPos.y + 1];
				}
			}
			

			for (auto nextRef = tempNeighbors; nextRef != lastNeighbor; ++nextRef) {
				auto next = *nextRef;
				f32 newCost = costSoFar[current.node] + distance((v2f)getPos(current.node), (v2f)getPos(next)) * (current.node->travelCost + next->travelCost) * 0.5f;
				if ((costSoFar.find(next) == costSoFar.end()) || (newCost < costSoFar[next])) {
					costSoFar[next] = newCost;
					frontier.push({next, newCost + distance((v2f)getPos(goal), (v2f)getPos(next))});
					cameFrom[next] = current.node;
				}
			}
		}
		auto current = goal;
		List<v2u> path;
		while (current != start) {
		   path.push_back(getPos(current));
		   auto it = cameFrom.find(current);
		   if (it == cameFrom.end())
			   return {};
		   current = it->second;
		}
		path.push_back(getPos(start));
		return path;
	}
};

MapGraph createGraph(TileStorage const &tiles) {
	MapGraph result;
	for(u32 x = 0; x < CHUNK_WIDTH; ++x) {
		for(u32 y = 0; y < CHUNK_WIDTH; ++y) {
			result.nodes[x][y].travelCost = getTile(tiles, x, y) ? 999999.0f : 1.0f;
		}
	}
	return result;
}
struct World {
	TileStorage tiles;
	MapGraph graph;
};

v2f makeValidPosition(v2f p) {
	if (isnan(p.x))
		p.x = 0;
	if (isnan(p.y))
		p.y = 0;
	return clamp(p, v2f{}, V2f(CHUNK_WIDTH - 1));
}
v2f moveEntity(TileStorage const &tiles, v2f from, v2f &velocity, f32 delta, f32 r, f32 bounciness = 0) {
	for (s32 i = 0; i < 4; ++i) {
		if (lengthSqr(velocity) == 0) {
			break;
		}
		v2f a = from;
		v2f b = a + velocity * delta;
		bool hit = false;
		v2s tMin = max(roundToInt(min(a, b) - (r + 0.5f)), V2s(0));
		v2s tMax = min(roundToInt(max(a, b) + (r + 0.5f)) + 1, V2s(CHUNK_WIDTH));
		for (s32 tx = tMin.x; tx < tMax.x; ++tx) {
			for (s32 ty = tMin.y; ty < tMax.y; ++ty) {
				if (!getTile(tiles, (u32)tx, (u32)ty))
					continue;
				v2f tilef = (v2f)v2s{tx, ty};

				v2f tileMin = tilef - 0.5f;
				v2f tileMax = tilef + 0.5f;

				v2f normal{};
				if (tileMin.x < b.x && b.x <= tileMax.x) {
					if (tilef.y < b.y && b.y < tilef.y + 0.5f + r) {
						from.y = tilef.y + 0.5f + r;
						normal = {0, 1};
					} else if (tilef.y - 0.5f - r < b.y && b.y < tilef.y) {
						from.y = tilef.y - 0.5f - r;
						normal = {0, -1};
					}
				} else if (tileMin.y < b.y && b.y <= tileMax.y) {
					if (tilef.x < b.x && b.x < tilef.x + 0.5f + r) {
						from.x = tilef.x + 0.5f + r;
						normal = {1, 0};
					} else if (tilef.x - 0.5f - r < b.x && b.x < tilef.x) {
						from.x = tilef.x - 0.5f - r;
						normal = {-1, 0};
					}
				} else {
					auto doCorner = [&](v2f corner) {
						if (lengthSqr(b - corner) < r * r) {
							normal = normalize(b - corner);
							from = corner + normal * r;
						}
					};
					if (b.x > tileMax.x && b.y > tileMax.y)
						doCorner(tileMax);
					else if (b.x <= tileMin.x && b.y <= tileMin.y)
						doCorner(tileMin);
					else if (b.x <= tileMin.x && b.y > tileMax.y)
						doCorner({tileMin.x, tileMax.y});
					else if (b.x > tileMax.x && b.y <= tileMin.y)
						doCorner({tileMax.x, tileMin.y});
				}
				hit = normal != v2f{};
				velocity -= normal * dot(velocity, normal) * (1 + bounciness);
				if (hit)
					break;
			}
			if (hit)
				break;
		}
	}
	return makeValidPosition(from + velocity * delta);
}

#define MAX_TILES  (1024 * 256)
#define MAX_LIGHTS (1024 * 256)

#define ATLAS_SIZE		 V2f(8)
#define ATLAS_ENTRY_SIZE (1.0f / ATLAS_SIZE)

v2f offsetAtlasTile(v2f v) { return v * ATLAS_ENTRY_SIZE; }
v2f offsetAtlasTile(f32 x, f32 y) { return offsetAtlasTile({x, y}); }

struct Bullet {
	v2f position;
	v2f velocity;
	f32 remainingLifetime = 5.0f;
	f32 rotation;
	u32 ignoredLayers;
	u32 damage;
};
struct Explosion {
	v2f position;
	f32 remainingLifeTime;
	f32 maxLifeTime;
	f32 rotationOffset;
};
struct Ember {
	v2f position;
	v2f velocity;
	v3f color;
	f32 remainingLifeTime;
	f32 maxLifeTime;
	f32 rotationOffset;
	f32 drag;
};
struct Tile {
	v4f color;

	v2f position;
	v2f size;

	v2f uv0;
	v2f uv1;

	v2f uvScale;
	f32 uvMix;
	f32 rotation;
};
struct FrameUvs {
	v2f uv0;
	v2f uv1;
	f32 uvMix;
};
struct Light {
	v3f color;
	f32 radius;
	v2f position;
};
struct Bot {
	v2f position;
	v2f velocity;
	f32 fireTimer;
	s32 health;
	bool isBoss;
	f32 getRadius() const {
		f32 const r = 0.4f;
		return isBoss ? r * 2 : r;
	}
};

struct Label {
	v2f p;
	String<TempAllocator> text;
	v4f color = V4f(1);
};
struct Rect {
	v2s pos;
	v2s size;
	v4f color;
};
enum class RaycastResultType { noHit, tile, bot, player };
struct RaycastResult {
	RaycastResultType type;
	u32 target;
};
enum RaycastLayer { player = 0x1, bot = 0x2 };
RaycastResult raycast(TileStorage const *tiles, Span<Bot> bots, Span<v2f> players, v2f a, v2f b, v2f &point,
					  v2f &normal) {
	if (tiles) {
		v2f tMin, tMax;
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
			if (raycastRect(a, moveAway(b, a, max(dot(a - b, bot.velocity), 0)), bot.position, V2f(bot.getRadius()), point,
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

v2f screenToWorld(v2f p, v2f clientSize, v2f cameraP, f32 cameraZoom) {
	return cameraP + (p * 2 - clientSize) / (clientSize.y * cameraZoom);
}

#include "light_atlas.h"

void addSoundSamples(u32 targetSampleRate, s16 *dstSubsample, u32 dstSampleCount, SoundBuffer const &buffer, f64 &playCursor, f32 volume[2],
					f32 pitch = 1.0f, bool looping = false, bool resample = true) {
	while (dstSampleCount--) {
		if (resample) {
			// TODO: probably buggy
			f64 sampleOffset = map(playCursor, 0.0, (f64)targetSampleRate, 0.0, (f64)buffer.sampleRate);

			if (looping)
				sampleOffset = mod(sampleOffset, buffer.sampleCount);
			else 
				sampleOffset = min(sampleOffset, buffer.sampleCount - 1);

			s64 sampleOffset0 = floorToInt(sampleOffset);
			s64 sampleOffset1 =  ceilToInt(sampleOffset);

			if (buffer.numChannels == 2) {
				s16 srcSubsampleL0 = ((s16 *)buffer.data)[sampleOffset0 * buffer.numChannels    ];
				s16 srcSubsampleL1 = ((s16 *)buffer.data)[sampleOffset1 * buffer.numChannels    ];
				s16 srcSubsampleR0 = ((s16 *)buffer.data)[sampleOffset0 * buffer.numChannels + 1];
				s16 srcSubsampleR1 = ((s16 *)buffer.data)[sampleOffset1 * buffer.numChannels + 1];

				f32 result[2];
				f32 t = (f32)frac(sampleOffset);
				result[0] = lerp((f32)srcSubsampleL0, (f32)srcSubsampleL1, t);
				result[1] = lerp((f32)srcSubsampleR0, (f32)srcSubsampleR1, t);

				for (u32 channelIndex = 0; channelIndex < 2; ++channelIndex) {
					*dstSubsample++ += (s16)(result[channelIndex] * volume[channelIndex] * 0.125f);
				}
			} else if (buffer.numChannels == 1) {
				s16 srcSubsample0 = ((s16 *)buffer.data)[sampleOffset0];
				s16 srcSubsample1 = ((s16 *)buffer.data)[sampleOffset1];

				f32 result = lerp((f32)srcSubsample0, (f32)srcSubsample1, (f32)frac(sampleOffset));
				for (u32 channelIndex = 0; channelIndex < 2; ++channelIndex) {
					*dstSubsample++ += (s16)(result * volume[channelIndex] * 0.125f);
				}
			} else {
				INVALID_CODE_PATH("audio channel count mismatch");
			}

			playCursor += pitch;
			if (playCursor > buffer.sampleCount) {
				if (looping) {
					playCursor -= buffer.sampleCount;
				} else {
					break;
				}
			}
		} else {
			u32 sampleOffset = (u32)roundToInt(map(playCursor, 0.0, (f64)targetSampleRate, 0.0, (f64)buffer.sampleRate));

			if (looping)
				sampleOffset %= buffer.sampleCount;
			else
				sampleOffset = min(sampleOffset, buffer.sampleCount - 1);

			s16 *musicSubsample = (s16 *)buffer.data + sampleOffset * buffer.numChannels;
			if (buffer.numChannels == 2) {
				for (u32 channelIndex = 0; channelIndex < 2; ++channelIndex) {
					*dstSubsample++ += (s16)(*musicSubsample++ * volume[channelIndex] * 0.125f);
				}
			} else if (buffer.numChannels == 1) {
				s16 srcSample = *musicSubsample++;
				for (u32 channelIndex = 0; channelIndex < 2; ++channelIndex) {
					*dstSubsample++ += (s16)(srcSample * volume[channelIndex] * 0.125f);
				}
			} else {
				INVALID_CODE_PATH("audio channel count mismatch");
			}

			playCursor += pitch;
			if (playCursor > buffer.sampleCount) {
				if (looping) {
					playCursor -= buffer.sampleCount;
				} else {
					break;
				}
			}
		}
	}
}
void addSoundSamples(u32 targetSampleRate, s16 *subsample, u32 dstSampleCount, SoundBuffer const &buffer, f64 &playCursor, f32 volume,
					f32 pitch = 1.0f, bool looping = false, bool resample = true) {
	addSoundSamples(targetSampleRate, subsample, dstSampleCount, buffer, playCursor, V2f(volume).data(), pitch, looping, resample);
}
	
v2s const letterSize{10, 16};

struct Game {
	v2f playerP;
	v2f playerV;

	static constexpr f32 playerR = 0.4f;
	static constexpr f32 bulletSpeed = 10.f;

	v2f cameraP;
	f32 targetCamZoom;
	f32 camZoom;
	f32 fireTimer;
	f32 fireDelta;

	v2f pixelsInMeter;

	World world;

	UnorderedList<Bullet> bullets;
	UnorderedList<Explosion> explosions;
	UnorderedList<Ember> embers;

	::Random random{(u32)time(0)};

	RenderTargetId gBuffers[2], basicLightsRt, diffuseVoxelTex, diffusePointTex, rougherVoxelTex;
	ShaderId tileShader, basicTileShader, solidShader, lightShader, mergeShader, lineShader, diffusorShader, diffusorToPointShader, rougherShader;
	TextureId atlasAlbedo, atlasNormal, fontTexture, specularVoxelTex;
	BufferId tilesBuffer, lightsBuffer, debugLineBuffer;

	LightAtlas lightAtlas;
	UnorderedList<LightTile, TempAllocator> allRaycastTargets;
	
	List<Tile, TempAllocator> tilesToDraw;
	List<Light, TempAllocator> lightsToDraw;

	f32 botSpawnTimer;
	f32 botSpawnDelta = 1.23456789f;
	UnorderedList<Bot> bots;
	u32 botsKilled;
	u32 currentWave;
	bool spawnBots;
	u32 totalBotsSpawned;

	s32 shotsAtOnce;
	u32 ultimateAttackPercent;
	f32 ultimateAttackRepeatTimer;
	u32 ultimatePower;
	float auraSize = 0.0f;
	s32 playerHealth;
	bool hasUltimateAttack() { return ultimateAttackPercent == 100; }

	UnorderedList<SoundBuffer> music;
	SoundBuffer shotSound, explosionSound, ultimateSound, coinSound;

	struct PlayingSound {
		SoundBuffer const *buffer;
		bool looping;
		v2f position;
		f32 volume;
		f32 pitch;
		f32 flatness;
		f64 cursor;

		// temporary
		f32 channelVolume[2];

		bool playing() {
			return cursor < buffer->sampleCount;
		}
	};
	PlayingSound createSound(SoundBuffer const &buffer, v2f position, f32 volume = 1.0f, f32 pitch = 1.0f, bool looping = false, f32 flatness = 0.0f) {
		PlayingSound result;
		result.buffer = &buffer;
		result.looping = looping;
		result.position = position;
		result.volume = volume;
		result.pitch = pitch;
		result.flatness = flatness;
		result.cursor = 0.0;
		return result;
	}
	PlayingSound createSound(SoundBuffer const &sound, f32 volume = 1.0f, f32 pitch = 1.0f, bool looping = false) {
		return createSound(sound, {}, volume, pitch, looping, 1.0f);
	}

	UnorderedList<PlayingSound> playingSounds;
	PlayingSound playingMusic{};
	std::mutex soundMutex;
	void pushSound(PlayingSound sound) {
		soundMutex.lock();
		playingSounds.push_back(sound);
		soundMutex.unlock();
	}
	
	UnorderedList<PlayingSound> shotSounds;
	UnorderedList<PlayingSound> explosionSounds;
	UnorderedList<PlayingSound> coinSounds;

	void pushShotSound(v2f position) { shotSounds.push_back(createSound(shotSound, position, 1, 0.9f + 0.2f * random.f32())); }
	void pushExplosionSound(v2f position) { explosionSounds.push_back(createSound(explosionSound, position, 1, 0.9f + 0.2f * random.f32())); }
	void pushUltimateSound(v2f position) { pushSound(createSound(ultimateSound, position, 1, 0.9f + 0.2f * random.f32())); }
	
	f32 masterVolume = 0.25f;

	enum class State { menu, game, pause, death };

	State currentState{};
	f32 timeScale = 0.0f;
	f32 scaledTime;
	
	f32 pauseAlpha = 0.0f;
	f32 uiAlpha = 0.0f;
	f32 deathScreenAlpha = 0.0f;
	f32 menuAlpha = 0.0f;
	f32 shopHintAlpha = 0.0f;
	f32 shopUiAlpha = 0.0f;

	struct Coin {
		v2f position;
		v2f velocity;
		f32 lifeTime = 0.0f;
	};

	UnorderedList<Coin> coinDrops;
	u32 coinsInWallet;
	static constexpr f32 coinRadius = 0.25f;

	bool isPlayerInShop, isPlayerInShopPrevFrame;
	bool shopIsAvailable;
	bool showNoobText = true;
	struct Upgrade {
		char const *name;
		char const *description;
		u32 cost;
		void (*purchaseAction)(Game &, Upgrade &);
		bool (*extraCheck)(Game &, char const *&);
		String<TempAllocator> (*currentValue)(Game &);
	};
	List<Upgrade> upgrades;
	f32 showPurchaseFailTime = 0.0f;

	bool hasCoinMagnet;
	bool explosiveBullets;
	char const *purchaseFailReason;

	struct DebugProfile {
		f32 raycastMS;
		u8 mode;
	};
	DebugProfile debugProfile{};
	
	struct DebugLine {
		v2f a, b;
		v4f color;
	};
	List<DebugLine, TempAllocator> debugLines;

	void pushDebugLine(v2f a, v2f b, v4f color) {
		DebugLine result;
		result.a = a;
		result.b = b;
		result.color = color;
		debugLines.push_back(result);
	}
	void pushDebugBox(v2f boxMin, v2f boxMax, v4f color) {
		v2f minMax = {boxMin.x, boxMax.y};
		v2f maxMin = {boxMax.x, boxMin.y};
		pushDebugLine(boxMin, minMax, color);
		pushDebugLine(boxMin, maxMin, color);
		pushDebugLine(boxMax, minMax, color);
		pushDebugLine(boxMax, maxMin, color);
	}
	
	bool debugDrawBotPath = false;
	bool debugDrawMapGraph = false;
	bool debugDrawRaycastTargets = false;
	bool debugUpdateBots = true;
	bool debugGod = false;
	bool debugSun = false;
	
	enum class DebugValueType { u1, u8, u16, u32, u64, s8, s16, s32, s64 };

	DebugValueType getDebugValueType(bool) { return DebugValueType::u1; }
	DebugValueType getDebugValueType(u8  ) { return DebugValueType::u8; }
	DebugValueType getDebugValueType(u16 ) { return DebugValueType::u16; }
	DebugValueType getDebugValueType(u32 ) { return DebugValueType::u32; }
	DebugValueType getDebugValueType(u64 ) { return DebugValueType::u64; }
	DebugValueType getDebugValueType(s8  ) { return DebugValueType::s8; }
	DebugValueType getDebugValueType(s16 ) { return DebugValueType::s16; }
	DebugValueType getDebugValueType(s32 ) { return DebugValueType::s32; }
	DebugValueType getDebugValueType(s64 ) { return DebugValueType::s64; }

	struct DebugValueEntry {
		void *data;
		DebugValueType type;
		char const *name;
	};

	List<DebugValueEntry> debugValues;

	void initDebug() {
		
#define DEBUG_MEMBER(name) debugValues.push_back({&name, getDebugValueType(name), #name})

		DEBUG_MEMBER(coinsInWallet);
		DEBUG_MEMBER(isPlayerInShop);
		DEBUG_MEMBER(isPlayerInShopPrevFrame);
		DEBUG_MEMBER(shopIsAvailable);
		DEBUG_MEMBER(spawnBots);
	}


	RaycastResult raycast(u32 ignoredLayers, v2f a, v2f b, v2f &point, v2f &normal) {
		Span players = {&playerP, 1};
		return ::raycast(&world.tiles, (ignoredLayers & RaycastLayer::bot) ? Span<Bot>{} : bots,
						 (ignoredLayers & RaycastLayer::player) ? Span<v2f>{} : players, a, b, point, normal);
	}
	void spawnEmbers(v2f position, v2f incoming, v2f normal) {
		v2f nin = normalize(incoming);
		f32 range = -dot(nin, normal);
		v2f rin = reflect(nin, normal);
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
			e.drag = 10.0f;
			e.color = v3f{1, .5, .2};
			embers.push_back(e);
		}
	
		pushExplosionSound(position);
	}
	void spawnExplosion(v2f position, v2f incoming, v2f normal) {
		Explosion ex;
		ex.position = position;
		ex.maxLifeTime = ex.remainingLifeTime = 0.5f + random.f32() * 0.25f;
		ex.rotationOffset = random.f32() * (2 * pi);
		explosions.push_back(ex);

		spawnEmbers(position, incoming, normal);
	}
	void spawnBullet(u32 ignoredLayers, v2f position, f32 rotation, f32 velocity, u32 damage) {
		Bullet b;
		b.damage = damage;
		b.ignoredLayers = ignoredLayers;
		b.position = position;
		b.rotation = rotation;
		sincos(rotation, b.velocity.y, b.velocity.x);
		b.velocity *= velocity;
		bullets.push_back(b);
	}
	void spawnBullet(u32 ignoredLayers, v2f position, v2f velocity, u32 damage) {
		Bullet b;
		b.damage = damage;
		b.ignoredLayers = ignoredLayers;
		b.position = position;
		b.velocity = velocity;
		b.rotation = atan2(normalize(velocity));
		bullets.push_back(b);
	}

	void spawnBot(v2f position, bool boss) {
		Bot newBot{};
		newBot.isBoss = boss;
		newBot.health = (s32)(boss ? currentWave * 5 : currentWave);
		newBot.position = position;
		bots.push_back(newBot);
		++totalBotsSpawned;
	}
	
	NOINLINE void pushRaycastTarget(v2f pos, f32 radius, v3f color) {
		LightTile t;
		t.boxMin = pos - radius;
		t.boxMax = pos + radius;
		t.color = color;
		allRaycastTargets.push_back(t);
	}
	void pushTile(List<Tile, TempAllocator> &list, v2f position, v2f uv0, v2f uv1, f32 uvMix, v2f uvScale, v2f size = V2f(1.0f), f32 rotation = 0.0f,
					v4f color = V4f(1)) {
		Tile t{};
		t.position = position;
		t.uv0 = uv0;
		t.uv1 = uv1;
		t.uvMix = uvMix;
		t.uvScale = uvScale;
		t.rotation = rotation;
		t.size = size;
		t.color = color;
		list.push_back(t);
	}
	void pushTile(List<Tile, TempAllocator> &list, v2f position, v2f uv0, v2f uvScale, v2f size = V2f(1.0f), f32 rotation = 0.0f, v4f color = V4f(1)) {
		pushTile(list, position, uv0, {}, 0, uvScale, size, rotation, color);
	}
	void pushTile(v2f position, v2f uv0, v2f uv1, f32 uvMix, v2f uvScale, v2f size = V2f(1.0f), f32 rotation = 0.0f, v4f color = V4f(1)) {
		pushTile(tilesToDraw, position, uv0, uv1, uvMix, uvScale, size, rotation, color);
	}
	void pushTile(v2f position, v2f uv0, v2f uvScale, v2f size = V2f(1.0f), f32 rotation = 0.0f, v4f color = V4f(1)) {
		pushTile(tilesToDraw, position, uv0, {}, 0, uvScale, size, rotation, color);
	}
	void pushLight(v3f color, v2f position, f32 radius, v2f clientSize) {
		Light l;
		l.color = color;
		l.radius = camZoom * radius * 2;
		l.position = (position - cameraP) * camZoom;
		l.position.x *= clientSize.y / clientSize.x;
		lightsToDraw.push_back(l);
	}

	void drawText(Renderer &renderer, Span<Label> labels, v2f clientSize) {
		tilesToDraw.clear();
		tilesToDraw.reserve(labels.size() * 64);
		for (auto &l : labels) {
			v2f p = l.p / (v2f)letterSize;
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

				v2f basePosition = p + v2f{column + 0.5f, row - 0.5f};

				Tile t{};
				t.position = basePosition - v2f{-2,2} / (v2f)letterSize;
				t.uv0 = {u, v};
				t.uvScale = V2f(1.0f / 16.0f);
				t.size = V2f(1.0f);
				t.color = {0,0,0,l.color.w};
				tilesToDraw.push_back(t);
				t.color = l.color;
				t.position = basePosition;
				tilesToDraw.push_back(t);
				column += 1.0f;
			}
		}
		renderer.updateBuffer(tilesBuffer, tilesToDraw.data(), (u32)tilesToDraw.size() * sizeof(tilesToDraw[0]));

		renderer.setBlend(Blend::srcAlpha, Blend::invSrcAlpha, BlendOp::add);
		renderer.bindShader(basicTileShader);
		renderer.bindBuffer(tilesBuffer, Stage::vs, 0);
		renderer.bindTexture(fontTexture, Stage::ps, 0);
		renderer.setMatrix(0, m4::translation(-1, 1, 0) * m4::scaling(2 * (v2f)letterSize / clientSize, 0));
		renderer.draw((u32)tilesToDraw.size() * 6, 0);
	}
	void drawRect(Renderer &renderer, Span<Rect> rects, v2f clientSize) {
		tilesToDraw.clear();
		tilesToDraw.reserve(rects.size());
		for (auto r : rects) {
			r.pos.y = (s32)clientSize.y - r.pos.y;
			pushTile((v2f)r.pos + (v2f)r.size * V2f(0.5f, -0.5f), {}, {}, (v2f)r.size, 0, r.color);
		}
		renderer.updateBuffer(tilesBuffer, tilesToDraw.data(), (u32)tilesToDraw.size() * sizeof(tilesToDraw[0]));

		renderer.setMatrix(0, m4::translation(-1, -1, 0) * m4::scaling(2.0f / clientSize, 0));

		renderer.setBlend(Blend::srcAlpha, Blend::invSrcAlpha, BlendOp::add);
		renderer.bindShader(solidShader);
		renderer.bindBuffer(tilesBuffer, Stage::vs, 0);
		renderer.draw((u32)tilesToDraw.size() * 6, 0);
	}

	void setShopOpen(bool open) {
		setTile(world.tiles, CHUNK_WIDTH / 2 - 1, 3, !open);
		setTile(world.tiles, CHUNK_WIDTH / 2	, 3, !open);
	}
	bool isInShop(v2f p) {
		return CHUNK_WIDTH / 2 - 2 < p.x && p.x < CHUNK_WIDTH / 2 + 1 &&
							  0 < p.y && p.y < 4;
	}

	void pushUpgrade(char const *name, char const *description, u32 cost, void (*purchaseAction)(Game &, Upgrade &), bool (*extraCheck)(Game &, char const *&), String<TempAllocator> (*currentValue)(Game &)) {
		Upgrade result;
		result.name = name;
		result.description = description;
		result.cost = cost;
		result.purchaseAction = purchaseAction;
		result.extraCheck = extraCheck;
		result.currentValue = currentValue;
		upgrades.push_back(result);
	}
	void initUpgrades() {
		pushUpgrade("Аптека", 
					"Лечит до 100%",
					30, 
					[](Game &state, Upgrade &thisUpgrade) { state.playerHealth = 100; }, 
					[](Game &state, char const *&msg) { msg = "Ты здоров"; return state.playerHealth < 100; },
					[](Game &state) { return toString<TempAllocator>(state.playerHealth); });
		pushUpgrade("+1 выстрел",
					"Превратись в дробовик",
					40, 
					[](Game &state, Upgrade &thisUpgrade) { ++state.shotsAtOnce; },
					[](Game &state, char const *&msg) { msg = "Максимал"; return state.shotsAtOnce < 10; },
					[](Game &state) { return toString<TempAllocator>(state.shotsAtOnce); });
		pushUpgrade("Магнит",
					"Притягивай монеты в радиусе 5 метров",
					100,
					[](Game &state, Upgrade &thisUpgrade) { state.hasCoinMagnet = true; },
					[](Game &state, char const *&msg) { msg = "Он уже есть у тебя"; return !state.hasCoinMagnet; },
					[](Game &state) { return toString<TempAllocator>(state.hasCoinMagnet); });
		pushUpgrade("Скорость атаки +10%", 
					"Скорострел", 
					30,
					[](Game &state, Upgrade &thisUpgrade) { thisUpgrade.cost = (u32)(thisUpgrade.cost * 1.1f); state.fireDelta = 1.0f / (1.1f / state.fireDelta); },
					[](Game &state, char const *&msg) { msg = "Это максимум"; return state.fireDelta > 0.1f; },
					[](Game &state) { return toString<TempAllocator>(1.0f/state.fireDelta); });
		pushUpgrade("Разрывной патрон", 
					"Маги получают урон от взрыва в радиусе 1.5 метра", 
					100,
					[](Game &state, Upgrade &thisUpgrade) { state.explosiveBullets = true; },
					[](Game &state, char const *&msg) { msg = "Ты уже купил это"; return !state.explosiveBullets; },
					[](Game &state) { return toString<TempAllocator>(state.explosiveBullets); });
		pushUpgrade("Сила ультимейта", 
					"Мощь", 
					40,
					[](Game &state, Upgrade &thisUpgrade) { ++state.ultimatePower; },
					[](Game &state, char const *&msg) { msg = "Мощнее некуда!"; return state.ultimatePower < 10; },
					[](Game &state) { return toString<TempAllocator>(state.ultimatePower); });
	}
	std::mt19937 mt{std::random_device{}()};
	void playNextMusic() {
		auto generatePlaylist = [&]() {
			std::shuffle(music.begin(), music.end(), mt);
			playingMusic.buffer = music.begin();
		};
		playingMusic.cursor = 0;
		if (!playingMusic.buffer) {
			generatePlaylist();
			return;
		}
		++playingMusic.buffer;
		if (playingMusic.buffer == music.end()) {
			generatePlaylist();
		}
	}
	void startNewGame(Renderer &renderer) {
		currentState = State::game;
		setCursorVisibility(false);

		playerHealth = 100;
		playerP = v2f{(CHUNK_WIDTH - 1) * 0.5f, 1.5f};
		playerV = {};
		botsKilled = 0;
		currentWave = 0;
		spawnBots = false;
		totalBotsSpawned = 0;
		botSpawnTimer = 0.0f;
		scaledTime = 0.0f;
		shotsAtOnce = 1;
		hasCoinMagnet = false;
		explosiveBullets = false;
		fireDelta = 60.0f / 113.5f;
		
		cameraP = playerP;
		targetCamZoom = 1.0f / (estimatedLightAtlasHeight / 2 - 1);
		camZoom = targetCamZoom;
		fireTimer = 0.0f;

		
		ultimateAttackPercent = 0;
		ultimateAttackRepeatTimer = 0.0f;
		ultimatePower = 1;

		random = {};

		isPlayerInShop = true;
		isPlayerInShopPrevFrame = true;

		bots.clear();
		bullets.clear();
		explosions.clear();
		embers.clear();
		coinDrops.clear();
		coinsInWallet = 0;

		soundMutex.lock();
		playingSounds.clear();
		soundMutex.unlock();

		memset(lightAtlas.voxels, 0, sizeof(lightAtlas.voxels));
		renderer.updateTexture(specularVoxelTex, lightAtlas.voxels);
	}

	u32 estimatedLightAtlasHeight;
	bool enableCheckerboard;
	bool skipLightUpdateFrame;
	
	struct BaseSaveVar {
		void *data;
		umm size;
		char const *name;
		template <class T>
		BaseSaveVar(T &var, char const *name) : data(&var), size(sizeof(var)), name(name) {}
		
		void write(StringBuilder<TempAllocator> &builder) {
			builder.append(Span{(char *)data, size});
		}
		umm read(void const *from) {
			memcpy(data, from, size);
			return size;
		}
	};
	template <class List>
	struct SpanSaveVar {
		List &list;
		char const *name;
		SpanSaveVar(List &list, char const *name) : list(list), name(name) {}
		
		void write(StringBuilder<TempAllocator> &builder) {
			//Log::print("writing {}", name);
			umm size = list.size();
			builder.append(Span{(char *)&size, sizeof(size)});
			if (size) {
				builder.append(Span{(char *)list.data(), size * sizeof(list[0])});
			}
		}
		umm read(void const *from) {
			//Log::print("reading {}", name);
			umm size = *(umm*)from;
			list.resize(size);
			memcpy(list.data(), (char *)from + sizeof(umm), size * sizeof(list[0]));
			return size * sizeof(list[0]) + sizeof(umm);
		}
	};

	template <class T> static constexpr bool isSpanSaveVar = false;
	template <class T> static constexpr bool isSpanSaveVar<SpanSaveVar<T>> = true;

	List<
		std::variant<
			BaseSaveVar, 
			SpanSaveVar<UnorderedList<Coin>>,
			SpanSaveVar<UnorderedList<Bullet>>,
			SpanSaveVar<UnorderedList<Bot>>,
			SpanSaveVar<UnorderedList<Explosion>>,
			SpanSaveVar<UnorderedList<Ember>>
		>
	> saveVars;

	template <class T> 
	void saveVar(T &var, char const *name) { saveVars.push_back(BaseSaveVar(var, name)); }
	template <class T> 
	void saveVar(List<T> &var, char const *name) { 
		saveVars.emplace_back(SpanSaveVar(var, name)); 
	}
	template <class T> 
	void saveVar(UnorderedList<T> &var, char const *name) { 
		saveVars.emplace_back(SpanSaveVar(var, name)); 
	}

	void init() {
		PROFILE_FUNCTION;
		
		Log::print("Utilizing {} instructions.", getOptimizedModuleName());

		//lightAtlas.optimizedUpdate = updateLightAtlas;
		lightAtlas.optimizedUpdate = (decltype(lightAtlas.optimizedUpdate))getOptimizedProcAddress("updateLightAtlas");
		ASSERT(estimateLightPerformance(lightAtlas, estimatedLightAtlasHeight, enableCheckerboard, skipLightUpdateFrame));

		WorkQueue work;

		work.push(&Game::initDebug, this);

		std::mutex musicMutex;
		for (u32 i = 0; ; ++i) {
			auto path = format(DATA "audio/music{}.wav", i);
			if (fileExists(path)) {
				work.push([this, &musicMutex, path] { 
					auto m = loadWaveFile(path);
					musicMutex.lock();
					music.push_back(m);
					musicMutex.unlock();
				});
			} else {
				break;
			}
		}

		work.push([&]{ shotSound = loadWaveFile(DATA "audio/shot.wav");});
		work.push([&]{ explosionSound = loadWaveFile(DATA "audio/explosion.wav");});
		work.push([&]{ ultimateSound = loadWaveFile(DATA "audio/ultimate.wav");});
		work.push([&]{ coinSound = loadWaveFile(DATA "audio/coin.wav");});
		
		world.tiles = genTiles();
		world.graph = createGraph(world.tiles);

		initUpgrades();
		
#define SAVE_VAR(x) saveVar(x, #x)

		SAVE_VAR(playerP);
		SAVE_VAR(playerV);
		SAVE_VAR(cameraP);
		SAVE_VAR(targetCamZoom);
		SAVE_VAR(camZoom);
		SAVE_VAR(fireTimer);
		SAVE_VAR(fireDelta);
		SAVE_VAR(pixelsInMeter);
		SAVE_VAR(world);
		SAVE_VAR(random);
		SAVE_VAR(bullets);
		SAVE_VAR(explosions);
		SAVE_VAR(embers);
		SAVE_VAR(botSpawnTimer);
		SAVE_VAR(botSpawnDelta);
		SAVE_VAR(bots);
		SAVE_VAR(botsKilled);
		SAVE_VAR(currentWave);
		SAVE_VAR(spawnBots);
		SAVE_VAR(totalBotsSpawned);
		SAVE_VAR(shotsAtOnce);
		SAVE_VAR(ultimateAttackPercent);
		SAVE_VAR(ultimateAttackRepeatTimer);
		SAVE_VAR(ultimatePower);
		SAVE_VAR(auraSize);
		SAVE_VAR(playerHealth);
		SAVE_VAR(masterVolume);
		SAVE_VAR(currentState);
		SAVE_VAR(timeScale);
		SAVE_VAR(scaledTime);
		SAVE_VAR(pauseAlpha);
		SAVE_VAR(uiAlpha);
		SAVE_VAR(deathScreenAlpha);
		SAVE_VAR(menuAlpha);
		SAVE_VAR(shopHintAlpha);
		SAVE_VAR(shopUiAlpha);
		SAVE_VAR(coinDrops);
		SAVE_VAR(coinsInWallet);
		SAVE_VAR(isPlayerInShop);
		SAVE_VAR(isPlayerInShopPrevFrame);
		SAVE_VAR(shopIsAvailable);
		SAVE_VAR(showNoobText);
		for (auto &upgrade : upgrades)
			SAVE_VAR(upgrade.cost);
		SAVE_VAR(showPurchaseFailTime);
		SAVE_VAR(hasCoinMagnet);
		SAVE_VAR(explosiveBullets);
		SAVE_VAR(debugDrawBotPath);
		SAVE_VAR(debugDrawMapGraph);
		SAVE_VAR(debugDrawRaycastTargets);
		SAVE_VAR(debugUpdateBots);
		SAVE_VAR(debugGod);
		SAVE_VAR(debugSun);

		work.completeAllWork();
		
		playNextMusic();

#if 0
		for (u32 i=0;i<CHUNK_WIDTH-2;++i){
			spawnBot(v2f{(f32)i+1, 1});
		}
#endif
	}
	void start(Window &window, Renderer &renderer, Input &input, Time &time) {
		PROFILE_FUNCTION;
#if 0
		char buf[100];
		s32 const testCount = 0x100000;

		PerfTimer timer;
		for (u32 i = 0; i < testCount; ++i) {
			dummy(toString(i, buf));
		}
		Log::print("toString(u32): {} ms\n", timer.getMilliseconds());
		timer.reset();
		for (u32 i = 0; i < testCount; ++i) {
			dummy(ultoa(i, buf, 10));
		}
		Log::print("ultoa: {} ms\n", timer.getMilliseconds());
		timer.reset();
		for (s32 i = -testCount/2; i < testCount/2; ++i) {
			dummy(toString(i, buf, 10));
		}
		Log::print("toString(s32): {} ms\n", timer.getMilliseconds());
		timer.reset();
		for (s32 i = -testCount/2; i < testCount/2; ++i) {
			dummy(itoa(i, buf, 10));
		}
		Log::print("itoa: {} ms\n", timer.getMilliseconds());
#endif
		//time.targetDelta = 0.0f;

		WorkQueue work;

		work.push([&]{ 
			basicTileShader = renderer.createShader(DATA "shaders/tile");
		});
		work.push([&]{ 
			StaticList<ShaderMacro, 4> macros;
			macros.push_back({"NORMAL_OUTPUT"});
			macros.push_back({"CLIP_ALPHA"});
			tileShader = renderer.createShader(DATA "shaders/tile", macros);
		});
		work.push([&]{ 
			StaticList<ShaderMacro, 4> macros;
			macros.push_back({"SOLID"});
			solidShader    = renderer.createShader(DATA "shaders/tile", macros);
		});
		work.push([&]{ lightShader    = renderer.createShader(DATA "shaders/light");});
		work.push([&]{ 
			char roughSampleCountTxt[16];
			StaticList<ShaderMacro, 4> macros;
			macros.push_back({"ROUGH_SAMPLE_COUNT", toStringNT(ROUGH_SAMPLE_COUNT, roughSampleCountTxt).data()});
			macros.push_back({"TO_DIFFUSE"});
			diffusorShader = renderer.createShader(DATA "shaders/diffusor", macros);
		});
		work.push([&]{ 
			char roughSampleCountTxt[16];
			StaticList<ShaderMacro, 4> macros;
			macros.push_back({"ROUGH_SAMPLE_COUNT", toStringNT(ROUGH_SAMPLE_COUNT, roughSampleCountTxt).data()});
			macros.push_back({"TO_POINT"});
			diffusorToPointShader = renderer.createShader(DATA "shaders/diffusor", macros);
		});
		work.push([&]{ 
			char sampleCountTxt[16];
			char roughSampleCountTxt[16];
			StaticList<ShaderMacro, 4> macros;
			macros.push_back({"LIGHT_SAMPLE_COUNT", toStringNT(lightAtlas.sampleCount, sampleCountTxt).data()});
			macros.push_back({"ROUGH_SAMPLE_COUNT", toStringNT(ROUGH_SAMPLE_COUNT, roughSampleCountTxt).data()});
			macros.push_back({"TO_SPECULAR"});
			rougherShader = renderer.createShader(DATA "shaders/diffusor", macros);
		});
		work.push([&]{ lineShader   = renderer.createShader(DATA "shaders/line");});
		work.push([&]{ tilesBuffer  = renderer.createBuffer(0, sizeof(Tile), MAX_TILES);});
		work.push([&]{ lightsBuffer = renderer.createBuffer(0, sizeof(Light), MAX_LIGHTS);});
		work.push([&]{ debugLineBuffer = renderer.createBuffer(0, sizeof(DebugLine), 1024 * 8);});
		work.push([&]{ atlasAlbedo = renderer.createTexture(DATA "textures/atlas_albedo.png", Address::clamp, Filter::point_point);});
		work.push([&]{ atlasNormal = renderer.createTexture(DATA "textures/atlas_normal.png", Address::clamp, Filter::point_point);});
		work.push([&]{ fontTexture = renderer.createTexture(DATA "textures/font.png", Address::clamp, Filter::point_point);});
		
		work.completeAllWork();
	}
	void debugReload() {
		upgrades.clear();
		initUpgrades();
	}
	void resize(Window &window, Renderer &renderer) {
		PROFILE_FUNCTION;

		WorkQueue work;

		work.push([&]{
			u32 multiSampleCount = 1;
			u32 superSampleCount = 1;
			if (gBuffers[0].valid())
				renderer.release(gBuffers[0]);
			gBuffers[0] = renderer.createRenderTarget(window.clientSize * superSampleCount, Format::UN_RGBA8, multiSampleCount, Address::clamp, Filter::linear_linear);

			if (gBuffers[1].valid())
				renderer.release(gBuffers[1]);
			gBuffers[1] = renderer.createRenderTarget(window.clientSize * superSampleCount, Format::UN_RGBA8, multiSampleCount, Address::clamp, Filter::linear_linear);
		});

		work.push([&]{
			if (basicLightsRt.valid())
				renderer.release(basicLightsRt);
			basicLightsRt = renderer.createRenderTarget(window.clientSize, Format::F_RGBA16, Address::clamp, Filter::linear_linear);
		});

		pixelsInMeter = (v2f)window.clientSize / (f32)window.clientSize.y * camZoom;
		
#if 1
		v2u size = V2u(estimatedLightAtlasHeight);
#else
#if BUILD_DEBUG
		v2u size = V2u(6);
#else
		v2u size = V2u(16);
#endif
#endif

		size.x = min(size.x * 2, size.x * window.clientSize.x / window.clientSize.y);

		lightAtlas.resize(size);

		work.push([&]{ 
			StaticList<ShaderMacro, 16> macros;
			macros.push_back({"MSAA_SAMPLE_COUNT", "4"});
			macros.push_back({"LIGHT_SAMPLE_COUNT", toStringNT<TempAllocator>(lightAtlas.sampleCount).data()});
			macros.push_back({"ROUGH_SAMPLE_COUNT", toStringNT<TempAllocator>(ROUGH_SAMPLE_COUNT).data()});
			macros.push_back({"LIGHT_ATLAS_DIM", formatAndTerminate("float2({},{})", lightAtlas.size.x, lightAtlas.size.y).data()});
			mergeShader = renderer.createShader(DATA "shaders/merge", macros);
		});
	
		if (diffusePointTex.valid())
			renderer.release(diffusePointTex);
		diffusePointTex = renderer.createRenderTarget(size, Format::F_RGBA16, Address::border, Filter::linear_point, V4f(1));
	
		if (diffuseVoxelTex.valid())
			renderer.release(diffuseVoxelTex);
		diffuseVoxelTex = renderer.createRenderTarget(size * v2u{ROUGH_SAMPLE_COUNT, 1}, Format::F_RGBA16, Address::border, Filter::linear_point, V4f(1));

		if (rougherVoxelTex.valid())
			renderer.release(rougherVoxelTex);
		rougherVoxelTex = renderer.createRenderTarget(size * v2u{ROUGH_SAMPLE_COUNT, 1}, Format::F_RGBA16, Address::border, Filter::linear_point, V4f(1));
		
		if (specularVoxelTex.valid())
			renderer.release(specularVoxelTex);
		specularVoxelTex = renderer.createTexture(size * v2u{lightAtlas.sampleCount, 1}, Format::F_RGB32, Address::border, Filter::point_point, V4f(1));

		work.completeAllWork();
	}
	void updateBots(f32 scaledDelta) {
		PROFILE_FUNCTION;
		for (auto &bot : bots) {
			v2f hitPoint, hitNormal;
			auto raycastResult = ::raycast(&world.tiles, {}, {&playerP, 1}, bot.position, playerP, hitPoint, hitNormal).type;
					
			f32 const botFireDelta = 120.0f / 113.5f;

			if (bot.fireTimer >= botFireDelta) {
				if (raycastResult == RaycastResultType::player) {
					bot.fireTimer -= botFireDelta;

					u32 botDamage = currentWave;

					if (bot.isBoss) {
						v2f dir = normalize(playerP - bot.position);
						f32 baseAngle = atan2(dir);
						for (u32 angleOffset : Range(5u)) {
							spawnBullet(RaycastLayer::bot, bot.position, baseAngle + (angleOffset - 2.5f) * 0.1f, bulletSpeed, botDamage);
						}
					} else {
						spawnBullet(RaycastLayer::bot, bot.position, normalize(playerP - bot.position) * bulletSpeed, botDamage);
					}
					pushShotSound(bot.position);
				}
			} else {
				bot.fireTimer += scaledDelta;
			}
			auto path = world.graph.getPath((v2u)roundToInt(bot.position), (v2u)roundToInt(playerP));
			if(path.size() > 1) {
				if (debugDrawBotPath) {
					for(u32 i = 1; i < path.size(); ++i) {
						pushDebugLine((v2f)path[i-1], (v2f)path[i], V4f(1,.5,.5,.5));
					}
				}
				v2f targetPosition = (v2f)path[path.size() - 2];

				v2f diff = targetPosition - bot.position;
				v2f velocity = normalize(diff, {1}) * 3;

				if (distanceSqr(playerP, bot.position) < pow2(3.0f) && raycastResult == RaycastResultType::player)
					velocity *= -1;

				for (auto &other : bots) {
					if (&bot != &other) {
						f32 rSum = bot.getRadius() + other.getRadius();
						f32 penetration = rSum - distance(bot.position, other.position);
						if (penetration > 0) {
							v2f dir = normalize(bot.position - other.position);
							bot.position += other.getRadius() / rSum * penetration * dir;
							other.position -= bot.getRadius() / rSum * penetration * dir;
						}
					}
				}
				
				bot.velocity = lerp(bot.velocity, velocity, scaledDelta * 10.0f);
				bot.position = moveEntity(world.tiles, bot.position, bot.velocity, scaledDelta, playerR);
			}
			// if(distanceSqr(bot.position, playerP) < playerR * playerR * 2) {}
		}
#if 1
		if (spawnBots) {
			botSpawnTimer += scaledDelta;
			if (botSpawnTimer >= botSpawnDelta) {
				botSpawnTimer -= botSpawnDelta;

				v2u position;
				do {
					position = random.v2u() % CHUNK_WIDTH;
				} while (getTile(world.tiles, position.x, position.y) ||
							isInShop((v2f)position) ||
							distanceSqr(playerP, (v2f)position) < pow2(8.0f));

				spawnBot((v2f)position, totalBotsSpawned % 50 == 49);
				if (totalBotsSpawned % 50 == 0) {
					spawnBots = false;
				}
			}
		}
#endif	
	}
	void updateBullets(f32 scaledDelta, Window &window) {
		PROFILE_FUNCTION;
		for (u32 i = 0; i < bullets.size(); ++i) {
			auto &bullet = bullets[i];
			bullet.remainingLifetime -= scaledDelta;
			if (bullet.remainingLifetime <= 0) {
				erase(bullets, bullet);
				--i;
				continue;
			}
			v2f nextPos = bullet.position + bullet.velocity * scaledDelta;
			v2f hitPoint, hitNormal;
			auto raycastResult = raycast(bullet.ignoredLayers, bullet.position, nextPos, hitPoint, hitNormal);

			auto damageBot = [&](Bot &bot, u32 damage) {
				bot.health -= damage;
				if (bot.health <= 0) {
					u32 cointCount = bot.isBoss ? 15u : 1u;
					for (u32 c = 0; c < cointCount; ++c) {
						Coin newCoin;
						newCoin.position = bot.position;
						f32 angle = random.f32() * pi * 2;
						sincos(angle, newCoin.velocity.x, newCoin.velocity.y);
						newCoin.velocity *= 3 * (1 + random.f32());
						coinDrops.push_back(newCoin);
					}

					++botsKilled;
					if (!hasUltimateAttack()) {
						++ultimateAttackPercent;
					}

					bots.erase(bot);
					return true;
				}
				return false;
			};
			if (raycastResult.type == RaycastResultType::bot) {
				damageBot(bots[raycastResult.target], bullet.damage);
			} else if (raycastResult.type == RaycastResultType::player) {
				if (playerHealth > 0 && !debugGod) {
					playerHealth -= bullet.damage;
					if (playerHealth <= 0) {
						currentState = State::death;
						setCursorVisibility(true);

						File recordFile("record.txt", File::OpenMode_write);
						DEFER { recordFile.close(); };

						char tempBuf[16];
						recordFile.write("Waves survived: ");
						recordFile.writeLine(toString(currentWave - 1, tempBuf));
						recordFile.write("Wizards killed: ");
						recordFile.write(toString(botsKilled, tempBuf));
					}
				}
			}
			if (raycastResult.type != RaycastResultType::noHit) {
				if (bullet.ignoredLayers & RaycastLayer::player && explosiveBullets) {
					spawnExplosion(hitPoint, normalize(bullet.velocity), hitNormal);
					for (auto bot = bots.begin(); bot != bots.end();) {
						if (distanceSqr(bot->position, hitPoint) < pow2(1.5f)) {
							if (damageBot(*bot, 1)) {
								continue;
							}
						}
						++bot;
					}
				} else {
					spawnEmbers(hitPoint, normalize(bullet.velocity), hitNormal);
				}
				erase(bullets, bullet);
				--i;
				continue;
			}

			bullet.position = nextPos;
		}
	}
	void updateExplosions(f32 scaledDelta) {
		PROFILE_FUNCTION;
		for (u32 i = 0; i < explosions.size(); ++i) {
			auto &e = explosions[i];
			e.remainingLifeTime -= scaledDelta;
			if (e.remainingLifeTime <= 0) {
				erase(explosions, e);
				--i;
				continue;
			}
		}
	}
	void updateEmbers(f32 scaledDelta) {
		PROFILE_FUNCTION;
		for (auto e = embers.begin(); e != embers.end();) {
			e->remainingLifeTime -= scaledDelta;
			if (e->remainingLifeTime <= 0) {
				erase(embers, e);
				continue;
			}
			e->position += e->velocity * scaledDelta;
			e->velocity = moveTowards(e->velocity, {}, scaledDelta * e->drag);
			++e;
		}
	}
	void updateCoins(f32 scaledDelta) {
		PROFILE_FUNCTION;
		for (auto c = coinDrops.begin(); c != coinDrops.end();) {
			if (distanceSqr(c->position, playerP) < pow2(playerR + coinRadius)) {
				++coinsInWallet;
				coinSounds.push_back(createSound(coinSound, c->position, 0.5f, 0.9f + 0.2f * random.f32()));
				coinDrops.erase(c);
				continue;
			}
			c->lifeTime += scaledDelta;
			c->position = moveEntity(world.tiles, c->position, c->velocity, scaledDelta, coinRadius, 1);
			c->velocity = moveTowards(c->velocity, v2f{}, scaledDelta * 10);
			if (hasCoinMagnet) {
				f32 ds = distanceSqr(c->position, playerP);
				if (ds < pow2(5)) {
					c->position = lerp(c->position, playerP, scaledDelta * min(1 / ds, 1) * 10);
				}
			}
			++c;
		}
	}
	void saveState() {
		Log::print("saving...");
		StringBuilder<TempAllocator> builder;
		for (auto &s : saveVars) {
			std::visit([&](auto &var) { var.write(builder); }, s);
		}
		File file("save.bin", File::OpenMode_write);
		auto data = builder.get();
		file.write(data.data(), data.size());
		file.close();
	}
	void loadState() {
		Log::print("loading...");
		auto file = readEntireFile("save.bin");
		umm offset = 0;
		for (auto &s : saveVars) {
			std::visit([&](auto &var) { offset += var.read(file.data.data() + offset); }, s);
		}
		freeEntireFile(file);
	}
	void update(Window &window, Renderer &renderer, Input &input, Time &time) {
		PROFILE_FUNCTION;

		debugLines = {};
		allRaycastTargets = {};
		tilesToDraw = {};
		lightsToDraw = {};

#if 0
		if (input.keyDown(Key_f5)) {
			File coinFile("cointest.txt", File::OpenMode_write);
			DEFER { coinFile.close(); };
			coinFile.writeLine("bitsPerSample: ", coinSound.bitsPerSample);
			coinFile.writeLine("numChannels: ", coinSound.numChannels);
			coinFile.writeLine("sampleCount: ", coinSound.sampleCount);
			coinFile.writeLine("sampleRate: ", coinSound.sampleRate);
			char buf[16];
			for (u32 i = 0; i < coinSound.sampleCount; ++i) {
				for (u32 c = 0; c < coinSound.numChannels; ++c) {
					coinFile.writeLine(toString(((s16 *)coinSound.data)[i * coinSound.numChannels + c], buf));
				}
			}
		}
#endif

		if (window.resized) {
			resize(window, renderer);
		}
		
		auto generateLightAtlasTextures = [&] {
			renderer.updateTexture(specularVoxelTex, lightAtlas.voxels);

			renderer.disableBlend();
			renderer.bindRenderTarget(rougherVoxelTex);
			renderer.bindTexture(specularVoxelTex, Stage::ps, 0);
			renderer.bindShader(rougherShader);
			renderer.draw(3);

			renderer.bindRenderTarget(diffuseVoxelTex);
			renderer.bindTexture(rougherVoxelTex, Stage::ps, 0);
			renderer.bindShader(diffusorShader);
			renderer.draw(3);

			renderer.bindRenderTarget(diffusePointTex);
			renderer.bindShader(diffusorToPointShader);
			renderer.draw(3);
		};
		auto makeLightAtlasWhite = [&] {
			populate(lightAtlas.voxels, 1.0f, lightAtlas.size.x * lightAtlas.size.y * lightAtlas.sampleCount * 3);
			generateLightAtlasTextures();
		};

		if (input.keyHeld(Key_f2)) {
			debugDrawMapGraph ^= input.keyDown('M');
			debugDrawRaycastTargets ^= input.keyDown('R');
			debugDrawBotPath ^= input.keyDown('P');
			debugUpdateBots ^= input.keyDown('B');
			debugGod ^= input.keyDown('G');
			spawnBots ^= input.keyDown('S');
			if (input.keyDown('L')) {
				debugSun ^= 1;
				if (debugSun) {
					makeLightAtlasWhite();
				}
			}
			if (input.keyHeld('U')) {
				ultimateAttackPercent = 100;
			}
			if (input.keyHeld('C')) {
				for (u32 i = 0; i < 10; ++i) {
					Coin coin{};
					f32 a = random.f32() * 2 * pi;
					coin.position = playerP + v2f{sin(a), cos(a)} * (1.0f + 2.0f * random.f32());
					coinDrops.push_back(coin);
				}
			}
			if (input.keyHeld('V')) {
				++currentWave;
			}
			if (input.keyHeld('E')) {
				botSpawnTimer = botSpawnDelta;
			}
		}
		if (input.keyDown(Key_f8)) {
			saveState();
		}
		if (input.keyDown(Key_f9)) {
			loadState();
		}
#if 0
		if (debugDrawMapGraph) {
			for(u32 x=0;x<CHUNK_WIDTH;++x) {
				for(u32 y=0;y<CHUNK_WIDTH;++y) {
					if (auto &node = world.graph.nodes[x][y]) {
						for (auto n : node->neighbors) {
							pushDebugLine((v2f)v2u{x,y}, (v2f)v2u{n.x, n.y}, V4f(V3f(1), 0.1f));
						}
					}
				}
			}
		}
#endif

		shotSounds.clear();
		explosionSounds.clear();
		coinSounds.clear();
		
		tilesToDraw.reserve(CHUNK_WIDTH * CHUNK_WIDTH + bullets.size() + embers.size() + explosions.size() + 128);
		lightsToDraw.reserve(bullets.size() + explosions.size() + 128);

		switch (currentState) {
			case State::menu: {
				if (input.keyDown(Key_enter)) {
					startNewGame(renderer);
					break;
				}
				break;
			}
			case State::pause: {
				if (input.keyDown(Key_escape)) {
					currentState = State::game;
					setCursorVisibility(false);
					break;
				}
				break;
			}
			case State::game: {
				if (input.keyDown(Key_escape)) {
					currentState = State::pause;
					setCursorVisibility(true);
					break;
				}
				break;
			}
			case State::death: {
				if (input.keyDown(Key_enter)) {
					currentState = State::menu;
					memset(lightAtlas.voxels, 0, sizeof(v3f) * lightAtlas.sampleCount * lightAtlas.size.x * lightAtlas.size.y);
					break;
				}
				break;
			}
			default: INVALID_CODE_PATH("invalid state");
		}
		f32 const longestFrameTimeAllowed = 1.f / 15;
		time.delta = min(time.delta, longestFrameTimeAllowed);
		timeScale = moveTowards(timeScale, (f32)(currentState == State::game), time.delta);

		f32 scaledDelta = time.delta * timeScale;
		scaledTime += scaledDelta;

		if (scaledDelta > 0) {
			v2f move{};
			if (currentState == State::game) {
				move.x = (f32)(input.keyHeld('D') - input.keyHeld('A'));
				move.y = (f32)(input.keyHeld('W') - input.keyHeld('S'));
			}
			if (!move.x) move.x = (f32)(input.joyButtonHeld(1) - input.joyButtonHeld(3));
			if (!move.y) move.y = (f32)(input.joyButtonHeld(0) - input.joyButtonHeld(2));

			playerV = moveTowards(playerV, normalize(move, {}) * 5, scaledDelta * 50);

			playerP = moveEntity(world.tiles, playerP, playerV, scaledDelta, playerR);

			cameraP = lerp(cameraP, playerP, min(scaledDelta * 10, 1));

			auraSize = lerp(auraSize, hasUltimateAttack(), scaledDelta * 5.0f);
		
			isPlayerInShopPrevFrame = isPlayerInShop;
			isPlayerInShop = isInShop(playerP);

			if (shopIsAvailable && isPlayerInShopPrevFrame && !isPlayerInShop) {
				// went out

				showNoobText = false;

				setShopOpen(false);

				spawnBots = true;
				++currentWave;
			}

			if(debugUpdateBots) {
				updateBots(scaledDelta);
			}
			updateBullets(scaledDelta, window);

			bool newShopIsAvailable = !spawnBots && bots.empty();
			if (!shopIsAvailable && newShopIsAvailable)
				setShopOpen(true);
			shopIsAvailable = newShopIsAvailable;

			if (hasUltimateAttack()) {
				Ember e;
				v2f dir;
				sincos(random.f32() * 2 * pi, dir.x, dir.y);
				e.velocity = dir * 2;
				e.position = playerP + cross(dir) * 0.5f;
				e.maxLifeTime = e.remainingLifeTime = 0.5f + random.f32();
				e.rotationOffset = 2 * pi * random.f32();
				e.drag = 0.0f;
				e.color = v3f{.3, .6, 1};
				embers.push_back(e);
			}

			updateExplosions(scaledDelta);
			updateEmbers(scaledDelta);
			updateCoins(scaledDelta);
		}

		
		v2s mousePos = input.mousePosition;

		v2f joyAxes = {
			input.joyAxis(JoyAxis_x),
			input.joyAxis(JoyAxis_y)
		};

		if (lengthSqr(joyAxes) > 0.0001f) {
			v2s halfClientSize = (v2s)(window.clientSize >> 1);
			s32 minDim = min(halfClientSize.x, halfClientSize.y);
			mousePos = halfClientSize + (v2s)(joyAxes * 0.9f * (f32)minDim);
		}

		mousePos.y = (s32)window.clientSize.y - mousePos.y;
		v2f mouseWorldPos = screenToWorld((v2f)mousePos, (v2f)window.clientSize, cameraP, camZoom);

		if (currentState == State::game) {
			if (fireTimer > 0) {
				fireTimer -= scaledDelta;
			} else if (!isPlayerInShop) {
				bool holdingFire = input.mouseHeld(0) || input.joyButtonHeld(4);
				if (holdingFire) {
					fireTimer += fireDelta;
					auto createBullet = [&](f32 offset) {
						spawnBullet(RaycastLayer::player, playerP, atan2(normalize(mouseWorldPos - playerP)) + offset, (0.9f + 0.2f * random.f32()) * bulletSpeed, 1);
					};
					if (shotsAtOnce == 1) {
						createBullet(0);
					} else {
						for(s32 i = 0; i < shotsAtOnce; ++i) {
							createBullet((random.f32() - 0.5f) * shotsAtOnce * 0.05f);
						}
					}
					pushShotSound(playerP);
				}
			}
			static u32 lastUltimateRepeatIndex;
			if (!shopIsAvailable && input.mouseDown(1) && hasUltimateAttack()) {
				ultimateAttackPercent = 0;
				lastUltimateRepeatIndex = 4;
				ultimateAttackRepeatTimer = (f32)lastUltimateRepeatIndex;
			}
			ultimateAttackRepeatTimer -= scaledDelta * 4.0f;
			if(lastUltimateRepeatIndex > 0) {

				UnorderedList<v2f, TempAllocator> botPositions;
				botPositions.reserve(bots.size());
				for (auto &b : bots) {
					v2f point, normal;
					if(::raycast(&world.tiles, bots, {}, playerP, b.position, point, normal).type == RaycastResultType::bot) {
						botPositions.push_back(b.position);
					}
				}
				std::sort(botPositions.begin(), botPositions.end(), [this](v2f a, v2f b){ return distanceSqr(playerP, a) < distanceSqr(playerP, b); });

				u32 ultimateRepeatIndex = (u32)ultimateAttackRepeatTimer;
				for(u32 j = ultimateRepeatIndex; j != lastUltimateRepeatIndex; ++j) {
					u32 const ultBulletCount = 25;
					for (u32 i : Range(ultBulletCount)) {
						v2f direction;
						if (i < botPositions.size()) {
							direction = normalize(botPositions[i] - playerP);
						} else {
							sincos(random.f32() * pi * 2, direction.x, direction.y);
						}
						spawnBullet(RaycastLayer::player, playerP, direction * bulletSpeed, ultimatePower);
					}
				}
				if (ultimateRepeatIndex != lastUltimateRepeatIndex) {
					pushUltimateSound(playerP);
				}
				lastUltimateRepeatIndex = ultimateRepeatIndex;
			}
		}
				
		f32 const zoomFactor = 1.25f;
		if (auto wheel = input.mouseWheel; wheel > 0) {
			targetCamZoom *= zoomFactor;
		} else if (wheel < 0) {
			targetCamZoom /= zoomFactor;
		}
		// targetCamZoom = clamp(
		//	targetCamZoom, 2.0f / (VOXEL_MAP_SIZE - 1) * max(1.0f, ((f32)window.clientSize.x / window.clientSize.y)),
		//	0.5f);
		
		camZoom = lerp(camZoom, targetCamZoom, min(time.delta * 15, 1.0f));

#if 0
		struct Test {
			__m256 v;
		};

		List<Test, TempAllocator> test;
		for (u32 i=0;i<32;++i)
			test.push_back({});
#endif

		allRaycastTargets.reserve(1024);

		for (u32 x = 0; x < CHUNK_WIDTH; ++x) {
			for (u32 y = 0; y < CHUNK_WIDTH; ++y) {
				if (getTile(world.tiles, x, y)) {
					pushRaycastTarget((v2f)v2u{x, y}, 0.5f, V3f(0.02f));
					pushTile((v2f)v2u{x, y}, offsetAtlasTile(4, 7), ATLAS_ENTRY_SIZE);
				} else {
					pushTile((v2f)v2u{x, y}, offsetAtlasTile((f32)(randomize(x) % 2), (f32)(randomize(y) % 2)), ATLAS_ENTRY_SIZE, V2f(1), (randomize(x ^ y) % 4) * .5f * pi);
				}
			}
		}
		pushTile(V2f(CHUNK_WIDTH/2 + 0.5f), offsetAtlasTile(2, 0), ATLAS_ENTRY_SIZE * v2f{2, 2}, {2, 2});
		
		for (auto &e : embers) {
			f32 t = e.remainingLifeTime / e.maxLifeTime;
			f32 tt = t * t;
			auto uvs = getFrameUvs(1 - t, 16, 4, {2, 7}, 0.25f, false);
			pushTile(e.position, uvs.uv0, uvs.uv1, uvs.uvMix, ATLAS_ENTRY_SIZE * 0.25f, V2f(map(tt, 0, 1, 0.1f, 0.25f)), t * 3 + e.rotationOffset, V4f(e.color * 2, 1));
			pushLight(e.color * tt * 2, e.position, .5f, (v2f)window.clientSize);
		}
		
		for (auto &c : coinDrops) {
			auto uvs = getFrameUvs(frac(c.lifeTime), 6, 6, {0, 4}, 1);

			pushTile(c.position, uvs.uv0, uvs.uv1, uvs.uvMix, ATLAS_ENTRY_SIZE, V2f(0.5f));
			pushRaycastTarget(c.position, 0.25f, v3f{1, .75, .1} * 0.3333f);
			//lightsToDraw.push_back(createLight(v3f{1, .75, .1}, c.position, 2, (v2f)window.clientSize));
		}

		for (auto &bot : bots) {
			pushTile(bot.position, offsetAtlasTile(3, 7), ATLAS_ENTRY_SIZE, V2f(bot.isBoss ? 2.0f : 1.0f));
			//{bot.position, playerR, {}};
		}

		pushTile(playerP, offsetAtlasTile(3, 7), ATLAS_ENTRY_SIZE);
		// player light
		pushRaycastTarget(playerP, playerR, V3f(0.5f));
		if (hasUltimateAttack()) {
			auto uvs = getFrameUvs(scaledTime, 8, 8, {0, 3}, 1);
			pushTile(playerP, uvs.uv0, uvs.uv1, uvs.uvMix, ATLAS_ENTRY_SIZE, V2f(1 + auraSize), scaledTime * pi, V4f(V3f(1), auraSize));
		}
		
		for (auto &b : bullets) {
			u32 frame = (u32)(b.remainingLifetime * 12) % 4;
			v2u framePos{frame % 2, frame / 2};
			pushTile(b.position, offsetAtlasTile(0, 7) + (v2f)framePos * ATLAS_ENTRY_SIZE * 0.5f, ATLAS_ENTRY_SIZE * 0.5f, V2f(0.5f), b.rotation);
			v3f color = 0.5f * v3f{.4, .5, 1};
			pushLight(color, b.position, 1, (v2f)window.clientSize);
			pushRaycastTarget(b.position, 0.25f, color);
		}
		for (auto &e : explosions) {
			f32 t = e.remainingLifeTime / e.maxLifeTime;

			auto uvs = getFrameUvs(1 - t, 8, 8, {0, 2}, 1, false);

			pushTile(e.position, uvs.uv0, uvs.uv1, uvs.uvMix, ATLAS_ENTRY_SIZE, V2f(map(t, 0, 1, 0.75f, 1.5f)), t * 2 + e.rotationOffset);
			v3f color = 0.5f * v3f{1, .5, .1};
			pushRaycastTarget(e.position, max(0.0f, t - 0.8f) * 5.0f, color * 5.0f);
			pushLight(color * t * 3, e.position, 3, (v2f)window.clientSize);
		}

		// lightsToDraw.push_back(createLight(V3f(1), playerP, 7, (v2f)window.clientSize));
		
		// safe zone
		v2f safeZonePos = {CHUNK_WIDTH/2-0.5f, 1.5f};
		v3f safeZoneColor = 0.5f * v3f{0.3f, 1.0f, 0.6f};
		if (shopIsAvailable) {
			pushRaycastTarget(safeZonePos, 1, safeZoneColor);
			pushLight(safeZoneColor, safeZonePos, 5, (v2f)window.clientSize);
		}
		pushTile(safeZonePos - v2f{0, 1.5f}, offsetAtlasTile(6, 4), ATLAS_ENTRY_SIZE * v2f{2, 1}, {2, 1}, pi * 0.1f);

		renderer.updateBuffer(tilesBuffer, tilesToDraw.data(), (u32)tilesToDraw.size() * sizeof(tilesToDraw[0]));
		renderer.updateBuffer(lightsBuffer, lightsToDraw.data(), (u32)lightsToDraw.size() * sizeof(lightsToDraw[0]));

		// for(auto &bot : bots) {
		//	{bot.position, 0.3f, V3f(0.6f, 0.5f, 0.4f)};
		//}

		renderer.setTopology(Topology::TriangleList);
		renderer.unbindTextures(8, Stage::ps, 0);
		
		if (debugSun) {
			if (lightAtlas.move(floor(playerP + 1.0f))) {
				makeLightAtlasWhite();
			}
		} else {
			bool doLight = true;
			if (skipLightUpdateFrame)
				doLight = time.frameCount & 1;
			if (doLight) {
				lightAtlas.move(floor(playerP + 1.0f));
				if (debugDrawRaycastTargets) {
					for (auto &t : allRaycastTargets) {
						pushDebugBox(t.boxMin, t.boxMax, V4f(t.color, 1));
					}
				}

				PerfTimer timer;
				bool swapChecker = (skipLightUpdateFrame ? time.frameCount / 2 : time.frameCount) & 1;
				lightAtlas.update(enableCheckerboard, swapChecker, scaledDelta, allRaycastTargets);
				debugProfile.raycastMS = lerp(debugProfile.raycastMS, timer.getMilliseconds(), time.delta);
				generateLightAtlasTextures();
			}
		}

		m4 worldMatrix = m4::scaling(camZoom) * m4::scaling((f32)window.clientSize.y / window.clientSize.x, 1, 1) *
						 m4::translation(-cameraP, 0);
		renderer.setMatrix(0, worldMatrix);
		renderer.bindTexture(atlasAlbedo, Stage::ps, 0);
		renderer.bindTexture(atlasNormal, Stage::ps, 1);
		renderer.clearRenderTarget(gBuffers[0], {});
		renderer.clearRenderTarget(gBuffers[1], {});
		renderer.bindRenderTargets(gBuffers);
		renderer.bindBuffer(tilesBuffer, Stage::vs, 0);
		renderer.bindShader(tileShader);
		renderer.disableBlend();
		renderer.draw((u32)tilesToDraw.size() * 6);

		renderer.bindRenderTarget(basicLightsRt);
		renderer.clearRenderTarget(basicLightsRt, {});
		renderer.bindShader(lightShader);
		renderer.bindBuffer(lightsBuffer, Stage::vs, 0);
		renderer.setBlend(Blend::one, Blend::one, BlendOp::add);
		renderer.draw((u32)lightsToDraw.size() * 6);
		
		v2f voxScale = v2f{(f32)window.clientSize.x / window.clientSize.y, 1};
		m4 voxMatrix = m4::translation((cameraP - lightAtlas.center) / (v2f)lightAtlas.size * 2, 0) *
					   m4::translation(V2f(1.0f / (v2f)lightAtlas.size), 0) *
					   m4::scaling(voxScale / (v2f)lightAtlas.size / camZoom * 2, 1);

		renderer.setMatrix(0, voxMatrix);
		renderer.setV4f(0, {menuAlpha, deathScreenAlpha});
		renderer.bindRenderTarget({0});
		renderer.bindTextures(gBuffers, Stage::ps, 0);
		renderer.bindTexture(basicLightsRt, Stage::ps, 2);
		renderer.bindTexture(diffuseVoxelTex, Stage::ps, 3);
		renderer.bindTexture(rougherVoxelTex, Stage::ps, 4);
		renderer.bindTexture(specularVoxelTex, Stage::ps, 5);
		renderer.bindTexture(diffusePointTex, Stage::ps, 6);
		renderer.bindShader(mergeShader);
		renderer.disableBlend();
		renderer.draw(3);
		
		/*
		tilesToDraw.clear();
		for (u32 x = 0; x < CHUNK_WIDTH; ++x) {
			for (u32 y = 0; y < CHUNK_WIDTH; ++y) {
				if (getTile(world.tiles, x, y)) {
					tilesToDraw.push_back(createTile((v2f)v2u{x, y}, offsetAtlasTile(1, 6), ATLAS_ENTRY_SIZE));
				}
			}
		}
		renderer.updateBuffer(tilesBuffer, tilesToDraw.data(), (u32)tilesToDraw.size() * sizeof(tilesToDraw[0]));
		renderer.setMatrix(0, worldMatrix);
		renderer.bindTexture(atlas, Stage::ps, 0);
		renderer.bindBuffer(tilesBuffer, Stage::vs, 0);
		renderer.bindShader(tileShader);
		renderer.setBlend(Blend::srcAlpha, Blend::invSrcAlpha, BlendOp::add);
		renderer.draw((u32)tilesToDraw.size() * 6);
		*/

		//pushDebugLine(playerP, mouseWorldPos, V4f(1,0,0,0.5f));

		renderer.updateBuffer(debugLineBuffer, debugLines.data(), (u32)debugLines.size() * sizeof(debugLines[0]));
		renderer.bindBuffer(debugLineBuffer, Stage::vs, 0);

		renderer.setTopology(Topology::LineList);
		renderer.setMatrix(0, worldMatrix);
		renderer.setBlend(Blend::srcAlpha, Blend::invSrcAlpha, BlendOp::add);
		renderer.bindShader(lineShader);
		renderer.draw((u32)debugLines.size() * 2);

		renderer.setTopology(Topology::TriangleList);

		tilesToDraw.clear();
		
		UnorderedList<Label, TempAllocator> labels;
		labels.reserve(8);

		f32 uiAnimationDelta = time.delta * 4;

		uiAlpha = moveTowards(uiAlpha, (f32)(currentState != State::menu && currentState != State::death), uiAnimationDelta);
		deathScreenAlpha = moveTowards(deathScreenAlpha, (f32)(currentState == State::death), uiAnimationDelta);
		menuAlpha = moveTowards(menuAlpha, (f32)(currentState == State::menu), uiAnimationDelta);
		pauseAlpha = moveTowards(pauseAlpha, (f32)(currentState == State::pause), uiAnimationDelta);
		shopHintAlpha = moveTowards(shopHintAlpha, (f32)shopIsAvailable, uiAnimationDelta);
		shopUiAlpha = moveTowards(shopUiAlpha, (f32)(shopIsAvailable && isPlayerInShop && currentState == State::game), uiAnimationDelta);
		
		if (shopHintAlpha > 0 && !isPlayerInShop && shopIsAvailable) {
			labels.push_back({V2f(8, 256), "Магазин открылся", V4f(1,1,1,shopHintAlpha)});
		}
		
		List<Tile, TempAllocator> solidTiles;
		solidTiles.reserve(16);

		List<Tile, TempAllocator> uiTiles;
		uiTiles.reserve(16);
		
		auto rect = [&](v2f pos, v2f size, v4f color) {
			pos.y = window.clientSize.y - pos.y - size.y;
			pushTile(solidTiles, pos + size * 0.5f, {}, {}, size, 0, color);
		};
		auto contains = [](auto pos, auto size, auto mp) {
			return inBounds(mp, boxMinDim(pos, size));
		};
		if (shopUiAlpha > 0) {
			showPurchaseFailTime -= time.delta;

			v4f uiColor = V4f(V3f(0.0f), shopUiAlpha * 0.5f);
			
			auto button = [&](v2f pos, v2f size, u32 index, auto &&text, bool &hovering) {
				v2f rectPos = pos;
				rectPos.y = window.clientSize.y - rectPos.y - size.y;
				hovering = contains(rectPos, size, (v2f)mousePos);
				rect(pos, size, hovering ? V4f(V3f(0.5f), uiColor.w) : uiColor);
				labels.push_back({pos + 2, move(text), V4f(1, 1, 1, shopUiAlpha)});
				return shopUiAlpha == 1 && hovering && input.mouseUp(0);
			};
			
			v2f buyMenuPos = {(f32)((window.clientSize.x - 300) / 2), (f32)(window.clientSize.y / 2)};

			StringView valueHeaderStr = "Текущее значение";
			//Label label;
			//label.p = buyMenuPos - v2f{0, 26} - v2f{(f32)(valueHeaderStr.size()+1)*letterSize.x + 2, -2};
			//label.color = V4f(1,1,1,shopUiAlpha);
			//label.text = valueHeaderStr;
			//labels.push_back(label);
			labels.push_back({buyMenuPos - v2f{0, 26} - v2f{(f32)(valueHeaderStr.size()+1)*letterSize.x + 2, -2}, valueHeaderStr, V4f(1,1,1,shopUiAlpha)});
			char const *upgradeStr = "Апгрейд";
			labels.push_back({buyMenuPos - v2f{0, 26} + 2, upgradeStr, V4f(1, 1, 1, shopUiAlpha)});
			for (u32 i = 0; i < upgrades.size(); ++i) {
				auto &upgrade = upgrades[i];
				v2f pos = buyMenuPos + v2f{0, (f32)(i * 26)};
				bool hovering;
				bool pressed = button(pos, {300, 26}, i, format("{}: ${}", upgrade.name, upgrade.cost), hovering);
				if (hovering) {
					labels.push_back({pos + v2f{312, 2}, upgrade.description, V4f(1,1,1,shopUiAlpha)});
				}
				auto currentValue = upgrade.currentValue(*this);
				labels.push_back({pos - v2f{(f32)(currentValue.size()+1)*letterSize.x + 2, -2}, std::move(currentValue), V4f(1,1,1,shopUiAlpha)});
				if (pressed) {
					if (upgrade.extraCheck(*this, purchaseFailReason)) {
						if (coinsInWallet >= upgrade.cost) {
							coinsInWallet -= upgrade.cost;
							upgrade.purchaseAction(*this, upgrade);
						} else {
							purchaseFailReason = "Нужно больше золота!";
							showPurchaseFailTime = 1;
						}
					} else {
						showPurchaseFailTime = 1;
					} 
				}
			}
			if (showPurchaseFailTime > 0) {
				labels.push_back({buyMenuPos + v2f{0, -32}, purchaseFailReason, V4f(1,1,1,shopUiAlpha)});
			}
		}
		
		if (pauseAlpha > 0) {
			v4f menuColor = V4f(V3f(0.5f), pauseAlpha * 0.5f);

			v2f menuPanelPos = (v2f)window.clientSize * 0.4f;
			v2f menuPanelSize = (v2f)V2s(letterSize.x * 10, letterSize.y) + V2f(16);
			
			auto button = [&](v2f pos, v2f size, char const *text) {
				v2f rectPos = pos;
				rectPos.y = window.clientSize.y - rectPos.y - size.y;
				bool c = contains(rectPos, size, (v2f)mousePos);
				rect(pos, size, c ? V4f(1, 1, 1, menuColor.w) : menuColor);
				labels.push_back({pos + 4, text, V4f(1, 1, 1, pauseAlpha)});
				return c && input.mouseUp(0);
			};

			rect(menuPanelPos, menuPanelSize, menuColor);
			if (button(menuPanelPos + 4, (v2f)V2s(letterSize.x * 10, letterSize.y) + V2f(8), "Продолжить")) {
				if (currentState == State::pause) {
					setCursorVisibility(false);
					currentState = State::game;
				}
			}
		}

		if (uiAlpha > 0) {
			auto part = [&](s32 offset, v2f uv0, v2f uv1, f32 uvMix, f32 uvScale, f32 scale, f32 rotation, char const *fmt, auto param) {
				v2f pos = {
					(f32)(window.clientSize.x / 2 + offset),
					(f32)(window.clientSize.y - 32 - letterSize.y) 
				};
				labels.push_back({pos, format(fmt, param), V4f(1,1,1,uiAlpha)});

				pos.y += 8 + (letterSize.y - 32) * 0.5f;
				pos.y = window.clientSize.y - pos.y;

				pos.x -= 16;
				pushTile(uiTiles, pos, uv0, uv1, uvMix, ATLAS_ENTRY_SIZE * uvScale, V2f(32 * scale), rotation, V4f(1,1,1,uiAlpha));
			};
			auto ultimateUvs = getFrameUvs(scaledTime, 8, 8, {0, 3}, 1);

			part(-200, offsetAtlasTile(1, 7), {}, 0, 1, 1,    0,               "{}%", playerHealth);
			part(-100, ultimateUvs.uv0, ultimateUvs.uv1, ultimateUvs.uvMix, 1, 1.5f, scaledTime * pi, "{}%", ultimateAttackPercent);
			part(   0, offsetAtlasTile(4, 4), {}, 0, 1, 1,    -pi / 6,         "{}",   coinsInWallet);
			part( 100, offsetAtlasTile(5, 7), {}, 0, 1, 1,    0,               "{}",   botsKilled);
			part( 200, offsetAtlasTile(6, 7), {}, 0, 1, 1,    0,               "{}",   currentWave);
			if (showNoobText) {
				auto noob = [&](v2f offset, char const *string) {
					v2f pos = {
						(f32)((window.clientSize.x - strlen(string) * letterSize.x) / 2 + offset.x),
						(f32)(window.clientSize.y - 90 + offset.y * letterSize.y)
					};
					labels.push_back({pos, string, V4f(1,1,1,uiAlpha)});
				};
				noob({-200, 0}, "ХП");
				noob({-100,-1}, "Ультимейт(ПКМ)");
				noob({   0, 0}, "Монеты");
				noob({ 100,-1}, "Киллы");
				noob({ 200, 0}, "Волна");
			}
		}

		if (menuAlpha > 0) {
			labels.push_back({(v2f)window.clientSize * 0.4f, "Нажми 'Enter' чтобы начать", V4f(1,1,1,menuAlpha)});
		}

		if (deathScreenAlpha > 0) {
			labels.push_back({(v2f)window.clientSize * 0.4f, format("Киллы: {}\nНажми 'Enter' для рестарта", botsKilled), V4f(1,1,1,deathScreenAlpha)});
		}
		
		f32 volumeBarOpacity = max(menuAlpha, pauseAlpha);

		if (volumeBarOpacity > 0) {
			pushTile(uiTiles, (v2f)V2u(32, window.clientSize.y - 32), offsetAtlasTile(7, 7), ATLAS_ENTRY_SIZE, V2f(32), 0, V4f(1,1,1,volumeBarOpacity));

			v2f volumeBarPos = {64, 24};
			v2f volumeBarSize = {128, 16}; 
			rect(volumeBarPos, volumeBarSize, V4f(V3f(0.5f), 0.5f * volumeBarOpacity));

			bool containsVolume = contains(v2f{volumeBarPos.x, (f32)window.clientSize.y-volumeBarPos.y-volumeBarSize.y}, volumeBarSize, (v2f)mousePos);
			rect({lerp(volumeBarPos.x, volumeBarPos.x+volumeBarSize.x-volumeBarSize.y, masterVolume), 24}, {volumeBarSize.y, volumeBarSize.y}, V4f(containsVolume ? V3f(1) : V3f(0.5f), 0.5f * volumeBarOpacity));

			static bool draggingVolume = false;
			if (containsVolume) {
				if (input.mouseDown(0)) {
					draggingVolume = true;
				}
			}
			if (input.mouseUp(0)) {
				draggingVolume = false;
			}
			if (draggingVolume) {
				masterVolume = clamp((f32)(mousePos.x - (volumeBarPos.x + volumeBarSize.y / 2)) / (volumeBarSize.x - volumeBarSize.y), 0, 1);
			}
		}
#if 0
		labels.push_back({V2f(8), R"(

 !"#$%&'()*+,-./
0123456789:;<=>?
@ABCDEFGHIJKLMNO
PQRSTUVWXYZ[\]^_
`abcdefghijklmno
pqrstuvwxyz{|}~


        Ё
        ё
АБВГДЕЖЗИЙКЛМНОП
РСТУФХЦЧШЩЪЫЬЭЮЯ
абвгдежзийклмноп
рстуфхцчшщъыьэюя)"});
#endif
		renderer.updateBuffer(tilesBuffer, solidTiles.data(), (u32)solidTiles.size() * sizeof(solidTiles[0]));
		renderer.setBlend(Blend::srcAlpha, Blend::invSrcAlpha, BlendOp::add);
		renderer.setMatrix(0, m4::translation(-1, -1, 0) * m4::scaling(2.0f / (v2f)window.clientSize, 1));
		renderer.bindBuffer(tilesBuffer, Stage::vs, 0);
		renderer.bindShader(solidShader);
		renderer.bindTexture(atlasAlbedo, Stage::ps, 0);
		renderer.draw((u32)solidTiles.size() * 6);
		
		drawText(renderer, labels, (v2f)window.clientSize);
		
		pushTile(uiTiles, (v2f)mousePos, offsetAtlasTile(6, 0), ATLAS_ENTRY_SIZE * 2, V2f(32.0f), 0, V4f(1,1,1,clamp(uiAlpha - pauseAlpha, 0, 1)));

		renderer.updateBuffer(tilesBuffer, uiTiles.data(), (u32)uiTiles.size() * sizeof(uiTiles[0]));
		renderer.setBlend(Blend::srcAlpha, Blend::invSrcAlpha, BlendOp::add);
		renderer.setMatrix(0, m4::translation(-1, -1, 0) * m4::scaling(2.0f / (v2f)window.clientSize, 1));
		renderer.bindBuffer(tilesBuffer, Stage::vs, 0);
		renderer.bindShader(basicTileShader);
		renderer.bindTexture(atlasAlbedo, Stage::ps, 0);
		renderer.draw((u32)uiTiles.size() * 6);

		soundMutex.lock();
		for (auto sound : shotSounds) {
			sound.volume /= shotSounds.size();
			playingSounds.push_back(sound);
		}
		for (auto sound : explosionSounds) {
			sound.volume /= explosionSounds.size();
			playingSounds.push_back(sound);
		}
		for (auto sound : coinSounds) {
			sound.volume /= coinSounds.size();
			playingSounds.push_back(sound);
		}
		for (auto sound = playingSounds.begin(); sound != playingSounds.end();) {
			if (!sound->looping && sound->cursor >= sound->buffer->sampleCount) {
				playingSounds.erase(sound);
				continue;
			}
			++sound;
		}
		if (playingMusic.cursor >= playingMusic.buffer->sampleCount) {
			playNextMusic();
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

			volume = lerp(volume, V2f(1.0f), sound.flatness);

			volume *= sound.volume * masterVolume;

			memcpy(sound.channelVolume, &volume, sizeof(sound.channelVolume));
		}
		soundMutex.unlock();
	}

	void fillSoundBuffer(Audio &audio, s16 *subsample, u32 sampleCount) {
		PROFILE_FUNCTION;
		
		memset(subsample, 0, sampleCount * sizeof(*subsample) * 2);
		soundMutex.lock();
#if 1
		for (auto &sound : playingSounds) {
			if (sound.buffer->data)
				addSoundSamples(audio.sampleRate, subsample, sampleCount, *sound.buffer, sound.cursor, sound.channelVolume, timeScale * sound.pitch, sound.looping, true);
		}
		if (playingMusic.buffer)
			addSoundSamples(audio.sampleRate, subsample, sampleCount, *playingMusic.buffer, playingMusic.cursor, masterVolume, 1, false, false);
#else 
		if (playingMusic.buffer) {
			ASSERT(playingMusic.buffer->numChannels == 2);
			ASSERT(playingMusic.buffer->sampleRate == audio.sampleRate);
			ASSERT(playingMusic.buffer->bitsPerSample == 16);
			static s16 *src = (s16 *)playingMusic.buffer->data;
			for (u32 i = 0; i < sampleCount; ++i) {
				for (u32 c = 0; c < 2; ++c) {
					subsample[i * 2 + c] = src[i * 2 + c] / 32;
				}
			}
			src += sampleCount * 2;
		}
#endif
		soundMutex.unlock();
	}
};
#include <map>
namespace GameApi {

char const *getName() { return "dunger"; }

static Profiler::Stats frameStats[256]{};
static Profiler::Stats *currentFrameStats = frameStats;
static ::Random random;
static u32 sortIndex = 0;
static bool showOnlyCurrentFrame = true;
static s16 audioGraph[1024];
static s16 *audioGraphCursor = audioGraph;
static std::mutex audioMutex;
void fillStartInfo(StartInfo &info) {
	info.windowTitle = "Dunger!";
}
void init(EngState &state) {
	Game *gameInstance = new Game;
	gameInstance->init();

	state.userData = gameInstance;
}
Game &getGame(EngState &state) { return *(Game *)state.userData; }
void start(EngState &state, Window &window, Renderer &renderer, Input &input, Time &time) {
	getGame(state).start(window, renderer, input, time);
}
void debugStart(EngState &, Window &window, Renderer &renderer, Input &input, Time &time, Profiler::Stats const &stats) {}
void debugReload(EngState &state) {
	getGame(state).debugReload();
}
void update(EngState &state, Window &window, Renderer &renderer, Input &input, Time &time) {
	getGame(state).update(window, renderer, input, time);
}
void debugUpdate(EngState &state, Window &window, Renderer &renderer, Input &input, Time &time, Profiler::Stats const &startStats, Profiler::Stats const &newFrameStats) {
	if (input.keyDown(Key_f3)) {
		state.forceReload = true;
	}
	auto &game = getGame(state);
	*currentFrameStats = newFrameStats;
	if (++currentFrameStats == std::end(frameStats)) {
		currentFrameStats = frameStats;
	}
	if (input.keyDown(Key_f1)) {
		game.debugProfile.mode = (u8)((game.debugProfile.mode + 1) % 4);
	}
	List<Label, TempAllocator> labels;
	List<Rect, TempAllocator> rects;
	v2s mp = input.mousePosition;
	if (game.debugProfile.mode == 1) {
		static f32 smoothDelta = time.delta;
		smoothDelta = lerp(smoothDelta, time.delta, 0.05f);
		
		labels.push_back({V2f(8), format(R"({}, {}, {} cores, L1: {}, L2: {}, L3: {}
delta: {} ms ({} FPS)
memory usage: {}
temp usage: {}
{} raycasts, {} ms total, {} volumes tested
draw calls: {})", 
				toString(cpuInfo.vendor), 
				cpuInfo.brand, 
				cpuInfo.logicalProcessorCount, 
				cvtBytes(cpuInfo.totalCacheSize(1)), 
				cvtBytes(cpuInfo.totalCacheSize(2)), 
				cvtBytes(cpuInfo.totalCacheSize(3)),
				smoothDelta * 1000, 1.0f / smoothDelta,
				cvtBytes(getMemoryUsage()),
				cvtBytes(getTempMemoryUsage()),
				game.lightAtlas.totalRaysCast, game.debugProfile.raycastMS, game.lightAtlas.totalVolumeChecks, renderer.getDrawCount())});
		
		StringBuilder<TempAllocator> builder;
		builder.append("debugValues:\n");
		for (auto &v : game.debugValues) {
			switch (v.type) {
				case Game::DebugValueType::u1 : builder.append(format("{}: {}\n", v.name, *(bool*)v.data)); break;
				case Game::DebugValueType::u8 : builder.append(format("{}: {}\n", v.name, *(u8  *)v.data)); break;
				case Game::DebugValueType::u16: builder.append(format("{}: {}\n", v.name, *(u16 *)v.data)); break;
				case Game::DebugValueType::u32: builder.append(format("{}: {}\n", v.name, *(u32 *)v.data)); break;
				case Game::DebugValueType::u64: builder.append(format("{}: {}\n", v.name, *(u64 *)v.data)); break;
				case Game::DebugValueType::s8 : builder.append(format("{}: {}\n", v.name, *(s8  *)v.data)); break;
				case Game::DebugValueType::s16: builder.append(format("{}: {}\n", v.name, *(s16 *)v.data)); break;
				case Game::DebugValueType::s32: builder.append(format("{}: {}\n", v.name, *(s32 *)v.data)); break;
				case Game::DebugValueType::s64: builder.append(format("{}: {}\n", v.name, *(s64 *)v.data)); break;
				default:
					INVALID_CODE_PATH();
			}
		}
		labels.push_back({v2f{1024, 8}, builder.get()});
#if 1
		audioMutex.lock();
		
		game.debugLines.clear();
		for (s32 i = 1; i < _countof(audioGraph); ++i) {
			s32 x0 = 15 + i;
			s32 x1 = 16 + i;
			s32 y0 = (250 + audioGraph[frac(i + (s32)(audioGraphCursor - audioGraph) - 1, _countof(audioGraph))] * 250 / max<s16>());
			s32 y1 = (250 + audioGraph[frac(i + (s32)(audioGraphCursor - audioGraph) - 0, _countof(audioGraph))] * 250 / max<s16>());
			game.pushDebugLine((v2f)v2s{x0,y0}, (v2f)v2s{x1,y1}, V4f(1));
		}
		audioMutex.unlock();

		renderer.updateBuffer(game.debugLineBuffer, game.debugLines.data(), (u32)game.debugLines.size() * sizeof(game.debugLines[0]));
		renderer.bindBuffer(game.debugLineBuffer, Stage::vs, 0);

		renderer.setTopology(Topology::LineList);
		renderer.setMatrix(0, m4::translation(-1, -1, 0) * m4::scaling(2.0f / (v2f)window.clientSize, 1));
		renderer.bindShader(game.lineShader);
		renderer.draw((u32)game.debugLines.size() * 2);
		renderer.setTopology(Topology::TriangleList);

#else 
		for (u32 i = 0; i < game.shotSound.sampleCount; ++i) {
			s32 x = 16 + i - mp.x * 16;
			s32 h = 250 + ((s16 *)game.shotSound.data)[i*game.shotSound.numChannels] * 250 / max<s16>();
			s32 y = window.clientSize.y - 16 - h;
			s32 w = 1;
			rects.push_back({{x, y}, {w, h}, V4f(1)});
		}
#endif
	}
	static bool (**comparers)(Profiler::Stat, Profiler::Stat) = [] {
		u32 const columnCount = 3;
		static bool (*result[columnCount * 2])(Profiler::Stat, Profiler::Stat);
		result[0] = [](Profiler::Stat a, Profiler::Stat b) { return a.name.compare(b.name) < 0; };
		result[2] = [](Profiler::Stat a, Profiler::Stat b) { return a.selfUs < b.selfUs; };
		result[4] = [](Profiler::Stat a, Profiler::Stat b) { return a.totalUs < b.totalUs; };

		result[1] = [](Profiler::Stat a, Profiler::Stat b) { return result[0](b, a); };
		result[3] = [](Profiler::Stat a, Profiler::Stat b) { return result[2](b, a); };
		result[5] = [](Profiler::Stat a, Profiler::Stat b) { return result[4](b, a); };

		return result;
	}();
	
	auto contains = [](v2s pos, v2s size, v2s mp) {
		return pos.x <= mp.x && mp.x < pos.x + size.x && pos.y <= mp.y && mp.y < pos.y + size.y;
	};
	auto button = [&](v2s pos, v2s size) {
		bool c = contains(pos, size, mp);
		rects.push_back({pos, size, V4f(c ? 0.75f : 0.5f)});
		return c && input.mouseUp(0);
	};
	auto displayProfile = [&](Profiler::Stats const &stats) {
		u32 threadCount = (u32)stats.entries.size();
		s32 tableWidth = letterSize.x * 12;
		s32 barWidth = 1000;
		s32 const barHeight = letterSize.y * 6;
		v2s threadInfoPos{8, (s32)window.clientSize.y - barHeight * (s32)threadCount - 8};
		s32 y = threadInfoPos.y;
		
		for (u32 i = 0; i < threadCount; ++i) {
			rects.push_back({threadInfoPos + v2s{tableWidth, barHeight * (s32)i}, {barWidth, barHeight}, (i & 1) ? V4f(0.5f) : V4f(0.45f)});
		}
		bool doStrips = true;
		s32 stripLen = 1000;
		while (1) {
			if ((u64)stripLen * 100 > stats.totalUs)
				break;
			stripLen *= 10;
		}
		for (s32 i = 0; doStrips; ++i) {
			s32 stripWidth = (s32)(stripLen * barWidth / stats.totalUs);
			s32 stripX = i * 2 * stripWidth;
			if (stripX >= barWidth) {
				break;
			}
			if (stripX + stripWidth > barWidth) {
				stripWidth = stripX + stripWidth - barWidth;
				doStrips = false;
			}
			rects.push_back({threadInfoPos + v2s{tableWidth + stripX, 0}, 
							{stripWidth, barHeight * (s32)threadCount}, V4f(V3f(i % 5 == 0 ? 0.4f : 0.3f), 0.25f)});
		}
		u32 worksDone = 0;
		StringBuilder<TempAllocator> builder;
		for (u32 i = 0; i < (u32)stats.entries.size(); ++i) {
			if (i == 0)
				builder.append("Main thread\n\n\n\n\n\n");
			else
				builder.append(format("Thread {}:\n\n\n\n\n\n", i - 1));
			//s32 x = threadInfoPos.x + tableWidth;
			for (auto &stat : stats.entries[i]) {
				random.seed = 0;
				u32 byteIndex = 0;
				for (auto c : stat.name) {
					((char *)&random.seed)[byteIndex] ^= c;
					byteIndex = (byteIndex + 1) & 3;
				}

				s32 x = threadInfoPos.x + tableWidth + (s32)((stat.startUs - stats.startUs) * (u32)barWidth / stats.totalUs);
				s32 w = max((s32)(stat.totalUs * (u32)barWidth / stats.totalUs), 3);
				v2s pos = {x + 1, y + 1 + letterSize.y / 2 * stat.depth};
				v2s size = {w - 2, letterSize.y / 2 - 2};
				//rects.push_back({pos, size, V4f(hsvToRgb(time.time / 6, 1, 1), 0.75f)});
				//rects.push_back({pos, size, V4f(unpack(hsvToRgb(F32x4(time.time), F32x4(1), F32x4(1)))[time.frameCount & 3], 0.75f)});
				rects.push_back({pos, size, V4f(hsvToRgb(map(random.f32(), -1, 1, 0, 1), 0.5f, 1), 0.75f)});
				if (contains(pos, size, mp)) {
					labels.push_back({(v2f)mp, format("{}, start: {}, duration: {}", stat.name.data(), cvtMicroseconds(stat.startUs - stats.startUs), cvtMicroseconds(stat.totalUs))});
				}
				//x += w;
			}
			y += barHeight;
			worksDone += (u32)stats.entries[i].size();
		}
		labels.push_back({(v2f)threadInfoPos, builder.get()});
	};
	auto displayInfo = [&](char const *title, Profiler::Stats const &stats) {
		auto printStats = [&](List<Profiler::Stat, TempAllocator> const &entries, u64 totalUs, char *buffer) {
			u32 offset = 0;
			for (auto const &stat : entries) {
				offset += sprintf(buffer + offset, "%35s: %4.1f%%, %10llu us, %4.1f%%, %10llu us\n",
									stat.name.data(), (f64)stat.selfUs / totalUs * 100, stat.selfUs,
									(f64)stat.totalUs / totalUs * 100, stat.totalUs);
			}
			return offset;
		};


		char debugLabel[1024 * 16];
		s32 offset = sprintf(debugLabel, "%35s:  Self:                 Total:\n", title);


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
		columnHeader(V2s(8) + letterSize * v2s{36, 0}, letterSize * v2s{23, 1}, 1);
		columnHeader(V2s(8) + letterSize * v2s{59, 0}, letterSize * v2s{21, 1}, 2);

		//[](Profiler::Entry a, Profiler::Entry b) { return a.selfCycles > b.selfCycles; }

		List<Profiler::Stat, TempAllocator> allEntries;
		u64 totalThreadUs = 0;
		for (auto const &th : stats.entries) {
			for(auto const &stat : th) {
				//if (stat.depth == 0)
					totalThreadUs += stat.selfUs;
				if (auto it = std::find_if(allEntries.begin(), allEntries.end(),
								   [&](Profiler::Stat n) { return n.name == stat.name; });
					it != allEntries.end()) {

					it->selfUs += stat.selfUs;
					it->totalUs += stat.totalUs;
				} else {
					allEntries.push_back(stat);
				}
			}
		}

		std::sort(allEntries.begin(), allEntries.end(),
					comparers[(sortIndex & 0xF) * 2 + (bool)(sortIndex & 0x10)]);

		//Log::print("{}", totalThreadUs);
		offset += printStats(allEntries, totalThreadUs, debugLabel + offset);
		labels.push_back({V2f(8), debugLabel});
	};
	if (game.debugProfile.mode == 2) {
		displayInfo("Update profile", newFrameStats);

		showOnlyCurrentFrame ^= input.keyDown(Key_tab);
		if (showOnlyCurrentFrame) {
			displayProfile(newFrameStats);
		} else {
			static bool wasHovering = false;
			bool hovering = false;
			for (s32 statIndex = 0; statIndex < _countof(frameStats); statIndex++) {
				u64 totalTime = 0;
				std::map<std::string, u64> list;
				for (auto const &th : frameStats[statIndex].entries) {
					for (auto const &stat : th) {
						list[stat.name] += stat.selfUs;
						totalTime += stat.selfUs;
					}
				}

				u64 pastDuration = 0;
				for (auto &[name, duration] : list) {
					random.seed = 0;
					u32 seedIndex = 0;
					for (auto c : name) {
						((char *)&random.seed)[seedIndex] ^= c;
						seedIndex = (seedIndex + 1) & 3;
					}

					s32 totalHeight = 500;
					s32 x = 16 + statIndex * 2;
					s32 y = (s32)window.clientSize.y - 16 - totalHeight + ((s32)pastDuration * totalHeight / (s32)totalTime);
					s32 w = 2;
					s32 h = (s32)duration * totalHeight / (s32)totalTime;
					v2s pos = {x, y};
					v2s size = {w, h};
					rects.push_back({pos, size, V4f(hsvToRgb(map(random.f32(), -1, 1, 0, 1), 0.5f, 1), 1)});
					if (contains(pos, size, mp)) {
						hovering = true;
						labels.push_back({(v2f)mp, name.data()});
					} 
					pastDuration += duration;
				}
			}
			if (hovering != wasHovering) {
				setCursorVisibility(hovering);
			}
			wasHovering = hovering;
		}
	}
	if (game.debugProfile.mode == 3) {
		displayInfo("Start profile", startStats);
		displayProfile(startStats);
	}
	game.drawRect(renderer, rects, (v2f)window.clientSize);
	game.drawText(renderer, labels, (v2f)window.clientSize);
}

void fillSoundBuffer(EngState &state, Audio &audio, s16 *subsample, u32 subsampleCount) {
	getGame(state).fillSoundBuffer(audio, subsample, subsampleCount);
	audioMutex.lock();
	while (subsampleCount) {
		u32 density = 64;
		s32 result = 0;
		s32 div = 0;
		while (subsampleCount && density--) {
			result += *subsample++ << 6;
			++div;
			--subsampleCount;
		}
		*audioGraphCursor = (s16)(result / div);
		if (++audioGraphCursor >= std::end(audioGraph)) {
			audioGraphCursor = audioGraph;
		}
	}
	audioMutex.unlock();
}

void shutdown(EngState &state) { delete state.userData; }
} // namespace GameApi
  //#include "../../src/game_default_interface.h"