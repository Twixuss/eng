#pragma once
#include "common.h"
#include "window.h"
#include <vector>

#define ID(name)                           \
	struct name##Id {                      \
		u32 id = ~0u;                      \
		bool valid() { return id != ~0u; } \
	}
ID(Shader);
ID(Buffer);
ID(Texture);
ID(RenderTarget);

enum class Stage { vs, ps };
enum class Address { wrap, clamp, mirror, count };
enum class Filter { point_point, point_linear, linear_point, linear_linear, anisotropic, count };
enum class Blend { one, zero, srcAlpha, invSrcAlpha, dstAlpha, invDstAlpha, count };
enum class BlendOp { add, subtract, min, max, invSubtract, count };
enum class Format { UN_RGB8, F_R32, F_RGB32, F_RGBA16, count };

#define RNAME(name) CONCAT(_, name)

// clang-format off
#define R_INIT						R_DECORATE(void, init, (u32 sampleCount), (sampleCount))
#define R_SHUTDOWN					R_DECORATE(void, shutdown, (), ())
#define R_RESIZE					R_DECORATE(void, resize, (v2u clientSize), (clientSize))
#define R_PREPARE					R_DECORATE(void, prepare, (), ())
#define R_RENDER					R_DECORATE(void, render, (), ())
#define R_CLEAR_TARGET				R_DECORATE(void, clearRenderTarget, (RenderTargetId rt, v4 color), (rt, color))
#define R_CREATE_BUFFER				R_DECORATE(BufferId, createBuffer, (void const* data, u32 stride, u32 count), (data, stride, count))
#define R_CREATE_SHADER				R_DECORATE(ShaderId, createShader, (char const* path, StringView prefix), (path, prefix))
#define R_CREATE_TEXTURE_FROM_FILE	R_DECORATE(TextureId, createTextureFromFile, (char const* path, Address address, Filter filter), (path, address, filter))
#define R_CREATE_TEXTURE			R_DECORATE(TextureId, createTexture, (Format format, u32 width, u32 height, Address address, Filter filter), (format, width, height, address, filter))
#define R_CREATE_RT					R_DECORATE(RenderTargetId, createRenderTarget, (v2u size, Format format, u32 sampleCount), (size, format, sampleCount))
#define R_DRAW						R_DECORATE(void, draw, (u32 vertexCount, u32 offset), (vertexCount, offset))
#define R_UPDATE_BUFFER				R_DECORATE(void, updateBuffer, (BufferId buffer, void const* data, u32 size), (buffer, data, size))
#define R_RELEASE_SHADER			R_DECORATE(void, releaseShader, (ShaderId shader), (shader))
#define R_RELEASE_RT				R_DECORATE(void, releaseRenderTarget, (RenderTargetId rt), (rt))
#define R_BIND_SHADER				R_DECORATE(void, bindShader, (ShaderId shader), (shader))
#define R_BIND_BUFFER				R_DECORATE(void, bindBuffer, (BufferId buffer, Stage stage, u32 slot), (buffer, stage, slot))
#define R_BIND_RT					R_DECORATE(void, bindRenderTarget, (RenderTargetId rt), (rt))
#define R_BIND_RT_AS_TEXTURE		R_DECORATE(void, bindRenderTargetAsTexture, (RenderTargetId rt, Stage stage, u32 slot), (rt, stage, slot))
#define R_BIND_TEXTURE				R_DECORATE(void, bindTexture, (TextureId texture, Stage stage, u32 slot), (texture, stage, slot))
#define R_SET_MATRIX				R_DECORATE(void, setMatrix, (u32 slot, const m4& matrix), (slot, matrix))
#define R_SET_BLEND					R_DECORATE(void, setBlend, (Blend src, Blend dst, BlendOp op), (src, dst, op))
#define R_DISABLE_BLEND				R_DECORATE(void, disableBlend, (), ())
#define R_UPDATE_TEXTURE			R_DECORATE(void, updateTexture, (TextureId tex, void const* data), (tex, data))
// clang-format on

#define R_ALLFUNS               \
	R_INIT;                     \
	R_SHUTDOWN;                 \
	R_RESIZE;                   \
	R_PREPARE;                  \
	R_RENDER;                   \
	R_CLEAR_TARGET;             \
	R_CREATE_SHADER;            \
	R_CREATE_RT;                \
	R_CREATE_TEXTURE_FROM_FILE; \
	R_CREATE_TEXTURE;           \
	R_CREATE_BUFFER;            \
	R_UPDATE_BUFFER;            \
	R_BIND_RT;                  \
	R_BIND_RT_AS_TEXTURE;       \
	R_BIND_TEXTURE;             \
	R_BIND_BUFFER;              \
	R_BIND_SHADER;              \
	R_RELEASE_SHADER;           \
	R_RELEASE_RT;               \
	R_SET_MATRIX;               \
	R_SET_BLEND;                \
	R_DISABLE_BLEND;            \
	R_DRAW;                     \
	R_UPDATE_TEXTURE

struct Renderer {
	struct ExportedFunction {
		char const* name;
		u32 memberOffset;
	};
	using ExportedFunctions = std::vector<ExportedFunction>;

	// function pointers
#define ADD_ARG(...)						 (Renderer * renderer, __VA_ARGS__)
#define R_DECORATE(type, name, args, params) type(*RNAME(name)) ADD_ARG args
	union {
		void* functions;
		struct {
			R_ALLFUNS;
		};
	};
#undef R_DECORATE
#undef ADD_ARG

	// functions
#define ADD_ARG(...) (this, __VA_ARGS__)
#define R_DECORATE(type, name, args, params) \
	FORCEINLINE type name args { return RNAME(name) ADD_ARG params; }
	R_ALLFUNS;
#undef R_DECORATE
#undef ADD_ARG

	FORCEINLINE void draw(u32 Count) { return draw(Count, 0); }
	FORCEINLINE ShaderId createShader(char const* path) { return createShader(path, {}); }
	FORCEINLINE TextureId createTexture(char const* path, Address address, Filter filter) {
		return createTextureFromFile(path, address, filter);
	}
	FORCEINLINE RenderTargetId createRenderTarget(v2u size) { return createRenderTarget(size, Format::UN_RGB8, 1); }
	FORCEINLINE RenderTargetId createRenderTarget(v2u size, u32 sampleCount) {
		return createRenderTarget(size, Format::UN_RGB8, sampleCount);
	}
	FORCEINLINE RenderTargetId createRenderTarget(v2u size, Format format) {
		return createRenderTarget(size, format, 1);
	}
	FORCEINLINE void release(ShaderId shader) { return releaseShader(shader); }
	FORCEINLINE void release(RenderTargetId rt) { return releaseRenderTarget(rt); }

	Window* window;
	u32 drawCalls;
};
