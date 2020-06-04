#pragma once
#include "eng.h"
namespace GameApi {

struct State {
	void *userData;
};

/* Fills info required for initialization */
GAME_API void fillStartInfo(StartInfo &);

/* Called after worker threads and profiler are initalized */
GAME_API void init(State &);

/* Called after all remaining stuff is initialized */
GAME_API void start(State &, Window &, Renderer &, Input &, Time &);

/* Startup profile can be accessed from here */
GAME_API void debugStart(State &, Window &, Renderer &, Input &, Time &, Profiler::Stats &&);

/* Called when game is recompiled */
GAME_API void debugReload(State &);

/* Main loop iteration */
GAME_API void update(State &, Window &, Renderer &, Input &, Time &);

/* Frame profile can be retreived from here */
GAME_API void debugUpdate(State &, Window &, Renderer &, Input &, Time &, Profiler::Stats &&);

/*	This is being called from a separate thread whenever audio buffer needs an update. 
	Calling period can be specified in 'Audio::updatePeriod' */
GAME_API void fillSoundBuffer(State &, Audio &, s16* subsample, u32 subsampleCount);

GAME_API void shutdown(State &);

} // namespace GameApi
