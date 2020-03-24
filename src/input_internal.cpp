#include "input_internal.h"
namespace Input {

struct State {
	bool keys[256]{};
	bool mouse[8]{};
	v2i mousePosition, mouseDelta;
};
static State current, previous;

void init() {
	PROFILE_FUNCTION;
	RAWINPUTDEVICE mouse = {};
	mouse.usUsagePage = 0x01;
	mouse.usUsage = 0x02;

	if (!RegisterRawInputDevices(&mouse, 1, sizeof(RAWINPUTDEVICE))) {
		ASSERT(0);
	}
}
void shutdown() {}
void swap() {
	previous = current;
	current.mouseDelta = {};
}
void reset() {
	memset(current.keys, 0, sizeof(current.keys));
	memset(current.mouse, 0, sizeof(current.mouse));
}
void processKey(u32 key, bool extended, bool alt, bool isRepeated, bool wentUp) {
	if (isRepeated == wentUp) { // Don't handle repeated
		current.keys[key] = !isRepeated;
	}
}
void setMouseButton(u32 i, bool b) { current.mouse[i] = b; }

} // namespace Input