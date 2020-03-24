#include "r_d3d11.h"
#include "window_internal.h"
namespace D3D11 {
Allocator<ShaderImpl> shaders;
Allocator<MeshImpl> meshes;
/*
DWORD D3D11::renderProc(void* param) {
	auto& d3d11 = *(D3D11*)param;
	auto& renderThread = d3d11.renderThreads[d3d11.renderThreadIndex++];
	ID3D11DeviceContext* deferredContext;
	DHR(d3d11.device->CreateDeferredContext(0, &deferredContext));
	// clang-format off
	auto visitor = Visitor{
		[](std::monostate) { INVALID_CODE_PATH("Invalid render command"); },
		[&](RC::Draw& drawCmd) { deferredContext->Draw(drawCmd.count, drawCmd.offset); },
		[&](RC::BindShader& bindCmd) {
			Shader& shader = d3d11.shaders[bindCmd.id];
			deferredContext->VSSetShader(shader.vs, 0, 0);
			deferredContext->PSSetShader(shader.ps, 0, 0);
		},
		[&](RC::ClearRenderTarget& clearCmd) {
			deferredContext->ClearRenderTargetView(d3d11.renderTargets[clearCmd.target.id], clearCmd.color.data());
		},
		[&](RC::ClearDepthTarget& clearCmd) {
			deferredContext->ClearDepthStencilView(d3d11.depthTargets[clearCmd.target.id], D3D11_CLEAR_DEPTH,
clearCmd.depth, 0);
		},
		[&](RC::SetTargets& setCmd) {
			deferredContext->OMSetRenderTargets(1, d3d11.renderTargets + setCmd.renderTarget.id,
												d3d11.depthTargets[setCmd.depthTarget.id]);
		},
		[&](RC::UpdateBuffer& updateCmd) {
			D3D11_MAPPED_SUBRESOURCE mapped;
			DHR(deferredContext->Map(updateCmd.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
			memcpy(mapped.pData, updateCmd.getData(), updateCmd.getSize());
			deferredContext->Unmap(updateCmd.buffer, 0);
			updateCmd.release();
		},
		[&](){
			D3D11_VIEWPORT viewport{};
			viewport.Width = (FLOAT)clientSize.x;
			viewport.Height = (FLOAT)clientSize.y;
			viewport.MaxDepth = 1.f;
			deferredContext->RSSetViewports(1, &viewport);
		}
	};
	// clang-format on
	for (;;) {
		WaitForSingleObjectEx(renderThread.beginSemaphore, INFINITE, 0);
		auto workSpan = renderThread.work;
		for (auto& cmd : workSpan) {
			std::visit(visitor, cmd);
		}
		deferredContext->FinishCommandList(false, &renderThread.commandList);
		ReleaseSemaphore(renderThread.endSemaphore, 1, 0);
	}
	return 0;
}
*/
struct {
	m4 mvp;
} matrixCBufferData;
ID3D11Buffer* matrixCBuffer;

Renderer::ProcEntries r_procEntries;
struct ProcAdder {
	ProcAdder(char const* name, void** procAddress) { r_procEntries.push_back({name, procAddress}); }
};

#undef R_DECORATE
#define R_DECORATE(type, name)                                   \
	ProcAdder _procAdder_##name(#name, (void**)&Renderer::name); \
	type name

R_INIT {
	PROFILE_FUNCTION;
	DXGI_SWAP_CHAIN_DESC swapChainDesc{};
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.RefreshRate = {60, 1};
	swapChainDesc.BufferDesc.Width = 1;
	swapChainDesc.BufferDesc.Height = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = Window::hwnd;
	swapChainDesc.SampleDesc = {1, 0};
	swapChainDesc.Windowed = true;

	UINT flags = 0;
#if BUILD_DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	DHR(D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags, 0, 0, D3D11_SDK_VERSION, &swapChainDesc,
									  &swapChain, &device, 0, &immediateContext));

	immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	matrixCBuffer = createBuffer(device, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE,
								 sizeof(matrixCBufferData), 0, 0, 0);
	/*
	for (u32 i = 0; i < _countof(renderThreads); ++i) {
		auto& renderThread = renderThreads[i];

		renderThread.beginSemaphore = CreateSemaphoreExA(0, 0, 1, 0, 0, SEMAPHORE_ALL_ACCESS);
		ASSERT(renderThread.beginSemaphore);

		renderThread.endSemaphore = CreateSemaphoreExA(0, 0, 1, 0, 0, SEMAPHORE_ALL_ACCESS);
		ASSERT(renderThread.endSemaphore);

		auto threadHandle = CreateThread(0, 0, renderProc, this, 0, 0);
		ASSERT(threadHandle);
		CloseHandle(threadHandle);
	} */
}
R_SHUTDOWN {}
R_RESIZE {
	PROFILE_FUNCTION;
	auto& backBuffer = renderTargets[0];
	if (backBuffer)
		backBuffer->Release();
	DHR(swapChain->ResizeBuffers(1, clientSize.x, clientSize.y, DXGI_FORMAT_UNKNOWN, 0));
	ID3D11Texture2D* backBufferTexture;
	DHR(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBufferTexture)));
	DHR(device->CreateRenderTargetView(backBufferTexture, 0, &backBuffer));
	backBufferTexture->Release();
	D3D11_VIEWPORT viewport{};
	viewport.Width = (FLOAT)clientSize.x;
	viewport.Height = (FLOAT)clientSize.y;
	viewport.MaxDepth = 1.f;
	immediateContext->RSSetViewports(1, &viewport);
	if (depthTargets[0])
		depthTargets[0]->Release();
	depthTargets[0] = createDepthTarget(clientSize);
}
R_CREATE_SHADER {
	PROFILE_FUNCTION;
	auto shaderSource = readFile(path);
	ASSERT(shaderSource.valid());

	auto shader = shaders.allocate();

	size_t const definesCount = 1;
	D3D_SHADER_MACRO defines[definesCount + 1]{};

	defines[0].Name = "COMPILE_VS";
	shader->vs = compileAndCreateShader<VS>(device, shaderSource.data, defines);
	ASSERT(shader->vs);

	defines[0].Name = "COMPILE_PS";
	shader->ps = compileAndCreateShader<PS>(device, shaderSource.data, defines);
	ASSERT(shader->ps);

	shaderSource.release();
	return shader;
}
R_CREATE_MESH { return meshes.allocate(); }
R_PREPARE {
	immediateContext->OMSetRenderTargets(1, &renderTargets[0], depthTargets[0]);
	immediateContext->ClearRenderTargetView(renderTargets[0], V4(.3, .6, .9, 1).data());
	immediateContext->ClearDepthStencilView(depthTargets[0], D3D11_CLEAR_DEPTH, 1, 0);
	immediateContext->VSSetConstantBuffers(0, 1, &matrixCBuffer);
}
R_RENDER {
	PROFILE_FUNCTION;
	/*
		for (auto& mr : meshRenderers) {
			if (!mr.entity->_enabled)
				continue;
			auto transform = mr.entity->transform;
			auto mesh = (MeshImpl*)mr.mesh;
			auto shader = (ShaderImpl*)mr.shader;
			if (transform->_dirty || mainCamera->transform->_dirty) {
				m4 mat = mainCamera->cBufferData.projection * m4::rotationYXZ(mainCamera->transform->_rotation) *
						 m4::translation(-mainCamera->transform->_position) * m4::translation(transform->_position) *
						 m4::rotationZXY(transform->_rotation);
				D3D11_MAPPED_SUBRESOURCE mapped{};
				DHR(immediateContext->Map(mainCamera->cBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
				memcpy(mapped.pData, &mat, sizeof(mainCamera->cBufferData));
				immediateContext->Unmap(mainCamera->cBuffer, 0);
			}
			immediateContext->VSSetShader(shader->vs, 0, 0);
			immediateContext->PSSetShader(shader->ps, 0, 0);
			immediateContext->VSSetShaderResources(0, 1, &mesh->vb.srv);
			immediateContext->VSSetShaderResources(1, 1, &mesh->ib.srv);
			immediateContext->Draw(mesh->indexCount, 0);
		}
	*/

	
	
	DHR(swapChain->Present(Window::isFullscreen(), 0));
}
R_FILL_RENDER_TARGET {
	ID3D11Resource* tex;
	renderTargets[0]->GetResource(&tex);
	immediateContext->UpdateSubresource(tex, 0, 0, data, Window::clientSize.x * sizeof(u32), 0);
	tex->Release();
}
R_BIND_SHADER {
	auto impl = (ShaderImpl*)shader;
	immediateContext->VSSetShader(impl->vs, 0, 0);
	immediateContext->PSSetShader(impl->ps, 0, 0);
}
#if 0
void D3D11::push(RenderCommand&& rc) {
	ASSERT(lastRenderCommand < renderCommandQueue + _countof(renderCommandQueue));
	new (lastRenderCommand++) RenderCommand(std::move(rc));
}
void D3D11::render(Window const& window) {
	PROFILE_FUNCTION;
/* 	RenderCommand* begin = renderCommandQueue;
	u32 commandCount = lastRenderCommand - renderCommandQueue;
	for (u32 i = 0; i < _countof(renderThreads); ++i) {
		auto& renderThread = renderThreads[i];
		RenderCommand* end = renderCommandQueue + (commandCount * (i + 1) / _countof(renderThreads));
		renderThread.work = {begin, end};
		ReleaseSemaphore(renderThread.beginSemaphore, 1, 0);
		begin = end;
	}
	for (u32 i = 0; i < _countof(renderThreads); ++i) {
		auto& renderThread = renderThreads[i];
		WaitForSingleObjectEx(renderThread.endSemaphore, INFINITE, 0);
		immediateContext->ExecuteCommandList(renderThread.commandList, true);
	} */
	auto visitor = Visitor{
		[](std::monostate) { INVALID_CODE_PATH("Invalid render command"); },
		[&](RC::Draw& drawCmd) { immediateContext->Draw(drawCmd.count, drawCmd.offset); },
		[&](RC::BindShader& bindCmd) {
			Shader& shader = shaders[bindCmd.id];
			immediateContext->VSSetShader(shader.vs, 0, 0);
			immediateContext->PSSetShader(shader.ps, 0, 0);
		},
		[&](RC::ClearRenderTarget& clearCmd) {
			immediateContext->ClearRenderTargetView(renderTargets[clearCmd.target.id], clearCmd.color.data());
		},
		[&](RC::ClearDepthTarget& clearCmd) {
			immediateContext->ClearDepthStencilView(depthTargets[clearCmd.target.id], D3D11_CLEAR_DEPTH, clearCmd.depth,
													0);
		},
		[&](RC::SetTargets& setCmd) {
			immediateContext->OMSetRenderTargets(1, renderTargets + setCmd.renderTarget.id,
												depthTargets[setCmd.depthTarget.id]);
		},
		[&](RC::UpdateBuffer& updateCmd) {
			D3D11_MAPPED_SUBRESOURCE mapped;
			DHR(immediateContext->Map(updateCmd.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
			memcpy(mapped.pData, updateCmd.getData(), updateCmd.getSize());
			immediateContext->Unmap(updateCmd.buffer, 0);
			updateCmd.release();
		}
	};
	// clang-format on
	for (RenderCommand* renderCommand = renderCommandQueue; renderCommand < lastRenderCommand; ++renderCommand) {
		std::visit(visitor, *renderCommand);
	}
	lastRenderCommand = renderCommandQueue;
	DHR(swapChain->Present(window.isFullscreen(), 0));
}
#endif
R_SET_MESH_VERTICES {
	PROFILE_FUNCTION;
	auto impl = (MeshImpl*)mesh;
	if (impl->vb.valid())
		impl->vb.release();
	impl->vb.create(device, data, stride, count);
}
R_SET_MESH_INDICES {
	PROFILE_FUNCTION;
	auto impl = (MeshImpl*)mesh;
	if (impl->ib.valid())
		impl->ib.release();
	impl->ib.create(device, data, stride, count);
	impl->indexCount = count;
}
R_DRAW_MESH {
	auto impl = (MeshImpl*)mesh;
	immediateContext->VSSetShaderResources(0, 1, &impl->vb.srv);
	immediateContext->VSSetShaderResources(1, 1, &impl->ib.srv);
	immediateContext->Draw(impl->indexCount, 0);
}
R_SET_MATRIX {
	switch (type) {
		case Renderer::MatrixType::mvp: matrixCBufferData.mvp = matrix; break;
		default: INVALID_CODE_PATH();
	}
	D3D11_MAPPED_SUBRESOURCE mapped{};
	DHR(immediateContext->Map(matrixCBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
	memcpy(mapped.pData, &matrixCBufferData, sizeof(matrixCBufferData));
	immediateContext->Unmap(matrixCBuffer, 0);
}
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
} // namespace D3D11