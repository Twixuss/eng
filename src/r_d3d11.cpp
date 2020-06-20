#include "common.h"
#include "renderer.h"
#include "win32_common.h"
#include "window.h"

#define D3D11_NO_HELPERS

#pragma warning(push, 0)
#include "../dep/Microsoft DirectX SDK/Include/D3DX11.h"
#include <d3d11.h>
#include <d3dcompiler.h>

#ifndef D3D_COMPILE_STANDARD_FILE_INCLUDE
#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude *)(UINT_PTR)1)
#endif

#include <vector>
#pragma warning(pop)

namespace D3D11 {

static IDXGISwapChain *swapChain;
static ID3D11Device *device;
static ID3D11DeviceContext *immediateContext;
static u32 drawCalls;

struct VS {
	using Type = ID3D11VertexShader *;
	static constexpr auto name = "vs";
	static constexpr auto target = "vs_5_0";
	static constexpr auto create = &ID3D11Device::CreateVertexShader;
};

struct PS {
	using Type = ID3D11PixelShader *;
	static constexpr auto name = "ps";
	static constexpr auto target = "ps_5_0";
	static constexpr auto create = &ID3D11Device::CreatePixelShader;
};

template <class Shader>
static typename Shader::Type _createShader(StringView src, char const *name, D3D_SHADER_MACRO const *defines) {
	ID3DBlob *code{};
	ID3DBlob *messages{};
	HRESULT compileResult = D3DCompile(src.data(), src.size(), name, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main",
									   Shader::target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &messages);
	if (messages) {
		Log::warn((char const *)messages->GetBufferPointer());
		messages->Release();
	}
	if (FAILED(compileResult)) {
		Log::print("Name: {}", name);
		Log::print("Defines:");
		for (auto d = defines; d->Name != 0; ++d) {
			Log::print("    {}: {}", d->Name, d->Definition);
		}
		Log::print("Source: {}", src);
		INVALID_CODE_PATH("shader compilation failed");
	}
	typename Shader::Type shader = 0;
	DHR((device->*Shader::create)(code->GetBufferPointer(), code->GetBufferSize(), 0, &shader));
	ASSERT(shader);
	code->Release();
	return shader;
}

#define CREATE_SHADER(createVertexShader, VS)                                                                       \
	static FORCEINLINE auto createVertexShader(StringView src, char const *name, D3D_SHADER_MACRO const *defines) { \
		PROFILE_FUNCTION;                                                                                           \
		return _createShader<VS>(src, name, defines);                                                               \
	}
CREATE_SHADER(createVertexShader, VS)
CREATE_SHADER(createPixelShader, PS)
#undef CREATE_SHADER

ID3D11Buffer *createBuffer(UINT bindFlags, D3D11_USAGE usage, UINT cpuAccess, UINT size, UINT stride, UINT misc,
						   void const *initialData) {
	PROFILE_FUNCTION;
	D3D11_BUFFER_DESC desc{};
	desc.BindFlags = bindFlags;
	desc.Usage = usage;
	desc.CPUAccessFlags = cpuAccess;
	desc.ByteWidth = size;
	desc.StructureByteStride = stride;
	desc.MiscFlags = misc;
	ID3D11Buffer *result;
	if (initialData) {
		D3D11_SUBRESOURCE_DATA d3dInitialData;
		d3dInitialData.pSysMem = initialData;
		DHR(device->CreateBuffer(&desc, &d3dInitialData, &result));
	} else {
		DHR(device->CreateBuffer(&desc, 0, &result));
	}
	return result;
}
ID3D11BlendState *createBlend(D3D11_BLEND_OP op, D3D11_BLEND src, D3D11_BLEND dst) {
	D3D11_BLEND_DESC desc{};
	desc.RenderTarget[0].BlendEnable = true;
	desc.RenderTarget[0].BlendOp = op;
	desc.RenderTarget[0].SrcBlend = src;
	desc.RenderTarget[0].DestBlend = dst;
	desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	ID3D11BlendState *result;
	DHR(device->CreateBlendState(&desc, &result));
	return result;
}
ID3D11RasterizerState *createRasterizer(D3D11_PRIMITIVE_TOPOLOGY topology) {
	D3D11_RASTERIZER_DESC desc{};
	desc.CullMode = D3D11_CULL_BACK;
	desc.DepthClipEnable = true;
	desc.FillMode = D3D11_FILL_SOLID;
	ID3D11RasterizerState *result;
	DHR(device->CreateRasterizerState(&desc, &result));
	return result;
}

D3D11_TEXTURE_ADDRESS_MODE cvtAddress(Address address) {
	switch (address) {
		case Address::wrap: return D3D11_TEXTURE_ADDRESS_WRAP;
		case Address::clamp: return D3D11_TEXTURE_ADDRESS_CLAMP;
		case Address::mirror: return D3D11_TEXTURE_ADDRESS_MIRROR;
		case Address::border: return D3D11_TEXTURE_ADDRESS_BORDER;
		default: INVALID_CODE_PATH("bad address");
	}
}
D3D11_FILTER cvtFilter(Filter filter) {
	switch (filter) {
		case Filter::point_point: return D3D11_FILTER_MIN_MAG_MIP_POINT;
		case Filter::point_linear: return D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
		case Filter::linear_point: return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		case Filter::linear_linear: return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		case Filter::anisotropic: return D3D11_FILTER_ANISOTROPIC;
		default: INVALID_CODE_PATH("bad filter");
	}
}
D3D11_BLEND_OP cvtBlendOp(BlendOp op) {
	switch (op) {
		case BlendOp::add: return D3D11_BLEND_OP_ADD;
		case BlendOp::subtract: return D3D11_BLEND_OP_SUBTRACT;
		case BlendOp::min: return D3D11_BLEND_OP_MIN;
		case BlendOp::max: return D3D11_BLEND_OP_MAX;
		case BlendOp::invSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
		default: INVALID_CODE_PATH("bad blend op");
	}
}
D3D11_BLEND cvtBlend(Blend blend) {
	switch (blend) {
		case Blend::one: return D3D11_BLEND_ONE;
		case Blend::zero: return D3D11_BLEND_ZERO;
		case Blend::srcAlpha: return D3D11_BLEND_SRC_ALPHA;
		case Blend::invSrcAlpha: return D3D11_BLEND_INV_SRC_ALPHA;
		case Blend::dstAlpha: return D3D11_BLEND_DEST_ALPHA;
		case Blend::invDstAlpha: return D3D11_BLEND_INV_DEST_ALPHA;
		default: INVALID_CODE_PATH("bad blend");
	}
}
D3D11_PRIMITIVE_TOPOLOGY cvtTopology(Topology topology) {
	switch (topology) {
		case Topology::TriangleList: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case Topology::LineList:	 return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		default: INVALID_CODE_PATH("bad topology");
	}
} 
DXGI_FORMAT cvtFormat(Format format) {
	switch (format) {
		case Format::UN_RGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case Format::UNS_RGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case Format::F_R32: return DXGI_FORMAT_R32_FLOAT;
		case Format::F_RGBA16: return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case Format::F_RGB32: return DXGI_FORMAT_R32G32B32_FLOAT;
		default: INVALID_CODE_PATH("bad format");
	}
}
u32 getSize(DXGI_FORMAT format) {
	switch (format) {
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT: return sizeof(u32) * 4;
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT: return sizeof(u32) * 3;
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT: return sizeof(u32) * 2;
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT: return sizeof(u32);
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT: return sizeof(u16) * 4;
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R16G16_SINT: return sizeof(u16) * 2;
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R16_SINT: return sizeof(u16);
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_SINT: return sizeof(u8) * 4;
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R8G8_SINT: return sizeof(u8) * 2;
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT: return sizeof(u8);
		default: INVALID_CODE_PATH("bad format");
	}
}

#define RELEASE(x)        \
	do {                  \
		if (x) {          \
			x->Release(); \
			x = 0;        \
		}                 \
	} while (0)

struct RenderTarget {
	ID3D11RenderTargetView *rtv;
	ID3D11ShaderResourceView *srv;
	ID3D11Texture2D *texture;
	v2u size;
	u32 samplerIndex;
};
struct StructuredBuffer {
	ID3D11Buffer *buffer;
	ID3D11ShaderResourceView *srv;
	u32 currentSize;
};
struct Shader {
	ID3D11VertexShader *vs;
	ID3D11PixelShader *ps;
};
struct Texture {
	ID3D11Texture2D *texture;
	ID3D11ShaderResourceView *srv;
	u32 samplerIndex;
	u32 rowPitch;
};

static ID3D11BlendState *blends[(u32)Blend::count][(u32)Blend::count][(u32)BlendOp::count]{};
static ID3D11SamplerState *samplers[(u32)Address::count][(u32)Filter::count]{};

#define MAX_BUFFERS		   1024
#define MAX_TEXTURES	   1024
#define MAX_SHADERS		   1024
#define MAX_RENDER_TARGETS 1024

template <class T, umm storageSize>
struct Storage {
	Storage() { 
		memset(usageMask, ~0, sizeof(usageMask)); 
	}
	T *allocate() {
		mutex.lock();
		u32 maskIndex = 0;
		while (usageMask[maskIndex] == 0) {
			++maskIndex;
		}
		if (maskIndex == storageSize) {
			INVALID_CODE_PATH("out of '%s' storage", typeid(T).name());
		}
		u32 bitIndex = findLowestOneBit(usageMask[maskIndex]);
		usageMask[maskIndex] &= ~(1 << bitIndex);
		mutex.unlock();

		return values + maskIndex * bitsInMask + bitIndex;
	}
	void deallocate(T &val) {
		auto index = indexOf(&val);

		auto maskIndex = index / 32;
		auto bitIndex = index % 32;

		mutex.lock();
		usageMask[maskIndex] |= (u32)(1u << bitIndex);
		mutex.unlock();
	}
	u32 indexOf(T const *addr) { 
		ASSERT(values <= addr && addr < values + storageSize);
		return (u32)(addr - values); 
	}

	T &operator[](umm i) { return values[i]; }
	T const &operator[](umm i) const { return values[i]; }

	static constexpr u32 bitsInMask = sizeof(umm) * 8;

	std::mutex mutex;
	T values[storageSize]{};
	umm usageMask[ceil(storageSize, bitsInMask) / bitsInMask]{};
};

static Storage<StructuredBuffer, MAX_BUFFERS> buffers;
static Storage<Texture, MAX_TEXTURES> textures;
static Storage<Shader, MAX_SHADERS> shaders;
static Storage<RenderTarget, MAX_RENDER_TARGETS> renderTargets;

struct alignas(16) GlobalCBufferData {
	m4 matrices[8];
	v4f v4f[8];
};
struct GlobalCBuffer {
	GlobalCBufferData data;
	ID3D11Buffer *buffer;

	void update() {
		D3D11_MAPPED_SUBRESOURCE mapped{};
		DHR(immediateContext->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy(mapped.pData, &data, sizeof(data));
		immediateContext->Unmap(buffer, 0);
	}
};
GlobalCBuffer globalCBuffer{};

#define CHECK_ID(val) ASSERT((val).valid(), "invalid id")

struct alignas(16) ScreenInfoCBD {
	v2f screenSize;
	v2f invScreenSize;
	f32 aspectRatio;
#pragma warning(suppress : 4324)
};
ScreenInfoCBD screenInfoCBD;
ID3D11Buffer *screenInfoCB;

struct alignas(16) FrameInfoCBD {
	f32 time;
#pragma warning(suppress : 4324)
};
ID3D11Buffer *frameInfoCB;

ID3D11SamplerState **initSampler(Address address, Filter filter, v4f borderColor) {
	auto sampler = &samplers[(u32)address][(u32)filter];
	if (*sampler == 0) {
		D3D11_SAMPLER_DESC desc{};
		desc.AddressU = desc.AddressV = desc.AddressW = cvtAddress(address);
		desc.Filter = cvtFilter(filter);
		desc.MaxAnisotropy = 16;
		desc.MaxLOD = FLT_MAX;
		memcpy(desc.BorderColor, &borderColor, sizeof(borderColor));
		DHR(device->CreateSamplerState(&desc, sampler));
	}
	return sampler;
}
void create(RenderTarget &rt, v2u size, Format format, u32 sampleCount, Address address, Filter filter, v4f borderColor) {
	auto dxgiFormat = cvtFormat(format);
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Format = dxgiFormat;
		desc.ArraySize = 1;
		desc.MipLevels = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.SampleDesc = {sampleCount, 0};
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.Width = size.x;
		desc.Height = size.y;
		DHR(device->CreateTexture2D(&desc, 0, &rt.texture));
	}
	D3D11_RENDER_TARGET_VIEW_DESC desc{};
	desc.ViewDimension = sampleCount == 1 ? D3D11_RTV_DIMENSION_TEXTURE2D : D3D11_RTV_DIMENSION_TEXTURE2DMS;
	desc.Format = dxgiFormat;
	DHR(device->CreateRenderTargetView(rt.texture, &desc, &rt.rtv));
	DHR(device->CreateShaderResourceView(rt.texture, 0, &rt.srv));
	rt.samplerIndex = (u32)(initSampler(address, filter, borderColor) - &samplers[0][0]);
	rt.size = size;
}
void bind(ID3D11ShaderResourceView *srv, u32 samplerIndex, Stage stage, u32 slot) {
	auto sampler = &samplers[0][0] + samplerIndex;
	ASSERT(sampler < &samplers[(u32)Address::count][(u32)Filter::count], "invalid sampler");
	switch (stage) {
		case Stage::vs:
			immediateContext->VSSetShaderResources(slot, 1, &srv);
			immediateContext->VSSetSamplers(slot, 1, sampler);
			break;
		case Stage::ps:
			immediateContext->PSSetShaderResources(slot, 1, &srv);
			immediateContext->PSSetSamplers(slot, 1, sampler);
			break;
		default: INVALID_CODE_PATH("Bad shader stage"); break;
	}
}
void bind(RenderTarget &rt, Stage stage, u32 slot) { bind(rt.srv, rt.samplerIndex, stage, slot); }
void create(StructuredBuffer &buf, void const *data, u32 stride, u32 count) {
	ASSERT(!buf.buffer, "StructuredBuffer not released");
	buf.buffer = createBuffer(D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, stride * count,
							  stride, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, data);
	D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementWidth = stride;
	desc.Buffer.NumElements = count;
	DHR(device->CreateShaderResourceView(buf.buffer, &desc, &buf.srv));
	buf.currentSize = count * stride;
}
bool valid(StructuredBuffer &buf) { return buf.buffer != 0; }
void update(StructuredBuffer &buf, void const *data, u32 size) {
	ASSERT(size <= buf.currentSize, "buffer overflow");
	D3D11_MAPPED_SUBRESOURCE mapped{};
	DHR(immediateContext->Map(buf.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
	memcpy(mapped.pData, data, size);
	immediateContext->Unmap(buf.buffer, 0);
}
void bind(StructuredBuffer &buf, Stage stage, u32 slot) {
	switch (stage) {
		case Stage::vs: immediateContext->VSSetShaderResources(slot, 1, &buf.srv); break;
		case Stage::ps: immediateContext->PSSetShaderResources(slot, 1, &buf.srv); break;
		default: INVALID_CODE_PATH("Bad shader stage"); break;
	}
}
void bind(Shader &sh) {
	immediateContext->VSSetShader(sh.vs, 0, 0);
	immediateContext->PSSetShader(sh.ps, 0, 0);
}
void bind(Texture &tex, Stage stage, u32 slot) { bind(tex.srv, tex.samplerIndex, stage, slot); }
void release(RenderTarget &rt) {
	RELEASE(rt.rtv);
	RELEASE(rt.srv);
	RELEASE(rt.texture);
}
void release(StructuredBuffer &buf) {
	RELEASE(buf.buffer);
	RELEASE(buf.srv);
	buf.currentSize = 0;
}
void release(Shader &sh) {
	RELEASE(sh.vs);
	RELEASE(sh.ps);
}
void release(Texture &tex) {
	RELEASE(tex.srv);
	RELEASE(tex.texture);
}

void setViewport(v2u size) {
	D3D11_VIEWPORT v{};
	v.Width = (f32)size.x;
	v.Height = (f32)size.y;
	v.MaxDepth = 1;
	immediateContext->RSSetViewports(1, &v);
}

#define R_DECORATE(type, name, args, params) FORCEINLINE type name args

#define this renderer

R_INIT {
	PROFILE_FUNCTION;

	DXGI_SWAP_CHAIN_DESC swapChainDesc{};
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Format = cvtFormat(backBufferFormat);
	swapChainDesc.BufferDesc.RefreshRate = {60, 1};
	swapChainDesc.BufferDesc.Width = 1;
	swapChainDesc.BufferDesc.Height = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = (HWND)window.handle;
	swapChainDesc.SampleDesc = {sampleCount, 0};
	swapChainDesc.Windowed = true;

	UINT flags = 0;
//#if BUILD_DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
//#endif
	PROFILE_BEGIN("D3D11CreateDeviceAndSwapChain");
	DHR(D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags, 0, 0, D3D11_SDK_VERSION, &swapChainDesc,
									  &swapChain, &device, 0, &immediateContext));
	PROFILE_END;

	immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	globalCBuffer.buffer = createBuffer(D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE,
										sizeof(globalCBuffer.data), 0, 0, 0);
	screenInfoCB = createBuffer(D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE,
								sizeof(screenInfoCBD), 0, 0, 0);
	frameInfoCB = createBuffer(D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE,
								sizeof(FrameInfoCBD), 0, 0, 0);
	immediateContext->VSSetConstantBuffers(0, 1, &screenInfoCB);
	immediateContext->PSSetConstantBuffers(0, 1, &screenInfoCB);
	immediateContext->VSSetConstantBuffers(1, 1, &globalCBuffer.buffer);
	immediateContext->PSSetConstantBuffers(1, 1, &globalCBuffer.buffer);
	immediateContext->VSSetConstantBuffers(2, 1, &frameInfoCB);
	immediateContext->PSSetConstantBuffers(2, 1, &frameInfoCB);
	renderTargets.allocate(); // backbuffer
}
R_CREATE_RT {
	auto rt = renderTargets.allocate();
	create(*rt, size, format, sampleCount, address, filter, borderColor);
	return {renderTargets.indexOf(rt)};
}
R_BIND_RT {
	ASSERT(rts.size());
	ASSERT(rts.size() <= 8);
	StaticList<ID3D11RenderTargetView *, 8> rtvs;
	for (auto &rt : rts) {
		CHECK_ID(rt);
		rtvs.push_back(renderTargets[rt.id].rtv);
	}
	if (updateViewport) {
		StaticList<D3D11_VIEWPORT, 8> viewports;
		for (auto &rt : rts) {
			auto &r = renderTargets[rt.id];

			D3D11_VIEWPORT v{};
			v.Width = (f32)r.size.x;
			v.Height = (f32)r.size.y;
			v.MaxDepth = 1;
			viewports.push_back(v);
		}
		immediateContext->RSSetViewports((UINT)viewports.size(), viewports.data());
	}
	immediateContext->OMSetRenderTargets((UINT)rtvs.size(), rtvs.data(), 0);
}
R_BIND_RT_AS_TEXTURE {
	ASSERT(rts.size() <= 8);
	StaticList<ID3D11ShaderResourceView *, 8> srvs;
	StaticList<ID3D11SamplerState *, 8> smps;
	for (auto &rt : rts) {
		CHECK_ID(rt);
		auto &r = renderTargets[rt.id];

		srvs.push_back(r.srv);

		auto sampler = &samplers[0][0] + r.samplerIndex;
		ASSERT(sampler < &samplers[(u32)Address::count][(u32)Filter::count], "invalid sampler");
		smps.push_back(*sampler);
	}
	switch (stage) {
		case Stage::vs:
			immediateContext->VSSetShaderResources(slot, (UINT)rts.size(), srvs.data());
			immediateContext->VSSetSamplers(slot, (UINT)rts.size(), smps.data());
			break;
		case Stage::ps:
			immediateContext->PSSetShaderResources(slot, (UINT)rts.size(), srvs.data());
			immediateContext->PSSetSamplers(slot, (UINT)rts.size(), smps.data());
			break;
		default: INVALID_CODE_PATH("Bad shader stage"); break;
	}
}
R_SHUTDOWN {}
R_GET_DRAW_COUNT {
	return drawCalls;
}
R_RESIZE {
	PROFILE_FUNCTION;
	auto &backBuffer = renderTargets[0];
	release(backBuffer);
	backBuffer.size = window.clientSize;
	DHR(swapChain->ResizeBuffers(1, window.clientSize.x, window.clientSize.y, DXGI_FORMAT_UNKNOWN, 0));
	DHR(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer.texture)));
	DHR(device->CreateRenderTargetView(backBuffer.texture, 0, &backBuffer.rtv));

	screenInfoCBD.screenSize = (v2f)window.clientSize;
	screenInfoCBD.invScreenSize = 1.0f / screenInfoCBD.screenSize;
	screenInfoCBD.aspectRatio = screenInfoCBD.screenSize.y / screenInfoCBD.screenSize.x;
	D3D11_MAPPED_SUBRESOURCE mapped{};
	DHR(immediateContext->Map(screenInfoCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
	memcpy(mapped.pData, &screenInfoCBD, sizeof(screenInfoCBD));
	immediateContext->Unmap(screenInfoCB, 0);
}
R_CREATE_BUFFER {
	auto vb = buffers.allocate();
	create(*vb, data, stride, count);
	return {buffers.indexOf(vb)};
}
R_UPDATE_BUFFER {
	PROFILE_FUNCTION;
	CHECK_ID(buffer);
	update(buffers[buffer.id], data, size);
}
R_BIND_BUFFER {
	CHECK_ID(buffer);
	bind(buffers[buffer.id], stage, slot);
}
R_CREATE_TEXTURE_FROM_FILE {
	PROFILE_FUNCTION;

	auto tex = textures.allocate();
	DHR(D3DX11CreateShaderResourceViewFromFileA(device, path, 0, 0, &tex->srv, 0));
	tex->samplerIndex = (u32)(initSampler(address, filter, borderColor) - &samplers[0][0]);

	tex->srv->GetResource((ID3D11Resource **)&tex->texture);

	D3D11_TEXTURE2D_DESC desc;
	tex->texture->GetDesc(&desc);
	tex->rowPitch = getSize(desc.Format) * desc.Width;

	return {textures.indexOf(tex)};
}
R_CREATE_TEXTURE {
	auto tex = textures.allocate();
	auto dxgiFormat = cvtFormat(format);
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Format = dxgiFormat;
		desc.ArraySize = 1;
		desc.MipLevels = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.SampleDesc = {1, 0};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Width = width;
		desc.Height = height;
		DHR(device->CreateTexture2D(&desc, 0, &tex->texture));
	}
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
		desc.Format = dxgiFormat;
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipLevels = 1;
		DHR(device->CreateShaderResourceView(tex->texture, &desc, &tex->srv));
	}
	tex->samplerIndex = (u32)(initSampler(address, filter, borderColor) - &samplers[0][0]);
	tex->rowPitch = getSize(dxgiFormat) * width;

	return {textures.indexOf(tex)};
}
R_RELEASE_TEXTURE {
	CHECK_ID(texture);
	release(textures[texture.id]);
	textures.deallocate(textures[texture.id]);
}
R_BIND_TEXTURE {
	CHECK_ID(texture);
	bind(textures[texture.id], stage, slot);
}
R_UNBIND_TEXTURES {
	ID3D11ShaderResourceView *nullSrv[8]{};
	ID3D11SamplerState *nullSmp[8]{};
	switch (stage) {
		case Stage::vs:
			immediateContext->VSSetShaderResources(slot, count, nullSrv);
			immediateContext->VSSetSamplers(slot, count, nullSmp);
			break;
		case Stage::ps:
			immediateContext->PSSetShaderResources(slot, count, nullSrv);
			immediateContext->PSSetSamplers(slot, count, nullSmp);
			break;
		default: INVALID_CODE_PATH("Bad shader stage"); break;
	}
}
R_CREATE_SHADER {
	PROFILE_FUNCTION;
	auto shader = shaders.allocate();

	char buffer[256];
	sprintf(buffer, "%s.hlsl", path);
	path = buffer;

	auto shaderSource = readEntireFile(path);
	ASSERT(shaderSource.valid);

	StaticList<D3D_SHADER_MACRO, 256> commonDefines;

	for (auto &d : macros) {
		commonDefines.push_back({d.name, d.value});
	}
	WorkQueue work;
	work.push([&] {
		auto defines = commonDefines;
		defines.push_back({"COMPILE_VS"});
		defines.push_back({});
		shader->vs = createVertexShader(shaderSource.data, path, defines.data());
		ASSERT(shader->vs);
	});
	{
		auto defines = commonDefines;
		defines.push_back({"COMPILE_PS"});
		defines.push_back({});
		shader->ps = createPixelShader(shaderSource.data, path, defines.data());
		ASSERT(shader->ps);
	}
	work.completeAllWork();

	freeEntireFile(shaderSource);

	return {shaders.indexOf(shader)};
}
R_RELEASE_SHADER {
	CHECK_ID(shader);
	release(shaders[shader.id]);
	shaders.deallocate(shaders[shader.id]);
}
R_RELEASE_RT {
	CHECK_ID(rt);
	release(renderTargets[rt.id]);
	renderTargets.deallocate(renderTargets[rt.id]);
}
R_PRESENT {
	DHR(swapChain->Present(window.fullscreen, 0));
	drawCalls = 0;

	FrameInfoCBD frameInfoCBD;
	frameInfoCBD.time = time.time;
	D3D11_MAPPED_SUBRESOURCE mapped{};
	DHR(immediateContext->Map(frameInfoCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
	memcpy(mapped.pData, &frameInfoCBD, sizeof(frameInfoCBD));
	immediateContext->Unmap(frameInfoCB, 0);
}
R_UPDATE_TEXTURE {
	PROFILE_FUNCTION;
	auto &t = textures[tex.id];
	immediateContext->UpdateSubresource(t.texture, 0, 0, data, t.rowPitch, 0);
}
R_BIND_SHADER {
	CHECK_ID(shader);
	bind(shaders[shader.id]);
}
R_DRAW {
	++drawCalls;
	immediateContext->Draw(vertexCount, offset);
}
R_SET_MATRIX {
	globalCBuffer.data.matrices[slot] = value;
	globalCBuffer.update();
}
R_SET_V4F {
	globalCBuffer.data.v4f[slot] = value;
	globalCBuffer.update();
}
R_SET_BLEND {
	auto &blend = blends[(u32)src][(u32)dst][(u32)op];
	if (!blend) {
		blend = createBlend(cvtBlendOp(op), cvtBlend(src), cvtBlend(dst));
	}
	immediateContext->OMSetBlendState(blend, v4f{}.data(), ~0u);
}
R_SET_TOPOLOGY {
	immediateContext->IASetPrimitiveTopology(cvtTopology(topology));
}
R_DISABLE_BLEND { 
	immediateContext->OMSetBlendState(0, v4f{}.data(), ~0u); 
}
R_CLEAR_TARGET {
	CHECK_ID(rt);
	immediateContext->ClearRenderTargetView(renderTargets[rt.id].rtv, color.data());
}
/*
ID3D11DepthStencilView* createDepthTarget(v2u size) {
	PROFILE_FUNCTION;
	ID3D11Texture2D* depthTex;
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.ArraySize = 1;
		desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		desc.Format = DXGI_FORMAT_D32_FLOAT;
		desc.Width = size.x;
		desc.Height = size.y;
		desc.MipLevels = 1;
		desc.SampleDesc = {1, 0};
		desc.Usage = D3D11_USAGE_DEFAULT;
		DHR(device->CreateTexture2D(&desc, 0, &depthTex));
	}
	D3D11_DEPTH_STENCIL_VIEW_DESC desc{};
	desc.Format = DXGI_FORMAT_D32_FLOAT;
	desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	ID3D11DepthStencilView* dsv;
	DHR(device->CreateDepthStencilView(depthTex, &desc, &dsv));
	depthTex->Release();
	return dsv;
}
*/

//EXPORT ID3D11Device *d3d11_getDevice() { return device; }
//EXPORT ID3D11DeviceContext *d3d11_getImmediateContext() { return immediateContext; }
//EXPORT IDXGISwapChain *d3d11_getSwapChain() { return swapChain; }

} // namespace D3D11

#undef R_DECORATE
#define R_DECORATE(type, name, args, params) extern "C" __declspec(dllexport) type RNAME(name) args { return D3D11::name params; }

R_ALLFUNS;
