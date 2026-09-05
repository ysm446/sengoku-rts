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
    formation.maneuverEnabled = true;
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
    formation.maneuverEnabled = false;
}
void BattleSimulation::reset() {
    ++generation;
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
        updateSmallGroups(static_cast<float>(fixedStep));
        time += fixedStep;
        accumulator = std::max(0.0, accumulator - fixedStep);
    }
}
void BattleSimulation::updateSmallGroups(float seconds) {
    struct Point { float x, z; };
    std::array<std::array<Point, 25>, 2> positions{};
    for (unsigned team = 0; team < 2; ++team) {
        const auto& f = formations[team];
        for (unsigned id = 0; id < 25; ++id) {
            const auto& g = f.organization.smallGroups[id];
            positions[team][id] = {f.x + (static_cast<float>(id % 5) - 2) * 5.2f + g.offsetX,
                f.z + (static_cast<float>(id / 5) - 2) * 5.2f + g.offsetZ};
        }
    }
    // 同じ時点の位置で接敵・通行可否を調べ、陣営の更新順に依存させない。
    for (unsigned team = 0; team < 2; ++team) {
        auto& f = formations[team];
        const auto& enemy = formations[1 - team];
        const bool alongX = std::abs(enemy.x - f.x) > std::abs(enemy.z - f.z);
        const float sign = (alongX ? enemy.x - f.x : enemy.z - f.z) >= 0 ? 1.0f : -1.0f;
        const bool contact = f.state == FormationState::Engaged && enemy.state == FormationState::Engaged;
        for (auto& g : f.organization.smallGroups) {
            if (g.state == SmallGroupState::Engaged) g.fatigue = std::min(30.0f, g.fatigue + seconds);
            else if (g.route == SmallGroupRoute::None) g.fatigue = std::max(0.0f, g.fatigue - seconds * 0.5f);
            if (g.route != SmallGroupRoute::None && (!contact || g.fatigue >= 8 ||
                g.routeAlongX != alongX || g.routeForward * sign < 0)) g.route = SmallGroupRoute::Returning;
        }
        if (contact && f.maneuverEnabled && std::abs(alongX ? enemy.z - f.z : enemy.x - f.x) <= 2) {
            for (unsigned lane : {0u, 4u}) {
                bool occupied = false;
                for (unsigned id = 0; id < 25; ++id) {
                    const auto& g = f.organization.smallGroups[id];
                    if (g.route != SmallGroupRoute::None &&
                        (g.routeAlongX != alongX || (alongX ? id / 5 : id % 5) == lane)) occupied = true;
                }
                if (occupied) continue;
                // 復帰が完了するまで同じ通路へ次の小組を出さない。前寄りの休息済み予備を選ぶ。
                for (unsigned depth = 1; depth < 5; ++depth) {
                    const unsigned rank = sign > 0 ? 4 - depth : depth;
                    const unsigned id = alongX ? lane * 5 + rank : rank * 5 + lane;
                    auto& g = f.organization.smallGroups[id];
                    if (g.route != SmallGroupRoute::None || g.fatigue > 2 || g.offsetX != 0 || g.offsetZ != 0 ||
                        g.state == SmallGroupState::Engaged) continue;
                    g.route = SmallGroupRoute::Outward; g.routeAlongX = alongX;
                    g.routeForward = sign * (6.8f + depth * 5.2f);
                    break;
                }
            }
        }
        for (unsigned id = 0; id < 25; ++id) {
            auto& g = f.organization.smallGroups[id];
            g.targetOffsetX = g.offsetX; g.targetOffsetZ = g.offsetZ;
            g.state = f.defeated() ? SmallGroupState::Retreating :
                f.moving ? SmallGroupState::Advancing : SmallGroupState::Waiting;
            const auto p = positions[team][id];
            if (contact) for (const auto q : positions[1 - team]) {
                const float dx = std::abs(p.x - q.x), dz = std::abs(p.z - q.z);
                if ((dx <= 4.5f && dz <= 7.5f) || (dx <= 7.5f && dz <= 4.5f)) {
                    g.state = SmallGroupState::Engaged;
                    break;
                }
            }
            if (g.route == SmallGroupRoute::None || (!f.maneuverEnabled && !f.defeated())) continue;
            const bool returning = g.route == SmallGroupRoute::Returning;
            if (g.state == SmallGroupState::Engaged && !returning) continue;
            const bool routeX = g.routeAlongX;
            const unsigned lane = routeX ? id / 5 : id % 5;
            const float side = lane == 0 ? -6.1f : 6.1f;
            const float lateral = routeX ? g.offsetZ : g.offsetX;
            const float forward = routeX ? g.offsetX : g.offsetZ;
            if (returning) {
                // 外側の通路を後退して元の列へ戻ってから横移動する。隊列を斜めに横切らない。
                if (routeX) { g.targetOffsetX = 0; if (forward == 0) g.targetOffsetZ = 0; }
                else { g.targetOffsetZ = 0; if (forward == 0) g.targetOffsetX = 0; }
            } else {
                if (std::abs(lateral - side) < 0.001f) g.route = SmallGroupRoute::Forward;
                if (routeX) { g.targetOffsetZ = side; if (g.route == SmallGroupRoute::Forward) g.targetOffsetX = g.routeForward; }
                else { g.targetOffsetX = side; if (g.route == SmallGroupRoute::Forward) g.targetOffsetZ = g.routeForward; }
            }
            const float dx = g.targetOffsetX - g.offsetX, dz = g.targetOffsetZ - g.offsetZ;
            const float distance = std::hypot(dx, dz);
            if (distance < 0.0001f) {
                if (returning && g.offsetX == 0 && g.offsetZ == 0) g.route = SmallGroupRoute::None;
                continue;
            }
            const float travel = std::min(distance, f.speed * seconds);
            const Point next{p.x + dx / distance * travel, p.z + dz / distance * travel};
            bool blocked = std::abs(next.x) > 76 || std::abs(next.z) > 76;
            // 小組の幅と双方の一刻みの移動量を確保する。障害物経路探索は後続工程。
            const float clearance = 4.5f + 2 * std::max(f.speed, enemy.speed) * seconds;
            for (unsigned otherTeam = 0; otherTeam < 2; ++otherTeam)
                for (unsigned other = 0; other < 25; ++other) {
                    if (otherTeam == team && other == id) continue;
                    const auto q = positions[otherTeam][other];
                    if (std::abs(next.x - q.x) < clearance && std::abs(next.z - q.z) < clearance) blocked = true;
                }
            if (blocked) continue;
            g.offsetX += dx / distance * travel; g.offsetZ += dz / distance * travel;
            if (travel == distance) { g.offsetX = g.targetOffsetX; g.offsetZ = g.targetOffsetZ; }
            g.state = returning ? SmallGroupState::Retreating : SmallGroupState::Advancing;
        }
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
        if (!broken[i]) { hold(i); f.maneuverEnabled = true; continue; }
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
