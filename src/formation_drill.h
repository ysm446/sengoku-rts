#pragma once
#include "unit_type.h"
#include <algorithm>
#include <array>
#include <cmath>

// 実測値ではなく、兵種ごとの隊列移動を比較するための仮パラメータ。
struct MovementProfile { float speed, acceleration, turnRate, radius, columnSpacing, rowSpacing; };
inline MovementProfile movementProfile(UnitType type) {
    switch(type) {
    case UnitType::Cavalry:return {4.0f,1.3f,.7f,1.05f,2.3f,2.7f};
    case UnitType::Samurai:return {2.0f,2.5f,2.3f,.32f,.8f,.9f};
    case UnitType::Archer:return {1.8f,2.2f,2.0f,.38f,.95f,1.05f};
    default:return {1.7f,2.0f,1.8f,.38f,.95f,1.1f};
    }
}
struct DrillSoldier {float x=0,z=0,heading=0,speed=0;double animationTime=0;bool moving=false;};
class FormationDrill {
public:
    static constexpr unsigned columns=4, rows=6, count=columns*rows;
    std::array<DrillSoldier,count> soldiers{};
    UnitType unit=UnitType::Spearman;
    float x=0,z=-10,heading=1.570796327f,targetX=0,targetZ=18,speed=0;
    bool running=false;
    double time=0;
    FormationDrill(){reset(UnitType::Spearman);}
    void reset(UnitType type) {
        unit=type;x=0;z=-10;heading=1.570796327f;targetX=0;targetZ=18;speed=0;running=false;time=accumulator=0;
        for(unsigned i=0;i<count;++i) {const auto p=slot(i);soldiers[i]={p[0],p[1],heading};}
    }
    std::array<float,2> slot(unsigned index) const {
        const auto p=movementProfile(unit);
        const float right=(static_cast<float>(index%columns)-(columns-1)*.5f)*p.columnSpacing;
        const float forward=(static_cast<float>(index/columns)-(rows-1)*.5f)*p.rowSpacing;
        return {x-std::sin(heading)*right+std::cos(heading)*forward,z+std::cos(heading)*right+std::sin(heading)*forward};
    }
    void move(float tx,float tz){if(std::isfinite(tx)&&std::isfinite(tz)){targetX=std::clamp(tx,-45.0f,45.0f);targetZ=std::clamp(tz,-45.0f,45.0f);}}
    void hold(){targetX=x;targetZ=z;speed=0;}
    float error() const {
        float total=0;for(unsigned i=0;i<count;++i){const auto p=slot(i);total+=std::hypot(soldiers[i].x-p[0],soldiers[i].z-p[1]);}
        return total/count;
    }
    void update(double seconds) {
        if(!running || !std::isfinite(seconds) || seconds<=0)return;
        accumulator+=seconds;
        constexpr double stepTime=1.0/60;
        while(accumulator+1e-9>=stepTime){step(static_cast<float>(stepTime));time+=stepTime;accumulator-=stepTime;}
    }
private:
    double accumulator=0;
    static float turn(float current,float target,float limit){return current+std::clamp(std::remainder(target-current,6.283185307f),-limit,limit);}
    void step(float dt) {
        const auto p=movementProfile(unit);
        const float dx=targetX-x,dz=targetZ-z,distance=std::hypot(dx,dz);
        if(distance>.08f)heading=turn(heading,std::atan2(dz,dx),p.turnRate*dt);
        const float alignment=distance>.08f?std::max(0.0f,(dx*std::cos(heading)+dz*std::sin(heading))/distance):0;
        const float desired=std::min(p.speed,std::sqrt(2*p.acceleration*distance))*alignment*(error()>2.5f?0.0f:1.0f);
        speed+=std::clamp(desired-speed,-p.acceleration*dt,p.acceleration*dt);
        const float travel=std::min(distance,speed*dt);
        x+=std::cos(heading)*travel;z+=std::sin(heading)*travel;
        const auto previous=soldiers;
        for(unsigned i=0;i<count;++i) {
            auto& s=soldiers[i];const auto goal=slot(i);
            float vx=goal[0]-s.x,vz=goal[1]-s.z;
            const float gap=std::hypot(vx,vz);
            // 周囲の兵を避ける向きも加え、隊列の内側へ直進して重なることを抑える。
            for(unsigned j=0;j<count;++j)if(i!=j){
                const float ax=s.x-previous[j].x,az=s.z-previous[j].z,d=std::hypot(ax,az);
                if(d>.001f && d<p.radius*2.6f){const float force=(p.radius*2.6f-d)*2;vx+=ax/d*force;vz+=az/d*force;}
            }
            const float length=std::hypot(vx,vz);
            if(gap>.05f && length>.05f)s.heading=turn(s.heading,std::atan2(vz,vx),p.turnRate*1.5f*dt);
            else s.heading=turn(s.heading,heading,p.turnRate*dt);
            const float facing=length>.001f?std::max(0.0f,(vx*std::cos(s.heading)+vz*std::sin(s.heading))/length):0;
            const float wanted=length>.05f?std::min(p.speed*1.25f,gap*3)*facing:0;
            s.speed+=std::clamp(wanted-s.speed,-p.acceleration*2*dt,p.acceleration*dt);
            const float advance=std::min(gap,s.speed*dt);
            s.x+=std::cos(s.heading)*advance;s.z+=std::sin(s.heading)*advance;
        }
        // 簡易円コリジョン。対称補正で一方だけを優先しない。
        for(unsigned iteration=0;iteration<8;++iteration)for(unsigned i=0;i<count;++i)for(unsigned j=i+1;j<count;++j) {
            auto& a=soldiers[i];auto& b=soldiers[j];float ax=b.x-a.x,az=b.z-a.z,d=std::hypot(ax,az);
            if(d>=p.radius*2)continue;
            if(d<.0001f){ax=1;az=0;d=1;}
            const float correction=(p.radius*2-std::hypot(b.x-a.x,b.z-a.z))*.5f;
            a.x-=ax/d*correction;a.z-=az/d*correction;b.x+=ax/d*correction;b.z+=az/d*correction;
        }
        for(unsigned i=0;i<count;++i) {
            auto& s=soldiers[i];s.moving=std::hypot(s.x-previous[i].x,s.z-previous[i].z)>.001f;
            if(s.moving)s.animationTime+=dt;
        }
    }
};
