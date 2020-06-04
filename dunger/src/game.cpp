#include "../../src/game.h"
#include <string>
#include <array>
#include <queue>
#include <random>
#include <unordered_map>

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
	
#if 0
struct MapGraph {
	struct NodeRef {
		u8 x, y;
	};
	struct Node {
		v2u position;
		UnorderedList<NodeRef> neighbors;
	};
	std::optional<Node> nodes[CHUNK_WIDTH][CHUNK_WIDTH];

	Node *deref(NodeRef ref) {
		return &nodes[ref.x][ref.y].value();
	}
	List<v2u> getPath(v2u a, v2u b) {
		if (a.x >= CHUNK_WIDTH || a.y >= CHUNK_WIDTH || b.x >= CHUNK_WIDTH || b.y >= CHUNK_WIDTH) {
			return {};
		}
		if (!nodes[a.x][a.y] || !nodes[b.x][b.y]) {
			return {};
		}
		Node *start = &*nodes[a.x][a.y];
		Node *goal = &*nodes[b.x][b.y];
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

		while (!frontier.empty()) {
			auto current = frontier.top();
			frontier.pop();

			if (current.node == goal)
				break;

			for (auto nextRef : current.node->neighbors) {
				auto next = deref(nextRef);
				f32 newCost = costSoFar[current.node] + distance((v2f)current.node->position, (v2f)next->position);
				if ((costSoFar.find(next) == costSoFar.end()) || (newCost < costSoFar[next])) {
					costSoFar[next] = newCost;
					frontier.push({next, newCost + distance((v2f)goal->position, (v2f)next->position)});
					cameFrom[next] = current.node;
				}
			}
		}
		auto current = goal;
		List<v2u> path;
		while (current != start) {
		   path.push_back(current->position);
		   current = cameFrom.at(current);
		}
		path.push_back(start->position);
		return path;
	}
};

MapGraph createGraph(TileStorage const &tiles) {
	MapGraph result;

	auto linkNodes = [](MapGraph::Node &a, MapGraph::Node &b) {
		a.neighbors.push_back({(u8)b.position.x, (u8)b.position.y});
		b.neighbors.push_back({(u8)a.position.x, (u8)a.position.y});
	};
	for(u32 x = 0; x < CHUNK_WIDTH; ++x) {
		for(u32 y = 0; y < CHUNK_WIDTH; ++y) {
			if (!getTile(tiles, x, y)) {
				auto &node = result.nodes[x][y].emplace();
				node.position = v2u{x, y};
				if (y != 0 && !getTile(tiles, x, y - 1)) {
					ASSERT(result.nodes[x][y - 1].has_value());
					linkNodes(node, *result.nodes[x][y - 1]);
				}
				if (x != 0 && !getTile(tiles, x - 1, y)) {
					ASSERT(result.nodes[x - 1][y].has_value());
					linkNodes(node, *result.nodes[x - 1][y]);
				}
				if (x != 0 && y != 0 && !getTile(tiles, x - 1, y - 1) && !getTile(tiles, x, y - 1) && !getTile(tiles, x - 1, y)) {
					ASSERT(result.nodes[x - 1][y - 1].has_value());
					linkNodes(node, *result.nodes[x - 1][y - 1]);
				}
				if (x != 0 && y != (CHUNK_WIDTH - 1) && !getTile(tiles, x - 1, y + 1) && !getTile(tiles, x, y + 1) && !getTile(tiles, x - 1, y)) {
					ASSERT(result.nodes[x - 1][y + 1].has_value());
					linkNodes(node, *result.nodes[x - 1][y + 1]);
				}
			} 
		}
	}
#if 0
	for(u32 x = 0; x < CHUNK_WIDTH; ++x) {
		for(u32 y = 0; y < CHUNK_WIDTH; ++y) {
			if (result.nodes[x][y].has_value()) {
				fputc('0' + result.nodes[x][y]->neighbors.size(), stdout);
			} else {
				fputc('.', stdout);
			}
		}
		fputc('\n', stdout);
	}
#endif
	return result;
}
#else
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
#endif
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
v2f movePlayer(TileStorage const &tiles, v2f from, v2f &velocity, f32 delta, f32 r) {
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
				velocity -= normal * dot(velocity, normal);
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
	f32 remainingLifeTime;
	f32 maxLifeTime;
	f32 rotationOffset;
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
	f32 radius;
	f32 fireTimer;
	u32 health;
};

struct Random {
	u32 seed;
	u32 u32() { return seed = randomize(seed); }
	s32 s32() { return (::s32)u32(); }
	bool u1() { return u32() & 0x10; }
	u32x4 u32x4() { return randomize(u32() ^ ::u32x4{0x0CB94D77, 0xA3579AD0, 0x207B3643, 0x97C0013D}); }
	u32x8 u32x8() {
		return randomize(u32() ^ ::u32x8{0x1800E669, 0x0635093E, 0x0CB94D77, 0xA3579AD0, 0x207B3643, 0x97C0013D,
										 0x3ACD6EE8, 0x85E0800F});
	}
	s32x4 s32x4() { return (::s32x4)u32x4(); }
	s32x8 s32x8() { return (::s32x8)u32x8(); }
	f32 f32() { return (u32() >> 8) * (1.0f / 0x1000000); }
	f32x4 f32x4() { return (::f32x4)(u32x4() >> 8) * (1.0f / 0x1000000); }
	f32x8 f32x8() { return (::f32x8)(u32x8() >> 8) * (1.0f / 0x1000000); }
	v2u v2u() { return {u32(), u32()}; }
	v2f v2f() { return {f32(), f32()}; }
	v4f v4f() { return V4f(f32x4()); }
	v3f v3f() { return V3f(f32(), f32(), f32()); }
	template <class T>
	T type() {
		return {};
	}
	template <>
	::s32x4 type<::s32x4>() {
		return s32x4();
	}
	template <>
	::s32x8 type<::s32x8>() {
		return s32x8();
	}
};
struct Label {
	v2f p;
	std::string text;
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
RaycastResult raycast(TileStorage const *tiles, View<Bot> bots, View<v2f> players, v2f a, v2f b, v2f &point,
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

v2f screenToWorld(v2f p, v2f clientSize, v2f cameraP, f32 cameraZoom) {
	return cameraP + (p * 2 - clientSize) / (clientSize.y * cameraZoom);
}

struct LightTile {
	v2f pos;
	f32 size;
	v3f color;
};
struct LightAtlas {
	static constexpr u32 simdElementCount = TL::simdElementCount<f32>;
	static constexpr u32 lightSampleCount = BUILD_DEBUG ? 16 : 256;
	static constexpr f32 accumulationRate = 20.0f;

	v2f center{};
	v2f oldCenter{};
	v3f *voxels{};
	v2f samplingCircle[lightSampleCount];
	::Random random;
	v2u size;
	
	v3f &getVoxel(u32 y, u32 x) {
		return voxels[y * size.x + x];
	}
	v3f *getRow(u32 y) {
		return voxels + y * size.x;
	}

	void resize(v2u newSize) {
		size = newSize;
		free(voxels);
		umm dataSize = sizeof(v3f) * size.x * size.y;
		voxels = (v3f *)malloc(dataSize);
		memset(voxels, 0, dataSize);
	}
	void init() {
		u32 samplingCircleSize = _countof(samplingCircle);
		for (u32 i = 0; i < samplingCircleSize; ++i) {
			f32 angle = (f32)i / samplingCircleSize * (pi * 2);
			sincos(angle, samplingCircle[i].x, samplingCircle[i].y);
		}
	}
	void move(v2f newCenter) {
		center = newCenter;
		if (center != oldCenter) {
			v2s const d = clamp((v2s)(center - oldCenter), -(v2s)size, (v2s)size);
			v2u const ad = (v2u)absolute(d);

			if (d.x > 0) {
				for (u32 voxelY = 0; voxelY < size.y; ++voxelY) {
					for (u32 voxelX = 0; voxelX < size.x - ad.x; ++voxelX) {
						getVoxel(voxelY, voxelX) = getVoxel(voxelY, voxelX + ad.x);
					}
					for (u32 voxelX = size.x - ad.x; voxelX < size.x; ++voxelX) {
						getVoxel(voxelY, voxelX) = {};
					}
				}
			} else if (d.x < 0) {
				for (u32 voxelY = 0; voxelY < size.y; ++voxelY) {
					for (u32 voxelX = size.x - 1; voxelX >= ad.x; --voxelX) {
						getVoxel(voxelY, voxelX) = getVoxel(voxelY, voxelX - ad.x);
					}
					for (u32 voxelX = 0; voxelX < ad.x; ++voxelX) {
						getVoxel(voxelY, voxelX) = {};
					}
				}
			}
			if (d.y > 0) {
				for (u32 voxelY = 0; voxelY < size.y - ad.y; ++voxelY) {
					memcpy(getRow(voxelY), getRow(voxelY + ad.y), size.x * sizeof(getVoxel(0, 0)));
				}
				for (u32 voxelY = size.y - ad.y; voxelY < size.y; ++voxelY) {
					memset(getRow(voxelY), 0, size.x * sizeof(getVoxel(0, 0)));
				}
			} else if (d.y < 0) {
				for (u32 voxelY = size.y - 1; voxelY >= ad.y; --voxelY) {
					memcpy(getRow(voxelY), getRow(voxelY - ad.y), size.x * sizeof(getVoxel(0, 0)));
				}
				for (u32 voxelY = 0; voxelY < ad.y; ++voxelY) {
					memset(getRow(voxelY), 0, size.x * sizeof(getVoxel(0, 0)));
				}
			}
		}
		oldCenter = center;
	}
	u32 update(Renderer &renderer, b32 inverseCheckerboard, f32 timeDelta,
						 View<LightTile> allRaycastTargets) {
		PROFILE_FUNCTION;
		// static auto samplingCircleX = [&] {
		//	std::array<v2x, lightSampleCountX * lightSampleRandomCount> r;
		//	for (u32 i = 0; i < r.size(); ++i) {
		//		f32x<> angle;
		//		for (u32 j = 0; j < simdElementCount; ++j) {
		//			angle[j] = i / simdElementCount / (f32)r.size() + j / (f32)simdElementCount +
		//					   (random.f32() * 2 - 1) * 0.01f;
		//		}
		//		sincos(angle * pi * 2, r[i].x, r[i].y);
		//	}
		//	return r;
		//}();

		std::atomic_uint32_t totalRaysCast = 0;

		constexpr s32xm sampleOffsetsx = []() {
			if constexpr (LightAtlas::simdElementCount == 4)
				return s32x4{0,1,2,3};
			else if constexpr (LightAtlas::simdElementCount == 8)
				return s32x8{0, 1, 2, 3, 4, 5, 6, 7};
			else
				static_assert(false, "can't initialize 'sampleOffsetsx'");
		}();

		auto cast = [&](s32 voxelY) {
			PROFILE_SCOPE("raycast");
			//LightTile tilesToTest[1024];
			for (s32 voxelX = 0; voxelX < (s32)size.x; ++voxelX) {
				//if ((voxelY & 1) ^ (voxelX & 1) ^ inverseCheckerboard)
				//	continue;
				// random.seed >>= 1;
				v2f rayBegin = V2f((f32)voxelX, (f32)voxelY) + center - (v2f)(size / 2);
				v3f vox{};
	#if 1
				v2fxm rayBeginX = V2fxm(rayBegin);
				v3fxm voxX{};
				for (s32 sampleIndex = 0; sampleIndex < lightSampleCount / simdElementCount; ++sampleIndex) {
					//constexpr f32 maxRayLength = size / 2 * sqrt2;
					f32 maxRayLength = length((v2f)size);

					v2fxm dir;
	#if 1
					if constexpr (1 /*lightSampleCount > maxRayLength * 2 * pi*/) {
						s32xm offsets = sampleIndex * (s32)simdElementCount + sampleOffsetsx;
						gather(dir.x, offsets * sizeof(f32) * 2, &samplingCircle[0].x);
						gather(dir.y, offsets * sizeof(f32) * 2, &samplingCircle[0].y);
					} else {
						v2fxm dir0, dir1;
						s32xm offsets0 = sampleIndex * (s32)simdElementCount + sampleOffsetsx;
						s32xm offsets1 = ((sampleIndex + 1) % (lightSampleCount / simdElementCount)) * (s32)simdElementCount + sampleOffsetsx;
						gather(dir0.x, offsets0 * sizeof(f32) * 2, &samplingCircle[0].x);
						gather(dir0.y, offsets0 * sizeof(f32) * 2, &samplingCircle[0].y);
						gather(dir1.x, offsets1 * sizeof(f32) * 2, &samplingCircle[0].x);
						gather(dir1.y, offsets1 * sizeof(f32) * 2, &samplingCircle[0].y);
						dir = lerp(dir0, dir1, random.f32());
						dir = normalize(dir);
					}
	#else
					for (u32 i = 0; i < simdElementCount; ++i)
						dir[i] =
							samplingCircle[(sampleIndex * simdElementCount + i) * lightSampleRandomCount +
												 random.u32() % lightSampleRandomCount];
					dir = pack(dir);
	#endif


					f32xm closestDistanceX = F32xm(pow2(maxRayLength));
					v2fxm rayEnd = rayBeginX + dir * maxRayLength;

	#if 1
					v2fxm tMin, tMax;
					minmax(rayBeginX, rayEnd, tMin, tMax);
					u32 tileCount = 0;
					v2fxm point, normal;
					v3fxm hitColor{};
					for (auto const &tile : allRaycastTargets) {
						if (anyTrue(inBounds(V2fxm(tile.pos), tMin - tile.size, tMax + tile.size))) {
							auto raycastMask = raycastAABB(rayBeginX, rayEnd, V2fxm(tile.pos - tile.size), V2fxm(tile.pos + tile.size), point,
														   normal);

							f32xm dist = distanceSqr(rayBeginX, point);
							auto mask = raycastMask && dist < closestDistanceX;
							closestDistanceX = select(mask, dist, closestDistanceX);
							hitColor = select(mask, V3fxm(tile.color), hitColor);
							// rayEnd = rayBeginX + dir * sqrt(closestDistanceX);
							// minmax(rayBeginX, rayEnd, tMin, tMax);
							++tileCount;
						}
					}
	#else
					v2fxm tMin, tMax;
					minmax(rayBeginX, rayEnd, tMin, tMax);

					u32 tileCount = 0;
					for (auto const &tile : allRaycastTargets) {
						if (anyTrue(inBounds(V2fxm(tile.pos), tMin - tile.size, tMax + tile.size))) {
							tilesToTest[tileCount++] = tile;
						}
					}

					v2fxm point, normal;
					v3fxm hitColor{};
					for (u32 i = 0; i < tileCount; ++i) {
						LightTile *tile = tilesToTest + i;
						auto raycastMask = raycastRect(rayBeginX, rayEnd, V2fxm(tile->pos), V2fxm(tile->size), point,
													   normal);

						f32xm dist = distanceSqr(rayBeginX, point);
						auto mask = raycastMask && dist < closestDistanceX;
						closestDistanceX = select(mask, dist, closestDistanceX);
						hitColor = select(mask, V3fxm(tile->color), hitColor);
						// rayEnd = rayBeginX + dir * sqrt(closestDistanceX);
					}
	#endif
					totalRaysCast += tileCount * simdElementCount;
					voxX += hitColor * 10;
				}
				voxX = unpack(voxX);
				for (u32 i = 0; i < simdElementCount; ++i) {
					vox += voxX[i];
				}
	#else
				for (u32 i = 0; i < lightSampleCount; ++i) {
					v2f dir = samplingCircle[i * lightSampleRandomCount + random.u32() % lightSampleRandomCount];
					v2f rayEnd = rayBegin + dir * (size / 2);

					v2f tMin, tMax;
					minmax(rayBegin, rayEnd, tMin, tMax);

					f32 closestDistance = INFINITY;
					v2f point, normal;
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
				getVoxel(voxelY, voxelX) = lerp(getVoxel(voxelY, voxelX), vox / lightSampleCount,
													min(timeDelta * accumulationRate, 1));
			}
		};
		if (timeDelta) {
			WorkQueue queue{};
			for (s32 i = 0; i < size.y; ++i) {
				queue.push(cast, i);
			}
			queue.completeAllWork();
		}
		return totalRaysCast;
	}
};

void addSoundSamples(u32 targetSampleRate, s16 *dstSubsample, u32 dstSampleCount, SoundBuffer const &buffer, f64 &playCursor, f32 volume[2],
					f32 pitch = 1.0f, bool looping = false, bool resample = true) {
	while (dstSampleCount--) {
		if (resample) {
			f64 sampleOffset = map(playCursor, 0.0f, targetSampleRate, 0.0f, buffer.sampleRate);

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

				s16 result[2];
				f64 t = frac(sampleOffset);
				result[0] = (s16)lerp((f32)srcSubsampleL0, (f32)srcSubsampleL1, t) >> 6;
				result[1] = (s16)lerp((f32)srcSubsampleR0, (f32)srcSubsampleR1, t) >> 6;

				for (u32 channelIndex = 0; channelIndex < 2; ++channelIndex) {
					*dstSubsample++ += (s16)(result[channelIndex] * volume[channelIndex]);
				}
			} else if (buffer.numChannels == 1) {
				s16 srcSubsample0 = ((s16 *)buffer.data)[sampleOffset0];
				s16 srcSubsample1 = ((s16 *)buffer.data)[sampleOffset1];

				s16 result = (s16)lerp((f32)srcSubsample0, (f32)srcSubsample1, frac(sampleOffset)) >> 6;
				for (u32 channelIndex = 0; channelIndex < 2; ++channelIndex) {
					*dstSubsample++ += (s16)(result * volume[channelIndex]);
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
			u32 sampleOffset = (u32)map(playCursor, 0.0f, targetSampleRate, 0.0f, buffer.sampleRate);

			if (looping)
				sampleOffset %= buffer.sampleCount;
			else
				sampleOffset = min(sampleOffset, buffer.sampleCount - 1);

			s16 *musicSubsample = (s16 *)buffer.data + sampleOffset * buffer.numChannels;
			if (buffer.numChannels == 2) {
				for (u32 channelIndex = 0; channelIndex < 2; ++channelIndex) {
					*dstSubsample++ += (s16)((*musicSubsample++ >> 6) * volume[channelIndex]);
				}
			} else if (buffer.numChannels == 1) {
				s16 result = *musicSubsample++ >> 6;
				for (u32 channelIndex = 0; channelIndex < 2; ++channelIndex) {
					*dstSubsample++ += (s16)(result * volume[channelIndex]);
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
					f32 pitch = 1.0f, bool looping = false) {
	f32 volume2[2] {volume, volume};
	addSoundSamples(targetSampleRate, subsample, dstSampleCount, buffer, playCursor, volume2, pitch, looping);
}

struct GameState {
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

	RenderTargetId msRenderTarget, diffuseRt, lightMapRt;
	ShaderId tileShader, solidShader, lightShader, mergeShader, lineShader;
	TextureId atlas, fontTexture, voxelsTex;
	BufferId tilesBuffer, lightsBuffer, debugLineBuffer;

	List<Tile> tilesToDraw;
	List<Light> lightsToDraw;

	LightAtlas lightAtlas;

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
	u32 const ultimateAttackRepeats = 8;
	float auraSize = 0.0f;
	u32 playerHealth;
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

	void pushShotSound(v2f position) { shotSounds.push_back(createSound(shotSound, position, 1, 0.9f + 0.2f * random.f32())); }
	void pushExplosionSound(v2f position) { explosionSounds.push_back(createSound(explosionSound, position, 1, 0.9f + 0.2f * random.f32())); }
	void pushUltimateSound(v2f position) { pushSound(createSound(ultimateSound, position, 1, 0.9f + 0.2f * random.f32())); }

	enum class State { menu, game, pause, death, selectingUpgrade };

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
	};

	UnorderedList<Coin> coinDrops;
	u32 coinsInWallet;
	static constexpr f32 coinRadius = 0.25f;

	bool isPlayerInShop, isPlayerInShopPrevFrame;
	bool shopIsAvailable;
	struct Upgrade {
		char const *name;
		char const *description;
		u32 cost;
		void (*purchaseAction)(GameState &);
		bool (*extraCheck)(GameState &, char const *&);
	};
	u32 selectedUpgradeIndex = 0;
	List<Upgrade> upgrades;
	f32 showPurchaseFailTime = 0.0f;

	bool hasCoinMagnet;
	bool explosiveBullets;
	char const *purchaseFailReason;

	struct DebugProfile {
		u64 raycastCy;
		u64 totalRaysCast;
		u8 mode;
	};
	DebugProfile debugProfile{};
	
	struct DebugLine {
		v2f a, b;
		v4f color;
	};

	enum class DebugValueType { u1, u8, u16, u32, u64, s8, s16, s32, s64 };

	DebugLine createDebugLine(v2f a, v2f b, v4f color) {
		DebugLine result;
		result.a = a;
		result.b = b;
		result.color = color;
		return result;
	}

	UnorderedList<DebugLine> debugLines;
	bool debugDrawBotPath = false;
	bool debugDrawMapGraph = false;
	bool debugUpdateBots = true;
	bool debugGod = false;
	bool debugSun = false;

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
		View players = {&playerP, 1};
		return ::raycast(&world.tiles, (ignoredLayers & RaycastLayer::bot) ? View<Bot>{} : bots,
						 (ignoredLayers & RaycastLayer::player) ? View<v2f>{} : players, a, b, point, normal);
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
			embers.push_back(e);
		}
	
		pushExplosionSound(position);
	}
	void spawnExplosion(v2f position, v2f incoming, v2f normal) {
		Explosion ex;
		ex.position = position;
		ex.maxLifeTime = ex.remainingLifeTime = 0.75f + random.f32() * 0.5f;
		ex.rotationOffset = random.f32() * (2 * pi);
		explosions.push_back(ex);

		spawnEmbers(position, incoming, normal);
	}
	void spawnBullet(u32 ignoredLayers, v2f position, f32 rotation, f32 velocity) {
		Bullet b;
		b.ignoredLayers = ignoredLayers;
		b.position = position;
		b.rotation = rotation;
		sincos(rotation, b.velocity.y, b.velocity.x);
		b.velocity *= velocity;
		bullets.push_back(b);
	}
	void spawnBullet(u32 ignoredLayers, v2f position, v2f velocity) {
		Bullet b;
		b.ignoredLayers = ignoredLayers;
		b.position = position;
		b.velocity = velocity;
		b.rotation = atan2(normalize(velocity));
		bullets.push_back(b);
	}

	void spawnBot(v2f position) {
		Bot newBot{};
		newBot.radius = playerR;
		newBot.health = currentWave;
		newBot.position = position;
		bots.push_back(newBot);
		++totalBotsSpawned;
	}

	Tile createTile(v2f position, v2f uv0, v2f uv1, f32 uvMix, v2f uvScale, v2f size = V2f(1.0f), f32 rotation = 0.0f,
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
		return t;
	}
	Tile createTile(v2f position, v2f uv0, v2f uvScale, v2f size = V2f(1.0f), f32 rotation = 0.0f, v4f color = V4f(1)) {
		return createTile(position, uv0, {}, 0, uvScale, size, rotation, color);
	}
	Light createLight(v3f color, v2f position, f32 radius, v2f clientSize) {
		Light l;
		l.color = color;
		l.radius = camZoom * radius * 2;
		l.position = (position - cameraP) * camZoom;
		l.position.x *= clientSize.y / clientSize.x;
		return l;
	}

	void drawText(Renderer &renderer, View<Label> labels, v2f clientSize) {
		v2f textScale{11, 22};

		tilesToDraw.clear();
		for (auto &l : labels) {
			v2f p = l.p / textScale;
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
					createTile(p + v2f{column + 0.5f, row - 0.5f}, {u, v}, V2f(1.0f / 16.0f), V2f(1.0f), 0, l.color));
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
	void drawRect(Renderer &renderer, View<Rect> rects, v2f clientSize) {
		tilesToDraw.clear();
		for (auto r : rects) {
			r.pos.y = (s32)clientSize.y - r.pos.y;
			// printf("%i\n", r.pos.y);
			tilesToDraw.push_back(
				createTile((v2f)r.pos + (v2f)r.size * V2f(0.5f, -0.5f), {}, {}, (v2f)r.size, 0, r.color));
		}
		renderer.updateBuffer(tilesBuffer, tilesToDraw.data(), (u32)tilesToDraw.size() * sizeof(tilesToDraw[0]));

		renderer.setMatrix(0, m4::translation(-1, -1, 0) * m4::scaling(2.0f / clientSize, 0));

		/*
		m4 m = m4::scaling(1.0f / clientSize, 0);
		for (auto &r : rects) {
			v4 p = V4f((m * V4f((v2f)r.pos, 0, 1)).xy, (m * V4f((v2f)r.size, 0, 1)).xy);
			printf("%.3f %.3f %.3f %.3f\n", p.x, p.y, p.z, p.w);
		}
		*/

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

	void pushUpgrade(char const *name, char const *description, u32 cost, void (*purchaseAction)(GameState &), bool (*extraCheck)(GameState &, char const *&) = 0) {
		Upgrade result;
		result.name = name;
		result.description = description;
		result.cost = cost;
		result.purchaseAction = purchaseAction;
		result.extraCheck = extraCheck;
		upgrades.push_back(result);
	}
	void initUpgrades() {
		pushUpgrade("Restore health", 
					"Heals you to 100%",
					30, 
					[](GameState &state){ state.playerHealth = 100; }, 
					[](GameState &state, char const *&msg){ msg = "Your health is good already"; return state.playerHealth < 100; });
		pushUpgrade("Multi shot",
					"You can shoot one more bullet",
					40, 
					[](GameState &state){ ++state.shotsAtOnce; },
					[](GameState &state, char const *&msg){ msg = "Maximum level reached"; return state.shotsAtOnce < 10; });
		pushUpgrade("Coin magnet",
					"You can attract coins within a radius of 5 meter ",
					100,
					[](GameState &state){ state.hasCoinMagnet = true; },
					[](GameState &state, char const *&msg){ msg = "You already have coin magnet"; return !state.hasCoinMagnet; });
		pushUpgrade("Fire rate +10%", 
					"That's it", 
					30,
					[](GameState &state){ state.fireDelta *= 1 / 1.1f; },
					[](GameState &state, char const *&msg){ msg = "Maximum level reached"; return state.fireDelta > 0.1f; });
		pushUpgrade("Explosive bullets", 
					"Deal damage to mobs standing next to your bullet's explosion", 
					100,
					[](GameState &state){ state.explosiveBullets = true; },
					[](GameState &state, char const *&msg){ msg = "You already have explosive bullets"; return !state.explosiveBullets; });
	}
	std::mt19937 mt{(srand((u32)time(0)), std::random_device{}() ^ rand())};
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
		targetCamZoom = 1.0f / 7;
		camZoom = targetCamZoom;
		fireTimer = 0.0f;

		
		ultimateAttackPercent = 0;
		ultimateAttackRepeatTimer = 0.0f;

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
		playNextMusic();
		soundMutex.unlock();

		memset(lightAtlas.voxels, 0, sizeof(lightAtlas.voxels));
		renderer.updateTexture(voxelsTex, lightAtlas.voxels);
	}

	void init() {
		PROFILE_FUNCTION;

		WorkQueue work;

		work.push(&GameState::initDebug, this);
		work.push(&LightAtlas::init, &lightAtlas);

		std::mutex musicMutex;
		for (u32 i = 0; ; ++i) {
			char path[64];
			sprintf(path, DATA "audio/music%u.wav", i);
			if (fileExists(path)) {
				work.push([this, &musicMutex, path = std::string(path)] { 
					auto m = loadWaveFile(path.data());
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

		work.completeAllWork();

		playingMusic.buffer = music.begin();

#if 0
		for (u32 i=0;i<CHUNK_WIDTH-2;++i){
			spawnBot(v2f{(f32)i+1, 1});
		}
#endif
	}
#if 0
	static constexpr u32 l1size = 32 * 1024;
	static constexpr u32 l2size = 256 * 1024;
	static constexpr u32 l3size = 6 * 1024 * 1024;

	v4s *test;
	static constexpr u32 testSize = l1size;
	static constexpr u32 testCount = testSize / sizeof(v4s);
	static constexpr u64 repeats = l3size / testSize * 1024;

	template<u32 step>
	void doTest() {
		u64 sum = 0;
		
		auto loop = [&]() {
			for (auto v = test; v != test + testCount; ++v) {
				*v = frac(*v, step);
			}
		};

		for (u32 i = 0; i < (repeats + 1); ++i) {
			test = (v4s *)_aligned_malloc(testSize, testSize);
			
			// warmup
			loop();
			
			auto begin = __rdtsc();
			loop();
			sum += __rdtsc() - begin;
			
			_aligned_free(test);
		}
		printf("frac<%u>(v): %.1f cy\n", step, (f64)sum / repeats / testCount);
	}
	void doTest(u32 step) {
		u64 sum = 0;
		
		auto loop = [&]() {
			for (auto v = test; v != test + testCount; ++v) {
				*v = frac(*v, step);
			}
		};

		for (u32 i = 0; i < (repeats + 1); ++i) {
			test = (v4s *)_aligned_malloc(testSize, testSize);
			
			// warmup
			loop();
			
			auto begin = __rdtsc();
			loop();
			sum += __rdtsc() - begin;
			
			_aligned_free(test);
		}
		printf("frac(v, %u): %.1f cy\n", step, (f64)sum / repeats / testCount);
	}
#endif
	void start(Window &window, Renderer &renderer, Input &input, Time &time) {
		PROFILE_FUNCTION;
		
		printf("%s, %s, %u cores, L1: %u, L2: %u, L3: %u\n", 
			   toString(cpuInfo.vendor), 
			   cpuInfo.brand, 
			   cpuInfo.logicalProcessorCount, 
			   cpuInfo.totalCacheSize(1), 
			   cpuInfo.totalCacheSize(2), 
			   cpuInfo.totalCacheSize(3));
		
#if 0
		printf("%i\n", startsWith("Hello", "Hello"));
		printf("%i\n", startsWith("Hell", "Hello"));
		printf("%i\n", startsWith("Hello", "Hell"));
		puts("");
		printf("%i\n", startsWith("Hello", 4, "Hello"));
		printf("%i\n", startsWith("Hello", 5, "Hello"));
		printf("%i\n", startsWith("Hello", 6, "Hello"));
		printf("%i\n", startsWith("Hello", 7, "Hello"));
		puts("");
		printf("%i\n", startsWith("Hello", "Hello", 4));
		printf("%i\n", startsWith("Hello", "Hello", 5));
		printf("%i\n", startsWith("Hello", "Hello", 6));
		printf("%i\n", startsWith("Hello", "Hello", 7)); // invalid
		puts("");
		printf("%i\n", startsWith("Hello", "Hell", 4));
		printf("%i\n", startsWith("Hello", "Hell", 5));
		printf("%i\n", startsWith("Hello", "Hell", 6));
		printf("%i\n", startsWith("Hello", "Hell", 7));
#endif

#if 0
		doTest<15>();
		doTest<16>();
		u32 p2 = 1u << (((u32)time(0) | 1) & 0xF);
		doTest(p2 + 1);
		doTest(p2);
#endif

		WorkQueue work;

		work.push([&]{ tileShader   = renderer.createShader(DATA "shaders/tile");});
		work.push([&]{ solidShader  = renderer.createShader(DATA "shaders/solid");});
		work.push([&]{ lightShader  = renderer.createShader(DATA "shaders/light");});
		work.push([&]{ 
			ShaderMacro macro;
			macro.name = "MSAA_SAMPLE_COUNT";
			macro.value = "4";
			mergeShader = renderer.createShader(DATA "shaders/merge", macro);
		});
		work.push([&]{ lineShader   = renderer.createShader(DATA "shaders/line");});
		work.push([&]{ tilesBuffer  = renderer.createBuffer(0, sizeof(Tile), MAX_TILES);});
		work.push([&]{ lightsBuffer = renderer.createBuffer(0, sizeof(Light), MAX_LIGHTS);});
		work.push([&]{ debugLineBuffer = renderer.createBuffer(0, sizeof(DebugLine), 1024 * 8);});
		work.push([&]{ atlas = renderer.createTexture(DATA "textures/atlas.png", Address::clamp, Filter::point_point);});
		work.push([&]{ fontTexture = renderer.createTexture(DATA "textures/font.png", Address::clamp, Filter::point_point);});
		
		work.completeAllWork();
	}
	void debugReload() {
		upgrades.clear();
		initUpgrades();
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

		pixelsInMeter = (v2f)window.clientSize / (f32)window.clientSize.y * camZoom;
		
#if BUILD_DEBUG
		v2u size = V2u(6);
#else
		v2u size = V2u(16);
#endif

		size.x = size.x * window.clientSize.x / window.clientSize.y;

		if (voxelsTex.valid())
			renderer.release(voxelsTex);
		voxelsTex = renderer.createTexture(Format::F_RGB32, size, Address::border, Filter::linear_linear, {});

		lightAtlas.resize(size);
	}
	void update(Window &window, Renderer &renderer, Input &input, Time &time) {
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

		if (input.keyHeld(Key_f2)) {
			debugDrawMapGraph ^= input.keyDown('M');
			debugDrawBotPath ^= input.keyDown('P');
			debugUpdateBots ^= input.keyDown('B');
			debugGod ^= input.keyDown('G');
			spawnBots ^= input.keyDown('S');
			if (input.keyDown('L')) {
				debugSun ^= 1;
				if (debugSun) {
					populate(lightAtlas.voxels, 1.0f, sizeof(lightAtlas.voxels) / sizeof(f32));
					renderer.updateTexture(voxelsTex, lightAtlas.voxels);
				}
			}
			if (input.keyDown('C')) {
				coinsInWallet += 100;
			}
		}
		
		debugLines.clear();

#if 0
		if (debugDrawMapGraph) {
			for(u32 x=0;x<CHUNK_WIDTH;++x) {
				for(u32 y=0;y<CHUNK_WIDTH;++y) {
					if (auto &node = world.graph.nodes[x][y]) {
						for (auto n : node->neighbors) {
							debugLines.push_back(createDebugLine((v2f)v2u{x,y}, (v2f)v2u{n.x, n.y}, V4f(V3f(1), 0.1f)));
						}
					}
				}
			}
		}
#endif

		shotSounds.clear();
		explosionSounds.clear();
		
		tilesToDraw.clear();
		tilesToDraw.reserve(CHUNK_WIDTH * CHUNK_WIDTH + bullets.size() + embers.size() + explosions.size() + 1);
		lightsToDraw.clear();
		lightsToDraw.reserve(bullets.size() + explosions.size() + 1);

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
					break;
				}
				break;
			}
			case State::selectingUpgrade: break;
			default: INVALID_CODE_PATH("invalid state");
		}
		f32 const longestFrameTimeAllowed = 1.f / 15;
		time.delta = min(time.delta, longestFrameTimeAllowed);
		timeScale = moveTowards(timeScale, (f32)(currentState == State::game || currentState == State::selectingUpgrade), time.delta);

		f32 scaledDelta = time.delta * timeScale;
		scaledTime += scaledDelta;

		if (scaledDelta > 0) {
			v2f move{};
			if (currentState == State::game) {
				move.x = input.keyHeld('D') - input.keyHeld('A');
				move.y = input.keyHeld('W') - input.keyHeld('S');
			}
			if (!move.x) move.x = input.joyButtonHeld(1) - input.joyButtonHeld(3);
			if (!move.y) move.y = input.joyButtonHeld(0) - input.joyButtonHeld(2);

			playerV = moveTowards(playerV, normalize(move, {}) * 5, scaledDelta * 50);

			playerP = movePlayer(world.tiles, playerP, playerV, scaledDelta, playerR);

			cameraP = lerp(cameraP, playerP, min(scaledDelta * 10, 1));

			auraSize = lerp(auraSize, hasUltimateAttack(), scaledDelta * 5.0f);
		
			isPlayerInShopPrevFrame = isPlayerInShop;
			isPlayerInShop = isInShop(playerP);

			if (shopIsAvailable && isPlayerInShopPrevFrame && !isPlayerInShop) {
				// went out

				setShopOpen(false);

				spawnBots = true;
				++currentWave;
			}

			if(debugUpdateBots) {
				PROFILE_BEGIN("bots");
				for (auto &bot : bots) {
					v2f hitPoint, hitNormal;
					auto raycastResult = ::raycast(&world.tiles, {}, {&playerP, 1}, bot.position, playerP, hitPoint, hitNormal).type;
					
					f32 const botFireDelta = 120.0f / 113.5f;

							v2f dir = normalize(playerP - bot.position);
#if 0
							f32 dt = 0;
							f32 playerVLenSqr = lengthSqr(playerV);
							v4f debugColor = {1, 0, 0, 1};
							if (playerVLenSqr > 0) {
								f32 playerVLen = TL::sqrt(playerVLenSqr);
								if (playerVLen <= bulletSpeed) {
									debugColor = {0, 1, 0, 1};
									v2f crss = normalize(cross(playerV));
									dt = dot(dir, crss);
									if (dt < 0) {
										crss = -crss;
										dt = -dt;
									}
									dir = (playerV + crss * TL::sqrt(bulletSpeed - playerVLen)) / bulletSpeed;
								}
							}
							v2f targetP = dt ? bot.position + dir * distance(bot.position, playerP) / dt : playerP;
							debugLines.push_back(createDebugLine(bot.position, targetP, debugColor));
#endif

					if (bot.fireTimer >= botFireDelta) {
						if (raycastResult == RaycastResultType::player) {
							bot.fireTimer -= botFireDelta;

							spawnBullet(RaycastLayer::bot, bot.position, dir * bulletSpeed);
							pushShotSound(bot.position);
						}
					} else {
						bot.fireTimer += scaledDelta;
					}
					auto path = world.graph.getPath((v2u)roundToInt(bot.position), (v2u)roundToInt(playerP));
					if(path.size() > 1) {
						if (debugDrawBotPath) {
							for(u32 i = 1; i < path.size(); ++i) {
								debugLines.push_back(createDebugLine((v2f)path[i-1], (v2f)path[i], V4f(1,.5,.5,.5)));
							}
						}
						v2f targetPosition = (v2f)path[path.size() - 2];

						v2f diff = targetPosition - bot.position;
						v2f velocity = normalize(diff, {1}) * 3;

						if (distanceSqr(playerP, bot.position) < pow2(3.0f) && raycastResult == RaycastResultType::player)
							velocity *= -1;

						for (auto &other : bots) {
							if (&bot != &other) {
								f32 penetration = playerR * 2 - distance(bot.position, other.position);
								if (penetration > 0) {
									bot.position += penetration * normalize(bot.position - other.position);
								}
							}
						}
				
						bot.velocity = lerp(bot.velocity, velocity, scaledDelta * 10.0f);
						bot.position = movePlayer(world.tiles, bot.position, bot.velocity, scaledDelta, playerR);
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

						spawnBot((v2f)position);
						if (totalBotsSpawned % 50 == 0) {
							spawnBots = false;
						}
					}
				}
#endif
				PROFILE_END;
			}
			PROFILE_BEGIN("bullets");
			for (u32 i = 0; i < bullets.size(); ++i) {
				auto &b = bullets[i];
				b.remainingLifetime -= scaledDelta;
				if (b.remainingLifetime <= 0) {
					erase(bullets, b);
					--i;
					continue;
				}
				v2f nextPos = b.position + b.velocity * scaledDelta;
				v2f hitPoint, hitNormal;
				auto raycastResult = raycast(b.ignoredLayers, b.position, nextPos, hitPoint, hitNormal);

				auto damageBot = [&](Bot &bot) {
					--bot.health;
					if (bot.health == 0) {
						Coin newCoin;
						newCoin.position = bot.position;
						coinDrops.push_back(newCoin);

						++botsKilled;
						if (!hasUltimateAttack()) {
							++ultimateAttackPercent;
						}

						bots.erase(bot);
					}
				};

				if (raycastResult.type == RaycastResultType::bot) {
					auto &hitBot = bots[raycastResult.target];
					damageBot(hitBot);
				} else if (raycastResult.type == RaycastResultType::player) {
					if (playerHealth != 0 && !debugGod) {
						--playerHealth;
						if (playerHealth == 0) {
							currentState = State::death;
							setCursorVisibility(true);
						}
					}
				}
				if (raycastResult.type != RaycastResultType::noHit) {
					if (b.ignoredLayers & RaycastLayer::player && explosiveBullets) {
						spawnExplosion(hitPoint, normalize(b.velocity), hitNormal);
						for (auto &bot : bots) {
							if (distanceSqr(bot.position, hitPoint) < 1) {
								damageBot(bot);
							}
						}
					} else {
						spawnEmbers(hitPoint, normalize(b.velocity), hitNormal);
					}
					erase(bullets, b);
					--i;
					continue;
				}
				b.position = nextPos;
			}
			PROFILE_END;

			if (hasCoinMagnet) {
				for (auto &c : coinDrops) {
					f32 ds = distanceSqr(c.position, playerP);
					if (ds < pow2(5)) {
						c.position = lerp(c.position, playerP, scaledDelta * min(1 / ds, 1) * 10);
					}
				}
			}

			bool newShopIsAvailable = !spawnBots && bots.empty();
			if (!shopIsAvailable && newShopIsAvailable)
				setShopOpen(true);
			shopIsAvailable = newShopIsAvailable;


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
			for (auto c = coinDrops.begin(); c != coinDrops.end();) {
				if (distanceSqr(c->position, playerP) < pow2(playerR + coinRadius)) {
					++coinsInWallet;
					pushSound(createSound(coinSound, c->position, 1, 0.9f + 0.2f * random.f32()));
					coinDrops.erase(c);
					continue;
				}
				++c;
			}
		}

		
		v2s mousePos = input.mousePosition;

		v2f joyAxes = {
			input.joyAxis(JoyAxis_x),
			input.joyAxis(JoyAxis_y)
		};

		if (lengthSqr(joyAxes) > 0.0001f) {
			v2s halfClientSize = (v2s)(window.clientSize >> 1);
			s32 minDim = min(halfClientSize.x, halfClientSize.y);
			mousePos = halfClientSize + (v2s)(joyAxes * 0.9f * minDim);
		}

		mousePos.y = (s32)window.clientSize.y - mousePos.y;
		v2f mouseWorldPos = screenToWorld((v2f)mousePos, (v2f)window.clientSize, cameraP, camZoom);

		if (currentState == State::game) {
			if (fireTimer > 0) {
				fireTimer -= scaledDelta;
			} else {
				bool holdingFire = input.mouseHeld(0) || input.joyButtonHeld(4);
				if (holdingFire) {
					fireTimer += fireDelta;
					auto createBullet = [&](f32 offset) {
						spawnBullet(RaycastLayer::player, playerP, atan2(normalize(mouseWorldPos - playerP)) + offset, (0.9f + 0.2f * random.f32()) * bulletSpeed);
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
			if (input.mouseDown(1) && hasUltimateAttack()) {
				ultimateAttackPercent = 0;
				ultimateAttackRepeatTimer = (f32)ultimateAttackRepeats;
				lastUltimateRepeatIndex = ultimateAttackRepeats;
			}
			ultimateAttackRepeatTimer -= scaledDelta * 4.0f;
			if(lastUltimateRepeatIndex > 0) {

				UnorderedList<v2f> botPositions;
				botPositions.reserve(bots.size());
				for (auto &b : bots) {
					v2f point, normal;
					if(::raycast(&world.tiles, bots, {}, playerP, b.position, point, normal).type == RaycastResultType::bot) {
						botPositions.push_back(b.position);
					}
				}
				std::sort(botPositions.begin(), botPositions.end(), [this](v2f a, v2f b){ return distanceSqr(playerP, a) < distanceSqr(playerP, b); });

				bool playUltimateSound = false;
				u32 ultimateRepeatIndex = (u32)ultimateAttackRepeatTimer;
				for(u32 j = ultimateRepeatIndex; j != lastUltimateRepeatIndex; ++j) {
					u32 const ultBulletCount = 25;
					for (u32 i = 0; i < ultBulletCount && i < botPositions.size(); ++i) {
						spawnBullet(RaycastLayer::player, playerP, normalize(botPositions[i] - playerP) * bulletSpeed);
					}
					if (botPositions.size()) {
						playUltimateSound = true;
					}
				}
				if (playUltimateSound) {
					pushUltimateSound(playerP);
				}
				lastUltimateRepeatIndex = ultimateRepeatIndex;
			}
			if (isPlayerInShop && shopIsAvailable) {
				if (input.keyDown('B')) {
					currentState = State::selectingUpgrade;
				}
			}
		} else if (currentState == State::selectingUpgrade) {
			if (input.keyDown('B') || input.keyDown(Key_escape) || !isPlayerInShop || !shopIsAvailable) {
				currentState = State::game;
			}
			if (isPlayerInShop) {
				if (input.keyDown(Key_downArrow)) {
					++selectedUpgradeIndex;
					if (selectedUpgradeIndex == (u32)upgrades.size()) {
						selectedUpgradeIndex = 0;
					}
				}
				if (input.keyDown(Key_upArrow)) {
					--selectedUpgradeIndex;
					if (selectedUpgradeIndex == -1) {
						selectedUpgradeIndex = (u32)upgrades.size() - 1;
					}
				}
			} else {
			
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

		PROFILE_BEGIN("prepare tiles & raycast targets");

		static UnorderedList<LightTile> allRaycastTargets;
		allRaycastTargets.clear();

		for (u32 x = 0; x < CHUNK_WIDTH; ++x) {
			for (u32 y = 0; y < CHUNK_WIDTH; ++y) {
				if (getTile(world.tiles, x, y)) {
					allRaycastTargets.push_back({(v2f)v2u{x, y}, 0.5f, V3f(0.03f)});
					tilesToDraw.push_back(createTile((v2f)v2u{x, y}, offsetAtlasTile(1, 6), ATLAS_ENTRY_SIZE));
				} else {
					tilesToDraw.push_back(createTile((v2f)v2u{x, y}, offsetAtlasTile(1, 0), ATLAS_ENTRY_SIZE));
				}
			}
		}

		for (auto &b : bullets) {
			u32 frame = (u32)(b.remainingLifetime * 12) % 4;
			v2u framePos{frame % 2, frame / 2};
			tilesToDraw.push_back(createTile(b.position,
											 offsetAtlasTile(0, 1) + (v2f)framePos * ATLAS_ENTRY_SIZE * 0.5f,
											 ATLAS_ENTRY_SIZE * 0.5f, V2f(0.5f), b.rotation));
			lightsToDraw.push_back(createLight({.4, .5, 1}, b.position, 1, (v2f)window.clientSize));
			allRaycastTargets.push_back({b.position, 0.25f, {.4, .5, 1}});
		}
		for (auto &e : embers) {
			f32 t = e.remainingLifeTime / e.maxLifeTime;
			f32 tt = t * t;
			auto uvs = getFrameUvs(1 - t, 16, 4, {1, 1}, 0.25f, false);
			tilesToDraw.push_back(createTile(e.position, uvs.uv0, uvs.uv1, uvs.uvMix, ATLAS_ENTRY_SIZE * 0.25f,
											 V2f(map(tt, 0, 1, 0.1f, 0.25f)), t * 3 + e.rotationOffset));
			//	allRaycastTargets.push_back({e.position, max(0.0f, t - 0.9f) * 5.0f, v3{1, .5, .1} * 10.0f});
			lightsToDraw.push_back(createLight(v3f{1, .5, .1} * tt * 5, e.position, .5f, (v2f)window.clientSize));
		}
		for (auto &e : explosions) {
			f32 t = e.remainingLifeTime / e.maxLifeTime;

			auto uvs = getFrameUvs(1 - t, 8, 2, {2, 0}, 1, false);

			tilesToDraw.push_back(createTile(e.position, uvs.uv0, uvs.uv1, uvs.uvMix, ATLAS_ENTRY_SIZE,
											 V2f(map(t, 0, 1, 0.75f, 1.5f)), t * 2 + e.rotationOffset));
			allRaycastTargets.push_back({e.position, max(0.0f, t - 0.8f) * 5.0f, v3f{1, .5, .1} * 10.0f});
			lightsToDraw.push_back(createLight(v3f{1, .5, .1} * t * 3, e.position, 3, (v2f)window.clientSize));
		}
		for (auto &c : coinDrops) {
			auto uvs = getFrameUvs(frac(scaledTime), 6, 4, {0, 4}, 1);

			tilesToDraw.push_back(createTile(c.position, uvs.uv0, uvs.uv1, uvs.uvMix, ATLAS_ENTRY_SIZE, V2f(0.5f)));
			allRaycastTargets.push_back({c.position, 0.25f, v3f{1, .75, .1} * 0.5f});
			//lightsToDraw.push_back(createLight(v3f{1, .75, .1}, c.position, 2, (v2f)window.clientSize));
		}
		for (auto &bot : bots) {
			tilesToDraw.push_back(createTile(bot.position, offsetAtlasTile(0, 0), ATLAS_ENTRY_SIZE));
			//allRaycastTargets.push_back({bot.position, playerR, {}});
		}
		tilesToDraw.push_back(createTile(playerP, offsetAtlasTile(0, 0), ATLAS_ENTRY_SIZE));

		if (hasUltimateAttack()) {
			auto uvs = getFrameUvs(scaledTime, 8, 2, {4, 0}, 2);
			tilesToDraw.push_back(createTile(playerP, uvs.uv0, uvs.uv1, uvs.uvMix, ATLAS_ENTRY_SIZE * 2,
											 V2f(1 + auraSize), scaledTime * pi, V4f(V3f(1), auraSize)));
		}

		// player light
		allRaycastTargets.push_back({playerP, playerR, V3f(1.0f)});
		// lightsToDraw.push_back(createLight(V3f(1), playerP, 7, (v2f)window.clientSize));
		
		// safe zone
		v2f safeZonePos = {CHUNK_WIDTH/2-0.5f, 1.5f};
		v3f safeZoneColor = v3f{0.3f, 1.0f, 0.6f};
		if (shopIsAvailable) {
			allRaycastTargets.push_back({safeZonePos, 1, safeZoneColor});
			lightsToDraw.push_back(createLight(safeZoneColor, safeZonePos, 5, (v2f)window.clientSize));
		}
		tilesToDraw.push_back(createTile(safeZonePos - v2f{0, 1.5f}, offsetAtlasTile(2, 5), ATLAS_ENTRY_SIZE * v2f{2, 1}, {2, 1}, pi * 0.1f));

		PROFILE_END;

		renderer.updateBuffer(tilesBuffer, tilesToDraw.data(), (u32)tilesToDraw.size() * sizeof(tilesToDraw[0]));
		renderer.updateBuffer(lightsBuffer, lightsToDraw.data(), (u32)lightsToDraw.size() * sizeof(lightsToDraw[0]));

		// for(auto &bot : bots) {
		//	allRaycastTargets.push_back({bot.position, 0.3f, V3f(0.6f, 0.5f, 0.4f)});
		//}

		lightAtlas.move(floor(playerP + 1.0f));
		if (!debugSun) {
			u64 raycastCyBegin = __rdtsc();
			debugProfile.totalRaysCast = lightAtlas.update(renderer, (b32)time.frameCount & 1, scaledDelta,
														  allRaycastTargets);
			debugProfile.raycastCy = __rdtsc() - raycastCyBegin;
			renderer.updateTexture(voxelsTex, lightAtlas.voxels);
		}

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

		/*
		renderer.bindRenderTarget(diffuseRt);
		renderer.bindRenderTargetAsTexture(msRenderTarget, Stage::ps, 0);
		renderer.bindShader(msaaShader);
		renderer.disableBlend();
		renderer.draw(3);
		*/

		renderer.bindRenderTarget(lightMapRt);
		renderer.clearRenderTarget(lightMapRt, {});
		renderer.bindShader(lightShader);
		renderer.bindBuffer(lightsBuffer, Stage::vs, 0);
		renderer.setBlend(Blend::one, Blend::one, BlendOp::add);
		renderer.draw((u32)lightsToDraw.size() * 6);

		v2f voxScale = v2f{(f32)window.clientSize.x / window.clientSize.y, 1};
		renderer.setMatrix(0, m4::translation((cameraP - lightAtlas.center) / (v2f)lightAtlas.size * 2, 0) *
								  m4::translation(V2f(1.0f / (v2f)lightAtlas.size), 0) *
								  m4::scaling(voxScale / (v2f)lightAtlas.size / camZoom * 2, 1));
		renderer.setV4f(0, {menuAlpha, deathScreenAlpha});
		renderer.bindRenderTarget({0});
		renderer.bindRenderTargetAsTexture(msRenderTarget, Stage::ps, 0);
		renderer.bindRenderTargetAsTexture(lightMapRt, Stage::ps, 1);
		renderer.bindTexture(voxelsTex, Stage::ps, 2);
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

		debugLines.push_back(createDebugLine(playerP, mouseWorldPos, V4f(1,0,0,0.5f)));

		renderer.updateBuffer(debugLineBuffer, debugLines.data(), (u32)debugLines.size() * sizeof(debugLines[0]));
		renderer.bindBuffer(debugLineBuffer, Stage::vs, 0);

		renderer.setTopology(Topology::LineList);
		renderer.setMatrix(0, worldMatrix);
		renderer.setBlend(Blend::srcAlpha, Blend::invSrcAlpha, BlendOp::add);
		renderer.bindShader(lineShader);
		renderer.draw((u32)debugLines.size() * 2);

		renderer.setTopology(Topology::TriangleList);

		tilesToDraw.clear();
		UnorderedList<Tile> uiTiles;
		
		UnorderedList<Label> labels;
		labels.reserve(8);

		f32 uiAnimationDelta = time.delta * 2;

		uiAlpha = moveTowards(uiAlpha, (f32)(currentState != State::menu && currentState != State::death), uiAnimationDelta);
		deathScreenAlpha = moveTowards(deathScreenAlpha, (f32)(currentState == State::death), uiAnimationDelta);
		menuAlpha = moveTowards(menuAlpha, (f32)(currentState == State::menu), uiAnimationDelta);
		pauseAlpha = moveTowards(pauseAlpha, (f32)(currentState == State::pause), uiAnimationDelta);
		shopHintAlpha = moveTowards(shopHintAlpha, (f32)(isPlayerInShop && shopIsAvailable && currentState != State::selectingUpgrade), uiAnimationDelta);
		shopUiAlpha = moveTowards(shopUiAlpha, (f32)(currentState == State::selectingUpgrade), uiAnimationDelta);
		
		if (shopHintAlpha > 0) {
			labels.push_back({V2f(8, 256), "Press b to enter the shop", V4f(1,1,1,shopHintAlpha)});
		}

		if (shopUiAlpha > 0) {
			showPurchaseFailTime -= time.delta;
			v2f solidSpritePos = offsetAtlasTile(0, 6);
			v2f solidSpriteSize = ATLAS_ENTRY_SIZE / 16;

			v4f uiColor = V4f(V3f(0.5f), shopUiAlpha * 0.5f);

			labels.push_back({V2f(8, 256), "Use arrow keys to select an upgrade.\nPress enter to buy it.", V4f(1,1,1,shopUiAlpha)});
			auto button = [&](v2f pos, v2f size, u32 index, std::string &&text) {
				v2f rectPos = pos;
				rectPos.y = window.clientSize.y - rectPos.y - size.y;
				bool c = selectedUpgradeIndex == index;
				uiTiles.push_back(createTile(rectPos + size * 0.5f, solidSpritePos, solidSpriteSize, size,
												0, c ? V4f(1, 1, 1, uiColor.w) : uiColor));
				labels.push_back({pos + 2, std::move(text), V4f(1, 1, 1, shopUiAlpha)});
				return currentState == State::selectingUpgrade && c && input.keyDown(Key_enter);
			};
			
			for (u32 i = 0; i < upgrades.size(); ++i) {
				auto &upgrade = upgrades[i];
				char buf[256];
				sprintf(buf, "%s: $%u", upgrade.name, upgrade.cost);
				if (selectedUpgradeIndex == i) {
					labels.push_back({{300, 300.0f + i * 26}, upgrade.description, V4f(1,1,1,shopUiAlpha)});
				}
				if (button({8, 300.0f + i * 26}, {290, 26}, i, buf)) {
					if (upgrade.extraCheck ? upgrade.extraCheck(*this, purchaseFailReason) : true) {
						if (coinsInWallet >= upgrade.cost) {
							upgrade.purchaseAction(*this);
							coinsInWallet -= upgrade.cost;
						} else {
							purchaseFailReason = "Not enough coins!";
							showPurchaseFailTime = 1;
						}
					} else {
						showPurchaseFailTime = 1;
					} 
				}
			}
			if (showPurchaseFailTime > 0) {
				labels.push_back({V2f(512, 256), purchaseFailReason, V4f(1,1,1,shopUiAlpha)});
			}
		}

		if (pauseAlpha > 0) {
			v2f solidSpritePos = offsetAtlasTile(0, 6);
			v2f solidSpriteSize = ATLAS_ENTRY_SIZE / 16;

			v4f menuColor = V4f(V3f(0.5f), pauseAlpha * 0.5f);

			v2f menuPanelPos = (v2f)window.clientSize * 0.4f;
			v2f menuPanelSize = V2f(11 * 8, 22) + V2f(8);
			
			auto contains = [](auto pos, auto size, auto mp) {
				return pos.x <= mp.x && mp.x < pos.x + size.x && pos.y <= mp.y && mp.y < pos.y + size.y;
			};
			auto button = [&](v2f pos, v2f size, char const *text) {
				v2f rectPos = pos;
				rectPos.y = window.clientSize.y - rectPos.y - size.y;
				bool c = contains(rectPos, size, mousePos);
				uiTiles.push_back(createTile(rectPos + size * 0.5f, solidSpritePos, solidSpriteSize, size,
											 0, c ? V4f(1, 1, 1, menuColor.w) : menuColor));
				labels.push_back({pos + 2, text, V4f(1, 1, 1, pauseAlpha)});
				return c && input.mouseUp(0);
			};

			auto rect = [&](v2f pos, v2f size) {
				pos.y = window.clientSize.y - pos.y - size.y;
				uiTiles.push_back(createTile(pos + size * 0.5f, solidSpritePos, solidSpriteSize,
											 size, 0, menuColor));
			};

			rect(menuPanelPos, menuPanelSize);
			if (button(menuPanelPos + 2, V2f(11 * 8, 22) + V2f(4), "Continue")) {
				if (currentState == State::pause) {
					currentState = State::game;
				}
			}
		}

		uiTiles.push_back(createTile((v2f)mousePos, offsetAtlasTile(0, 2), ATLAS_ENTRY_SIZE * 2, V2f(32.0f), 0, V4f(1,1,1,uiAlpha)));

		renderer.updateBuffer(tilesBuffer, uiTiles.data(), (u32)uiTiles.size() * sizeof(uiTiles[0]));
		renderer.setBlend(Blend::srcAlpha, Blend::invSrcAlpha, BlendOp::add);
		renderer.setMatrix(0, m4::translation(-1, -1, 0) * m4::scaling(2.0f / (v2f)window.clientSize, 1));
		renderer.bindBuffer(tilesBuffer, Stage::vs, 0);
		renderer.bindShader(tileShader);
		renderer.bindTexture(atlas, Stage::ps, 0);
		renderer.draw((u32)uiTiles.size() * 6);

		char buf[256];
		sprintf(buf, "Health: %u%%\nULT: %u%%\nCoins: %u\nKills: %u\nCurrent wave: %u\n%s", playerHealth, ultimateAttackPercent, coinsInWallet, botsKilled, currentWave, debugGod ? "God enabled" : "");
		labels.push_back({V2f(8, 64), buf, V4f(1,1,1,uiAlpha)});

		labels.push_back({(v2f)window.clientSize * 0.4f, "Press enter to start\n\nWASD - move\nLMB - shoot\nRMB - ultimate", V4f(1,1,1,menuAlpha)});

		sprintf(buf, "Kills: %u\nPress enter to restart", botsKilled);
		labels.push_back({(v2f)window.clientSize * 0.4f, buf, V4f(1,1,1,deathScreenAlpha)});

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
		drawText(renderer, labels, (v2f)window.clientSize);
		
		soundMutex.lock();
		for (auto sound : shotSounds) {
			sound.volume /= shotSounds.size();
			playingSounds.push_back(sound);
		}
		for (auto sound : explosionSounds) {
			sound.volume /= explosionSounds.size();
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

			volume *= sound.volume;

			memcpy(sound.channelVolume, &volume, sizeof(sound.channelVolume));
		}
		soundMutex.unlock();
	}

	void fillSoundBuffer(Audio &audio, s16 *subsample, u32 sampleCount) {
		PROFILE_FUNCTION;
		
		memset(subsample, 0, sampleCount * sizeof(*subsample) * 2);
		soundMutex.lock();
		for (auto &sound : playingSounds) {
			if (sound.buffer->data)
				addSoundSamples(audio.sampleRate, subsample, sampleCount, *sound.buffer, sound.cursor, sound.channelVolume, timeScale * sound.pitch, sound.looping);
		}
		if (playingMusic.buffer)
			addSoundSamples(audio.sampleRate, subsample, sampleCount, *playingMusic.buffer, playingMusic.cursor, 1.0f, timeScale);
		soundMutex.unlock();
	}
};
#include <map>
namespace GameApi {
static Profiler::Stats startStats;
static Profiler::Stats frameStats[256]{};
static Profiler::Stats *currentFrameStats = frameStats;
static ::Random random;
static u32 sortIndex = 0;
static GameState *gameInstance = 0;
static bool showOnlyCurrentFrame = true;
static s16 audioGraph[1024];
static s16 *audioGraphCursor = audioGraph;
static std::mutex audioMutex;
void fillStartInfo(StartInfo &info) {
}
void init(State &state) {
	GameState *gameInstance = new GameState;
	gameInstance->init();

	state.userData = gameInstance;
}
GameState &getGame(State &state) { return *(GameState *)state.userData; }
void start(State &state, Window &window, Renderer &renderer, Input &input, Time &time) {
	getGame(state).start(window, renderer, input, time);
}
void debugStart(State &, Window &window, Renderer &renderer, Input &input, Time &time, Profiler::Stats &&stats) {
	startStats = std::move(stats);
}
void debugReload(State &state) {
	getGame(state).debugReload();
}
void update(State &state, Window &window, Renderer &renderer, Input &input, Time &time) {
	getGame(state).update(window, renderer, input, time);
}
void debugUpdate(State &state, Window &window, Renderer &renderer, Input &input, Time &time, Profiler::Stats &&newFrameStats) {
	auto &game = getGame(state);
	*currentFrameStats = newFrameStats;
	if (++currentFrameStats == std::end(frameStats)) {
		currentFrameStats = frameStats;
	}
	if (input.keyDown(Key_f1)) {
		game.debugProfile.mode = (u8)((game.debugProfile.mode + 1) % 4);
	}
	UnorderedList<Label> labels;
	UnorderedList<Rect> rects;
	v2s mp = input.mousePosition;
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

		labels.push_back({V2f(8), debugLabel});

		int offset = sprintf(debugLabel, "debugValues:\n");
		auto append = [&](GameState::DebugValueEntry &v) {
			switch (v.type) {
				case GameState::DebugValueType::u1 : return sprintf(debugLabel + offset, "%s: %s\n"  , v.name, *(bool *)v.data ? "true" : "false");
				case GameState::DebugValueType::u8 : return sprintf(debugLabel + offset, "%s: %u\n"  , v.name, *(u8  *)v.data);
				case GameState::DebugValueType::u16: return sprintf(debugLabel + offset, "%s: %u\n"  , v.name, *(u16 *)v.data);
				case GameState::DebugValueType::u32: return sprintf(debugLabel + offset, "%s: %u\n"  , v.name, *(u32 *)v.data);
				case GameState::DebugValueType::u64: return sprintf(debugLabel + offset, "%s: %llu\n", v.name, *(u64 *)v.data);
				case GameState::DebugValueType::s8 : return sprintf(debugLabel + offset, "%s: %i\n"  , v.name, *(s8  *)v.data);
				case GameState::DebugValueType::s16: return sprintf(debugLabel + offset, "%s: %i\n"  , v.name, *(s16 *)v.data);
				case GameState::DebugValueType::s32: return sprintf(debugLabel + offset, "%s: %i\n"  , v.name, *(s32 *)v.data);
				case GameState::DebugValueType::s64: return sprintf(debugLabel + offset, "%s: %lli\n", v.name, *(s64 *)v.data);
				default:
					INVALID_CODE_PATH();
			}
		};
		for (auto &v : game.debugValues) {
			offset += append(v);
		}
		labels.push_back({v2f{1024, 8}, debugLabel});
#if 1
		audioMutex.lock();
		for (u32 i = 0; i < _countof(audioGraph); ++i) {
			s32 x = 16 + i;
			s32 h = 250 + audioGraph[frac(i + (audioGraphCursor - audioGraph), _countof(audioGraph))] * 250 / max<s16>();
			s32 y = window.clientSize.y - 16 - h;
			s32 w = 1;
			rects.push_back({{x, y}, {w, h}, V4f(1)});
		}
		audioMutex.unlock();
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
		
	v2s const letterSize{11, 22};

	auto contains = [](v2s pos, v2s size, v2s mp) {
		return pos.x <= mp.x && mp.x < pos.x + size.x && pos.y <= mp.y && mp.y < pos.y + size.y;
	};
	auto button = [&](v2s pos, v2s size) {
		bool c = contains(pos, size, mp);
		rects.push_back({pos, size, V4f(c ? 0.75f : 0.5f)});
		return c && input.mouseUp(0);
	};
	auto displayProfile = [&](Profiler::Stats &stats) {
		char debugLabel[1024 * 16];
		u32 threadCount = (u32)stats.entries.size();
		s32 tableWidth = letterSize.x * 12;
		s32 barWidth = 1000;
		v2s threadInfoPos{8, (s32)window.clientSize.y - letterSize.y * (s32)threadCount * 3 - 8};
		s32 y = threadInfoPos.y;
		
		s32 offset = 0;
		
		for (u32 i = 0; i < threadCount; ++i) {
			rects.push_back({threadInfoPos + v2s{tableWidth, letterSize.y * (s32)i * 3}, {barWidth, letterSize.y * 3}, (i & 1) ? V4f(0.5f) : V4f(0.45f)});
		}
		u32 worksDone = 0;
		for (u32 i = 0; i < (u32)stats.entries.size(); ++i) {
			if (i == 0)
				offset += sprintf(debugLabel + offset, "Main thread\n\n\n");
			else
				offset += sprintf(debugLabel + offset, "Thread %u:\n\n\n", i - 1);
			//s32 x = threadInfoPos.x + tableWidth;
			for (auto &stat : stats.entries[i]) {
				random.seed = 0;
				u32 seedIndex = 0;
				for (auto c : stat.name) {
					((char *)&random.seed)[seedIndex] ^= c;
					seedIndex = (seedIndex + 1) & 3;
				}

				s32 x = threadInfoPos.x + tableWidth + (s32)((stat.startUs - stats.startUs) * (u32)barWidth / stats.totalUs);
				s32 w = max((s32)(stat.totalUs * (u32)barWidth / stats.totalUs), 3);
				v2s pos = {x + 1, y + 1 + letterSize.y / 2 * stat.depth};
				v2s size = {w - 2, letterSize.y / 2 - 2};
				//rects.push_back({pos, size, V4f(hsvToRgb(time.time / 6, 1, 1), 0.75f)});
				//rects.push_back({pos, size, V4f(unpack(hsvToRgb(F32x4(time.time), F32x4(1), F32x4(1)))[time.frameCount & 3], 0.75f)});
				rects.push_back({pos, size, V4f(hsvToRgb(map(random.f32(), -1, 1, 0, 1), 0.5f, 1), 0.75f)});
				if (contains(pos, size, mp)) {
					labels.push_back({(v2f)mp, stat.name});
				}
				//x += w;
			}
			y += letterSize.y * 3;
			worksDone += (u32)stats.entries[i].size();
		}
		labels.push_back({(v2f)threadInfoPos, debugLabel});
	};
	auto displayInfo = [&](char const *title, Profiler::Stats &stats) {
		u32 threadCount = (u32)stats.entries.size();
		auto printStats = [&](List<Profiler::Stat> const &entries, char *buffer) {
			u32 offset = 0;
			for (auto const &stat : entries) {
				offset += sprintf(buffer + offset, "%35s: %4.1f%%, %10llu us, %4.1f%%, %10llu us\n",
									stat.name.data(), (f64)stat.selfUs / (stats.totalUs * threadCount) * 100, stat.selfUs,
									(f64)stat.totalUs / stats.totalUs * 100, stat.totalUs);
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

		List<Profiler::Stat> allEntries;
		
		for (auto &th : stats.entries) {
			for(auto &stat : th) {
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

		offset += printStats(allEntries, debugLabel + offset);
		labels.push_back({V2f(8), debugLabel});
	};
	if (game.debugProfile.mode == 2) {
		displayInfo("Update profile", newFrameStats);

		showOnlyCurrentFrame ^= input.keyDown(Key_f2);
		if (showOnlyCurrentFrame) {
			displayProfile(newFrameStats);
		} else {
			static bool wasHovering = false;
			bool hovering = false;
			for (s32 statIndex = 0; statIndex < _countof(frameStats); statIndex++) {
				auto &frameStat = frameStats[statIndex];
				u64 totalUs = 0;
				std::map<std::string, u64> list;
				for (auto &th : frameStat.entries) {
					for (auto &stat : th) {
						list[stat.name] += stat.selfUs;
						totalUs += stat.selfUs;
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
					s32 y = (s32)window.clientSize.y - 16 - totalHeight + ((s32)pastDuration * totalHeight / (s32)totalUs);
					s32 w = 2;
					s32 h = (s32)duration * totalHeight / (s32)totalUs;
					v2s pos = {x, y};
					v2s size = {w, h};
					rects.push_back({pos, size, V4f(hsvToRgb(map(random.f32(), -1, 1, 0, 1), 0.5f, 1), 1)});
					if (contains(pos, size, mp)) {
						hovering = true;
						labels.push_back({(v2f)mp, name});
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

void fillSoundBuffer(State &state, Audio &audio, s16 *subsample, u32 subsampleCount) {
	getGame(state).fillSoundBuffer(audio, subsample, subsampleCount);
	audioMutex.lock();
	while (subsampleCount) {
		u32 density = 10;
		s32 result = 0;
		s32 div = 0;
		while (subsampleCount && density--) {
			result += *subsample++ << 6;
			++div;
			--subsampleCount;
		}
		*audioGraphCursor = result / div;
		if (++audioGraphCursor >= std::end(audioGraph)) {
			audioGraphCursor = audioGraph;
		}
	}
	audioMutex.unlock();
}

void shutdown(State &state) { delete state.userData; }
} // namespace GameApi
  //#include "../../src/game_default_interface.h"