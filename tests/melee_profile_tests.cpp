#include "scene.h"
#include <iostream>
#include <stdexcept>

void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
BattleSimulation duel(UnitType type,float distance) {
    BattleSimulation battle;battle.reset(type);
    for(unsigned team=0;team<2;++team) {
        auto& f=battle.formations[team];f.x=f.targetX=team==0?-40.0f:40.0f;f.z=f.targetZ=0;f.maneuverEnabled=false;
        for(auto& g:f.organization.smallGroups){g.offsetZ=1000;g.heading=team==0?0.0f:3.141592654f;}
        auto& g=f.organization.smallGroups[12];g.offsetX=(team==0?0:distance)-f.x;g.offsetZ=0;
    }
    battle.running=true;return battle;
}
int main(){
    try {
        auto spear=duel(UnitType::Spearman,6.5f),sword=duel(UnitType::Samurai,6.5f);
        spear.update(.1f);sword.update(.1f);
        require(spear.formations[1].strength<500,"Spear range changed");
        require(sword.formations[1].strength==500,"Sword dealt damage beyond its range");
        auto close=duel(UnitType::Samurai,5.0f);close.update(.1f);
        require(close.formations[0].organization.smallGroups[12].canAttack && close.formations[1].strength<500,"Sword cannot attack in range");
        auto rear=duel(UnitType::Samurai,-5.0f);rear.update(.1f);
        require(rear.formations[1].strength==500,"Sword attacked without facing the target");
        auto blocked=duel(UnitType::Samurai,5.0f);
        auto& blocker=blocked.formations[0].organization.smallGroups[13];blocker.offsetZ=0;blocker.offsetX=2.75f-blocked.formations[0].x-5.2f;
        blocked.update(.1f);
        require(!blocked.formations[0].organization.smallGroups[12].canAttack,"Sword attacked through another group");
        BattleSimulation a,b;a.reset(UnitType::Samurai);b.reset(UnitType::Samurai);a.running=b.running=true;
        a.update(20);for(unsigned i=0;i<600;++i)b.update(1.0f/30);
        for(unsigned team=0;team<2;++team)require(std::abs(a.formations[team].strength-b.formations[team].strength)<.001f,"Sword damage depends on frame interval");
        const auto strength=a.formations[0].strength;a.running=false;a.update(2);
        require(a.formations[0].strength==strength,"Paused sword battle changed");
        bool rejected=false;try{a.reset(UnitType::Archer);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected && a.formations[0].unit==UnitType::Samurai,"Unsupported unit entered melee battle");
        // 表示の攻撃も刀の短い個体間距離を守る。
        BattleSimulation visualBattle;visualBattle.reset(UnitType::Samurai);visualBattle.running=true;
        SoldierVisuals visuals;bool observed=false;
        for(unsigned frame=0;frame<300;++frame){
            visualBattle.update(.05f);visuals.update(visualBattle);
            for(const auto& s:visuals.soldiers)if(s.attacking){
                observed=true;const auto& target=visuals.soldiers[static_cast<unsigned>(s.attackTarget)];
                require(std::hypot(s.position.x-target.position.x,s.position.z-target.position.z)<=1.5001f,"Long-distance sword animation");
            }
        }
        require(observed,"No sword attacks were displayed");
        std::cout<<"PASS: sword range, facing, obstruction, pause, fixed updates and individual reach\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
