cbuffer screenInfo : register(b0) {
	float2 screenSize;
	float2 invScreenSize;
	float aspectRatio;
};
cbuffer m : register(b1) {
	float4x4 matrices[8];
	float4 g_v4f[8];
};
cbuffer frameInfo : register(b2) {
	float g_time;
}
float lengthSqr(float2 a) { return dot(a,a); }
float distanceSqr(float2 a, float2 b) { return lengthSqr(a - b); }
float map(float v, float si, float sa, float di, float da) {
	return (v - si) / (sa - si) * (da - di) + di;
}
#define PI 3.1415926535897932384626433832795