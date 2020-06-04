#pragma once
#include "input.h"

void swap(Input &input) {
	input.previous = input.current;
	input.mouseDelta = {};
	input.mouseWheel = {};
}
void reset(Input &input) {
	memset(&input.current, 0, sizeof(input.current));
}
