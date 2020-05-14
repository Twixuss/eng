#include "renderer.h"

char const* getRendererName(RenderingApi api) {
	switch (api) {
		case RenderingApi::d3d11:  return "D3D11";
		case RenderingApi::opengl: return "OpenGL";
		default: return "";
	}
}
