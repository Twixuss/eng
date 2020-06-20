#include "common.hlsl"
#define VOXEL_MATRIX matrices[0]
#define MENU_ALPHA g_v4f[0].x
#define DEATH_SCREEN_ALPHA g_v4f[0].y
struct V2P {
	float4 position : SV_Position;
	float2 voxelUv : VUV;
	float2 uv : UV;
	float2 deathUv : DUV;
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
	o.deathUv = o.uv * 2 - 1;
}
#elif defined COMPILE_PS
Texture2D albedoTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D lightTexture : register(t2);
Texture2D diffuseVoxTex : register(t3);
Texture2D roughVoxTex : register(t4);
Texture2D specVoxTex : register(t5);
Texture2D diffusePointTex : register(t6);
SamplerState albedoSampler : register(s0);
SamplerState normalSampler : register(s1);
SamplerState lightSampler : register(s2);
SamplerState diffuseVoxSampler : register(s3);
SamplerState roughVoxSampler : register(s4);
SamplerState specVoxSampler : register(s5);
SamplerState diffusePointSampler : register(s6);

float greater(float a, float b) {
	return a > b ? 1 : -1;
}

float getTexelMultiplier(float x, float roughness, float normal, int sampleCount) {
	//return max(0, 1-abs(gloss*(sampleCount/2-abs(x-normal * sampleCount + sampleCount/2*(normal-.5f>0?1:-1)))));
	return max(1-max(abs(abs(x-normal*sampleCount+greater(normal,0.5f)*sampleCount/2)-sampleCount/2)-roughness*sampleCount/4, 0), 0);
}
float4 sampleVoxel(float normal, float roughness, int2 voxel) {
	voxel.x *= ROUGH_SAMPLE_COUNT;
	//roughVoxTex.Load(int3(voxel.x + normal * 4, voxel.y, 0));
	float4 result = 0;
	for (int x = 0; x < ROUGH_SAMPLE_COUNT; ++x) {
		float mul = getTexelMultiplier(x, roughness, normal, ROUGH_SAMPLE_COUNT);
		result += roughVoxTex.Load(int3(voxel.x + x, voxel.y, 0)) * mul;
	}
	return result / ROUGH_SAMPLE_COUNT;
}
float4 sampleVoxelBilinear(float normal, float roughness, float2 uv) {
	uv *= LIGHT_ATLAS_DIM;
	uv -= .5f;
	float fx = floor(uv.x);
	float fy = floor(uv.y);
	roughness = roughness * 0.5f + 0.5f;
	return lerp(lerp(sampleVoxel(normal, roughness, int2(fx,     fy    )), 
					 sampleVoxel(normal, roughness, int2(fx + 1, fy    )), frac(uv.x)), 
				lerp(sampleVoxel(normal, roughness, int2(fx,     fy + 1)),  
					 sampleVoxel(normal, roughness, int2(fx + 1, fy + 1)), frac(uv.x)), frac(uv.y));
}
float4 sampleDiffuse(float normal, float upness, int2 voxel) {
	voxel.x *= ROUGH_SAMPLE_COUNT;
	//return diffuseVoxTex.Load(int3(voxel, 0));

	float s = frac(normal) * ROUGH_SAMPLE_COUNT;
	float s0 = floor(s);
	if (s0 > (ROUGH_SAMPLE_COUNT-1)) s0 = 0;
	float s1 = s0 + 1;
	if (s1 > (ROUGH_SAMPLE_COUNT-1)) s1 = 0;

	return lerp(diffuseVoxTex.Load(int3(voxel.x + s0, voxel.y, 0)), 
				diffuseVoxTex.Load(int3(voxel.x + s1, voxel.y, 0)), frac(s));
}
float4 sampleDiffuseBilinear(float normal, float upness, float2 uv) {
	float2 baseUv = uv;
	uv *= LIGHT_ATLAS_DIM;
	uv -= .5f;
	float fx = floor(uv.x);
	float fy = floor(uv.y);
	float4 result = lerp(lerp(sampleDiffuse(normal, upness, int2(fx,     fy    )), 
							  sampleDiffuse(normal, upness, int2(fx + 1, fy    )), frac(uv.x)), 
						 lerp(sampleDiffuse(normal, upness, int2(fx,     fy + 1)),  
							  sampleDiffuse(normal, upness, int2(fx + 1, fy + 1)), frac(uv.x)), frac(uv.y));
	return lerp(result, diffusePointTex.Sample(diffusePointSampler, baseUv), upness);
}
float getNormalAngle(float4 data) { 
	return data.x; 
	//data.xy = normalize(data.xy * 2 - 1);
	//return frac(map(atan2(data.x, data.y), -PI, PI, 0.5, 1.5));
}
float getUpness(float4 data) { 
	return data.y; 
	//data.xy = data.xy * 2 - 1;
	//return sqrt(1-(data.x*data.x + data.y*data.y));
}
float getSpecMask(float upness) {
	float result = sin(asin(upness) * 2);
	return result * result * result;
}
float4 main(in V2P i) : SV_Target {
	//return float4(i.voxelUv, 0, 1);
	//return specVoxTex.Sample(specVoxSampler, float2(floor(i.voxelUv.x * LIGHT_ATLAS_DIM.x) / LIGHT_ATLAS_DIM.x, i.voxelUv.y));
	//return specVoxTex.Sample(specVoxSampler, i.uv);
	//return roughVoxTex.Sample(roughVoxSampler, i.uv);
	//return float4(i.uv, 0, 1);
	//return roughVoxTex.Load(int3(i.position.xy / 3, 0));
	//return diffuseVoxTex.Load(int3(i.position.xy / 3 / float2(1,ROUGH_SAMPLE_COUNT), 0));
	//return roughVoxTex.Sample(roughVoxSampler, i.voxelUv);
	//return diffusePointTex.Sample(diffusePointSampler, i.voxelUv);

	float4 albedo = albedoTex.Sample(albedoSampler, i.uv);
	float4 normal = normalTex.Sample(normalSampler, i.uv);
	float4 light = lightTexture.Sample(lightSampler, i.uv);
	

	float2 normal2 = normalize(normal.xy);
	//return float4(normal2, 0, 1);
	float normalAngle = getNormalAngle(normal);
	//return normalAngle;
	
	float upness = getUpness(normal.y);
	//return upness;

	float roughness = 1-normal.z*2;
	//return roughness;
	
	float specMask = getSpecMask(upness); 
	
	//return normalAngle;
	//return normal.x;
	//normalAngle = lerp(normalAngle, 1, 0.25f);

	//return float4(((floor(i.voxelUv.x * LIGHT_ATLAS_DIM.x) + normalAngle) / LIGHT_ATLAS_DIM.x - i.voxelUv.x) * LIGHT_ATLAS_DIM.x, 0, 0, 1);

	//return diffuseVoxTex.Load(int3(i.voxelUv * float2(ROUGH_SAMPLE_COUNT, 1) + normalAngle * ROUGH_SAMPLE_COUNT, 0));

	//float4 diffuseVox = sampleVoxelBilinear(normalAngle, normal.y*normal.y*normal.y, 1, i.voxelUv);
	float4 diffuseVox = sampleDiffuseBilinear(normalAngle, upness*upness*upness, i.voxelUv);
	float4 specVox = sampleVoxelBilinear(normalAngle, roughness, i.voxelUv) * (normal.z * 0.8f + 0.2f);
	float4 roughVox = 0;//sampleRough(normalAngle, i.voxelUv);
	//if(i.voxelUv.x < 0 || i.voxelUv.x > 1 || i.voxelUv.y < 0 || i.voxelUv.y > 1)
	//	diffuseVox = 1;
	float deathMask = min(distanceSqr(i.deathUv, float2(0,0)), 1);
	deathMask *= deathMask;
	//return specVox * specMask;
	//return albedo * diffuseVox;
	//return albedo * (diffuseVox + specVox);
	//return albedo*diffuseVox;
	//return diffuseVox;
	//return roughVox * 20 * normal.y - .5;
	//return (albedo * (light + diffuseVox) * (1 - MENU_ALPHA)) * lerp(1, deathMask, DEATH_SCREEN_ALPHA);
	float4 diffuse = albedo * (light + diffuseVox);
	float4 specular = specVox * specMask;//lerp(diffuseVox, lerp(roughVox, specVox, saturate(normal.y * 2 - 1)), saturate(normal.y * 2)) * .1 * gloss;
	float4 color = diffuse + specular;
	//return diffuse;
	//return specular;
	//return albedo;
	//if (normal.a == 0)
	//	return albedo;
	return lerp(albedo, color, normal.a) * (1 - MENU_ALPHA) * lerp(1, deathMask, DEATH_SCREEN_ALPHA);
	return (specVox * 4 + albedo * (light + diffuseVox + roughVox) * (1 - MENU_ALPHA)) * lerp(1, deathMask, DEATH_SCREEN_ALPHA);
}
#endif