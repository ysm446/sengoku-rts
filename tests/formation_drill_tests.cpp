#include "formation_drill.h"
#include <iostream>
#include <stdexcept>

void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){
    try{
        for(unsigned type=0;type<4;++type){
            FormationDrill drill;drill.reset(static_cast<UnitType>(type));const auto p=movementProfile(drill.unit);
            drill.update(1);require(drill.z==-10,"Paused movement");
            drill.running=true;
            for(unsigned frame=0;frame<3600;++frame){
                if(frame==600)drill.move(18,5);
                drill.update(1.0/60);
                require(std::isfinite(drill.error()),"Non-finite formation");
                for(unsigned i=0;i<drill.count;++i)for(unsigned j=i+1;j<drill.count;++j)
                    require(std::hypot(drill.soldiers[i].x-drill.soldiers[j].x,drill.soldiers[i].z-drill.soldiers[j].z)>=p.radius*2-.03f,"Soldier overlap");
            }
            require(std::hypot(drill.x-18,drill.z-5)<.3f,"Formation did not reach destination");
            require(drill.error()<1,"Formation did not reform");
            const float x=drill.x,z=drill.z;drill.hold();drill.update(2);
            require(drill.x==x&&drill.z==z,"Hold moved the formation anchor");
            FormationDrill a,b;a.reset(drill.unit);b.reset(drill.unit);a.running=b.running=true;
            a.update(4);for(unsigned i=0;i<120;++i)b.update(1.0/30);
            require(std::abs(a.z-b.z)<1e-6f,"Frame-dependent movement");
            for(unsigned i=0;i<a.count;++i)require(std::abs(a.soldiers[i].x-b.soldiers[i].x)<1e-6f,"Frame-dependent individual movement");
        }
        std::cout<<"PASS: four unit profiles, turn/reform, collision, pause/hold, fixed updates\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
