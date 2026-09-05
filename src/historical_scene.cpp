#include "historical_scene.h"
#include <windows.h>
#include <stdexcept>

namespace {
constexpr unsigned soldiers = 72, trail = 180, ring = 32, front = 16;
constexpr unsigned leaders = 12;
constexpr unsigned stride = soldiers + trail + ring + front + leaders + 1;
DirectX::XMFLOAT3 color(history::Side side) {
    return side == history::Side::East ? DirectX::XMFLOAT3{0.28f,0.64f,1.0f} :
        side == history::Side::West ? DirectX::XMFLOAT3{1.0f,0.34f,0.30f} : DirectX::XMFLOAT3{1.0f,0.8f,0.25f};
}
unsigned labelTile(unsigned index) { return index < 2 ? 14+index : 24+(index-2)/4*12+(index-2)%4; }
void label(Scene& scene, unsigned tile, const std::wstring& name, COLORREF background, bool mountain) {
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=64;
    info.bmiHeader.biHeight=-64; info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr; HDC dc=CreateCompatibleDC(nullptr);
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if (!dc || !bitmap) { if(bitmap) DeleteObject(bitmap); if(dc) DeleteDC(dc); throw std::runtime_error("Cannot create scenario label"); }
    auto old=SelectObject(dc,bitmap);
    RECT bounds{0,0,64,64}; auto brush=CreateSolidBrush(background); FillRect(dc,&bounds,brush); DeleteObject(brush);
    auto font=CreateFontW(mountain?15:19,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Yu Gothic UI");
    auto oldFont=SelectObject(dc,font); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(248,240,219));
    std::wstring text=name;
    if (!mountain && text.size()>3) text.insert(text.size()==5 && text.back()!=L'ら'?3:text.size()/2,L"\n");
    RECT textRect{1,mountain?22:9,63,63}; DrawTextW(dc,text.c_str(),-1,&textRect,DT_CENTER|DT_NOPREFIX);
    const auto* pixels=static_cast<const std::uint32_t*>(bits);
    for (unsigned y=0;y<64;++y) for(unsigned x=0;x<64;++x) {
        const auto p=pixels[y*64+x];
        const bool visible=mountain ? y>=19 && y<44 : x>=2 && x<62 && y>=3 && y<61;
        scene.atlas[((tile/12)*64+y)*Scene::atlasWidth+(tile%12)*64+x]=visible?
            ((p&255)<<16)|(p&0xff00)|((p>>16)&255)|0xff000000u:0;
    }
    SelectObject(dc,oldFont); DeleteObject(font); SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc);
}
}

HistoricalScene::HistoricalScene(const history::Scenario& scenario, const SceneOptions& options) {
    scene=makeScene(1000,options);
    scene.terrain.clear(); scene.sprites.clear(); scene.soldierBindings.clear();
    for(int z=-75;z<75;++z) for(int x=-75;x<75;++x) {
        const float fx=static_cast<float>(x), fz=static_cast<float>(z);
        const float h=history::height(scenario,fx,fz);
        const float slope=history::height(scenario,fx+0.5f,fz)-history::height(scenario,fx-0.5f,fz);
        const float shade=std::clamp(1.0f-slope*0.24f,0.65f,1.15f);
        const bool river=std::abs(fz-(-5.5f+2.8f*std::sin(fx*0.10f)))<0.7f && fx>-34;
        const bool road=std::abs(fz-(-12+3*std::sin(fx*.045f)))<0.75f;
        const float hill=std::clamp(h/8,0.0f,1.0f);
        DirectX::XMFLOAT3 tint = river ? DirectX::XMFLOAT3{.29f,.49f,.55f} : road ? DirectX::XMFLOAT3{.65f,.56f,.37f} :
            DirectX::XMFLOAT3{.48f-.20f*hill,.49f-.12f*hill,.32f-.07f*hill};
        const float grain=1+static_cast<float>((x*x+z*z*3)%7)*.007f;
        tint.x*=shade*grain; tint.y*=shade*grain; tint.z*=shade*grain;
        auto vertex=[&](float vx,float vz) {return TerrainVertex{{vx,history::height(scenario,vx,vz),vz},tint};};
        const auto a=vertex(fx,fz), b=vertex(fx+1,fz), c=vertex(fx,fz+1), d=vertex(fx+1,fz+1);
        scene.terrain.insert(scene.terrain.end(),{a,c,b,b,c,d});
    }
    for(unsigned i=0;i<scenario.armies.size();++i) {
        const unsigned tile=labelTile(i); labelTiles.push_back(tile);
        label(scene,tile,scenario.armies[i].name,scenario.armies[i].side==history::Side::East?RGB(29,66,93):RGB(105,42,39),false);
    }
    for(unsigned i=0;i<scenario.hills.size();++i) {
        const auto& h=scenario.hills[i]; const unsigned tile=labelTile(static_cast<unsigned>(scenario.armies.size())+i);
        label(scene,tile,h.name,RGB(46,54,43),true);
        scene.sprites.push_back({{h.x,history::height(scenario,h.x,h.z)+5,h.z+3},{8,6},tile,{1,1,1}});
    }
    const unsigned compassTile=labelTile(static_cast<unsigned>(scenario.armies.size()+scenario.hills.size()));
    label(scene,compassTile,L"北 N",RGB(46,54,43),true);
    scene.sprites.push_back({{57,5,39},{7,6},compassTile,{1,1,1}});
    for(unsigned i=0;i<20;++i) {
        const float z=29+static_cast<float>(i)*.4f;
        scene.sprites.push_back({{57,history::height(scenario,57,z)+.2f,z},{.65f,.65f},12,{.95f,.91f,.74f},{1,0,0},{0,0,1}});
    }
    for(unsigned i=0;i<6;++i) for(unsigned arm=0;arm<2;++arm) {
        const float x=57+(arm?1.0f:-1.0f)*i*.3f,z=37-i*.4f;
        scene.sprites.push_back({{x,history::height(scenario,x,z)+.2f,z},{.7f,.7f},12,{.95f,.91f,.74f},{1,0,0},{0,0,1}});
    }
    // 森林は丘陵の外周へ置き、布陣と矢印の視認性を保つ。
    for(int z=-65;z<=65;z+=4) for(int x=-65;x<=65;x+=4) {
        const float fx=static_cast<float>(x), fz=static_cast<float>(z), h=history::height(scenario,fx,fz);
        if(h>3 && h<9 && (x*x+z*z)%5<2)
            scene.sprites.push_back({{fx,h,fz},{1.7f,2.3f},2,{1,1,1}});
    }
    armyStart=scene.sprites.size();
    labelPositions.resize(scenario.armies.size());
    scene.sprites.resize(armyStart+scenario.armies.size()*stride);
    scene.soldierCount=static_cast<unsigned>(scenario.armies.size())*soldiers;
    update(scenario,scenario.startMinute,Camera{},-1,true);
}
void HistoricalScene::update(const history::Scenario& scenario,double minute,const Camera& camera,int selected,bool arrows) {
    for(std::size_t i=armyStart;i<scene.sprites.size();++i) scene.sprites[i]={{0,0,0},{0,0},13,{1,1,1}};
    std::vector<DirectX::XMFLOAT2> occupiedLabels;
    const auto right=camera.right(), up=camera.up();
    for(const auto& h:scenario.hills) {
        const float y=history::height(scenario,h.x,h.z)+5,z=h.z+3;
        occupiedLabels.push_back({h.x*right.x+z*right.z,h.x*up.x+y*up.y+z*up.z});
    }
    for(unsigned i=0;i<scenario.armies.size();++i) {
        const auto& army=scenario.armies[i]; const auto p=history::sample(army,minute);
        const auto tint=color(p.side); const auto base=armyStart+i*stride;
        float heading=p.heading, reach=1.4f;
        if(p.action==history::Action::Fighting) {
            float nearest=10;
            for(unsigned opponent=0;opponent<scenario.armies.size();++opponent) {
                if(opponent==i)continue;
                const auto other=history::sample(scenario.armies[opponent],minute);
                if((p.side==history::Side::West)==(other.side==history::Side::West) || other.action!=history::Action::Fighting)continue;
                const float distance=std::hypot(other.x-p.x,other.z-p.z);
                if(distance<nearest && distance>.1f) {nearest=distance;heading=std::atan2(other.z-p.z,other.x-p.x);reach=std::clamp(distance*.5f-.25f,1.4f,4.5f);}
            }
        }
        const float dx=std::cos(heading), dz=std::sin(heading);
        auto dot=[&](unsigned slot,float x,float z,float size,DirectX::XMFLOAT3 c) {
            scene.sprites[base+slot]={{x,history::height(scenario,x,z)+.18f,z},{size,size},12,c,{1,0,0},{0,0,1}};
        };
        if(p.action!=history::Action::Dispersed) {
            for(unsigned j=0;j<soldiers;++j) {
                const float across=(static_cast<float>(j%12)-5.5f)*.42f, depth=-1+static_cast<float>(j/12)/5*(reach+1);
                const float spread=p.action==history::Action::Retreating?1.5f:1.0f;
                const float x=p.x+(-dz*across+dx*depth)*spread, z=p.z+(dx*across+dz*depth)*spread;
                unsigned tile=scene.generatedSoldiers?4+camera.spriteDirection(heading):0;
                if(p.moving && scene.animatedSoldiers) tile+=12*(1+(static_cast<unsigned>(minute*8)+j/12)%8);
                else if(p.action==history::Action::Fighting && j/12==5 && scene.attackSoldiers)
                    tile+=12*(Scene::attackRow+(static_cast<unsigned>(minute*6)+j)%8);
                scene.sprites[base+j]={{x,history::height(scenario,x,z)+.08f,z},{1.2f,1.5f},tile,tint};
            }
            for(unsigned j=0;j<front;++j) {
                const float across=(static_cast<float>(j)-7.5f)*.4f;
                dot(soldiers+trail+ring+j,p.x-dz*across+dx*reach,p.z+dx*across+dz*reach,.65f,tint);
            }
        }
        // 移動履歴を固定数の点へサンプリング。過去の部分ほど暗くする。
        if(arrows) {
            std::vector<DirectX::XMFLOAT2> path;
            for(const auto& key:army.route) {
                if(key.minute>minute)break;
                if(path.empty() || std::hypot(key.x-path.back().x,key.z-path.back().y)>.01f)path.push_back({key.x,key.z});
            }
            if(path.empty() || std::hypot(p.x-path.back().x,p.z-path.back().y)>.01f)path.push_back({p.x,p.z});
            float total=0;
            for(std::size_t k=1;k<path.size();++k)total+=std::hypot(path[k].x-path[k-1].x,path[k].y-path[k-1].y);
            for(unsigned j=0;j<trail-12;++j) {
                if(total<.01f)break;
                float distance=total*j/(trail-13);
                for(std::size_t k=1;k<path.size();++k) {
                    const auto a=path[k-1], b=path[k];const float length=std::hypot(b.x-a.x,b.y-a.y);
                    if(distance>length && k+1<path.size()){distance-=length;continue;}
                    const float t=std::clamp(distance/length,0.0f,1.0f),fade=.38f+.5f*j/(trail-13);
                    dot(soldiers+j,a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,1.25f,{tint.x*fade,tint.y*fade,tint.z*fade});break;
                }
            }
            if(path.size()>1) for(unsigned j=0;j<6;++j) for(unsigned arm=0;arm<2;++arm) {
                const auto last=path.back(), previous=path[path.size()-2];
                const float length=std::hypot(last.x-previous.x,last.y-previous.y);
                const float hx=(last.x-previous.x)/length,hz=(last.y-previous.y)/length;
                const float back=static_cast<float>(j)*.45f, side=(arm?1.0f:-1.0f)*back*.55f;
                dot(soldiers+trail-12+j*2+arm,p.x+hx*(2-back)-hz*side,p.z+hz*(2-back)+hx*side,1.25f,tint);
            }
        }
        if(selected==static_cast<int>(i)) for(unsigned j=0;j<ring;++j) {
            const float angle=DirectX::XM_2PI*j/ring;
            dot(soldiers+trail+j,p.x+std::cos(angle)*4,p.z+std::sin(angle)*4,.6f,{1,1,.75f});
        }
        const float fade=p.action==history::Action::Dispersed?.65f:1;
        DirectX::XMFLOAT3 labelPosition{p.x,history::height(scenario,p.x,p.z)+4,p.z};
        // 平行投影の画面位置を保ったまま手前へ寄せ、説明ラベルが丘や兵士に埋もれないようにする。
        labelPosition.x+=60*up.y*std::cos(camera.yaw);
        labelPosition.y+=60*std::sqrt(1-up.y*up.y);
        labelPosition.z+=60*up.y*std::sin(camera.yaw);
        const float screenX=labelPosition.x*right.x+labelPosition.z*right.z;
        const float screenY=labelPosition.x*up.x+labelPosition.y*up.y+labelPosition.z*up.z;
        std::vector<DirectX::XMFLOAT2> candidates;
        for(int y=-3;y<=3;++y)for(int x=-3;x<=3;++x)candidates.push_back({x*7.4f,y*6.2f});
        std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.x*a.x+a.y*a.y<b.x*b.x+b.y*b.y;});
        DirectX::XMFLOAT2 shift{};
        for(const auto& candidate:candidates) {
            bool overlap=false;
            for(const auto& other:occupiedLabels) if(std::abs(other.x-screenX-candidate.x)<7.3f && std::abs(other.y-screenY-candidate.y)<5.9f) overlap=true;
            if(!overlap){shift=candidate;break;}
        }
        occupiedLabels.push_back({screenX+shift.x,screenY+shift.y});
        const DirectX::XMFLOAT3 offset{right.x*shift.x+up.x*shift.y,up.y*shift.y,right.z*shift.x+up.z*shift.y};
        for(unsigned j=0;j<leaders;++j) if(shift.x!=0 || shift.y!=0) {
            const float t=static_cast<float>(j)/leaders;
            scene.sprites[base+soldiers+trail+ring+front+j]={{labelPosition.x+offset.x*t,labelPosition.y+offset.y*t,labelPosition.z+offset.z*t},{.3f,.3f},12,tint};
        }
        labelPosition.x+=offset.x;labelPosition.y+=offset.y;labelPosition.z+=offset.z;
        labelPositions[i]=labelPosition;
        scene.sprites[base+stride-1]={labelPosition,{6,6},labelTiles[i],{fade,fade,fade}};
    }
}
