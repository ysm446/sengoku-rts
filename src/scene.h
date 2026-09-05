#pragma once
#include "camera.h"
#include "simulation.h"
#include <DirectXMath.h>
#include <cstdint>
#include <filesystem>
#include <vector>
#include <optional>
#include <memory>

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
    DirectX::XMFLOAT3 rightAxis{};
    DirectX::XMFLOAT3 upAxis{};
};

enum class SoldierLife { Alive, Falling, Fallen };
struct SoldierVisual {
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT2 slot{};
    float heading = 0;
    double animationTime = 0;
    double deathTime = 0;
    bool walking = false;
    bool attacking = false;
    int attackTarget = -1;
    unsigned smallGroup = 0;
    SoldierLife life = SoldierLife::Alive;
};

// 表示密度を変えても同じ兵士の位置・死亡状態を保持する固定IDの表示用集団。
struct SoldierVisuals {
    static constexpr unsigned perTeam = 5000;
    std::vector<SoldierVisual> soldiers;
    std::uint64_t generation = 0;
    double time = 0;
    std::array<std::uint64_t, 2> impacts{};
    std::array<DirectX::XMFLOAT3, 2> impactPositions{};
    void update(const BattleSimulation& simulation);
    static DirectX::XMFLOAT2 offset(unsigned id);
};

struct Scene {
    static constexpr unsigned tileWidth = 64;
    static constexpr unsigned tileHeight = 64;
    static constexpr unsigned atlasColumns = 12;
    static constexpr unsigned walkFrames = 8;
    static constexpr unsigned attackFrames = 8;
    static constexpr unsigned attackRow = 1 + walkFrames;
    static constexpr unsigned tileCount = atlasColumns * (1 + walkFrames + attackFrames);
    static constexpr unsigned atlasWidth = tileWidth * atlasColumns;
    static constexpr unsigned atlasHeight = tileHeight * (1 + walkFrames + attackFrames);
    std::vector<TerrainVertex> terrain;
    std::vector<SpriteInstance> sprites;
    std::size_t routMarkerStart = 0;
    static constexpr unsigned routMarkerCount = 96 + 25 * 12;
    std::vector<std::uint32_t> atlas;
    unsigned soldierCount = 0;
    bool generatedSoldiers = false;
    bool animatedSoldiers = false;
    bool attackSoldiers = false;
    bool inspectAttack = false;
    bool inspect = false;
    float headingOffset = 0;
    std::shared_ptr<SoldierVisuals> individuals = std::make_shared<SoldierVisuals>();
    struct SoldierBinding {
        std::size_t spriteIndex;
        unsigned formation;
        DirectX::XMFLOAT2 offset;
        float heading;
        unsigned phase;
        unsigned ordinal = 0;
    };
    std::vector<SoldierBinding> soldierBindings;
};

struct SceneOptions {
    std::filesystem::path soldierSheet;
    std::filesystem::path walkSheet;
    std::filesystem::path attackSheet;
    unsigned directionOffset = 0;
    bool inspect = false;
    bool inspectAttack = false;
};

float terrainHeight(float x, float z);
Scene makeScene(unsigned soldiers = 1000, const SceneOptions& options = {});
void updateSceneSprites(Scene& scene, const BattleSimulation& simulation, const Camera& camera, int selected = -1, int selectedGroup = -1);
std::optional<DirectX::XMFLOAT3> pickTerrain(const Scene& scene, const Camera& camera,
    float pixelX, float pixelY, unsigned width, unsigned height);
