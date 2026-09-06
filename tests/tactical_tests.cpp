#include "simulation.h"
#include <memory>
#include <iostream>
#include <stdexcept>

void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void place(Formation& f,unsigned id,float x,float z) {
    auto& g=f.organization.smallGroups[id];
    g.offsetX=x-f.x-(static_cast<float>(g.slot%5)-2)*5.2f;
    g.offsetZ=z-f.z-(static_cast<float>(g.slot/5)-2)*5.2f;
}
std::unique_ptr<BattleSimulation> fixture(float distance=20,UnitType type=UnitType::Spearman) {
    auto battle=std::make_unique<BattleSimulation>();
    for(unsigned team=0;team<2;++team) {
        auto& f=battle->formations[team];f.x=f.targetX=team==0?-40.0f:40.0f;f.z=f.targetZ=0;
        for(auto& g:f.organization.smallGroups){g.offsetZ=1000;g.heading=team==0?0:3.141592654f;g.morale=100;}
        place(f,12,team==0?0:distance,0);battle->hold(team);
    }
    battle->formations[0].organization.smallGroups[12].unit=type;
    battle->running=true;return battle;
}
int main(){try {
    auto seen=fixture();seen->update(1.0f/60);
    const auto& scout=seen->formations[0].organization.smallGroups[12];
    require(scout.awareness.enemies==1 && scout.attackTarget==12 && !scout.canAttack,"Sight and attack range were not separated");
    require(scout.awareness.decision==TacticalDecision::Hold,"Hold was overridden by scouting");
    seen->formations[0].maneuverEnabled=true;const auto heldPosition=seen->formations[0].groupPosition(12);seen->update(.2f);
    require(scout.awareness.decision==TacticalDecision::Reserve && battleDistance(heldPosition,seen->formations[0].groupPosition(12))==0,
        "Distant scouting pulled infantry reserves out of formation");
    auto distant=fixture(25);distant->update(.1f);
    require(distant->formations[0].organization.smallGroups[12].awareness.enemies==0 &&
        distant->formations[0].organization.smallGroups[12].attackTarget<0,"Infantry detected outside sight radius");
    auto mounted=fixture(25,UnitType::Cavalry);mounted->update(.1f);
    require(mounted->formations[0].organization.smallGroups[12].awareness.enemies==1,"Mounted scouting range was not applied");
    auto stable=fixture(12);place(stable->formations[1],13,0,12.2f);stable->update(1.0f/60);
    require(stable->formations[0].organization.smallGroups[12].attackTarget==12,"Initial target was not the closest clear opponent");
    place(stable->formations[1],13,0,11.8f);stable->update(1.0f/60);
    require(stable->formations[0].organization.smallGroups[12].attackTarget==12,"Minor distance changes caused target switching");
    place(stable->formations[1],12,40,0);stable->update(1.0f/60);
    require(stable->formations[0].organization.smallGroups[12].attackTarget==13,"Lost target remained selected outside sight");
    auto choice=fixture(12,UnitType::Cavalry);place(choice->formations[1],13,12,6);
    choice->formations[1].organization.smallGroups[13].unit=UnitType::Archer;
    choice->update(1.0f/60);
    require(choice->formations[0].organization.smallGroups[12].attackTarget==13,"Cavalry ignored exposed archer in favor of braced spears");
    auto mirrored=fixture(12,UnitType::Cavalry);place(mirrored->formations[1],13,12,6);
    mirrored->formations[1].organization.smallGroups[13].unit=UnitType::Archer;
    std::swap(mirrored->formations[0],mirrored->formations[1]);mirrored->update(1.0f/60);
    require(mirrored->formations[1].organization.smallGroups[12].attackTarget==13,"Target judgment depends on team color");
    choice->formations[1].organization.smallGroups[13].strength=0;choice->update(1.0f/60);
    require(choice->formations[0].organization.smallGroups[12].attackTarget==12,"Destroyed target was retained");
    auto weak=fixture(10);auto& f=weak->formations[0];auto& g=f.organization.smallGroups[12];
    g.morale=35;place(weak->formations[1],13,10,5.2f);place(weak->formations[1],11,10,-5.2f);
    f.maneuverEnabled=true;const auto start=f.groupPosition(12);weak->update(.2f);
    require(g.awareness.cautious && g.awareness.decision==TacticalDecision::Reserve && battleDistance(start,f.groupPosition(12))==0,
        "Isolated weak group advanced into superior force");
    place(f,11,-5.2f,0);weak->update(.2f);
    require(!g.awareness.cautious && f.groupPosition(12).x>start.x,"Support did not release cautious advance");
    weak->hold(0);require(g.awareness.decision==TacticalDecision::Hold,"Hold decision was delayed");
    auto a=fixture(10),b=fixture(10);a->formations[0].maneuverEnabled=b->formations[0].maneuverEnabled=true;
    a->update(2);for(unsigned tick=0;tick<120;++tick)b->update(1.0f/60);
    require(a->formations[0].organization.smallGroups[12].attackTarget==b->formations[0].organization.smallGroups[12].attackTarget &&
        a->formations[0].organization.smallGroups[12].awareness.decision==b->formations[0].organization.smallGroups[12].awareness.decision &&
        a->formations[0].strength==b->formations[0].strength,"Tactical decisions depend on update cadence");
    a->running=false;const auto before=a->formations[0].organization.smallGroups[12].awareness.enemies;
    place(a->formations[1],12,75,0);a->update(2);
    require(a->formations[0].organization.smallGroups[12].awareness.enemies==before,"Paused scouting changed");
    a->reset();require(a->formations[0].organization.smallGroups[12].awareness.enemies==0,"Reset retained scouting");
    std::cout<<"Scouting range, target judgment, local support, command priority and cadence passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
