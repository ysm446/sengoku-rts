#include "simulation.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::unique_ptr<BattleSimulation> fixture(bool axis, float sign, unsigned team, float stagger) {
    auto battle = std::make_unique<BattleSimulation>();
    for (unsigned t=0; t<2; ++t) {
        auto& f=battle->formations[t];
        const float forward=(t==team?-14.0f:14.0f)*sign, lateral=t==team?0:stagger;
        f.x=f.targetX=axis?forward:lateral; f.z=f.targetZ=axis?lateral:forward;
        f.morale=100; for(auto& g:f.organization.smallGroups) g.morale=100;
    }
    battle->hold(1-team); battle->running=true; return battle;
}
int main() {
    try {
        // 展開済みの両翼と中央の接触だけを残し、旧経路の終点からの接近を検証する。
        for(bool axis:{false,true}) for(float sign:{-1.0f,1.0f}) for(unsigned team:{0u,1u}) for(unsigned lane:{0u,4u}) {
            auto battle=fixture(axis,sign,team,0);
            for(auto& f:battle->formations) for(auto& g:f.organization.smallGroups) g.offsetX=g.offsetZ=1000;
            for(unsigned t=0;t<2;++t) {
                auto& f=battle->formations[t];auto& anchor=f.organization.smallGroups[12];
                const float forward=(t==team?-3.6f:3.6f)*sign;
                anchor.offsetX=(axis?forward:0)-f.x;anchor.offsetZ=(axis?0:forward)-f.z;
            }
            auto& f=battle->formations[team];
            const unsigned rank=sign>0?3:1, id=axis?lane*5+rank:rank*5+lane;
            auto& g=f.organization.smallGroups[id];
            g.route=SmallGroupRoute::Forward;g.routeAlongX=axis;g.routeForward=12*sign;
            g.routeLateral=lane==0?-6.1f:6.1f;
            g.offsetX=axis?g.routeForward:g.routeLateral;g.offsetZ=axis?g.routeLateral:g.routeForward;
            const auto start=f.groupPosition(id);
            auto obstructed=std::make_unique<BattleSimulation>(*battle);
            auto& blocker=obstructed->formations[team].organization.smallGroups[6];
            const auto target=obstructed->formations[1-team].groupPosition(12);
            blocker.offsetX=(start.x+target.x)*.5f-f.x+5.2f;blocker.offsetZ=(start.z+target.z)*.5f-f.z+5.2f;
            blocker.resting=true;blocker.fatigue=20;
            obstructed->update(.2f);
            require(battleDistance(start,obstructed->formations[team].groupPosition(id))<.001f,"Flank ignored a blocked approach");
            blocker.offsetX=blocker.offsetZ=1000;obstructed->update(1);
            require(battleDistance(start,obstructed->formations[team].groupPosition(id))>1,"Flank failed to resume after lane cleared");
            auto held=std::make_unique<BattleSimulation>(*battle);held->hold(team);held->update(1);
            require(battleDistance(start,held->formations[team].groupPosition(id))<.001f,"Held flank approached a target");
            auto split=std::make_unique<BattleSimulation>(*battle);
            battle->update(3);for(unsigned tick=0;tick<180;++tick)split->update(1.0f/60);
            require(battleDistance(f.groupPosition(id),split->formations[team].groupPosition(id))<.001f &&
                g.flankClosing==split->formations[team].organization.smallGroups[id].flankClosing,"Closing flank depends on update interval");
            require(g.flankClosing && battleDistance(start,f.groupPosition(id))>4,"Flank stayed at its old endpoint");
            auto lost=std::make_unique<BattleSimulation>(*battle);
            for(auto& enemy:lost->formations[1-team].organization.smallGroups) enemy.offsetX+=1000;
            lost->update(1.0f/60);
            require(lost->formations[team].organization.smallGroups[id].route==SmallGroupRoute::Returning,"Flank pursued a lost target");
            bool attacked=false;
            for(unsigned tick=0;tick<300;++tick) {
                battle->update(1.0f/60);attacked|=g.canAttack;
                for(unsigned t=0;t<2;++t)for(unsigned other=0;other<25;++other) {
                    if(t==team && other==id)continue;
                    require(battleDistance(f.groupPosition(id),battle->formations[t].groupPosition(other))>=4.5f,"Closing flank crossed another group");
                }
                if(attacked)break;
            }
            require(attacked,"Deployed flank never attacked its detected target");
            g.fatigue=8;battle->update(1.0f/60);
            require(g.route==SmallGroupRoute::Returning,"Tired closing flank did not return");
            battle->reset();require(!battle->formations[team].organization.smallGroups[id].flankClosing,"Reset retained closing state");
        }
        for(bool axis:{false,true}) for(float sign:{-1.0f,1.0f}) for(unsigned team:{0u,1u}) for(float stagger:{-12.0f,12.0f}) {
            auto battle=fixture(axis,sign,team,stagger);
            std::array<float,2> spread{};
            for(unsigned tick=0; tick<360; ++tick) {
                battle->update(1.0f/60);
                const auto& f=battle->formations[team];
                for(unsigned id=0;id<25;++id) {
                    const auto& g=f.organization.smallGroups[id];
                    const unsigned lane=axis?g.slot/5:g.slot%5;
                    if(g.route==SmallGroupRoute::Outward || g.route==SmallGroupRoute::Forward) {
                        require(lane==0 || lane==4, "Interior reserve used a flank lane");
                        spread[lane==0?0:1]=std::max(spread[lane==0?0:1],std::abs(axis?g.offsetZ:g.offsetX));
                        require(g.routeLateral*(lane==0?-1:1)>=6.1f, "Flank moved inward");
                    }
                    if(g.strength<=0)continue;
                    for(unsigned other=0;other<id;++other) if(f.organization.smallGroups[other].strength>0)
                        require(battleDistance(f.groupPosition(id),f.groupPosition(other))>=4.5f,"Flank crossed a friendly group");
                }
            }
            if(spread[0]<5 || spread[1]<5) std::cerr<<axis<<' '<<sign<<' '<<team<<' '<<stagger<<" spread="<<spread[0]<<','<<spread[1]<<'\n';
            require(spread[0]>=5 && spread[1]>=5,"Staggered formation failed to spread both flanks");
            battle->hold(team); const auto held=battle->formations[team].organization.smallGroups;
            battle->update(.2f);
            for(unsigned id=0;id<25;++id) require(held[id].offsetX==battle->formations[team].organization.smallGroups[id].offsetX &&
                held[id].offsetZ==battle->formations[team].organization.smallGroups[id].offsetZ,"Hold moved flank group");
        }
        auto slow=fixture(false,1,0,12),fast=fixture(false,1,0,12);
        slow->update(6);for(unsigned tick=0;tick<360;++tick)fast->update(1.0f/60);
        for(unsigned id=0;id<25;++id) {
            const auto& a=slow->formations[0].organization.smallGroups[id];const auto& b=fast->formations[0].organization.smallGroups[id];
            require(a.offsetX==b.offsetX && a.offsetZ==b.offsetZ && a.route==b.route,"Flank depends on update interval");
        }
        fast->reset();for(const auto& f:fast->formations)for(const auto& g:f.organization.smallGroups)
            require(g.routeLateral==0 && g.route==SmallGroupRoute::None,"Reset retained flank route");
        auto mixed=std::make_unique<BattleSimulation>();mixed->reset(UnitType::Spearman,true);mixed->running=true;
        unsigned deployed=0;
        for(unsigned tick=0;tick<1200;++tick) {
            mixed->update(1.0f/60);
            for(const auto& f:mixed->formations)for(unsigned id=0;id<25;++id) {
                const auto& g=f.organization.smallGroups[id];
                if(g.route==SmallGroupRoute::Outward || g.route==SmallGroupRoute::Forward) {
                    require(f.groupUnit(id)!=UnitType::Archer,"Archer abandoned ranged support to flank");
                    if(std::hypot(g.offsetX,g.offsetZ)>5)++deployed;
                }
            }
        }
        require(deployed>0,"Mixed formation never deployed a flank");
        std::cout<<"Staggered flanks, mirrored directions, collision, hold, reset and mixed battle passed\n";
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
