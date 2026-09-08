#pragma once
#include "scene.h"
#include "historical_scenario.h"
#include "formation_world.h"

struct HistoricalScene {
    Scene scene;
    std::size_t armyStart = 0;
    std::vector<unsigned> labelTiles;
    std::vector<DirectX::XMFLOAT3> labelPositions;
    explicit HistoricalScene(const history::Scenario& scenario, const SceneOptions& options);
    void update(const history::Scenario& scenario, double minute, const Camera& camera, int selected, bool arrows,
        const std::vector<history::Pose>& poses = {}, const std::vector<BattlePoint>& goals = {}, const FormationWorld* world = nullptr);
};
