#include "scene.h"
#include <algorithm>
#include <cmath>

DirectX::XMFLOAT2 SoldierVisuals::offset(unsigned id) {
    constexpr unsigned columns = 71;
    constexpr float spacing = 26.0f / columns;
    return {(static_cast<float>(id % columns) - 35) * spacing,
            (static_cast<float>(id / columns) - 35) * spacing};
}

void SoldierVisuals::update(const BattleSimulation& simulation) {
    if (generation != simulation.generation || soldiers.empty() || simulation.time < time) {
        generation = simulation.generation; time = simulation.time;
        soldiers.assign(perTeam * 2, {});
        for (unsigned i = 0; i < soldiers.size(); ++i) {
            auto& soldier = soldiers[i];
            const auto& formation = simulation.formations[i / perTeam];
            const auto relative = offset(i % perTeam);
            soldier.position = {formation.x + relative.x, 0, formation.z + relative.y};
            soldier.position.y = terrainHeight(soldier.position.x, soldier.position.z) + 0.03f;
            soldier.heading = formation.heading;
            soldier.walking = formation.moving;
            soldier.animationTime = static_cast<double>((i * 37) % 101) / 101;
        }
    }
    const double dt = std::max(0.0, simulation.time - time);
    for (unsigned i = 0; i < soldiers.size(); ++i) {
        auto& soldier = soldiers[i];
        const auto& formation = simulation.formations[i / perTeam];
        const unsigned id = i % perTeam;
        const unsigned survivors = static_cast<unsigned>(std::ceil(perTeam * std::clamp(formation.strength / 500, 0.0f, 1.0f)));
        if (soldier.life == SoldierLife::Alive && (id * 137) % perTeam >= survivors) {
            soldier.life = SoldierLife::Falling;
            soldier.deathTime = simulation.time;
            soldier.walking = false;
        }
        if (soldier.life != SoldierLife::Alive) {
            if (simulation.time - soldier.deathTime >= 0.8) soldier.life = SoldierLife::Fallen;
            continue;
        }
        if (dt <= 0) continue;
        const auto relative = offset(id);
        const float variation = static_cast<float>((id * 53) % 101) / 100;
        const float disorder = (1 - formation.cohesion / 100) * 0.6f;
        const float targetX = formation.x + relative.x + std::sin(id * 2.4f) * disorder;
        const float targetZ = formation.z + relative.y + std::cos(id * 1.7f) * disorder;
        const float dx = targetX - soldier.position.x, dz = targetZ - soldier.position.z;
        const float distance = std::hypot(dx, dz);
        // 個体ごとの追従速度。経路や損害を決める部隊Simulationへは書き戻さない。
        const float follow = static_cast<float>(1 - std::exp(-(4 + variation * 5) * dt));
        soldier.position.x += dx * follow; soldier.position.z += dz * follow;
        const float remaining = distance * (1 - follow);
        soldier.walking = remaining > 0.003f || (formation.moving && distance > 0.001f);
        if (remaining <= 0.003f) { soldier.position.x = targetX; soldier.position.z = targetZ; }
        if (soldier.walking && distance > 0.001f) soldier.heading = std::atan2(dz, dx);
        else soldier.heading = formation.heading;
        if (soldier.walking || formation.state == FormationState::Engaged)
            soldier.animationTime += dt * (0.85 + variation * 0.3);
        soldier.position.y = terrainHeight(soldier.position.x, soldier.position.z) + 0.03f;
    }
    time = simulation.time;
}
