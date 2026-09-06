#include "simulation.h"
#include <iostream>
#include <stdexcept>

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
BattleSimulation reserves() {
    BattleSimulation battle;
    battle.hold(0); battle.hold(1);
    for (unsigned id = 0; id < 13; ++id) {
        auto& g = battle.formations[0].organization.smallGroups[id];
        g.routed = true; g.fleeX = g.fleeTargetX = -60;
        g.fleeZ = g.fleeTargetZ = -60 + id * 4.6f;
    }
    battle.running = true;
    return battle;
}
int main() {
    try {
        auto healthy = reserves(); healthy.update(10);
        require(healthy.formations[0].readyGroups() == 12 && healthy.result == BattleResult::Ongoing,
            "Healthy remaining troops were forced to withdraw by rout count alone");
        auto exhausted = reserves();
        for (unsigned id = 13; id < 25; ++id) exhausted.formations[0].organization.smallGroups[id].strength = 9;
        exhausted.update(3);
        require(exhausted.result == BattleResult::Ongoing && exhausted.formations[0].withdrawalPressure > 2,
            "Army withdrawal did not wait for sustained exhaustion");
        auto recovered = exhausted;
        for (unsigned id = 13; id < 19; ++id) recovered.formations[0].organization.smallGroups[id].strength = 20;
        recovered.update(4);
        require(recovered.result == BattleResult::Ongoing && recovered.formations[0].withdrawalPressure == 0,
            "Available reserves did not cancel withdrawal consideration");
        const float pressure = exhausted.formations[0].withdrawalPressure;
        exhausted.running = false; exhausted.update(10);
        require(exhausted.formations[0].withdrawalPressure == pressure, "Pause advanced withdrawal pressure");
        exhausted.running = true; exhausted.update(4);
        require(exhausted.result == BattleResult::BlueVictory && exhausted.formations[0].defeated(),
            "Exhausted army failed to withdraw");

        BattleSimulation resting; resting.hold(0); resting.hold(1); resting.running = true;
        auto& group = resting.formations[0].organization.smallGroups[2];
        group.resting = true; group.morale = 18; group.fatigue = 10; group.strength = 16;
        resting.formations[0].strength -= 4;
        resting.update(1);
        require(group.resting && !group.routed && group.morale > 18 && group.fatigue < 10 && !group.canAttack,
            "Withdrawing troops routed or failed to recover safely");
        auto paused = resting; paused.running = false; paused.update(5);
        require(paused.formations[0].organization.smallGroups[2].morale == group.morale, "Pause healed resting troops");
        resting.update(20);
        require(!group.resting && group.morale >= 70 && group.fatigue <= 2 && group.strength == 16,
            "Recovered troops did not rejoin readiness or regenerated casualties");
        BattleSimulation threatened; threatened.hold(0); threatened.hold(1); threatened.running = true;
        auto& wounded = threatened.formations[0].organization.smallGroups[2];
        wounded.resting = true; wounded.morale = 30; wounded.lastDamageTime = 0;
        threatened.update(2);
        require(wounded.morale == 30 && wounded.resting, "Recently hit troops recovered immediately");
        resting.reset(); require(!resting.formations[0].organization.smallGroups[2].resting && resting.formations[0].withdrawalPressure == 0,
            "Reset retained recovery state");
        std::cout << "Reserve readiness, rest, recovery and delayed withdrawal checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
