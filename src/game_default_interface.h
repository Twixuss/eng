#pragma once
#include "game.h"
namespace GameApi {
GameState* start(Window& window, Renderer& renderer) { return new GameState(window, renderer); }
void update(GameState& game, Window& window, Renderer& renderer, Input const& input, Time& time) {
	return game.update(window, renderer, input, time);
}
void shutdown(GameState& game) { delete &game; }
} // namespace GameApi