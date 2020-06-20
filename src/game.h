#pragma once
#include "eng.h"

namespace GameApi {

GAME_API char const *getName();

/* Fills info required for initialization */
GAME_API void fillStartInfo(StartInfo &);

/* Called after worker threads and profiler are initalized */
GAME_API void init(EngState &);

/* Called after all remaining stuff is initialized */
GAME_API void start(EngState &, Window &, Renderer &, Input &, Time &);

/* Startup profile can be accessed from here */
GAME_API void debugStart(EngState &, Window &, Renderer &, Input &, Time &, Profiler::Stats const &startProfile);

/* Called when game is recompiled */
GAME_API void debugReload(EngState &);

/* Main loop iteration */
GAME_API void update(EngState &, Window &, Renderer &, Input &, Time &);

/* Frame profile can be retreived from here */
GAME_API void debugUpdate(EngState &, Window &, Renderer &, Input &, Time &, Profiler::Stats const &startProfile, Profiler::Stats const &updateProfile);

/*	This is being called from a separate thread whenever audio buffer needs an update. 
	Calling period can be specified in 'Audio::updatePeriod' */
GAME_API void fillSoundBuffer(EngState &, Audio &, s16* subsample, u32 sampleCount);

GAME_API void shutdown(EngState &);

} // namespace GameApi
