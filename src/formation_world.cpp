#include "formation_world.h"
#include <stdexcept>

std::size_t FormationWorld::indexOf(unsigned id) const {
    for(std::size_t i=0;i<formations.size();++i)if(formations[i].id==id)return i;
    throw std::invalid_argument("Unknown formation ID");
}
void FormationWorld::reset(std::vector<WorldFormation> initial) {
    for(std::size_t i=0;i<initial.size();++i) {
        const auto& f=initial[i].formation;
        if(!std::isfinite(f.x) || !std::isfinite(f.z) || std::abs(f.x)>boundary-14 || std::abs(f.z)>boundary-14)
            throw std::invalid_argument("Invalid deployment position");
        for(std::size_t j=0;j<i;++j)if(initial[j].id==initial[i].id)throw std::invalid_argument("Duplicate formation ID");
        initial[i].formation.targetX=f.x;initial[i].formation.targetZ=f.z;
        initial[i].formation.moving=false;initial[i].formation.movementBlocked=false;
        initial[i].targets={};initial[i].nextImpactTime={};
        initial[i].order=WorldOrder::Hold;initial[i].advanceTarget.reset();initial[i].withdrawalSeconds=0;
        initial[i].initialGroups=0;initial[i].initialStrength=0;
        for(const auto& g:f.organization.smallGroups)if(g.strength>0){++initial[i].initialGroups;initial[i].initialStrength+=g.nominalStrength;}
    }
    formations=std::move(initial);running=false;time=accumulator=0;impacts.clear();
}
void FormationWorld::move(unsigned id,BattlePoint target) {
    if(!std::isfinite(target.x) || !std::isfinite(target.z) || std::abs(target.x)>boundary-14 || std::abs(target.z)>boundary-14)
        throw std::invalid_argument("Move target outside battlefield");
    auto& entry=formations[indexOf(id)];auto& f=entry.formation;
    entry.order=WorldOrder::Move;entry.advanceTarget.reset();entry.withdrawalSeconds=0;
    f.targetX=target.x;f.targetZ=target.z;f.maneuverEnabled=true;
}
void FormationWorld::hold(unsigned id) {
    auto& entry=formations[indexOf(id)];auto& f=entry.formation;
    entry.order=WorldOrder::Hold;entry.advanceTarget.reset();entry.withdrawalSeconds=0;
    f.targetX=f.x;f.targetZ=f.z;f.moving=false;f.movementBlocked=false;f.maneuverEnabled=false;
    if(!f.defeated())f.state=FormationState::Idle;
}
void FormationWorld::advance(unsigned id) {
    auto& entry=formations[indexOf(id)];auto& f=entry.formation;
    if(f.defeated() || f.strength<=0)return;
    entry.order=WorldOrder::Advance;entry.advanceTarget.reset();entry.withdrawalSeconds=0;
    f.targetX=f.x;f.targetZ=f.z;f.maneuverEnabled=true;
}
void FormationWorld::advanceAll() {
    for(const auto& entry:formations)if(entry.order!=WorldOrder::Withdraw)advance(entry.id);
}
void FormationWorld::decide(float seconds) {
    // 位置を更新する前に全軍の命令先を決める。手動命令を自動で上書きしない。
    for(auto& entry:formations) {
        auto& f=entry.formation;
        if(entry.order!=WorldOrder::Advance || f.defeated() || f.strength<=0)continue;
        const auto nearest=nearestEnemy(entry.id);
        if(!nearest){entry.advanceTarget.reset();entry.withdrawalSeconds=0;f.targetX=f.x;f.targetZ=f.z;continue;}
        const auto distance=[&](unsigned id){const auto& other=formations[indexOf(id)].formation;return battleDistance({f.x,f.z},{other.x,other.z});};
        bool retained=false;
        if(entry.advanceTarget)for(const auto& other:formations)if(other.id==*entry.advanceTarget && other.side!=entry.side &&
            other.formation.strength>0 && !other.formation.defeated())retained=distance(*entry.advanceTarget)<=distance(*nearest)*1.25f;
        if(!retained)entry.advanceTarget=nearest;
        const auto& enemy=formations[indexOf(*entry.advanceTarget)].formation;
        const bool exhausted=(f.readyGroups()<=entry.initialGroups/4 && f.strength<entry.initialStrength*.6f) || f.strength<entry.initialStrength*.25f;
        entry.withdrawalSeconds=exhausted && distance(*nearest)<75 ? entry.withdrawalSeconds+seconds:0;
        if(entry.withdrawalSeconds>=2) {
            const auto& threat=formations[indexOf(*nearest)].formation;
            float dx=f.x-threat.x,dz=f.z-threat.z;const float length=std::hypot(dx,dz);
            if(length>.001f){dx/=length;dz/=length;}else{dx=-std::cos(f.heading);dz=-std::sin(f.heading);}
            const float limit=boundary-14;
            f.targetX=std::clamp(f.x+dx*40,-limit,limit);f.targetZ=std::clamp(f.z+dz*40,-limit,limit);
            entry.order=WorldOrder::Withdraw;entry.advanceTarget.reset();
        } else {f.targetX=enemy.x;f.targetZ=enemy.z;}
    }
}
std::optional<unsigned> FormationWorld::nearestEnemy(unsigned id) const {
    const auto& self=formations[indexOf(id)];std::optional<unsigned> result;float best=1e30f;
    for(const auto& other:formations) {
        if(other.side==self.side || other.formation.defeated() || other.formation.strength<=0)continue;
        const float distance=battleDistance({self.formation.x,self.formation.z},{other.formation.x,other.formation.z});
        if(distance<best || (distance==best && (!result || other.id<*result))) {best=distance;result=other.id;}
    }
    return result;
}
void FormationWorld::update(float seconds) {
    if(!std::isfinite(seconds) || seconds<0 || seconds>3600)throw std::invalid_argument("Invalid world timestep");
    if(!running)return;
    constexpr double dt=1.0/60;accumulator+=seconds;
    while(accumulator+1e-8>=dt){step(static_cast<float>(dt));time+=dt;accumulator-=dt;}
}
void FormationWorld::step(float seconds) {
    decide(seconds);
    std::vector<BattlePoint> motion(formations.size());std::vector<bool> blocked(formations.size());
    for(std::size_t i=0;i<formations.size();++i) {
        const auto& f=formations[i].formation;
        if(!f.maneuverEnabled || f.defeated() || f.strength<=0)continue;
        const float dx=f.targetX-f.x,dz=f.targetZ-f.z,length=std::hypot(dx,dz);
        if(length>.001f){const float amount=std::min(length,f.speed*seconds);motion[i]={dx/length*amount,dz/length*amount};}
    }
    // 同じ時点の全備を参照する。味方も障害物とし、相手が停止する場合も検査する。
    for(std::size_t i=0;i<formations.size();++i) {
        const auto delta=motion[i];if(delta.x==0 && delta.z==0)continue;
        const auto& f=formations[i].formation;
        for(std::size_t j=0;j<formations.size() && !blocked[i];++j) {
            const auto& other=formations[j].formation;
            for(unsigned a=0;a<25 && !blocked[i];++a) {
                const auto& g=f.organization.smallGroups[a];if(g.routed || g.strength<=0)continue;
                const auto p=f.groupPosition(a);
                for(unsigned b=0;b<25;++b) {
                    const auto& h=other.organization.smallGroups[b];if(h.strength<=0 || (i==j && !h.routed))continue;
                    const auto q=other.groupPosition(b);const BattlePoint relative{p.x-q.x,p.z-q.z};
                    const auto collides=[&](BattlePoint d) {
                        if(battleDistance(relative,{})<4.5f)return relative.x*d.x+relative.z*d.z<-.000001f;
                        return segmentDistance(relative,{relative.x+d.x,relative.z+d.z},{})<4.5f;
                    };
                    const auto otherMotion=h.routed?BattlePoint{}:motion[j];
                    if(collides(delta) || collides({delta.x-otherMotion.x,delta.z-otherMotion.z})){blocked[i]=true;break;}
                }
            }
        }
    }
    for(std::size_t i=0;i<formations.size();++i) {
        auto& f=formations[i].formation;f.movementBlocked=blocked[i];
        f.moving=!blocked[i] && (motion[i].x!=0 || motion[i].z!=0);
        if(f.moving) {f.x+=motion[i].x;f.z+=motion[i].z;f.heading=std::atan2(motion[i].z,motion[i].x);}
        if(!f.defeated())f.state=f.moving?FormationState::Marching:FormationState::Idle;
    }
    fight(seconds);
}
