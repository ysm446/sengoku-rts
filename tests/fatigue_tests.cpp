#include "simulation.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

void require(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
std::unique_ptr<BattleSimulation> duel(UnitType unit,float fatigue) {
    auto battle=std::make_unique<BattleSimulation>();
    for(unsigned team=0;team<2;++team) {
        auto& f=battle->formations[team];f.x=f.targetX=team==0?-40.0f:40.0f;f.z=f.targetZ=0;
        battle->hold(team);
        for(auto& g:f.organization.smallGroups) { g.offsetZ=1000;g.morale=100; }
        auto& g=f.organization.smallGroups[12];
        g.offsetX=(team==0?0:unit==UnitType::Archer?20.0f:5.0f)-f.x;g.offsetZ=0;
        g.heading=team==0?0:3.14159265f;
        g.faceDeployment.accountedStrength=20;g.faceDeployment.deployed={5,5,5,5};
    }
    auto& g=battle->formations[0].organization.smallGroups[12];g.unit=unit;g.fatigue=fatigue;
    battle->running=true;return battle;
}
float strike(BattleSimulation& battle,bool charge=false) {
    auto& g=battle.formations[0].organization.smallGroups[12];
    if(charge) { battle.formations[0].maneuverEnabled=true;g.chargeWindow=.5f; }
    const float before=battle.formations[1].organization.smallGroups[12].strength;
    battle.update(1.0f/60);
    if(battle.formations[0].groupUnit(12)==UnitType::Archer) {
        require(battle.arrows.size()==1,"Bow failed to shoot");return battle.arrows[0].damage;
    }
    return before-battle.formations[1].organization.smallGroups[12].strength;
}
int main() {
    try {
        for(auto unit:{UnitType::Spearman,UnitType::Samurai,UnitType::Cavalry,UnitType::Archer}) {
            auto fresh=duel(unit,0),tired=duel(unit,20),exhausted=duel(unit,30);
            const float full=strike(*fresh),low=strike(*tired),floor=strike(*exhausted);
            require(full>0 && std::abs(low/full-.6f)<.002f && std::abs(floor-low)<.0001f,"Fatigue did not reduce damage consistently");
        }
        auto charged=duel(UnitType::Cavalry,0),tiredCharge=duel(UnitType::Cavalry,20);
        const float fullCharge=strike(*charged,true),lowCharge=strike(*tiredCharge,true);
        require(std::abs(lowCharge/fullCharge-.6f)<.002f && charged->formations[0].organization.smallGroups[12].fatigue>=2,
            "Charge bypassed fatigue or incurred no fatigue");
        auto rested=duel(UnitType::Spearman,20);
        auto& g=rested->formations[0].organization.smallGroups[12];g.resting=true;g.strength=16;
        rested->formations[0].strength-=4;
        rested->formations[1].organization.smallGroups[12].offsetZ=1000;
        rested->update(1);
        require(std::abs(g.fatigue-19)<.001f && g.strength==16,"Rest healed casualties or recovered fatigue twice");
        auto paused=std::make_unique<BattleSimulation>(*rested);paused->running=false;paused->update(10);
        require(paused->formations[0].organization.smallGroups[12].fatigue==g.fatigue,"Pause recovered fatigue");
        rested->update(24);require(g.fatigue==0 && g.strength==16 && !g.resting,"Rest did not restore readiness");
        auto damagedFresh=duel(UnitType::Spearman,0);damagedFresh->formations[0].organization.smallGroups[12].strength=16;
        rested->formations[1].organization.smallGroups[12].offsetZ=0;
        g.heading=0;rested->formations[1].organization.smallGroups[12].heading=3.14159265f;
        g.faceDeployment=damagedFresh->formations[0].organization.smallGroups[12].faceDeployment;
        require(std::abs(strike(*rested)-strike(*damagedFresh))<.0001f,"Rest did not restore attack performance");
        auto threatened=duel(UnitType::Spearman,20);auto& h=threatened->formations[0].organization.smallGroups[12];h.resting=true;
        threatened->update(.5f);require(h.fatigue>=20,"Nearby enemy allowed fatigue recovery");
        auto idle=duel(UnitType::Spearman,20);idle->formations[1].organization.smallGroups[12].offsetZ=1000;
        auto recentlyHit=std::make_unique<BattleSimulation>(*idle);
        recentlyHit->formations[0].organization.smallGroups[12].lastDamageTime=0;
        idle->update(1);recentlyHit->update(1);
        require(std::abs(idle->formations[0].organization.smallGroups[12].fatigue-19.5f)<.001f &&
            recentlyHit->formations[0].organization.smallGroups[12].fatigue==20,"Idle recovery ignored its rate or recent damage");
        auto bow=duel(UnitType::Archer,20);const float shot=strike(*bow);
        require(bow->formations[0].organization.smallGroups[12].fatigue>=22,"Shooting incurred no fatigue");
        bow->formations[0].organization.smallGroups[12].fatigue=0;bow->update(1.1f);
        require(std::abs(20-bow->formations[1].organization.smallGroups[12].strength-shot)<.0001f,"Arrow used fatigue at impact instead of release");
        auto whole=duel(UnitType::Spearman,10),split=duel(UnitType::Spearman,10);
        whole->update(4);for(unsigned tick=0;tick<240;++tick)split->update(1.0f/60);
        require(whole->formations[0].organization.smallGroups[12].fatigue==split->formations[0].organization.smallGroups[12].fatigue &&
            whole->formations[1].strength==split->formations[1].strength,"Fatigue depends on update interval");
        whole->reset();require(whole->formations[0].organization.smallGroups[12].attackEfficiency()==1,"Reset retained fatigue penalty");
        std::cout<<"Fatigue damage, charge, arrows, rest, casualties, pause and cadence passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
