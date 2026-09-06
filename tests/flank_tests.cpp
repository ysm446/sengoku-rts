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
