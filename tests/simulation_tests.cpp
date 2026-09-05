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
        for (unsigned id = 0; id < 25; ++id) {
            const auto& g = x.organization.smallGroups[id];
            const auto& h = y.organization.smallGroups[id];
            require(g.state == h.state && std::abs(g.offsetX - h.offsetX) < 0.001f &&
                std::abs(g.offsetZ - h.offsetZ) < 0.001f && std::abs(g.fatigue - h.fatigue) < 0.001f &&
                g.route == h.route, "Small group depends on update interval");
        }
    }
}
int main() {
    try {
        BattleSimulation battle;
        const auto& organization = battle.formations[0].organization;
        unsigned total = 0;
        for (unsigned troop = 0; troop < organization.troops.size(); ++troop) {
            const auto& unit = organization.troops[troop];
            unsigned strength = 0;
            for (unsigned c = unit.firstCompany; c < unit.firstCompany + unit.companyCount; ++c) {
                const auto& company = organization.companies[c];
                require(company.troop == troop, "Company assigned to wrong troop");
                for (unsigned g = company.firstGroup; g < company.firstGroup + company.groupCount; ++g) {
                    require(organization.smallGroups[g].company == c, "Small group assigned to wrong company");
                    strength += organization.smallGroups[g].nominalStrength;
                }
            }
            require(strength >= 200 && strength <= 500, "Troop strength outside planned range");
            total += strength;
        }
        require(total == 500, "Hierarchy lost or duplicated nominal strength");
        battle.toggle(); battle.update(10);
        for (unsigned team = 0; team < 2; ++team) {
            unsigned engaged = 0, waiting = 0, advancing = 0;
            for (const auto& group : battle.formations[team].organization.smallGroups) {
                engaged += group.state == SmallGroupState::Engaged;
                waiting += group.state == SmallGroupState::Waiting;
                advancing += group.state == SmallGroupState::Advancing;
            }
            require(engaged == 5 && waiting == 18 && advancing == 2, "Unexpected frontline or flank allocation");
        }
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
        for (bool alongX : {false, true}) {
            BattleSimulation flank;
            for (unsigned team = 0; team < 2; ++team) {
                auto& f = flank.formations[team];
                f.x = f.targetX = alongX ? (team == 0 ? -14.0f : 14.0f) : 0;
                f.z = f.targetZ = alongX ? 0 : (team == 0 ? -14.0f : 14.0f);
                f.morale = 100;
            }
            flank.toggle(); flank.update(2);
            const unsigned id = alongX ? 3 : 15;
            const auto outside = flank.formations[0].organization.smallGroups[id];
            require((alongX ? outside.offsetZ : outside.offsetX) < -3 &&
                (alongX ? outside.offsetX : outside.offsetZ) == 0, "Flank cut through the front rank");
            auto halted = flank;
            halted.hold(0); halted.update(2);
            const auto& held = halted.formations[0].organization.smallGroups[id];
            require(held.offsetX == outside.offsetX && held.offsetZ == outside.offsetZ,
                "Hold did not cancel an active flank route");
            flank.toggle(); const auto pausedFlank = flank; flank.update(2); same(flank, pausedFlank);
            flank.toggle(); flank.update(6);
            const auto advanced = flank.formations[0].organization.smallGroups[id];
            require(std::abs((alongX ? advanced.offsetZ : advanced.offsetX) + 6.1f) < 0.001f &&
                (alongX ? advanced.offsetX : advanced.offsetZ) > 3, "Flank did not advance outside its formation");
            require(advanced.state == SmallGroupState::Engaged, "Flank did not stop at enemy contact");
            auto relief = flank;
            bool returned = false, replaced = false, withdrew = false;
            for (unsigned tick = 0; tick < 1500; ++tick) {
                relief.update(1.0f / 60);
                const auto& groups = relief.formations[0].organization.smallGroups;
                const auto& veteran = groups[id];
                if (veteran.route == SmallGroupRoute::Returning) {
                    withdrew = true;
                    if ((alongX ? veteran.offsetX : veteran.offsetZ) != 0)
                        require(std::abs((alongX ? veteran.offsetZ : veteran.offsetX) + 6.1f) < 0.001f,
                            "Returning group cut diagonally through its formation");
                }
                if (withdrew && veteran.route == SmallGroupRoute::None && veteran.offsetX == 0 && veteran.offsetZ == 0)
                    returned = true;
                const unsigned reserveId = alongX ? 2 : 10;
                if (groups[reserveId].route != SmallGroupRoute::None) {
                    require(returned, "Reserve entered the route before the veteran returned");
                    replaced = true;
                    break;
                }
            }
            require(withdrew && returned && replaced, "Fatigued flank did not rotate with a reserve");
            const float tired = relief.formations[0].organization.smallGroups[id].fatigue;
            relief.hold(0); relief.update(2);
            require(relief.formations[0].organization.smallGroups[id].fatigue < tired,
                "Resting veteran did not recover fatigue");
            auto disengaged = flank;
            disengaged.move(0, alongX ? -55.0f : 0, alongX ? 0 : -55.0f);
            disengaged.hold(1); disengaged.update(20);
            const auto& regrouped = disengaged.formations[0].organization.smallGroups[id];
            require(regrouped.route == SmallGroupRoute::None && regrouped.offsetX == 0 && regrouped.offsetZ == 0,
                "Disengaged group did not return to its standard slot");
            auto returningPause = flank;
            returningPause.move(0, alongX ? -55.0f : 0, alongX ? 0 : -55.0f);
            returningPause.update(1);
            returningPause.toggle(); const auto frozenReturn = returningPause;
            returningPause.update(10); same(returningPause, frozenReturn);
            returningPause.toggle(); returningPause.hold(0);
            const auto heldReturn = returningPause.formations[0].organization.smallGroups[id];
            returningPause.update(1);
            const auto& stillHeld = returningPause.formations[0].organization.smallGroups[id];
            require(stillHeld.offsetX == heldReturn.offsetX && stillHeld.offsetZ == heldReturn.offsetZ,
                "Hold did not freeze the returning group");
            flank.hold(0); flank.update(1);
            const auto stopped = flank.formations[0].organization.smallGroups[id];
            require(stopped.offsetX == advanced.offsetX && stopped.offsetZ == advanced.offsetZ,
                "Hold did not stop small-group movement");
            flank.reset(); same(flank, BattleSimulation{});
        }
        BattleSimulation blocked;
        blocked.formations[0].z = blocked.formations[0].targetZ = -14;
        blocked.formations[1].z = blocked.formations[1].targetZ = 14;
        // 後方小組を外側の通路へ置き、追い越し側が重ならず待つことを確認する。
        blocked.formations[0].organization.smallGroups[10].offsetX = -5.2f;
        blocked.formations[0].organization.smallGroups[10].offsetZ = 5.2f;
        blocked.toggle(); blocked.update(2);
        const auto& waiting = blocked.formations[0].organization.smallGroups[15];
        require(waiting.state == SmallGroupState::Waiting && waiting.offsetX > -0.7f && waiting.offsetZ == 0,
            "Flank passed through an occupied route");
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
