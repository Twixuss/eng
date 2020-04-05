#include "../../src/game.h"
#include <array>
#include <queue>

#define CHUNK_WIDTH 32
using TileStorage = std::array<bool, CHUNK_WIDTH * CHUNK_WIDTH>;
void setTile(TileStorage& tiles, u32 x, u32 y, bool value) { tiles[x * CHUNK_WIDTH + y] = value; }
bool getTile(TileStorage const& tiles, u32 x, u32 y) { return tiles[x * CHUNK_WIDTH + y]; }
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
	return clamp(p, v2{}, V2(CHUNK_WIDTH - 1));
}
v2 movePlayer(TileStorage const& tiles, Time const& time, v2 from, v2 velocity, f32 r) {
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
				v2 tilef = V2(v2s{tx, ty});

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

#define ATLAS_SIZE		 V2(4)
#define ATLAS_ENTRY_SIZE (1.0f / ATLAS_SIZE)

v2 offsetAtlasTile(u32 x, u32 y) { return V2(v2u{x, y}) * ATLAS_ENTRY_SIZE; }

struct Bullet {
	v2 position;
	v2 velocity;
	f32 remainingLifetime = 5.0f;
	f32 rotation;
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

	v2 uv;
	v2 uvScale;

	f32 rotation;
	v3 padding;
};
struct Light {
	v3 color;
	f32 radius;
	v2 position;
};
struct Random {
	s32 seed = (TL::s32)PerfTimer::getCounter();
	s32 s32() { return seed = randomize(seed); }
	u32 u32() { return (::u32)s32(); }
	f32 f32() { return (s32() / 256) * (1.0f / 8388608.f); }
	bool u1() { return u32() & 0x10; }
	s32x4 s32x4() { return {s32(), s32(), s32(), s32()}; }
	s32x8 s32x8() { return {s32(), s32(), s32(), s32(), s32(), s32(), s32(), s32()}; }
	u32x4 u32x4() { return U32x4(s32x4()); }
	u32x8 u32x8() { return U32x8(s32x8()); }
	f32x4 f32x4() { return F32x4(s32x4() / 256) * (1.0f / 8388608.f); }
	f32x8 f32x8() { return F32x8(s32x8() / 256) * (1.0f / 8388608.f); }
};
struct Label {
	v2 p;
	std::string text;
};
bool raycastBullet(TileStorage& tiles, v2 a, v2 b, v2& point, v2& normal) {
	v2 tMin, tMax;
	minmax(a, b, tMin, tMax);
	v2s tMini = max(roundToInt(tMin), V2s(0));
	v2s tMaxi = min(roundToInt(tMax) + 1, V2s(CHUNK_WIDTH));
	for (s32 tx = tMini.x; tx < tMaxi.x; ++tx) {
		for (s32 ty = tMini.y; ty < tMaxi.y; ++ty) {
			ASSERT(tx < CHUNK_WIDTH);
			ASSERT(ty < CHUNK_WIDTH);
			if (!getTile(tiles, (u32)tx, (u32)ty))
				continue;
			if (raycastRect(a, b, V2(v2s{tx, ty}), V2(0.5f), point, normal)) {
				return true;
			}
		}
	}
	return false;
}
#define VOXEL_MAP_WIDTH 32
struct Game {
	v2 playerP = V2((CHUNK_WIDTH-1) * 0.5f);
	v2 playerV{};

	f32 const playerR = 0.4f;

	v2 cameraP = playerP;
	f32 targetCamZoom = 1.0f / 8;
	f32 camZoom = targetCamZoom;
	f32 shotTimer = 0.0f;
	f32 shotDelta = 0.1f;

	v2 pixelsInMeter;

	TileStorage tiles = genTiles();

	UnorderedList<Bullet> bullets;
	UnorderedList<Explosion> explosions;
	UnorderedList<Ember> embers;

	::Random random;

	RenderTargetId msRenderTarget, diffuseRt, lightMapRt;
	ShaderId tileShader, msaaShader, lightShader, mergeShader;
	TextureId atlas, fontTexture, voxelsTex;
	BufferId tilesBuffer, lightsBuffer;

	std::vector<Tile> tilesToDraw;
	std::vector<Light> lightsToDraw;

	v2 voxelMapCenter{};
	v2 oldVoxelMapCenter{};

	v2 screenToWorld(v2 p, v2 clientSize) { return cameraP + (p * 2 - clientSize) / (clientSize.y * camZoom); }
	void createExplosion(v2 position, v2 incoming, v2 normal) {
		Explosion ex;
		ex.position = position;
		ex.maxLifeTime = ex.remainingLifeTime = 0.75f + random.f32() * 0.25f;
		ex.rotationOffset = random.f32();
		explosions.push_back(ex);

		v2 nin = normalize(incoming);
		f32 range = -dot(nin, normal);
		v2 rin = reflect(nin, normal);
		f32 rinAngle = atan2f(rin.x, rin.y);

		Ember e;
		e.position = position;
		for (u32 i = 0; i < 16; ++i) {
			f32x4 rnd = random.f32x4();
			e.maxLifeTime = e.remainingLifeTime = 0.75f + rnd[0] * 0.25f;
			f32 angle = rinAngle + rnd[1] * (pi * 0.5f) * range;
			sincos(angle, e.velocity.x, e.velocity.y);
			e.velocity *= (1.0f + rnd[2]) * 4.0f;
			e.rotationOffset = rnd[3];
			embers.push_back(e);
		}
	}
	Tile createTile(v2 position, v2 uv, v2 uvScale, v2 size = V2(1.0f), f32 rotation = 0.0f, v4 color = V4(1)) {
		Tile t;
		t.position = position;
		t.uv = uv;
		t.uvScale = uvScale;
		t.rotation = rotation;
		t.size = size;
		t.color = color;
		return t;
	}
	Light createLight(v3 color, v2 position, f32 radius, v2 clientSize) {
		Light l;
		l.color = color;
		l.radius = camZoom * radius * 2;
		l.position = (position - cameraP) * camZoom;
		l.position.x *= clientSize.y / clientSize.x;
		return l;
	}

	void drawText(Renderer& renderer, std::vector<Label> const& labels, v2 clientSize) {
		v2 textScale = V2(16.0f);
		textScale.x *= 3.0f / 4.0f;

		tilesToDraw.clear();
		for (auto& l : labels) {
			v2 p = l.p / textScale;
			p.y *= -1;
			f32 row = 0.0f;
			f32 column = 0.0f;
			for (unsigned char c : l.text) {
				if (c == '\n') {
					row -= 1.0f;
					column = 0.0f;
					continue;
				}
				float u = (c % 16) * 0.0625f;
				float v = (c / 16) * 0.0625f;
				tilesToDraw.push_back(createTile(p + v2{column + 0.5f, row - 0.5f}, {u, v}, V2(1.0f / 16.0f)));
				column += 1.0f;
			}
		}
		renderer.updateBuffer(tilesBuffer, tilesToDraw.data(), (u32)tilesToDraw.size() * sizeof(tilesToDraw[0]));

		renderer.setMatrix(0, m4::translation(-1, 1, 0) * m4::scaling(2 * textScale / clientSize, 0));

		renderer.setBlend(Blend::srcAlpha, Blend::invSrcAlpha, BlendOp::add);
		renderer.bindShader(tileShader);
		renderer.bindBuffer(tilesBuffer, Stage::vs, 0);
		renderer.bindTexture(fontTexture, Stage::ps, 0);
		renderer.draw(tilesToDraw.size() * 6, 0);
	}

	v3 voxels[VOXEL_MAP_WIDTH][VOXEL_MAP_WIDTH]{};

	Game(Window& window, Renderer& renderer) {
		PROFILE_FUNCTION;
		tileShader = renderer.createShader(DATA "shaders/tile");
		lightShader = renderer.createShader(DATA "shaders/light");
		mergeShader = renderer.createShader(DATA "shaders/merge");

		tilesBuffer = renderer.createBuffer(0, sizeof(Tile), MAX_TILES);
		lightsBuffer = renderer.createBuffer(0, sizeof(Light), MAX_LIGHTS);

		atlas = renderer.createTexture(DATA "textures/atlas.png", Address::clamp, Filter::point_point);
		fontTexture = renderer.createTexture(DATA "textures/font.png", Address::clamp, Filter::point_point);
		voxelsTex = renderer.createTexture(Format::F_RGB32, VOXEL_MAP_WIDTH, VOXEL_MAP_WIDTH, Address::clamp,
										   Filter::linear_linear);
	}
	void resize(Window& window, Renderer& renderer) {
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
	void update(Window& window, Renderer& renderer, Input const& input, Time& time) {
		time.targetFrameTime = 0.0f;
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

		v2 move{};
		move.x += input.keyHeld('D') - input.keyHeld('A');
		move.y += input.keyHeld('W') - input.keyHeld('S');

		v2 targetPlayerV = normalize(move, {});

		playerV = moveTowards(playerV, targetPlayerV, time.delta * 10);

		playerP = movePlayer(tiles, time, playerP, playerV * time.delta * 5, playerR);
		cameraP = lerp(cameraP, playerP, min(time.delta * 10, 1));

		for (u32 i = 0; i < bullets.size(); ++i) {
			auto& b = bullets[i];
			b.remainingLifetime -= time.delta;
			if (b.remainingLifetime <= 0) {
				erase(bullets, b);
				--i;
				continue;
			}
			v2 nextPos = b.position + b.velocity * time.delta * 10;
			v2 hitPoint, hitNormal;
			if (raycastBullet(tiles, b.position, nextPos, hitPoint, hitNormal)) {
				createExplosion(hitPoint, normalize(b.velocity), hitNormal);
				erase(bullets, b);
				--i;
				continue;
			}
			b.position = nextPos;
		}
		for (u32 i = 0; i < explosions.size(); ++i) {
			auto& e = explosions[i];
			e.remainingLifeTime -= time.delta;
			if (e.remainingLifeTime <= 0) {
				erase(explosions, e);
				--i;
				continue;
			}
		}
		for (u32 i = 0; i < embers.size(); ++i) {
			auto& e = embers[i];
			e.remainingLifeTime -= time.delta;
			if (e.remainingLifeTime <= 0) {
				erase(embers, e);
				--i;
				continue;
			}
			e.position += e.velocity * time.delta;
			e.velocity = moveTowards(e.velocity, {}, time.delta * 5);
		}

		s32 shotsAtOnce = 1;
		if (shotTimer > 0) {
			shotTimer -= time.delta;
		} else {
			if (input.mouseHeld(0)) {
				shotTimer += shotDelta;
				auto createBullet = [&](f32 offset) {
					Bullet b;
					v2s mousePos = input.mousePosition;
					mousePos.y = (s32)window.clientSize.y - mousePos.y;
					b.velocity = normalize(screenToWorld((v2)mousePos, (v2)window.clientSize) - playerP);
					b.velocity = normalize(b.velocity + cross(b.velocity) * offset * 0.5f);
					b.position = playerP;
					v2 sc = normalize(b.velocity);
					b.rotation = atan2f(sc.x, sc.y);
					bullets.push_back(b);
				};
				if (shotsAtOnce == 1) {
					createBullet(0);
				} else {
					if (shotsAtOnce & 1) {
						for (s32 i = -shotsAtOnce / 2; i <= shotsAtOnce / 2; ++i) {
							createBullet(f32(i) / shotsAtOnce);
						}
					} else {
						for (s32 i = 0; i < shotsAtOnce; ++i) {
							createBullet(f32(i - shotsAtOnce * 0.5f + 0.5f) / shotsAtOnce);
						}
					}
				}
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
		for (int x = 0; x < CHUNK_WIDTH; ++x) {
			for (int y = 0; y < CHUNK_WIDTH; ++y) {
				v4 color = V4(1);
				if (!getTile(tiles, x, y)) {
					color.xyz *= 0.25f;
				}
				tilesToDraw.push_back(
					createTile(V2(x, y), offsetAtlasTile(1, 0), ATLAS_ENTRY_SIZE, V2(1.0f), 0.0f, color));
			}
		}

		for (auto& b : bullets) {
			u32 frame = (u32)(b.remainingLifetime * 12) % 4;
			v2u framePos{frame % 2, frame / 2};
			tilesToDraw.push_back(createTile(b.position, offsetAtlasTile(0, 1) + (v2)framePos * ATLAS_ENTRY_SIZE * 0.5f,
											 ATLAS_ENTRY_SIZE * 0.5f, V2(0.5f), b.rotation));
			lightsToDraw.push_back(createLight({.2, .3, 1}, b.position, 1, (v2)window.clientSize));
		}
		auto getExplosionAtlasPos = [](f32 t) {
			u32 frame = clamp((u32)(t * 8), 0u, 7u);
			v2u framePos{frame % 2, frame / 2};
			return offsetAtlasTile(2, 0) + (v2)framePos * ATLAS_ENTRY_SIZE;
		};
		for (auto& e : embers) {
			f32 t = e.remainingLifeTime / e.maxLifeTime;
			u32 frame = clamp((u32)((1 - t) * 8), 0u, 7u);
			v2u framePos{frame % 2, frame / 2};
			tilesToDraw.push_back(
				createTile(e.position, offsetAtlasTile(1, 1) + (v2)framePos * ATLAS_ENTRY_SIZE * 0.25f,
						   ATLAS_ENTRY_SIZE * 0.25f, V2(map(t, 0, 1, 0.1f, 0.25f)), (t + e.rotationOffset) * 10));
		}
		for (auto& e : explosions) {
			f32 t = e.remainingLifeTime / e.maxLifeTime;
			tilesToDraw.push_back(createTile(e.position, getExplosionAtlasPos(1 - t), ATLAS_ENTRY_SIZE,
											 V2(map(t, 0, 1, 0.75f, 1)), (t + e.rotationOffset) * 2));
			// lightsToDraw.push_back(createLight(v3{1, .5, .1} * t * 5, e.position, 3, (v2)window.clientSize));
		}

		tilesToDraw.push_back(createTile(playerP, offsetAtlasTile(0, 0), ATLAS_ENTRY_SIZE));
		// lightsToDraw.push_back(createLight(V3(1), playerP, 7, (v2)window.clientSize));

		renderer.updateBuffer(tilesBuffer, tilesToDraw.data(), (u32)tilesToDraw.size() * sizeof(tilesToDraw[0]));
		renderer.updateBuffer(lightsBuffer, lightsToDraw.data(), (u32)lightsToDraw.size() * sizeof(lightsToDraw[0]));

		voxelMapCenter = floor(playerP + 1.0f);
		if (voxelMapCenter != oldVoxelMapCenter) {
			v2s const d = (v2s)(voxelMapCenter - oldVoxelMapCenter);
			v2s const ad = abs(d);
			if (d.x > 0) {
				for (s32 voxelY = 0; voxelY < VOXEL_MAP_WIDTH; ++voxelY) {
					for (s32 voxelX = 0; voxelX < VOXEL_MAP_WIDTH - ad.x; ++voxelX) {
						voxels[voxelY][voxelX] = voxels[voxelY][voxelX + ad.x];
					}
					for (s32 voxelX = VOXEL_MAP_WIDTH - ad.x; voxelX < VOXEL_MAP_WIDTH; ++voxelX) {
						voxels[voxelY][voxelX] = {};
					}
				}
			} else if (d.x < 0) {
				for (s32 voxelY = 0; voxelY < VOXEL_MAP_WIDTH; ++voxelY) {
					for (s32 voxelX = VOXEL_MAP_WIDTH - 1; voxelX >= ad.x; --voxelX) {
						voxels[voxelY][voxelX] = voxels[voxelY][voxelX - ad.x];
					}
					for (s32 voxelX = 0; voxelX < ad.x; ++voxelX) {
						voxels[voxelY][voxelX] = {};
					}
				}
			}
			if (d.y > 0) {
				for (s32 voxelY = 0; voxelY < VOXEL_MAP_WIDTH - ad.y; ++voxelY) {
					memcpy(voxels[voxelY], voxels[voxelY + ad.y], VOXEL_MAP_WIDTH * sizeof(voxels[0][0]));
				}
				for (s32 voxelY = VOXEL_MAP_WIDTH - ad.y; voxelY < VOXEL_MAP_WIDTH; ++voxelY) {
					memset(voxels[voxelY], 0, VOXEL_MAP_WIDTH * sizeof(voxels[0][0]));
				}
			} else if (d.y < 0) {
				for (s32 voxelY = VOXEL_MAP_WIDTH - 1; voxelY >= ad.y; --voxelY) {
					memcpy(voxels[voxelY], voxels[voxelY - ad.y], VOXEL_MAP_WIDTH * sizeof(voxels[0][0]));
				}
				for (s32 voxelY = 0; voxelY < ad.y; ++voxelY) {
					memset(voxels[voxelY], 0, VOXEL_MAP_WIDTH * sizeof(voxels[0][0]));
				}
			}
		}
		oldVoxelMapCenter = voxelMapCenter;

		u32 const lightSampleCount = 64;
		u32 const lightSampleRandomCount = 16;
		static auto samplingCircle = [] {
			std::array<v2, lightSampleCount * lightSampleRandomCount> r;
			for (u32 i = 0; i < r.size(); ++i) {
				f32 angle = (f32)(i * 2) / r.size() * pi;
				sincos(angle, r[i].x, r[i].y);
			}
			return r;
		}();

		u32 const raycastThreads = 4;

		struct RaycastWorkEntry {
			Game* game;
			Time const* time;
			u32 voxelBeginY;
			u32 voxelEndY;
		};

		RaycastWorkEntry raycastWork[raycastThreads];

		struct VoxTestTile {
			v2 pos;
			f32 size;
			v3 color;
		};
		struct VoxTestTilex8 {
			v2x8 pos;
			f32x8 size;
			v3x8 color;
		};
		static std::vector<VoxTestTile> allRaycastTargets;
		allRaycastTargets.clear();
		for (s32 targetX = 0; targetX < CHUNK_WIDTH; ++targetX) {
			for (s32 targetY = 0; targetY < CHUNK_WIDTH; ++targetY) {
				if (getTile(tiles, targetX, targetY)) {
					allRaycastTargets.push_back({V2(targetX, targetY), 0.5f, 0.0f});
				}
			}
		}
		for (auto& e : explosions) {
			f32 t = e.remainingLifeTime / e.maxLifeTime;
			allRaycastTargets.push_back({e.position, max(0.0f, t - 0.9f) * 10.0f, v3{1, .5, .1} * 10.0f});
		}
		//for (auto& e : embers) {
		//	f32 t = e.remainingLifeTime / e.maxLifeTime;
		//	allRaycastTargets.push_back({e.position, max(0.0f, t - 0.9f) * 5.0f, v3{1, .5, .1} * 10.0f});
		//}
		for (auto& b : bullets) {
			allRaycastTargets.push_back({b.position, 0.25f, v3{.2, .3, 1} * 0.5f});
		}
		allRaycastTargets.push_back({playerP, 0.4f, V3(1.0f)});

		auto cast = [](void* param) {
			RaycastWorkEntry* work = (RaycastWorkEntry*)param;
			Game* game = work->game;
			Time const* time = work->time;
			u32 voxelBeginY = work->voxelBeginY;
			u32 voxelEndY = work->voxelEndY;
			for (u32 voxelY = voxelBeginY; voxelY < voxelEndY; ++voxelY) {
				for (u32 voxelX = 0; voxelX < VOXEL_MAP_WIDTH; ++voxelX) {
					bool skip = (voxelY & 1) ^ (voxelX & 1);
					if (time->frameCount & 1)
						skip = !skip;
					if (skip)
						continue;
					game->random.seed >>= 1;
#if 1
					v3 vox{};
					v2 rayBegin = V2(voxelX, voxelY) + game->voxelMapCenter - VOXEL_MAP_WIDTH * 0.5f;
					v2x8 rayBeginx8 = V2x8(rayBegin);
					for (u32 lightSampleIndex = 0; lightSampleIndex < lightSampleCount; ++lightSampleIndex) {
						u32 rnd = game->random.u32();
						// if(i==0)
						//	printf("%u\n", rnd);
						v2 dir = samplingCircle[lightSampleIndex * lightSampleRandomCount + rnd % lightSampleRandomCount];
						v2 rayEnd = rayBegin + dir * (VOXEL_MAP_WIDTH / 2);
						v2x8 rayEndx8 = V2x8(rayEnd);

						v2 tMin, tMax;
						minmax(rayBegin, rayEnd, tMin, tMax);

						union {
							VoxTestTile tilesToTest[1024];
							VoxTestTilex8 tilesToTestx8[_countof(tilesToTest) / 8];
						};
						u32 tileCount = 0;
						VoxTestTile* tileToTest = tilesToTest;
						for (auto const& tile : allRaycastTargets) {
							if (tMin.x - tile.size < tile.pos.x && tile.pos.x < tMax.x + tile.size &&
								tMin.y - tile.size < tile.pos.y && tile.pos.y < tMax.y + tile.size) {
								*tileToTest++ = tile;
								++tileCount;
							}
						}
						u32 tileCountx8 = ceil(tileCount, 8) / 8;
						for(u32 i = tileCount; i < tileCountx8 * 8; ++i) {
							tilesToTest[i] = {};
						}
						for (u32 j = 0; j < tileCountx8; ++j) {
							VoxTestTilex8 untx8 = tilesToTestx8[j];
							VoxTestTile *unt = (VoxTestTile*)&untx8;
							tilesToTestx8[j].pos = V2x8(unt[0].pos, unt[1].pos,
														unt[2].pos, unt[3].pos,
														unt[4].pos, unt[5].pos,
														unt[6].pos, unt[7].pos);
							tilesToTestx8[j].size = F32x8(unt[0].size, unt[1].size,
														  unt[2].size, unt[3].size,
														  unt[4].size, unt[5].size,
														  unt[6].size, unt[7].size);
							tilesToTestx8[j].color = V3x8(unt[0].color, unt[1].color,
														  unt[2].color, unt[3].color,
														  unt[4].color, unt[5].color,
														  unt[6].color, unt[7].color);
						}
						f32x8 closestDistancex8 = F32x8(INFINITY);
						v3x8 hitColorx8{};
						v2x8 point, normal;
						for (u32 j = 0; j < tileCountx8; ++j) {
							VoxTestTilex8* tile = tilesToTestx8 + j;
							auto raycastMask = raycastRect(rayBeginx8, rayEndx8, tile->pos, V2x8(tile->size), point, normal);

							f32x8 dist = distanceSqr(rayBeginx8, point);
							auto mask = raycastMask && dist < closestDistancex8;
							closestDistancex8 = select(mask, dist, closestDistancex8);
							hitColorx8 = select(mask, tile->color, hitColorx8);
						}
						hitColorx8 = unpack(hitColorx8) * 10;
						f32 closestDistance = INFINITY;
						v3 hitColor{};
						for(u32 i=0;i<8;++i) {
							if(closestDistancex8[i] < closestDistance){
								closestDistance = closestDistancex8[i];
								hitColor = ((v3*)&hitColorx8)[i];
							}
						}
						vox += hitColor; //(10000.0f - pow2(distanceSqr(rayBegin, point))) * 0.001f * hitColor;
					}
					game->voxels[voxelY][voxelX] = lerp(game->voxels[voxelY][voxelX], vox / lightSampleCount,
														min(time->delta * 6, 1));
#else
					v3 vox{};
					v2 rayBegin = V2(voxelX, voxelY) + game->voxelMapCenter - VOXEL_MAP_WIDTH * 0.5f;
					for (u32 i = 0; i < lightSampleCount; ++i) {
						u32 rnd = game->random.u32();
						// if(i==0)
						//	printf("%u\n", rnd);
						v2 dir = samplingCircle[i * lightSampleRandomCount + rnd % lightSampleRandomCount];
						v2 rayEnd = rayBegin + dir * (VOXEL_MAP_WIDTH / 2);

						v2 tMin, tMax;
						minmax(rayBegin, rayEnd, tMin, tMax);

						f32 closestDistance = INFINITY;
						v2 point, normal;
						v3 hitColor{};

						VoxTestTile tilesToTest[64];
						VoxTestTile* tileToTest = tilesToTest;
						for (auto const& tile : allRaycastTargets) {
							if (tMin.x - tile.size < tile.pos.x && tile.pos.x < tMax.x + tile.size &&
								tMin.y - tile.size < tile.pos.y && tile.pos.y < tMax.y + tile.size) {
								*tileToTest++ = tile;
							}
						}
						for (VoxTestTile* tile = tilesToTest; tile < tileToTest; ++tile) {
							if (raycastRect(rayBegin, rayEnd, tile->pos, V2(tile->size), point, normal)) {
								f32 dist = distanceSqr(rayBegin, point);
								if (dist < closestDistance) {
									closestDistance = dist;
									hitColor = tile->color;
								}
							}
						}
						vox += hitColor * 10; //(10000.0f - pow2(distanceSqr(rayBegin, point))) * 0.001f * hitColor;
					}
					game->voxels[voxelY][voxelX] = lerp(game->voxels[voxelY][voxelX], vox / lightSampleCount,
														min(time->delta * 6, 1));
#endif
				}
			}
		};

		raycastWork[0] = {this, &time, 0, VOXEL_MAP_WIDTH / raycastThreads};
		raycastWork[1] = {this, &time, raycastWork[0].voxelEndY, VOXEL_MAP_WIDTH / raycastThreads * 2};
		raycastWork[2] = {this, &time, raycastWork[1].voxelEndY, VOXEL_MAP_WIDTH / raycastThreads * 3};
		raycastWork[3] = {this, &time, raycastWork[2].voxelEndY, VOXEL_MAP_WIDTH / raycastThreads * 4};

		auto raycastBegin = PerfTimer::getCounter();
		pushWork(cast, raycastWork + 0);
		pushWork(cast, raycastWork + 1);
		pushWork(cast, raycastWork + 2);
		cast(raycastWork + 3);

		waitForWorkCompletion();
		auto raycastTime = PerfTimer::getMilliseconds(raycastBegin, PerfTimer::getCounter());

		renderer.updateTexture(voxelsTex, voxels);

		renderer.setMatrix(0, m4::scaling(camZoom) * m4::scaling((f32)window.clientSize.y / window.clientSize.x, 1, 1) *
								  m4::translation(-cameraP, 0));
		renderer.bindTexture(atlas, Stage::ps, 0);
		renderer.clearRenderTarget(msRenderTarget, V4(.1));
		renderer.bindRenderTarget(msRenderTarget);
		renderer.bindBuffer(tilesBuffer, Stage::vs, 0);
		renderer.bindShader(tileShader);
		renderer.setBlend(Blend::srcAlpha, Blend::invSrcAlpha, BlendOp::add);
		renderer.draw(tilesToDraw.size() * 6);

		renderer.bindRenderTarget(diffuseRt);
		renderer.bindRenderTargetAsTexture(msRenderTarget, Stage::ps, 0);
		renderer.bindShader(msaaShader);
		renderer.disableBlend();
		renderer.draw(3);

		renderer.bindRenderTarget(lightMapRt);
		renderer.clearRenderTarget(lightMapRt, v4{});
		renderer.bindShader(lightShader);
		renderer.bindBuffer(lightsBuffer, Stage::vs, 0);
		renderer.setBlend(Blend::one, Blend::one, BlendOp::add);
		renderer.draw(lightsToDraw.size() * 6);

		v2 voxScale = v2{(f32)window.clientSize.x / window.clientSize.y, 1};
		renderer.setMatrix(0, m4::translation((cameraP - voxelMapCenter) / VOXEL_MAP_WIDTH * 2, 0) *
								  m4::translation(V2(1.0f / VOXEL_MAP_WIDTH), 0) *
								  m4::scaling(voxScale / VOXEL_MAP_WIDTH / camZoom * 2, 1));
		renderer.bindRenderTarget({0});
		renderer.bindRenderTargetAsTexture(diffuseRt, Stage::ps, 0);
		renderer.bindRenderTargetAsTexture(lightMapRt, Stage::ps, 1);
		renderer.bindTexture(voxelsTex, Stage::ps, 2);
		renderer.bindShader(mergeShader);
		renderer.disableBlend();
		renderer.draw(3);

		char debugLabel[256];
		static f32 smoothDelta = time.delta, smoothRaycastTime = raycastTime;
		sprintf(debugLabel, R"(delta: %.1f (%.1f) ms
raycast: %.1f (%.1f) ms
draw calls: %u
tile count: %u)",
				smoothDelta * 1000, time.delta * 1000, 
				smoothRaycastTime, raycastTime,
				renderer.drawCalls, (u32)tilesToDraw.size());
		smoothDelta = lerp(smoothDelta, time.delta, 0.05f);
		smoothRaycastTime = lerp(smoothRaycastTime, raycastTime, 0.05f);

		std::vector<Label> labels;
		labels.push_back({V2(8), debugLabel});
		drawText(renderer, labels, (v2)window.clientSize);
	}
	~Game() {}
};

namespace GameAPI {
void fillStartupInfo(StartupInfo& info) {}
} // namespace GameAPI
#include "../../src/game_default_interface.h"