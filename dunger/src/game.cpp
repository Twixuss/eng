#include "../../src/game.h"
#include <array>

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
		v2s tMin = max(roundInt(min(a, b) - (r + 0.5f)), V2s(0));
		v2s tMax = min(roundInt(max(a, b) + (r + 0.5f)) + 1, V2s(CHUNK_WIDTH));
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

#define MAX_TILES					 (1024 * 1024 * 16)
#define MAX_LIGHTS					 (1024 * 1024)

#define ATLAS_SIZE					 V2(2)
#define ATLAS_ENTRY_SIZE_PIX		 V2(64)
#define CROP_AMOUNT					 1

#define ATLAS_ENTRY_SIZE_CROPPED_PIX (ATLAS_ENTRY_SIZE_PIX - CROP_AMOUNT*2)
#define ATLAS_ENTRY_SIZE			 (1.0f / ATLAS_SIZE)
#define ATLAS_ENTRY_SIZE_CROPPED	 ((ATLAS_ENTRY_SIZE_CROPPED_PIX / ATLAS_ENTRY_SIZE_PIX) / ATLAS_SIZE)
#define ATLAS_CROP_OFFSET			 (CROP_AMOUNT / ATLAS_ENTRY_SIZE_PIX / ATLAS_SIZE)

v2 offsetAtlasTile(u32 x, u32 y) { return V2(v2u{x, y}) * ATLAS_ENTRY_SIZE + ATLAS_CROP_OFFSET; }

struct Bullet {
	v2 position;
	v2 velocity;
	f32 remainingLifetime = 5.0f;
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
	f32 f32() { return (s32() / 256) * (1.0f / 8388608.f); }
	f32x4 f32x4() { return F32x4(s32x4{s32(), s32(), s32(), s32()} / 256) * (1.0f / 8388608.f); }
	bool u1() { return u32() & 0x10; }
	s32 s32() { return seed = randomize(seed); }
	u32 u32() { return (::u32)s32(); }
};
struct Label {
	v2 p;
	std::string text;
};
bool raycastBullet(TileStorage& tiles, v2 a, v2 b, v2& point, v2& normal) {
	v2 tMin, tMax;
	minmax(a, b, tMin, tMax);
	v2s tMini = max(roundInt(tMin), V2s(0));
	v2s tMaxi = min(roundInt(tMax) + 1, V2s(CHUNK_WIDTH));
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
struct Game {
	v2 playerP = V2(CHUNK_WIDTH) * 0.5f;
	v2 playerV{};

	v2 cameraP = playerP;
	f32 targetCamZoom = 1.0f / 16;
	f32 camZoom = targetCamZoom;

	v2 pixelsInMeter;

	TileStorage tiles = genTiles();

	UnorderedList<Bullet> bullets;
	UnorderedList<Explosion> explosions;
	UnorderedList<Ember> embers;

	::Random random;

	RenderTargetId msRenderTarget, diffuseRt, lightMapRt;
	ShaderId tileShader, msaaShader, lightShader, mergeShader;
	TextureId atlas, fontTexture;
	BufferId tilesBuffer, lightsBuffer;

	std::vector<Tile> tilesToDraw;
	std::vector<Light> lightsToDraw;

	v2 screenToWorld(v2 p, v2 clientSize) { return cameraP + (p * 2 - clientSize) / (clientSize.y * camZoom); }
	void createExplosion(v2 position, v2 normal) {
		Explosion ex;
		ex.position = position;
		ex.maxLifeTime = ex.remainingLifeTime = 0.75f + random.f32() * 0.25f;
		ex.rotationOffset = random.f32();
		explosions.push_back(ex);

		Ember e;
		e.position = position;
		for (u32 i = 0; i < 16; ++i) {
			f32x4 rnd = random.f32x4();
			e.maxLifeTime = e.remainingLifeTime = 0.75f + rnd[0] * 0.25f;
			e.velocity = normalize(v2{rnd[1], rnd[2]} + normal) * (1.0f + rnd[3]);
			e.rotationOffset = random.f32();
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
		renderer.bindBuffer(Stage::vs, 0, tilesBuffer);
		renderer.bindTexture(Stage::ps, 0, fontTexture);
		renderer.draw(tilesToDraw.size() * 6, 0);
	}

	Game(Window& window, Renderer& renderer) {
		PROFILE_FUNCTION;
		tileShader = renderer.createShader(DATA "shaders/tile");
		lightShader = renderer.createShader(DATA "shaders/light");
		mergeShader = renderer.createShader(DATA "shaders/merge");

		tilesBuffer = renderer.createBuffer(0, sizeof(Tile), MAX_TILES);
		lightsBuffer = renderer.createBuffer(0, sizeof(Light), MAX_LIGHTS);

		atlas = renderer.createTexture(DATA "textures/atlas.png", Address::clamp, Filter::anisotropic);
		fontTexture = renderer.createTexture(DATA "textures/font.png", Address::clamp, Filter::point_point);
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
		lightMapRt = renderer.createRenderTarget(window.clientSize, Format::F_RGB16);

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
	void update(Window& window, Renderer& renderer, Input const& input, Time const& time) {
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

		playerP = movePlayer(tiles, time, playerP, playerV * time.delta * 5, 0.4f);
		cameraP = lerp(cameraP, playerP, time.delta * 10);

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
				createExplosion(hitPoint, hitNormal);
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
		}

		s32 shotsAtOnce = 1;
		if (input.mouseHeld(0)) {
			auto createBullet = [&](f32 offset) {
				Bullet b;
				v2s mousePos = input.mousePosition;
				mousePos.y = (s32)window.clientSize.y - mousePos.y;
				b.velocity = normalize(screenToWorld((v2)mousePos, (v2)window.clientSize) - playerP);
				b.velocity = normalize(b.velocity + cross(b.velocity) * offset * 0.5f);
				b.position = playerP;
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

		f32 const zoomFactor = 1.25f;
		if (auto wheel = input.mouseWheel; wheel > 0) {
			targetCamZoom *= zoomFactor;
		} else if (wheel < 0) {
			targetCamZoom /= zoomFactor;
		}
		camZoom = lerp(camZoom, targetCamZoom, time.delta * 15);
		renderer.setMatrix(0, m4::scaling(camZoom) * m4::scaling((f32)window.clientSize.y / window.clientSize.x, 1, 1) *
								  m4::translation(-cameraP, 0));

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
					createTile(V2(x, y), offsetAtlasTile(1, 0), ATLAS_ENTRY_SIZE_CROPPED, V2(1.0f), 0.0f, color));
			}
		}

		for (auto& b : bullets) {
			tilesToDraw.push_back(createTile(b.position, offsetAtlasTile(0, 1), ATLAS_ENTRY_SIZE_CROPPED));
			lightsToDraw.push_back(createLight({1, .3, .2}, b.position, 1, (v2)window.clientSize));
		}
		for (auto& e : embers) {
			f32 t = e.remainingLifeTime;
			tilesToDraw.push_back(createTile(e.position, offsetAtlasTile(1, 1), ATLAS_ENTRY_SIZE_CROPPED, V2(t),
											 (t + e.rotationOffset) * 10));
		}
		for (auto& e : explosions) {
			f32 t = e.remainingLifeTime;
			tilesToDraw.push_back(createTile(e.position, offsetAtlasTile(1, 1), ATLAS_ENTRY_SIZE_CROPPED, V2(t),
											 (t + e.rotationOffset) * 10));
			lightsToDraw.push_back(createLight(v3{1, .5, .1} * t * 5, e.position, 3, (v2)window.clientSize));
		}

		tilesToDraw.push_back(createTile(playerP, offsetAtlasTile(0, 0), ATLAS_ENTRY_SIZE_CROPPED));
		lightsToDraw.push_back(createLight(V3(1), playerP, 7, (v2)window.clientSize));

		renderer.updateBuffer(tilesBuffer, tilesToDraw.data(), tilesToDraw.size() * sizeof(tilesToDraw[0]));
		renderer.updateBuffer(lightsBuffer, lightsToDraw.data(), lightsToDraw.size() * sizeof(lightsToDraw[0]));

		renderer.bindTexture(Stage::ps, 0, atlas);
		renderer.clearRenderTarget(msRenderTarget, V4(.1));
		renderer.bindRenderTarget(msRenderTarget);
		renderer.bindBuffer(Stage::vs, 0, tilesBuffer);
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
		renderer.bindBuffer(Stage::vs, 0, lightsBuffer);
		renderer.setBlend(Blend::one, Blend::one, BlendOp::add);
		renderer.draw(lightsToDraw.size() * 6);

		renderer.bindRenderTarget({0});
		renderer.bindRenderTargetAsTexture(diffuseRt, Stage::ps, 0);
		renderer.bindRenderTargetAsTexture(lightMapRt, Stage::ps, 1);
		renderer.bindShader(mergeShader);
		renderer.disableBlend();
		renderer.draw(3);

		char debugLabel[256];
		sprintf(debugLabel, R"(delta: %.1fms
draw calls: %u
tile count: %u
light count: %u)",
				time.delta * 1000, renderer.drawCalls, (u32)tilesToDraw.size(), (u32)lightsToDraw.size());

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