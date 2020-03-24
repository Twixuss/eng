struct V2P {
	float2 uv : UV;
};
#if defined COMPILE_VS

cbuffer test {
	matrix testMatrix;
};
struct Vertex {
	float3 position;
	float2 uv;
};
StructuredBuffer<Vertex> vertices;
StructuredBuffer<uint> indices;
void vs_main(in uint i_id : SV_VertexID, out V2P o, out float4 o_position : SV_Position) {
	Vertex v = vertices[indices[i_id]];
	o_position = mul(testMatrix, float4(v.position, 1));
	o.uv = v.uv;
}

#elif defined COMPILE_PS

void ps_main(in V2P i, out float4 o_target : SV_Target) {
	o_target = float4(i.uv, 0, 0);
}

#else
#error "No shader"
#endif