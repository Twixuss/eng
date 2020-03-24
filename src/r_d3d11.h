#pragma once
#include "allocator.h"
#include "common.h"
#include "renderer.h"
#include "window.h"

#pragma warning(push, 0)
#include <d3d11.h>
#include <d3dcompiler.h>

#ifndef D3D_COMPILE_STANDARD_FILE_INCLUDE
#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)
#endif

#include <vector>
#pragma warning(pop)

#define EXPORT extern "C" __declspec(dllexport)

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

struct CompiledShader {
	ID3DBlob* code{};
	ID3DBlob* messages{};
	void release() {
		RELEASE_ZERO(code);
		RELEASE_ZERO(messages);
	}
};

CompiledShader compileShader(char const* src, size_t srcSize, D3D_SHADER_MACRO const* defines, char const* name,
							 char const* entry, char const* target) {
	PROFILE_FUNCTION;
	CompiledShader result;
	D3DCompile(src, srcSize, name, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, target, 0, 0, &result.code,
			   &result.messages);
	return result;
};

struct VS {
	using Type = ID3D11VertexShader*;
	static constexpr auto name = "vs";
	static constexpr auto entry = "vs_main";
	static constexpr auto target = "vs_5_0";
	static constexpr auto create = &ID3D11Device::CreateVertexShader;
};

struct PS {
	using Type = ID3D11PixelShader*;
	static constexpr auto name = "ps";
	static constexpr auto entry = "ps_main";
	static constexpr auto target = "ps_5_0";
	static constexpr auto create = &ID3D11Device::CreatePixelShader;
};

template <class T>
concept CShader = std::is_same_v<T, VS> || std::is_same_v<T, PS>;

template <CShader Shader>
typename Shader::Type createShader(ID3D11Device* device, ID3DBlob* code) {
	PROFILE_FUNCTION;
	typename Shader::Type shader = 0;
	(device->*Shader::create)(code->GetBufferPointer(), code->GetBufferSize(), 0, &shader);
	return shader;
}

template <CShader Shader>
typename Shader::Type compileAndCreateShader(ID3D11Device* device, StringView src, D3D_SHADER_MACRO const* defines) {
	PROFILE_FUNCTION;
	auto code = compileShader(src.data(), src.size(), defines, Shader::name, Shader::entry, Shader::target);
	if (code.messages) {
		puts((char const*)code.messages->GetBufferPointer());
	}
	typename Shader::Type shader = createShader<Shader>(device, code.code);
	ASSERT(shader);
	code.release();
	return shader;
}

ID3D11Buffer* createBuffer(ID3D11Device* device, UINT bindFlags, D3D11_USAGE usage, UINT cpuAccess, UINT size,
						   UINT stride, UINT misc, void const* initialData) {
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

#define MAX_SHADERS		   1024
#define MAX_MESHES		   1024
#define MAX_RENDER_TARGETS 1024
namespace D3D11 {
struct ShaderImpl : Shader {
	ID3D11VertexShader* vs = 0;
	ID3D11PixelShader* ps = 0;
};
struct StructuredBuffer {
	ID3D11Buffer* buffer = 0;
	ID3D11ShaderResourceView* srv = 0;
	void release() {
		buffer->Release();
		srv->Release();
		buffer = 0;
	}
	bool valid() { return buffer != 0; }
	void create(ID3D11Device* device, void* data, u32 stride, u32 count) {
		ASSERT(!buffer, "StructuredBuffer not released");
		buffer = createBuffer(device, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_IMMUTABLE, 0, stride * count, stride,
							  D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, data);
		D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
		desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		desc.Buffer.ElementWidth = stride;
		desc.Buffer.NumElements = count;
		DHR(device->CreateShaderResourceView(buffer, &desc, &srv));
	}
};
struct MeshImpl : Mesh {
	StructuredBuffer vb, ib;
	u32 indexCount = 0;
};
IDXGISwapChain* swapChain;
ID3D11Device* device;
ID3D11DeviceContext* immediateContext;

EXPORT Renderer::ProcEntries r_procEntries;

#undef R_DECORATE
#define R_DECORATE(type, name) EXPORT type name
R_ALLFUNS;

ID3D11DepthStencilView* createDepthTarget(v2u size);

// void push(RenderCommand&&);
// void release(DepthTargetID);
// DepthTargetID createDepthTarget(V2u size);

ID3D11RenderTargetView* renderTargets[MAX_RENDER_TARGETS]{};
ID3D11DepthStencilView* depthTargets[MAX_RENDER_TARGETS]{};
extern Allocator<ShaderImpl> shaders;
extern Allocator<MeshImpl> meshes;
}; // namespace D3D11