#include "common.hlsl"
struct v2p {
	float4 position : SV_Position;
	float3 color : COLOR;
	float2 uv : UV;
};
#if defined COMPILE_VS
struct Light {
	float3 color;
	float radius;
	float2 position;
};
StructuredBuffer<Light> lights : register(t0);
void main(in uint id : SV_VertexID, out v2p o){
	const float2 offsets[] = {
		float2(-.5*aspectRatio,-.5),
		float2(-.5*aspectRatio, .5),
		float2( .5*aspectRatio,-.5),
		float2( .5*aspectRatio, .5),
	};
	const float2 uvs[] = {
		float2(-1, 1),
		float2(-1,-1),
		float2( 1, 1),
		float2( 1,-1),
	};
	const uint map[6] = { 0,1,2,2,1,3 };
	uint vertexIndex = map[id%6];
	uint lightIndex = id/6;
	Light l = lights[lightIndex];
	o.uv = uvs[vertexIndex];
	o.color = l.color;
	o.position = float4(l.position + offsets[vertexIndex] * l.radius, 0, 1);
}
#elif defined COMPILE_PS
float pow2(float v) { return v*v; }
float pow3(float v) { return v*v*v; }
float invpow2(float v) { return 1-pow2(1-v); }
float4 main(in v2p i) : SV_Target {
	//return float4(i.color/(lenSqr(i.uv)+1), 1);
	return float4(pow3(max(0, 1-lengthSqr(i.uv))) * i.color, 1);
}
#endif