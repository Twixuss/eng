#pragma once
#pragma warning(push, 0)
#include <Windows.h>
#pragma warning(pop)

#define DHR(call)                      \
	do {                               \
		HRESULT dhr = call;            \
		ASSERT(SUCCEEDED(dhr), #call); \
	} while (0)

#define RELEASE_ZERO(x)   \
	do {                  \
		if (x) {          \
			x->Release(); \
			x = 0;        \
		}                 \
	} while (0)
