#include "allocator.h"
#include "common.h"
#include "renderer.h"
#include "window.h"

#define D3D11_NO_HELPERS

#pragma warning(push, 0)
#include "../dep/Microsoft DirectX SDK/Include/D3DX11.h"
#include <d3d11.h>
#include <d3dcompiler.h>

#ifndef D3D_COMPILE_STANDARD_FILE_INCLUDE
#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)
#endif

#include <vector>
#pragma warning(pop)

#define DHR(call)               \
	do {                        \
		HRESULT dhr = call;     \
		ASSERT(SUCCEEDED(dhr)); \
	} while (0)

#define RELEASE_ZERO(x)   \
	do {                  \
		if (x) {          \
			x->Release(); \
			x = 0;        \
		}                 \
	} while (0)

namespace D3D11 {

static IDXGISwapChain* swapChain;
static ID3D11Device* device;
static ID3D11DeviceContext* immediateContext;

struct VS {
	using Type = ID3D11VertexShader*;
	static constexpr auto name = "vs";
	static constexpr auto target = "vs_5_0";
	static constexpr auto create = &ID3D11Device::CreateVertexShader;
};

struct PS {
	using Type = ID3D11PixelShader*;
	static constexpr auto name = "ps";
	static constexpr auto target = "ps_5_0";
	static constexpr auto create = &ID3D11Device::CreatePixelShader;
};

template <class T>
concept CShader = std::is_same_v<T, VS> || std::is_same_v<T, PS>;

template <CShader Shader>
typename Shader::Type createShader(StringView src, char const* name, D3D_SHADER_MACRO const* defines) {
	PROFILE_FUNCTION;
	ID3DBlob* code{};
	ID3DBlob* messages{};
	HRESULT compileResult = D3DCompile(src.data(), src.size(), name, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main",
									   Shader::target, 0, 0, &code, &messages);
	if (messages) {
		puts((char const*)messages->GetBufferPointer());
		messages->Release();
	}
	DHR(compileResult);
	typename Shader::Type shader = 0;
	DHR((device->*Shader::create)(code->GetBufferPointer(), code->GetBufferSize(), 0, &shader));
	ASSERT(shader);
	code->Release();
	return shader;
}
ID3D11Buffer* createBuffer(UINT bindFlags, D3D11_USAGE usage, UINT cpuAccess, UINT size, UINT stride, UINT misc,
						   void const* initialData) {
	PROFILE_FUNCTION;
	D3D11_BUFFER_DESC desc{};
	desc.BindFlags = bindFlags;
	desc.Usage = usage;
	desc.CPUAccessFlags = cpuAccess;
	desc.ByteWidth = size;
	desc.StructureByteStride = stride;
	desc.MiscFlags = misc;
	ID3D11Buffer* result;
	if (initialData) {
		D3D11_SUBRESOURCE_DATA d3dInitialData;
		d3dInitialData.pSysMem = initialData;
		DHR(device->CreateBuffer(&desc, &d3dInitialData, &result));
	} else {
		DHR(device->CreateBuffer(&desc, 0, &result));
	}
	return result;
}
ID3D11BlendState* createBlend(D3D11_BLEND_OP op, D3D11_BLEND src, D3D11_BLEND dst) {
	D3D11_BLEND_DESC desc{};
	desc.RenderTarget[0].BlendEnable = true;
	desc.RenderTarget[0].BlendOp = op;
	desc.RenderTarget[0].SrcBlend = src;
	desc.RenderTarget[0].DestBlend = dst;
	desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	ID3D11BlendState* result;
	DHR(device->CreateBlendState(&desc, &result));
	return result;
}

D3D11_TEXTURE_ADDRESS_MODE cvtAddress(Address address) {
	switch (address) {
		case Address::wrap: return D3D11_TEXTURE_ADDRESS_WRAP;
		case Address::clamp: return D3D11_TEXTURE_ADDRESS_CLAMP;
		case Address::mirror: return D3D11_TEXTURE_ADDRESS_MIRROR;
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
DXGI_FORMAT cvtFormat(Format format) {
	switch (format) {
		case Format::UN_RGB8: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case Format::F_RGB16: return DXGI_FORMAT_R16G16B16A16_FLOAT;
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
	ID3D11RenderTargetView* rtv;
	ID3D11ShaderResourceView* srv;
	ID3D11Texture2D* texture;
	void release() {
		RELEASE(rtv);
		RELEASE(srv);
		RELEASE(texture);
	}
	void create(v2u size, Format format, u32 sampleCount) {
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
			DHR(device->CreateTexture2D(&desc, 0, &texture));
		}
		D3D11_RENDER_TARGET_VIEW_DESC desc{};
		desc.ViewDimension = sampleCount == 1 ? D3D11_RTV_DIMENSION_TEXTURE2D : D3D11_RTV_DIMENSION_TEXTURE2DMS;
		desc.Format = dxgiFormat;
		DHR(device->CreateRenderTargetView(texture, &desc, &rtv));
		DHR(device->CreateShaderResourceView(texture, 0, &srv));
	}
	void bind() { immediateContext->OMSetRenderTargets(1, &rtv, 0); }
	void bind(Stage stage, u32 slot) {
		switch (stage) {
			case Stage::vs: immediateContext->VSSetShaderResources(slot, 1, &srv); break;
			case Stage::ps: immediateContext->PSSetShaderResources(slot, 1, &srv); break;
			default: INVALID_CODE_PATH("bad shader stage");
		}
	}
};
struct StructuredBuffer {
	ID3D11Buffer* buffer = 0;
	ID3D11ShaderResourceView* srv = 0;
	u32 currentSize = 0;
	void release() {
		buffer->Release();
		srv->Release();
		buffer = 0;
	}
	bool valid() { return buffer != 0; }
	void create(void const* data, u32 stride, u32 count) {
		ASSERT(!buffer, "StructuredBuffer not released");
		buffer = createBuffer(D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE, stride * count,
							  stride, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, data);
		D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
		desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		desc.Buffer.ElementWidth = stride;
		desc.Buffer.NumElements = count;
		DHR(device->CreateShaderResourceView(buffer, &desc, &srv));
		currentSize = count * stride;
	}
	void update(void const* data, u32 size) {
		ASSERT(size <= currentSize, "buffer overflow");
		D3D11_MAPPED_SUBRESOURCE mapped{};
		DHR(immediateContext->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy(mapped.pData, data, size);
		immediateContext->Unmap(buffer, 0);
	}
	void bind(Stage stage, u32 slot) {
		switch (stage) {
			case Stage::vs: immediateContext->VSSetShaderResources(slot, 1, &srv); break;
			case Stage::ps: immediateContext->PSSetShaderResources(slot, 1, &srv); break;
			default: INVALID_CODE_PATH("Bad shader stage"); break;
		}
	}
};
struct Shader {
	ID3D11VertexShader* vs = 0;
	ID3D11PixelShader* ps = 0;
	void bind() {
		immediateContext->VSSetShader(vs, 0, 0);
		immediateContext->PSSetShader(ps, 0, 0);
	}
	void release() {
		RELEASE(vs);
		RELEASE(ps);
	}
};

static ID3D11SamplerState* samplers[(u32)Address::count][(u32)Filter::count]{};

struct Texture {
	ID3D11ShaderResourceView* srv = 0;
	u32 samplerIndex = 0;
	void bind(Stage stage, u32 slot) {
		auto sampler = &samplers[0][0] + samplerIndex;
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
};

#define CHECK_ID(val) ASSERT((val).valid(), "invalid id")

static ID3D11BlendState* blends[(u32)Blend::count][(u32)Blend::count][(u32)BlendOp::count]{};

#define MAX_BUFFERS 1024
static StructuredBuffer buffers[MAX_BUFFERS]{};

#define MAX_TEXTURES 1024
static Texture textures[MAX_TEXTURES]{};

#define MAX_SHADERS 1024
static Shader shaders[MAX_SHADERS]{};

#define MAX_RENDER_TARGETS 1024
static RenderTarget renderTargets[MAX_RENDER_TARGETS]{};

#define MAX_MATRIX_COUNT 8
struct MatrixCBufferData {
	m4 matrices[MAX_MATRIX_COUNT];
};
MatrixCBufferData matrixCBufferData{};
ID3D11Buffer* matrixCBuffer;

#include "renderer_exporter.inl"

#define ADD_ARG(...)						 (Renderer * this, __VA_ARGS__)
#define R_DECORATE(type, name, args, params) EXPORT type name ADD_ARG args

#define this renderer

struct alignas(16) ScreenInfoCBD {
	v2 screenSize;
	v2 invScreenSize;
	f32 aspectRatio;
#pragma warning(suppress : 4324)
};
ScreenInfoCBD screenInfoCBD;
ID3D11Buffer* screenInfoCB;

R_INIT {
	PROFILE_FUNCTION;

	char buf[1024];
	GetCurrentDirectoryA(1024, buf);
	puts(buf);

	DXGI_SWAP_CHAIN_DESC swapChainDesc{};
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.RefreshRate = {60, 1};
	swapChainDesc.BufferDesc.Width = 1;
	swapChainDesc.BufferDesc.Height = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = (HWND)this->window->handle;
	swapChainDesc.SampleDesc = {sampleCount, 0};
	swapChainDesc.Windowed = true;

	UINT flags = 0;
#if BUILD_DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	DHR(D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags, 0, 0, D3D11_SDK_VERSION, &swapChainDesc,
									  &swapChain, &device, 0, &immediateContext));

	immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	matrixCBuffer = createBuffer(D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE,
								 sizeof(matrixCBufferData), 0, 0, 0);
	screenInfoCB = createBuffer(D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE,
								sizeof(screenInfoCBD), 0, 0, 0);
	immediateContext->VSSetConstantBuffers(0, 1, &screenInfoCB);
	immediateContext->PSSetConstantBuffers(0, 1, &screenInfoCB);
	immediateContext->VSSetConstantBuffers(1, 1, &matrixCBuffer);
	immediateContext->PSSetConstantBuffers(1, 1, &matrixCBuffer);
}
R_CREATE_RT {
	auto rt = std::find_if(std::begin(renderTargets), std::end(renderTargets),
						   [](RenderTarget& rt) { return rt.texture == 0; });
	ASSERT(rt != std::end(renderTargets), "out of render targets");
	rt->create(size, format, sampleCount);
	return {(u32)(rt - std::begin(renderTargets))};
}
R_BIND_RT {
	CHECK_ID(rt);
	renderTargets[rt.id].bind();
}
R_BIND_RT_AS_TEXTURE {
	CHECK_ID(rt);
	renderTargets[rt.id].bind(stage, slot);
}
R_SHUTDOWN {}
R_RESIZE {
	PROFILE_FUNCTION;
	auto& backBuffer = renderTargets[0];
	backBuffer.release();
	DHR(swapChain->ResizeBuffers(1, clientSize.x, clientSize.y, DXGI_FORMAT_UNKNOWN, 0));
	DHR(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer.texture)));
	DHR(device->CreateRenderTargetView(backBuffer.texture, 0, &backBuffer.rtv));
	D3D11_VIEWPORT viewport{};
	viewport.Width = (FLOAT)clientSize.x;
	viewport.Height = (FLOAT)clientSize.y;
	viewport.MaxDepth = 1.f;
	immediateContext->RSSetViewports(1, &viewport);

	screenInfoCBD.screenSize = (v2)this->window->clientSize;
	screenInfoCBD.invScreenSize = 1.0f / screenInfoCBD.screenSize;
	screenInfoCBD.aspectRatio = screenInfoCBD.screenSize.y / screenInfoCBD.screenSize.x;
	D3D11_MAPPED_SUBRESOURCE mapped{};
	DHR(immediateContext->Map(screenInfoCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
	memcpy(mapped.pData, &screenInfoCBD, sizeof(screenInfoCBD));
	immediateContext->Unmap(screenInfoCB, 0);
}
R_CREATE_BUFFER {
	auto vb = std::find_if(std::begin(buffers), std::end(buffers), [](StructuredBuffer& b) { return b.buffer == 0; });
	ASSERT(vb != std::end(buffers), "out of buffers");
	vb->create(data, stride, count);
	return {(u32)(vb - std::begin(buffers))};
}
R_UPDATE_BUFFER {
	CHECK_ID(buffer);
	buffers[buffer.id].update(data, size);
}
R_BIND_BUFFER {
	CHECK_ID(buffer);
	buffers[buffer.id].bind(stage, slot);
}

R_CREATE_TEXTURE {
	auto tex = std::find_if(std::begin(textures), std::end(textures), [](Texture& t) { return t.srv == 0; });
	ASSERT(tex != std::end(textures), "out of textures");
	DHR(D3DX11CreateShaderResourceViewFromFileA(device, path, 0, 0, &tex->srv, 0));
	auto sampler = &samplers[(u32)address][(u32)filter];
	if (*sampler == 0) {
		D3D11_SAMPLER_DESC desc{};
		desc.AddressU = desc.AddressV = desc.AddressW = cvtAddress(address);
		desc.Filter = cvtFilter(filter);
		desc.MaxAnisotropy = 16;
		desc.MaxLOD = FLT_MAX;
		DHR(device->CreateSamplerState(&desc, sampler));
	}
	tex->samplerIndex = (u32)(sampler - &samplers[0][0]);
	return {(u32)(tex - std::begin(textures))};
}
R_BIND_TEXTURE {
	CHECK_ID(texture);
	textures[texture.id].bind(stage, slot);
}
R_CREATE_SHADER {
	PROFILE_FUNCTION;
	auto shader = std::find_if(std::begin(shaders), std::end(shaders), [](Shader& s) { return s.vs == 0; });
	ASSERT(shader != std::end(shaders), "out of shaders");

	char buffer[256];
	sprintf(buffer, "%s.hlsl", path);
	path = buffer;

	auto shaderFile = readFile(path);
	ASSERT(shaderFile.valid());

	StringSpan shaderSource;

	if (prefix.size()) {
		u32 totalSize = (u32)(shaderFile.data.size() + prefix.size());
		shaderSource = {(char*)malloc(totalSize), totalSize};
		memcpy(shaderSource.begin(), prefix.begin(), prefix.size());
		memcpy(shaderSource.begin() + prefix.size(), shaderFile.data.begin(), shaderFile.data.size());
	} else {
		shaderSource = shaderFile.data;
	}

	size_t const definesCount = 1;
	D3D_SHADER_MACRO defines[definesCount + 1]{};

	defines[0].Name = "COMPILE_VS";
	shader->vs = createShader<VS>(shaderSource, path, defines);
	ASSERT(shader->vs);

	defines[0].Name = "COMPILE_PS";
	shader->ps = createShader<PS>(shaderSource, path, defines);
	ASSERT(shader->ps);

	if (prefix.size()) {
		free(shaderSource.begin());
	}
	shaderFile.release();
	return {(u32)(shader - std::begin(shaders))};
}
R_RELEASE_SHADER {
	CHECK_ID(shader);
	shaders[shader.id].release();
}
R_RELEASE_RT {
	CHECK_ID(rt);
	renderTargets[rt.id].release();
}
R_PREPARE {
}
R_RENDER { 
	DHR(swapChain->Present(this->window->fullscreen, 0)); 
	this->drawCalls = 0;
}
R_FILL_RENDER_TARGET {
	ID3D11Resource* tex;
	renderTargets[0].rtv->GetResource(&tex);
	immediateContext->UpdateSubresource(tex, 0, 0, data, this->window->clientSize.x * sizeof(u32), 0);
	tex->Release();
}
R_BIND_SHADER {
	CHECK_ID(shader);
	shaders[shader.id].bind();
}
R_DRAW {
	++this->drawCalls;
	immediateContext->Draw(vertexCount, offset);
}
R_SET_MATRIX {
	matrixCBufferData.matrices[slot] = matrix;
	D3D11_MAPPED_SUBRESOURCE mapped{};
	DHR(immediateContext->Map(matrixCBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
	memcpy(mapped.pData, &matrixCBufferData, sizeof(matrixCBufferData));
	immediateContext->Unmap(matrixCBuffer, 0);
}
R_SET_BLEND {
	auto& blend = blends[(u32)src][(u32)dst][(u32)op];
	if (!blend) {
		blend = createBlend(cvtBlendOp(op), cvtBlend(src), cvtBlend(dst));
	}
	immediateContext->OMSetBlendState(blend, v4{}.data(), ~0u);
}
R_DISABLE_BLEND {
	immediateContext->OMSetBlendState(0, v4{}.data(), ~0u);
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

EXPORT ID3D11Device* d3d11_getDevice() { return device; }
EXPORT ID3D11DeviceContext* d3d11_getImmediateContext() { return immediateContext; }
EXPORT IDXGISwapChain* d3d11_getSwapChain() { return swapChain; }

} // namespace D3D11