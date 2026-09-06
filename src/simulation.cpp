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
    formation.movementBlocked = false;
    formation.detouring = false;
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
    formation.movementBlocked = false;
    formation.detouring = false;
}
void BattleSimulation::reset(UnitType unit) {
    const auto profile = meleeProfile(unit);
    ++generation;
    running = false; time = 0; accumulator = 0; result = BattleResult::Ongoing;
    // 最小デモの配置と移動先。関ヶ原のデータではない。
    formations = {{{0, -22, 0, 0, DirectX::XM_PIDIV2, 1.8f, false},
                   {0, 22, 0, 0, -DirectX::XM_PIDIV2, 1.8f, false}}};
    formations[1].morale = 85; // 退却の差を観察する試作条件。陣営固有の補正ではない。
    for(auto& f:formations){f.unit=unit;f.speed=profile.moveSpeed;}
    for (auto& f : formations) for (auto& g : f.organization.smallGroups) { g.morale = f.morale; g.heading = f.heading; }
}
void BattleSimulation::update(float seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 3600) throw std::invalid_argument("Invalid simulation time step");
    if (!running) return;
    constexpr double fixedStep = 1.0 / 60.0;
    accumulator += seconds;
    while (accumulator + 1e-8 >= fixedStep) {
        step(static_cast<float>(fixedStep));
        updateRouts(static_cast<float>(fixedStep));
        updateSmallGroups(static_cast<float>(fixedStep));
        updateFaceDeployments(static_cast<float>(fixedStep));
        time += fixedStep;
        accumulator = std::max(0.0, accumulator - fixedStep);
    }
}
void BattleSimulation::updateSmallGroups(float seconds) {
    using Point = BattlePoint;
    std::array<std::array<Point, 25>, 2> positions{};
    for (unsigned team = 0; team < 2; ++team) {
        const auto& f = formations[team];
        for (unsigned id = 0; id < 25; ++id) {
            positions[team][id] = f.groupPosition(id);
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
            if (g.routed) continue;
            if (g.state == SmallGroupState::Engaged) g.fatigue = std::min(30.0f, g.fatigue + seconds);
            else if (g.route == SmallGroupRoute::None) g.fatigue = std::max(0.0f, g.fatigue - seconds * 0.5f);
            if (g.route != SmallGroupRoute::None && g.route != SmallGroupRoute::ReliefReserve && g.route != SmallGroupRoute::ReliefWithdraw &&
                (!contact || g.fatigue >= 8 || g.strength <= g.nominalStrength * 0.5f || g.morale <= 30 ||
                g.routeAlongX != alongX || g.routeForward * sign < 0)) g.route = SmallGroupRoute::Returning;
        }
        for (unsigned sideIndex = 0; sideIndex < 2; ++sideIndex) {
            auto& relief = f.organization.frontReliefs[sideIndex];
            if (relief.front >= 0 && (result != BattleResult::Ongoing || f.organization.smallGroups[relief.front].routed ||
                f.organization.smallGroups[relief.reserve].routed)) {
                // 敗走・戦闘終了で交代予約を解除する。残存小組は現在位置から通常の復帰を行う。
                for (int id : {relief.front, relief.reserve}) {
                    auto& g = f.organization.smallGroups[id];
                    g.route = g.routed ? SmallGroupRoute::None : SmallGroupRoute::Returning;
                }
                relief = {};
            }
            const unsigned lane = sideIndex * 4;
            if (relief.front < 0 && contact && f.maneuverEnabled &&
                std::abs(alongX ? enemy.z - f.z : enemy.x - f.x) <= 2) {
                const unsigned rank = sign > 0 ? 4 : 0;
                const unsigned frontSlot = alongX ? lane * 5 + rank : rank * 5 + lane;
                const unsigned rearSlot = alongX ? lane * 5 + (sign > 0 ? 3 : 1) : (sign > 0 ? 3 : 1) * 5 + lane;
                int front = -1, reserve = -1;
                bool occupied = false;
                for (unsigned id = 0; id < 25; ++id) {
                    const auto& g = f.organization.smallGroups[id];
                    if (g.slot == frontSlot) front = static_cast<int>(id);
                    if (g.slot == rearSlot) reserve = static_cast<int>(id);
                    if (g.slot != rearSlot && g.route != SmallGroupRoute::None &&
                        (g.routeAlongX != alongX || (alongX ? g.slot / 5 : g.slot % 5) == lane)) occupied = true;
                }
                if (!occupied && front >= 0 && reserve >= 0) {
                    auto& a = f.organization.smallGroups[front];
                    auto& b = f.organization.smallGroups[reserve];
                    const bool ready = b.route == SmallGroupRoute::None ||
                        ((b.route == SmallGroupRoute::Outward || b.route == SmallGroupRoute::Forward) &&
                            b.routeAlongX == alongX && b.routeForward * sign > 0);
                    if (!a.routed && !b.routed && a.approachX == 0 && a.approachZ == 0 && b.approachX == 0 && b.approachZ == 0 &&
                        a.route == SmallGroupRoute::None && a.offsetX == 0 && a.offsetZ == 0 && a.strength > 0 &&
                        (a.fatigue >= 8 || a.strength <= a.nominalStrength * 0.5f || a.morale <= 30) && ready &&
                        b.fatigue <= 2 && b.strength > b.nominalStrength * 0.5f && b.morale > 30) {
                        relief = {front, reserve, 0, alongX, sign * 5.2f};
                        a.route = SmallGroupRoute::ReliefWithdraw; b.route = SmallGroupRoute::ReliefReserve;
                        a.routeAlongX = b.routeAlongX = alongX;
                    }
                }
            }
            if (relief.front < 0) continue;
            auto& a = f.organization.smallGroups[relief.front];
            auto& b = f.organization.smallGroups[relief.reserve];
            const float side = sideIndex == 0 ? -6.1f : 6.1f;
            const float af = relief.alongX ? a.offsetX : a.offsetZ;
            const float bf = relief.alongX ? b.offsetX : b.offsetZ;
            const float bl = relief.alongX ? b.offsetZ : b.offsetX;
            if (relief.phase == 0 && bf == 0 && bl == side) relief.phase = 1;
            if (relief.phase == 1 && af == -relief.forward) relief.phase = 2;
            if (relief.phase == 2 && bf == relief.forward) relief.phase = 3;
            if (relief.phase == 3 && bl == 0) {
                std::swap(a.slot, b.slot);
                a.offsetX = a.offsetZ = b.offsetX = b.offsetZ = 0;
                a.route = b.route = SmallGroupRoute::None;
                relief = {};
            }
        }
        if (contact && f.maneuverEnabled && std::abs(alongX ? enemy.z - f.z : enemy.x - f.x) <= 2) {
            for (unsigned lane : {0u, 4u}) {
                bool occupied = false;
                for (unsigned id = 0; id < 25; ++id) {
                    const auto& g = f.organization.smallGroups[id];
                    if (g.route != SmallGroupRoute::None &&
                        (g.routeAlongX != alongX || (alongX ? g.slot / 5 : g.slot % 5) == lane)) occupied = true;
                }
                if (occupied) continue;
                // 復帰が完了するまで同じ通路へ次の小組を出さない。前寄りの休息済み予備を選ぶ。
                for (unsigned depth = 1; depth < 5; ++depth) {
                    const unsigned rank = sign > 0 ? 4 - depth : depth;
                    const unsigned slot = alongX ? lane * 5 + rank : rank * 5 + lane;
                    auto& g = *std::find_if(f.organization.smallGroups.begin(), f.organization.smallGroups.end(),
                        [slot](const SmallGroup& group) { return group.slot == slot; });
                    if (g.routed || g.approachX != 0 || g.approachZ != 0 || g.route != SmallGroupRoute::None || g.fatigue > 2 || g.strength <= g.nominalStrength * 0.5f || g.morale <= 30 || g.offsetX != 0 || g.offsetZ != 0 ||
                        g.state == SmallGroupState::Engaged) continue;
                    g.route = SmallGroupRoute::Outward; g.routeAlongX = alongX;
                    g.routeForward = sign * (6.8f + depth * 5.2f);
                    break;
                }
            }
        }
        for (unsigned id = 0; id < 25; ++id) {
            auto& g = f.organization.smallGroups[id];
            if (g.routed) {
                const float dx = g.fleeTargetX - g.fleeX, dz = g.fleeTargetZ - g.fleeZ;
                const float distance = std::hypot(dx, dz);
                const float travel = std::min(distance, f.speed * 1.5f * seconds);
                g.fleeBlocked = false;
                if (distance > 0.0001f && g.strength > 0) {
                    const Point from{g.fleeX, g.fleeZ};
                    bool moved = false;
                    // 退路側の半円を調べる。左右の優先を保持し、同じ障害物の前で往復しにくくする。
                    for (float angle : {0.0f, 0.52359878f, -0.52359878f, 1.04719755f, -1.04719755f, 1.57079633f, -1.57079633f}) {
                        const float turn = angle * g.fleeSide;
                        const float vx = (dx * std::cos(turn) - dz * std::sin(turn)) / distance;
                        const float vz = (dx * std::sin(turn) + dz * std::cos(turn)) / distance;
                        const Point next{from.x + vx * travel, from.z + vz * travel};
                        const float lookAhead = std::min(distance, std::max(travel, 1.0f));
                        const Point probe{from.x + vx * lookAhead, from.z + vz * lookAhead};
                        bool blocked = std::abs(probe.x) > 76 || std::abs(probe.z) > 76;
                        for (unsigned otherTeam = 0; otherTeam < 2 && !blocked; ++otherTeam) for (unsigned other = 0; other < 25; ++other) {
                            if ((otherTeam == team && other == id) || formations[otherTeam].organization.smallGroups[other].strength <= 0) continue;
                            const float clearance = 4.5f + formations[otherTeam].speed * 1.5f * seconds;
                            if (segmentDistance(from, probe, positions[otherTeam][other]) < clearance) { blocked = true; break; }
                        }
                        if (blocked) continue;
                        g.fleeX = next.x; g.fleeZ = next.z;
                        if (angle == 0 && travel == distance) { g.fleeX = g.fleeTargetX; g.fleeZ = g.fleeTargetZ; }
                        g.heading = std::atan2(vz, vx);
                        if (angle < 0) g.fleeSide = -g.fleeSide;
                        moved = true; break;
                    }
                    g.fleeBlocked = !moved;
                }
                g.offsetX = g.fleeX - f.x - (static_cast<float>(g.slot % 5) - 2) * 5.2f;
                g.offsetZ = g.fleeZ - f.z - (static_cast<float>(g.slot / 5) - 2) * 5.2f;
                g.targetOffsetX = g.offsetX; g.targetOffsetZ = g.offsetZ;
                g.state = g.strength > 0 && battleDistance({g.fleeX, g.fleeZ}, {g.fleeTargetX, g.fleeTargetZ}) > 0.0001f ?
                    SmallGroupState::Fleeing : SmallGroupState::Routed;
                continue;
            }
            g.targetOffsetX = g.offsetX; g.targetOffsetZ = g.offsetZ;
            g.state = f.defeated() ? SmallGroupState::Retreating :
                f.moving ? SmallGroupState::Advancing : SmallGroupState::Waiting;
            const auto p = positions[team][id];
            if (contact) for (unsigned other = 0; other < 25; ++other) {
                if (enemy.organization.smallGroups[other].routed || enemy.organization.smallGroups[other].strength <= 0) continue;
                const auto q = positions[1 - team][other];
                if (battleDistance(p, q) <= meleeProfile(formations[team].unit).groupRange) {
                    g.state = SmallGroupState::Engaged;
                    break;
                }
            }
            if (g.route == SmallGroupRoute::None) {
                Point target{p.x - g.approachX, p.z - g.approachZ};
                const bool approachingEnemy = std::hypot(f.targetX - enemy.x, f.targetZ - enemy.z) <=
                    std::hypot(f.x - enemy.x, f.z - enemy.z) + 0.001f;
                const bool seeking = result == BattleResult::Ongoing && !f.defeated() && approachingEnemy && !f.moving && g.attackTarget >= 0;
                if (seeking) {
                    const auto q = positions[1 - team][g.attackTarget];
                    const float distance = battleDistance(p, q);
                    target = distance > meleeProfile(formations[team].unit).groupRange ? Point{p.x + (q.x - p.x) / distance * (distance - meleeProfile(formations[team].unit).stopDistance),
                        p.z + (q.z - p.z) / distance * (distance - meleeProfile(formations[team].unit).stopDistance)} : p;
                }
                if (!f.maneuverEnabled && !f.defeated()) {
                    if (!g.canAttack && g.combatWait == CombatWait::OutOfRange) g.combatWait = CombatWait::Held;
                    continue;
                }
                const float clearance = 4.5f + 3 * std::max(f.speed, enemy.speed) * seconds;
                const auto clearPath = [&](Point from, Point to) {
                    if (std::abs(to.x) > 76 || std::abs(to.z) > 76) return false;
                    for (unsigned otherTeam = 0; otherTeam < 2; ++otherTeam) for (unsigned other = 0; other < 25; ++other) {
                        if ((otherTeam == team && other == id) || formations[otherTeam].organization.smallGroups[other].strength <= 0) continue;
                        if (segmentDistance(from, to, positions[otherTeam][other]) < clearance) return false;
                    }
                    return true;
                };
                if (!seeking || g.detourTarget != g.attackTarget || g.canAttack) g.detourTarget = -1;
                if (seeking && !g.canAttack) {
                    const auto enemyPoint = positions[1 - team][g.attackTarget];
                    const auto attackPoint = [&](Point from) {
                        const float length = battleDistance(from, enemyPoint);
                        return length > meleeProfile(formations[team].unit).stopDistance ? Point{enemyPoint.x - (enemyPoint.x - from.x) * meleeProfile(formations[team].unit).stopDistance / length,
                            enemyPoint.z - (enemyPoint.z - from.z) * meleeProfile(formations[team].unit).stopDistance / length} : from;
                    };
                    if (g.detourTarget >= 0 && battleDistance(p, {g.detourX, g.detourZ}) < 0.001f) g.detourTarget = -1;
                    if (g.detourTarget >= 0) {
                        const Point waypoint{g.detourX, g.detourZ};
                        if (!clearPath(p, waypoint) || !clearPath(waypoint, attackPoint(waypoint))) g.detourTarget = -1;
                    }
                    if (g.detourTarget < 0 && !clearPath(p, target)) {
                        const float length = battleDistance(p, enemyPoint);
                        float best = 1000;
                        // 左右の一つの中継点で通れる経路だけを採用する。密集した列は横断しない。
                        for (float side : {-1.0f, 1.0f}) for (float width : {6.1f, 10.4f, 15.6f}) {
                            if (length < 0.001f) continue;
                            const Point waypoint{p.x - (enemyPoint.z - p.z) / length * side * width,
                                p.z + (enemyPoint.x - p.x) / length * side * width};
                            const auto end = attackPoint(waypoint);
                            const float cost = width + battleDistance(waypoint, end);
                            if (cost < best && clearPath(p, waypoint) && clearPath(waypoint, end)) {
                                best = cost; g.detourX = waypoint.x; g.detourZ = waypoint.z; g.detourTarget = g.attackTarget;
                            }
                        }
                    }
                    if (g.detourTarget >= 0) { target = {g.detourX, g.detourZ}; g.combatWait = CombatWait::Detouring; }
                }
                const float distance = battleDistance(p, target);
                if (distance <= 0.0001f) continue;
                const float travel = std::min(distance, f.speed * seconds);
                const Point next{p.x + (target.x - p.x) / distance * travel, p.z + (target.z - p.z) / distance * travel};
                bool blocked = std::abs(next.x) > 76 || std::abs(next.z) > 76;
                for (unsigned otherTeam = 0; otherTeam < 2; ++otherTeam) for (unsigned other = 0; other < 25; ++other) {
                    if ((otherTeam == team && other == id) || formations[otherTeam].organization.smallGroups[other].strength <= 0) continue;
                    const auto q = positions[otherTeam][other];
                    if (segmentDistance(p, next, q) < clearance || (seeking && segmentDistance(p, target, q) < clearance)) blocked = true;
                }
                if (!blocked) {
                    g.approachX += next.x - p.x; g.approachZ += next.z - p.z;
                    if (travel == distance && !seeking) g.approachX = g.approachZ = 0;
                    g.state = SmallGroupState::Advancing;
                } else if (seeking) g.combatWait = CombatWait::PathBlocked;
                continue;
            }
            if (!f.maneuverEnabled && !f.defeated()) continue;
            const bool relieving = g.route == SmallGroupRoute::ReliefReserve || g.route == SmallGroupRoute::ReliefWithdraw;
            const bool returning = g.route == SmallGroupRoute::Returning || g.route == SmallGroupRoute::ReliefWithdraw;
            if (g.state == SmallGroupState::Engaged && !returning && !relieving) continue;
            const bool routeX = g.routeAlongX;
            const unsigned lane = routeX ? g.slot / 5 : g.slot % 5;
            const float side = lane == 0 ? -6.1f : 6.1f;
            const float lateral = routeX ? g.offsetZ : g.offsetX;
            const float forward = routeX ? g.offsetX : g.offsetZ;
            if (relieving) {
                const auto& relief = f.organization.frontReliefs[lane == 0 ? 0 : 1];
                float targetForward = forward, targetLateral = lateral;
                if (g.route == SmallGroupRoute::ReliefWithdraw) {
                    if (relief.phase >= 1) targetForward = -relief.forward;
                } else if (relief.phase == 0) {
                    // 既存の回り込みを転用した場合も、先に外側を後退して後列を空ける。
                    targetForward = 0;
                    if (forward == 0) targetLateral = side;
                } else if (relief.phase == 2) targetForward = relief.forward;
                else if (relief.phase == 3) targetLateral = 0;
                if (routeX) { g.targetOffsetX = targetForward; g.targetOffsetZ = targetLateral; }
                else { g.targetOffsetZ = targetForward; g.targetOffsetX = targetLateral; }
            } else if (returning) {
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
                if (returning && !relieving && g.offsetX == 0 && g.offsetZ == 0) g.route = SmallGroupRoute::None;
                continue;
            }
            const float travel = std::min(distance, f.speed * seconds);
            const Point next{p.x + dx / distance * travel, p.z + dz / distance * travel};
            bool blocked = std::abs(next.x) > 76 || std::abs(next.z) > 76;
            // 小組の幅と双方の一刻みの移動量を確保する。障害物経路探索は後続工程。
            const float clearance = 4.5f + 3 * std::max(f.speed, enemy.speed) * seconds;
            for (unsigned otherTeam = 0; otherTeam < 2; ++otherTeam)
                for (unsigned other = 0; other < 25; ++other) {
                    if (otherTeam == team && other == id) continue;
                    if (formations[otherTeam].organization.smallGroups[other].strength <= 0) continue;
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
    const auto beforeMove = formations;
    for (unsigned team = 0; team < formations.size(); ++team) {
        auto& formation = formations[team];
        const auto& original = beforeMove[team];
        formation.movementBlocked = false;
        if (formation.state == FormationState::Routed) continue;
        const bool retreating = formation.defeated();
        const BattlePoint start{original.x, original.z}, goal{formation.targetX, formation.targetZ};
        const auto clearRoute = [&](BattlePoint from, BattlePoint to) {
            if (std::abs(to.x) > 60 || std::abs(to.z) > 60) return false;
            for (unsigned id = 0; id < 25; ++id) {
                const auto& group = original.organization.smallGroups[id];
                if (group.routed || group.strength <= 0) continue;
                const auto p = original.groupPosition(id);
                const BattlePoint a{p.x + from.x - start.x, p.z + from.z - start.z};
                const BattlePoint b{p.x + to.x - start.x, p.z + to.z - start.z};
                for (unsigned otherTeam = 0; otherTeam < 2; ++otherTeam) for (unsigned other = 0; other < 25; ++other) {
                    const auto& obstacle = beforeMove[otherTeam].organization.smallGroups[other];
                    if (obstacle.strength <= 0 || (otherTeam == team && !obstacle.routed)) continue;
                    if (segmentDistance(a, b, beforeMove[otherTeam].groupPosition(other)) < 4.5f) return false;
                }
            }
            return true;
        };
        if (formation.detouring) {
            const BattlePoint waypoint{formation.waypointX, formation.waypointZ};
            const BattlePoint backtrack{formation.backtrackX, formation.backtrackZ};
            const BattlePoint exitPoint{formation.exitX, formation.exitZ};
            if (formation.detourBacking && battleDistance(start, backtrack) < 0.001f) formation.detourBacking = false;
            if (battleDistance(start, waypoint) < 0.001f) formation.detourExiting = true;
            const bool firstLegClear = formation.detourBacking ? clearRoute(start, backtrack) && clearRoute(backtrack, waypoint) : clearRoute(start, waypoint);
            if (battleDistance(start, exitPoint) < 0.001f ||
                !(formation.detourExiting ? clearRoute(start, exitPoint) : firstLegClear && clearRoute(waypoint, exitPoint)) || !clearRoute(exitPoint, goal))
                formation.detouring = false;
        }
        const float goalDistance = battleDistance(start, goal);
        if (!formation.detouring && original.movementBlocked && goalDistance > 0.001f && !clearRoute(start, goal)) {
            float best = 1000;
            for (float back : {0.0f, 10.0f, 20.0f}) for (float side : {-1.0f, 1.0f})
                for (float width : {10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f}) {
                const BattlePoint backtrack{start.x - (goal.x - start.x) / goalDistance * back,
                    start.z - (goal.z - start.z) / goalDistance * back};
                const BattlePoint waypoint{backtrack.x - (goal.z - start.z) / goalDistance * side * width,
                    backtrack.z + (goal.x - start.x) / goalDistance * side * width};
                const BattlePoint exitPoint{goal.x + waypoint.x - backtrack.x, goal.z + waypoint.z - backtrack.z};
                const float cost = back + 2 * width + battleDistance(waypoint, exitPoint);
                if (cost < best && clearRoute(start, backtrack) && clearRoute(backtrack, waypoint) &&
                    clearRoute(waypoint, exitPoint) && clearRoute(exitPoint, goal)) {
                    best = cost; formation.detouring = true;
                    formation.detourExiting = false; formation.exitX = exitPoint.x; formation.exitZ = exitPoint.z;
                    formation.detourBacking = back > 0;
                    formation.backtrackX = backtrack.x; formation.backtrackZ = backtrack.z;
                    formation.waypointX = waypoint.x; formation.waypointZ = waypoint.z;
                }
            }
        }
        const BattlePoint nextTarget = !formation.detouring ? goal : formation.detourBacking ?
            BattlePoint{formation.backtrackX, formation.backtrackZ} : formation.detourExiting ?
            BattlePoint{formation.exitX, formation.exitZ} : BattlePoint{formation.waypointX, formation.waypointZ};
        const float dx = nextTarget.x - formation.x, dz = nextTarget.z - formation.z;
        const float distance = std::hypot(dx, dz);
        if (distance < 0.0001f) {
            formation.moving = false;
            formation.state = retreating ? FormationState::Routed : FormationState::Idle;
            continue;
        }
        formation.heading = std::atan2(dz, dx);
        const float step = std::min(distance, formation.speed * (retreating ? 1.5f : 1.0f) * seconds);
        formation.x += dx / distance * step; formation.z += dz / distance * step;
        if (step == distance) { formation.x = nextTarget.x; formation.z = nextTarget.z; }
        formation.moving = formation.detouring || step < distance;
        formation.state = retreating ? (formation.moving ? FormationState::Retreating : FormationState::Routed) :
            (formation.moving ? FormationState::Marching : FormationState::Idle);
    }
    auto& red = formations[0]; auto& blue = formations[1];
    const float dx = blue.x - red.x, dz = blue.z - red.z;
    // 表示密度や向きに依存しない、半幅14の軸平行な隊列として扱う。
    const float overlapX = 28 - std::abs(dx), overlapZ = 28 - std::abs(dz);
    if (result == BattleResult::Ongoing && overlapX > 0 && overlapZ > 0) {
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
    // 命令移動と備同士の押し戻しを、同じ時点の小組位置で検証してから確定する。
    std::array<BattlePoint, 2> translation{};
    std::array<bool, 2> movementObstructed{};
    for (unsigned team = 0; team < 2; ++team)
        translation[team] = {formations[team].x - beforeMove[team].x, formations[team].z - beforeMove[team].z};
    for (unsigned team = 0; team < 2; ++team) {
        const auto delta = translation[team];
        if (delta.x == 0 && delta.z == 0) continue;
        for (unsigned id = 0; id < 25 && !movementObstructed[team]; ++id) {
            const auto& group = beforeMove[team].organization.smallGroups[id];
            if (group.routed || group.strength <= 0) continue;
            const auto p = beforeMove[team].groupPosition(id);
            for (unsigned otherTeam = 0; otherTeam < 2 && !movementObstructed[team]; ++otherTeam) for (unsigned other = 0; other < 25; ++other) {
                const auto& obstacle = beforeMove[otherTeam].organization.smallGroups[other];
                // 同じ備の非敗走小組は一緒に平行移動するため、相対位置が変わらない。
                if (obstacle.strength <= 0 || (otherTeam == team && !obstacle.routed)) continue;
                const auto q = beforeMove[otherTeam].groupPosition(other);
                const BattlePoint relative{p.x - q.x, p.z - q.z};
                const auto otherDelta = obstacle.routed ? BattlePoint{} : translation[otherTeam];
                const auto obstructed = [&](BattlePoint motion) {
                    const BattlePoint end{relative.x + motion.x, relative.z + motion.z};
                    const float initial = battleDistance(relative, {});
                    // 既存配置に重なりがある場合でも、離れる動きは許す。
                    if (initial < 4.5f) return relative.x * motion.x + relative.z * motion.z < -0.000001f;
                    return segmentDistance(relative, end, {}) < 4.5f;
                };
                // 相手が停止した場合と、同時に動いた場合の両方で安全な移動だけ通す。
                if (obstructed(delta) || obstructed({delta.x - otherDelta.x, delta.z - otherDelta.z})) {
                    movementObstructed[team] = true; break;
                }
            }
        }
    }
    for (unsigned team = 0; team < 2; ++team) if (movementObstructed[team]) {
        auto& f = formations[team];
        f.x = beforeMove[team].x; f.z = beforeMove[team].z;
        f.moving = false; f.movementBlocked = true;
        f.state = beforeMove[team].defeated() ? FormationState::Retreating : FormationState::Idle;
    }
    if (result != BattleResult::Ongoing) return;
    const bool formationsTouch = std::abs(blue.x - red.x) <= 28.001f && std::abs(blue.z - red.z) <= 28.001f;
    std::array<std::array<float, 25>, 2> damage{};
    std::array<std::array<BattlePoint, 25>, 2> points{};
    for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id)
        points[team][id] = formations[team].groupPosition(id);
    for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id) {
        auto& g = formations[team].organization.smallGroups[id];
        g.attackTarget = -1;
        g.activeFighters = 0;
        g.canAttack = false;
        g.combatWait = CombatWait::NoTarget;
        if (g.routed || g.strength <= 0) continue;
        float nearest = 16;
        bool targetBlocked = true;
        for (unsigned other = 0; other < 25; ++other) {
            const auto& enemy = formations[1 - team].organization.smallGroups[other];
            if (enemy.routed || enemy.strength <= 0) continue;
            const float distance = battleDistance(points[team][id], points[1 - team][other]);
            if (distance >= 16 || distance < 0.001f) continue;
            const auto p = points[team][id], q = points[1 - team][other];
            const bool inRange = distance <= meleeProfile(formations[team].unit).groupRange;
            const BattlePoint end = inRange ? q : BattlePoint{q.x - (q.x - p.x) * meleeProfile(formations[team].unit).stopDistance / distance,
                q.z - (q.z - p.z) * meleeProfile(formations[team].unit).stopDistance / distance};
            const float clearance = inRange ? 2.25f : 4.5f + 3 * std::max(formations[team].speed, formations[1 - team].speed) * seconds;
            bool blocked = false;
            for (unsigned blockerTeam = 0; blockerTeam < 2 && !blocked; ++blockerTeam)
                for (unsigned blocker = 0; blocker < 25; ++blocker) {
                    if ((blockerTeam == team && blocker == id) || (blockerTeam != team && blocker == other) ||
                        formations[blockerTeam].organization.smallGroups[blocker].strength <= 0 ||
                        (inRange && formations[blockerTeam].organization.smallGroups[blocker].routed)) continue;
                    if (segmentDistance(p, end, points[blockerTeam][blocker]) < clearance) { blocked = true; break; }
                }
            // 最も近い相手の陰で停止し続けず、攻撃線・接近経路の空いた候補を優先する。
            if (g.attackTarget < 0 || (!blocked && targetBlocked) || (blocked == targetBlocked && distance < nearest)) {
                nearest = distance; g.attackTarget = static_cast<int>(other); targetBlocked = blocked;
            }
        }
        const auto p = points[team][id];
        const float targetHeading = g.attackTarget >= 0 ? std::atan2(points[1 - team][g.attackTarget].z - p.z,
            points[1 - team][g.attackTarget].x - p.x) : formations[team].heading;
        g.heading = turnToward(g.heading, targetHeading, meleeProfile(formations[team].unit).turnRate * seconds);
    }
    bool localContact = false;
    std::array<std::array<ContactBody, 25>, 2> contactBodies{};
    for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id) {
        const auto& g = formations[team].organization.smallGroups[id];
        contactBodies[team][id] = {points[team][id], g.heading, g.strength > 0, g.strength > 0 && !g.routed};
    }
    for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id) {
        const auto& f = formations[team];
        auto& g = formations[team].organization.smallGroups[id];
        if (g.attackTarget < 0) continue;
        const auto p = points[team][id], q = points[1 - team][g.attackTarget];
        const float distance = battleDistance(p, q);
        g.combatWait = CombatWait::OutOfRange;
        if (distance > meleeProfile(formations[team].unit).groupRange || distance < 0.001f) continue;
        localContact = true;
        if (g.route == SmallGroupRoute::Returning || g.route == SmallGroupRoute::ReliefReserve ||
            g.route == SmallGroupRoute::ReliefWithdraw) { g.combatWait = CombatWait::Returning; continue; }
        const float facing = (std::cos(g.heading) * (q.x - p.x) + std::sin(g.heading) * (q.z - p.z)) / distance;
        if (facing < 0.5f) { g.combatWait = CombatWait::Turning; continue; }
        bool blocked = false;
        for (unsigned otherTeam = 0; otherTeam < 2; ++otherTeam) for (unsigned other = 0; other < 25; ++other) {
            if ((otherTeam == team && other == id) || (otherTeam != team && other == static_cast<unsigned>(g.attackTarget)) ||
                formations[otherTeam].organization.smallGroups[other].routed) continue;
            if (segmentDistance(p, q, points[otherTeam][other]) < 2.25f) blocked = true;
        }
        if (blocked) { g.combatWait = CombatWait::Obstructed; continue; }
        g.canAttack = true;
        g.combatWait = CombatWait::None;
        const auto& victim = formations[1 - team].organization.smallGroups[g.attackTarget];
        const float defense = (std::cos(victim.heading) * (p.x - q.x) + std::sin(victim.heading) * (p.z - q.z)) / distance;
        const float directionBonus = defense < -0.5f ? 1.5f : defense < 0.5f ? 1.25f : 1.0f;
        const auto participation = participatingContactFronts(measureContactFronts(contactBodies, team, id), g.faceDeployment, g.strength);
        for (const auto& face : participation.faces) g.activeFighters += face.enemyFighters[g.attackTarget];
        // 幅4.5・一人幅0.9の正面一列（5人）を基準とする。予備兵は攻撃力へ加算しない。
        damage[1 - team][g.attackTarget] += meleeProfile(f.unit).damagePerSecond * (g.activeFighters / 5) * (0.5f + f.cohesion / 200) *
            (0.5f + g.morale / 200) * directionBonus * seconds;
    }
    for (unsigned i = 0; i < formations.size(); ++i) {
        auto& f = formations[i];
        float actualLoss = 0;
        for (unsigned id = 0; id < 25; ++id) {
            auto& g = f.organization.smallGroups[id];
            const float loss = std::min(g.strength, damage[i][id]);
            g.strength = std::max(0.0f, g.strength - loss);
            if (loss > 0) g.morale = std::max(0.0f, g.morale - loss * 2 - seconds * 0.5f);
            actualLoss += loss;
        }
        if (formationsTouch || localContact) { f.moving = false; f.state = FormationState::Engaged; }
        f.strength = std::max(0.0f, f.strength - actualLoss);
        if (actualLoss > 0) f.cohesion = std::max(0.0f, f.cohesion - 1.5f * seconds - actualLoss * 0.08f);
    }
}
void BattleSimulation::updateFaceDeployments(float seconds) {
    std::array<std::array<ContactBody, 25>, 2> bodies{};
    for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id) {
        const auto& g = formations[team].organization.smallGroups[id];
        bodies[team][id] = {formations[team].groupPosition(id), g.heading, g.strength > 0, g.strength > 0 && !g.routed};
    }
    for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id) {
        auto& g = formations[team].organization.smallGroups[id];
        if (g.routed || g.strength <= 0 || result != BattleResult::Ongoing) { g.faceDeployment = {}; g.activeFighters = 0; continue; }
        const bool returning = g.route == SmallGroupRoute::Returning || g.route == SmallGroupRoute::ReliefReserve || g.route == SmallGroupRoute::ReliefWithdraw;
        const auto fronts = returning ? ContactFronts{} : measureContactFronts(bodies, team, id);
        advanceFaceDeployment(g.faceDeployment, allocateContactFronts(fronts, g.strength), g.strength, seconds);
    }
}
ContactFronts BattleSimulation::contactFronts(unsigned team, unsigned group) const {
    (void)formations.at(team).organization.smallGroups.at(group);
    if (result != BattleResult::Ongoing) return {};
    std::array<std::array<ContactBody, 25>, 2> bodies{};
    for (unsigned t = 0; t < 2; ++t) for (unsigned id = 0; id < 25; ++id) {
        const auto& g = formations[t].organization.smallGroups[id];
        bodies[t][id] = {formations[t].groupPosition(id), g.heading, g.strength > 0, g.strength > 0 && !g.routed};
    }
    return measureContactFronts(bodies, team, group);
}
bool BattleSimulation::routInfluences(unsigned team, unsigned source, unsigned target) const {
    const auto& f = formations.at(team);
    const auto& a = f.organization.smallGroups.at(source);
    const auto& b = f.organization.smallGroups.at(target);
    return result == BattleResult::Ongoing && a.routed && a.routShock > 0 && !b.routed &&
        battleDistance(f.groupPosition(source), f.groupPosition(target)) <= routRadius;
}
unsigned BattleSimulation::nearbyRouts(unsigned team, unsigned group) const {
    const auto& groups = formations.at(team).organization.smallGroups;
    const auto& target = groups.at(group);
    if (result != BattleResult::Ongoing || target.routed) return 0;
    unsigned count = 0;
    for (unsigned id = 0; id < groups.size(); ++id) {
        if (routInfluences(team, id, group)) ++count;
    }
    return count;
}
void BattleSimulation::updateRouts(float seconds) {
    if (result != BattleResult::Ongoing) return;
    std::array<bool, 2> broken{};
    for (unsigned team = 0; team < 2; ++team) {
        auto& f = formations[team];
        const auto& enemy = formations[1 - team];
        // 刻みの開始時の敗走者と実位置だけを参照し、同じ刻みで連鎖を再帰させない。
        std::array<unsigned, 25> nearbyCounts{};
        for (unsigned id = 0; id < 25; ++id) nearbyCounts[id] = nearbyRouts(team, id);
        for (auto& g : f.organization.smallGroups) {
            g.routShock = std::max(0.0f, g.routShock - seconds);
        }
        for (unsigned id = 0; id < 25; ++id) {
            auto& g = f.organization.smallGroups[id];
            if (g.routed) continue;
            g.morale = std::max(0.0f, g.morale - nearbyCounts[id] * 6.0f * seconds);
            if (g.morale > 20 && g.strength > g.nominalStrength * 0.25f) continue;
            const auto position = f.groupPosition(id);
            g.routed = true; g.routShock = 8; g.route = SmallGroupRoute::None; g.attackTarget = -1;
            g.detourTarget = -1;
            g.approachX = g.approachZ = 0;
            g.fleeX = position.x; g.fleeZ = position.z;
            float dx = f.x - enemy.x, dz = f.z - enemy.z;
            const float distance = std::hypot(dx, dz);
            if (distance > 0.001f) { dx /= distance; dz /= distance; }
            else { dx = 0; dz = team == 0 ? -1.0f : 1.0f; }
            g.fleeTargetX = std::clamp(g.fleeX + dx * 120, -74.0f, 74.0f);
            g.fleeTargetZ = std::clamp(g.fleeZ + dz * 120, -74.0f, 74.0f);
        }
        float morale = 0;
        for (const auto& g : f.organization.smallGroups) morale += g.routed ? 0 : g.morale;
        f.morale = morale / 25;
        broken[team] = f.routedGroups() >= 13;
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
        f.detouring = false;
        f.heading = std::atan2(awayZ, awayX); f.state = FormationState::Retreating;
    }
}
