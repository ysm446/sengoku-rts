#include "scene.h"
#include "camera.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        for (const auto count : {1000u, 5000u, 10000u}) {
            const auto scene = makeScene(count);
            unsigned soldiers = 0;
            for (const auto& sprite : scene.sprites) {
                require(sprite.tile < Scene::tileCount, "Atlas tile out of bounds");
                require(sprite.size.x > 0 && sprite.size.y > 0, "Invalid sprite size");
                require(std::abs(sprite.position.y - terrainHeight(sprite.position.x, sprite.position.z) - 0.03f) < 0.0001f,
                        "Sprite is not grounded");
                if (sprite.tile == 0) ++soldiers;
            }
            require(soldiers == count, "Incorrect soldier count");
            require(scene.terrain.size() % 3 == 0 && !scene.terrain.empty(), "Invalid terrain triangles");
            require(scene.atlas.size() == Scene::atlasWidth * Scene::atlasHeight, "Invalid atlas dimensions");
            for (unsigned tile = 0; tile < Scene::tileCount; ++tile) {
                unsigned opaque = 0;
                for (unsigned y = 0; y < Scene::tileHeight; ++y)
                    for (unsigned x = 0; x < Scene::tileWidth; ++x)
                        opaque += (scene.atlas[y * Scene::atlasWidth + tile * Scene::tileWidth + x] >> 24) != 0;
                require(opaque > 0 && opaque < Scene::tileWidth * Scene::tileHeight, "Sprite transparency missing");
            }
        }
        for (const auto invalid : {0u, 1u, 10001u}) {
            bool rejected = false;
            try { makeScene(invalid); } catch (const std::invalid_argument&) { rejected = true; }
            require(rejected, "Invalid soldier count accepted");
        }
        Camera camera;
        camera.zoom(1000); require(camera.span == 24, "Near zoom limit failed");
        camera.zoom(-1000); require(camera.span == 160, "Far zoom limit failed");
        camera.pan(100, 100, 100); require(std::abs(camera.x) <= 55 && std::abs(camera.z) <= 55, "Pan limits failed");
        camera = Camera{};
        const auto center = DirectX::XMVector3TransformCoord(DirectX::XMVectorZero(), camera.matrix(1.6f));
        require(std::abs(DirectX::XMVectorGetX(center)) < 0.0001f && std::abs(DirectX::XMVectorGetY(center)) < 0.0001f,
                "Camera does not center the world origin");
        require(DirectX::XMVectorGetZ(center) > 0 && DirectX::XMVectorGetZ(center) < 1, "World origin is clipped");
        std::cout << "Scene and camera checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
