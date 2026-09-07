#include "simulation.h"
#include <iostream>
#include <stdexcept>
#include <memory>

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
BattleSimulation reserves() {
    BattleSimulation battle;
    battle.hold(0); battle.hold(1);
    for (unsigned id = 0; id < 13; ++id) {
        auto& g = battle.formations[0].organization.smallGroups[id];
        g.routed = true; g.fleeX = g.fleeTargetX = -60;
        g.fleeZ = g.fleeTargetZ = -60 + id * 4.6f;
    }
    battle.running = true;
    return battle;
}
int main() {
    try {
        for(bool panic:{false,true})for(float sign:{-1.0f,1.0f}) {
            auto refuge=std::make_unique<BattleSimulation>();
            for(unsigned team=0;team<2;++team) {
                auto& f=refuge->formations[team];f.x=f.targetX=team==0?-40.0f:40.0f;f.z=f.targetZ=0;
                for(auto& g:f.organization.smallGroups) {g.offsetZ=1000;g.morale=100;}
            }
            refuge->hold(1);refuge->running=true;
            auto& f=refuge->formations[0];auto& g=f.organization.smallGroups[12];
            g.offsetX=-f.x;g.offsetZ=0;g.resting=true;g.morale=60;g.fatigue=10;g.strength=16;
            g.heading=sign>0?3.14159265f:0;f.strength-=4;
            if(panic) {
                auto& source=f.organization.smallGroups[13];source.routed=true;source.routShock=8;
                source.fleeX=source.fleeTargetX=sign*6;source.fleeZ=source.fleeTargetZ=0;
            } else {
                auto& enemy=refuge->formations[1].organization.smallGroups[12];enemy.offsetX=sign*9.95f-refuge->formations[1].x;enemy.offsetZ=0;
            }
            refuge->update(1.0f/60);require(g.restRelocating,"Threatened resting group did not seek refuge");
            auto frozen=std::make_unique<BattleSimulation>(*refuge);frozen->running=false;frozen->update(2);
            require(battleDistance(frozen->formations[0].groupPosition(12),f.groupPosition(12))==0,"Pause moved resting group");
            auto held=std::make_unique<BattleSimulation>(*refuge);held->hold(0);held->update(.5f);
            require(battleDistance(held->formations[0].groupPosition(12),f.groupPosition(12))==0,"Hold moved resting group");
            auto split=std::make_unique<BattleSimulation>(*refuge),whole=std::make_unique<BattleSimulation>(*refuge);
            whole->update(1);for(unsigned tick=0;tick<60;++tick)split->update(1.0f/60);
            require(battleDistance(whole->formations[0].groupPosition(12),split->formations[0].groupPosition(12))<.001f &&
                whole->formations[0].organization.smallGroups[12].morale==split->formations[0].organization.smallGroups[12].morale,"Rest relocation depends on cadence");
            bool completed=false;
            for(unsigned tick=0;tick<2400;++tick) {
                const auto before=f.groupPosition(12);refuge->update(1.0f/60);
                require(battleDistance(before,f.groupPosition(12))<=f.speed/60+.001f,"Rest relocation teleported");
                for(unsigned t=0;t<2;++t)for(unsigned other=0;other<25;++other) {
                    if(t==0 && other==12)continue;
                    require(battleDistance(f.groupPosition(12),refuge->formations[t].groupPosition(other))>=4.5f,"Rest relocation crossed occupancy");
                }
                if(!g.resting) {completed=true;break;}
            }
            require(completed && !g.routed && g.morale>=70 && g.fatigue<=2 && g.strength==16,"Relocated group failed to recover without regenerating casualties");
            auto returnHeld=std::make_unique<BattleSimulation>(*refuge);returnHeld->hold(0);returnHeld->update(1);
            require(battleDistance(returnHeld->formations[0].groupPosition(12),f.groupPosition(12))==0,"Hold moved recovered group");
            auto noTarget=std::make_unique<BattleSimulation>(*refuge);
            noTarget->formations[1].organization.smallGroups[12].offsetZ=1000;
            for(unsigned tick=0;tick<600;++tick)noTarget->update(1.0f/60);
            require(battleDistance(noTarget->formations[0].groupPosition(12),{0,0})<.001f,
                "Recovered group without a target failed to return to its reserve position");
            if(!panic) {
                bool rejoined=false;
                for(unsigned tick=0;tick<900;++tick) {
                    const auto before=f.groupPosition(12);refuge->update(1.0f/60);
                    require(battleDistance(before,f.groupPosition(12))<=f.speed/60+.001f,"Recovered group teleported on return");
                    require(battleDistance(f.groupPosition(12),refuge->formations[1].groupPosition(12))>=4.5f,"Recovered group crossed enemy occupancy");
                    if(g.canAttack) {rejoined=true;break;}
                }
                require(rejoined,"Recovered group stranded beyond infantry pursuit range");
            }
        }
        auto healthy = reserves(); healthy.update(10);
        require(healthy.formations[0].readyGroups() == 12 && healthy.result == BattleResult::Ongoing,
            "Healthy remaining troops were forced to withdraw by rout count alone");
        auto exhausted = reserves();
        for (unsigned id = 13; id < 25; ++id) exhausted.formations[0].organization.smallGroups[id].strength = 9;
        exhausted.update(3);
        require(exhausted.result == BattleResult::Ongoing && exhausted.formations[0].withdrawalPressure > 2,
            "Army withdrawal did not wait for sustained exhaustion");
        auto recovered = exhausted;
        for (unsigned id = 13; id < 19; ++id) recovered.formations[0].organization.smallGroups[id].strength = 20;
        recovered.update(4);
        require(recovered.result == BattleResult::Ongoing && recovered.formations[0].withdrawalPressure == 0,
            "Available reserves did not cancel withdrawal consideration");
        const float pressure = exhausted.formations[0].withdrawalPressure;
        exhausted.running = false; exhausted.update(10);
        require(exhausted.formations[0].withdrawalPressure == pressure, "Pause advanced withdrawal pressure");
        exhausted.running = true; exhausted.update(4);
        require(exhausted.result == BattleResult::BlueVictory && exhausted.formations[0].defeated(),
            "Exhausted army failed to withdraw");

        BattleSimulation resting; resting.hold(0); resting.hold(1); resting.running = true;
        auto& group = resting.formations[0].organization.smallGroups[2];
        group.resting = true; group.morale = 18; group.fatigue = 10; group.strength = 16;
        resting.formations[0].strength -= 4;
        resting.update(1);
        require(group.resting && !group.routed && group.morale > 18 && group.fatigue < 10 && !group.canAttack,
            "Withdrawing troops routed or failed to recover safely");
        auto paused = resting; paused.running = false; paused.update(5);
        require(paused.formations[0].organization.smallGroups[2].morale == group.morale, "Pause healed resting troops");
        resting.update(20);
        require(!group.resting && group.morale >= 70 && group.fatigue <= 2 && group.strength == 16,
            "Recovered troops did not rejoin readiness or regenerated casualties");
        BattleSimulation threatened; threatened.hold(0); threatened.hold(1); threatened.running = true;
        auto& wounded = threatened.formations[0].organization.smallGroups[2];
        wounded.resting = true; wounded.morale = 30; wounded.lastDamageTime = 0;
        threatened.update(2);
        require(wounded.morale == 30 && wounded.resting, "Recently hit troops recovered immediately");
        resting.reset(); require(!resting.formations[0].organization.smallGroups[2].resting && resting.formations[0].withdrawalPressure == 0,
            "Reset retained recovery state");
        std::cout << "Reserve readiness, rest, recovery and delayed withdrawal checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
