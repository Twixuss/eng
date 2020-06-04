#include "common.hlsl"
struct V2P {
	float4 position : SV_Position;
	float4 color : COLOR;
};
#if defined COMPILE_VS
#define MVP_MATRIX matrices[0]
struct Line {
	float2 p[2];
	float4 color;
};
StructuredBuffer<Line> lines : register(t0);
void main(in uint i_id : SV_VertexID, out V2P o) {
	Line l = lines[i_id >> 1];
	o.position = mul(MVP_MATRIX, float4(l.p[i_id & 1], 0, 1));
	o.color = l.color;
}

#elif defined COMPILE_PS
void main(in V2P i, out float4 o_target : SV_Target) {
	o_target = i.color;
}

#else
#error "No shader"
#endif