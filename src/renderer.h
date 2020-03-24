#pragma once
#include "common.h"
#include "window.h"
#include <vector>

struct Shader;
struct Mesh;

namespace Renderer {

enum class MatrixType { mvp };

#define R_DECORATE(type, name) extern ENG_API type(*name)

#define R_INIT				 R_DECORATE(void, init)()
#define R_SHUTDOWN			 R_DECORATE(void, shutdown)()
#define R_RESIZE			 R_DECORATE(void, resize)(v2u clientSize)
#define R_PREPARE			 R_DECORATE(void, prepare)()
#define R_RENDER			 R_DECORATE(void, render)()
#define R_CREATE_SHADER		 R_DECORATE(Shader*, createShader)(wchar const* path)
#define R_CREATE_MESH		 R_DECORATE(Mesh*, createMesh)()
#define R_BIND_SHADER		 R_DECORATE(void, bindShader)(Shader * shader)
#define R_SET_MESH_VERTICES	 R_DECORATE(void, setMeshVertices)(Mesh * mesh, void* data, u32 stride, u32 count)
#define R_SET_MESH_INDICES	 R_DECORATE(void, setMeshIndices)(Mesh * mesh, void* data, u32 stride, u32 count)
#define R_DRAW_MESH			 R_DECORATE(void, drawMesh)(Mesh * mesh)
#define R_SET_MATRIX		 R_DECORATE(void, setMatrix)(Renderer::MatrixType type, const m4& matrix)
#define R_FILL_RENDER_TARGET R_DECORATE(void, fillRenderTarget)(u32 const* data)

#define R_ALLFUNS         \
	R_INIT;               \
	R_SHUTDOWN;           \
	R_RESIZE;             \
	R_PREPARE;            \
	R_RENDER;             \
	R_CREATE_SHADER;      \
	R_CREATE_MESH;        \
	R_BIND_SHADER;        \
	R_SET_MESH_VERTICES;  \
	R_SET_MESH_INDICES;   \
	R_DRAW_MESH;          \
	R_FILL_RENDER_TARGET; \
	R_SET_MATRIX

R_ALLFUNS;

struct ProcEntry {
	char const* name;
	void** procAddress;
};
using ProcEntries = std::vector<Renderer::ProcEntry>;

}; // namespace Renderer

struct ENG_API Shader {};

struct ENG_API Mesh {
	static Mesh* create();
	inline void setVertices(void* data, u32 stride, u32 count) { Renderer::setMeshVertices(this, data, stride, count); }
	inline void setIndices(void* data, u32 stride, u32 count) { Renderer::setMeshIndices(this, data, stride, count); }
};
