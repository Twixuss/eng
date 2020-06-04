#include "common.hlsl"
#define VOXEL_MATRIX matrices[0]
#define MENU_ALPHA g_v4f[0].x
#define DEATH_SCREEN_ALPHA g_v4f[0].y
struct V2P {
	float4 position : SV_Position;
	float2 uv : UV;
	float2 voxelUv : VUV;
};
#if defined COMPILE_VS
void main(in uint id : SV_VertexID, out V2P o){
	const float2 positions[] = {
		float2(-1,-1),
		float2(-1, 3),
		float2( 3,-1),
	};
	o.position = float4(positions[id], 0, 1);
	o.uv = o.position.xy * 0.5f + 0.5f;
	o.voxelUv = mul(VOXEL_MATRIX, o.position).xy * 0.5f + 0.5f;
	o.uv.y = 1 - o.uv.y;
}
#elif defined COMPILE_PS
Texture2DMS<float4> diffuseTexture : register(t0);
Texture2D lightTexture : register(t1);
Texture2D voxelsTexture : register(t2);
SamplerState diffuseSampler : register(s0);
SamplerState lightSampler : register(s1);
SamplerState voxelsSampler : register(s2);
float4 main(in V2P i) : SV_Target {
	float4 diffuse = 0;
	for (int j = 0;j < MSAA_SAMPLE_COUNT; ++j)
		diffuse += diffuseTexture.Load(i.position.xy, j);
	diffuse *= (1.0f / MSAA_SAMPLE_COUNT);

	float4 light = lightTexture.Sample(lightSampler, i.uv);
	float4 voxels = voxelsTexture.Sample(voxelsSampler, i.voxelUv);
	//if(i.voxelUv.x < 0 || i.voxelUv.x > 1 || i.voxelUv.y < 0 || i.voxelUv.y > 1)
	//	voxels = 1;
	float deathMask = min(distanceSqr(i.uv*2-1, float2(0,0)), 1);
	deathMask *= deathMask;
	return diffuse * (light + voxels) * (1 - MENU_ALPHA) * lerp(1, deathMask, DEATH_SCREEN_ALPHA);
}
#endif