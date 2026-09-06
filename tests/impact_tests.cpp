#include "scene.h"
#include <iostream>
#include <memory>
#include <stdexcept>

void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::unique_ptr<BattleSimulation> duel(float distance=7) {
    auto battle=std::make_unique<BattleSimulation>();
    for(unsigned team=0;team<2;++team) {
        auto& f=battle->formations[team];f.x=f.targetX=team==0?-40.0f:40.0f;f.z=f.targetZ=0;
        for(auto& g:f.organization.smallGroups){g.offsetZ=1000;g.heading=team==0?0:3.141592654f;}
        auto& g=f.organization.smallGroups[12];g.offsetX=(team==0?0:distance)-f.x;g.offsetZ=0;
        battle->hold(team);
    }
    battle->running=true;return battle;
}
int main(){try {
    auto far=duel(14);far->update(.1f);require(far->impactSerial==0,"Out of range attack emitted a spark");
    auto hit=duel();hit->update(.1f);require(hit->impactSerial>0,"Damage did not emit an impact");
    for(const auto& impact:hit->impacts)if(impact.damage>0) {
        require(impact.position.x==3.5f && impact.position.z==0 && impact.group==12,"Impact missed contact midpoint");
        require(hit->formations[impact.team].organization.smallGroups[12].strength<20,"Spark had no actual damage");
    }
    const auto firstSerial=hit->impactSerial;hit->update(.1f);
    require(hit->impactSerial==firstSerial,"Continuous damage emitted sparks every tick");
    hit->update(.1f);require(hit->impactSerial>firstSerial,"Continuous melee never emitted a later spark");
    auto slow=duel(),fast=duel();slow->update(1);for(unsigned tick=0;tick<60;++tick)fast->update(1.0f/60);
    require(slow->impactSerial==fast->impactSerial,"Impact emission depends on update interval");
    for(unsigned i=0;i<BattleSimulation::impactCapacity;++i)require(slow->impacts[i].time==fast->impacts[i].time &&
        slow->impacts[i].damage==fast->impacts[i].damage,"Impact events differ by update interval");
    auto charge=duel(5.5f);auto& cavalry=charge->formations[0].organization.smallGroups[12];
    cavalry.unit=UnitType::Cavalry;cavalry.chargeWindow=.5f;cavalry.faceDeployment.accountedStrength=20;cavalry.faceDeployment.deployed={5,5,5,5};
    charge->formations[0].maneuverEnabled=true;charge->update(1.0f/60);
    bool charged=false;for(const auto& impact:charge->impacts)charged|=impact.damage>0 && impact.kind==ImpactKind::Charge;
    require(charged,"Charge did not emit its distinct impact");
    for(bool miss:{false,true}) {
        auto bow=duel(14);ArrowVolley arrow{0,12,12,{0,0},{miss?24.0f:14.0f,0}};
        arrow.duration=.1f;arrow.damage=1;bow->arrows.push_back(arrow);bow->update(.11f);
        require((bow->impactSerial==0)==miss,"Arrow spark did not match hit or miss");
        if(!miss)require(bow->impacts[0].kind==ImpactKind::Arrow && bow->impacts[0].position.x==14,"Arrow spark missed landing point");
    }
    auto scene=makeScene();Camera camera;
    hit->running=false;updateSceneSprites(scene,*hit,camera);
    unsigned visible=0;for(unsigned i=0;i<BattleSimulation::impactCapacity;++i)visible+=scene.sprites[scene.impactStart+i].tile!=13;
    require(visible>0,"Recent impact was not rendered");
    const auto serial=hit->impactSerial;const auto sprites=scene.sprites;
    hit->update(10);updateSceneSprites(scene,*hit,camera);
    require(hit->impactSerial==serial,"Pause emitted new impacts");
    for(unsigned i=0;i<BattleSimulation::impactCapacity;++i)require(scene.sprites[scene.impactStart+i].tile==sprites[scene.impactStart+i].tile,"Paused spark animation advanced");
    hit->time+=.2;updateSceneSprites(scene,*hit,camera);
    for(unsigned i=0;i<BattleSimulation::impactCapacity;++i)require(scene.sprites[scene.impactStart+i].tile==13,"Expired spark remained visible");
    hit->reset();updateSceneSprites(scene,*hit,camera);require(hit->impactSerial==0,"Reset retained impacts");
    for(unsigned tile:{36u,37u,38u}) {
        unsigned opaque=0;for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)
            opaque+=(scene.atlas[(tile/12*64+y)*Scene::atlasWidth+tile%12*64+x]>>24)!=0;
        require(opaque>0 && opaque<4096,"Spark tile is blank or lacks transparency");
    }
    std::cout<<"Damage impacts, charge, arrow misses, cadence, pause, expiry and reset passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
