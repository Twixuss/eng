#pragma once
#include "../../src/common.h"
struct Random {
	u32 seed;
	u32 u32() { return seed = randomize(seed); }
	s32 s32() { return (::s32)u32(); }
	bool u1() { return u32() & 0x10; }
	u32x4 u32x4() { return randomize(u32() ^ ::u32x4{0x0CB94D77, 0xA3579AD0, 0x207B3643, 0x97C0013D}); }
	u32x8 u32x8() {
		return randomize(u32() ^ ::u32x8{0x1800E669, 0x0635093E, 0x0CB94D77, 0xA3579AD0, 0x207B3643, 0x97C0013D,
										 0x3ACD6EE8, 0x85E0800F});
	}
	s32x4 s32x4() { return (::s32x4)u32x4(); }
	s32x8 s32x8() { return (::s32x8)u32x8(); }
	f32 f32() { return (u32() >> 8) * (1.0f / 0x1000000); }
	f32x4 f32x4() { return (::f32x4)(u32x4() >> 8) * (1.0f / 0x1000000); }
	f32x8 f32x8() { return (::f32x8)(u32x8() >> 8) * (1.0f / 0x1000000); }
	v2u v2u() { return {u32(), u32()}; }
	v2f v2f() { return {f32(), f32()}; }
	v4f v4f() { return V4f(f32x4()); }
	v3f v3f() { return V3f(f32(), f32(), f32()); }
	template <class T>
	T type() {
		return {};
	}
	template <>
	::s32x4 type<::s32x4>() {
		return s32x4();
	}
	template <>
	::s32x8 type<::s32x8>() {
		return s32x8();
	}
};