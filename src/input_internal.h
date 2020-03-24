#pragma once
#include "common.h"
namespace Input {
ENG_API void init();
ENG_API void shutdown();
ENG_API void swap();
ENG_API void reset();
ENG_API void processKey(u32 key, bool extended, bool alt, bool isRepeated, bool wentUp);
ENG_API void setMouseButton(u32 i, bool b);
} // namespace Input
