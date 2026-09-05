#include "scene.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <limits>

DirectX::XMFLOAT2 SoldierVisuals::offset(unsigned id) {
    constexpr unsigned columns = 71;
    constexpr float spacing = 26.0f / columns;
    const auto coordinate = [&](unsigned cell) {
        const unsigned group = cell * 5 / columns;
        const unsigned first = (group * columns + 4) / 5;
        const unsigned last = ((group + 1) * columns + 4) / 5 - 1;
        const float center = (first + last) * 0.5f;
        return (center - 35 + (cell - center) * 0.8f) * spacing;
    };
    return {coordinate(id % columns), coordinate(id / columns)};
}

void SoldierVisuals::update(const BattleSimulation& simulation) {
    if (generation != simulation.generation || soldiers.empty() || simulation.time < time) {
        generation = simulation.generation; time = simulation.time;
        impacts = {}; impactPositions = {};
        soldiers.assign(perTeam * 2, {});
        for (unsigned i = 0; i < soldiers.size(); ++i) {
            auto& soldier = soldiers[i];
            const auto& formation = simulation.formations[i / perTeam];
            const auto relative = offset(i % perTeam);
            soldier.slot = relative;
            soldier.smallGroup = Organization::groupForSoldier(i % perTeam);
            const auto& group = formation.organization.smallGroups[soldier.smallGroup];
            soldier.position = {formation.x + relative.x + group.displacementX(soldier.smallGroup), 0, formation.z + relative.y + group.displacementZ(soldier.smallGroup)};
            soldier.position.y = terrainHeight(soldier.position.x, soldier.position.z) + 0.03f;
            soldier.heading = formation.heading;
            soldier.walking = formation.moving;
            soldier.animationTime = static_cast<double>((i * 37) % 101) / 101;
        }
    }
    const double dt = std::max(0.0, simulation.time - time);
    if (dt > 0) {
        for (unsigned team = 0; team < 2; ++team) {
            const auto& formation = simulation.formations[team];
            const auto& enemy = simulation.formations[1 - team];
            if (formation.state != FormationState::Engaged) continue;
            const bool alongX = std::abs(enemy.x - formation.x) > std::abs(enemy.z - formation.z);
            const float sign = (alongX ? enemy.x - formation.x : enemy.z - formation.z) >= 0 ? 1.0f : -1.0f;
            std::array<float, 71 * 25> fronts;
            fronts.fill(-std::numeric_limits<float>::max());
            std::array<unsigned, 71 * 25> ranks{};
            for (unsigned id = 0; id < perTeam; ++id) {
                const auto relative = offset(id);
                const unsigned lane = Organization::groupForSoldier(id) * 71 + (alongX ? id / 71 : id % 71);
                fronts[lane] = std::max(fronts[lane], sign * (alongX ? relative.x : relative.y));
            }
            // 列の並びを維持して生存兵士の目標位置を詰める。死亡位置は変更しない。
            for (unsigned n = 0; n < perTeam; ++n) {
                const unsigned id = sign > 0 ? perTeam - 1 - n : n;
                auto& soldier = soldiers[team * perTeam + id];
                if (soldier.life != SoldierLife::Alive || formation.organization.smallGroups[soldier.smallGroup].routed) continue;
                const unsigned lane = soldier.smallGroup * 71 + (alongX ? id / 71 : id % 71);
                soldier.slot = offset(id);
                const float forward = sign * (fronts[lane] - ranks[lane]++ * (26.0f / 71) * 0.8f);
                if (alongX) soldier.slot.x = forward;
                else soldier.slot.y = forward;
            }
        }
    }
    for (unsigned i = 0; i < soldiers.size(); ++i) {
        auto& soldier = soldiers[i];
        const auto& formation = simulation.formations[i / perTeam];
        const unsigned id = i % perTeam;
        if (soldier.life != SoldierLife::Alive) {
            if (simulation.time - soldier.deathTime >= 0.8) soldier.life = SoldierLife::Fallen;
            continue;
        }
        if (dt <= 0) continue;
        soldier.attacking = false; soldier.attackTarget = -1;
        const auto relative = soldier.slot;
        const float variation = static_cast<float>((id * 53) % 101) / 100;
        const float disorder = (1 - formation.cohesion / 100) * 0.6f;
        const auto& group = formation.organization.smallGroups[soldier.smallGroup];
        const float targetX = formation.x + relative.x + group.displacementX(soldier.smallGroup) + std::sin(id * 2.4f) * disorder;
        const float targetZ = formation.z + relative.y + group.displacementZ(soldier.smallGroup) + std::cos(id * 1.7f) * disorder;
        const float dx = targetX - soldier.position.x, dz = targetZ - soldier.position.z;
        const float distance = std::hypot(dx, dz);
        // 個体ごとの追従速度。経路や損害を決める部隊Simulationへは書き戻さない。
        const float follow = static_cast<float>(1 - std::exp(-(4 + variation * 5) * dt));
        soldier.position.x += dx * follow; soldier.position.z += dz * follow;
        const float remaining = distance * (1 - follow);
        soldier.walking = remaining > 0.003f || (formation.moving && distance > 0.001f);
        if (remaining <= 0.003f) { soldier.position.x = targetX; soldier.position.z = targetZ; }
        if (soldier.walking && distance > 0.001f) soldier.heading = std::atan2(dz, dx);
        else if (!group.routed) soldier.heading = group.heading;
        soldier.position.y = terrainHeight(soldier.position.x, soldier.position.z) + 0.03f;
    }
    if (dt > 0) {
        // 損害は相手に届く生存前列へだけ割り当てる。両軍の候補は死亡確定前に求める。
        std::array<std::vector<unsigned>, 2> casualties;
        std::array<std::array<unsigned, 25>, 2> casualtyBudget{};
        std::vector<unsigned> hits;
        std::array<std::array<std::vector<unsigned>, 25>, 2> members;
        for (unsigned team = 0; team < 2; ++team) {
            std::array<unsigned, 25> count{}, dead{};
            for (unsigned id = team * perTeam; id < (team + 1) * perTeam; ++id) {
                const auto& s = soldiers[id];
                ++count[s.smallGroup];
                if (s.life != SoldierLife::Alive) ++dead[s.smallGroup];
                else members[team][s.smallGroup].push_back(id);
            }
            for (unsigned group = 0; group < 25; ++group) {
                const auto& unit = simulation.formations[team].organization.smallGroups[group];
                const float fraction = unit.nominalStrength ? std::clamp(unit.strength / unit.nominalStrength, 0.0f, 1.0f) : 0;
                const unsigned desired = count[group] - static_cast<unsigned>(std::ceil(count[group] * fraction));
                casualtyBudget[team][group] = desired > dead[group] ? desired - dead[group] : 0;
            }
        }
        for (unsigned team = 0; team < 2; ++team) {
            const auto& formation = simulation.formations[team];
            const auto& enemy = simulation.formations[1 - team];
            if (formation.state != FormationState::Engaged || enemy.state != FormationState::Engaged) continue;
            for (unsigned group = 0; group < 25; ++group) {
                const auto& unit = formation.organization.smallGroups[group];
                if (unit.routed || unit.route == SmallGroupRoute::Returning || unit.route == SmallGroupRoute::ReliefReserve ||
                    unit.route == SmallGroupRoute::ReliefWithdraw || (unit.attackTarget >= 0 && !unit.canAttack)) continue;
                const auto p = formation.groupPosition(group);
                int targetGroup = unit.attackTarget;
                // 静止した表示確認用の状態でも、小組間の実距離から相手を選べるようにする。
                if (targetGroup < 0) {
                    float nearest = 16;
                    for (unsigned other = 0; other < 25; ++other) {
                        if (enemy.organization.smallGroups[other].routed) continue;
                        const float distance = battleDistance(p, enemy.groupPosition(other));
                        if (distance < nearest) { nearest = distance; targetGroup = static_cast<int>(other); }
                    }
                }
                if (targetGroup < 0 || enemy.organization.smallGroups[targetGroup].routed) continue;
                const auto q = enemy.groupPosition(targetGroup);
                const float distance = battleDistance(p, q);
                if (distance < 0.001f) continue;
                const float fx = (q.x - p.x) / distance, fz = (q.z - p.z) / distance;
                const auto lane = [&](const SoldierVisual& s) {
                    return static_cast<int>(std::floor((-fz * s.position.x + fx * s.position.z) / 0.75f));
                };
                const auto forward = [&](const SoldierVisual& s) { return fx * s.position.x + fz * s.position.z; };
                std::map<int, float> fronts, enemyFronts;
                for (unsigned id : members[team][group]) {
                    const auto& s = soldiers[id];
                    const int key = lane(s);
                    const auto found = fronts.find(key);
                    if (found == fronts.end()) fronts.emplace(key, forward(s));
                    else found->second = std::max(found->second, forward(s));
                }
                for (unsigned id : members[1 - team][targetGroup]) {
                    const auto& s = soldiers[id];
                    const int key = lane(s);
                    const auto found = enemyFronts.find(key);
                    if (found == enemyFronts.end()) enemyFronts.emplace(key, forward(s));
                    else found->second = std::min(found->second, forward(s));
                }
                std::vector<unsigned> opponents;
                for (unsigned id : members[1 - team][targetGroup])
                    if (forward(soldiers[id]) <= enemyFronts.at(lane(soldiers[id])) + 0.1f) opponents.push_back(id);
                for (unsigned id : members[team][group]) {
                    auto& s = soldiers[id];
                    if (forward(s) < fronts.at(lane(s)) - 0.1f) continue;
                    float nearest = std::numeric_limits<float>::max();
                    int target = -1;
                    for (unsigned other : opponents) {
                        const auto& opponent = soldiers[other];
                        const float x = s.position.x - opponent.position.x, z = s.position.z - opponent.position.z;
                        const float squared = x * x + z * z;
                        if (squared < nearest) { nearest = squared; target = static_cast<int>(other); }
                    }
                    if (nearest > 3.5f * 3.5f) continue;
                    s.attacking = true; s.attackTarget = target; s.walking = false;
                    const auto& opponent = soldiers[static_cast<unsigned>(target)];
                    s.heading = std::atan2(opponent.position.z - s.position.z, opponent.position.x - s.position.x);
                    const float variation = static_cast<float>(((id % perTeam) * 53) % 101) / 100;
                    const double next = s.animationTime + dt * (0.85 + variation * 0.3);
                    if (std::floor(next - 0.5) > std::floor(s.animationTime - 0.5)) {
                        hits.push_back(static_cast<unsigned>(target));
                        ++impacts[team]; impactPositions[team] = opponent.position;
                    }
                }
            }
        }
        // 両軍の命中を集めてから死亡を確定する。同じ兵士への複数命中は一人分として扱う。
        std::sort(hits.begin(), hits.end());
        hits.erase(std::unique(hits.begin(), hits.end()), hits.end());
        for (unsigned id : hits) {
            const unsigned team = id / perTeam;
            auto& budget = casualtyBudget[team][soldiers[id].smallGroup];
            if (budget > 0) { casualties[team].push_back(id); --budget; }
        }
        for (unsigned id = 0; id < soldiers.size(); ++id) {
            auto& soldier = soldiers[id];
            if (soldier.life == SoldierLife::Alive && (soldier.walking || soldier.attacking)) {
                const float variation = static_cast<float>(((id % perTeam) * 53) % 101) / 100;
                soldier.animationTime += dt * (0.85 + variation * 0.3);
            }
        }
        for (const auto& team : casualties) for (unsigned id : team) {
            auto& soldier = soldiers[id];
            soldier.life = SoldierLife::Falling; soldier.deathTime = simulation.time; soldier.walking = false;
            soldier.attacking = false; soldier.attackTarget = -1;
        }
        for (unsigned id = 0; id < soldiers.size(); ++id) {
            auto& soldier = soldiers[id];
            if (soldier.attacking && soldiers[static_cast<unsigned>(soldier.attackTarget)].life != SoldierLife::Alive) {
                soldier.attacking = false; soldier.attackTarget = -1;
            }
        }
    }
    time = simulation.time;
}
