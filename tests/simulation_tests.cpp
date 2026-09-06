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
        require(x.state == y.state && x.movementBlocked == y.movementBlocked && x.detouring == y.detouring &&
            std::abs(x.waypointX - y.waypointX) < 0.001f && std::abs(x.waypointZ - y.waypointZ) < 0.001f &&
            x.detourBacking == y.detourBacking && std::abs(x.backtrackX - y.backtrackX) < 0.001f &&
            std::abs(x.backtrackZ - y.backtrackZ) < 0.001f &&
            x.detourExiting == y.detourExiting && std::abs(x.exitX - y.exitX) < 0.001f && std::abs(x.exitZ - y.exitZ) < 0.001f &&
            std::abs(x.x - y.x) < 0.001f && std::abs(x.z - y.z) < 0.001f &&
            std::abs(x.strength - y.strength) < 0.001f && std::abs(x.morale - y.morale) < 0.001f &&
            std::abs(x.cohesion - y.cohesion) < 0.001f, "Formation depends on update interval");
        for (unsigned id = 0; id < 25; ++id) {
            const auto& g = x.organization.smallGroups[id];
            const auto& h = y.organization.smallGroups[id];
            require(g.cavalry.phase==h.cavalry.phase && g.cavalry.handledCharge==h.cavalry.handledCharge &&
                g.cavalry.blocked==h.cavalry.blocked && g.cavalry.blockedSeconds==h.cavalry.blockedSeconds && g.cavalry.destination.x==h.cavalry.destination.x &&
                g.cavalry.destination.z==h.cavalry.destination.z,"Cavalry tactics depend on update interval");
            require(g.awareness.enemies == h.awareness.enemies && g.awareness.allies == h.awareness.allies &&
                g.awareness.cautious == h.awareness.cautious && g.awareness.decision == h.awareness.decision &&
                g.awareness.enemyStrength == h.awareness.enemyStrength && g.awareness.alliedStrength == h.awareness.alliedStrength,
                "Tactical awareness depends on update interval");
            require(std::abs(g.faceDeployment.accountedStrength - h.faceDeployment.accountedStrength) < 0.001f,
                "Deployment strength depends on update interval");
            for (unsigned face = 0; face < 4; ++face)
                require(std::abs(g.faceDeployment.deployed[face] - h.faceDeployment.deployed[face]) < 0.001f,
                    "Face deployment depends on update interval");
            require(g.state == h.state && std::abs(g.offsetX - h.offsetX) < 0.001f &&
                std::abs(g.offsetZ - h.offsetZ) < 0.001f && std::abs(g.fatigue - h.fatigue) < 0.001f &&
                g.route == h.route && g.routeLateral == h.routeLateral && g.flankClosing == h.flankClosing && g.slot == h.slot && g.routed == h.routed &&
                std::abs(g.fleeX - h.fleeX) < 0.001f && std::abs(g.fleeZ - h.fleeZ) < 0.001f &&
                g.fleeBlocked == h.fleeBlocked && g.fleeSide == h.fleeSide &&
                std::abs(g.routShock - h.routShock) < 0.001f &&
                g.attackTarget == h.attackTarget && g.canAttack == h.canAttack && g.combatWait == h.combatWait && std::abs(g.heading - h.heading) < 0.001f &&
                std::abs(g.approachX - h.approachX) < 0.001f && std::abs(g.approachZ - h.approachZ) < 0.001f &&
                g.detourTarget == h.detourTarget && std::abs(g.detourX - h.detourX) < 0.001f &&
                std::abs(g.detourZ - h.detourZ) < 0.001f &&
                std::abs(g.strength - h.strength) < 0.001f &&
                std::abs(g.morale - h.morale) < 0.001f, "Small group depends on update interval");
        }
        for (unsigned side = 0; side < x.organization.frontReliefs.size(); ++side) {
            const auto& g = x.organization.frontReliefs[side];
            const auto& h = y.organization.frontReliefs[side];
            require(g.front == h.front && g.reserve == h.reserve && g.phase == h.phase &&
                g.alongX == h.alongX && g.forward == h.forward && g.lateral == h.lateral &&
                g.corridorGroups == h.corridorGroups && g.corridorShift == h.corridorShift, "Front relief depends on update interval");
        }
    }
}
int main() {
    try {
        const auto duel = [](BattlePoint target) {
            BattleSimulation test;
            for (unsigned team = 0; team < 2; ++team) {
                auto& f = test.formations[team];
                f.x = f.targetX = team == 0 ? -40.0f : 40.0f;
                f.z = f.targetZ = 0;
                f.maneuverEnabled = false;
                for (auto& g : f.organization.smallGroups) {
                    g.offsetZ = 1000; g.heading = team == 0 ? 0 : 3.14159265f;
                }
                auto& g = f.organization.smallGroups[12];
                g.offsetX = (team == 0 ? 0 : target.x) - f.x;
                g.offsetZ = team == 0 ? 0 : target.z;
            }
            test.toggle(); return test;
        };
        for (const auto second : {BattlePoint{-7, 0}, BattlePoint{0, 7}, BattlePoint{6, 2.6f}}) {
            auto multiple = duel(second.x == 6 ? BattlePoint{6, -2.6f} : BattlePoint{7, 0});
            auto& attacker = multiple.formations[0].organization.smallGroups[12];
            attacker.faceDeployment.accountedStrength = 20;
            attacker.faceDeployment.deployed = {5, 5, 5, 5};
            auto& opponent = multiple.formations[1].organization.smallGroups[13];
            opponent.offsetX = second.x - multiple.formations[1].x - 5.2f;
            opponent.offsetZ = second.z;
            multiple.update(1.0f / 60);
            require(attacker.activeOpponents[12] > 0 && attacker.activeOpponents[13] > 0,
                "Multiple contact opponents did not participate simultaneously");
            require(multiple.formations[1].organization.smallGroups[12].strength < 20 && opponent.strength < 20,
                "Simultaneous contact damage missed an opponent");
            float sum = 0; for (float fighters : attacker.activeOpponents) sum += fighters;
            require(std::abs(sum - attacker.activeFighters) < .0001f && sum <= 20,
                "Simultaneous attacks duplicated strength");
            auto batched = multiple, split = multiple;
            batched.update(.5f); for (unsigned frame = 0; frame < 30; ++frame) split.update(1.0f / 60);
            same(batched, split);
        }
        BattleSimulation mixed;
        mixed.reset(UnitType::Spearman, true);
        for (const auto& formation : mixed.formations) {
            unsigned swords = 0;
            for (unsigned id = 0; id < 25; ++id) swords += formation.groupUnit(id) == UnitType::Samurai;
            unsigned archers = 0;
            for (unsigned id = 0; id < 25; ++id) archers += formation.groupUnit(id) == UnitType::Archer;
            require(swords == (&formation == &mixed.formations[0] ? 4u : 8u) && archers == 5 && formation.strength == 500, "Mixed composition lost its units or strength");
        }
        auto mixedRange = duel({7, 0});
        mixedRange.formations[0].organization.smallGroups[12].unit = UnitType::Samurai;
        mixedRange.update(.1f);
        require(mixedRange.formations[1].strength == 500 && mixedRange.formations[0].strength < 500,
            "Mixed units did not use their own melee range");
        auto distant = duel({7, 3}); distant.update(0.1f);
        for (unsigned condition = 0; condition < 3; ++condition) {
            auto marching = duel({6, 0});
            if (condition > 0) {
                auto& obstacle = marching.formations[condition - 1].organization.smallGroups[13];
                obstacle.routed = true;
                obstacle.fleeX = obstacle.fleeTargetX = 6;
                obstacle.fleeZ = obstacle.fleeTargetZ = 0;
                marching.formations[1].organization.smallGroups[12].offsetZ = 1000;
            }
            marching.move(0, -34, 0);
            marching.update(2);
            require(marching.formations[0].movementBlocked && !marching.formations[0].moving &&
                marching.formations[0].targetX == -34 && marching.formations[0].groupPosition(12).x <= 1.5001f,
                "Army move crossed a group or discarded its blocked command");
            const auto stopped = marching;
            marching.toggle(); marching.update(1); same(marching, stopped); marching.toggle();
            if (condition == 0) marching.formations[1].organization.smallGroups[12].offsetZ = 1000;
            else {
                auto& obstacle = marching.formations[condition - 1].organization.smallGroups[13];
                obstacle.fleeZ = obstacle.fleeTargetZ = 20;
            }
            const float stoppedX = marching.formations[0].x;
            marching.update(1);
            require(marching.formations[0].x > stoppedX + 1.7f && !marching.formations[0].movementBlocked,
                "Army move did not resume when its path cleared");
            marching.hold(0);
            require(!marching.formations[0].movementBlocked, "Hold retained a stale blockage");
            marching.reset(); same(marching, BattleSimulation{});
        }
        auto sweptMarch = duel({6, 0});
        sweptMarch.formations[0].speed = 1200;
        sweptMarch.move(0, -20, 0); sweptMarch.update(1.0f / 60);
        require(sweptMarch.formations[0].x == -40 && sweptMarch.formations[0].movementBlocked,
            "Fast army movement tunneled through a group");
        auto meeting = duel({6, 0});
        meeting.formations[0].speed = meeting.formations[1].speed = 60;
        meeting.move(0, -30, 0); meeting.move(1, 30, 0); meeting.update(1.0f / 60);
        require(meeting.formations[0].movementBlocked && meeting.formations[1].movementBlocked &&
            battleDistance(meeting.formations[0].groupPosition(12), meeting.formations[1].groupPosition(12)) >= 4.5f,
            "Simultaneous army movement favored one side or overlapped");
        BattleSimulation pushed;
        pushed.formations[0].z = -14; pushed.formations[1].z = 14;
        pushed.hold(0); pushed.move(1, 0, 0);
        auto& rearObstacle = pushed.formations[0].organization.smallGroups[0];
        rearObstacle.routed = true;
        rearObstacle.fleeX = rearObstacle.fleeTargetX = -10.4f;
        rearObstacle.fleeZ = rearObstacle.fleeTargetZ = -23.71f;
        pushed.toggle(); pushed.update(1.0f / 60);
        require(pushed.formations[0].z == -14 && pushed.formations[0].movementBlocked,
            "Army pushback crossed a rear obstacle");
        auto retreatBlock = duel({6, 0});
        retreatBlock.formations[0].state = FormationState::Retreating;
        retreatBlock.formations[0].targetX = -34;
        retreatBlock.result = BattleResult::BlueVictory;
        retreatBlock.update(2);
        require(retreatBlock.formations[0].movementBlocked && retreatBlock.formations[0].state == FormationState::Retreating &&
            retreatBlock.formations[0].groupPosition(12).x <= 1.5001f, "Post-battle retreat ignored collision");
        BattleSimulation armyDetour;
        armyDetour.formations[0].x = -40; armyDetour.formations[0].z = 0;
        armyDetour.formations[0].speed = 5;
        armyDetour.formations[1].x = armyDetour.formations[1].z = 40;
        armyDetour.hold(1);
        auto& roadblock = armyDetour.formations[0].organization.smallGroups[14];
        roadblock.routed = true;
        roadblock.fleeX = roadblock.fleeTargetX = -22;
        roadblock.fleeZ = roadblock.fleeTargetZ = 0;
        armyDetour.move(0, 0, 0); armyDetour.toggle();
        bool sawArmyDetour = false, armyArrived = false;
        for (unsigned tick = 0; tick < 2400; ++tick) {
            armyDetour.update(1.0f / 60);
            const auto& f = armyDetour.formations[0];
            for (unsigned id = 0; id < 25; ++id) if (id != 14)
                require(battleDistance(f.groupPosition(id), f.groupPosition(14)) >= 4.5f,
                    "Army detour crossed a routed group");
            if (f.detouring && !sawArmyDetour) {
                sawArmyDetour = true;
                auto pausedArmy = armyDetour;
                pausedArmy.toggle(); pausedArmy.update(2); same(pausedArmy, armyDetour);
                auto heldArmy = armyDetour;
                heldArmy.hold(0); const float heldX = heldArmy.formations[0].x, heldZ = heldArmy.formations[0].z;
                heldArmy.update(1);
                require(!heldArmy.formations[0].detouring && heldArmy.formations[0].x == heldX && heldArmy.formations[0].z == heldZ,
                    "Hold did not cancel an army detour");
                heldArmy.move(0, -40, -20);
                require(!heldArmy.formations[0].detouring && heldArmy.formations[0].targetZ == -20,
                    "New move retained an old army waypoint");
                auto batched = armyDetour, split = armyDetour;
                batched.update(1);
                for (unsigned frame = 0; frame < 60; ++frame) split.update(1.0f / 60);
                same(batched, split);
            }
            if (battleDistance({f.x, f.z}, {0, 0}) < 0.001f) { armyArrived = true; break; }
        }
        require(sawArmyDetour && armyArrived, "Full army did not detour to its original destination");
        armyDetour.reset(); same(armyDetour, BattleSimulation{});
        require(distant.formations[0].strength == 500 && distant.formations[1].strength == 500,
            "Rectangular contact caused damage outside physical range");
        auto frontal = duel({7, 0}); frontal.update(0.1f);
        auto undeployed=duel({7,0});undeployed.update(1.0f/60);
        require(undeployed.formations[1].strength==500,"Undeployed reserves dealt instant damage");
        auto prepared=duel({7,0}),deep=duel({7,0}),partial=duel({7,0});
        for(auto* fixture:{&prepared,&deep,&partial}) {
            auto& g=fixture->formations[0].organization.smallGroups[12];
            g.faceDeployment.accountedStrength=20;g.faceDeployment.deployed[0]=fixture==&partial?2.0f:5.0f;
        }
        deep.formations[0].strength+=20;deep.formations[0].organization.smallGroups[12].strength+=20;
        prepared.update(.1f);deep.update(.1f);partial.update(.1f);
        require(std::abs(prepared.formations[1].strength-deep.formations[1].strength)<.001f,
            "Adding reserves increased damage through unchanged frontage");
        require(partial.formations[1].strength>prepared.formations[1].strength && prepared.formations[1].strength<500,
            "Deployed fighters did not control actual damage");
        require(frontal.formations[0].strength < 500 && frontal.formations[1].strength < 500,
            "Nearby groups outside army contact failed to attack");
        auto rearAttack = duel({7, 0});
        rearAttack.formations[1].organization.smallGroups[12].heading = 0;
        rearAttack.update(0.1f);
        require(rearAttack.formations[0].strength == 500 && rearAttack.formations[1].strength < frontal.formations[1].strength,
            "Rear-facing group attacked instantly or ignored attack direction");
        auto sideAttack = duel({7, 0});
        sideAttack.formations[1].organization.smallGroups[12].heading = 1.57079633f;
        sideAttack.update(0.1f);
        require(sideAttack.formations[1].strength < frontal.formations[1].strength &&
            sideAttack.formations[1].strength > rearAttack.formations[1].strength, "Side attack ignored relative facing");
        rearAttack.update(1);
        require(rearAttack.formations[0].strength < 500, "Group did not turn toward its attacker");
        auto screened = duel({7, 0});
        auto& screen = screened.formations[0].organization.smallGroups[13];
        screen.offsetX = 3.5f - screened.formations[0].x - 5.2f; screen.offsetZ = 0;
        screen.route = SmallGroupRoute::Returning;
        screened.update(0.1f);
        require(screened.formations[1].strength == 500, "Attack passed through another group");
        require(screened.formations[0].organization.smallGroups[12].combatWait == CombatWait::Obstructed,
            "Blocked attack did not report its reason");
        auto alternative = duel({6, 0});
        auto& shield = alternative.formations[0].organization.smallGroups[13];
        shield.offsetX = 3 - alternative.formations[0].x - 5.2f; shield.offsetZ = 0;
        shield.route = SmallGroupRoute::Returning;
        auto& otherEnemy = alternative.formations[1].organization.smallGroups[13];
        otherEnemy.offsetX = -alternative.formations[1].x - 5.2f; otherEnemy.offsetZ = 7;
        alternative.formations[0].organization.smallGroups[12].heading = 1.57079633f;
        alternative.update(0.1f);
        require(alternative.formations[0].organization.smallGroups[12].attackTarget == 13 &&
            alternative.formations[0].organization.smallGroups[12].canAttack && otherEnemy.strength < 20,
            "Group stayed idle behind an obstruction despite an accessible enemy");
        auto closeIn = duel({12, 0});
        closeIn.formations[0].maneuverEnabled = true;
        closeIn.formations[1].maneuverEnabled = true;
        closeIn.update(0.5f);
        require(battleDistance(closeIn.formations[0].groupPosition(12), closeIn.formations[1].groupPosition(12)) < 12 &&
            closeIn.formations[0].strength == 500 && closeIn.formations[1].strength == 500,
            "Approach did not move or inflicted damage before reaching range");
        closeIn.update(2);
        require(closeIn.formations[0].strength < 500 && closeIn.formations[1].strength < 500,
            "Approaching groups did not attack in range");
        require(battleDistance(closeIn.formations[0].groupPosition(12), closeIn.formations[1].groupPosition(12)) >= 4.5f,
            "Approaching groups overlapped");
        for (unsigned direction = 0; direction < 8; ++direction) {
            const float angle = direction * 3.14159265f / 4;
            auto oriented = duel({12 * std::cos(angle), 12 * std::sin(angle)});
            for (unsigned team = 0; team < 2; ++team) {
                oriented.formations[team].maneuverEnabled = true;
                oriented.formations[team].organization.smallGroups[12].heading = angle + team * 3.14159265f;
            }
            oriented.update(2.5f);
            require(oriented.formations[0].strength < 500 && oriented.formations[1].strength < 500 &&
                battleDistance(oriented.formations[0].groupPosition(12), oriented.formations[1].groupPosition(12)) >= 4.5f,
                "Approach, attack or collision depended on a cardinal direction");
        }
        auto blockedApproach = duel({12, 0});
        blockedApproach.formations[0].maneuverEnabled = true;
        auto& blocker = blockedApproach.formations[0].organization.smallGroups[13];
        blocker.offsetX = 5.2f - blockedApproach.formations[0].x - 5.2f; blocker.offsetZ = 0;
        const auto beforeBlocked = blockedApproach.formations[0].groupPosition(12);
        blockedApproach.update(0.1f);
        require(blockedApproach.formations[0].groupPosition(12).x == beforeBlocked.x,
            "Approach crossed an occupied path");
        require(blockedApproach.formations[0].organization.smallGroups[12].combatWait == CombatWait::Detouring,
            "Blocked movement did not start a detour");
        auto pausedDetour = blockedApproach;
        pausedDetour.toggle();
        const auto frozenDetour = pausedDetour;
        pausedDetour.update(1);
        same(pausedDetour, frozenDetour);
        bool reachedEnemy = false;
        for (unsigned tick = 0; tick < 900; ++tick) {
            blockedApproach.update(1.0f / 60);
            require(battleDistance(blockedApproach.formations[0].groupPosition(12),
                blockedApproach.formations[0].groupPosition(13)) >= 4.5f, "Detour crossed its blocker");
            if (blockedApproach.formations[0].organization.smallGroups[12].canAttack) { reachedEnemy = true; break; }
        }
        require(reachedEnemy, "Detour never reached attack range");
        auto enclosed = duel({12, 0});
        enclosed.formations[0].maneuverEnabled = true;
        for (unsigned index = 0; index < 8; ++index) {
            auto& obstacle = enclosed.formations[0].organization.smallGroups[index];
            const float angle = index * 3.14159265f / 4;
            obstacle.offsetX = 5.2f * std::cos(angle) - enclosed.formations[0].x - (static_cast<int>(index % 5) - 2) * 5.2f;
            obstacle.offsetZ = 5.2f * std::sin(angle) - (static_cast<int>(index / 5) - 2) * 5.2f;
        }
        enclosed.update(1.0f / 60);
        require(enclosed.formations[0].organization.smallGroups[12].combatWait == CombatWait::PathBlocked &&
            battleDistance(enclosed.formations[0].groupPosition(12), {0, 0}) < 0.001f,
            "Enclosed group crossed blocked detour exits");
        require(segmentDistance({0, 0}, {10, 0}, {5, 1}) == 1 &&
            segmentDistance({0, 0}, {0, 0}, {3, 4}) == 5, "Swept collision distance is invalid");
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
            float remaining = 0;
            for (const auto& g : battle.formations[team].organization.smallGroups) {
                remaining += g.strength;
                if (g.state == SmallGroupState::Engaged)
                    require(g.strength < 20 && g.morale < (team == 0 ? 100 : 85), "Contact group did not take damage");
                else require(g.strength == 20 && g.morale == (team == 0 ? 100 : 85), "Rear reserve took combat damage");
            }
            require(std::abs(remaining - battle.formations[team].strength) < 0.05f, "Small-group losses do not match formation losses");
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
        // 予備が残る戦闘は継続できる。命令拒否の検証には全小組が崩れた状態を明示する。
        if (battle.result == BattleResult::Ongoing) {
            for (auto& group : battle.formations[1].organization.smallGroups) group.morale = 0;
            battle.update(7);
        }
        require(battle.result != BattleResult::Ongoing, "Exhausted battle did not finish with a retreat");
        const unsigned loser = battle.formations[1].defeated() ? 1u : 0u;
        const auto defeated = battle.formations[loser];
        auto withoutOrders = battle;
        battle.move(loser, 0, 0); battle.hold(loser); battle.update(10);
        withoutOrders.update(10); same(battle, withoutOrders);
        require(battle.formations[loser].targetZ == defeated.targetZ &&
            battle.formations[loser].strength == defeated.strength, "Defeated formation obeyed orders or took post-battle losses");
        BattleSimulation thirty, oneFortyFour, single;
        thirty.toggle(); oneFortyFour.toggle(); single.toggle();
        for (int i = 0; i < 2400; ++i) thirty.update(1.0f / 30);
        for (int i = 0; i < 11520; ++i) oneFortyFour.update(1.0f / 144);
        single.update(80); same(thirty, oneFortyFour); same(thirty, single);
        BattleSimulation equal;
        equal.formations[1].morale = 100;
        for (auto& g : equal.formations[1].organization.smallGroups) g.morale = 100;
        equal.toggle(); equal.update(80);
        require((equal.result == BattleResult::Draw || equal.result == BattleResult::Ongoing) &&
            equal.formations[0].defeated() == equal.formations[1].defeated() &&
            std::abs(equal.formations[0].strength - equal.formations[1].strength) < .001f &&
            std::abs(equal.formations[0].z + equal.formations[1].z) < 0.001f,
            "Symmetric battle favored update order");
        BattleSimulation reversed;
        reversed.formations[0].morale = 85; reversed.formations[1].morale = 100;
        for (auto& f : reversed.formations) for (auto& g : f.organization.smallGroups) g.morale = f.morale;
        reversed.toggle(); reversed.update(80);
        const auto reversedResult = single.result == BattleResult::RedVictory ? BattleResult::BlueVictory :
            single.result == BattleResult::BlueVictory ? BattleResult::RedVictory : single.result;
        require(reversed.result == reversedResult, "Result is hardcoded to a team");
        BattleSimulation separated;
        separated.move(0, -45, -22); separated.move(1, 45, 22); separated.toggle(); separated.update(80);
        require(separated.result == BattleResult::Ongoing && separated.formations[0].strength == 500 &&
            separated.formations[1].strength == 500, "Non-contact formations fought");
        BattleSimulation isolated;
        isolated.hold(0); isolated.hold(1);
        isolated.formations[0].organization.smallGroups[12].morale = 20;
        isolated.toggle(); isolated.update(1.0f / 60);
        require(isolated.formations[0].routedGroups() == 1 && isolated.result == BattleResult::Ongoing &&
            !isolated.formations[0].defeated(), "A single rout forced an army retreat");
        require(isolated.formations[0].organization.smallGroups[11].morale == 100,
            "New rout propagated recursively in the same step");
        require(isolated.nearbyRouts(0, 11) == 1 && isolated.nearbyRouts(0, 6) == 1 &&
            isolated.nearbyRouts(0, 0) == 0 && isolated.nearbyRouts(0, 12) == 0 &&
            isolated.nearbyRouts(1, 11) == 0, "Rout status included distant, routed or opposing groups");
        auto exchanged = isolated;
        std::swap(exchanged.formations[0].organization.smallGroups[0].slot,
            exchanged.formations[0].organization.smallGroups[11].slot);
        require(exchanged.nearbyRouts(0, 0) == 1 && exchanged.nearbyRouts(0, 11) == 0,
            "Rout status followed membership instead of exchanged slots");
        exchanged.result = BattleResult::RedVictory;
        require(exchanged.nearbyRouts(0, 0) == 0, "Finished battle retained active rout pressure");
        auto physicalShock = isolated;
        auto& shockFormation = physicalShock.formations[0];
        auto& shockSource = shockFormation.organization.smallGroups[12];
        const auto receiver = shockFormation.groupPosition(11);
        shockSource.fleeX = receiver.x + 8; shockSource.fleeZ = receiver.z;
        require(physicalShock.nearbyRouts(0, 11) == 1, "Shock excluded its radius boundary");
        shockSource.fleeX = receiver.x + 8.01f;
        require(physicalShock.nearbyRouts(0, 11) == 0, "Distant runner affected its old neighboring slot");
        shockSource.fleeX = receiver.x + 6; shockSource.fleeZ = receiver.z + 6;
        require(physicalShock.nearbyRouts(0, 11) == 0, "Shock used rectangular rather than radial distance");
        const auto distantSlot = shockFormation.groupPosition(0);
        shockSource.fleeX = distantSlot.x + 6; shockSource.fleeZ = distantSlot.z;
        require(physicalShock.nearbyRouts(0, 0) == 1, "Nearby runner ignored a different original slot");
        shockFormation.organization.smallGroups[0].approachX = -10;
        require(physicalShock.nearbyRouts(0, 0) == 0, "Shock ignored the receiver's approach displacement");
        const auto isolatedPause = isolated;
        isolated.toggle(); isolated.update(5); same(isolated, isolatedPause);
        isolated.toggle(); isolated.update(1);
        require(isolated.formations[0].organization.smallGroups[11].morale < 100 &&
            isolated.formations[0].organization.smallGroups[0].morale == 100,
            "Rout shock skipped neighbors or reached a distant slot");
        const auto fleeing = isolated.formations[0].organization.smallGroups[12];
        isolated.hold(0); isolated.update(1);
        require(isolated.formations[0].organization.smallGroups[12].routed &&
            isolated.formations[0].organization.smallGroups[12].fleeTargetZ == fleeing.fleeTargetZ,
            "Hold recalled a fleeing group");
        auto freeEscape = duel({12, 0});
        freeEscape.formations[0].organization.smallGroups[12].morale = 20;
        freeEscape.update(1.0f / 60);
        const auto freeStart = freeEscape.formations[0].groupPosition(12);
        freeEscape.hold(0); freeEscape.update(1);
        require(freeEscape.formations[0].groupPosition(12).x < freeStart.x - 2.6f,
            "Hold stopped a runner on an open path");
        auto movedArmy = freeEscape;
        movedArmy.move(0, 40, -22);
        freeEscape.update(1); movedArmy.update(1);
        const auto& freeRunner = freeEscape.formations[0].organization.smallGroups[12];
        const auto& movingRunner = movedArmy.formations[0].organization.smallGroups[12];
        require(freeRunner.fleeX == movingRunner.fleeX && freeRunner.fleeZ == movingRunner.fleeZ,
            "Army movement dragged a fleeing group");
        for (unsigned obstacleTeam = 0; obstacleTeam < 2; ++obstacleTeam) {
            auto diverted = duel({12, 0});
            auto& obstacle = diverted.formations[obstacleTeam].organization.smallGroups[13];
            obstacle.offsetX = -6 - diverted.formations[obstacleTeam].x - 5.2f; obstacle.offsetZ = 0;
            diverted.formations[0].organization.smallGroups[12].morale = 20;
            bool wentAround = false;
            for (unsigned tick = 0; tick < 600; ++tick) {
                diverted.update(1.0f / 60);
                const auto runner = diverted.formations[0].groupPosition(12);
                require(battleDistance(runner, diverted.formations[obstacleTeam].groupPosition(13)) >= 4.5f,
                    "Runner crossed an allied or enemy group");
                wentAround |= runner.x < -7;
            }
            require(wentAround, "Runner failed to pass an isolated obstacle");
        }
        auto trapped = duel({12, 0});
        for (unsigned id = 0; id < 6; ++id) {
            auto& wall = trapped.formations[0].organization.smallGroups[id];
            const float angle = id * 3.14159265f / 3;
            wall.offsetX = 5.2f * std::cos(angle) - trapped.formations[0].x - (static_cast<int>(id % 5) - 2) * 5.2f;
            wall.offsetZ = 5.2f * std::sin(angle) - (static_cast<int>(id / 5) - 2) * 5.2f;
        }
        trapped.formations[0].organization.smallGroups[12].morale = 20;
        trapped.update(2);
        require(trapped.formations[0].organization.smallGroups[12].fleeBlocked &&
            battleDistance(trapped.formations[0].groupPosition(12), {0, 0}) < 1,
            "Runner escaped through a closed ring");
        const auto trappedPause = trapped;
        trapped.toggle(); trapped.update(1); same(trapped, trappedPause); trapped.toggle();
        trapped.formations[0].organization.smallGroups[3].offsetZ = 1000;
        trapped.formations[0].organization.smallGroups[2].offsetZ = 1000;
        trapped.update(4);
        require(trapped.formations[0].groupPosition(12).x < -7 &&
            !trapped.formations[0].organization.smallGroups[12].fleeBlocked, "Runner did not use a newly opened exit");
        auto crossingRunners = duel({12, 0});
        for (unsigned team = 0; team < 2; ++team) {
            auto& runner = crossingRunners.formations[team].organization.smallGroups[12];
            runner.routed = true; runner.fleeX = team == 0 ? -6.0f : 6.0f;
            runner.fleeZ = 0; runner.fleeTargetX = -runner.fleeX * 5; runner.fleeTargetZ = 0;
        }
        auto batchedRunners = crossingRunners;
        for (unsigned tick = 0; tick < 240; ++tick) {
            crossingRunners.update(1.0f / 60);
            require(battleDistance(crossingRunners.formations[0].groupPosition(12),
                crossingRunners.formations[1].groupPosition(12)) >= 4.5f, "Two runners overlapped");
        }
        batchedRunners.update(4); same(crossingRunners, batchedRunners);
        isolated.update(20);
        require(isolated.nearbyRouts(0, 11) == 0, "Expired rout shock remained visible");
        require(isolated.formations[0].routedGroups() == 1 && isolated.result == BattleResult::Ongoing &&
            isolated.formations[0].strength == 500, "An isolated shock caused inevitable collapse or casualties");
        isolated.reset(); same(isolated, BattleSimulation{});
        BattleSimulation chain;
        chain.hold(0); chain.hold(1);
        for (auto& g : chain.formations[0].organization.smallGroups) g.morale = 35;
        chain.formations[0].organization.smallGroups[12].morale = 20;
        chain.toggle(); chain.update(1.0f / 60);
        require(chain.formations[0].routedGroups() == 1, "Chain routed every group at once");
        chain.update(3);
        require(chain.formations[0].routedGroups() > 1 && chain.formations[0].routedGroups() < 13 &&
            chain.result == BattleResult::Ongoing, "Local rout did not spread gradually");
        chain.update(20);
        require(chain.formations[0].routedGroups() >= 13 && chain.result == BattleResult::BlueVictory &&
            chain.formations[0].defeated() && chain.formations[0].strength == 500,
            "Accumulated small-group routs did not trigger army withdrawal");
        BattleSimulation cohesive;
        cohesive.hold(0); cohesive.hold(1);
        cohesive.formations[0].morale = 0; cohesive.formations[0].cohesion = 0;
        cohesive.toggle(); cohesive.update(1);
        require(cohesive.result == BattleResult::Ongoing && cohesive.formations[0].routedGroups() == 0,
            "Aggregate morale bypassed small-group routs");
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
                for (auto& g : f.organization.smallGroups) g.morale = 100;
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
            flank.toggle();
            // 回り込みの経路を検証する間は、前衛を健全に保って別の交代を開始させない。
            for (unsigned tick = 0; tick < 360; ++tick) {
                for (unsigned team = 0; team < 2; ++team) for (auto& g : flank.formations[team].organization.smallGroups)
                    if ((alongX ? g.slot % 5 : g.slot / 5) == (team == 0 ? 4u : 0u)) {
                        g.fatigue = 0; g.strength = 20; g.morale = 100;
                    }
                flank.update(1.0f / 60);
            }
            const auto advanced = flank.formations[0].organization.smallGroups[id];
            require(std::abs((alongX ? advanced.offsetZ : advanced.offsetX) + 6.1f) < 0.001f &&
                (alongX ? advanced.offsetX : advanced.offsetZ) > 3, "Flank did not advance outside its formation");
            require(advanced.state == SmallGroupState::Engaged, "Flank did not stop at enemy contact");
            auto relief = flank;
            bool returned = false, replaced = false, withdrew = false;
            for (unsigned tick = 0; tick < 1500; ++tick) {
                // このケースでは正面交代を発生させず、両翼の帰還と再出撃を単独で検証する。
                for (unsigned team = 0; team < 2; ++team) for (auto& g : relief.formations[team].organization.smallGroups)
                    if ((alongX ? g.slot % 5 : g.slot / 5) == (team == 0 ? 4u : 0u)) {
                        g.fatigue = 0; g.strength = 20; g.morale = 100;
                    }
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
        for (bool alongX : {false, true}) for (float sign : {-1.0f, 1.0f}) {
            BattleSimulation rotation;
            for (unsigned team = 0; team < 2; ++team) {
                auto& f = rotation.formations[team];
                const float center = (team == 0 ? -14.0f : 14.0f) * sign;
                f.x = f.targetX = alongX ? center : 0;
                f.z = f.targetZ = alongX ? 0 : center;
                f.morale = 100;
                for (auto& g : f.organization.smallGroups) g.morale = 100;
                const unsigned rank = (team == 0 ? sign : -sign) > 0 ? 4 : 0;
                for (unsigned lane : {0u, 4u}) {
                    const unsigned id = alongX ? lane * 5 + rank : rank * 5 + lane;
                    f.organization.smallGroups[id].fatigue = 8;
                    f.organization.smallGroups[id].strength = 19;
                    f.strength -= 1;
                }
            }
            rotation.toggle(); rotation.update(1);
            const auto active = rotation;
            auto failedRelief = active;
            const auto reservation = failedRelief.formations[0].organization.frontReliefs[0];
            require(reservation.front >= 0, "Front relief fixture did not reserve a pair");
            auto withdrawing = active;
            withdrawing.formations[0].organization.smallGroups[reservation.front].morale = 18;
            withdrawing.update(1.0f / 60);
            require(!withdrawing.formations[0].organization.smallGroups[reservation.front].routed &&
                withdrawing.formations[0].organization.smallGroups[reservation.front].route == SmallGroupRoute::ReliefWithdraw,
                "A recoverable morale dip turned an orderly relief into a rout");
            failedRelief.formations[0].organization.smallGroups[reservation.front].morale = 10;
            failedRelief.update(1.0f / 60);
            require(failedRelief.formations[0].organization.smallGroups[reservation.front].routed &&
                failedRelief.formations[0].organization.frontReliefs[0].front < 0 &&
                failedRelief.formations[0].organization.smallGroups[reservation.reserve].route == SmallGroupRoute::Returning,
                "Rout left a stale front relief reservation");
            rotation.toggle(); rotation.update(2); same(rotation, active);
            rotation.toggle(); rotation.hold(0);
            const auto held = rotation.formations[0].organization;
            rotation.update(1);
            for (unsigned id = 0; id < 25; ++id) {
                const auto& g = rotation.formations[0].organization.smallGroups[id];
                require(g.offsetX == held.smallGroups[id].offsetX && g.offsetZ == held.smallGroups[id].offsetZ,
                    "Hold moved a front relief participant");
            }
            rotation = active;
            std::array<std::array<float, 25>, 2> peakFatigue{};
            for (unsigned tick = 0; tick < 780; ++tick) {
                rotation.update(1.0f / 60);
                for (unsigned team = 0; team < 2; ++team) for (unsigned id = 0; id < 25; ++id)
                    peakFatigue[team][id] = std::max(peakFatigue[team][id], rotation.formations[team].organization.smallGroups[id].fatigue);
                for (const auto& f : rotation.formations) {
                    std::array<bool, 25> slots{};
                    for (unsigned id = 0; id < 25; ++id) {
                        const auto& g = f.organization.smallGroups[id];
                        require(g.slot < 25 && !slots[g.slot], "Front relief duplicated a formation slot");
                        slots[g.slot] = true;
                        require(g.company == id / 5, "Front relief changed group membership");
                        const float x = (static_cast<float>(g.slot % 5) - 2) * 5.2f + g.offsetX;
                        const float z = (static_cast<float>(g.slot / 5) - 2) * 5.2f + g.offsetZ;
                        for (unsigned other = 0; other < id; ++other) {
                            const auto& h = f.organization.smallGroups[other];
                            if (g.routed || h.routed) continue;
                            const float hx = (static_cast<float>(h.slot % 5) - 2) * 5.2f + h.offsetX;
                            const float hz = (static_cast<float>(h.slot / 5) - 2) * 5.2f + h.offsetZ;
                            require(std::abs(x - hx) >= 4.5f || std::abs(z - hz) >= 4.5f,
                                "Front relief passed through another group");
                        }
                    }
                }
            }
            for (unsigned team = 0; team < 2; ++team) {
                const auto& groups = rotation.formations[team].organization.smallGroups;
                const unsigned rank = (team == 0 ? sign : -sign) > 0 ? 4 : 0;
                const unsigned rearRank = rank == 4 ? 3 : 1;
                for (unsigned lane : {0u, 4u}) {
                    const unsigned front = alongX ? lane * 5 + rank : rank * 5 + lane;
                    const unsigned rear = alongX ? lane * 5 + rearRank : rearRank * 5 + lane;
                    require(groups[front].slot == rear && groups[rear].slot == front,
                        "Front and reserve did not exchange slots");
                    require(groups[front].strength < 20 && groups[front].fatigue < peakFatigue[team][front],
                        "Relieved veteran lost its damage or failed to rest");
                }
            }
            auto departed = active;
            departed.move(0, alongX ? -55 * sign : 0, alongX ? 0 : -55 * sign);
            departed.hold(1); departed.update(20);
            for (const auto& relief : departed.formations[0].organization.frontReliefs)
                require(relief.front < 0, "Disengagement stranded a front relief");
            auto finishedBattle = active;
            for (auto& group : finishedBattle.formations[1].organization.smallGroups) group.morale = 10;
            finishedBattle.update(20);
            for (const auto& f : finishedBattle.formations) for (const auto& relief : f.organization.frontReliefs)
                require(relief.front < 0, "Battle end stranded a front relief");
            rotation.reset(); same(rotation, BattleSimulation{});
        }
        for (unsigned condition = 0; condition < 3; ++condition) {
            BattleSimulation unfitRelief;
            auto& groups = unfitRelief.formations[0].organization.smallGroups;
            groups[20].fatigue = 8;
            if (condition == 0) groups[15].fatigue = 20;
            if (condition == 1) groups[15].strength = 10;
            if (condition == 2) groups[15].morale = 30;
            unfitRelief.toggle(); unfitRelief.update(6);
            require(groups[20].route == SmallGroupRoute::None && groups[20].slot == 20,
                "Front relief selected an unfit reserve");
        }
        BattleSimulation blocked;
        blocked.formations[0].z = blocked.formations[0].targetZ = -14;
        blocked.formations[1].z = blocked.formations[1].targetZ = 14;
        // 通路の小組が迂回で動いても、追い越し側が重ならないことを確認する。
        blocked.formations[0].organization.smallGroups[10].offsetX = -5.2f;
        blocked.formations[0].organization.smallGroups[10].offsetZ = 5.2f;
        blocked.toggle();
        for (unsigned tick = 0; tick < 120; ++tick) {
            blocked.update(1.0f / 60);
            require(battleDistance(blocked.formations[0].groupPosition(10),
                blocked.formations[0].groupPosition(15)) >= 4.5f, "Flank passed through an occupied route");
        }
        auto blockedRelief = blocked;
        blockedRelief.formations[0].organization.smallGroups[20].fatigue = 8;
        for (unsigned tick = 0; tick < 120; ++tick) {
            blockedRelief.update(1.0f / 60);
            const auto& heldFront = blockedRelief.formations[0].organization.smallGroups[20];
            const auto& relief = blockedRelief.formations[0].organization.frontReliefs[0];
            require(relief.front < 0 || relief.phase > 0 || (heldFront.offsetX == 0 && heldFront.offsetZ == 0),
                "Front withdrew before the blocked reserve cleared its slot");
            require(battleDistance(blockedRelief.formations[0].groupPosition(20),
                blockedRelief.formations[0].groupPosition(15)) >= 4.5f, "Relief overlapped its front");
        }
        BattleSimulation edge;
        BattleSimulation depleted;
        for (unsigned team = 0; team < 2; ++team) {
            auto& f = depleted.formations[team];
            f.z = f.targetZ = team == 0 ? -14.0f : 14.0f;
            f.maneuverEnabled = false;
        }
        for (unsigned id = 20; id < 25; ++id) depleted.formations[0].organization.smallGroups[id].strength = 0.01f;
        depleted.formations[0].strength = 400.05f;
        depleted.toggle(); depleted.update(2);
        float remainingFront = 0;
        for (unsigned id = 0; id < 25; ++id) {
            const auto& g = depleted.formations[0].organization.smallGroups[id];
            require(id < 20 ? g.strength == 20.0f : g.strength >= 0 && g.strength <= .01f,
                "Group strength underflowed or rear took overflow damage");
            if (id >= 20) { remainingFront += g.strength; require(g.routed || g.strength == 0,"Exhausted front kept fighting"); }
        }
        // 配置に時間が必要なため、敵の攻撃開始前に敗走した端数兵力は生存しうる。
        require(std::abs(depleted.formations[0].strength - 400 - remainingFront) < .001f,"Exhausted front passed damage into reserves");
        for (bool lowMorale : {false, true}) {
            BattleSimulation reserves;
            auto& unfit = reserves.formations[0].organization.smallGroups[15];
            if (lowMorale) unfit.morale = 30; else unfit.strength = 10;
            reserves.toggle(); reserves.update(5);
            require(unfit.route == SmallGroupRoute::None && reserves.formations[0].organization.smallGroups[10].route != SmallGroupRoute::None,
                "Unfit group was selected instead of a fresh reserve");
        }
        // 地形端で接する備へ移動する。初期状態から小組を重ねた配置にはしない。
        edge.formations[0].x = 60; edge.formations[1].x = 32;
        edge.formations[0].z = edge.formations[1].z = 60;
        edge.hold(0); edge.hold(1); edge.move(1, 60, 60); edge.toggle(); edge.update(1);
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
