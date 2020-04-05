#include "common.hlsl"
struct V2P {
	float4 position : SV_Position;
	float4 color : COLOR;
	float2 uv : UV;
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
	float2 uv;
	float2 uvScale;
	float rotation;
	float3 padding;
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
		c, -s,
		s, c
	};

	float2 position = tile.position + mul(offsets[vertexIndex], rotMat) * tile.size;
	o.uv = tile.uv + uvs[vertexIndex] * tile.uvScale;
	o.position = mul(MVP_MATRIX, float4(position, 0, 1));
	o.color = tile.color;
}

#elif defined COMPILE_PS
Texture2D atlas : register(t0);
SamplerState atlasSampler : register(s0);
void main(in V2P i, out float4 o_target : SV_Target) {
	o_target = atlas.Sample(atlasSampler, i.uv) * i.color;
}

#else
#error "No shader"
#endif