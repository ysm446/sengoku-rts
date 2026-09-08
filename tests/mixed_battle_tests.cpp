#include "simulation.h"
#include "formation_drill.h"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
struct Metrics {
    unsigned exchanges=0,restStarted=0,restCompleted=0,reused=0,disengaged=0,regrouped=0,charges=0;
    unsigned reliefStarted=0,reliefAborted=0,frontRoutAborts=0,reserveRoutAborts=0;
    double wornFrontSeconds=0,displacedFrontSeconds=0,lastAttack=0;
    double attackSeconds=0,restSeconds=0,blockedSeconds=0,efficiencySum=0,fatigueRecovered=0;
    float strength=500;
    unsigned routed=0;
};
struct Sample { std::array<Metrics,2> armies; double elapsed=0; int winner=-1; };
Sample measure(unsigned configuration,bool swapped,unsigned duration,bool restReport=false,bool reliefReport=false) {
    auto battle=std::make_unique<BattleSimulation>();battle->reset(UnitType::Spearman,true);
    if(configuration>0) for(auto& f:battle->formations) { f.morale=100;for(auto& g:f.organization.smallGroups)g.morale=100; }
    if(configuration==2) for(auto& f:battle->formations)f.x=f.targetX=0;
    if(swapped)std::swap(battle->formations[0],battle->formations[1]);
    battle->running=true;
    Sample sample;
    std::array<std::array<bool,25>,2> readyAfterRest{};
    std::array<unsigned,2> changedSlots{};
    auto previous=std::make_unique<std::array<Formation,2>>();
    constexpr float dt=1.0f/60;
    for(unsigned tick=0;tick<duration*60 && battle->result==BattleResult::Ongoing;++tick) {
        *previous=battle->formations;
        battle->update(dt);
        for(unsigned team=0;team<2;++team) {
            auto& stats=sample.armies[team];const auto& f=battle->formations[team];
            const auto& enemy=battle->formations[1-team];
            const bool alongX=std::abs(enemy.x-f.x)>std::abs(enemy.z-f.z);
            const unsigned frontRank=(alongX?enemy.x-f.x:enemy.z-f.z)>=0?4:0;
            float total=0;
            for(unsigned id=0;id<25;++id) {
                const auto& g=f.organization.smallGroups[id];const auto& old=(*previous)[team].organization.smallGroups[id];
                require(std::isfinite(g.strength) && g.strength>=0 && g.strength<=old.strength+.0001f,"Long battle regenerated casualties");
                require(std::isfinite(g.fatigue) && g.fatigue>=0 && g.fatigue<=30,"Long battle has invalid fatigue");
                total+=g.strength;
                if(restReport && g.resting && (!old.resting || tick%60==0)) {
                    float nearest=10000;
                    for(unsigned other=0;other<25;++other)if(!enemy.organization.smallGroups[other].routed && enemy.organization.smallGroups[other].strength>0)
                        nearest=std::min(nearest,battleDistance(f.groupPosition(id),enemy.groupPosition(other)));
                    std::cout<<battle->time<<','<<team<<','<<id<<','<<g.morale<<','<<g.fatigue<<','<<g.strength<<','<<nearest<<','
                        <<battle->time-g.lastDamageTime<<','<<battle->nearbyRouts(team,id)<<','<<g.routed<<','<<g.restRelocating<<','<<g.restBlocked<<'\n';
                }
                if(!g.routed && !g.resting && (alongX?g.slot%5:g.slot/5)==frontRank &&
                    (g.fatigue>=8 || g.strength<=g.nominalStrength*.75f || g.morale<=50)) {
                    stats.wornFrontSeconds+=dt;
                    if(reliefReport && tick%60==0) {
                        const unsigned rearSlot=alongX?g.slot+(frontRank==4?-1:1):g.slot+(frontRank==4?-5:5);
                        for(unsigned rear=0;rear<25;++rear)if(f.organization.smallGroups[rear].slot==rearSlot) {
                            const auto& r=f.organization.smallGroups[rear];
                            const auto p=f.groupPosition(id),q=f.groupPosition(rear);
                            const float forward=(alongX?p.x-q.x:p.z-q.z)*(frontRank==4?1:-1);
                            const bool ready=!r.routed && !r.resting && r.fatigue<=2 && r.strength>r.nominalStrength*.5f &&
                                r.morale>40 && f.groupUnit(rear)!=UnitType::Archer;
                            // 更新後の実位置による診断。実際の予約判定とは時点が異なる。
                            float speed=std::max(f.speed,enemy.speed);
                            for(const auto& army:battle->formations)for(unsigned other=0;other<25;++other)
                                if(army.groupUnit(other)==UnitType::Cavalry)speed=std::max(speed,movementProfile(UnitType::Cavalry).speed);
                            const float clearance=4.5f+3*speed*dt;
                            const unsigned lane=alongX?g.slot/5:g.slot%5;
                            std::array<int,2> blockedLeg{},blockerTeam{{-1,-1}},blockerId{{-1,-1}};
                            for(unsigned side=0;side<2;++side) {
                                const float width=(lane==0 || lane==4?6.1f:5.2f)*(side==0?-1:1);
                                const BattlePoint shift{alongX?0:width,alongX?width:0};
                                const BattlePoint ps{p.x+shift.x,p.z+shift.z},qs{q.x+shift.x,q.z+shift.z};
                                const std::array<BattlePoint,4> from{{q,p,qs,ps}},to{{qs,q,ps,p}},stationary{{p,qs,q,q}};
                                for(unsigned leg=0;leg<4 && !blockedLeg[side];++leg) {
                                    if(std::abs(to[leg].x)>76 || std::abs(to[leg].z)>76 || segmentDistance(from[leg],to[leg],stationary[leg])<clearance) {
                                        blockedLeg[side]=leg+1;blockerTeam[side]=-2;break;
                                    }
                                    for(unsigned t=0;t<2 && !blockedLeg[side];++t)for(unsigned other=0;other<25;++other) {
                                        if((t==team && (other==id || other==rear)) || battle->formations[t].organization.smallGroups[other].strength<=0)continue;
                                        if(segmentDistance(from[leg],to[leg],battle->formations[t].groupPosition(other))<clearance) {
                                            blockedLeg[side]=leg+1;blockerTeam[side]=t;blockerId[side]=other;break;
                                        }
                                    }
                                }
                            }
                            std::cout<<configuration<<','<<battle->time<<','<<team<<','<<id<<','<<rear<<','<<ready<<','
                                <<static_cast<int>(g.route)<<','<<static_cast<int>(r.route)<<','<<r.routed<<','<<r.resting<<','
                                <<r.fatigue<<','<<r.strength<<','<<r.morale<<','<<forward<<','<<battleDistance(p,q);
                            for(unsigned side=0;side<2;++side)std::cout<<','<<blockedLeg[side]<<','<<blockerTeam[side]<<','<<blockerId[side];
                            std::cout<<'\n';
                        }
                    }
                    if(g.approachX!=0 || g.approachZ!=0 || g.offsetX!=0 || g.offsetZ!=0)stats.displacedFrontSeconds+=dt;
                }
                changedSlots[team]+=g.slot!=old.slot;
                stats.restStarted+=g.resting && !old.resting;
                if(old.resting && !g.resting && !g.routed && g.morale>=70 && g.fatigue<=2) { ++stats.restCompleted;readyAfterRest[team][id]=true; }
                if(g.canAttack && readyAfterRest[team][id]) { ++stats.reused;readyAfterRest[team][id]=false; }
                if(g.routed)readyAfterRest[team][id]=false;
                stats.disengaged+=g.cavalry.phase==CavalryPhase::Disengaging && old.cavalry.phase==CavalryPhase::None;
                stats.regrouped+=g.cavalry.phase==CavalryPhase::Regrouping && old.cavalry.phase==CavalryPhase::Disengaging;
                stats.charges+=g.charges-old.charges;
                stats.attackSeconds+=g.canAttack?dt:0;
                if(g.canAttack)stats.lastAttack=battle->time;
                stats.efficiencySum+=g.canAttack?g.attackEfficiency()*dt:0;
                stats.restSeconds+=(g.resting || g.cavalry.phase==CavalryPhase::Regrouping)?dt:0;
                stats.blockedSeconds+=(g.combatWait==CombatWait::PathBlocked || g.cavalry.blocked)?dt:0;
                stats.fatigueRecovered+=std::max(0.0f,old.fatigue-g.fatigue);
            }
            for(unsigned lane=0;lane<f.organization.frontReliefs.size();++lane) {
                const auto& r=f.organization.frontReliefs[lane];const auto& old=(*previous)[team].organization.frontReliefs[lane];
                stats.reliefStarted+=r.front>=0 && r.front!=old.front;
                if(old.front>=0 && r.front!=old.front && f.organization.smallGroups[old.front].slot==(*previous)[team].organization.smallGroups[old.front].slot &&
                    (*previous)[team].organization.smallGroups[old.front].route==SmallGroupRoute::ReliefWithdraw) {
                    ++stats.reliefAborted;
                    stats.frontRoutAborts+=f.organization.smallGroups[old.front].routed;
                    stats.reserveRoutAborts+=f.organization.smallGroups[old.reserve].routed;
                }
            }
            require(std::abs(total-f.strength)<.05f,"Long battle lost aggregate strength consistency");
            stats.strength=total;stats.routed=f.routedGroups();stats.exchanges=changedSlots[team]/2;
        }
    }
    sample.elapsed=battle->time;
    sample.winner=battle->result==BattleResult::RedVictory?0:battle->result==BattleResult::BlueVictory?1:-1;
    return sample;
}
void compare(const Sample& a,const Sample& b) {
    require(std::abs(a.elapsed-b.elapsed)<.001 && (a.winner<0?b.winner<0:b.winner==1-a.winner),"Team swap changed battle outcome");
    for(unsigned team=0;team<2;++team) {
        const auto& x=a.armies[team];const auto& y=b.armies[1-team];
        require(std::abs(x.strength-y.strength)<.001f && x.routed==y.routed && x.charges==y.charges &&
            x.exchanges==y.exchanges && x.restStarted==y.restStarted && x.restCompleted==y.restCompleted && x.reused==y.reused &&
            x.reliefStarted==y.reliefStarted && x.reliefAborted==y.reliefAborted && x.frontRoutAborts==y.frontRoutAborts &&
            x.reserveRoutAborts==y.reserveRoutAborts && std::abs(x.wornFrontSeconds-y.wornFrontSeconds)<.001 &&
            std::abs(x.displacedFrontSeconds-y.displacedFrontSeconds)<.001 && std::abs(x.lastAttack-y.lastAttack)<.001 &&
            x.disengaged==y.disengaged && x.regrouped==y.regrouped && std::abs(x.attackSeconds-y.attackSeconds)<.001 &&
            std::abs(x.restSeconds-y.restSeconds)<.001 && std::abs(x.blockedSeconds-y.blockedSeconds)<.001 &&
            std::abs(x.fatigueRecovered-y.fatigueRecovered)<.001,"Team swap changed recovery or combat metrics");
    }
}
void row(unsigned configuration,bool swapped,unsigned team,const Sample& sample) {
    const auto& m=sample.armies[team];
    std::cout<<configuration<<','<<swapped<<','<<team<<','<<sample.elapsed<<','<<sample.winner<<','<<m.strength<<','<<m.routed<<','
        <<m.exchanges<<','<<m.restStarted<<','<<m.restCompleted<<','<<m.reused<<','<<m.disengaged<<','<<m.regrouped<<','<<m.charges<<','
        <<m.attackSeconds<<','<<m.restSeconds<<','<<m.blockedSeconds<<','<<m.fatigueRecovered<<','
        <<(m.attackSeconds>0?100*m.efficiencySum/m.attackSeconds:100)<<','<<m.reliefStarted<<','<<m.reliefAborted<<','
        <<m.frontRoutAborts<<','<<m.reserveRoutAborts<<','<<m.wornFrontSeconds<<','<<m.displacedFrontSeconds<<','<<m.lastAttack<<'\n';
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string_view(argv[1])=="--stagnation-report") {
            auto battle=std::make_unique<BattleSimulation>();battle->reset(UnitType::Spearman,true);
            for(auto& f:battle->formations) {f.x=f.targetX=0;f.morale=100;for(auto& g:f.organization.smallGroups)g.morale=100;}
            battle->running=true;battle->update(90);
            std::cout<<"team,group,unit,morale,fatigue,route,wait,decision,target,distance,nearest_enemy,cautious,offset_x,offset_z,approach_x,approach_z\n"<<std::fixed<<std::setprecision(3);
            for(unsigned team=0;team<2;++team)for(unsigned id=0;id<25;++id) {
                const auto& f=battle->formations[team];const auto& enemy=battle->formations[1-team];const auto& g=f.organization.smallGroups[id];
                if(g.routed || g.strength<=0)continue;
                float nearest=10000;for(unsigned other=0;other<25;++other)if(!enemy.organization.smallGroups[other].routed && enemy.organization.smallGroups[other].strength>0)
                    nearest=std::min(nearest,battleDistance(f.groupPosition(id),enemy.groupPosition(other)));
                std::cout<<team<<','<<id<<','<<static_cast<int>(f.groupUnit(id))<<','<<g.morale<<','<<g.fatigue<<','<<static_cast<int>(g.route)<<','
                    <<static_cast<int>(g.combatWait)<<','<<static_cast<int>(g.awareness.decision)<<','<<g.attackTarget<<','
                    <<(g.attackTarget<0?-1:battleDistance(f.groupPosition(id),enemy.groupPosition(g.attackTarget)))<<','<<nearest<<','<<g.awareness.cautious<<','
                    <<g.offsetX<<','<<g.offsetZ<<','<<g.approachX<<','<<g.approachZ<<'\n';
            }
            return 0;
        }
        if(argc==2 && std::string_view(argv[1])=="--relief-report") {
            std::cout<<"configuration,seconds,team,front,reserve,reserve_ready,front_route,reserve_route,reserve_routed,reserve_resting,reserve_fatigue,reserve_strength,reserve_morale,forward_gap,distance,left_blocked_leg,left_blocker_team,left_blocker_id,right_blocked_leg,right_blocker_team,right_blocker_id\n"<<std::fixed<<std::setprecision(3);
            for(unsigned configuration=0;configuration<3;++configuration)measure(configuration,false,180,false,true);
            return 0;
        }
        if(argc==2 && std::string_view(argv[1])=="--rest-report") {
            std::cout<<"seconds,team,group,morale,fatigue,strength,nearest_enemy,time_since_damage,nearby_routs,routed,relocating,blocked\n"<<std::fixed<<std::setprecision(3);
            measure(0,false,180,true);return 0;
        }
        const bool report=argc==2 && std::string_view(argv[1])=="--report";
        require(argc==1 || report,"Usage: mixed_battle_tests [--report|--rest-report|--relief-report|--stagnation-report]");
        if(report)std::cout<<"configuration,swapped,team,seconds,winner,strength,routed,exchanges,rest_started,rest_completed,reused,disengaged,regrouped,charges,attack_group_seconds,rest_group_seconds,blocked_group_seconds,fatigue_recovered,mean_attack_efficiency,relief_started,relief_aborted,front_rout_aborts,reserve_rout_aborts,worn_front_group_seconds,displaced_worn_front_group_seconds,last_attack_s\n"<<std::fixed<<std::setprecision(3);
        for(unsigned configuration=0;configuration<(report?3u:1u);++configuration) {
            const auto original=measure(configuration,false,report?180:30),swapped=measure(configuration,true,report?180:30);
            if(report)for(unsigned team=0;team<2;++team) { row(configuration,false,team,original);row(configuration,true,team,swapped); }
            compare(original,swapped);
        }
        if(!report)std::cout<<"Mixed battle endurance, casualties, fatigue and swapped-team recovery metrics passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
