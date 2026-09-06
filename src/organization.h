#pragma once
#include <array>
#include "contact_fronts.h"
#include "unit_type.h"
#include <optional>

// ユーザー指定の4階層。人数は編成兵力であり描画Sprite数ではない。
enum class SmallGroupState { Waiting, Advancing, Engaged, Retreating, Fleeing, Routed };
enum class SmallGroupRoute { None, Outward, Forward, Returning, ReliefReserve, ReliefWithdraw };
enum class CombatWait { None, NoTarget, OutOfRange, Turning, Obstructed, PathBlocked, Held, Returning, Detouring };
struct SmallGroup {
    unsigned nominalStrength = 20;
    float strength = 20;
    float morale = 100;
    unsigned company = 0;
    unsigned slot = 0;
    SmallGroupState state = SmallGroupState::Waiting;
    // 備の標準配置からの変位。表示人数とは独立して固定刻みで更新する。
    float offsetX = 0, offsetZ = 0;
    float targetOffsetX = 0, targetOffsetZ = 0;
    float fatigue = 0;
    float heading = 0;
    FaceDeployment faceDeployment;
    float activeFighters = 0;
    std::array<float, 25> activeOpponents{};
    std::optional<UnitType> unit;
    int attackTarget = -1;
    bool canAttack = false;
    CombatWait combatWait = CombatWait::NoTarget;
    float approachX = 0, approachZ = 0;
    int detourTarget = -1;
    float detourX = 0, detourZ = 0;
    SmallGroupRoute route = SmallGroupRoute::None;
    bool routeAlongX = false;
    float routeForward = 0;
    bool routed = false;
    float routShock = 0;
    // 敗走は命令移動から独立した世界座標で進める。
    float fleeX = 0, fleeZ = 0, fleeTargetX = 0, fleeTargetZ = 0;
    bool fleeBlocked = false;
    int fleeSide = 1;
    float displacementX(unsigned id) const { return (static_cast<float>(slot % 5) - static_cast<float>(id % 5)) * 5.2f + offsetX + approachX; }
    float displacementZ(unsigned id) const { return (static_cast<float>(slot / 5) - static_cast<float>(id / 5)) * 5.2f + offsetZ + approachZ; }
};
// 正面両端の交代。配置だけを交換し、所属・兵力・表示IDは移さない。
struct FrontRelief {
    int front = -1, reserve = -1;
    unsigned phase = 0;
    bool alongX = false;
    float forward = 0;
};
struct Company { unsigned firstGroup = 0, groupCount = 5, troop = 0; };
struct Troop { unsigned firstCompany = 0, companyCount = 0; };
struct Organization {
    std::array<SmallGroup, 25> smallGroups{};
    std::array<FrontRelief, 2> frontReliefs{};
    std::array<Company, 5> companies{};
    std::array<Troop, 2> troops{{{0, 3}, {3, 2}}};
    Organization() {
        for (unsigned i = 0; i < smallGroups.size(); ++i) { smallGroups[i].company = i / 5; smallGroups[i].slot = i; }
        for (unsigned i = 0; i < companies.size(); ++i) companies[i] = {i * 5, 5, i < 3 ? 0u : 1u};
    }
    static unsigned groupForSoldier(unsigned id) {
        return ((id / 71) * 5 / 71) * 5 + ((id % 71) * 5 / 71);
    }
};
