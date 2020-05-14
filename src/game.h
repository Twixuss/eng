#pragma once
#include "eng.h"
struct GameState;
namespace GameAPI {
GAME_API GameState *start(Window &, Renderer &);
GAME_API void update(GameState &, Window &, Renderer &, Input const &, Time &);
GAME_API void endFrame(GameState &, Window &, Renderer &, Input const &, Time &);
GAME_API void fillSoundBuffer(GameState &, Audio const &, s16*, u32);
GAME_API void shutdown(GameState &);
GAME_API void fillStartInfo(StartInfo &);
} // namespace GameAPI
