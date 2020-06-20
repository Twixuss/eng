struct V2P {
	float4 pos : SV_Position;
};
#if defined COMPILE_VS
void main(in uint id : SV_VertexID, out V2P o){
	const float2 positions[] = {
		float2(-1,-1),
		float2(-1, 3),
		float2( 3,-1),
	};
	o.pos = float4(positions[id], 0, 1);
}
#elif defined COMPILE_PS
Texture2D sourceTex : register(t0);

float greater(float a, float b) {
	return a > b ? 1 : -1;
}
float getTexelMultiplier(float x, int sampleCount) {
	//return max(0, 1-abs(roughness*(sampleCount/2-abs(x-normal * sampleCount + sampleCount/2*(normal-.5f>0?1:-1)))));
	return max(1-max(abs(abs(x-x/ROUGH_SAMPLE_COUNT*sampleCount+greater(x/ROUGH_SAMPLE_COUNT,0.5f)*sampleCount/2)-sampleCount/2)-sampleCount/4, 0), 0);
}

float4 main(in V2P i) : SV_Target {

#ifdef TO_SPECULAR

	float4 col = 0;
	int3 sampleP = int3(i.pos.xy, 0);
	sampleP.x *= LIGHT_SAMPLE_COUNT / ROUGH_SAMPLE_COUNT;
	for(int i = 0; i < LIGHT_SAMPLE_COUNT / ROUGH_SAMPLE_COUNT; ++i) {
		col += sourceTex.Load(sampleP + int3(i, 0, 0));
	}
	return col * ((float)ROUGH_SAMPLE_COUNT / LIGHT_SAMPLE_COUNT);

#elif defined TO_DIFFUSE

	uint2 voxel = i.pos.xy;
	voxel.x = voxel.x / ROUGH_SAMPLE_COUNT * ROUGH_SAMPLE_COUNT;
	//return roughVoxTex.Load(int3(voxel, 0));
	//voxel.x *= ROUGH_SAMPLE_COUNT;
	float4 result = 0;
	for (int x = 0; x < ROUGH_SAMPLE_COUNT/2; ++x) {
		uint2 samplePos = uint2(i.pos.x + x - ROUGH_SAMPLE_COUNT/4, i.pos.y);
		uint srcVoxel = samplePos.x / ROUGH_SAMPLE_COUNT * ROUGH_SAMPLE_COUNT;
		if (srcVoxel > voxel.x) { samplePos.x -= ROUGH_SAMPLE_COUNT; }
		if (srcVoxel < voxel.x) { samplePos.x += ROUGH_SAMPLE_COUNT; }
		result += sourceTex.Load(int3(samplePos, 0));
	}
	return result / (ROUGH_SAMPLE_COUNT/2);

#elif defined TO_POINT
	
	//return sourceTex.Load(i.pos.xyz * int3(ROUGH_SAMPLE_COUNT,1,1));

	float4 col = 0;
	int3 sampleP = int3(i.pos.xy, 0);
	sampleP.x *= ROUGH_SAMPLE_COUNT;
	for(int i = 0; i < ROUGH_SAMPLE_COUNT; ++i) {
		col += sourceTex.Load(sampleP + int3(i, 0, 0));
	}
	return col / ROUGH_SAMPLE_COUNT;

#endif
}
#endif