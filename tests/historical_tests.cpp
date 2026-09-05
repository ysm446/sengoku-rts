#include "historical_scene.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
int main() {
    try {
        const auto s=history::sekigahara();
        for(const auto& a:s.armies) {
            require(!a.route.empty(),"Empty route");
            for(std::size_t i=1;i<a.route.size();++i) require(a.route[i].minute>a.route[i-1].minute,"Unordered route");
            for(int minute=480;minute<=870;++minute) {
                const auto p=history::sample(a,minute);
                require(std::isfinite(p.x)&&std::isfinite(p.z)&&std::abs(p.x)<65&&std::abs(p.z)<65,"Invalid pose");
            }
        }
        history::Player a,b;a.playing=b.playing=true;
        a.update(s,30);for(int i=0;i<300;++i)b.update(s,.1);
        require(std::abs(a.minute-b.minute)<1e-8,"Frame-dependent clock");
        a.playing=false;const auto saved=a.minute;a.update(s,10);require(a.minute==saved,"Pause advanced");
        a.seek(s,5000);require(a.minute==s.endMinute&&!a.playing,"End clamp");
        a.seek(s,-100);require(a.minute==s.startMinute,"Start clamp");
        require(history::sample(s.armies[5],719.9).side==history::Side::West,"Early allegiance change");
        require(history::sample(s.armies[5],720).side==history::Side::Changed,"Missing allegiance change");
        require(history::sample(s.armies[5],730).z>history::sample(s.armies[5],720).z,"Kobayakawa did not descend north");
        require(history::sample(s.armies[7],850).x==history::sample(s.armies[7],480).x,"Nangu army moved");
        HistoricalScene scene(s,{});const Camera camera{};
        scene.update(s,735,camera,5,true);const auto before=scene.scene.sprites;
        scene.update(s,870,camera,5,true);scene.update(s,480,camera,5,true);scene.update(s,735,camera,5,true);
        require(before.size()==scene.scene.sprites.size(),"Sprite capacity changed");
        for(std::size_t i=0;i<before.size();++i) {
            const auto& x=before[i];const auto& y=scene.scene.sprites[i];
            require(x.position.x==y.position.x&&x.position.y==y.position.y&&x.position.z==y.position.z&&x.tile==y.tile,"Rewind was not deterministic");
            require(y.tile<Scene::tileCount,"Invalid atlas tile");
        }
        require(history::eventIndex(s,719)==2 && history::eventIndex(s,720)==3,"Event boundary");
        std::cout<<"PASS historical timeline, routing, allegiance, rewind, sprite capacity\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
