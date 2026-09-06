#include "scene.h"
#include "camera.h"
#include "sprite_sheet.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <set>

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int wmain(int argc, wchar_t** argv) {
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
            for (unsigned tile = 0; tile < 4; ++tile) {
                unsigned opaque = 0;
                for (unsigned y = 0; y < Scene::tileHeight; ++y)
                    for (unsigned x = 0; x < Scene::tileWidth; ++x)
                        opaque += (scene.atlas[y * Scene::atlasWidth + tile * Scene::tileWidth + x] >> 24) != 0;
                require(opaque > 0 && opaque < Scene::tileWidth * Scene::tileHeight, "Sprite transparency missing");
            }
        }
        require(argc == 2, "Expected generated sprite sheet path");
        SceneOptions options;
        options.soldierSheet = argv[1]; options.inspect = true;
        const auto inspected = makeScene(1000, options);
        require(inspected.generatedSoldiers && inspected.soldierCount == 16 && inspected.sprites.size() == 16,
                "Inspection must show eight directions for both teams");
        unsigned directions[8]{};
        for (const auto& sprite : inspected.sprites) {
            require(sprite.tile >= 4 && sprite.tile < 12, "Invalid generated direction");
            ++directions[sprite.tile - 4];
        }
        for (const auto count : directions) require(count == 2, "A direction is missing from inspection");
        for (unsigned tile = 4; tile < 12; ++tile) {
            unsigned opaque = 0;
            for (unsigned y = 0; y < Scene::tileHeight; ++y) for (unsigned x = 0; x < Scene::tileWidth; ++x) {
                const unsigned alpha = inspected.atlas[y * Scene::atlasWidth + tile * Scene::tileWidth + x] >> 24;
                require(alpha == 0 || alpha == 255, "Generated alpha must be binary");
                opaque += alpha == 255;
                if (x == 0 || y == 0 || x == 63 || y == 63) require(alpha == 0, "Generated sprite is clipped");
            }
            require(opaque > 100 && opaque < 4096, "Generated sprite is empty or not transparent");
        }
        options.directionOffset = 7;
        const auto rotated = makeScene(1000, options);
        require(rotated.sprites[0].tile == 11 && rotated.sprites[1].tile == 4, "Direction wrap failed");
        bool wrongSizeRejected = false;
        try { loadSpriteSheet(options.soldierSheet, 64, 64); } catch (const std::runtime_error&) { wrongSizeRejected = true; }
        require(wrongSizeRejected, "Incorrect sprite dimensions accepted");
        options.soldierSheet += L".missing";
        bool missingRejected = false;
        try { makeScene(1000, options); } catch (const std::runtime_error&) { missingRejected = true; }
        require(missingRejected, "Missing explicitly requested sprite accepted");
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

        for (int angle = 0; angle < 360; angle += 15) {
            camera = Camera{};
            camera.rotate(DirectX::XMConvertToRadians(static_cast<float>(angle)));
            const auto right = camera.right(), up = camera.up();
            const auto matrix = camera.matrix(16.0f / 9.0f);
            const auto origin = DirectX::XMVector3TransformCoord(DirectX::XMVectorZero(), matrix);
            const auto projectedRight = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&right), matrix);
            const auto projectedUp = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&up), matrix);
            require(DirectX::XMVectorGetX(projectedRight) > DirectX::XMVectorGetX(origin), "Billboard right points away from screen right");
            require(std::abs(DirectX::XMVectorGetY(projectedRight) - DirectX::XMVectorGetY(origin)) < 0.0001f,
                    "Billboard right is tilted on screen");
            require(DirectX::XMVectorGetY(projectedUp) > DirectX::XMVectorGetY(origin), "Billboard up points down");
            require(std::abs(DirectX::XMVectorGetZ(projectedUp) - DirectX::XMVectorGetZ(origin)) < 0.0001f,
                    "Billboard up is not parallel to screen");
            camera.pan(1, 0, 0.1f);
            const auto shifted = DirectX::XMVector3TransformCoord(DirectX::XMVectorZero(), camera.matrix(16.0f / 9.0f));
            require(DirectX::XMVectorGetX(shifted) < 0, "Pan right does not move scenery left after orbit");
        }
        camera = Camera{};
        for (unsigned direction = 0; direction < 8; ++direction) {
            const float heading = Camera::initialYaw + direction * DirectX::XM_PIDIV4;
            require(camera.spriteDirection(heading) == direction, "World heading to image direction mismatch");
        }
        camera.rotate(DirectX::XM_PIDIV2);
        require(camera.spriteDirection(Camera::initialYaw) == 6, "Orbit changed heading instead of viewpoint");
        camera.rotate(DirectX::XM_2PI * 1000);
        require(std::isfinite(camera.yaw) && std::abs(camera.yaw) <= DirectX::XM_PI, "Yaw wrap failed");

        BattleSimulation simulation;
        simulation.formations[1].x = 50;
        simulation.move(0, 0, -15); simulation.move(1, 50, 15);
        simulation.update(1);
        require(simulation.time == 0 && simulation.formations[0].z == -22, "Paused simulation moved");
        simulation.toggle(); simulation.update(1);
        require(std::abs(simulation.formations[0].z + 20.2f) < 0.0001f && simulation.formations[0].moving,
                "Formation movement speed is wrong");
        simulation.toggle();
        const auto pausedTime = simulation.time;
        simulation.update(1);
        require(simulation.time == pausedTime, "Paused animation clock advanced");
        simulation.toggle(); simulation.update(100);
        require(simulation.formations[0].z == -15 && simulation.formations[1].z == 15 &&
                !simulation.formations[0].moving && !simulation.formations[1].moving, "Formation overshot or failed to stop");
        simulation.reset();
        require(!simulation.running && simulation.time == 0 && simulation.formations[0].z == -22, "Formation reset failed");
        BattleSimulation smallSteps, bigStep;
        smallSteps.toggle(); bigStep.toggle();
        for (int i = 0; i < 60; ++i) smallSteps.update(1.0f / 60);
        bigStep.update(1);
        require(std::abs(smallSteps.formations[0].z - bigStep.formations[0].z) < 0.0001f, "Movement depends on display frame rate");
        for (const float invalidDt : {-1.0f, std::numeric_limits<float>::infinity()}) {
            bool rejected = false;
            try { simulation.update(invalidDt); } catch (const std::invalid_argument&) { rejected = true; }
            require(rejected, "Invalid simulation dt accepted");
        }

        SceneOptions walkingOptions;
        BattleSimulation commanded;
        commanded.hold(1);
        commanded.move(0, 12, -8);
        commanded.update(1);
        require(commanded.formations[0].x == 0, "Move command bypassed pause");
        commanded.toggle(); commanded.update(100);
        require(commanded.formations[0].x == 12 && commanded.formations[0].z == -8,
                "Move command did not reach destination");
        commanded.move(0, -20, 30); commanded.update(1); commanded.hold(0);
        const auto held = commanded.formations[0]; commanded.update(100);
        require(commanded.formations[0].x == held.x && commanded.formations[0].z == held.z && !held.moving,
                "Hold did not cancel destination");
        commanded.move(0, 100, -100);
        require(commanded.formations[0].targetX == 60 && commanded.formations[0].targetZ == -60,
                "Move failed to keep formation inside terrain");
        const auto pickingScene = makeScene();
        for (float yaw : {0.0f, 0.7f, 2.0f, -2.0f}) for (float span : {24.0f, 96.0f, 160.0f}) {
            Camera pickingCamera; pickingCamera.x = -37; pickingCamera.z = 25;
            pickingCamera.yaw = yaw; pickingCamera.span = span;
            // 三角形の内部にある点を投影し、同じ描画面に戻ることを確認する。
            const auto a = pickingScene.terrain[13080].position;
            const auto b = pickingScene.terrain[13081].position;
            const auto c = pickingScene.terrain[13082].position;
            const auto world = DirectX::XMVectorSet((a.x + b.x + c.x) / 3, (a.y + b.y + c.y) / 3, (a.z + b.z + c.z) / 3, 1);
            pickingCamera.x = DirectX::XMVectorGetX(world); pickingCamera.z = DirectX::XMVectorGetZ(world);
            const auto projected = DirectX::XMVector3TransformCoord(world, pickingCamera.matrix(16.0f / 9.0f));
            const auto hit = pickTerrain(pickingScene, pickingCamera,
                (DirectX::XMVectorGetX(projected) + 1) * 960, (1 - DirectX::XMVectorGetY(projected)) * 540, 1920, 1080);
            require(hit && std::abs(hit->x - DirectX::XMVectorGetX(world)) < 0.002f &&
                std::abs(hit->z - DirectX::XMVectorGetZ(world)) < 0.002f, "Terrain picking failed after orbit/zoom/pan");
        }
        require(!pickTerrain(pickingScene, Camera{}, -1, 20, 1920, 1080) &&
                !pickTerrain(pickingScene, Camera{}, 20, 20, 0, 0), "Invalid viewport pick accepted");

        walkingOptions.soldierSheet = argv[1];
        walkingOptions.walkSheet = walkingOptions.soldierSheet.parent_path() / L"ashigaru_walk.png";
        walkingOptions.attackSheet = walkingOptions.soldierSheet.parent_path() / L"ashigaru_attack.png";
        auto movingScene = makeScene(1000, walkingOptions);
        require(movingScene.animatedSoldiers, "Walk sprite sheet was not loaded");
        require(movingScene.attackSoldiers, "Attack sprite sheet was not loaded");
        std::set<std::uint64_t> attackPoses;
        for (unsigned frame = 0; frame < Scene::attackFrames; ++frame) {
            std::uint64_t hash = 1469598103934665603ull;
            unsigned opaque = 0;
            for (unsigned direction = 0; direction < 8; ++direction) {
                for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 64; ++x) {
                    const auto pixel = movingScene.atlas[((Scene::attackRow + frame) * 64 + y) * Scene::atlasWidth + (4 + direction) * 64 + x];
                    const auto alpha = pixel >> 24;
                    require(alpha == 0 || alpha == 255, "Attack alpha is not binary");
                    if (x == 0 || y == 0 || x == 63 || y == 63) require(alpha == 0, "Attack frame clipped");
                    opaque += alpha == 255;
                    hash = (hash ^ pixel) * 1099511628211ull;
                }
            }
            require(opaque > 800, "Attack frame missing");
            attackPoses.insert(hash);
        }
        require(attackPoses.size() >= 4, "Attack frames do not change pose");
        std::set<std::uint64_t> poses;
        for (unsigned frame = 1; frame <= Scene::walkFrames; ++frame) {
            std::uint64_t hash = 1469598103934665603ull;
            for (unsigned direction = 0; direction < 8; ++direction) {
                unsigned opaque = 0;
                for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 64; ++x) {
                    const auto pixel = movingScene.atlas[(frame * 64 + y) * Scene::atlasWidth + (4 + direction) * 64 + x];
                    const auto alpha = pixel >> 24;
                    require(alpha == 0 || alpha == 255, "Walk alpha is not binary");
                    if (x == 0 || y == 0 || x == 63 || y == 63) require(alpha == 0, "Walk sprite is clipped");
                    opaque += alpha == 255;
                    hash = (hash ^ pixel) * 1099511628211ull;
                }
                require(opaque > 100, "Walk frame is empty");
            }
            poses.insert(hash);
        }
        require(poses.size() == 8, "Walk does not contain eight distinct poses");
        camera = Camera{};
        // 到着表示だけを検証するため、局所接敵が始まらない外側へ移動する。
        simulation.move(0, 0, -30); simulation.move(1, 0, 30);
        simulation.toggle(); simulation.update(0.125f);
        updateSceneSprites(movingScene, simulation, camera);
        const auto beforeOrbit = movingScene.sprites[0];
        require(beforeOrbit.tile >= 16, "Moving soldier is not using a walk frame");
        camera.rotate(DirectX::XM_PIDIV2);
        updateSceneSprites(movingScene, simulation, camera);
        require(movingScene.sprites[0].position.z == beforeOrbit.position.z && movingScene.sprites[0].tile != beforeOrbit.tile,
                "Orbit failed to change the view independently of simulation");
        simulation.update(100);
        updateSceneSprites(movingScene, simulation, camera);
        for (const auto& binding : movingScene.soldierBindings) {
            const auto& sprite = movingScene.sprites[binding.spriteIndex];
            require(sprite.tile >= 4 && sprite.tile < 12, "Arrived soldier did not return to idle");
            require(std::abs(sprite.position.y - terrainHeight(sprite.position.x, sprite.position.z) - 0.03f) < 0.0001f,
                    "Moving soldier lost ground contact");
            require(binding.formation == 0 ? sprite.position.z < -1 : sprite.position.z > 1, "Demo formations overlap at destination");
        }
        for (unsigned count : {1000u, 5000u, 10000u}) {
            auto casualtyScene = makeScene(count);
            BattleSimulation losses;
            losses.formations[0].strength = 450;
            for (unsigned g = 20; g < 25; ++g) losses.formations[0].organization.smallGroups[g].strength = 10;
            losses.formations[0].cohesion = 30;
            losses.formations[0].z = -14; losses.formations[1].z = 14;
            losses.formations[0].state = losses.formations[1].state = FormationState::Engaged;
            updateSceneSprites(casualtyScene, losses, Camera{});
            losses.time = 1;
            updateSceneSprites(casualtyScene, losses, Camera{});
            unsigned visible = 0;
            for (const auto& binding : casualtyScene.soldierBindings) {
                const auto& sprite = casualtyScene.sprites[binding.spriteIndex];
                const auto& individual = casualtyScene.individuals->soldiers[binding.formation * SoldierVisuals::perTeam + binding.ordinal];
                if (individual.life == SoldierLife::Alive) ++visible;
                else require(individual.position.z > -3, "Rear soldier was selected as a casualty");
                require(sprite.size.x > 0, "Casualty disappeared instead of falling");
                require(std::isfinite(sprite.position.x) && std::abs(sprite.position.y -
                    terrainHeight(sprite.position.x, sprite.position.z) - (individual.life == SoldierLife::Alive ? 0.03f : 0.08f)) < 0.0001f,
                    "Disordered soldier lost ground contact");
            }
            require(visible < count && visible > count * 3 / 4, "Casualties must be limited to the contacting front");
            losses.reset(); updateSceneSprites(casualtyScene, losses, Camera{});
            for (const auto& binding : casualtyScene.soldierBindings)
                require(casualtyScene.sprites[binding.spriteIndex].size.x > 0, "Reset did not restore casualty sprites");
        }
        auto individualsScene = makeScene(1000, walkingOptions);
        BattleSimulation individualsBattle;
        updateSceneSprites(individualsScene, individualsBattle, Camera{});
        const auto initial0 = individualsScene.individuals->soldiers[0].position;
        const auto initial10 = individualsScene.individuals->soldiers[10].position;
        individualsBattle.toggle();
        for (unsigned step = 0; step < 60; ++step) {
            individualsBattle.update(1.0f / 60);
            updateSceneSprites(individualsScene, individualsBattle, Camera{});
        }
        const auto& individual0 = individualsScene.individuals->soldiers[0];
        const auto& individual10 = individualsScene.individuals->soldiers[10];
        require(std::abs((individual0.position.z - initial0.z) - (individual10.position.z - initial10.z)) > 0.01f,
            "Individuals move as one rigid formation");
        require(individual0.walking && individual10.walking && individual0.animationTime != individual10.animationTime,
            "Individual walk clocks are not independent");
        individualsBattle.formations[0].strength = 450;
        for (unsigned g = 20; g < 25; ++g) individualsBattle.formations[0].organization.smallGroups[g].strength = 10;
        individualsBattle.update(6);
        updateSceneSprites(individualsScene, individualsBattle, Camera{});
        std::size_t casualty = 0;
        while (individualsScene.individuals->soldiers[casualty].life == SoldierLife::Alive) ++casualty;
        const auto death = individualsScene.individuals->soldiers[casualty];
        require(death.life == SoldierLife::Falling, "Death skipped falling state");
        individualsBattle.toggle();
        individualsBattle.update(10);
        updateSceneSprites(individualsScene, individualsBattle, camera);
        require(individualsScene.individuals->soldiers[casualty].life == SoldierLife::Falling,
            "Fall animation advanced while paused");
        individualsBattle.toggle(); individualsBattle.update(1);
        updateSceneSprites(individualsScene, individualsBattle, Camera{});
        const auto& fallen = individualsScene.individuals->soldiers[casualty];
        require(fallen.life == SoldierLife::Fallen && fallen.position.x == death.position.x && fallen.position.z == death.position.z &&
            fallen.heading == death.heading, "Corpse moved with its formation");
        auto rebuilt = makeScene(10000, walkingOptions);
        rebuilt.individuals = individualsScene.individuals;
        updateSceneSprites(rebuilt, individualsBattle, Camera{});
        const auto corpseSprite = rebuilt.sprites[rebuilt.soldierBindings[casualty].spriteIndex];
        camera.rotate(1.7f); updateSceneSprites(rebuilt, individualsBattle, camera);
        const auto rotatedCorpse = rebuilt.sprites[rebuilt.soldierBindings[casualty].spriteIndex];
        require(corpseSprite.tile == rotatedCorpse.tile && corpseSprite.position.z == rotatedCorpse.position.z &&
            corpseSprite.rightAxis.x == rotatedCorpse.rightAxis.x && corpseSprite.upAxis.z == rotatedCorpse.upAxis.z,
            "Corpse changed pose after camera rotation or display density change");
        require(std::abs(corpseSprite.upAxis.y) < 0.5f && corpseSprite.size.x > 0, "Corpse is not lying on terrain");
        individualsBattle.reset(); updateSceneSprites(rebuilt, individualsBattle, camera);
        for (const auto& soldier : rebuilt.individuals->soldiers)
            require(soldier.life == SoldierLife::Alive, "Reset left a corpse behind");
        for (const DirectX::XMFLOAT2 direction : {DirectX::XMFLOAT2{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            BattleSimulation frontBattle;
            for (unsigned team = 0; team < 2; ++team) {
                auto& f = frontBattle.formations[team];
                const float sign = team == 0 ? -1.0f : 1.0f;
                f.x = f.targetX = direction.x * sign * 14;
                f.z = f.targetZ = direction.y * sign * 14;
                f.state = FormationState::Engaged;
                const float forwardX = direction.x * (team == 0 ? 1 : -1);
                const float forwardZ = direction.y * (team == 0 ? 1 : -1);
                // 命中時刻の検証は敵を正面に捉えた状態から始める。旋回は別途検証する。
                f.heading = std::atan2(forwardZ, forwardX);
                for (auto& group : f.organization.smallGroups) group.heading = f.heading;
                const unsigned hurtGroup = forwardX > 0 ? 14 : forwardX < 0 ? 10 : forwardZ > 0 ? 22 : 2;
                unsigned members = 0;
                for (unsigned id = 0; id < SoldierVisuals::perTeam; ++id)
                    members += Organization::groupForSoldier(id) == hurtGroup;
                f.organization.smallGroups[hurtGroup].strength = 20 * (1 - 3.5f / members);
                f.strength = 480 + f.organization.smallGroups[hurtGroup].strength;
            }
            SoldierVisuals frontVisuals;
            frontVisuals.update(frontBattle);
            for (auto& soldier : frontVisuals.soldiers) soldier.animationTime = 0;
            frontBattle.time = 0.1; frontVisuals.update(frontBattle);
            require(frontVisuals.impacts[0] == 0 && frontVisuals.impacts[1] == 0,
                "Impact event was emitted before the thrust reached its target");
            std::vector<bool> hitTargets(frontVisuals.soldiers.size(), false);
            for (const auto& soldier : frontVisuals.soldiers) {
                require(soldier.life == SoldierLife::Alive, "Soldier died before spear impact");
                if (soldier.attacking) hitTargets[static_cast<unsigned>(soldier.attackTarget)] = true;
            }
            std::array<unsigned, 2> expectedDeaths{};
            for (unsigned i = 0; i < frontVisuals.soldiers.size(); ++i) {
                const auto& soldier = frontVisuals.soldiers[i];
                const unsigned team = i / SoldierVisuals::perTeam;
                if (hitTargets[i] && soldier.attacking &&
                    frontBattle.formations[team].organization.smallGroups[soldier.smallGroup].strength < 20)
                    ++expectedDeaths[team];
            }
            for (auto& count : expectedDeaths) {
                require(count > 0, "Injured group has no reachable impact target");
                count = std::min(count, 3u);
            }
            // 同じ時刻での再描画では命中も動作時計も進めない。
            frontVisuals.update(frontBattle);
            for (const auto& soldier : frontVisuals.soldiers)
                require(soldier.life == SoldierLife::Alive, "Redraw generated a hit");
            frontBattle.time = 1; frontVisuals.update(frontBattle);
            require(frontVisuals.impacts[0] > 0 && frontVisuals.impacts[1] > 0, "Spear impact did not emit audio event");
            const auto impactCounts = frontVisuals.impacts;
            frontVisuals.update(frontBattle);
            require(frontVisuals.impacts == impactCounts, "Redraw duplicated impact events");
            unsigned dead = 0;
            for (unsigned i = 0; i < frontVisuals.soldiers.size(); ++i) {
                const auto& soldier = frontVisuals.soldiers[i];
                if (soldier.life == SoldierLife::Alive) continue;
                require(hitTargets[i], "Casualty was not an attack target");
                ++dead;
                const unsigned team = i / SoldierVisuals::perTeam;
                const auto& f = frontBattle.formations[team];
                require(f.organization.smallGroups[soldier.smallGroup].strength < 20,
                    "Damage was transferred to an unharmed small group");
                const float sign = team == 0 ? 1.0f : -1.0f;
                require(((soldier.position.x - f.x) * direction.x + (soldier.position.z - f.z) * direction.y) * sign > 12,
                    "Casualty selection ignored the enemy direction");
            }
            require(dead == expectedDeaths[0] + expectedDeaths[1], "Small-group loss budgets were not respected");
            for (unsigned step = 0; step < 20; ++step) {
                frontBattle.time += 0.1; frontVisuals.update(frontBattle);
            }
            std::array<unsigned, 2> accumulatedDeaths{};
            for (unsigned i = 0; i < frontVisuals.soldiers.size(); ++i) {
                const auto& soldier = frontVisuals.soldiers[i];
                if (soldier.life == SoldierLife::Alive) continue;
                const unsigned team = i / SoldierVisuals::perTeam;
                require(frontBattle.formations[team].organization.smallGroups[soldier.smallGroup].strength < 20,
                    "Repeated impacts transferred damage to an unharmed group");
                ++accumulatedDeaths[team];
            }
            require(accumulatedDeaths[0] <= 3 && accumulatedDeaths[1] <= 3,
                "Repeated impacts exceeded a small-group loss budget");
            dead = accumulatedDeaths[0] + accumulatedDeaths[1];
            frontBattle.formations[1].x += 100;
            frontBattle.formations[0].strength = 250;
            for (auto& group : frontBattle.formations[0].organization.smallGroups) group.strength = 10;
            frontBattle.time += 10; frontVisuals.update(frontBattle);
            unsigned afterSeparation = 0;
            for (const auto& soldier : frontVisuals.soldiers) afterSeparation += soldier.life != SoldierLife::Alive;
            require(afterSeparation == dead, "Distant enemy caused rear casualties");
        }
        BattleSimulation exchanged;
        BattleSimulation localRout;
        for (unsigned team = 0; team < 2; ++team) {
            auto& f = localRout.formations[team];
            f.z = f.targetZ = team == 0 ? -14.0f : 14.0f;
            f.maneuverEnabled = false;
        }
        localRout.formations[0].organization.smallGroups[22].morale = 20;
        SoldierVisuals routVisuals;
        routVisuals.update(localRout);
        localRout.toggle(); localRout.update(0.1f); routVisuals.update(localRout);
        unsigned runners = 0, fighters = 0;
        for (unsigned id = 0; id < SoldierVisuals::perTeam; ++id) {
            const auto& s = routVisuals.soldiers[id];
            if (s.smallGroup == 22) {
                require(!s.attacking && s.attackTarget == -1, "Fleeing soldier kept attacking");
                runners += s.walking;
            } else fighters += s.attacking;
        }
        require(runners == 0 && fighters > 0 && localRout.formations[0].organization.smallGroups[22].fleeBlocked,
            "Blocked rout walked through its rear line or stopped the whole army");
        auto markedScene = makeScene(1000);
        const auto spriteCount = markedScene.sprites.size();
        const auto markerCount = [&]() {
            unsigned count = 0;
            for (std::size_t i = markedScene.routMarkerStart; i < markedScene.sprites.size(); ++i)
                count += markedScene.sprites[i].tile == 12;
            return count;
        };
        updateSceneSprites(markedScene, localRout, camera, 0, 22);
        unsigned affected = 0;
        for (unsigned id = 0; id < 25; ++id) affected += localRout.routInfluences(0, 22, id);
        require(markerCount() == 96 + affected * 12, "Rout range markers disagree with combat influence");
        const auto dot = markedScene.sprites[markedScene.routMarkerStart];
        require(std::abs(battleDistance({dot.position.x, dot.position.z}, localRout.formations[0].groupPosition(22)) - BattleSimulation::routRadius) < 0.001f,
            "Rout range marker uses the wrong world radius");
        updateSceneSprites(markedScene, localRout, camera, 0, 17);
        require(markerCount() == 96 + localRout.nearbyRouts(0, 17) * 12, "Receiver markers omitted a shock source");
        updateSceneSprites(markedScene, localRout, camera);
        require(markerCount() == 0 && markedScene.sprites.size() == spriteCount, "Deselection left markers or changed GPU capacity");
        auto expiredRout = localRout;
        expiredRout.formations[0].organization.smallGroups[22].routShock = 0;
        updateSceneSprites(markedScene, expiredRout, camera, 0, 22);
        require(markerCount() == 0, "Expired shock kept its range markers");
        expiredRout = localRout; expiredRout.result = BattleResult::RedVictory;
        updateSceneSprites(markedScene, expiredRout, camera, 0, 22);
        require(markerCount() == 0, "Finished battle kept its range markers");
        for (unsigned id : {2u, 7u, 12u, 17u}) localRout.formations[0].organization.smallGroups[id].offsetX = 100;
        // 退路が背後にあるため、有限速度で向き直る時間も進める。
        for (unsigned step = 0; step < 25; ++step) { localRout.update(0.1f); routVisuals.update(localRout); }
        for (unsigned id = 0; id < SoldierVisuals::perTeam; ++id) {
            const auto& soldier = routVisuals.soldiers[id];
            if (soldier.smallGroup == 22 && soldier.walking) ++runners;
        }
        require(runners > 0 && !localRout.formations[0].organization.smallGroups[22].fleeBlocked,
            "Opened retreat path did not restart walking");
        auto& exchangedGroups = exchanged.formations[0].organization.smallGroups;
        exchangedGroups[20].offsetZ = -5.2f;
        exchangedGroups[15].offsetZ = 5.2f;
        SoldierVisuals exchangedVisuals;
        exchangedVisuals.update(exchanged);
        const auto beforeExchange = exchangedVisuals.soldiers;
        std::swap(exchangedGroups[20].slot, exchangedGroups[15].slot);
        exchangedGroups[20].offsetZ = exchangedGroups[15].offsetZ = 0;
        exchanged.time = 1;
        exchangedVisuals.update(exchanged);
        for (unsigned i = 0; i < exchangedVisuals.soldiers.size(); ++i) {
            const auto& s = exchangedVisuals.soldiers[i];
            const auto& old = beforeExchange[i];
            require(s.smallGroup == old.smallGroup && s.life == old.life &&
                std::abs(s.position.x - old.position.x) < 0.0001f && std::abs(s.position.z - old.position.z) < 0.0001f,
                "Slot exchange changed soldier identity or teleported a soldier");
        }
        auto meleeScene = makeScene(10000, walkingOptions);
        BattleSimulation meleeBattle;
        meleeBattle.formations[0].z = meleeBattle.formations[0].targetZ = -14;
        meleeBattle.formations[1].z = meleeBattle.formations[1].targetZ = 14;
        meleeBattle.formations[1].morale = 100;
        meleeBattle.formations[0].state = meleeBattle.formations[1].state = FormationState::Engaged;
        updateSceneSprites(meleeScene, meleeBattle, Camera{});
        meleeBattle.time = 0.1; updateSceneSprites(meleeScene, meleeBattle, Camera{});
        unsigned attackers = 0;
        unsigned frontId = 0;
        for (unsigned i = 0; i < meleeScene.individuals->soldiers.size(); ++i) {
            const auto& soldier = meleeScene.individuals->soldiers[i];
            if (!soldier.attacking) continue;
            ++attackers;
            if (i < SoldierVisuals::perTeam) frontId = i;
            require(soldier.attackTarget >= 0 && !soldier.walking, "Attacker has no target or is walking");
            const auto& target = meleeScene.individuals->soldiers[static_cast<unsigned>(soldier.attackTarget)];
            require(target.life == SoldierLife::Alive && std::hypot(soldier.position.x - target.position.x,
                soldier.position.z - target.position.z) <= 3.5f, "Attacker is swinging at a distant or dead enemy");
            require(meleeScene.sprites[meleeScene.soldierBindings[i].spriteIndex].tile >= Scene::attackRow * Scene::atlasColumns,
                "Contacting soldier is not using the attack atlas");
        }
        require(attackers > 0 && attackers < 300, "Whole formation is attacking");
        const auto rear = meleeScene.individuals->soldiers[0];
        require(!rear.attacking && !rear.walking, "Rear soldier did not wait");
        require(frontId >= 71, "Missing front replacement candidate");
        const unsigned replacement = frontId - 71;
        const auto beforeReplacement = meleeScene.individuals->soldiers[replacement].position;
        require(meleeScene.individuals->soldiers[replacement].smallGroup ==
            meleeScene.individuals->soldiers[frontId].smallGroup, "Replacement crossed small-group boundary");
        auto& front = meleeScene.individuals->soldiers[frontId];
        front.life = SoldierLife::Fallen; front.deathTime = -10;
        const auto deathPosition = front.position;
        meleeBattle.time += 0.5; updateSceneSprites(meleeScene, meleeBattle, Camera{});
        require(meleeScene.individuals->soldiers[replacement].position.z > beforeReplacement.z + 0.1f,
            "Next rank did not fill front vacancy");
        require(front.position.z == deathPosition.z, "Replenishment moved a corpse");
        require(meleeScene.individuals->soldiers[0].animationTime == rear.animationTime,
            "Uninvolved rear rank advanced attack animation");
        BattleSimulation flanking;
        SoldierVisuals flankingVisuals;
        flankingVisuals.update(flanking);
        flanking.toggle();
        bool observedFlank = false;
        for (unsigned step = 0; step < 600; ++step) {
            flanking.update(1.0f / 60);
            flankingVisuals.update(flanking);
            const auto& soldier = flankingVisuals.soldiers[43 * 71];
            const auto slot = SoldierVisuals::offset(43 * 71);
            if (soldier.position.x < slot.x - 5.9f && soldier.position.z > flanking.formations[0].z + slot.y + 3 && soldier.walking) {
                observedFlank = true; break;
            }
        }
        const unsigned flankSoldier = 43 * 71;
        const auto& flanker = flankingVisuals.soldiers[flankSoldier];
        const auto base = SoldierVisuals::offset(flankSoldier);
        require(observedFlank && flanker.smallGroup == 15 && flanker.position.x < base.x - 5.9f &&
            flanker.position.z > flanking.formations[0].z + base.y + 3,
            "Soldier did not follow its small group's flank route");
        require(flanker.life == SoldierLife::Alive && flanker.walking,
            "Moving flank soldier did not remain alive and walking");
        flanking.move(0, 0, -55); flanking.hold(1);
        // 追従と旋回も継続更新し、20秒後の整列を検証する。
        for (unsigned step = 0; step < 600; ++step) {
            flanking.update(1.0f / 30); flankingVisuals.update(flanking);
        }
        const auto& restored = flankingVisuals.soldiers[flankSoldier];
        require(restored.life == SoldierLife::Alive && !restored.attacking &&
            std::abs(restored.position.x - flanking.formations[0].x - base.x) < 0.6f,
            "Returning soldier did not follow its restored small-group slot");
        std::cout << "Scene, camera and individual soldier checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
