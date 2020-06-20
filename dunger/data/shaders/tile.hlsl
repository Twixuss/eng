#include "common.hlsl"
struct V2P {
	float4 position : SV_Position;
	float4 color : COLOR;
#ifndef SOLID
	float2 uv0 : UV0;
	float2 uv1 : UV1;
	float uvMix : UV_MIX;
#endif
};
#if defined COMPILE_VS
#define ATLAS_SIZE 2
#define INV_ATLAS_SIZE (1.0f/ATLAS_SIZE)
#define TILE_SIZE 64
#define INV_TILE_SIZE (1.0f/TILE_SIZE)
#define MVP_MATRIX matrices[0]
struct Tile {
	float4 color;
	float2 position;
	float2 size;
	float2 uv0;
	float2 uv1;
	float2 uvScale;
	float uvMix;
	float rotation;
};
StructuredBuffer<Tile> tiles : register(t0);
#define UV_CROP 0.01f
void main(in uint i_id : SV_VertexID, out V2P o) {
	const float2 offsets[] = {
		float2(-.5,-.5),
		float2(-.5, .5),
		float2( .5,-.5),
		float2( .5, .5),
	};
	const float2 uvs[] = {
		float2(0+UV_CROP,1-UV_CROP),
		float2(0+UV_CROP,0+UV_CROP),
		float2(1-UV_CROP,1-UV_CROP),
		float2(1-UV_CROP,0+UV_CROP),
	};
	const uint map[6] = { 0,1,2,2,1,3 };

	uint tileIndex = i_id / 6;
	uint vertexIndex = map[i_id % 6];

	Tile tile = tiles[tileIndex];

	float s, c;
	sincos(tile.rotation, s, c);

	float2x2 rotMat = {
		c, s,
		-s, c
	};

#ifndef SOLID
	o.uv0 = tile.uv0 + uvs[vertexIndex] * tile.uvScale;
	o.uv1 = tile.uv1 + uvs[vertexIndex] * tile.uvScale;
	o.uvMix = tile.uvMix;
#endif
	float2 position = tile.position + mul(offsets[vertexIndex] * tile.size, rotMat);
	o.position = mul(MVP_MATRIX, float4(position, 0, 1));
	o.color = tile.color;
}

#elif defined COMPILE_PS
struct P2O {
	float4 albedo : SV_Target0;
#ifdef NORMAL_OUTPUT
	float4 normal : SV_Target1;
#endif
};

Texture2D albedoTex : register(t0);
Texture2D normalTex : register(t1);
SamplerState albedoSampler : register(s0);
SamplerState normalSampler : register(s1);
void main(in V2P i, out P2O o) {

#ifdef SOLID

	o.albedo = i.color;

#else

	o.albedo = lerp(albedoTex.Sample(albedoSampler, i.uv0), albedoTex.Sample(albedoSampler, i.uv1), i.uvMix);
#ifdef CLIP_ALPHA
	clip(o.albedo.a - 0.25f);
#endif
	o.albedo *= i.color;
#ifdef NORMAL_OUTPUT
	o.normal = lerp(normalTex.Sample(normalSampler, i.uv0), normalTex.Sample(normalSampler, i.uv1), i.uvMix);
#endif

#endif
}

#else
#error "No shader"
#endif