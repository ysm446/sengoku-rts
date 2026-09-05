#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <vector>

struct TerrainVertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 color;
};

// 描画専用データ。兵士個体のシミュレーション状態は持たない。
struct SpriteInstance {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT2 size;
    std::uint32_t tile;
    DirectX::XMFLOAT3 tint;
};

struct Scene {
    static constexpr unsigned tileWidth = 32;
    static constexpr unsigned tileHeight = 48;
    static constexpr unsigned tileCount = 4;
    static constexpr unsigned atlasWidth = tileWidth * tileCount;
    static constexpr unsigned atlasHeight = tileHeight;
    std::vector<TerrainVertex> terrain;
    std::vector<SpriteInstance> sprites;
    std::vector<std::uint32_t> atlas;
    unsigned soldierCount = 0;
};

float terrainHeight(float x, float z);
Scene makeScene(unsigned soldiers = 1000);
