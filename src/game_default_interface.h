#pragma once
#include "game.h"
namespace GameAPI {
Game* start(Window& window, Renderer& renderer) { return new Game(window, renderer); }
void update(Game& game, Window& window, Renderer& renderer, Input const& input, Time& time) {
	return game.update(window, renderer, input, time);
}
void shutdown(Game& game) { delete &game; }
} // namespace GameAPI