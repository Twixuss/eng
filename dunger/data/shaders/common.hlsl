cbuffer screenInfo : register(b0) {
	float2 screenSize;
	float2 invScreenSize;
	float aspectRatio;
};
cbuffer m : register(b1) {
	float4x4 matrices[8];
	float4 g_v4f[8];
};
float lengthSqr(float2 a) { return dot(a,a); }
float distanceSqr(float2 a, float2 b) { return lengthSqr(a - b); }