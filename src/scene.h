#pragma once
#include "camera.h"
#include "simulation.h"
#include <DirectXMath.h>
#include <cstdint>
#include <filesystem>
#include <vector>
#include <optional>

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
    static constexpr unsigned tileWidth = 64;
    static constexpr unsigned tileHeight = 64;
    static constexpr unsigned atlasColumns = 12;
    static constexpr unsigned walkFrames = 8;
    static constexpr unsigned tileCount = atlasColumns * (1 + walkFrames);
    static constexpr unsigned atlasWidth = tileWidth * atlasColumns;
    static constexpr unsigned atlasHeight = tileHeight * (1 + walkFrames);
    std::vector<TerrainVertex> terrain;
    std::vector<SpriteInstance> sprites;
    std::vector<std::uint32_t> atlas;
    unsigned soldierCount = 0;
    bool generatedSoldiers = false;
    bool animatedSoldiers = false;
    bool inspect = false;
    float headingOffset = 0;
    struct SoldierBinding {
        std::size_t spriteIndex;
        unsigned formation;
        DirectX::XMFLOAT2 offset;
        float heading;
        unsigned phase;
    };
    std::vector<SoldierBinding> soldierBindings;
};

struct SceneOptions {
    std::filesystem::path soldierSheet;
    std::filesystem::path walkSheet;
    unsigned directionOffset = 0;
    bool inspect = false;
};

float terrainHeight(float x, float z);
Scene makeScene(unsigned soldiers = 1000, const SceneOptions& options = {});
void updateSceneSprites(Scene& scene, const BattleSimulation& simulation, const Camera& camera, int selected = -1);
std::optional<DirectX::XMFLOAT3> pickTerrain(const Scene& scene, const Camera& camera,
    float pixelX, float pixelY, unsigned width, unsigned height);
