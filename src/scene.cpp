#include "scene.h"
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
void makeAtlas(Scene& scene) {
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
            scene.atlas[py * Scene::atlasWidth + 64 + px] =
                shade == 0 ? rgba(66, 89, 50) : shade == 1 ? rgba(49, 73, 44) : rgba(85, 104, 56);
        }
    };
    foliage(8, 24, 8); foliage(23, 18, 8); foliage(13, 14, 10); foliage(17, 7, 8);

    rect(3, 1, 11, 2, 47, wood); rect(3, 29, 11, 30, 47, wood);
    rect(3, 3, 14, 28, 40, cloth); rect(3, 3, 34, 28, 39, rgba(171, 177, 161));
    rect(3, 14, 14, 16, 40, rgba(190, 186, 163));
    rect(3, 7, 21, 11, 28, ink); rect(3, 20, 21, 24, 28, ink);
}
}

float terrainHeight(float x, float z) {
    return 1.2f * std::sin(x * 0.055f) * std::cos(z * 0.07f) +
        3.6f * std::exp(-((x + 37) * (x + 37) + (z - 25) * (z - 25)) / 330.0f);
}

Scene makeScene(unsigned soldiers) {
    if (soldiers == 0 || soldiers > 10000 || soldiers % 2 != 0)
        throw std::invalid_argument("Soldier count must be even and between 2 and 10000.");
    Scene scene;
    scene.soldierCount = soldiers;
    makeAtlas(scene);
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
    for (unsigned team = 0; team < 2; ++team) {
        const DirectX::XMFLOAT3 tint = team == 0 ? DirectX::XMFLOAT3{0.85f, 0.34f, 0.25f} : DirectX::XMFLOAT3{0.34f, 0.48f, 0.66f};
        const unsigned columns = static_cast<unsigned>(std::ceil(std::sqrt(static_cast<float>(soldiers / 2))));
        const float spacing = std::min(1.2f, 26.0f / static_cast<float>(columns));
        for (unsigned i = 0; i < soldiers / 2; ++i) {
            const float x = (static_cast<float>(i % columns) - static_cast<float>(columns - 1) * 0.5f) * spacing;
            const float z = (team == 0 ? -18.0f : 18.0f) + (static_cast<float>(i / columns) - static_cast<float>(columns - 1) * 0.5f) * spacing;
            add(x, z, 1.7f, 2.7f, 0, tint);
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
    return scene;
}
