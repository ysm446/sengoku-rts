#include "simulation.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

BattleSimulation::BattleSimulation() { reset(); }
void BattleSimulation::move(unsigned index, float x, float z) {
    if (index >= formations.size() || !std::isfinite(x) || !std::isfinite(z))
        throw std::invalid_argument("Invalid formation move command");
    auto& formation = formations[index];
    // 隊列の半幅を含めて地形の内側に収める。
    formation.targetX = std::clamp(x, -60.0f, 60.0f);
    formation.targetZ = std::clamp(z, -60.0f, 60.0f);
}
void BattleSimulation::hold(unsigned index) {
    if (index >= formations.size()) throw std::invalid_argument("Invalid formation hold command");
    auto& formation = formations[index];
    formation.targetX = formation.x; formation.targetZ = formation.z; formation.moving = false;
}
void BattleSimulation::reset() {
    running = false; time = 0;
    // 最小デモの配置と移動先。関ヶ原のデータではない。
    formations = {{{0, -22, 0, -14, DirectX::XM_PIDIV2, 1.8f, false},
                   {0, 22, 0, 14, -DirectX::XM_PIDIV2, 1.8f, false}}};
}
void BattleSimulation::update(float seconds) {
    if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("Invalid simulation time step");
    if (!running) return;
    time += seconds;
    for (auto& formation : formations) {
        const float dx = formation.targetX - formation.x, dz = formation.targetZ - formation.z;
        const float distance = std::hypot(dx, dz);
        if (distance < 0.0001f) { formation.moving = false; continue; }
        formation.heading = std::atan2(dz, dx);
        const float step = std::min(distance, formation.speed * seconds);
        formation.x += dx / distance * step; formation.z += dz / distance * step;
        formation.moving = step < distance;
        if (!formation.moving) { formation.x = formation.targetX; formation.z = formation.targetZ; }
    }
}
