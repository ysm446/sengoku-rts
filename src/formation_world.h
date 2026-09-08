#pragma once
#include "simulation.h"
#include <optional>
#include <string>

struct WorldGroupId { unsigned formation=0, group=0; };
enum class WorldOrder { Hold, Move, Advance, Withdraw };
struct WorldImpact {
    WorldGroupId victim;
    BattlePoint position{};
    double time=-10;
    float damage=0;
};
// 備のIDと陣営を分離する。小組の戦闘参照にも備IDを含める。
struct WorldFormation {
    unsigned id=0, side=0;
    std::wstring name;
    Formation formation;
    std::array<std::optional<WorldGroupId>,25> targets{};
    std::array<double,25> nextImpactTime{};
    WorldOrder order=WorldOrder::Hold;
    std::optional<unsigned> advanceTarget;
    unsigned initialGroups=0;
    float initialStrength=0, withdrawalSeconds=0;
};
class FormationWorld {
public:
    std::vector<WorldFormation> formations;
    bool running=false;
    double time=0;
    bool combatEnabled=true;
    std::vector<WorldImpact> impacts;
    void reset(std::vector<WorldFormation> initial);
    void move(unsigned id,BattlePoint target);
    void hold(unsigned id);
    void advance(unsigned id);
    void advanceAll();
    std::optional<unsigned> nearestEnemy(unsigned id) const;
    void update(float seconds);
    static constexpr float boundary=450;
private:
    double accumulator=0;
    std::size_t indexOf(unsigned id) const;
    void step(float seconds);
    void fight(float seconds);
    void decide(float seconds);
};
