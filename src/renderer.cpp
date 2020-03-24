#include "renderer.h"
#include "allocator.h"


namespace Renderer {
#undef R_DECORATE
#define R_DECORATE(type, name) type(*name)
R_ALLFUNS;
} // namespace Renderer

Mesh* Mesh::create() { return Renderer::createMesh(); }
