#include "game.h"
#include <string.h>
#include <string>
#include <variant>
#include <vector>
template <class T>
T* malloc(size_t count) {
	return (T*)_aligned_malloc(count * sizeof(T), alignof(T));
}

struct ParsedOBJ {
	struct Index {
		struct P {
			u32 position;
		};
		struct PT {
			u32 position;
			u32 texCoord;
		};
		struct PN {
			u32 position;
			u32 normal;
		};
		struct PTN {
			u32 position;
			u32 texCoord;
			u32 normal;
		};
	};
	enum class IndexType : u32 { Null, P, PT, PN, PTN };
	struct Indices {
		IndexType type = IndexType::Null;
		u32 indexSize = 0;
		u32 count = 0;
		union {
			void* data = 0;
			Index::P* p;
			Index::PT* pt;
			Index::PN* pn;
			Index::PTN* ptn;
		};
		void calcSize() {
			indexSize = [this]() -> u32 {
				switch (type) {
					case ParsedOBJ::IndexType::P: return sizeof(ParsedOBJ::Index::P);
					case ParsedOBJ::IndexType::PT: return sizeof(ParsedOBJ::Index::PT);
					case ParsedOBJ::IndexType::PN: return sizeof(ParsedOBJ::Index::PN);
					case ParsedOBJ::IndexType::PTN: return sizeof(ParsedOBJ::Index::PTN);
					default: INVALID_CODE_PATH("unresolved index type");
				}
			}();
		}
	};
	Span<v3> positions;
	Span<v3> normals;
	Span<v2> texCoords;
	Indices indices;

	bool valid() { return positions.data() != 0; }
	void release() {
		if (positions.data())
			_aligned_free(positions.data());
		if (normals.data())
			_aligned_free(normals.data());
		if (texCoords.data())
			_aligned_free(texCoords.data());
		free(indices.data);
		positions = {};
		normals = {};
		texCoords = {};
		indices = {};
	}
};

ParsedOBJ loadOBJ(char const* path) {
	PROFILE_FUNCTION;

	auto file = readFile(path);
	if (!file.valid())
		return {};

	char* c = file.data.begin();
	char* end = file.data.end();

	auto skipToNextLine = [&c]() {
		while (*c != '\n')
			++c;
		++c;
	};

	ParsedOBJ result;
	u32 positionCount = 0;
	u32 texCoordCount = 0;
	u32 normalCount = 0;
	u32 triangleCount = 0;

	PROFILE_BEGIN(prepare, "Prepare");
	while (c < end) {
		if (strncmp(c, "v ", 2) == 0) {
			++positionCount;
		} else if (strncmp(c, "vt ", 3) == 0) {
			++texCoordCount;
		} else if (strncmp(c, "vn ", 3) == 0) {
			++normalCount;
		} else if (strncmp(c, "f ", 2) == 0) {
			u32 spaceCount = 0;
			for (char* s = c + 2; *s != '\n'; ++s) {
				if (*s == ' ')
					++spaceCount;
			}
			ASSERT(spaceCount >= 2);
			triangleCount += spaceCount - 1;
			if (result.indices.type == ParsedOBJ::IndexType::Null) {
				c += 2;
				while (*c != '/' && *c != ' ')
					++c;
				if (*c == ' ') {
					result.indices.type = ParsedOBJ::IndexType::P;
				} else if (*++c == '/') {
					result.indices.type = ParsedOBJ::IndexType::PN;
				} else {
					while (*c != '/' && *c != ' ')
						++c;
					if (*c == ' ') {
						result.indices.type = ParsedOBJ::IndexType::PT;
					} else if (*c == '/') {
						result.indices.type = ParsedOBJ::IndexType::PTN;
					} else {
						INVALID_CODE_PATH("bad index");
					}
				}
			}
		}
		skipToNextLine();
	}
	PROFILE_END(prepare);

	result.indices.count = triangleCount * 3;

	result.indices.calcSize();

	result.indices.data = malloc(result.indices.indexSize * result.indices.count);
	result.positions = {malloc<v3>(positionCount), positionCount};
	result.normals = {malloc<v3>(normalCount), normalCount};
	result.texCoords = {malloc<v2>(texCoordCount), texCoordCount};

	auto position = result.positions.begin();
	auto normal = result.normals.begin();
	auto texCoord = result.texCoords.begin();

	u32 currentIndex = 0;

	auto parseFloat = [&c]() {
		char buf[12];
		char* p = buf;
		while (*c != ' ' && *c != '\n') {
			*p++ = *c++;
		}
		*p = 0;
		++c;
		return (f32)atof(buf);
	};
	auto parseIndex = [&c]() {
		char buf[12];
		char* p = buf;
		while (isdigit(*c)) {
			*p++ = *c++;
		}
		*p = 0;
		return (u32)(atoi(buf) - 1);
	};

	c = file.data.begin();

	PROFILE_BEGIN(parse, "Parse");
	while (c < end) {
		if (strncmp(c, "v ", 2) == 0) {
			c += 2;
			*position++ = {parseFloat(), parseFloat(), parseFloat()};
			continue;
		} else if (strncmp(c, "vt ", 3) == 0) {
			c += 3;
			*texCoord++ = {parseFloat(), parseFloat()};
			continue;
		} else if (strncmp(c, "vn ", 3) == 0) {
			c += 3;
			*normal++ = {parseFloat(), parseFloat(), parseFloat()};
			continue;
		} else if (strncmp(c, "f ", 2) == 0) {
			c += 2;
			struct Index {
				ParsedOBJ::IndexType type = ParsedOBJ::IndexType::Null;
				union {
					ParsedOBJ::Index::P p;
					ParsedOBJ::Index::PT pt;
					ParsedOBJ::Index::PN pn;
					ParsedOBJ::Index::PTN ptn;
				};
				Index() = default;
				Index(ParsedOBJ::Index::P p) : type(ParsedOBJ::IndexType::P), p(p) {}
				Index(ParsedOBJ::Index::PT pt) : type(ParsedOBJ::IndexType::PT), pt(pt) {}
				Index(ParsedOBJ::Index::PN pn) : type(ParsedOBJ::IndexType::PN), pn(pn) {}
				Index(ParsedOBJ::Index::PTN ptn) : type(ParsedOBJ::IndexType::PTN), ptn(ptn) {}
			};
			u32 firstIndex = 0, lastIndex = 0;
#define TRIANGULATE_POLY(type)                                                 \
	if (i > 2) {                                                               \
		result.indices.type[currentIndex++] = result.indices.type[firstIndex]; \
		result.indices.type[currentIndex++] = result.indices.type[lastIndex];  \
	}
			for (u32 i = 0;; ++i) {
				u32 positionIndex = parseIndex();
				if (*c == ' ' || *c == '\n') {
					ASSERT(result.indices.type == ParsedOBJ::IndexType::P);
					TRIANGULATE_POLY(p);
					ParsedOBJ::Index::P index;
					index.position = positionIndex;
					result.indices.p[currentIndex] = index;
				} else if (*++c == '/') {
					ASSERT(result.indices.type == ParsedOBJ::IndexType::PN);
					++c;
					u32 normalIndex = parseIndex();
					TRIANGULATE_POLY(pn);
					ParsedOBJ::Index::PN index;
					index.position = positionIndex;
					index.normal = normalIndex;
					result.indices.pn[currentIndex] = index;
				} else {
					u32 texCoordIndex = parseIndex();
					if (*c == ' ' || *c == '\n') {
						ASSERT(result.indices.type == ParsedOBJ::IndexType::PT);
						TRIANGULATE_POLY(pt);
						ParsedOBJ::Index::PT index;
						index.position = positionIndex;
						index.texCoord = texCoordIndex;
						result.indices.pt[currentIndex] = index;
					} else if (*c++ == '/') {
						ASSERT(result.indices.type == ParsedOBJ::IndexType::PTN);
						u32 normalIndex = parseIndex();
						TRIANGULATE_POLY(ptn);
						ParsedOBJ::Index::PTN index;
						index.position = positionIndex;
						index.texCoord = texCoordIndex;
						index.normal = normalIndex;
						result.indices.ptn[currentIndex] = index;
					} else {
						INVALID_CODE_PATH("bad index");
					}
				}
				if (i == 0) {
					firstIndex = currentIndex;
				}
				lastIndex = currentIndex;
				++currentIndex;
				if (*c++ == '\n') {
					break;
				}
			}
#undef TRIANGULATE_POLY
			continue;
		} else {
			skipToNextLine();
			continue;
		}
	}
	PROFILE_END(parse);

	file.release();

	return result;
}

template <class Vertex>
struct LoadedMesh {
	std::vector<Vertex> vertices;
	std::vector<u32> indices;
};

template <class Vertex>
LoadedMesh<Vertex> convertOBJtoMesh(ParsedOBJ const& obj) {
	PROFILE_FUNCTION;
	LoadedMesh<Vertex> result;
	result.indices.reserve(obj.indices.count);
	result.vertices.reserve(max(max(obj.positions.size(), obj.texCoords.size()), obj.normals.size()));
	for (u32 i = 0; i < obj.positions.size(); ++i) {
		Vertex v;
		v.position = obj.positions[i];
		result.vertices.push_back(v);
	}
	switch (obj.indices.type) {
		case ParsedOBJ::IndexType::PTN: {
			for (u32 i = 0; i < obj.indices.count; ++i) {
				auto index = obj.indices.ptn[i];
				Vertex candidate;
				candidate.position = obj.positions[index.position];
				candidate.normal = obj.normals[index.normal];
				candidate.texCoord = obj.texCoords[index.texCoord];
				Vertex& existing = result.vertices[index.position];
				if (existing.normal == decltype(existing.normal){}) {
					// initialize
					existing.normal = candidate.normal;
					existing.texCoord = candidate.texCoord;
					result.indices.push_back(index.position);
				} else if (lengthSqr(existing.normal - candidate.normal) < 0.000001f &&
						   lengthSqr(existing.texCoord - candidate.texCoord) < 0.000001f) {
					// use vertex with same normal and texCoord
					result.indices.push_back(index.position);
				} else {
					// push vertex with new normal and texCoord
					result.indices.push_back(u32(result.vertices.size()));
					result.vertices.push_back(candidate);
				}
			}
			break;
		}
		case ParsedOBJ::IndexType::PN: {
			for (u32 i = 0; i < obj.indices.count; ++i) {
				auto index = obj.indices.pn[i];
				Vertex candidate;
				candidate.position = obj.positions[index.position];
				candidate.normal = obj.normals[index.normal];
				Vertex& existing = result.vertices[index.position];
				if (existing.normal == decltype(existing.normal){}) {
					// initialize
					existing.normal = candidate.normal;
					result.indices.push_back(index.position);
				} else if (lengthSqr(existing.normal - candidate.normal) < 0.000001f) {
					// use vertex with same normal
					result.indices.push_back(index.position);
				} else {
					// push vertex with new normal
					result.indices.push_back(u32(result.vertices.size()));
					result.vertices.push_back(candidate);
				}
			}
			break;
		}
		case ParsedOBJ::IndexType::PT: {
			for (u32 i = 0; i < obj.indices.count; ++i) {
				auto index = obj.indices.pt[i];
				Vertex candidate;
				candidate.position = obj.positions[index.position];
				candidate.texCoord = obj.texCoords[index.texCoord];
				Vertex& existing = result.vertices[index.position];
				if (existing.texCoord == decltype(existing.texCoord){}) {
					// initialize
					existing.texCoord = candidate.texCoord;
					result.indices.push_back(index.position);
				} else if (lengthSqr(existing.texCoord - candidate.texCoord) < 0.000001f) {
					// use vertex with same texCoord
					result.indices.push_back(index.position);
				} else {
					// push vertex with new texCoord
					result.indices.push_back(u32(result.vertices.size()));
					result.vertices.push_back(candidate);
				}
			}
			break;
		}
		case ParsedOBJ::IndexType::P: {
			for (u32 i = 0; i < obj.indices.count; ++i) {
				result.indices.push_back(obj.indices.p[i].position);
			}
			break;
		}
		default: {
			INVALID_CODE_PATH("not implemented");
		}
	}
	for (auto& v : result.vertices) {
		v.position.x *= -1;
	}
	for (u32 i = 0; i < result.indices.size(); i += 3) {
		std::swap(result.indices[i], result.indices[i + 1]);
	}
	return result;
}

struct GAME_API Game {
	static constexpr i32 w = 128;
	Mesh* m;
	Game() {
		PROFILE_FUNCTION;
		auto testShader = Renderer::createShader(LDATA "shaders/test.shader");
		Renderer::bindShader(testShader);

		auto obj = loadOBJ(DATA "mesh/untitled.obj");
		ASSERT(obj.valid());

		struct Vertex {
			v3 position;
			v3 normal;
			v2 texCoord;
		};

		auto mesh = convertOBJtoMesh<Vertex>(obj);

		obj.release();

		m = Mesh::create();
		m->setVertices(mesh.vertices.data(), sizeof(mesh.vertices[0]), (u32)mesh.vertices.size());
		m->setIndices(mesh.indices.data(), sizeof(mesh.indices[0]), (u32)mesh.indices.size());
	}
	m4 cameraProjection;
	v3 camPos{0, 0, -(w + 10)};
	v3 camRot{};
	u32* backBuffer = 0;
	void resize() {
		delete backBuffer;
		backBuffer = new u32[Window::getClientSize().x * Window::getClientSize().y];
		cameraProjection = m4::perspective(aspectRatio(Window::getClientSize()), PI / 2, .01f, 1000.f);
	}
	void update() {
		PROFILE_FUNCTION;

		camRot += V3(-Input::mouseDelta().y, Input::mouseDelta().x, 0) * .001f;

		v3 mov;
		mov.x = (Input::keyHeld('D') - Input::keyHeld('A')) * Time::frameTime * 10.f;
		mov.y = (Input::keyHeld('E') - Input::keyHeld('Q')) * Time::frameTime * 10.f;
		mov.z = (Input::keyHeld('W') - Input::keyHeld('S')) * Time::frameTime * 10.f;
		mov = m4::rotationZXY(-camRot) * mov;

		camPos += mov;

		m4 cameraMatrix = cameraProjection * m4::rotationYXZ(camRot) * m4::translation(-camPos);

		auto cs = Window::getClientSize();
		PerfTimer timer;
		u64 begin = __rdtsc();
		for (u32 y = 0; y < cs.y; ++y) {
			for (u32 x = 0; x < cs.x; ++x) {
#if 1
				u8 c = u8(voronoi(V4i(x, y, Time::time * 25, Time::time), 16) * 255);
				backBuffer[y * cs.x + x] = u32(c | (c << 8) | (c << 16) | (c << 24));
#else
#if 1
				Renderer::setMatrix(Renderer::MatrixType::mvp,
									cameraMatrix * m4::translation(x, y, 0) *
										m4::scaling(1, 1, voronoi(V3i(x, y, Time::time * 25), 16) * 64));
				Renderer::drawMesh(m);
#else
				if (voronoi(V3i(x, y, Time::time * 16), 16) < 0.25f) {
					Renderer::setMatrix(Renderer::MatrixType::mvp, cameraMatrix * m4::translation(x, y, 0));
					Renderer::drawMesh(m);
				}
#endif
#endif
			}
		}
		u64 end = __rdtsc();
		u64 total = end - begin;
		Renderer::fillRenderTarget(backBuffer);
		printf("time: %fms; cy: %llu; cy/e: %llu\n", timer.getMilliseconds(), total, total / (cs.x * cs.y));
	}
	~Game() {}
};
namespace GameAPI {
WindowCreationInfo getWindowInfo() {
	WindowCreationInfo info{};
	info.resizeable = true;
	info.clientSize = {256, 256};
	return info;
}
Game* start() { return new Game; }
void update(Game& game) { return game.update(); }
void resize(Game& game) { return game.resize(); }
void shutdown(Game& game) { delete &game; }
} // namespace GameAPI