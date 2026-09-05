#include "simulation.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void same(const BattleSimulation& a, const BattleSimulation& b) {
    require(a.result == b.result, "Result depends on update interval");
    for (unsigned i = 0; i < 2; ++i) {
        const auto& x = a.formations[i]; const auto& y = b.formations[i];
        require(x.state == y.state && std::abs(x.x - y.x) < 0.001f && std::abs(x.z - y.z) < 0.001f &&
            std::abs(x.strength - y.strength) < 0.001f && std::abs(x.morale - y.morale) < 0.001f &&
            std::abs(x.cohesion - y.cohesion) < 0.001f, "Formation depends on update interval");
    }
}
int main() {
    try {
        BattleSimulation battle;
        battle.toggle(); battle.update(10);
        require(battle.formations[0].state == FormationState::Engaged && battle.formations[1].state == FormationState::Engaged,
            "Default formations did not engage");
        require(battle.formations[1].z - battle.formations[0].z >= 27.99f, "Formations overlapped");
        for (unsigned i = 0; i < 2; ++i) {
            const auto& f = battle.formations[i];
            require(f.strength < 500 && f.cohesion < 100 && f.morale < (i == 0 ? 100 : 85) && !f.moving,
                "Combat did not reduce strength, cohesion and morale");
        }
        battle.toggle(); const auto paused = battle; battle.update(30); same(battle, paused);
        require(battle.time == paused.time, "Paused battle advanced time");
        battle.toggle(); battle.hold(0); battle.update(1);
        require(battle.formations[0].strength < paused.formations[0].strength, "Hold made engaged formation invulnerable");
        battle.update(60);
        require(battle.result == BattleResult::RedVictory && battle.formations[1].state == FormationState::Routed,
            "Default battle did not finish with a retreat");
        const auto defeated = battle.formations[1];
        battle.move(1, 0, 0); battle.hold(1); battle.update(10);
        require(battle.formations[1].z == defeated.z && battle.formations[1].targetZ == defeated.targetZ &&
            battle.formations[1].strength == defeated.strength, "Defeated formation obeyed orders or took post-battle losses");
        BattleSimulation thirty, oneFortyFour, single;
        thirty.toggle(); oneFortyFour.toggle(); single.toggle();
        for (int i = 0; i < 2400; ++i) thirty.update(1.0f / 30);
        for (int i = 0; i < 11520; ++i) oneFortyFour.update(1.0f / 144);
        single.update(80); same(thirty, oneFortyFour); same(thirty, single);
        BattleSimulation equal;
        equal.formations[1].morale = 100; equal.toggle(); equal.update(80);
        require(equal.result == BattleResult::Draw && equal.formations[0].state == FormationState::Routed &&
            equal.formations[1].state == FormationState::Routed, "Symmetric battle favored update order");
        BattleSimulation reversed;
        reversed.formations[0].morale = 85; reversed.formations[1].morale = 100; reversed.toggle(); reversed.update(80);
        require(reversed.result == BattleResult::BlueVictory, "Result is hardcoded to a team");
        BattleSimulation separated;
        separated.move(0, -45, -22); separated.move(1, 45, 22); separated.toggle(); separated.update(80);
        require(separated.result == BattleResult::Ongoing && separated.formations[0].strength == 500 &&
            separated.formations[1].strength == 500, "Non-contact formations fought");
        BattleSimulation escape;
        escape.toggle(); escape.update(6); escape.move(0, 0, -55); escape.hold(1); escape.update(10);
        const auto escapedStrength = escape.formations[0].strength; escape.update(10);
        require(escape.formations[0].strength == escapedStrength && escape.result == BattleResult::Ongoing,
            "Move away did not disengage");
        battle.reset(); same(battle, BattleSimulation{});
        BattleSimulation edge;
        edge.formations[0].x = 60; edge.formations[1].x = 59;
        edge.formations[0].z = edge.formations[1].z = 60;
        edge.hold(0); edge.hold(1); edge.toggle(); edge.update(1);
        for (const auto& f : edge.formations) require(std::abs(f.x) <= 60 && std::abs(f.z) <= 60,
            "Contact pushed formation outside terrain");
        require(std::abs(edge.formations[0].x - edge.formations[1].x) >= 27.99f ||
            std::abs(edge.formations[0].z - edge.formations[1].z) >= 27.99f, "Edge contact overlap unresolved");
        require(!battle.running && battle.time == 0, "Reset did not clear battle");
        for (float invalid : {-1.0f, 3601.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
            bool rejected = false;
            try { battle.update(invalid); } catch (const std::invalid_argument&) { rejected = true; }
            require(rejected, "Invalid timestep accepted");
        }
        std::cout << "Combat, morale, retreat and fixed-step checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
