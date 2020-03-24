#pragma once
#include "..\eng.h"
struct Game;
namespace GameAPI {
	GAME_API Game* start();
	GAME_API void update(Game&);
	GAME_API void resize(Game&);
	GAME_API void shutdown(Game&);
	GAME_API WindowCreationInfo getWindowInfo();
}