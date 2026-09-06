#include "simulation.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::unique_ptr<BattleSimulation> duel(float distance = 14, UnitType enemy = UnitType::Samurai) {
    auto battle = std::make_unique<BattleSimulation>();
    for (unsigned team = 0; team < 2; ++team) {
        auto& f = battle->formations[team];
        f.x = f.targetX = team == 0 ? -40.0f : 40.0f; f.z = f.targetZ = 0;
        for (auto& g : f.organization.smallGroups) { g.offsetZ = 1000; g.heading = team == 0 ? 0 : 3.141592654f; g.morale = 100; }
        auto& g = f.organization.smallGroups[12];
        g.offsetX = (team == 0 ? 0 : distance) - f.x; g.offsetZ = 0;
        g.unit = team == 0 ? UnitType::Cavalry : enemy;
        f.morale = 100;
    }
    battle->hold(1); battle->running = true; return battle;
}
float impactLoss(UnitType enemy, bool front, bool charge) {
    auto battle = duel(5.5f, enemy);
    battle->hold(0);
    auto& attacker = battle->formations[0].organization.smallGroups[12];
    attacker.faceDeployment.accountedStrength = 20;
    attacker.faceDeployment.deployed = {5,5,5,5};
    battle->formations[0].maneuverEnabled = true;
    attacker.chargeWindow = charge ? .5f : 0;
    if (!front) battle->formations[1].organization.smallGroups[12].heading = 0;
    battle->update(1.0f/60);
    return 20-battle->formations[1].organization.smallGroups[12].strength;
}
int main() {
    try {
        auto battle = duel();
        auto& g = battle->formations[0].organization.smallGroups[12];
        for (unsigned tick = 0; tick < 600 && g.charges == 0; ++tick) {
            const float speed = g.approachSpeed;
            battle->update(1.0f/60);
            require(g.approachSpeed <= speed + 1.3f/60 + .0001f, "Cavalry accelerated instantly");
            require(battleDistance(battle->formations[0].groupPosition(12),battle->formations[1].groupPosition(12)) >= 4.5f,
                "Cavalry crossed enemy occupancy");
        }
        std::cout << "charges=" << g.charges << " distance=" << g.chargeDistance << '\n';
        require(g.charges == 1, "Approaching cavalry did not charge");
        battle->hold(0); battle->update(10);
        require(g.charges == 1 && g.chargeWindow == 0, "Stationary cavalry repeated charge");
        auto close = duel(5.5f); close->update(1);
        require(close->formations[0].organization.smallGroups[12].charges == 0, "Cavalry charged without run-up");
        auto blocked = duel();
        auto& obstacle = blocked->formations[0].organization.smallGroups[13];
        obstacle.offsetX = 7-blocked->formations[0].x-5.2f; obstacle.offsetZ = 0;
        blocked->update(1);
        require(blocked->formations[0].organization.smallGroups[12].charges == 0, "Blocked cavalry gained charge impact");
        const auto& detouring = blocked->formations[0].organization.smallGroups[12];
        require(detouring.detourTarget >= 0 && std::abs(detouring.heading) > .3f,
            "Cavalry kept facing the enemy instead of its detour waypoint");
        const float braced = impactLoss(UnitType::Spearman,true,true)-impactLoss(UnitType::Spearman,true,false);
        const float rear = impactLoss(UnitType::Spearman,false,true)-impactLoss(UnitType::Spearman,false,false);
        require(braced > 0 && rear > braced * 5, "Spear facing did not affect charge impact");
        auto turning = duel();
        turning->formations[0].organization.smallGroups[12].heading = 3.141592654f;
        const auto start = turning->formations[0].groupPosition(12);
        turning->update(.5f);
        require(battleDistance(start,turning->formations[0].groupPosition(12)) < .001f, "Cavalry slid before facing enemy");
        auto slow = duel(), fast = duel();
        slow->update(6); for (unsigned tick=0; tick<360; ++tick) fast->update(1.0f/60);
        const auto& a = slow->formations[0].organization.smallGroups[12];
        const auto& b = fast->formations[0].organization.smallGroups[12];
        require(a.charges == b.charges && a.chargeCooldown == b.chargeCooldown && a.approachX == b.approachX &&
            slow->formations[1].strength == fast->formations[1].strength, "Cavalry depends on update frequency");
        fast->running = false; const auto time = fast->time; fast->update(2);
        require(fast->time == time && a.chargeCooldown == b.chargeCooldown, "Paused cavalry changed");
        auto mirrored = duel(); std::swap(mirrored->formations[0],mirrored->formations[1]); mirrored->update(6);
        require(mirrored->formations[1].organization.smallGroups[12].charges == a.charges &&
            std::abs(mirrored->formations[0].strength-slow->formations[1].strength)<.001f, "Cavalry impact depends on team color");
        fast->reset(UnitType::Spearman,true);
        for (const auto& f : fast->formations) {
            unsigned cavalry=0; float strength=0;
            for (unsigned id=0; id<25; ++id) { cavalry += f.groupUnit(id)==UnitType::Cavalry; strength += f.organization.smallGroups[id].strength;
                require(f.organization.smallGroups[id].charges==0, "Reset retained charge state"); }
            require(cavalry==4 && strength==500, "Mixed cavalry composition lost strength");
        }
        fast->running = true; fast->update(20);
        unsigned mixedCharges = 0;
        for (const auto& f : fast->formations) for (const auto& group : f.organization.smallGroups) mixedCharges += group.charges;
        std::cout << "mixed charges=" << mixedCharges << '\n';
        require(mixedCharges > 0, "Mixed cavalry never completed a charge");
        std::cout << "Cavalry run-up, impact, spear facing, pause, reset and cadence passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
