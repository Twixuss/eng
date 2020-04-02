#pragma once
#include "eng.h"
struct Game;
namespace GameAPI {
	GAME_API Game* start(Window&, Renderer&);
	GAME_API void update(Game&, Window&, Renderer&, Input const&, Time const&);
	GAME_API void shutdown(Game&);
	GAME_API void fillStartupInfo(StartupInfo&);
}
