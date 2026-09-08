#include "formation_world.h"
#include <numeric>

namespace {
struct Body {
    std::size_t formation;
    unsigned group;
    BattlePoint point;
    float heading;
    bool fighting, targetable;
};
}

void FormationWorld::fight(float seconds) {
    std::erase_if(impacts,[&](const auto& impact){return time-impact.time>=CombatImpact::lifetime;});
    // ID順で評価・加算する。格納順が変わっても、同距離の選択と丸め順を維持する。
    std::vector<std::size_t> order(formations.size());std::iota(order.begin(),order.end(),0);
    std::sort(order.begin(),order.end(),[&](auto a,auto b){return formations[a].id<formations[b].id;});
    std::vector<Body> bodies;
    for(auto i:order) {
        auto& entry=formations[i];auto& f=entry.formation;entry.targets={};
        for(unsigned g=0;g<25;++g) {
            auto& group=f.organization.smallGroups[g];group.canAttack=false;group.activeFighters=0;group.activeOpponents={};
            group.attackTarget=-1;
            if(group.strength>0)bodies.push_back({i,g,f.groupPosition(g),group.heading,!group.routed && !group.resting && !f.defeated(),!group.routed && !f.defeated()});
        }
    }
    if(!combatEnabled)return;
    const auto enemy=[&](std::size_t a,std::size_t b){return formations[bodies[a].formation].side!=formations[bodies[b].formation].side;};
    const auto clear=[&](BattlePoint p,BattlePoint q,std::size_t a,std::size_t b) {
        for(std::size_t k=0;k<bodies.size();++k)if(k!=a && k!=b && segmentDistance(p,q,bodies[k].point)<ContactFronts::halfExtent)return false;
        return true;
    };
    // 全員の旋回を先に完了し、接触面と損害は同じ状態を参照する。
    for(std::size_t a=0;a<bodies.size();++a) {
        auto& body=bodies[a];auto& entry=formations[body.formation];auto& f=entry.formation;
        auto& g=f.organization.smallGroups[body.group];
        if(!body.fighting || f.groupUnit(body.group)==UnitType::Archer)continue;
        const auto profile=meleeProfile(f.groupUnit(body.group));float best=profile.groupRange+.00001f;
        std::optional<std::size_t> target;
        for(std::size_t b=0;b<bodies.size();++b)if(enemy(a,b) && bodies[b].targetable) {
            const float distance=battleDistance(body.point,bodies[b].point);
            if(distance>=.001f && distance<best && clear(body.point,bodies[b].point,a,b)){best=distance;target=b;}
        }
        if(target) {
            const auto& other=bodies[*target];entry.targets[body.group]=WorldGroupId{formations[other.formation].id,other.group};
            g.heading=turnToward(g.heading,std::atan2(other.point.z-body.point.z,other.point.x-body.point.x),profile.turnRate*seconds);
        } else if(f.moving)g.heading=turnToward(g.heading,f.heading,profile.turnRate*seconds);
        body.heading=g.heading;
    }
    std::vector<float> damage(bodies.size());std::vector<BattlePoint> impactPoints(bodies.size());
    std::vector<float> strongest(bodies.size());
    constexpr float half=ContactFronts::halfExtent;
    for(std::size_t a=0;a<bodies.size();++a) {
        const auto& body=bodies[a];auto& f=formations[body.formation].formation;auto& g=f.organization.smallGroups[body.group];
        if(!body.fighting || f.groupUnit(body.group)==UnitType::Archer)continue;
        const auto profile=meleeProfile(f.groupUnit(body.group));
        std::vector<std::size_t> candidates;
        for(std::size_t b=0;b<bodies.size();++b)if(enemy(a,b) && bodies[b].targetable) {
            const float distance=battleDistance(body.point,bodies[b].point);
            if(distance>=2*half && distance<=profile.groupRange && clear(body.point,bodies[b].point,a,b))candidates.push_back(b);
        }
        std::vector<std::array<float,4>> widths(candidates.size());ContactFronts fronts;
        for(unsigned face=0;face<4;++face) {
            const float angle=body.heading-face*1.57079632679f;
            const BattlePoint normal{std::cos(angle),std::sin(angle)},tangent{-normal.z,normal.x};
            for(unsigned sample=0;sample<ContactFronts::samples;++sample) {
                const float offset=-half+(sample+.5f)*(2*half/ContactFronts::samples);
                const BattlePoint p{body.point.x+normal.x*half+tangent.x*offset,body.point.z+normal.z*half+tangent.z*offset};
                std::optional<std::size_t> nearest;float best=ContactFronts::reach+.00001f;
                for(std::size_t n=0;n<candidates.size();++n) {
                    const auto b=candidates[n];const auto& h=bodies[b];
                    const float c=std::cos(h.heading),s=std::sin(h.heading),dx=p.x-h.point.x,dz=p.z-h.point.z;
                    const float x=std::clamp(dx*c+dz*s,-half,half),z=std::clamp(-dx*s+dz*c,-half,half);
                    const BattlePoint q{h.point.x+x*c-z*s,h.point.z+x*s+z*c};const float distance=battleDistance(p,q);
                    if(distance<best && (q.x-p.x)*normal.x+(q.z-p.z)*normal.z>.00001f && clear(p,q,a,b)){best=distance;nearest=n;}
                }
                if(nearest)widths[*nearest][face]+=2*half/ContactFronts::samples;
            }
            // 既存の面別配分・毎秒2人の配置処理へ、軍勢数に依存しない合計幅を渡す。
            for(const auto& width:widths)fronts.faces[face].enemyWidths[0]+=width[face];
        }
        advanceFaceDeployment(g.faceDeployment,allocateContactFronts(fronts,g.strength),g.strength,seconds);
        const auto participation=participatingContactFronts(fronts,g.faceDeployment,g.strength);
        for(std::size_t n=0;n<candidates.size();++n) {
            float fighters=0;
            for(unsigned face=0;face<4;++face)if(fronts.faces[face].width()>0)
                fighters+=participation.faces[face].fighters()*widths[n][face]/fronts.faces[face].width();
            if(fighters<=0)continue;
            const auto b=candidates[n];const auto& h=bodies[b];const float distance=battleDistance(body.point,h.point);
            const float defense=(std::cos(h.heading)*(body.point.x-h.point.x)+std::sin(h.heading)*(body.point.z-h.point.z))/distance;
            const float bonus=defense<-.5f?1.5f:defense<.5f?1.25f:1;
            const float strike=profile.damagePerSecond*(fighters/5)*(.5f+f.cohesion/200)*(.5f+g.morale/200)*bonus*seconds*g.attackEfficiency();
            damage[b]+=strike;g.activeFighters+=fighters;g.canAttack=true;
            if(strike>strongest[b]){strongest[b]=strike;impactPoints[b]={(body.point.x+h.point.x)/2,(body.point.z+h.point.z)/2};}
        }
        if(g.canAttack){g.state=SmallGroupState::Engaged;f.state=FormationState::Engaged;}
        else g.state=f.moving?SmallGroupState::Advancing:SmallGroupState::Waiting;
    }
    // 全攻撃を算出後に損害を適用する。同じ刻みに倒された小組も、その刻みの攻撃を失わない。
    for(std::size_t b=0;b<bodies.size();++b) {
        const auto& body=bodies[b];auto& entry=formations[body.formation];auto& g=entry.formation.organization.smallGroups[body.group];
        float loss=std::min(g.strength,damage[b]);
        if(loss<=0)continue;
        // 連続量の端数だけが永続して通路を塞がないよう、被弾後0.001人未満を除く。
        if(g.strength-loss<.001f)loss=g.strength;
        g.strength=std::max(0.0f,g.strength-loss);g.morale=std::max(0.0f,g.morale-loss*2-seconds*.5f);g.lastDamageTime=time;
        entry.formation.cohesion=std::max(0.0f,entry.formation.cohesion-loss*.08f);
        entry.formation.state=FormationState::Engaged;
        if(time>=entry.nextImpactTime[body.group]) {
            impacts.push_back({{entry.id,body.group},impactPoints[b],time,loss});entry.nextImpactTime[body.group]=time+.22;
        }
    }
    for(auto& entry:formations) {
        auto& f=entry.formation;float strength=0,morale=0;
        for(auto& g:f.organization.smallGroups){strength+=g.strength;morale+=g.morale*g.strength;if(g.strength<=0){g.canAttack=false;g.activeFighters=0;}}
        f.strength=strength;f.morale=strength>0?morale/strength:0;
        if(strength<=0){f.state=FormationState::Routed;f.moving=false;f.movementBlocked=false;entry.targets={};}
    }
}
