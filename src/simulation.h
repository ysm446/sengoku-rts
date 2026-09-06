#pragma once
#include <array>
#include <cstdint>
#include "organization.h"
#include "combat_geometry.h"
#include "contact_fronts.h"
#include "melee_profile.h"
#include "ranged_profile.h"
#include <vector>

enum class FormationState { Idle, Marching, Engaged, Retreating, Routed };
enum class BattleResult { Ongoing, RedVictory, BlueVictory, Draw };

struct Formation {
    float x = 0, z = 0;
    float targetX = 0, targetZ = 0;
    float heading = 0;
    float speed = 1.8f;
    bool moving = false;
    float strength = 500;
    float cohesion = 100;
    float morale = 100;
    FormationState state = FormationState::Idle;
    Organization organization;
    bool maneuverEnabled = true;
    bool movementBlocked = false;
    bool detouring = false;
    float waypointX = 0, waypointZ = 0;
    bool detourBacking = false;
    float backtrackX = 0, backtrackZ = 0;
    bool detourExiting = false;
    float exitX = 0, exitZ = 0;
    UnitType unit = UnitType::Spearman;
    UnitType groupUnit(unsigned id) const { return organization.smallGroups.at(id).unit.value_or(unit); }
    bool defeated() const { return state == FormationState::Retreating || state == FormationState::Routed; }
    BattlePoint groupPosition(unsigned id) const {
        const auto& g = organization.smallGroups[id];
        return g.routed ? BattlePoint{g.fleeX, g.fleeZ} : BattlePoint{
            x + (static_cast<float>(id % 5) - 2) * 5.2f + g.displacementX(id),
            z + (static_cast<float>(id / 5) - 2) * 5.2f + g.displacementZ(id)};
    }
    unsigned routedGroups() const {
        unsigned count = 0;
        for (const auto& g : organization.smallGroups) count += g.routed;
        return count;
    }
};

// シミュレーションは部隊だけを更新する。表示兵士数には依存しない。
class BattleSimulation {
public:
    BattleSimulation();
    void update(float seconds);
    void reset(UnitType unit = UnitType::Spearman, bool mixed = false);
    bool mixed = false;
    std::vector<ArrowVolley> arrows;
    std::uint64_t volleysFired = 0, volleysHit = 0;
    void move(unsigned index, float x, float z);
    void hold(unsigned index);
    unsigned nearbyRouts(unsigned team, unsigned group) const;
    static constexpr float routRadius = 8.0f;
    bool routInfluences(unsigned team, unsigned source, unsigned target) const;
    ContactFronts contactFronts(unsigned team, unsigned group) const;
    void toggle() { running = !running; }
    bool running = false;
    double time = 0;
    std::uint64_t generation = 0;
    std::array<Formation, 2> formations;
    BattleResult result = BattleResult::Ongoing;
private:
    double accumulator = 0;
    void step(float seconds);
    void updateSmallGroups(float seconds);
    void updateRouts(float seconds);
    void updateFaceDeployments(float seconds);
    void updateArrows(float seconds, std::array<std::array<float, 25>, 2>& damage, const std::array<bool, 2>& moved);
    bool clearShot(const ArrowVolley& arrow) const;
};
