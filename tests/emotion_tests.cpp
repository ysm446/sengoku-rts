#include "scene.h"
#include <stdexcept>
#include <iostream>
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
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
        std::cout << "Emotion transitions and scene integration passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
