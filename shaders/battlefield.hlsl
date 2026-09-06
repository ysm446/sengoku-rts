cbuffer Camera : register(b0) {
    row_major float4x4 viewProjection;
    float4 cameraRight;
    float4 cameraUp;
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
    float3 rightAxis : RIGHTAXIS;
    float3 upAxis : UPAXIS;
    float pivot : PIVOT;
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
    bool posed = dot(input.rightAxis, input.rightAxis) > 0.001;
    float3 right = posed ? input.rightAxis : cameraRight.xyz;
    float3 world = input.position + right * ((corner.x - 0.5) * input.size.x);
    if (input.tile >= 4) {
        // Blender側ですでに俯瞰投影した画像は画面に平行な板へ置く。
        // Pivotは生成スクリプトと揃え、足元の原点を地形へ接地させる。
        float3 up = posed ? input.upAxis : cameraUp.xyz;
        float pivot = input.pivot > 0 ? input.pivot : input.tile == 12 ? 0.5 : input.tile >= 108 ? 0.75 : 0.88;
        world += up * ((pivot - corner.y) * input.size.y);
    } else world += (posed ? input.upAxis : float3(0, 1, 0)) * ((1 - corner.y) * input.size.y);
    SpriteOutput result;
    result.position = mul(float4(world, 1), viewProjection);
    // 隣のAtlasタイルを拾わないよう半Texel内側に収める。
    result.uv = float2(((input.tile % 12) * 64 + 0.5 + corner.x * 63) / 768,
                       ((input.tile / 12) * 64 + 0.5 + corner.y * 63) / 2176);
    result.tile = input.tile;
    result.tint = input.tint;
    return result;
}
float4 spritePS(SpriteOutput input) : SV_TARGET {
    float4 color = atlas.Sample(pointSampler, input.uv);
    clip(color.a - 0.5);
    // 灰色の鎧と明るい旗布だけを陣営色で染める。
    bool armor = (input.tile == 0 || input.tile >= 4) &&
        max(color.r, max(color.g, color.b)) - min(color.r, min(color.g, color.b)) < 0.04 && color.r > 0.25;
    bool banner = input.tile == 1 && color.r > 0.7;
    if (armor || banner) color.rgb *= input.tint;
    return color;
}
