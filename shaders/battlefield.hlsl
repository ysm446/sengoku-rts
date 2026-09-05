cbuffer Camera : register(b0) {
    row_major float4x4 viewProjection;
};
Texture2D atlas : register(t0);
SamplerState pointSampler : register(s0);

struct TerrainInput { float3 position : POSITION; float3 color : COLOR; };
struct TerrainOutput { float4 position : SV_POSITION; float3 color : COLOR; float3 world : TEXCOORD; };
TerrainOutput terrainVS(TerrainInput input) {
    TerrainOutput result;
    result.position = mul(float4(input.position, 1), viewProjection);
    result.color = input.color;
    result.world = input.position;
    return result;
}
float4 terrainPS(TerrainOutput input) : SV_TARGET {
    float2 cell = floor(input.world.xz * 8);
    float grain = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);
    float variation = grain > 0.88 ? 0.91 : 1.0;
    return float4(input.color * variation, 1);
}

struct SpriteInput {
    float3 position : POSITION;
    float2 size : SIZE;
    uint tile : TILE;
    float3 tint : COLOR;
};
struct SpriteOutput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    nointerpolation uint tile : TILE;
    float3 tint : COLOR;
};
SpriteOutput spriteVS(SpriteInput input, uint id : SV_VertexID) {
    const float2 corners[6] = {
        float2(0, 1), float2(0, 0), float2(1, 1),
        float2(1, 1), float2(0, 0), float2(1, 0)
    };
    float2 corner = corners[id];
    float3 right = float3(-0.70710678, 0, 0.70710678);
    float3 world = input.position + right * ((corner.x - 0.5) * input.size.x);
    world.y += (1 - corner.y) * input.size.y;
    SpriteOutput result;
    result.position = mul(float4(world, 1), viewProjection);
    // 隣のAtlasタイルを拾わないよう半Texel内側に収める。
    result.uv = float2((input.tile * 32 + 0.5 + corner.x * 31) / 128, (0.5 + corner.y * 47) / 48);
    result.tile = input.tile;
    result.tint = input.tint;
    return result;
}
float4 spritePS(SpriteOutput input) : SV_TARGET {
    float4 color = atlas.Sample(pointSampler, input.uv);
    clip(color.a - 0.5);
    // 灰色の鎧と明るい旗布だけを陣営色で染める。
    bool armor = input.tile == 0 && abs(color.r - color.g) < 0.04 && color.r > 0.35;
    bool banner = input.tile == 1 && color.r > 0.7;
    if (armor || banner) color.rgb *= input.tint;
    return color;
}
