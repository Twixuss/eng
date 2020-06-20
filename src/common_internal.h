#include "common.h"
namespace Profiler {

ENG_API Stats getStats();

}

namespace Log {

ENG_API void setHandle(void *);

}

ENG_API void loadOptimizedModule();

ENG_API void resetTempStorage();
