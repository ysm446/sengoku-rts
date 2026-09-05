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
    if (formation.defeated()) return;
    // 隊列の半幅を含めて地形の内側に収める。
    formation.targetX = std::clamp(x, -60.0f, 60.0f);
    formation.targetZ = std::clamp(z, -60.0f, 60.0f);
}
void BattleSimulation::hold(unsigned index) {
    if (index >= formations.size()) throw std::invalid_argument("Invalid formation hold command");
    auto& formation = formations[index];
    if (formation.defeated()) return;
    formation.targetX = formation.x; formation.targetZ = formation.z; formation.moving = false;
    formation.state = FormationState::Idle;
}
void BattleSimulation::reset() {
    running = false; time = 0; accumulator = 0; result = BattleResult::Ongoing;
    // 最小デモの配置と移動先。関ヶ原のデータではない。
    formations = {{{0, -22, 0, 0, DirectX::XM_PIDIV2, 1.8f, false},
                   {0, 22, 0, 0, -DirectX::XM_PIDIV2, 1.8f, false}}};
    formations[1].morale = 85; // 退却の差を観察する試作条件。陣営固有の補正ではない。
}
void BattleSimulation::update(float seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 3600) throw std::invalid_argument("Invalid simulation time step");
    if (!running) return;
    constexpr double fixedStep = 1.0 / 60.0;
    accumulator += seconds;
    while (accumulator + 1e-8 >= fixedStep) {
        step(static_cast<float>(fixedStep));
        time += fixedStep;
        accumulator = std::max(0.0, accumulator - fixedStep);
    }
}
void BattleSimulation::step(float seconds) {
    for (auto& formation : formations) {
        if (formation.state == FormationState::Routed) continue;
        const bool retreating = formation.defeated();
        const float dx = formation.targetX - formation.x, dz = formation.targetZ - formation.z;
        const float distance = std::hypot(dx, dz);
        if (distance < 0.0001f) {
            formation.moving = false;
            formation.state = retreating ? FormationState::Routed : FormationState::Idle;
            continue;
        }
        formation.heading = std::atan2(dz, dx);
        const float step = std::min(distance, formation.speed * (retreating ? 1.5f : 1.0f) * seconds);
        formation.x += dx / distance * step; formation.z += dz / distance * step;
        formation.moving = step < distance;
        if (!formation.moving) { formation.x = formation.targetX; formation.z = formation.targetZ; }
        formation.state = retreating ? (formation.moving ? FormationState::Retreating : FormationState::Routed) :
            (formation.moving ? FormationState::Marching : FormationState::Idle);
    }
    if (result != BattleResult::Ongoing) return;
    auto& red = formations[0]; auto& blue = formations[1];
    const float dx = blue.x - red.x, dz = blue.z - red.z;
    // 表示密度や向きに依存しない、半幅14の軸平行な隊列として扱う。
    const float overlapX = 28 - std::abs(dx), overlapZ = 28 - std::abs(dz);
    if (overlapX < -0.001f || overlapZ < -0.001f) return;
    if (overlapX > 0 && overlapZ > 0) {
        const auto keepInside = [](float& a, float& b) {
            const float shift = std::max(0.0f, -60 - std::min(a, b)) - std::max(0.0f, std::max(a, b) - 60);
            a += shift; b += shift;
        };
        if (overlapX < overlapZ) {
            const float correction = std::copysign(overlapX * 0.5f, dx);
            red.x -= correction; blue.x += correction;
            keepInside(red.x, blue.x);
        } else {
            const float correction = std::copysign(overlapZ * 0.5f, dz);
            red.z -= correction; blue.z += correction;
            keepInside(red.z, blue.z);
        }
    }
    red.heading = std::atan2(blue.z - red.z, blue.x - red.x);
    blue.heading = std::atan2(red.z - blue.z, red.x - blue.x);
    // 両軍の損害を同じ時点の状態から計算し、更新順の有利不利をなくす。
    const auto power = [](const Formation& f) {
        return (f.strength / 500) * (0.5f + f.cohesion / 200) * (0.5f + f.morale / 200);
    };
    const std::array<float, 2> losses{6 * power(blue) * seconds, 6 * power(red) * seconds};
    std::array<bool, 2> broken{};
    for (unsigned i = 0; i < formations.size(); ++i) {
        auto& f = formations[i];
        f.moving = false; f.state = FormationState::Engaged;
        f.strength = std::max(0.0f, f.strength - losses[i]);
        f.cohesion = std::max(0.0f, f.cohesion - 1.5f * seconds - losses[i] * 0.08f);
        f.morale = std::max(0.0f, f.morale - (1 + (100 - f.cohesion) * 0.025f) * seconds - losses[i] * 0.2f);
        broken[i] = f.morale <= 20 || f.cohesion <= 15 || f.strength <= 100;
    }
    if (!broken[0] && !broken[1]) return;
    result = broken[0] && broken[1] ? BattleResult::Draw : broken[0] ? BattleResult::BlueVictory : BattleResult::RedVictory;
    for (unsigned i = 0; i < formations.size(); ++i) {
        auto& f = formations[i];
        if (!broken[i]) { hold(i); continue; }
        const auto& enemy = formations[1 - i];
        float awayX = f.x - enemy.x, awayZ = f.z - enemy.z;
        const float distance = std::hypot(awayX, awayZ);
        if (distance < 0.001f) { awayX = 0; awayZ = i == 0 ? -1.0f : 1.0f; }
        else { awayX /= distance; awayZ /= distance; }
        f.targetX = std::clamp(f.x + awayX * 120, -60.0f, 60.0f);
        f.targetZ = std::clamp(f.z + awayZ * 120, -60.0f, 60.0f);
        f.heading = std::atan2(awayZ, awayX); f.state = FormationState::Retreating;
    }
}
