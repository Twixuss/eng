#pragma once
#include "common.h"
#include "window.h"

enum class RenderingApi {
	d3d11, opengl
};

ENG_API char const *getRendererName(RenderingApi);

#define ID(name)                           \
	struct name##Id {                      \
		u32 id = ~0u;                      \
		bool valid() { return id != ~0u; } \
	}
ID(Shader);
ID(Buffer);
ID(Texture);
ID(RenderTarget);
#undef ID

struct ShaderMacro {
	char const *name;
	char const *value;
};

enum class Stage { vs, ps };
enum class Address { wrap, clamp, mirror, border, count };
enum class Filter { point_point, point_linear, linear_point, linear_linear, anisotropic, count };
enum class Blend { one, zero, srcAlpha, invSrcAlpha, dstAlpha, invDstAlpha, count };
enum class BlendOp { add, subtract, min, max, invSubtract, count };
enum class Format { UN_RGBA8, UNS_RGBA8, F_R32, F_RGB32, F_RGBA16, count };
enum class Topology { TriangleList, LineList, count };

#define RNAME(name) CONCAT(_r_, name)

// clang-format off
#define R_INIT						R_DECORATE(void, init, (Window &window, u32 sampleCount, Format backBufferFormat), (window, sampleCount, backBufferFormat))
#define R_SHUTDOWN					R_DECORATE(void, shutdown, (), ())
#define R_RESIZE					R_DECORATE(void, resize, (Window &window), (window))
#define R_PRESENT					R_DECORATE(void, present, (Window &window, Time const &time), (window, time))
#define R_GET_DRAW_COUNT			R_DECORATE(u32, getDrawCount, (), ())
#define R_CLEAR_TARGET				R_DECORATE(void, clearRenderTarget, (RenderTargetId rt, v4f color), (rt, color))
#define R_CREATE_BUFFER				R_DECORATE(BufferId, createBuffer, (void const* data, u32 stride, u32 count), (data, stride, count))
#define R_CREATE_SHADER				R_DECORATE(ShaderId, createShader, (char const* path, Span<ShaderMacro const> macros), (path, macros))
#define R_CREATE_TEXTURE_FROM_FILE	R_DECORATE(TextureId, createTextureFromFile, (char const* path, Address address, Filter filter, v4f borderColor), (path, address, filter, borderColor))
#define R_CREATE_TEXTURE			R_DECORATE(TextureId, createTexture, (u32 width, u32 height, Format format, Address address, Filter filter, v4f borderColor), (width, height, format, address, filter, borderColor))
#define R_CREATE_RT					R_DECORATE(RenderTargetId, createRenderTarget, (v2u size, Format format, u32 sampleCount, Address address, Filter filter, v4f borderColor), (size, format, sampleCount, address, filter, borderColor))
#define R_DRAW						R_DECORATE(void, draw, (u32 vertexCount, u32 offset), (vertexCount, offset))
#define R_UPDATE_BUFFER				R_DECORATE(void, updateBuffer, (BufferId buffer, void const* data, u32 size), (buffer, data, size))
#define R_RELEASE_SHADER			R_DECORATE(void, releaseShader, (ShaderId shader), (shader))
#define R_RELEASE_TEXTURE			R_DECORATE(void, releaseTexture, (TextureId texture), (texture))
#define R_RELEASE_RT				R_DECORATE(void, releaseRenderTarget, (RenderTargetId rt), (rt))
#define R_BIND_SHADER				R_DECORATE(void, bindShader, (ShaderId shader), (shader))
#define R_BIND_BUFFER				R_DECORATE(void, bindBuffer, (BufferId buffer, Stage stage, u32 slot), (buffer, stage, slot))
#define R_BIND_RT					R_DECORATE(void, bindRenderTargets, (Span<RenderTargetId> rts, bool updateViewport), (rts, updateViewport))
#define R_BIND_RT_AS_TEXTURE		R_DECORATE(void, bindRenderTargetsAsTextures, (Span<RenderTargetId> rts, Stage stage, u32 slot), (rts, stage, slot))
#define R_BIND_TEXTURE				R_DECORATE(void, bindTexture, (TextureId texture, Stage stage, u32 slot), (texture, stage, slot))
#define R_UNBIND_TEXTURES			R_DECORATE(void, unbindTextures, (u32 count, Stage stage, u32 slot), (count, stage, slot))
#define R_SET_MATRIX				R_DECORATE(void, setMatrix, (u32 slot, const m4& value), (slot, value))
#define R_SET_V4F					R_DECORATE(void, setV4f, (u32 slot, v4f value), (slot, value))
#define R_SET_BLEND					R_DECORATE(void, setBlend, (Blend src, Blend dst, BlendOp op), (src, dst, op))
#define R_SET_TOPOLOGY				R_DECORATE(void, setTopology, (Topology topology), (topology))
#define R_DISABLE_BLEND				R_DECORATE(void, disableBlend, (), ())
#define R_UPDATE_TEXTURE			R_DECORATE(void, updateTexture, (TextureId tex, void const* data), (tex, data))
// clang-format on

#define R_ALLFUNS               \
	R_INIT;                     \
	R_SHUTDOWN;                 \
	R_RESIZE;                   \
	R_PRESENT;                  \
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
	R_UNBIND_TEXTURES;          \
	R_RELEASE_SHADER;           \
	R_RELEASE_TEXTURE;          \
	R_RELEASE_RT;               \
	R_SET_MATRIX;               \
	R_SET_V4F;					\
	R_SET_BLEND;                \
	R_SET_TOPOLOGY;             \
	R_DISABLE_BLEND;            \
	R_DRAW;                     \
	R_GET_DRAW_COUNT;           \
	R_UPDATE_TEXTURE

union Renderer {
	//struct ExportedFunction {
	//	char const* name;
	//	u32 memberOffset;
	//};
	//using ExportedFunctions = std::vector<ExportedFunction>;

	// function pointers
#define R_DECORATE(type, name, args, params) type(*RNAME(name)) args
	void* functions;
	struct {
		R_ALLFUNS;
	};
#undef R_DECORATE

	// functions
#define R_DECORATE(type, name, args, params) \
	FORCEINLINE type name args { return RNAME(name) params; }
	R_ALLFUNS;
#undef R_DECORATE

	FORCEINLINE void setValue(u32 slot, m4 const &value) { setMatrix(slot, value); }
	FORCEINLINE void setValue(u32 slot, v4f value) { setV4f(slot, value); }
	FORCEINLINE void draw(u32 count) { return draw(count, 0); }
	FORCEINLINE ShaderId createShader(char const* path) { return createShader(path, {}); }
	FORCEINLINE TextureId createTextureFromFile(char const* path, Address address, Filter filter) { return createTextureFromFile(path, address, filter, {}); }
	FORCEINLINE TextureId createTexture(char const* path, Address address, Filter filter) { return createTextureFromFile(path, address, filter); }
	FORCEINLINE TextureId createTexture(u32 width, u32 height, Format format, Address address, Filter filter) { return createTexture(width, height, format, address, filter, {}); }
	FORCEINLINE TextureId createTexture(v2u size, Format format, Address address, Filter filter, v4f borderColor = {}) { return createTexture(size.x, size.y, format, address, filter, borderColor); }
	FORCEINLINE RenderTargetId createRenderTarget(v2u size, Format format, Address address, Filter filter, v4f borderColor = {}) { return createRenderTarget(size, format, 1, address, filter, borderColor); }
	FORCEINLINE RenderTargetId createRenderTarget(v2u size, Format format, u32 sampleCount, Address address, Filter filter) { return createRenderTarget(size, format, sampleCount, address, filter, {}); }
	FORCEINLINE void bindTexture(RenderTargetId rt, Stage stage, u32 slot) { bindRenderTargetsAsTextures(Span(&rt, 1), stage, slot); }
	FORCEINLINE void bindTextures(Span<RenderTargetId> rts, Stage stage, u32 slot) { bindRenderTargetsAsTextures(rts, stage, slot); }
	FORCEINLINE void bindRenderTargets(Span<RenderTargetId> rts) { bindRenderTargets(rts, true); }
	FORCEINLINE void bindRenderTarget(RenderTargetId rt, bool updateViewport = true) { bindRenderTargets(Span(&rt, 1), updateViewport); }
	FORCEINLINE void release(ShaderId shader) { return releaseShader(shader); }
	FORCEINLINE void release(TextureId texture) { return releaseTexture(texture); }
	FORCEINLINE void release(RenderTargetId rt) { return releaseRenderTarget(rt); }

	//Window* window;
	//u32 drawCalls;
};
