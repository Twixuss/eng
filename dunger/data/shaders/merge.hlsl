struct V2P {
	float4 position : SV_Position;
	float2 uv : UV;
};
#if defined COMPILE_VS
void main(in uint id : SV_VertexID, out V2P o){
	const float2 positions[] = {
		float2(-1,-1),
		float2(-1, 3),
		float2( 3,-1),
	};
	o.position = float4(positions[id], 0, 1);
	o.uv = o.position * 0.5f + 0.5f;
	o.uv.y = 1 - o.uv.y;
}
#elif defined COMPILE_PS
Texture2D diffuseTexture : register(t0);
Texture2D lightTexture : register(t1);
SamplerState testSampler : register(s0);
float4 main(in V2P i) : SV_Target {
	float4 diffuse = diffuseTexture.Sample(testSampler, i.uv);
	float4 light = lightTexture.Sample(testSampler, i.uv);
	return diffuse * light;
}
#endif