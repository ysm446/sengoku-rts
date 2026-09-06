#pragma once
#include "melee_profile.h"
#include "combat_geometry.h"
#include "terrain.h"

struct BowProfile {
    static constexpr float minRange = 8, maxRange = 36, interval = 3, arrowSpeed = 20;
    static constexpr float maxShooters = 10, damagePerShooter = 0.18f;
};

// 接近・旋回用の共通値。弓の損害は近接処理へ流さない。
inline MeleeProfile combatProfile(UnitType unit) {
    if (unit == UnitType::Archer) return {BowProfile::maxRange, 30, BowProfile::maxRange, 1.8f, 3.0f, 0};
    return meleeProfile(unit);
}

struct ArrowVolley {
    unsigned team = 0, shooter = 0, target = 0;
    BattlePoint from{}, aim{};
    float age = 0, duration = 1, damage = 0;
    float height(float fraction) const {
        const float start = terrainHeight(from.x, from.z) + 1.5f;
        const float end = terrainHeight(aim.x, aim.z) + 0.5f;
        return start + (end - start) * fraction + 4 * std::max(3.0f, battleDistance(from, aim) * .2f) * fraction * (1 - fraction);
    }
    BattlePoint position(float fraction) const {
        return {from.x + (aim.x - from.x) * fraction, from.z + (aim.z - from.z) * fraction};
    }
};
