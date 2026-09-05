#include "scene.h"
#include "sprite_sheet.h"
#include <iostream>
#include <set>
#include <stdexcept>

void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Asset directory required");
        const std::filesystem::path directory=argv[1];
        for(unsigned type=1;type<unitVisuals.size();++type) {
            const auto unit=static_cast<UnitType>(type);const auto& definition=unitVisual(unit);
            const std::wstring prefix=definition.assetPrefix;
            for(const auto* animation:{L"idle",L"walk",L"attack"}) {
                const unsigned frames=std::wstring_view(animation)==L"idle"?1:8;
                const auto pixels=loadSpriteSheet(directory/(prefix+L"_"+animation+L".png"),512,64*frames);
                for(unsigned direction=0;direction<8;++direction) {
                    std::set<std::vector<std::uint32_t>> poses;
                    for(unsigned frame=0;frame<frames;++frame) {
                        unsigned opaque=0;std::vector<std::uint32_t> tile;
                        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x) {
                            const auto pixel=pixels[(frame*64+y)*512+direction*64+x];tile.push_back(pixel);
                            const auto alpha=pixel>>24;require(alpha==0||alpha==255,"Non-binary alpha");
                            if(alpha) {++opaque;require(x>0&&x<63&&y>0&&y<63,"Clipped unit sprite");}
                        }
                        require(opaque>100,"Empty unit pose");poses.insert(std::move(tile));
                    }
                    require(frames==1||poses.size()>=4,"Animation lacks distinct poses");
                }
            }
            SceneOptions options;options.inspect=true;options.unit=unit;
            options.soldierSheet=directory/(prefix+L"_idle.png");options.walkSheet=directory/(prefix+L"_walk.png");
            options.attackSheet=directory/(prefix+L"_attack.png");
            auto scene=makeScene(1000,options);BattleSimulation simulation;simulation.time=.5;
            scene.inspectAttack=true;updateSceneSprites(scene,simulation,Camera{});
            for(const auto& sprite:scene.sprites)require(sprite.size.x==definition.attackSize&&sprite.pivot==.75f,"Attack scale or pivot");
            scene.inspectAttack=false;updateSceneSprites(scene,simulation,Camera{});
            for(const auto& sprite:scene.sprites)require(sprite.size.x==definition.idleSize&&sprite.pivot==definition.idlePivot,"Walk scale not restored");
        }
        std::cout<<"PASS: 3 units, 408 poses, transparency, animation diversity, attack/walk scale and pivot\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
