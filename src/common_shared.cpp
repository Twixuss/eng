#include "common.h"

extern "C" struct IMAGE_DOS_HEADER __ImageBase;
ENG_API Span<char const> _getModuleName(void *);

namespace Log {

static Span<char const> moduleName = _getModuleName(&__ImageBase);
Span<char const> getModuleName() { return moduleName; }

}