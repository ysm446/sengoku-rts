#include "scene.h"
#include "sprite_sheet.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
std::uint32_t hash(unsigned value) {
    value ^= value >> 16; value *= 0x7feb352du;
    value ^= value >> 15; value *= 0x846ca68bu;
    return value ^ (value >> 16);
}
std::uint32_t rgba(unsigned r, unsigned g, unsigned b) { return r | (g << 8) | (b << 16) | 0xff000000u; }

// 外部素材なしで描画経路を検証するための仮Pixel Sprite。
void makeAtlas(Scene& scene, const std::filesystem::path& emotionDirectory) {
    scene.atlas.resize(Scene::atlasWidth * Scene::atlasHeight);
    auto rect = [&](unsigned tile, int x0, int y0, int x1, int y1, std::uint32_t color) {
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                scene.atlas[y * Scene::atlasWidth + tile * Scene::tileWidth + x] = color;
    };
    const auto ink = rgba(43, 39, 32), wood = rgba(99, 72, 45);
    const auto cloth = rgba(220, 213, 184), skin = rgba(193, 164, 113);
    const auto armor = rgba(184, 181, 169);
    rect(0, 26, 3, 26, 43, wood); rect(0, 25, 0, 27, 4, cloth);
    rect(0, 10, 38, 13, 45, ink); rect(0, 18, 38, 21, 45, ink);
    rect(0, 8, 45, 14, 46, cloth); rect(0, 18, 45, 24, 46, cloth);
    rect(0, 9, 23, 22, 38, ink); rect(0, 10, 24, 21, 36, armor);
    rect(0, 7, 26, 9, 33, armor); rect(0, 22, 26, 24, 32, armor);
    rect(0, 6, 33, 9, 36, skin); rect(0, 24, 31, 27, 34, skin);
    rect(0, 11, 18, 20, 23, skin); rect(0, 12, 21, 20, 22, wood);
    rect(0, 10, 13, 21, 17, ink); rect(0, 8, 17, 23, 18, ink);
    rect(0, 13, 12, 18, 13, cloth);
    for (int y = 27; y < 37; y += 3) rect(0, 10, y, 21, y, rgba(107, 105, 97));
    rect(0, 15, 24, 16, 36, cloth);

    rect(1, 9, 2, 10, 47, wood); rect(1, 8, 2, 27, 3, ink);
    rect(1, 11, 4, 26, 34, cloth); rect(1, 12, 5, 25, 32, rgba(238, 229, 197));
    rect(1, 16, 10, 21, 15, ink); rect(1, 14, 12, 23, 13, ink);
    rect(1, 17, 22, 20, 27, ink);

    rect(2, 14, 17, 17, 47, wood); rect(2, 12, 44, 19, 47, wood);
    rect(2, 6, 25, 16, 27, wood); rect(2, 16, 18, 27, 20, wood);
    auto foliage = [&](int cx, int cy, int radius) {
        for (int y = -3; y <= 3; ++y) for (int x = -radius; x <= radius; ++x) {
            const int px = cx + x, py = cy + y;
            if (px < 0 || px >= 32 || py < 0 || py >= 48 || std::abs(x) + std::abs(y) > radius + 1) continue;
            const auto shade = hash(static_cast<unsigned>(px + py * 37)) % 3;
            scene.atlas[py * Scene::atlasWidth + 2 * Scene::tileWidth + px] =
                shade == 0 ? rgba(66, 89, 50) : shade == 1 ? rgba(49, 73, 44) : rgba(85, 104, 56);
        }
    };
    foliage(8, 24, 8); foliage(23, 18, 8); foliage(13, 14, 10); foliage(17, 7, 8);

    rect(3, 1, 11, 2, 47, wood); rect(3, 29, 11, 30, 47, wood);
    rect(3, 3, 14, 28, 40, cloth); rect(3, 3, 34, 28, 39, rgba(171, 177, 161));
    rect(3, 14, 14, 16, 40, rgba(190, 186, 163));
    rect(3, 7, 21, 11, 28, ink); rect(3, 20, 21, 24, 28, ink);

    // 既存32x48の仮素材を64x64タイルへ最近傍で展開する。
    const auto original = scene.atlas;
    for (unsigned tile = 0; tile < 4; ++tile)
        for (unsigned y = 0; y < Scene::tileHeight; ++y)
            for (unsigned x = 0; x < Scene::tileWidth; ++x)
                scene.atlas[y * Scene::atlasWidth + tile * Scene::tileWidth + x] =
                    original[(y * 48 / Scene::tileHeight) * Scene::atlasWidth + tile * Scene::tileWidth + x * 32 / Scene::tileWidth];
    // 空きタイル12を地表の点、13を非表示用の透明タイルとして使う。
    for (unsigned y = 12; y < 52; ++y) for (unsigned x = 12; x < 52; ++x)
        scene.atlas[(Scene::tileHeight + y) * Scene::atlasWidth + x] = rgba(255, 255, 255);
    // 空きタイル24〜26。32pxの仮アイコンを透明背景へ2倍で描く。
    for (unsigned icon = 0; icon < 3; ++icon) {
        auto box = [&](int x0, int y0, int x1, int y1, std::uint32_t color) {
            for (int y = y0 * 2; y < (y1 + 1) * 2; ++y)
                for (int x = x0 * 2; x < (x1 + 1) * 2; ++x)
                    scene.atlas[(128 + y) * Scene::atlasWidth + icon * 64 + x] = color;
        };
        box(3, 2, 28, 25, ink); box(6, 24, 11, 29, ink);
        box(4, 3, 27, 24, cloth); box(7, 24, 10, 27, cloth);
        if (!emotionDirectory.empty()) {
            const wchar_t* names[] = {L"motivation.png", L"anxiety.png", L"fear.png"};
            const auto pixels = loadSpriteSheet(emotionDirectory / names[icon], 32, 32);
            bool transparent = false, opaque = false;
            for (const auto pixel : pixels) {
                transparent |= (pixel >> 24) == 0;
                opaque |= (pixel >> 24) == 255;
                if ((pixel >> 24) != 0 && (pixel >> 24) != 255)
                    throw std::runtime_error("Emotion icons require binary alpha");
            }
            if (!transparent || !opaque) throw std::runtime_error("Emotion icons require transparent background and visible pixels");
            // 枠は共通のコード素材、中身は独立した32px PNG。透明余白も含めて2倍で配置。
            for (unsigned y = 0; y < 32; ++y) for (unsigned x = 0; x < 32; ++x) {
                const auto pixel = pixels[y * 32 + x];
                if (pixel >> 24) {
                    if (x < 5 || x > 26 || y < 4 || y > 23)
                        throw std::runtime_error("Emotion icon overlaps bubble border");
                    box(x, y, x, y, pixel);
                }
            }
            continue;
        }
        if (icon == 0) {
            box(10, 12, 22, 21, rgba(219, 84, 27)); box(12, 8, 19, 20, rgba(219, 84, 27));
            box(15, 5, 17, 18, rgba(219, 84, 27)); box(14, 14, 18, 21, rgba(255, 210, 62));
        } else {
            box(9, 7, 22, 21, ink);
            box(10, 8, 21, 20, icon == 1 ? rgba(234, 184, 69) : rgba(131, 189, 211));
            box(12, 11, 13, 13, ink); box(18, 11, 19, 13, ink);
            box(14, 17, 17, icon == 1 ? 17 : 19, ink);
            if (icon == 1) { box(23, 7, 24, 10, rgba(42, 123, 204)); box(22, 10, 25, 13, rgba(42, 123, 204)); }
            else { box(6, 11, 6, 17, ink); box(25, 11, 25, 17, ink); }
        }
    }
}
}

Scene makeScene(unsigned soldiers, const SceneOptions& options) {
    if (soldiers == 0 || soldiers > 10000 || soldiers % 2 != 0)
        throw std::invalid_argument("Soldier count must be even and between 2 and 10000.");
    Scene scene;
    scene.unit = options.unit;
    scene.mixed = options.mixed;
    scene.inspect = options.inspect;
    scene.inspectAttack = options.inspectAttack;
    scene.headingOffset = static_cast<float>(options.directionOffset % 8) * DirectX::XM_PIDIV4;
    scene.soldierCount = options.inspect ? 16 : soldiers;
    makeAtlas(scene, options.emotionDirectory);
    if (!options.soldierSheet.empty()) {
        const auto pixels = loadSpriteSheet(options.soldierSheet, Scene::tileWidth * 8, Scene::tileHeight);
        for (unsigned y = 0; y < Scene::tileHeight; ++y)
            std::copy_n(pixels.data() + y * Scene::tileWidth * 8, Scene::tileWidth * 8,
                        scene.atlas.data() + y * Scene::atlasWidth + Scene::tileWidth * 4);
        scene.generatedSoldiers = true;
        if (!options.walkSheet.empty()) {
            const auto walk = loadSpriteSheet(options.walkSheet, Scene::tileWidth * 8, Scene::tileHeight * Scene::walkFrames);
            for (unsigned y = 0; y < Scene::tileHeight * Scene::walkFrames; ++y)
                std::copy_n(walk.data() + y * Scene::tileWidth * 8, Scene::tileWidth * 8,
                            scene.atlas.data() + (y + Scene::tileHeight) * Scene::atlasWidth + Scene::tileWidth * 4);
            scene.animatedSoldiers = true;
        }
        if (!options.attackSheet.empty()) {
            const auto attack = loadSpriteSheet(options.attackSheet, Scene::tileWidth * 8, Scene::tileHeight * Scene::attackFrames);
            for (unsigned y = 0; y < Scene::tileHeight * Scene::attackFrames; ++y)
                std::copy_n(attack.data() + y * Scene::tileWidth * 8, Scene::tileWidth * 8,
                    scene.atlas.data() + (y + Scene::attackRow * Scene::tileHeight) * Scene::atlasWidth + Scene::tileWidth * 4);
            scene.attackSoldiers = true;
        }
    }
    if (options.mixed && scene.generatedSoldiers) {
        const auto directory = options.soldierSheet.parent_path();
        for (unsigned bank = 1; bank < 3; ++bank) {
            const std::wstring prefix = bank == 1 ? L"samurai" : L"archer";
            const wchar_t* suffixes[] = {L"_idle.png", L"_walk.png", L"_attack.png"};
            const unsigned rows[] = {0, 1, Scene::attackRow};
            const unsigned frames[] = {1, Scene::walkFrames, Scene::attackFrames};
            for (unsigned animation = 0; animation < 3; ++animation) {
                const auto pixels = loadSpriteSheet(directory / (prefix + suffixes[animation]), Scene::tileWidth * 8, Scene::tileHeight * frames[animation]);
                for (unsigned y = 0; y < Scene::tileHeight * frames[animation]; ++y)
                    std::copy_n(pixels.data() + y * Scene::tileWidth * 8, Scene::tileWidth * 8,
                        scene.atlas.data() + (bank * Scene::atlasHeight / 3 + rows[animation] * Scene::tileHeight + y) * Scene::atlasWidth + 4 * Scene::tileWidth);
            }
        }
    }
    constexpr int cells = 100;
    for (int z = 0; z < cells; ++z) for (int x = 0; x < cells; ++x) {
        const float wx = static_cast<float>(x) * 1.5f - 75.0f;
        const float wz = static_cast<float>(z) * 1.5f - 75.0f;
        const float noise = static_cast<float>(hash(x + z * cells) % 100) / 100.0f;
        const float path = std::abs(wz - 7.0f * std::sin(wx * 0.06f));
        const bool grass = path > 5 && (std::sin(wx * 0.12f) + std::cos(wz * 0.15f) > 0.8f || std::abs(wx) > 43);
        DirectX::XMFLOAT3 color = grass ? DirectX::XMFLOAT3{0.30f, 0.35f, 0.22f} : DirectX::XMFLOAT3{0.65f, 0.57f, 0.39f};
        const float shade = 0.94f + noise * 0.12f;
        color.x *= shade; color.y *= shade; color.z *= shade;
        auto vertex = [&](float vx, float vz) { return TerrainVertex{{vx, terrainHeight(vx, vz), vz}, color}; };
        const auto a = vertex(wx, wz), b = vertex(wx + 1.5f, wz);
        const auto c = vertex(wx, wz + 1.5f), d = vertex(wx + 1.5f, wz + 1.5f);
        scene.terrain.insert(scene.terrain.end(), {a, c, b, b, c, d});
    }
    auto add = [&](float x, float z, float w, float h, unsigned tile, DirectX::XMFLOAT3 tint) {
        scene.sprites.push_back({{x, terrainHeight(x, z) + 0.03f, z}, {w, h}, tile, tint});
    };
    const DirectX::XMFLOAT3 white{1, 1, 1};
    if (options.inspect) {
        if (options.formationPreview) {
            scene.soldierCount = FormationDrill::count;
            scene.sprites.resize(FormationDrill::count * 2 + 16);
            FormationDrill drill; drill.reset(options.unit);
            updateDrillSprites(scene, drill, Camera{});
            return scene;
        }
        // Cameraの横方向へ8方向を並べる。左右が方向番号順になる。
        for (unsigned team = 0; team < 2; ++team) for (unsigned direction = 0; direction < 8; ++direction) {
            const float horizontal = (static_cast<float>(direction) - 3.5f) * 2.8f;
            const float depth = team == 0 ? -3.0f : 3.0f;
            const unsigned tile = scene.generatedSoldiers ? 4 + (direction + options.directionOffset) % 8 : 0;
            const DirectX::XMFLOAT3 tint = team == 0 ? DirectX::XMFLOAT3{0.85f, 0.34f, 0.25f} : DirectX::XMFLOAT3{0.34f, 0.48f, 0.66f};
            add((-horizontal + depth) * 0.70710678f, (horizontal + depth) * 0.70710678f,
                scene.generatedSoldiers ? unitVisual(scene.unit).idleSize : 1.7f, scene.generatedSoldiers ? unitVisual(scene.unit).idleSize : 2.7f, tile, tint);
            scene.soldierBindings.push_back({scene.sprites.size() - 1, team, {},
                Camera::initialYaw + static_cast<float>(direction) * DirectX::XM_PIDIV4, 0});
        }
        return scene;
    }
    for (unsigned team = 0; team < 2; ++team) {
        const DirectX::XMFLOAT3 tint = team == 0 ? DirectX::XMFLOAT3{0.85f, 0.34f, 0.25f} : DirectX::XMFLOAT3{0.34f, 0.48f, 0.66f};
        for (unsigned i = 0; i < soldiers / 2; ++i) {
            const unsigned id = i * SoldierVisuals::perTeam / (soldiers / 2);
            const auto relative = SoldierVisuals::offset(id);
            const float x = relative.x;
            const float centerZ = team == 0 ? -22.0f : 22.0f;
            const float offsetZ = relative.y;
            const float z = centerZ + offsetZ;
            // 初期の2陣営は互いの側を向く。方向は画面基準の8方向。
            const unsigned direction = (team == 0 ? 1 : 5) + options.directionOffset;
            add(x, z, scene.generatedSoldiers ? 3.4f : 1.7f, scene.generatedSoldiers ? 3.4f : 2.7f,
                scene.generatedSoldiers ? 4 + direction % 8 : 0, tint);
            scene.soldierBindings.push_back({scene.sprites.size() - 1, team, {x, offsetZ}, 0, i % 8, id});
        }
        for (int i = 0; i < 6; ++i) {
            add(-14.0f + i * 5.6f, team == 0 ? -33.0f : 33.0f, 2.1f, 5.5f, 1, tint);
            add(-14.0f + i * 5.6f, team == 0 ? -37.0f : 37.0f, 5.8f, 3.8f, 3, white);
        }
    }
    for (unsigned i = 0; i < 70; ++i) {
        const float side = i % 2 == 0 ? -1.0f : 1.0f;
        const float x = side * (35.0f + static_cast<float>(hash(i * 7) % 160) / 10.0f);
        const float z = static_cast<float>(hash(i * 13 + 1) % 1000) / 10.0f - 50.0f;
        const float h = 6.0f + static_cast<float>(hash(i + 15) % 30) / 10.0f;
        add(x, z, h * 0.9f, h, 2, white);
    }
    scene.routMarkerStart = scene.sprites.size();
    for (unsigned i = 0; i < Scene::routMarkerCount; ++i) add(0, 0, 0.5f, 0.5f, 13, white);
    scene.arrowStart = scene.sprites.size();
    for (unsigned i = 0; i < Scene::arrowCount; ++i) add(0, 0, 1, .14f, 13, white);
    scene.emotionStart = scene.sprites.size();
    for (unsigned i = 0; i < 50; ++i) add(0, 0, 2.4f, 2.4f, 13, white);
    return scene;
}

void updateDrillSprites(Scene& scene, const FormationDrill& drill, const Camera& camera) {
    const auto& visual = unitVisual(drill.unit);
    for(unsigned i=0;i<FormationDrill::count;++i) {
        const auto& soldier=drill.soldiers[i];
        const unsigned frame=scene.animatedSoldiers && soldier.moving ? 1+static_cast<unsigned>(soldier.animationTime*8)%8 : 0;
        auto& sprite=scene.sprites[i];
        sprite={{soldier.x,terrainHeight(soldier.x,soldier.z)+.03f,soldier.z},{visual.idleSize,visual.idleSize},
            scene.generatedSoldiers?4+camera.spriteDirection(soldier.heading)+frame*12:0,{.85f,.34f,.25f},{},{},scene.generatedSoldiers?visual.idlePivot:0};
        const auto goal=drill.slot(i);
        scene.sprites[FormationDrill::count+i]={{goal[0],terrainHeight(goal[0],goal[1])+.06f,goal[1]},
            {.45f,.45f},12,{.9f,.9f,.65f},{1,0,0},{0,0,1}};
    }
    for(unsigned i=0;i<16;++i) {
        const float angle=DirectX::XM_2PI*i/16;
        const float x=drill.targetX+std::cos(angle),z=drill.targetZ+std::sin(angle);
        scene.sprites[FormationDrill::count*2+i]={{x,terrainHeight(x,z)+.08f,z},{.55f,.55f},12,{.35f,.85f,1},{1,0,0},{0,0,1}};
    }
}

std::optional<DirectX::XMFLOAT3> pickTerrain(const Scene& scene, const Camera& camera,
    float pixelX, float pixelY, unsigned width, unsigned height) {
    using namespace DirectX;
    if (!width || !height || !std::isfinite(pixelX) || !std::isfinite(pixelY) ||
        pixelX < 0 || pixelY < 0 || pixelX >= width || pixelY >= height) return std::nullopt;
    const auto inverse = XMMatrixInverse(nullptr, camera.matrix(static_cast<float>(width) / height));
    const float nx = 2 * pixelX / width - 1, ny = 1 - 2 * pixelY / height;
    const auto origin = XMVector3TransformCoord(XMVectorSet(nx, ny, 0, 1), inverse);
    const auto direction = XMVector3Normalize(XMVectorSubtract(
        XMVector3TransformCoord(XMVectorSet(nx, ny, 1, 1), inverse), origin));
    float nearest = 300;
    std::optional<XMFLOAT3> result;
    // 描画に使う三角形と交差させ、丘でも画面上の位置と一致させる。
    for (std::size_t i = 0; i + 2 < scene.terrain.size(); i += 3) {
        const auto a = XMLoadFloat3(&scene.terrain[i].position);
        const auto e1 = XMVectorSubtract(XMLoadFloat3(&scene.terrain[i + 1].position), a);
        const auto e2 = XMVectorSubtract(XMLoadFloat3(&scene.terrain[i + 2].position), a);
        const auto p = XMVector3Cross(direction, e2);
        const float determinant = XMVectorGetX(XMVector3Dot(e1, p));
        if (std::abs(determinant) < 0.000001f) continue;
        const auto offset = XMVectorSubtract(origin, a);
        const float u = XMVectorGetX(XMVector3Dot(offset, p)) / determinant;
        const auto q = XMVector3Cross(offset, e1);
        const float v = XMVectorGetX(XMVector3Dot(direction, q)) / determinant;
        const float distance = XMVectorGetX(XMVector3Dot(e2, q)) / determinant;
        if (u < 0 || v < 0 || u + v > 1 || distance < 0 || distance >= nearest) continue;
        nearest = distance;
        XMFLOAT3 point;
        XMStoreFloat3(&point, XMVectorAdd(origin, XMVectorScale(direction, distance)));
        result = point;
    }
    return result;
}

void updateSceneSprites(Scene& scene, const BattleSimulation& simulation, const Camera& camera, int selected, int selectedGroup, float aspect, unsigned viewportHeight) {
    if (!scene.inspect) {
        for (unsigned i = 0; i < Scene::routMarkerCount; ++i) scene.sprites[scene.routMarkerStart + i].tile = 13;
        if (selected >= 0 && selected < 2 && selectedGroup >= 0 && selectedGroup < 25 && simulation.result == BattleResult::Ongoing) {
            const auto& f = simulation.formations[selected];
            const auto& group = f.organization.smallGroups[selectedGroup];
            const bool source = group.routed && group.routShock > 0;
            const bool receiver = simulation.nearbyRouts(selected, selectedGroup) > 0;
            unsigned marker = 0;
            const auto circle = [&](BattlePoint center, float radius, unsigned points, DirectX::XMFLOAT3 tint) {
                for (unsigned i = 0; i < points; ++i) {
                    const float angle = DirectX::XM_2PI * i / points;
                    const float x = center.x + radius * std::cos(angle), z = center.z + radius * std::sin(angle);
                    auto& dot = scene.sprites[scene.routMarkerStart + marker++];
                    dot.position = {x, terrainHeight(x, z) + 0.09f, z};
                    dot.size = {0.55f, 0.55f}; dot.tile = 12; dot.tint = tint;
                    dot.rightAxis = {1, 0, 0}; dot.upAxis = {0, 0, 1};
                }
            };
            if (source || receiver) {
                circle(f.groupPosition(selectedGroup), BattleSimulation::routRadius, 96, source ?
                    DirectX::XMFLOAT3{1, 0.85f, 0.15f} : DirectX::XMFLOAT3{0.2f, 0.9f, 1});
                for (unsigned id = 0; id < 25; ++id) if (source ? simulation.routInfluences(selected, selectedGroup, id) :
                    simulation.routInfluences(selected, id, selectedGroup))
                    circle(f.groupPosition(id), 2.4f, 12, {1, 0.45f, 0.1f});
            }
        }
    }
    if (!scene.inspect) scene.individuals->update(simulation);
    for (const auto& binding : scene.soldierBindings) {
        auto& sprite = scene.sprites[binding.spriteIndex];
        const auto renderedUnit = scene.inspect ? scene.unit : simulation.formations[binding.formation].groupUnit(Organization::groupForSoldier(binding.ordinal));
        const unsigned tileOffset = scene.mixed ? static_cast<unsigned>(renderedUnit) * Scene::unitTileCount : 0;
        float heading = binding.heading + scene.headingOffset;
        double animationTime = simulation.time;
        bool walking = simulation.time > 0;
        bool attacking = scene.inspect && scene.inspectAttack;
        if (scene.inspect && scene.generatedSoldiers) {
            const float size = unitVisual(renderedUnit).idleSize;
            sprite.size = {size,size};
        }
        sprite.pivot = scene.generatedSoldiers ? unitVisual(renderedUnit).idlePivot : 0;
        sprite.rightAxis = {}; sprite.upAxis = {};
        if (!scene.inspect) {
            const auto& soldier = scene.individuals->soldiers[binding.formation * SoldierVisuals::perTeam + binding.ordinal];
            heading = soldier.heading; animationTime = soldier.animationTime; walking = soldier.walking;
            attacking = soldier.attacking;
            sprite.size = scene.generatedSoldiers ? DirectX::XMFLOAT2{unitVisual(renderedUnit).idleSize, unitVisual(renderedUnit).idleSize} : DirectX::XMFLOAT2{1.7f, 2.7f};
            sprite.position = soldier.position;
            sprite.tint = binding.formation == 0 ? DirectX::XMFLOAT3{0.85f, 0.34f, 0.25f} : DirectX::XMFLOAT3{0.34f, 0.48f, 0.66f};
            if (static_cast<int>(binding.formation) == selected && soldier.life == SoldierLife::Alive) {
                sprite.tint.x = std::min(1.0f, sprite.tint.x + 0.2f);
                sprite.tint.y += 0.2f; sprite.tint.z += 0.12f;
                if (static_cast<int>(soldier.smallGroup) == selectedGroup) {
                    sprite.tint.x = std::min(1.0f, sprite.tint.x + 0.15f);
                    sprite.tint.y = std::min(1.0f, sprite.tint.y + 0.2f);
                    sprite.tint.z = std::min(1.0f, sprite.tint.z + 0.2f);
                }
            }
            if (soldier.life != SoldierLife::Alive) {
                const float fall = std::clamp(static_cast<float>((simulation.time - soldier.deathTime) / 0.8), 0.0f, 1.0f);
                const float eased = fall * fall * (3 - 2 * fall);
                const auto groundAxis = [&](float x, float z) {
                    const float slope = (terrainHeight(sprite.position.x + x * 0.5f, sprite.position.z + z * 0.5f) -
                        terrainHeight(sprite.position.x - x * 0.5f, sprite.position.z - z * 0.5f));
                    return DirectX::XMFLOAT3{x, slope, z};
                };
                const auto right = groundAxis(-std::sin(heading), std::cos(heading));
                const auto up = groundAxis(std::cos(heading), std::sin(heading));
                const auto cameraRight = camera.right();
                const auto cameraUp = scene.generatedSoldiers ? camera.up() : DirectX::XMFLOAT3{0, 1, 0};
                const auto blend = [&](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
                    if (eased >= 1) return b;
                    return DirectX::XMFLOAT3{a.x + (b.x - a.x) * eased, a.y + (b.y - a.y) * eased, a.z + (b.z - a.z) * eased};
                };
                sprite.rightAxis = blend(cameraRight, right); sprite.upAxis = blend(cameraUp, up);
                sprite.position.y += 0.05f;
                // 倒れた図柄は世界方向に固定し、カメラ回転でSpriteを切り替えない。
                sprite.tile = scene.generatedSoldiers ? tileOffset + 4 : 0;
                continue;
            }
            if (soldier.attacking && !scene.attackSoldiers) {
                // Attack素材ができるまでの簡易な接触表現。
                const float thrust = std::sin(static_cast<float>(std::fmod(animationTime * 12, 6.2831853))) * 0.15f;
                sprite.position.x += std::cos(soldier.heading) * thrust;
                sprite.position.z += std::sin(soldier.heading) * thrust;
            }
            sprite.position.y = terrainHeight(sprite.position.x, sprite.position.z) + 0.03f;
        }
        unsigned frame = 0;
        if (scene.attackSoldiers && attacking) {
            frame = Scene::attackRow + static_cast<unsigned>(std::fmod(animationTime * 8.0, Scene::attackFrames));
            const float size = unitVisual(renderedUnit).attackSize;
            sprite.size = {size, size};
            sprite.pivot = .75f;
        } else if (scene.animatedSoldiers && walking)
            frame = 1 + (static_cast<unsigned>(std::fmod(animationTime * 8.0, 8.0)) + binding.phase) % Scene::walkFrames;
        sprite.tile = scene.generatedSoldiers ? tileOffset + 4 + camera.spriteDirection(heading) + frame * Scene::atlasColumns : 0;
    }
    if (!scene.inspect) {
        scene.emotions.update(simulation);
        std::vector<DirectX::XMFLOAT2> occupied;
        const auto right = camera.right(), up = camera.up();
        const auto projection = camera.matrix(aspect);
        // 32pxの図柄を常に2倍表示する。ズームによる縮小・巨大化を避ける。
        const float worldPerPixel = camera.span / (aspect * std::max(1u, viewportHeight));
        const float bubbleSize = 64 * worldPerPixel;
        const float bubbleSpacing = 68 * worldPerPixel;
        for (unsigned i = 0; i < 50; ++i) scene.sprites[scene.emotionStart + i].tile = 13;
        // 恐怖を優先し、投影後の矩形で重なりを抑える。同時表示は8個まで。
        for (auto emotion : {Emotion::Fear, Emotion::Anxiety, Emotion::Motivation}) for (unsigned i = 0; i < 50; ++i) {
            if (scene.emotions.signals[i].visible != emotion || occupied.size() >= 8) continue;
            const auto p = simulation.formations[i / 25].groupPosition(i % 25);
            const float y = terrainHeight(p.x, p.z) + 4;
            const auto projected = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(p.x, y, p.z, 1), projection);
            const float halfWidth = bubbleSize / camera.span, halfHeight = halfWidth * aspect;
            // 一部でも画面に入る吹き出しは残す。画面外は表示枠を使わない。
            if (std::abs(DirectX::XMVectorGetX(projected)) > 1 + halfWidth ||
                std::abs(DirectX::XMVectorGetY(projected)) > 1 + halfHeight ||
                DirectX::XMVectorGetZ(projected) < 0 || DirectX::XMVectorGetZ(projected) > 1) continue;
            const DirectX::XMFLOAT2 screen{p.x * right.x + p.z * right.z, p.x * up.x + y * up.y + p.z * up.z};
            bool overlap = false;
            for (const auto& other : occupied) overlap |= std::abs(screen.x - other.x) < bubbleSpacing && std::abs(screen.y - other.y) < bubbleSpacing;
            if (overlap) continue;
            occupied.push_back(screen);
            scene.sprites[scene.emotionStart + i] = {{p.x, y, p.z}, {bubbleSize, bubbleSize},
                23 + static_cast<unsigned>(emotion), {1, 1, 1}, right, up, .5f};
        }
        for (unsigned i = 0; i < Scene::arrowCount; ++i) scene.sprites[scene.arrowStart + i].tile = 13;
        unsigned marker = 0;
        for (const auto& arrow : simulation.arrows) {
            const float fraction = arrow.age / arrow.duration;
            const auto p = arrow.position(fraction), next = arrow.position(std::min(1.0f, fraction + .01f));
            const float dx = next.x - p.x, dy = arrow.height(std::min(1.0f, fraction + .01f)) - arrow.height(fraction), dz = next.z - p.z;
            const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (length <= 0) continue;
            // 一斉射の代表として三本を表示する。表示本数から損害は計算しない。
            for (unsigned shaft = 0; shaft < 3 && marker < Scene::arrowCount; ++shaft) {
                auto& sprite = scene.sprites[scene.arrowStart + marker++];
                const float spread = (static_cast<float>(shaft) - 1) * .45f;
                sprite.position = {p.x - dz / length * spread, arrow.height(fraction), p.z + dx / length * spread};
                sprite.tile = 12; sprite.size = {1.4f, .16f}; sprite.pivot = .5f;
                sprite.rightAxis = {dx / length, dy / length, dz / length}; sprite.upAxis = camera.up();
                sprite.tint = arrow.team == 0 ? DirectX::XMFLOAT3{1, .75f, .35f} : DirectX::XMFLOAT3{.65f, .85f, 1};
            }
        }
    }
}
