#include "scene.h"
#include "battle_clock.h"
#include "sprite_sheet.h"
#include <stdexcept>
#include <iostream>
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Emotion asset directory is required");
        SceneOptions assetOptions;
        assetOptions.emotionDirectory = argv[1];
        const auto assetScene = makeScene(1000, assetOptions);
        const auto fallbackScene = makeScene();
        const char* names[] = {"motivation.png", "anxiety.png", "fear.png"};
        for (unsigned icon = 0; icon < 3; ++icon) {
            const auto pixels = loadSpriteSheet(assetOptions.emotionDirectory / names[icon], 32, 32);
            unsigned visiblePixels = 0;
            bool differs = false;
            for (unsigned y = 0; y < 32; ++y) for (unsigned x = 0; x < 32; ++x) {
                const auto pixel = pixels[y * 32 + x];
                visiblePixels += (pixel >> 24) != 0;
                for (unsigned dy = 0; dy < 2; ++dy) for (unsigned dx = 0; dx < 2; ++dx) {
                    const auto index = (128 + y * 2 + dy) * Scene::atlasWidth + icon * 64 + x * 2 + dx;
                    if (pixel >> 24) require(assetScene.atlas[index] == pixel, "Icon pixels were not copied intact");
                    differs |= assetScene.atlas[index] != fallbackScene.atlas[index];
                }
            }
            require(visiblePixels > 100 && visiblePixels < 440 && differs, "Icon is empty, opaque, or still placeholder");
            const auto border = 132 * Scene::atlasWidth + icon * 64 + 6;
            require(assetScene.atlas[border] == fallbackScene.atlas[border], "Asset overwrote bubble frame");
        }
        auto invalidOptions = assetOptions;
        invalidOptions.emotionDirectory /= "missing";
        bool rejected = false;
        try { makeScene(1000, invalidOptions); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "Missing emotion assets were silently ignored");
        BattleSimulation battle;
        auto scene = makeScene();
        Camera camera;
        updateSceneSprites(scene, battle, camera);
        auto& g = battle.formations[0].organization.smallGroups[0];
        auto visible = [&] { return scene.emotions.signals[0].visible; };
        require(visible() == Emotion::None, "Initial state emitted an event");
        g.morale = 49; battle.time = 1;
        updateSceneSprites(scene, battle, camera);
        require(visible() == Emotion::Anxiety && scene.sprites[scene.emotionStart].tile == 25, "Anxiety missing");
        updateSceneSprites(scene, battle, camera);
        require(visible() == Emotion::Anxiety, "Paused event expired");
        g.morale = 51; battle.time = 4;
        updateSceneSprites(scene, battle, camera);
        require(visible() == Emotion::None, "Threshold jitter repeated event");
        g.routed = true;
        updateSceneSprites(scene, battle, camera);
        require(visible() == Emotion::Fear, "Fear failed to override cooldown");
        battle.time = 7; updateSceneSprites(scene, battle, camera);
        require(visible() == Emotion::None, "Persistent rout repeated fear");
        battle.reset(); updateSceneSprites(scene, battle, camera);
        require(visible() == Emotion::None, "Reset retained event");
        auto& recovering = battle.formations[0].organization.smallGroups[0];
        recovering.resting = true; recovering.morale = 65;
        updateSceneSprites(scene, battle, camera);
        recovering.resting = false; recovering.morale = 70; battle.time = 1;
        updateSceneSprites(scene, battle, camera);
        require(visible() == Emotion::Motivation, "Recovery event missing");
        for (auto& f : battle.formations) for (auto& group : f.organization.smallGroups) group.routed = true;
        battle.time = 2; updateSceneSprites(scene, battle, camera);
        unsigned count = 0;
        for (unsigned i = 0; i < 50; ++i) count += scene.sprites[scene.emotionStart + i].tile != 13;
        require(count <= 8 && count > 0, "Display limit failed");
        require(scene.atlas[128 * Scene::atlasWidth] == 0, "Icon background is not transparent");
        // 先に列挙される画面外の恐怖が、画面内の通知を押し出さない。
        for (auto& f : battle.formations) for (auto& group : f.organization.smallGroups) {
            group.fleeX = 500; group.fleeZ = 500;
        }
        auto& onScreen = battle.formations[1].organization.smallGroups[24];
        onScreen.fleeX = 0; onScreen.fleeZ = 0;
        camera.yaw = 0; camera.span = 24;
        updateSceneSprites(scene, battle, camera);
        require(scene.sprites[scene.emotionStart + 49].tile == 26, "Offscreen candidates consumed display slots");
        require(scene.sprites[scene.emotionStart].tile == 13, "Offscreen icon remained visible");
        // 縦長では見える上下端の候補が、横長では画面外になる。
        onScreen.fleeX = -20;
        updateSceneSprites(scene, battle, camera, -1, -1, .5f);
        require(scene.sprites[scene.emotionStart + 49].tile == 26, "Portrait viewport culled visible icon");
        updateSceneSprites(scene, battle, camera, -1, -1, 2);
        require(scene.sprites[scene.emotionStart + 49].tile == 13, "Landscape viewport retained offscreen icon");
        camera.x = -20;
        updateSceneSprites(scene, battle, camera, -1, -1, 2);
        require(scene.sprites[scene.emotionStart + 49].tile == 26, "Panning did not restore active notification");
        // タイルの投影サイズはズーム・縦横比・解像度によらず64px。
        for (const float span : {24.0f, 96.0f, 160.0f}) for (const unsigned height : {640u, 1080u, 2160u})
            for (const float aspect : {.5f, 16.0f / 9, 2.0f}) {
                camera.span = span;
                updateSceneSprites(scene, battle, camera, -1, -1, aspect, height);
                const auto& bubble = scene.sprites[scene.emotionStart + 49];
                const auto center = DirectX::XMLoadFloat3(&bubble.position);
                const auto right = camera.right();
                const auto offset = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&right), bubble.size.x * .5f);
                const auto matrix = camera.matrix(aspect);
                const auto left = DirectX::XMVector3TransformCoord(DirectX::XMVectorSubtract(center, offset), matrix);
                const auto edge = DirectX::XMVector3TransformCoord(DirectX::XMVectorAdd(center, offset), matrix);
                const float pixels = std::abs(DirectX::XMVectorGetX(edge) - DirectX::XMVectorGetX(left)) * height * aspect * .5f;
                require(bubble.tile == 26 && std::abs(pixels - 64) < .01f, "Bubble changed pixel size across viewports");
            }
        auto& neighbor = battle.formations[1].organization.smallGroups[23];
        neighbor.fleeX = onScreen.fleeX;
        camera.span = 96;
        const float unitsPerPixel = camera.span / 1920;
        neighbor.fleeZ = 66 * unitsPerPixel;
        updateSceneSprites(scene, battle, camera);
        require(scene.sprites[scene.emotionStart + 48].tile == 26 && scene.sprites[scene.emotionStart + 49].tile == 13,
            "Nearby bubbles lost pixel spacing");
        neighbor.fleeZ = 70 * unitsPerPixel;
        updateSceneSprites(scene, battle, camera);
        require(scene.sprites[scene.emotionStart + 49].tile == 26, "Separated bubbles were suppressed");
        auto recoveryBattle = [] {
            BattleSimulation b;
            b.hold(0); b.hold(1); b.running = true;
            auto& group = b.formations[0].organization.smallGroups[0];
            group.resting = true; group.morale = 70; group.fatigue = 0;
            return b;
        };
        auto slow = recoveryBattle(), fast = recoveryBattle();
        SoldierVisuals slowVisuals, fastVisuals;
        EmotionSignals slowSignals, fastSignals;
        BattleClock slowClock, fastClock;
        slowClock.advance(slow, slowVisuals, 1, &slowSignals);
        for (unsigned frame = 0; frame < 144; ++frame)
            fastClock.advance(fast, fastVisuals, 1.0 / 144, &fastSignals);
        require(slowSignals.signals[0].visible == Emotion::Motivation, "Recovery between draws was lost");
        require(slowSignals.signals[0].until < 2.6, "Notification time followed drawing instead of fixed steps");
        for (unsigned i = 0; i < 50; ++i) {
            const auto& a = slowSignals.signals[i]; const auto& b = fastSignals.signals[i];
            require(a.visible == b.visible && a.state == b.state && a.until == b.until && a.cooldown == b.cooldown,
                "Emotion timing depends on rendering frequency");
        }
        slow.running = false;
        slowClock.advance(slow, slowVisuals, 10, &slowSignals);
        require(slowSignals.signals[0].visible == Emotion::Motivation, "Paused fixed clock expired notification");
        slow.reset(); slowClock.advance(slow, slowVisuals, 0, &slowSignals);
        require(slowSignals.signals[0].visible == Emotion::None, "Clock reset retained notification");
        std::cout << "Emotion transitions and scene integration passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
