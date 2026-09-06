#include "scene.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <limits>
#include <unordered_map>

void SoldierVisuals::separateOverlaps(float seconds) {
    if (!std::isfinite(seconds) || seconds <= 0) return;
    // 固定IDの全表示兵士で判定する。描画する人数や陣営順序によって押し合いを変えない。
    const auto key = [](int x, int z) {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) | static_cast<std::uint32_t>(z);
    };
    const auto cell = [](float coordinate) { return static_cast<int>(std::floor(coordinate / minimumSpacing)); };
    // 格子ごとのvectorを作らず、IDの連結リストで近隣を保持する。
    std::unordered_map<std::uint64_t, unsigned> grid;
    grid.reserve(soldiers.size());
    const unsigned end = static_cast<unsigned>(soldiers.size());
    std::vector<unsigned> next(soldiers.size(), end);
    for (unsigned i = end; i-- > 0;) if (soldiers[i].life == SoldierLife::Alive) {
        auto [entry, inserted] = grid.try_emplace(key(cell(soldiers[i].position.x), cell(soldiers[i].position.z)), i);
        if (!inserted) { next[i] = entry->second; entry->second = i; }
    }
    std::vector<DirectX::XMFLOAT2> corrections(soldiers.size());
    for (unsigned i = 0; i < soldiers.size(); ++i) {
        const auto& a = soldiers[i];
        if (a.life != SoldierLife::Alive) continue;
        const int x = cell(a.position.x), z = cell(a.position.z);
        for (int dz = -1; dz <= 1; ++dz) for (int dx = -1; dx <= 1; ++dx) {
            const auto found = grid.find(key(x + dx, z + dz));
            if (found == grid.end()) continue;
            for (unsigned j = found->second; j != end; j = next[j]) if (j > i) {
                const auto& b = soldiers[j];
                float vx = b.position.x - a.position.x, vz = b.position.z - a.position.z;
                const float squared = vx * vx + vz * vz;
                if (squared >= minimumSpacing * minimumSpacing) continue;
                const float distance = std::sqrt(squared);
                if (distance > 0.00001f) { vx /= distance; vz /= distance; }
                else { vx = 1; vz = 0; } // 完全一致でも有限の方向を持たせる。
                const float push = (minimumSpacing - distance) * 0.5f;
                corrections[i].x -= vx * push; corrections[i].y -= vz * push;
                corrections[j].x += vx * push; corrections[j].y += vz * push;
            }
        }
    }
    for (unsigned i = 0; i < soldiers.size(); ++i) {
        auto& s = soldiers[i]; const auto push = corrections[i];
        const float distance = std::hypot(push.x, push.y);
        if (distance == 0) continue;
        // 混雑時の補正量に上限を設ける。残る重なりは次の更新で再判定する。
        const float maxPush = std::min(minimumSpacing * 0.5f, minimumSpacing * 30 * seconds);
        const float scale = std::min(1.0f, maxPush / distance);
        s.position.x += push.x * scale; s.position.z += push.y * scale;
        s.position.y = terrainHeight(s.position.x, s.position.z) + 0.03f;
    }
}

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
        rangedDeaths = {};
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
    std::vector<float> desiredHeadings;
    desiredHeadings.reserve(soldiers.size());
    for (const auto& soldier : soldiers) desiredHeadings.push_back(soldier.heading);
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
        // 隊列確認と同じ加速値で持ち場へ追従する。遠い持ち場への急加速を制限する。
        // 表示密度と実兵力は異なるため、経路や損害を決めるSimulationへは書き戻さない。
        const float follow = static_cast<float>(1 - std::exp(-(4 + variation * 5) * dt));
        const auto movement = movementProfile(formation.groupUnit(soldier.smallGroup));
        const float maxSpeed = formation.speed * (group.routed || formation.defeated() ? 2.0f : 1.5f);
        const float forwardX = std::cos(soldier.heading), forwardZ = std::sin(soldier.heading);
        const float alignment = distance > 0.003f ? std::clamp((dx * forwardX + dz * forwardZ) / distance, 0.0f, 1.0f) : 0;
        const float wanted = std::min(maxSpeed, distance * follow / static_cast<float>(dt)) * alignment;
        const float acceleration = movement.acceleration * (0.85f + variation * 0.3f);
        soldier.followSpeed += std::clamp(wanted - soldier.followSpeed,
            -acceleration * 2 * static_cast<float>(dt), acceleration * static_cast<float>(dt));
        // 更新開始時の正面へだけ進む。目的地が背後ならその場で旋回し、横滑りを防ぐ。
        // 正面への投影距離を超えて進まず、旋回中に持ち場から遠ざかることも防ぐ。
        const float travel = std::min(distance * alignment, soldier.followSpeed * static_cast<float>(dt));
        soldier.position.x += forwardX * travel; soldier.position.z += forwardZ * travel;
        const float remaining = std::hypot(targetX - soldier.position.x, targetZ - soldier.position.z);
        soldier.walking = travel > 0.00001f;
        if (remaining <= 0.003f) { soldier.position.x = targetX; soldier.position.z = targetZ; soldier.followSpeed = 0; soldier.walking = false; }
        if (remaining > 0.003f) desiredHeadings[i] = std::atan2(dz, dx);
        else if (!group.routed) desiredHeadings[i] = group.heading;
        soldier.position.y = terrainHeight(soldier.position.x, soldier.position.z) + 0.03f;
    }
    if (dt > 0) {
        // 損害は相手に届く生存前列へだけ割り当てる。両軍の候補は死亡確定前に求める。
        separateOverlaps(static_cast<float>(dt));
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
            for (unsigned group = 0; group < 25; ++group) {
                const auto& archer = formation.organization.smallGroups[group];
                const double shotAge = simulation.time - archer.lastShot;
                if (formation.groupUnit(group) != UnitType::Archer || archer.routed || formation.moving ||
                    shotAge < 0 || shotAge >= 1 || archer.attackTarget < 0) continue;
                const auto target = enemy.groupPosition(archer.attackTarget);
                unsigned remaining = static_cast<unsigned>(std::ceil(members[team][group].size() *
                    std::min(BowProfile::maxShooters, archer.strength) / std::max(.001f, archer.strength)));
                for (unsigned id : members[team][group]) {
                    if (remaining == 0) break;
                    auto& soldier = soldiers[id];
                    desiredHeadings[id] = std::atan2(target.z - soldier.position.z, target.x - soldier.position.x);
                    const float turned = turnToward(soldier.heading, desiredHeadings[id], movementProfile(UnitType::Archer).turnRate * static_cast<float>(dt));
                    if (std::abs(std::remainder(desiredHeadings[id] - turned, 6.283185307f)) > .35f) continue;
                    --remaining;
                    soldier.attacking = true; soldier.walking = false;
                    soldier.animationTime = shotAge;
                    const auto& targets = members[1 - team][archer.attackTarget];
                    if (!targets.empty()) soldier.attackTarget = static_cast<int>(targets.front());
                    else soldier.attacking = false;
                }
            }
            if (formation.state != FormationState::Engaged || enemy.state != FormationState::Engaged) continue;
            for (unsigned group = 0; group < 25; ++group) {
                if (formation.groupUnit(group) == UnitType::Archer) continue;
                const auto& unit = formation.organization.smallGroups[group];
                if (unit.routed || unit.resting || unit.route == SmallGroupRoute::Returning || unit.route == SmallGroupRoute::ReliefReserve ||
                    unit.route == SmallGroupRoute::ReliefWithdraw || unit.route == SmallGroupRoute::ReliefCorridor || (unit.attackTarget >= 0 && !unit.canAttack)) continue;
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
                const bool simulated = unit.attackTarget >= 0;
                for (unsigned candidateGroup = 0; candidateGroup < 25; ++candidateGroup) {
                    if (simulated ? unit.activeOpponents[candidateGroup] <= 0 : candidateGroup != static_cast<unsigned>(targetGroup)) continue;
                    targetGroup = static_cast<int>(candidateGroup);
                    unsigned remaining = simulated ? static_cast<unsigned>(std::ceil(members[team][group].size() *
                        unit.activeOpponents[candidateGroup] / std::max(unit.strength, 0.001f))) : static_cast<unsigned>(members[team][group].size());
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
                        if (remaining == 0 || s.attacking || forward(s) < fronts.at(lane(s)) - 0.1f) continue;
                        float nearest = std::numeric_limits<float>::max();
                        int target = -1;
                        for (unsigned other : opponents) {
                            const auto& opponent = soldiers[other];
                            const float x = s.position.x - opponent.position.x, z = s.position.z - opponent.position.z;
                            const float squared = x * x + z * z;
                            if (squared < nearest) { nearest = squared; target = static_cast<int>(other); }
                        }
                        const float reach = meleeProfile(formation.groupUnit(group)).individualRange;
                        if (nearest > reach * reach) continue;
                        const auto& opponent = soldiers[static_cast<unsigned>(target)];
                        desiredHeadings[id] = std::atan2(opponent.position.z - s.position.z, opponent.position.x - s.position.x);
                        const float turned = turnToward(s.heading, desiredHeadings[id],
                            movementProfile(formation.groupUnit(group)).turnRate * static_cast<float>(dt));
                        // 接近後も向き直る時間を必要とし、背を向けたまま攻撃しない。
                        if (std::abs(std::remainder(desiredHeadings[id] - turned, 6.283185307f)) > 0.35f) continue;
                        --remaining;
                        s.attacking = true; s.attackTarget = target; s.walking = false;
                        const float variation = static_cast<float>(((id % perTeam) * 53) % 101) / 100;
                        const double next = s.animationTime + dt * (0.85 + variation * 0.3);
                        if (std::floor(next - 0.5) > std::floor(s.animationTime - 0.5)) {
                            hits.push_back(static_cast<unsigned>(target));
                            ++impacts[team]; impactPositions[team] = opponent.position;
                        }
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
        // 遠隔損害は近接の命中を待たず、着弾した小組の狙点に近い生存兵へ反映する。
        for (unsigned team = 0; team < 2; ++team) for (unsigned group = 0; group < 25; ++group) {
            const auto& state = simulation.formations[team].organization.smallGroups[group];
            if (state.rangedLoss <= 0) continue;
            unsigned count = 0;
            for (unsigned id = team * perTeam; id < (team + 1) * perTeam; ++id) count += soldiers[id].smallGroup == group;
            const unsigned desired = state.nominalStrength ? static_cast<unsigned>(count * state.rangedLoss / state.nominalStrength) : 0;
            if (desired <= rangedDeaths[team][group] || casualtyBudget[team][group] == 0) continue;
            auto candidates = members[team][group];
            std::sort(candidates.begin(), candidates.end(), [&](unsigned a, unsigned b) {
                const auto distance = [&](unsigned id) { return std::hypot(soldiers[id].position.x - state.lastRangedImpact.x,
                    soldiers[id].position.z - state.lastRangedImpact.z); };
                return distance(a) < distance(b);
            });
            for (unsigned id : candidates) {
                if (rangedDeaths[team][group] >= desired || casualtyBudget[team][group] == 0) break;
                if (std::find(casualties[team].begin(), casualties[team].end(), id) != casualties[team].end()) continue;
                casualties[team].push_back(id); ++rangedDeaths[team][group]; --casualtyBudget[team][group];
            }
        }
        for (unsigned id = 0; id < soldiers.size(); ++id) {
            auto& soldier = soldiers[id];
            if (soldier.life == SoldierLife::Alive) {
                soldier.heading = turnToward(soldier.heading, desiredHeadings[id],
                    movementProfile(simulation.formations[id / perTeam].groupUnit(soldiers[id].smallGroup)).turnRate * static_cast<float>(dt));
            }
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
