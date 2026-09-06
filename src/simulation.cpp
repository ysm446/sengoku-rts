#include "simulation.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "formation_drill.h"

namespace {
float groupMovementLimit(const std::array<Formation, 2>& formations) {
    float speed = std::max(formations[0].speed, formations[1].speed);
    for (const auto& f : formations) for (unsigned id = 0; id < 25; ++id)
        if (f.groupUnit(id) == UnitType::Cavalry) speed = std::max(speed, movementProfile(UnitType::Cavalry).speed);
    return speed;
}
}

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
    for (auto& g : formation.organization.smallGroups) g.approachSpeed = g.chargeDistance = g.chargeWindow = 0;
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
    for (auto& g : formation.organization.smallGroups) {
        g.approachSpeed = g.chargeDistance = g.chargeWindow = 0;
    }
    updateDecisions();
}
void BattleSimulation::reset(UnitType unit, bool mixedBattle) {
    const auto profile = meleeProfile(unit);
    mixed = mixedBattle;
    arrows.clear(); volleysFired = volleysHit = 0;
    impacts = {}; impactSerial = 0; nextImpactTime = {};
    ++generation;
    running = false; time = 0; accumulator = 0; result = BattleResult::Ongoing;
    // 最小デモの配置と移動先。関ヶ原のデータではない。
    formations = {{{0, -22, 0, 0, DirectX::XM_PIDIV2, 1.8f, false},
                   {0, 22, 0, 0, -DirectX::XM_PIDIV2, 1.8f, false}}};
    formations[1].morale = 85; // 退却の差を観察する試作条件。陣営固有の補正ではない。
    if (mixed) {
        formations[0].x = formations[0].targetX = -6;
        formations[1].x = formations[1].targetX = 6;
    }
    for(auto& f:formations){f.unit=unit;f.speed=profile.moveSpeed;}
    for (auto& f : formations) for (unsigned id = 0; id < 25; ++id) {
        auto& g = f.organization.smallGroups[id];
        g.morale = f.morale; g.heading = f.heading;
        // 赤は槍中央・刀両翼。青は槍前衛・刀予備。兵種は移動・交代後も保つ。
        if (mixed) {
            const bool red = &f == &formations[0];
            const unsigned row = id / 5, column = id % 5;
            const bool rear = row == (red ? 0u : 4u);
            const bool sword = red ? column == 0 || column == 4 : row == 3 || (row == 2 && column > 0 && column < 4);
            const bool cavalry = (column == 0 || column == 4) && (red ? row >= 3 : row <= 1);
            g.unit = cavalry ? UnitType::Cavalry : rear ? UnitType::Archer : sword ? UnitType::Samurai : UnitType::Spearman;
        }
    }
}
void BattleSimulation::update(float seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 3600) throw std::invalid_argument("Invalid simulation time step");
    if (!running) return;
    constexpr double fixedStep = 1.0 / 60.0;
    accumulator += seconds;
    while (accumulator + 1e-8 >= fixedStep) {
        std::array<std::array<BattlePoint, 25>, 2> previous{};
        for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id)
            previous[team][id] = formations[team].groupPosition(id);
        step(static_cast<float>(fixedStep));
        updateRouts(static_cast<float>(fixedStep));
        updateSmallGroups(static_cast<float>(fixedStep));
        // 助走は補正前の速度ではなく、衝突検査を通った実移動から数える。
        for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id) {
            auto& f = formations[team]; auto& g = f.organization.smallGroups[id];
            if (f.groupUnit(id) != UnitType::Cavalry) continue;
            g.chargeCooldown = std::max(0.0f, g.chargeCooldown - static_cast<float>(fixedStep));
            g.chargeWindow = std::max(0.0f, g.chargeWindow - static_cast<float>(fixedStep));
            if (result != BattleResult::Ongoing || !f.maneuverEnabled || f.defeated() || g.routed || g.resting ||
                g.route != SmallGroupRoute::None || g.morale <= 40 || g.attackTarget < 0) {
                g.chargeDistance = g.chargeWindow = g.approachSpeed = 0; continue;
            }
            const auto p = f.groupPosition(id), old = previous[team][id];
            const auto q = formations[1 - team].groupPosition(g.attackTarget);
            const float dx = p.x - old.x, dz = p.z - old.z, travel = std::hypot(dx, dz);
            const float distance = battleDistance(p, q);
            const float alignment = distance > .001f ? ((q.x-p.x)*std::cos(g.heading)+(q.z-p.z)*std::sin(g.heading))/distance : 0;
            if (travel / fixedStep >= 2.5f && dx*std::cos(g.heading)+dz*std::sin(g.heading) >= travel*.95f && alignment >= .95f)
                g.chargeDistance += travel;
            else g.chargeDistance = 0;
            if (g.chargeCooldown == 0 && g.chargeDistance >= 4 && distance <= meleeProfile(UnitType::Cavalry).groupRange) {
                g.chargeWindow = .75f; g.chargeCooldown = 8; g.chargeDistance = 0;
            }
        }
        updateFaceDeployments(static_cast<float>(fixedStep));
        updateDecisions();
        time += fixedStep;
        accumulator = std::max(0.0, accumulator - fixedStep);
    }
}
void BattleSimulation::updateSmallGroups(float seconds) {
    using Point = BattlePoint;
    const float movementLimit = groupMovementLimit(formations);
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
            if (g.route != SmallGroupRoute::None && g.route != SmallGroupRoute::ReliefReserve && g.route != SmallGroupRoute::ReliefWithdraw && g.route != SmallGroupRoute::ReliefCorridor &&
                (!contact || g.fatigue >= 8 || g.strength <= g.nominalStrength * 0.5f || g.morale <= 30 ||
                g.routeAlongX != alongX || g.routeForward * sign < 0)) g.route = SmallGroupRoute::Returning;
        }
        for (unsigned sideIndex = 0; sideIndex < f.organization.frontReliefs.size(); ++sideIndex) {
            auto& relief = f.organization.frontReliefs[sideIndex];
            if (relief.front >= 0 && (result != BattleResult::Ongoing ||
                (relief.corridorGroups && (!contact || relief.alongX != alongX || relief.forward * sign < 0)) || f.organization.smallGroups[relief.front].routed ||
                f.organization.smallGroups[relief.reserve].routed)) {
                // 敗走・戦闘終了で交代予約を解除する。残存小組は現在位置から通常の復帰を行う。
                for (int id : {relief.front, relief.reserve}) {
                    auto& g = f.organization.smallGroups[id];
                    g.route = g.routed ? SmallGroupRoute::None : SmallGroupRoute::Returning;
                }
                for (unsigned id = 0; id < 25; ++id) if (relief.corridorGroups & (1u << id)) {
                    auto& g = f.organization.smallGroups[id];
                    g.route = g.routed ? SmallGroupRoute::None : SmallGroupRoute::Returning;
                }
                relief = {};
            }
            const unsigned lane = sideIndex < 2 ? sideIndex * 4 : sideIndex - 1;
            if (relief.front < 0 && contact && f.maneuverEnabled) {
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
                    if (!a.routed && !a.resting && !b.routed && a.approachX == 0 && a.approachZ == 0 && b.approachX == 0 && b.approachZ == 0 &&
                        a.route == SmallGroupRoute::None && a.offsetX == 0 && a.offsetZ == 0 && a.strength > 0 &&
                        (a.fatigue >= 8 || a.strength <= a.nominalStrength * 0.75f || a.morale <= 50) && ready &&
                        !b.resting && b.fatigue <= 2 && b.strength > b.nominalStrength * 0.5f && b.morale > 40 &&
                        f.groupUnit(reserve) != UnitType::Archer) {
                        float lateral = sideIndex == 0 ? -6.1f : 6.1f;
                        unsigned corridorGroups = 0;
                        float corridorShift = 0;
                        if (sideIndex >= 2) {
                            // 中央は隣列の空いた空間を使う。同時に複数の中央経路を予約しない。
                            bool centralBusy = false;
                            for (unsigned index = 2; index < f.organization.frontReliefs.size(); ++index)
                                centralBusy |= f.organization.frontReliefs[index].front >= 0;
                            if (centralBusy || b.offsetX != 0 || b.offsetZ != 0) continue;
                            const float clearance = 4.5f + 3 * movementLimit * seconds;
                            const auto clearSegment = [&](Point from, Point to, unsigned ignored = 0) {
                                if (std::abs(to.x) > 76 || std::abs(to.z) > 76) return false;
                                for (unsigned otherTeam = 0; otherTeam < 2; ++otherTeam) for (unsigned other = 0; other < 25; ++other) {
                                    if (otherTeam == team && (other == static_cast<unsigned>(front) || other == static_cast<unsigned>(reserve))) continue;
                                    if (otherTeam == team && (ignored & (1u << other))) continue;
                                    const auto& obstacle = formations[otherTeam].organization.smallGroups[other];
                                    if (obstacle.strength <= 0) continue;
                                    const auto q = positions[otherTeam][other];
                                    if (q.x > std::min(from.x, to.x) - clearance && q.x < std::max(from.x, to.x) + clearance &&
                                        q.z > std::min(from.z, to.z) - clearance && q.z < std::max(from.z, to.z) + clearance) {
                                        return false;
                                    }
                                }
                                return true;
                            };
                            lateral = 0;
                            const float preferred = lane <= 2 ? -5.2f : 5.2f;
                            for (float candidate : {preferred, -preferred}) {
                                const auto rear = positions[team][reserve], head = positions[team][front];
                                const Point rearSide{rear.x + (alongX ? 0 : candidate), rear.z + (alongX ? candidate : 0)};
                                const Point headSide{head.x + (alongX ? 0 : candidate), head.z + (alongX ? candidate : 0)};
                                if (clearSegment(rear, rearSide) && clearSegment(head, rear) &&
                                    clearSegment(rearSide, headSide) && clearSegment(headSide, head)) { lateral = candidate; break; }
                            }
                            if (lateral == 0) for (float candidate : {preferred, -preferred}) {
                                unsigned supporters = 0;
                                bool readyToShift = true;
                                for (unsigned id = 0; id < 25; ++id) {
                                    const auto& g = f.organization.smallGroups[id];
                                    const unsigned column = alongX ? g.slot / 5 : g.slot % 5;
                                    if (candidate < 0 ? column >= lane : column <= lane) continue;
                                    if (g.strength <= 0) continue;
                                    if (g.routed || g.resting || g.route != SmallGroupRoute::None || g.offsetX != 0 || g.offsetZ != 0 ||
                                        g.approachX != 0 || g.approachZ != 0) { readyToShift = false; break; }
                                    supporters |= 1u << id;
                                }
                                if (!readyToShift || supporters == 0) continue;
                                const float shift = candidate < 0 ? -6.1f : 6.1f;
                                for (unsigned id = 0; id < 25 && readyToShift; ++id) if (supporters & (1u << id)) {
                                    const auto from = positions[team][id];
                                    const Point to{from.x + (alongX ? 0 : shift), from.z + (alongX ? shift : 0)};
                                    readyToShift = clearSegment(from, to, supporters);
                                }
                                const auto rear = positions[team][reserve], head = positions[team][front];
                                const Point rearSide{rear.x + (alongX ? 0 : candidate), rear.z + (alongX ? candidate : 0)};
                                const Point headSide{head.x + (alongX ? 0 : candidate), head.z + (alongX ? candidate : 0)};
                                if (readyToShift && clearSegment(rear, rearSide, supporters) && clearSegment(head, rear, supporters) &&
                                    clearSegment(rearSide, headSide, supporters) && clearSegment(headSide, head, supporters)) {
                                    lateral = candidate; corridorGroups = supporters; corridorShift = shift; break;
                                }
                            }
                            if (lateral == 0) continue;
                        }
                        relief = {front, reserve, 0, alongX, sign * 5.2f, lateral};
                        relief.corridorGroups = corridorGroups; relief.corridorShift = corridorShift;
                        if (corridorGroups) {
                            relief.phase = 4;
                            for (unsigned id = 0; id < 25; ++id) if (corridorGroups & (1u << id)) {
                                auto& g = f.organization.smallGroups[id];
                                g.route = SmallGroupRoute::ReliefCorridor; g.routeAlongX = alongX;
                            }
                        }
                        a.route = SmallGroupRoute::ReliefWithdraw; b.route = SmallGroupRoute::ReliefReserve;
                        a.routeAlongX = b.routeAlongX = alongX;
                    }
                }
            }
            if (relief.front < 0) continue;
            if (relief.phase >= 4) {
                bool positioned = true;
                for (unsigned id = 0; id < 25; ++id) if (relief.corridorGroups & (1u << id)) {
                    const auto& g = f.organization.smallGroups[id];
                    if (g.routed || g.strength <= 0) continue;
                    positioned &= (relief.alongX ? g.offsetZ : g.offsetX) == (relief.phase == 4 ? relief.corridorShift : 0);
                }
                if (positioned && relief.phase == 4) relief.phase = 0;
                else if (positioned && relief.phase == 5) {
                    for (unsigned id = 0; id < 25; ++id) if (relief.corridorGroups & (1u << id)) {
                        auto& g = f.organization.smallGroups[id];
                        if (!g.routed) g.route = SmallGroupRoute::None;
                    }
                    relief = {};
                }
                continue;
            }
            auto& a = f.organization.smallGroups[relief.front];
            auto& b = f.organization.smallGroups[relief.reserve];
            const float side = relief.lateral;
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
                a.resting = true;
                if (relief.corridorGroups) relief.phase = 5;
                else relief = {};
            }
        }
        if (contact && f.maneuverEnabled) {
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
                    if (g.routed || g.resting || g.awareness.cautious || f.groupUnit(static_cast<unsigned>(&g - f.organization.smallGroups.data())) == UnitType::Archer || g.approachX != 0 || g.approachZ != 0 || g.route != SmallGroupRoute::None || g.fatigue > 2 || g.strength <= g.nominalStrength * 0.5f || g.morale <= 30 || g.offsetX != 0 || g.offsetZ != 0 ||
                        g.state == SmallGroupState::Engaged) continue;
                    g.route = SmallGroupRoute::Outward; g.routeAlongX = alongX;
                    g.routeForward = sign * (6.8f + depth * 5.2f);
                    const float side = lane == 0 ? -1.0f : 1.0f;
                    const float lateralGap = alongX ? enemy.z-f.z : enemy.x-f.x;
                    // 敵の同じ側の端からも6.1離れる。左右にずれた布陣でも敵列の中へ進まない。
                    g.routeLateral = side * std::max(6.1f, side*lateralGap + 6.1f);
                    break;
                }
            }
        }
        const auto reservedForCentralRelief = [&](unsigned id, Point point) {
            for (unsigned index = 2; index < f.organization.frontReliefs.size(); ++index) {
                const auto& r = f.organization.frontReliefs[index];
                if (r.front < 0 || r.front == static_cast<int>(id) || r.reserve == static_cast<int>(id) || (r.corridorGroups & (1u << id))) continue;
                const auto base = [&](int group) {
                    const auto s = f.organization.smallGroups[group].slot;
                    return Point{f.x + (static_cast<float>(s % 5) - 2) * 5.2f,
                        f.z + (static_cast<float>(s / 5) - 2) * 5.2f};
                };
                const auto a = base(r.front), b = base(r.reserve);
                const float dx = r.alongX ? 0 : r.lateral, dz = r.alongX ? r.lateral : 0;
                const float margin = 4.5f + 3 * movementLimit * seconds;
                if (point.x > std::min(a.x, b.x) + std::min(0.0f, dx) - margin &&
                    point.x < std::max(a.x, b.x) + std::max(0.0f, dx) + margin &&
                    point.z > std::min(a.z, b.z) + std::min(0.0f, dz) - margin &&
                    point.z < std::max(a.z, b.z) + std::max(0.0f, dz) + margin) return true;
            }
            return false;
        };
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
            if (g.resting) {
                g.state = SmallGroupState::Waiting;
                continue;
            }
            g.state = f.defeated() ? SmallGroupState::Retreating :
                f.moving ? SmallGroupState::Advancing : SmallGroupState::Waiting;
            const auto p = positions[team][id];
            if (contact) for (unsigned other = 0; other < 25; ++other) {
                if (enemy.organization.smallGroups[other].routed || enemy.organization.smallGroups[other].strength <= 0) continue;
                const auto q = positions[1 - team][other];
                if (battleDistance(p, q) <= combatProfile(formations[team].groupUnit(id)).groupRange) {
                    g.state = SmallGroupState::Engaged;
                    break;
                }
            }
            if (g.route == SmallGroupRoute::None) {
                // 停止命令と既存の交代経路を優先し、不利な予備の独断接近だけを控える。
                if (g.awareness.cautious && !f.moving && !g.canAttack && !f.defeated() && g.attackTarget >= 0 &&
                    battleDistance(p,positions[1-team][g.attackTarget]) > combatProfile(f.groupUnit(id)).groupRange) {
                    g.approachSpeed = 0; g.detourTarget = -1; continue;
                }
                Point target{p.x - g.approachX, p.z - g.approachZ};
                const bool approachingEnemy = std::hypot(f.targetX - enemy.x, f.targetZ - enemy.z) <=
                    std::hypot(f.x - enemy.x, f.z - enemy.z) + 0.001f;
                const bool cavalry = f.groupUnit(id) == UnitType::Cavalry;
                const bool withinPursuit = g.attackTarget >= 0 && (cavalry || f.groupUnit(id) == UnitType::Archer ||
                    battleDistance(p,positions[1-team][g.attackTarget]) < 16);
                const bool seeking = result == BattleResult::Ongoing && !f.defeated() && approachingEnemy && (!f.moving || cavalry) && withinPursuit;
                if (seeking) {
                    const auto q = positions[1 - team][g.attackTarget];
                    const float distance = battleDistance(p, q);
                    target = distance > combatProfile(formations[team].groupUnit(id)).groupRange ? Point{p.x + (q.x - p.x) / distance * (distance - combatProfile(formations[team].groupUnit(id)).stopDistance),
                        p.z + (q.z - p.z) / distance * (distance - combatProfile(formations[team].groupUnit(id)).stopDistance)} : p;
                }
                if (!f.maneuverEnabled && !f.defeated()) {
                    if (!g.canAttack && g.combatWait == CombatWait::OutOfRange) g.combatWait = CombatWait::Held;
                    continue;
                }
                const float clearance = 4.5f + 3 * movementLimit * seconds;
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
                        return length > combatProfile(formations[team].groupUnit(id)).stopDistance ? Point{enemyPoint.x - (enemyPoint.x - from.x) * combatProfile(formations[team].groupUnit(id)).stopDistance / length,
                            enemyPoint.z - (enemyPoint.z - from.z) * combatProfile(formations[team].groupUnit(id)).stopDistance / length} : from;
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
                if (distance <= 0.0001f) { g.approachSpeed = 0; continue; }
                float speed = f.speed;
                if (cavalry && seeking) {
                    const auto movement = movementProfile(UnitType::Cavalry);
                    const float alignment = ((target.x-p.x)*std::cos(g.heading)+(target.z-p.z)*std::sin(g.heading))/distance;
                    const float desired = alignment >= .95f ? std::max(0.0f, movement.speed - (f.moving ? f.speed : 0)) : 0;
                    g.approachSpeed += std::clamp(desired-g.approachSpeed, -movement.acceleration*2*seconds, movement.acceleration*seconds);
                    speed = alignment >= .95f ? g.approachSpeed : 0;
                }
                const float travel = std::min(distance, speed * seconds);
                const Point next{p.x + (target.x - p.x) / distance * travel, p.z + (target.z - p.z) / distance * travel};
                bool blocked = std::abs(next.x) > 76 || std::abs(next.z) > 76 || reservedForCentralRelief(id, next);
                for (unsigned otherTeam = 0; otherTeam < 2; ++otherTeam) for (unsigned other = 0; other < 25; ++other) {
                    if ((otherTeam == team && other == id) || formations[otherTeam].organization.smallGroups[other].strength <= 0) continue;
                    const auto q = positions[otherTeam][other];
                    if (segmentDistance(p, next, q) < clearance || (seeking && segmentDistance(p, target, q) < clearance)) blocked = true;
                }
                if (!blocked) {
                    g.approachX += next.x - p.x; g.approachZ += next.z - p.z;
                    if (travel == distance && !seeking) g.approachX = g.approachZ = 0;
                    g.state = SmallGroupState::Advancing;
                } else if (seeking) { g.combatWait = CombatWait::PathBlocked; g.approachSpeed = 0; }
                continue;
            }
            if (!f.maneuverEnabled && !f.defeated()) continue;
            const bool relieving = g.route == SmallGroupRoute::ReliefReserve || g.route == SmallGroupRoute::ReliefWithdraw || g.route == SmallGroupRoute::ReliefCorridor;
            const bool returning = g.route == SmallGroupRoute::Returning || g.route == SmallGroupRoute::ReliefWithdraw;
            if (g.state == SmallGroupState::Engaged && !returning && !relieving) continue;
            const bool routeX = g.routeAlongX;
            const unsigned lane = routeX ? g.slot / 5 : g.slot % 5;
            const float side = g.routeLateral != 0 ? g.routeLateral : lane == 0 ? -6.1f : 6.1f;
            const float lateral = routeX ? g.offsetZ : g.offsetX;
            const float forward = routeX ? g.offsetX : g.offsetZ;
            if (relieving) {
                const auto reservation = std::find_if(f.organization.frontReliefs.begin(), f.organization.frontReliefs.end(),
                    [id](const FrontRelief& value) { return value.front == static_cast<int>(id) || value.reserve == static_cast<int>(id) || (value.corridorGroups & (1u << id)); });
                if (reservation == f.organization.frontReliefs.end()) { g.route = SmallGroupRoute::Returning; continue; }
                const auto& relief = *reservation;
                float targetForward = forward, targetLateral = lateral;
                if (g.route == SmallGroupRoute::ReliefCorridor) {
                    const float desired = relief.phase == 5 ? 0 : relief.corridorShift;
                    bool precedingReady = true;
                    for (unsigned other = 0; other < 25; ++other) if (relief.corridorGroups & (1u << other)) {
                        const auto& h = f.organization.smallGroups[other];
                        if (h.routed || h.strength <= 0) continue;
                        const unsigned column = routeX ? h.slot / 5 : h.slot % 5;
                        const bool outside = relief.corridorShift < 0 ? column < lane : column > lane;
                        const bool inside = relief.corridorShift < 0 ? column > lane : column < lane;
                        if ((relief.phase == 5 ? inside : outside) && (routeX ? h.offsetZ : h.offsetX) != desired) precedingReady = false;
                    }
                    if (precedingReady) targetLateral = desired;
                } else if (g.route == SmallGroupRoute::ReliefWithdraw) {
                    if (relief.phase >= 1 && relief.phase <= 3) targetForward = -relief.forward;
                } else if (relief.phase == 0) {
                    // 既存の回り込みを転用した場合も、先に外側を後退して後列を空ける。
                    targetForward = 0;
                    if (forward == 0) targetLateral = relief.lateral;
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
            bool blocked = std::abs(next.x) > 76 || std::abs(next.z) > 76 || reservedForCentralRelief(id, next);
            // 小組の幅と双方の一刻みの移動量を確保する。障害物経路探索は後続工程。
            const float clearance = 4.5f + 3 * movementLimit * seconds;
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
    const float movementLimit = groupMovementLimit(formations);
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
    std::array<std::array<CombatImpact, 25>, 2> impactCandidates{};
    std::array<std::array<float, 25>, 2> strongestImpact{};
    std::array<std::array<BattlePoint, 25>, 2> points{};
    for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id)
        points[team][id] = formations[team].groupPosition(id);
    for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id) {
        auto& g = formations[team].organization.smallGroups[id];
        const int previousTarget = g.attackTarget;
        g.attackTarget = -1;
        g.activeFighters = 0;
        g.activeOpponents = {};
        g.canAttack = false;
        g.combatWait = CombatWait::NoTarget;
        g.awareness = {};
        const auto unit = formations[team].groupUnit(id);
        const float sight = unit == UnitType::Archer ? 36.0f : unit == UnitType::Cavalry ? 32.0f : 24.0f;
        g.awareness.sightRange = sight;
        if (!g.routed && g.strength > 0) for (unsigned otherTeam = 0; otherTeam < 2; ++otherTeam) for (unsigned other = 0; other < 25; ++other) {
            const auto& h = beforeMove[otherTeam].organization.smallGroups[other];
            if (h.routed || h.strength <= 0) continue;
            const float distance = battleDistance(points[team][id],points[otherTeam][other]);
            if (otherTeam != team && distance < sight) ++g.awareness.enemies;
            if (distance > 12 || h.resting) continue;
            if (otherTeam == team) { g.awareness.alliedStrength += h.strength; if (other != id) ++g.awareness.allies; }
            else g.awareness.enemyStrength += h.strength;
        }
        g.awareness.cautious = g.morale < 50 && g.awareness.enemyStrength > g.awareness.alliedStrength * 1.5f;
        if (g.routed || g.resting || g.strength <= 0) continue;
        const bool ranged = formations[team].groupUnit(id) == UnitType::Archer;
        float bestScore = 1000;
        bool targetBlocked = true;
        for (unsigned other = 0; other < 25; ++other) {
            const auto& enemy = formations[1 - team].organization.smallGroups[other];
            if (enemy.routed || enemy.strength <= 0) continue;
            const float distance = battleDistance(points[team][id], points[1 - team][other]);
            if (distance >= sight || distance < (ranged ? BowProfile::minRange : 0.001f)) continue;
            const auto p = points[team][id], q = points[1 - team][other];
            const bool inRange = distance <= combatProfile(formations[team].groupUnit(id)).groupRange;
            const BattlePoint end = inRange ? q : BattlePoint{q.x - (q.x - p.x) * combatProfile(formations[team].groupUnit(id)).stopDistance / distance,
                q.z - (q.z - p.z) * combatProfile(formations[team].groupUnit(id)).stopDistance / distance};
            const float clearance = inRange ? 2.25f : 4.5f + 3 * movementLimit * seconds;
            bool blocked = false;
            for (unsigned blockerTeam = 0; blockerTeam < 2 && !blocked; ++blockerTeam)
                for (unsigned blocker = 0; blocker < 25; ++blocker) {
                    if ((blockerTeam == team && blocker == id) || (blockerTeam != team && blocker == other) ||
                        formations[blockerTeam].organization.smallGroups[blocker].strength <= 0 ||
                        (inRange && formations[blockerTeam].organization.smallGroups[blocker].routed)) continue;
                    if (segmentDistance(p, end, points[blockerTeam][blocker]) < clearance) { blocked = true; break; }
                }
            if (ranged) blocked = !clearShot({team, id, other, p, q});
            // 通れる候補を最優先。射程内と現在の目標を優先し、騎馬は槍正面を避ける。
            float score = distance - (inRange ? 8.0f : 0) - (previousTarget == static_cast<int>(other) ? .75f : 0);
            if (unit == UnitType::Cavalry) {
                const float heading = beforeMove[1-team].organization.smallGroups[other].heading;
                const float facing = ((p.x-q.x)*std::cos(heading)+(p.z-q.z)*std::sin(heading))/distance;
                if (formations[1-team].groupUnit(other) == UnitType::Spearman && facing >= .5f && enemy.morale > 40) score += 6;
                if (formations[1-team].groupUnit(other) == UnitType::Archer) score -= 3;
                score += 1.5f * (enemy.morale / 100 + enemy.strength / enemy.nominalStrength);
            }
            if (g.attackTarget < 0 || (!blocked && targetBlocked) || (blocked == targetBlocked && score < bestScore)) {
                bestScore = score; g.attackTarget = static_cast<int>(other); targetBlocked = blocked;
            }
        }
        const auto p = points[team][id];
        const float targetHeading = g.detourTarget >= 0 && g.route == SmallGroupRoute::None ? std::atan2(g.detourZ-p.z,g.detourX-p.x) :
            g.attackTarget >= 0 ? std::atan2(points[1 - team][g.attackTarget].z - p.z,
            points[1 - team][g.attackTarget].x - p.x) : formations[team].heading;
        g.heading = turnToward(g.heading, targetHeading, combatProfile(formations[team].groupUnit(id)).turnRate * seconds);
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
        if (f.groupUnit(id) == UnitType::Archer) continue;
        if (g.attackTarget < 0) continue;
        const auto p = points[team][id], q = points[1 - team][g.attackTarget];
        const float distance = battleDistance(p, q);
        g.combatWait = CombatWait::OutOfRange;
        if (distance > combatProfile(formations[team].groupUnit(id)).groupRange || distance < 0.001f) continue;
        localContact = true;
        if (g.route == SmallGroupRoute::Returning || g.route == SmallGroupRoute::ReliefReserve ||
            g.route == SmallGroupRoute::ReliefWithdraw || g.route == SmallGroupRoute::ReliefCorridor) { g.combatWait = CombatWait::Returning; continue; }
        const float facing = (std::cos(g.heading) * (q.x - p.x) + std::sin(g.heading) * (q.z - p.z)) / distance;
        if (facing < 0.5f) { g.combatWait = CombatWait::Turning; continue; }
        bool blocked = false;
        for (unsigned otherTeam = 0; otherTeam < 2; ++otherTeam) for (unsigned other = 0; other < 25; ++other) {
            if ((otherTeam == team && other == id) || (otherTeam != team && other == static_cast<unsigned>(g.attackTarget)) ||
                formations[otherTeam].organization.smallGroups[other].routed) continue;
            if (segmentDistance(p, q, points[otherTeam][other]) < 2.25f) blocked = true;
        }
        if (blocked) { g.combatWait = CombatWait::Obstructed; continue; }
        const auto participation = participatingContactFronts(measureContactFronts(contactBodies, team, id), g.faceDeployment, g.strength);
        bool charged = false;
        for (unsigned target = 0; target < 25; ++target) {
            float fighters = 0;
            for (const auto& face : participation.faces) fighters += face.enemyFighters[target];
            if (fighters <= 0) continue;
            const auto targetPoint = points[1 - team][target];
            const float targetDistance = battleDistance(p, targetPoint);
            if (targetDistance < 0.001f || targetDistance > combatProfile(f.groupUnit(id)).groupRange) continue;
            bool targetBlocked = false;
            for (unsigned blockerTeam = 0; blockerTeam < 2 && !targetBlocked; ++blockerTeam)
                for (unsigned blocker = 0; blocker < 25; ++blocker) {
                    if ((blockerTeam == team && blocker == id) || (blockerTeam != team && blocker == target) ||
                        formations[blockerTeam].organization.smallGroups[blocker].strength <= 0 ||
                        formations[blockerTeam].organization.smallGroups[blocker].routed) continue;
                    if (segmentDistance(p, targetPoint, points[blockerTeam][blocker]) < 2.25f) { targetBlocked = true; break; }
                }
            if (targetBlocked) continue;
            g.activeOpponents[target] = fighters;
            g.activeFighters += fighters;
            const auto& victim = formations[1 - team].organization.smallGroups[target];
            const float defense = (std::cos(victim.heading) * (p.x - targetPoint.x) + std::sin(victim.heading) * (p.z - targetPoint.z)) / targetDistance;
            const float directionBonus = defense < -0.5f ? 1.5f : defense < 0.5f ? 1.25f : 1.0f;
            float strike = combatProfile(f.groupUnit(id)).damagePerSecond * (fighters / 5) * (0.5f + f.cohesion / 200) *
                (0.5f + g.morale / 200) * directionBonus * seconds;
            damage[1-team][target] += strike;
            bool chargeImpact = false;
            if (f.groupUnit(id) == UnitType::Cavalry && g.chargeWindow > 0 && f.maneuverEnabled &&
                !g.resting && !g.routed && g.route == SmallGroupRoute::None && g.morale > 40) {
                const bool braced = formations[1-team].groupUnit(target) == UnitType::Spearman && defense >= .5f && victim.morale > 40;
                strike += fighters * .4f * (braced ? .15f : 1.0f);
                damage[1-team][target] += fighters * .4f * (braced ? .15f : 1.0f);
                chargeImpact = true;
                charged = true;
            }
            auto& impact = impactCandidates[1-team][target];
            if (chargeImpact || (impact.kind != ImpactKind::Charge && strike > strongestImpact[1-team][target])) {
                impact.position = {(p.x+targetPoint.x)*.5f, (p.z+targetPoint.z)*.5f};
                impact.kind = chargeImpact ? ImpactKind::Charge : ImpactKind::Melee;
                strongestImpact[1-team][target] = strike;
            }
        }
        g.canAttack = g.activeFighters > 0;
        if (charged) {
            ++g.charges; g.lastCharge = time; g.chargeWindow = 0;
        }
        g.combatWait = CombatWait::None;
    }
    std::array<bool, 2> moved{};
    for (unsigned team = 0; team < 2; ++team)
        moved[team] = std::hypot(formations[team].x - beforeMove[team].x, formations[team].z - beforeMove[team].z) > .0001f;
    updateArrows(seconds, damage, moved);
    for (unsigned i = 0; i < formations.size(); ++i) {
        auto& f = formations[i];
        float actualLoss = 0;
        for (unsigned id = 0; id < 25; ++id) {
            auto& g = f.organization.smallGroups[id];
            const float loss = std::min(g.strength, damage[i][id]);
            if (loss > 0) {
                auto impact = impactCandidates[i][id];
                if (g.rangedLoss > beforeMove[i].organization.smallGroups[id].rangedLoss && impact.kind != ImpactKind::Charge) {
                    impact.position = g.lastRangedImpact; impact.kind = ImpactKind::Arrow;
                }
                // 実損害だけ通知する。連続損害は間引き、突撃の初撃は優先して残す。
                if (time >= nextImpactTime[i][id] || impact.kind == ImpactKind::Charge) {
                    impact.time = time; impact.team = i; impact.group = id; impact.damage = loss;
                    impacts[impactSerial++ % impactCapacity] = impact;
                    nextImpactTime[i][id] = time + .22;
                }
            }
            g.strength = std::max(0.0f, g.strength - loss);
            if (loss > 0) { g.morale = std::max(0.0f, g.morale - loss * 2 - seconds * 0.5f); g.lastDamageTime = time; }
            actualLoss += loss;
        }
        if (formationsTouch || localContact) { f.moving = false; f.state = FormationState::Engaged; }
        f.strength = std::max(0.0f, f.strength - actualLoss);
        if (actualLoss > 0) f.cohesion = std::max(0.0f, f.cohesion - 1.5f * seconds - actualLoss * 0.08f);
    }
}
void BattleSimulation::updateDecisions() {
    for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id) {
        auto& f = formations[team]; auto& g = f.organization.smallGroups[id];
        auto& decision = g.awareness.decision;
        if (g.routed || f.defeated()) decision = TacticalDecision::Retreat;
        else if (result != BattleResult::Ongoing || !f.maneuverEnabled) decision = TacticalDecision::Hold;
        else if (g.resting || g.route == SmallGroupRoute::Returning || g.route == SmallGroupRoute::ReliefWithdraw ||
            g.route == SmallGroupRoute::ReliefReserve || g.route == SmallGroupRoute::ReliefCorridor) decision = TacticalDecision::Recover;
        else if (g.canAttack) decision = TacticalDecision::Engage;
        else if (g.route == SmallGroupRoute::Outward || g.route == SmallGroupRoute::Forward || g.detourTarget >= 0) decision = TacticalDecision::Flank;
        else if (g.awareness.cautious && !f.moving) decision = TacticalDecision::Reserve;
        else if (g.attackTarget >= 0 && !f.moving && f.groupUnit(id) != UnitType::Cavalry && f.groupUnit(id) != UnitType::Archer &&
            battleDistance(f.groupPosition(id),formations[1-team].groupPosition(g.attackTarget)) >= 16) decision = TacticalDecision::Reserve;
        else if (g.state == SmallGroupState::Advancing || g.attackTarget >= 0) decision =
            g.combatWait == CombatWait::PathBlocked || g.combatWait == CombatWait::Obstructed ? TacticalDecision::Reserve : TacticalDecision::Advance;
        else decision = TacticalDecision::Search;
    }
}
void BattleSimulation::updateFaceDeployments(float seconds) {
    if (result != BattleResult::Ongoing) arrows.clear();
    std::array<std::array<ContactBody, 25>, 2> bodies{};
    for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id) {
        const auto& g = formations[team].organization.smallGroups[id];
        bodies[team][id] = {formations[team].groupPosition(id), g.heading, g.strength > 0, g.strength > 0 && !g.routed};
    }
    for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id) {
        auto& g = formations[team].organization.smallGroups[id];
        if (g.routed || g.resting || g.strength <= 0 || result != BattleResult::Ongoing) { g.faceDeployment = {}; g.activeFighters = 0; g.activeOpponents = {}; continue; }
        const bool returning = g.route == SmallGroupRoute::Returning || g.route == SmallGroupRoute::ReliefReserve || g.route == SmallGroupRoute::ReliefWithdraw || g.route == SmallGroupRoute::ReliefCorridor;
        const auto fronts = returning ? ContactFronts{} : measureContactFronts(bodies, team, id);
        advanceFaceDeployment(g.faceDeployment, allocateContactFronts(fronts, g.strength), g.strength, seconds);
    }
}
bool BattleSimulation::clearShot(const ArrowVolley& arrow) const {
    for (unsigned sample = 1; sample < 32; ++sample) {
        const float fraction = sample / 32.0f;
        const auto p = arrow.position(fraction);
        const float height = arrow.height(fraction);
        if (height <= terrainHeight(p.x, p.z) + .1f) return false;
        for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id) {
            if ((team == arrow.team && id == arrow.shooter) || (team != arrow.team && id == arrow.target) ||
                formations[team].organization.smallGroups[id].strength <= 0) continue;
            const auto body = formations[team].groupPosition(id);
            if (battleDistance(p, body) < 2.25f && height <= terrainHeight(body.x, body.z) + 2) return false;
        }
    }
    return true;
}

void BattleSimulation::updateArrows(float seconds, std::array<std::array<float, 25>, 2>& damage, const std::array<bool, 2>& moved) {
    for (auto& arrow : arrows) {
        arrow.age = std::min(arrow.duration, arrow.age + seconds);
        const float fraction = arrow.age / arrow.duration;
        const auto p = arrow.position(fraction);
        bool blocked = arrow.height(fraction) <= terrainHeight(p.x, p.z) + .1f;
        for (unsigned team = 0; team < 2 && !blocked; ++team) for (unsigned id = 0; id < 25; ++id) {
            if ((team == arrow.team && id == arrow.shooter) || (team != arrow.team && id == arrow.target) ||
                formations[team].organization.smallGroups[id].strength <= 0) continue;
            const auto body = formations[team].groupPosition(id);
            if (battleDistance(p, body) < 2.25f && arrow.height(fraction) <= terrainHeight(body.x, body.z) + 2) { blocked = true; break; }
        }
        if (blocked) { arrow.age = arrow.duration; continue; }
        if (arrow.age < arrow.duration) continue;
        auto& victim = formations[1 - arrow.team].organization.smallGroups[arrow.target];
        // 発射時の狙点に到着する。追尾せず、射手が敗走した後も飛翔は継続する。
        if (victim.strength <= 0 || battleDistance(arrow.aim, formations[1 - arrow.team].groupPosition(arrow.target)) > 2.25f) continue;
        const float loss = std::min(arrow.damage, std::max(0.0f, victim.strength - damage[1 - arrow.team][arrow.target]));
        damage[1 - arrow.team][arrow.target] += loss;
        victim.rangedLoss += loss;
        if (loss > 0) { victim.lastRangedImpact = arrow.aim; ++volleysHit; }
    }
    std::erase_if(arrows, [](const ArrowVolley& arrow) { return arrow.age >= arrow.duration; });
    for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id) {
        auto& formation = formations[team];
        auto& group = formation.organization.smallGroups[id];
        if (formation.groupUnit(id) != UnitType::Archer) continue;
        group.shotCooldown = std::max(0.0f, group.shotCooldown - seconds);
        if (group.routed || group.resting || group.strength <= 0 || formation.defeated()) continue;
        group.combatWait = CombatWait::OutOfRange;
        if (group.attackTarget < 0) continue;
        const auto p = formation.groupPosition(id), q = formations[1 - team].groupPosition(group.attackTarget);
        const float distance = battleDistance(p, q);
        if (distance < BowProfile::minRange || distance > BowProfile::maxRange) continue;
        bool closeEnemy = false;
        for (unsigned other = 0; other < 25; ++other)
            if (formations[1 - team].organization.smallGroups[other].strength > 0 &&
                battleDistance(p, formations[1 - team].groupPosition(other)) < BowProfile::minRange) closeEnemy = true;
        if (closeEnemy || moved[team] || group.route != SmallGroupRoute::None) { group.combatWait = CombatWait::Held; continue; }
        const float facing = (std::cos(group.heading) * (q.x - p.x) + std::sin(group.heading) * (q.z - p.z)) / distance;
        if (facing < .94f) { group.combatWait = CombatWait::Turning; continue; }
        ArrowVolley arrow{team, id, static_cast<unsigned>(group.attackTarget), p, q};
        if (!clearShot(arrow)) { group.combatWait = CombatWait::Obstructed; continue; }
        group.combatWait = CombatWait::None;
        if (group.shotCooldown > 0) continue;
        const float shooters = std::min(BowProfile::maxShooters, group.strength);
        arrow.duration = distance / BowProfile::arrowSpeed;
        arrow.damage = shooters * BowProfile::damagePerShooter * (.5f + group.morale / 200);
        arrows.push_back(arrow); ++volleysFired;
        group.lastShot = time; group.shotCooldown = BowProfile::interval;
        group.activeFighters = shooters; group.activeOpponents[group.attackTarget] = shooters; group.canAttack = true;
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
    const auto beforeRouts = formations;
    std::array<bool, 2> broken{};
    for (unsigned team = 0; team < 2; ++team) {
        auto& f = formations[team];
        const auto& enemy = beforeRouts[1 - team];
        // 刻みの開始時の敗走者と実位置だけを参照し、同じ刻みで連鎖を再帰させない。
        std::array<unsigned, 25> nearbyCounts{};
        for (unsigned id = 0; id < 25; ++id) nearbyCounts[id] = nearbyRouts(team, id);
        for (auto& g : f.organization.smallGroups) {
            g.routShock = std::max(0.0f, g.routShock - seconds);
        }
        for (unsigned id = 0; id < 25; ++id) {
            auto& g = f.organization.smallGroups[id];
            if (g.routed) continue;
            // 同時に複数小組が崩れても、一瞬で周囲の士気を使い切らない。
            g.morale = std::max(0.0f, g.morale - std::min(nearbyCounts[id], 1u) * 6.0f * seconds);
            if (g.resting) {
                bool safe = nearbyCounts[id] == 0 && time - g.lastDamageTime >= 3;
                for (unsigned other = 0; other < 25 && safe; ++other)
                    if (!enemy.organization.smallGroups[other].routed && enemy.organization.smallGroups[other].strength > 0 &&
                        battleDistance(f.groupPosition(id), enemy.groupPosition(other)) < 10) safe = false;
                if (safe) {
                    g.morale = std::min(100.0f, g.morale + 3 * seconds);
                    g.fatigue = std::max(0.0f, g.fatigue - seconds);
                    if (g.morale >= 70 && g.fatigue <= 2) g.resting = false;
                }
            }
            if (g.morale > 20 && g.strength > g.nominalStrength * 0.25f) continue;
            // 交代後退中と休息中は、士気の軽い下振れだけで敗走を確定しない。
            if ((g.route == SmallGroupRoute::ReliefWithdraw || g.resting) && g.morale > 10 &&
                g.strength > g.nominalStrength * .25f) continue;
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
        const bool exhausted = f.routedGroups() >= 13 && f.readyGroups() <= 5;
        f.withdrawalPressure = exhausted ? f.withdrawalPressure + seconds : std::max(0.0f, f.withdrawalPressure - 2 * seconds);
        broken[team] = f.withdrawalPressure >= 6;
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
