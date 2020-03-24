struct V2P {
	float3 normal : NORMAL;
	float2 uv : UV;
};
#if defined COMPILE_VS

cbuffer test {
	matrix testMatrix;
};
struct Vertex {
	float3 position;
	float3 normal;
	float2 uv;
};
StructuredBuffer<Vertex> vertices;
StructuredBuffer<uint> indices;
void vs_main(in uint i_id : SV_VertexID, out V2P o, out float4 o_position : SV_Position) {
	Vertex v = vertices[indices[i_id]];
	o_position = mul(testMatrix, float4(v.position, 1));
	//const float3 positions[] = {
	//	float3(-.5,-.5,.5),
	//	float3( .0, .5,.5),
	//	float3( .5,-.5,.5),
	//};
	//o_position = mul(testMatrix, float4(positions[i_id], 1));
	o.normal = v.normal;
	o.uv = v.uv;
}

#elif defined COMPILE_PS

void ps_main(in V2P i, out float4 o_target : SV_Target) {
	float3 col = float3(.9,.6, .3);
	col *= saturate(dot(normalize(float3(1,2,3)), normalize(i.normal)));
	col.xy += i.uv;
	col = i.normal;
	o_target = float4(col, 0);
}

#else
#error "No shader"
#endif