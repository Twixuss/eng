#include "../allocator.h"
#include "game.h"
#include <vector>
struct Cell {
	v2u index{};
	v3 position{};
	void update(v2u clientSize, v2i mousePos) {
		v2i bottom = V2i(clientSize * index / 3);
		v2i top = V2i(clientSize * (index + 1) / 3);
		v3 tp = position;
		if (mousePos.x > bottom.x && mousePos.y > bottom.y && mousePos.x <= top.x && mousePos.y <= top.y) {
			tp.z = -0.5f;
		} else {
			tp.z = 0;
		}
		position = lerp(position, tp, Time::frameTime * 5);
	}
	m4 getMatrix() { return m4::translation(position); }

private:
};
struct GAME_API Game {
	Mesh* cellMesh;
	m4 cameraMatrix;
	Cell cells[9];
	Game() {
		auto testShader = Renderer::createShader(LDATA "shaders/ui.shader");
		Renderer::bindShader(testShader);
		cellMesh = Mesh::create();
		struct Vertex {
			v3 position;
			v2 uv;
		};
		Vertex vertices[4]{
			{{-1, -1, 0.5f}, {0, 0}},
			{{-1, 1, 0.5f}, {0, 1}},
			{{1, 1, 0.5f}, {1, 1}},
			{{1, -1, 0.5f}, {1, 0}},
		};
		u32 indices[6]{0, 1, 3, 1, 2, 3};
		cellMesh->setVertices(vertices, sizeof(vertices[0]), _countof(vertices));
		cellMesh->setIndices(indices, sizeof(indices[0]), _countof(indices));

		for (i32 i = 0; i < 9; ++i) {
			auto& c = cells[i];
			c.index = V2u(i / 3, 2 - i % 3);
			c.position = V3(i / 3, i % 3, 1) * 2 - 2;
		}
	}
	void resize() {
		cameraMatrix = m4::perspective(aspectRatio(Window::getClientSize()), PI / 2, .01f, 1000.f) *
					   m4::translation(0, 0, 3);
	}
	void update() {
		auto cs = Window::getClientSize();
		auto mp = Input::mousePosition();
		for (auto& c : cells) {
			c.update(cs, mp);
			Renderer::setMatrix(Renderer::MatrixType::mvp, cameraMatrix * c.getMatrix());
			Renderer::drawMesh(cellMesh);
		}
	}
	~Game() {}
};
namespace GameAPI {
WindowCreationInfo getWindowInfo() {
	WindowCreationInfo info{};
	info.resizeable = false;
	info.clientSize = {800, 800};
	return info;
}
Game* start() { return new Game; }
void update(Game& game) { return game.update(); }
void resize(Game& game) { return game.resize(); }
void shutdown(Game& game) { delete &game; }
} // namespace GameAPI