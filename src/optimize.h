#pragma once
#include "common.h"

struct OptimizedProc {
	char const *name;
};

#define OPTIMIZE_EXPORT extern "C" __declspec(dllexport)
