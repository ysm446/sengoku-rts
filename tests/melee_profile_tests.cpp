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
        SoldierVisuals visuals;bool observed=false, checkedTurn=false;
        for(unsigned frame=0;frame<300;++frame){
            const auto previous=visuals.soldiers;
            visualBattle.update(.05f);visuals.update(visualBattle);
            if(!previous.empty())for(unsigned id=0;id<previous.size();++id)
                require(std::abs(std::remainder(visuals.soldiers[id].heading-previous[id].heading,6.283185307f))
                    <=movementProfile(UnitType::Samurai).turnRate*.05f+.0001f,"Individual turned instantly during combat");
            for(const auto& s:visuals.soldiers)if(s.attacking){
                observed=true;const auto& target=visuals.soldiers[static_cast<unsigned>(s.attackTarget)];
                require(std::hypot(s.position.x-target.position.x,s.position.z-target.position.z)<=1.5001f,"Long-distance sword animation");
                const float bearing=std::atan2(target.position.z-s.position.z,target.position.x-s.position.x);
                require(std::abs(std::remainder(bearing-s.heading,6.283185307f))<=.3501f,"Individual attacked without facing enemy");
                if(!checkedTurn) {
                    auto turningBattle=visualBattle;auto turningVisuals=visuals;
                    const auto id=static_cast<unsigned>(&s-visuals.soldiers.data());
                    turningVisuals.soldiers[id].heading=bearing+3.141592654f;
                    turningBattle.time+=.01;turningVisuals.update(turningBattle);
                    require(!turningVisuals.soldiers[id].attacking,"Backward soldier attacked before turning");
                    checkedTurn=true;
                }
            }
        }
        require(observed,"No sword attacks were displayed");
        // 持ち場を離しても表示兵士が瞬間的に吸い寄せられず、停止後は整列する。
        for(const auto type:{UnitType::Spearman,UnitType::Samurai}) {
            BattleSimulation following;following.reset(type);
            SoldierVisuals followers;followers.update(following);
            // 単体の移動上限を検証する。混雑による押し戻しは別途検証する。
            for(unsigned id=1;id<followers.soldiers.size();++id)followers.soldiers[id].life=SoldierLife::Fallen;
            const auto start=followers.soldiers[0].position;
            following.formations[0].x+=10;
            followers.soldiers[0].heading=3.141592654f;
            constexpr double dt=1.0/60;
            for(unsigned frame=0;frame<600;++frame){
                const auto before=followers.soldiers[0];
                following.time+=dt;followers.update(following);
                const auto& after=followers.soldiers[0];
                const float movedX=after.position.x-before.position.x,movedZ=after.position.z-before.position.z;
                if(frame==0)require(std::hypot(movedX,movedZ)<.00001f && !after.walking,
                    "Follower walked before turning toward a rear destination");
                require(std::hypot(start.x+10-after.position.x,start.z-after.position.z)
                    <=std::hypot(start.x+10-before.position.x,start.z-before.position.z)+.0001f,
                    "Follower overshot its slot during a turn");
                require(std::abs(-std::sin(before.heading)*movedX+std::cos(before.heading)*movedZ)<=.0031f,
                    "Follower slid sideways while turning");
                require(std::cos(before.heading)*movedX+std::sin(before.heading)*movedZ>=-.0031f,
                    "Follower moved backward while turning");
                require(std::abs(std::remainder(after.heading-before.heading,6.283185307f))
                    <=movementProfile(type).turnRate*dt+.0001f,"Follower turned instantly");
                require(std::hypot(after.position.x-before.position.x,after.position.z-before.position.z)
                    <=following.formations[0].speed*1.5f*dt+.0031f,"Follower exceeded travel limit");
                require(after.followSpeed-before.followSpeed<=movementProfile(type).acceleration*1.15f*dt+.0001f,
                    "Follower accelerated instantly");
            }
            require(std::abs(followers.soldiers[0].position.x-start.x-10)<.01f,"Follower did not settle at its slot");
            const auto paused=followers.soldiers[0];followers.update(following);
            require(followers.soldiers[0].position.x==paused.position.x && followers.soldiers[0].followSpeed==paused.followSpeed,
                "Paused follower moved");
            following.reset(type);followers.update(following);
            require(followers.soldiers[0].followSpeed==0 && followers.soldiers[0].position.x==start.x,"Follower reset retained momentum");
        }
        SoldierVisuals overlap;overlap.soldiers.resize(3);
        overlap.soldiers[0].position={-.05f,0,0};overlap.soldiers[1].position={.05f,0,0};
        overlap.soldiers[2].life=SoldierLife::Fallen;
        const auto corpse=overlap.soldiers[2].position;
        overlap.separateOverlaps();
        require(std::abs(overlap.soldiers[1].position.x-overlap.soldiers[0].position.x-SoldierVisuals::minimumSpacing)<.0001f,
            "Overlapping circles were not separated");
        require(std::abs(overlap.soldiers[0].position.x+overlap.soldiers[1].position.x)<.0001f,"Separation favored one soldier");
        require(overlap.soldiers[2].position.x==corpse.x && overlap.soldiers[2].position.y==corpse.y,"Collision moved a corpse");
        const auto separated=overlap.soldiers;overlap.separateOverlaps();
        require(std::abs(overlap.soldiers[0].position.x-separated[0].position.x)<.0001f,"Separated soldiers kept drifting");
        overlap.soldiers[0].position=overlap.soldiers[1].position={0,0,0};overlap.separateOverlaps();
        require(std::isfinite(overlap.soldiers[0].position.x) &&
            std::abs(overlap.soldiers[1].position.x-overlap.soldiers[0].position.x-SoldierVisuals::minimumSpacing)<.0001f,
            "Coincident soldiers could not separate");
        std::cout<<"PASS: melee reach, facing, movement and visual collision\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
