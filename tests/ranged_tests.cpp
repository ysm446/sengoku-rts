#include "scene.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
BattleSimulation bowDuel(float range = 20) {
    BattleSimulation battle;
    for (unsigned team = 0; team < 2; ++team) {
        auto& f = battle.formations[team];
        f.x = f.targetX = team == 0 ? -40.0f : 40.0f;
        f.z = f.targetZ = 0; f.maneuverEnabled = false;
        for (auto& g : f.organization.smallGroups) { g.offsetZ = 1000; g.heading = team == 0 ? 0 : 3.14159265f; }
        auto& g = f.organization.smallGroups[12];
        g.offsetX = (team == 0 ? 0 : range) - f.x; g.offsetZ = 0;
    }
    battle.formations[0].organization.smallGroups[12].unit = UnitType::Archer;
    battle.running = true;
    return battle;
}
int main() {
    try {
        auto battle = bowDuel();
        battle.update(1.0f / 60);
        require(battle.volleysFired == 1 && battle.arrows.size() == 1 && battle.formations[1].strength == 500,
            "Bow damage arrived before its projectile");
        battle.update(.5f);
        require(battle.formations[1].strength == 500 && battle.arrows[0].age > 0, "Arrow flight did not delay damage");
        const auto pausedAge = battle.arrows[0].age;
        battle.running = false; battle.update(2);
        require(battle.arrows[0].age == pausedAge && battle.volleysFired == 1, "Pause advanced arrows or cooldown");
        battle.running = true; battle.update(.6f);
        require(battle.volleysHit == 1 && battle.formations[1].strength < 500 && battle.volleysFired == 1,
            "Arrow did not land or bow ignored its cooldown");
        for (unsigned id = 0; id < 25; ++id) if (id != 12)
            require(battle.formations[1].organization.smallGroups[id].strength == 20, "Arrow damaged another group");
        battle.update(2);
        require(battle.volleysFired == 2, "Bow did not repeat after its interval");
        battle.reset(UnitType::Spearman, true);
        require(battle.arrows.empty() && battle.volleysFired == 0 && battle.volleysHit == 0, "Reset retained an arrow");

        for (float range : {7.0f, 37.0f}) {
            auto limited = bowDuel(range); limited.update(.5f);
            require(limited.volleysFired == 0, "Bow ignored its minimum or maximum range");
        }
        auto turning = bowDuel(); turning.formations[0].organization.smallGroups[12].heading = 3.14159265f;
        turning.update(.1f); require(turning.volleysFired == 0, "Bow fired backwards before turning");

        auto missed = bowDuel(); missed.update(1.0f / 60);
        missed.formations[1].organization.smallGroups[12].offsetZ = 6;
        missed.update(1.1f);
        require(missed.volleysHit == 0 && missed.formations[1].strength == 500, "Arrow tracked a moving target");

        auto blocked = bowDuel();
        auto& friendGroup = blocked.formations[0].organization.smallGroups[13];
        friendGroup.offsetX = 2 - blocked.formations[0].x - 5.2f; friendGroup.offsetZ = 0;
        blocked.update(.1f); require(blocked.volleysFired == 0, "Bow fired through a blocked launch path");
        auto intercepted = bowDuel(); intercepted.update(1.0f / 60);
        auto& movingFriend = intercepted.formations[0].organization.smallGroups[13];
        movingFriend.offsetX = 20 - intercepted.formations[0].x - 5.2f; movingFriend.offsetZ = 0;
        intercepted.update(1.1f);
        require(intercepted.volleysHit == 0 && intercepted.formations[1].strength == 500,
            "Arrow passed through a friendly group entering its landing point");

        auto routed = bowDuel(); routed.update(1.0f / 60);
        auto& shooter = routed.formations[0].organization.smallGroups[12];
        shooter.routed = true; shooter.fleeX = shooter.fleeTargetX = -10; shooter.fleeZ = shooter.fleeTargetZ = 0;
        routed.update(1.1f);
        require(routed.volleysFired == 1 && routed.volleysHit == 1, "Routing recalled a launched arrow or fired another");

        auto batched = bowDuel(), split = batched;
        batched.update(6); for (unsigned frame = 0; frame < 180; ++frame) split.update(1.0f / 30);
        require(batched.volleysFired == split.volleysFired && batched.volleysHit == split.volleysHit &&
            std::abs(batched.formations[1].strength - split.formations[1].strength) < .0001f,
            "Arrow results depend on frame interval");

        auto visualBattle = bowDuel(); SoldierVisuals visuals; visuals.update(visualBattle);
        bool shotShown = false;
        for (unsigned frame = 0; frame < 40; ++frame) {
            visualBattle.update(1.0f / 30); visuals.update(visualBattle);
            for (unsigned id = 0; id < SoldierVisuals::perTeam; ++id)
                shotShown |= visuals.soldiers[id].smallGroup == 12 && visuals.soldiers[id].attacking;
        }
        bool fallen = false;
        for (unsigned id = SoldierVisuals::perTeam; id < visuals.soldiers.size(); ++id) {
            const auto& soldier = visuals.soldiers[id];
            if (soldier.life != SoldierLife::Alive) { require(soldier.smallGroup == 12, "Ranged deaths leaked into another group"); fallen = true; }
        }
        require(shotShown && fallen, "Ranged animation or impact casualties were missing");
        BattleSimulation mixed; mixed.reset(UnitType::Spearman, true); mixed.running = true;
        require(mixed.formations[0].x == -6 && mixed.formations[1].x == 6,
            "Mixed armies lost their staggered starting positions");
        require(mixed.formations[0].groupUnit(20) == UnitType::Samurai &&
            mixed.formations[1].groupUnit(0) == UnitType::Spearman && mixed.formations[1].groupUnit(17) == UnitType::Samurai,
            "Mixed armies lost their distinct front and reserve deployments");
        mixed.update(20);
        std::cout << "Mixed volleys: " << mixed.volleysFired << " fired, " << mixed.volleysHit << " hit\n";
        require(mixed.volleysFired > 0 && mixed.volleysHit > 0, "Mixed formation failed to use its archers");
        std::cout << "Bow range, flight, obstruction, misses, timing and display checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
