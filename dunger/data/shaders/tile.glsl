#if defined COMPILE_VS
#define V2P out
#elif defined COMPILE_PS
#define V2P in
#endif
V2P vec4 position;
V2P vec4 color;
V2P vec2 uv;

#if defined COMPILE_VS

#define ATLAS_SIZE 2
#define INV_ATLAS_SIZE (1.0f/ATLAS_SIZE)
#define TILE_SIZE 64
#define INV_TILE_SIZE (1.0f/TILE_SIZE)
#define MVP_MATRIX matrices[0]

#define MAX_TILES 1024

struct Tile {
	vec2 position;
	vec2 uv;
	vec4 color;
	float size;
	float rotation;
	vec2 padding;
};

uniform Tile tiles[MAX_TILES];

void main() {
	vec2 offsets[4];
	offsets[0] = vec2(-.5,-.5);
	offsets[1] = vec2(-.5, .5);
	offsets[2] = vec2( .5,-.5);
	offsets[3] = vec2( .5, .5);

	vec2 uvs[4];
	uvs[0] = INV_ATLAS_SIZE * vec2(  INV_TILE_SIZE,1-INV_TILE_SIZE);
	uvs[1] = INV_ATLAS_SIZE * vec2(  INV_TILE_SIZE,  INV_TILE_SIZE);
	uvs[2] = INV_ATLAS_SIZE * vec2(1-INV_TILE_SIZE,1-INV_TILE_SIZE);
	uvs[3] = INV_ATLAS_SIZE * vec2(1-INV_TILE_SIZE,  INV_TILE_SIZE);

	uint map[6];
	map[0] = 0u;
	map[1] = 1u;
	map[2] = 2u;
	map[3] = 2u;
	map[4] = 1u;
	map[5] = 3u;
	

	uint tileIndex = uint(gl_VertexID) / 6u;
	uint vertexIndex = map[uint(gl_VertexID) % 6u];

	Tile tile = tiles[tileIndex];

	float s = sin(tile.rotation);
	float c = cos(tile.rotation);

	mat2 rotMat = mat2(
		c, -s,
		s, c
	);

	vec2 position = tile.position + (rotMat * offsets[vertexIndex]) * tile.size;
	uv = tile.uv + uvs[vertexIndex];
	position = mul(MVP_MATRIX, vec4(position, 0, 1));
	color = tile.color;
}

#elif defined COMPILE_PS

Texture2D atlas : register(t0);
SamplerState atlasSampler : register(s0);

out vec4 o_target;
void main() {
	o_target = atlas.Sample(atlasSampler, uv) * color;
}

#endif