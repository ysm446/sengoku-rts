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
std::unique_ptr<BattleSimulation> afterCharge(UnitType enemy=UnitType::Samurai) {
    auto battle=duel(5.5f,enemy);
    auto& g=battle->formations[0].organization.smallGroups[12];
    g.charges=1;g.lastCharge=-1;g.chargeCooldown=7;
    return battle;
}
void placeSupport(BattleSimulation& battle,float x,float z) {
    auto& f=battle.formations[0];auto& support=f.organization.smallGroups[13];
    support.offsetX=x-f.x-5.2f;support.offsetZ=z-f.z;
    support.resting=true;support.fatigue=20;
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
        auto repeat=std::make_unique<BattleSimulation>(*battle);
        bool withdrew=false,regrouped=false;
        for(unsigned tick=0;tick<2400;++tick) {
            repeat->update(1.0f/60);
            const auto& rider=repeat->formations[0].organization.smallGroups[12];
            withdrew|=rider.cavalry.phase==CavalryPhase::Disengaging;
            regrouped|=rider.cavalry.phase==CavalryPhase::Regrouping;
            if(rider.cavalry.phase!=CavalryPhase::None) require(!rider.canAttack && rider.chargeDistance==0,"Withdrawing cavalry attacked or gained run-up");
            require(battleDistance(repeat->formations[0].groupPosition(12),repeat->formations[1].groupPosition(12))>=4.5f,"Withdrawing cavalry crossed enemy");
            if(rider.charges>=2)break;
        }
        const auto& repeated=repeat->formations[0].organization.smallGroups[12];
        std::cout<<"repeat charges="<<repeated.charges<<" phase="<<static_cast<unsigned>(repeated.cavalry.phase)<<" morale="<<repeated.morale<<'\n';
        require(withdrew && regrouped && repeated.charges>=2,"Cavalry failed to disengage and charge again");
        for(const auto enemy:{UnitType::Samurai,UnitType::Spearman,UnitType::Archer}) {
            auto supported=afterCharge(enemy);placeSupport(*supported,-2,7);
            supported->formations[0].organization.smallGroups[13].resting=false;
            supported->update(1.0f/60);
            require((supported->formations[0].organization.smallGroups[12].cavalry.phase==CavalryPhase::Disengaging)==(enemy==UnitType::Spearman),
                "Cavalry ignored support or braced spears");
        }
        auto vulnerable=afterCharge(UnitType::Archer);vulnerable->update(1.0f/60);
        require(vulnerable->formations[0].organization.smallGroups[12].cavalry.phase==CavalryPhase::None,"Cavalry abandoned a vulnerable archer");
        auto rally=duel(15);
        auto& rallying=rally->formations[0].organization.smallGroups[12];
        rallying.cavalry.phase=CavalryPhase::Regrouping;rallying.morale=45;
        rally->update(2);
        require(rallying.morale>50 && rallying.cavalry.phase==CavalryPhase::Regrouping,"Regrouping cavalry did not recover safely or resumed too early");
        rallying.morale=60;rallying.strength=9;rally->update(1.0f/60);
        require(rallying.cavalry.phase==CavalryPhase::Regrouping,"Depleted cavalry resumed charging");
        rallying.strength=20;rally->update(1.0f/60);
        require(rallying.cavalry.phase==CavalryPhase::None,"Ready cavalry did not resume approach");
        auto unsafe=duel(8);
        auto& unsafeRider=unsafe->formations[0].organization.smallGroups[12];
        unsafeRider.cavalry.phase=CavalryPhase::Regrouping;unsafeRider.morale=45;
        unsafe->update(1);
        require(unsafeRider.morale==45 && unsafeRider.cavalry.phase==CavalryPhase::Regrouping,"Cavalry recovered near an enemy");
        auto trapped=afterCharge();placeSupport(*trapped,-5,0);
        const auto trappedStart=trapped->formations[0].groupPosition(12);
        trapped->update(1.0f/60);
        auto& trappedRider=trapped->formations[0].organization.smallGroups[12];
        require(trappedRider.cavalry.blocked && trappedRider.cavalry.phase==CavalryPhase::None &&
            battleDistance(trappedStart,trapped->formations[0].groupPosition(12))<.001f,"Cavalry used a blocked retreat");
        placeSupport(*trapped,-30,30);trapped->update(1.0f/60);
        require(trappedRider.cavalry.phase==CavalryPhase::Disengaging,"Cavalry failed to retry cleared retreat");
        auto diagonal=afterCharge();placeSupport(*diagonal,-8,0);diagonal->update(1.0f/60);
        require(diagonal->formations[0].organization.smallGroups[12].cavalry.phase==CavalryPhase::Disengaging &&
            std::abs(diagonal->formations[0].organization.smallGroups[12].cavalry.destination.z)>5,"Cavalry did not select an open diagonal retreat");
        auto withdrawing=afterCharge();withdrawing->update(1.0f/60);
        auto rerouted=std::make_unique<BattleSimulation>(*withdrawing);
        const auto originalGoal=rerouted->formations[0].organization.smallGroups[12].cavalry.destination;
        placeSupport(*rerouted,-8,0);rerouted->update(.5f);
        auto& rerouting=rerouted->formations[0].organization.smallGroups[12];
        require(battleDistance(originalGoal,rerouting.cavalry.destination)<.001f,"Retreat switched before waiting for traffic");
        rerouted->update(.5f);
        require(battleDistance(originalGoal,rerouting.cavalry.destination)>5,"Blocked retreat never selected a new route");
        bool arrived=false;
        for(unsigned tick=0;tick<900 && !arrived;++tick) {
            const auto old=rerouted->formations[0].groupPosition(12);
            rerouted->update(1.0f/60);
            const auto current=rerouted->formations[0].groupPosition(12);
            require(battleDistance(old,current)<=4.0f/60+.001f,"Retreat replanning teleported cavalry");
            require(battleDistance(current,rerouted->formations[0].groupPosition(13))>=4.5f,"Retreat replanning crossed its blocker");
            arrived=rerouting.cavalry.phase==CavalryPhase::Regrouping;
        }
        require(arrived,"Replanned retreat did not reach regrouping point");
        auto traffic=std::make_unique<BattleSimulation>(*withdrawing);
        placeSupport(*traffic,-8,0);traffic->update(.4f);placeSupport(*traffic,-30,30);traffic->update(.5f);
        const auto& clearTraffic=traffic->formations[0].organization.smallGroups[12];
        require(battleDistance(originalGoal,clearTraffic.cavalry.destination)<.001f && clearTraffic.cavalry.blockedSeconds==0,
            "Brief traffic unnecessarily changed the retreat route");
        auto sealed=std::make_unique<BattleSimulation>(*withdrawing);
        placeSupport(*sealed,-5,0);sealed->update(2);
        const auto& sealedRider=sealed->formations[0].organization.smallGroups[12];
        require(sealedRider.cavalry.blocked && sealedRider.cavalry.phase==CavalryPhase::Disengaging &&
            battleDistance(originalGoal,sealedRider.cavalry.destination)<.001f,"Cavalry forced a route through sealed exits");
        sealed->hold(0);sealed->update(1);
        require(sealed->formations[0].organization.smallGroups[12].cavalry.blockedSeconds==0,"Hold retained retreat retry timer");
        auto replanSlow=std::make_unique<BattleSimulation>(*withdrawing);placeSupport(*replanSlow,-8,0);
        auto replanFast=std::make_unique<BattleSimulation>(*replanSlow),replanMirror=std::make_unique<BattleSimulation>(*replanSlow);
        std::swap(replanMirror->formations[0],replanMirror->formations[1]);
        replanSlow->update(8);replanMirror->update(8);for(unsigned tick=0;tick<480;++tick)replanFast->update(1.0f/60);
        const auto& replanned=replanSlow->formations[0].organization.smallGroups[12];
        for(const auto* actual:{&replanFast->formations[0].organization.smallGroups[12],&replanMirror->formations[1].organization.smallGroups[12]})
            require(replanned.cavalry.destination.x==actual->cavalry.destination.x && replanned.cavalry.destination.z==actual->cavalry.destination.z &&
                replanned.cavalry.blockedSeconds==actual->cavalry.blockedSeconds && replanned.approachX==actual->approachX &&
                replanned.approachZ==actual->approachZ,"Retreat replanning depends on update interval or team");
        auto lostDuringRetreat=std::make_unique<BattleSimulation>(*withdrawing);placeSupport(*lostDuringRetreat,-8,0);
        for(auto& h:lostDuringRetreat->formations[1].organization.smallGroups)h.offsetX+=1000;
        lostDuringRetreat->update(1);
        require(lostDuringRetreat->formations[0].organization.smallGroups[12].attackTarget<0 &&
            battleDistance(originalGoal,lostDuringRetreat->formations[0].organization.smallGroups[12].cavalry.destination)>5,
            "Target loss prevented blocked retreat replanning");
        const auto withdrawStart=withdrawing->formations[0].groupPosition(12);
        withdrawing->update(.5f);
        require(battleDistance(withdrawStart,withdrawing->formations[0].groupPosition(12))<.001f,"Cavalry slid before turning away");
        auto interrupted=std::make_unique<BattleSimulation>(*withdrawing);
        interrupted->hold(0);interrupted->update(1);
        const auto& stopped=interrupted->formations[0].organization.smallGroups[12];
        require(stopped.cavalry.phase==CavalryPhase::None && stopped.cavalry.handledCharge==stopped.charges &&
            battleDistance(withdrawStart,interrupted->formations[0].groupPosition(12))<.001f,"Hold failed to cancel cavalry retreat");
        interrupted=std::make_unique<BattleSimulation>(*withdrawing);interrupted->move(0,-50,0);
        require(interrupted->formations[0].organization.smallGroups[12].cavalry.phase==CavalryPhase::None,"Move retained autonomous cavalry retreat");
        interrupted=std::make_unique<BattleSimulation>(*withdrawing);interrupted->formations[0].organization.smallGroups[12].morale=10;
        interrupted->update(1.0f/60);
        require(interrupted->formations[0].organization.smallGroups[12].routed &&
            interrupted->formations[0].organization.smallGroups[12].cavalry.phase==CavalryPhase::None,"Rout retained cavalry retreat");
        auto cadence=afterCharge(),sliced=afterCharge(),reversed=afterCharge();
        std::swap(reversed->formations[0],reversed->formations[1]);
        cadence->update(24);reversed->update(24);for(unsigned tick=0;tick<1440;++tick)sliced->update(1.0f/60);
        const auto& expected=cadence->formations[0].organization.smallGroups[12];
        for(const auto* actual:{&sliced->formations[0].organization.smallGroups[12],&reversed->formations[1].organization.smallGroups[12]})
            require(expected.charges==actual->charges && expected.approachX==actual->approachX && expected.approachZ==actual->approachZ &&
                expected.cavalry.phase==actual->cavalry.phase && expected.cavalry.handledCharge==actual->cavalry.handledCharge &&
                expected.strength==actual->strength,"Cavalry retreat depends on cadence or team color");
        withdrawing->running=false;const auto frozen=withdrawing->formations[0].groupPosition(12);withdrawing->update(10);
        require(battleDistance(frozen,withdrawing->formations[0].groupPosition(12))==0 &&
            withdrawing->formations[0].organization.smallGroups[12].cavalry.phase==CavalryPhase::Disengaging,"Pause advanced retreat");
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
                require(f.organization.smallGroups[id].charges==0 && f.organization.smallGroups[id].cavalry.phase==CavalryPhase::None &&
                    f.organization.smallGroups[id].cavalry.handledCharge==0, "Reset retained charge state"); }
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
