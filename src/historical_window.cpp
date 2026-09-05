#include "historical_window.h"
#include "historical_scene.h"
#include "renderer.h"
#include "resource.h"
#include <windowsx.h>
#include <chrono>
#include <fstream>
#include <stdexcept>

namespace {
constexpr int sidebar=328;
struct Demo {
    history::Scenario scenario=history::sekigahara();
    history::Player player;
    Camera camera;
    const HistoricalScene* visual=nullptr;
    HWND window=nullptr, viewport=nullptr;
    int width=1600,height=960, selected=-1, dragX=0;
    bool arrows=true, orbit=false, scrubbing=false, battle=false;
    HFONT normal=nullptr, heading=nullptr, smallFont=nullptr;
    RECT map() const {return {16,84,width-sidebar-16,height-158};}
    RECT timeline() const {return {30,height-62,width-30,height-40};}
    void resetCamera() {camera=Camera{}; camera.span=145; camera.x=2; camera.yaw=-DirectX::XM_PIDIV2;}
    void seekPixel(int x) {
        const auto r=timeline(); player.playing=false;
        player.seek(scenario,scenario.startMinute+(scenario.endMinute-scenario.startMinute)*(x-r.left)/(r.right-r.left));
        InvalidateRect(window,nullptr,FALSE);
    }
    void layout() {
        RECT r{}; GetClientRect(window,&r); width=r.right;height=r.bottom;
        const auto m=map(); if(viewport) MoveWindow(viewport,m.left,m.top,m.right-m.left,m.bottom-m.top,TRUE);
        InvalidateRect(window,nullptr,FALSE);
    }
    void pick(int x,int y) {
        const auto m=map(); const float w=static_cast<float>(m.right-m.left),h=static_cast<float>(m.bottom-m.top);
        selected=-1; float nearest=38*38;
        for(unsigned i=0;i<scenario.armies.size();++i) {
            const auto p=history::sample(scenario.armies[i],player.minute);
            const auto label=visual?visual->labelPositions[i]:DirectX::XMFLOAT3{p.x,history::height(scenario,p.x,p.z)+5,p.z};
            const auto point=DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(label.x,label.y+1,label.z,1),camera.matrix(w/h));
            const float px=(DirectX::XMVectorGetX(point)+1)*w*.5f,py=(1-DirectX::XMVectorGetY(point))*h*.5f;
            const float d=(px-x)*(px-x)+(py-y)*(py-y);
            if(d<nearest) {nearest=d;selected=static_cast<int>(i);}
        }
        InvalidateRect(window,nullptr,FALSE);
    }
    void key(WPARAM key) {
        if(key==VK_SPACE) { if(player.minute>=scenario.endMinute) player.seek(scenario,scenario.startMinute); player.playing=!player.playing; }
        if(key==VK_HOME) {player.seek(scenario,scenario.startMinute);player.playing=false;}
        if(key==VK_LEFT || key==VK_RIGHT) {player.playing=false;player.seek(scenario,player.minute+(key==VK_LEFT?-10:10));}
        if(key=='R') resetCamera();
        if(key=='G') arrows=!arrows;
        if(key==VK_ESCAPE) DestroyWindow(window);
        InvalidateRect(window,nullptr,FALSE);
    }
    void click(int x,int y) {
        if(y>=22 && y<=59) {
            if(x>=320 && x<442) key(VK_SPACE);
            else if(x>=450 && x<562) key(VK_HOME);
            else if(x>=570 && x<690) player.speed=player.speed==2?6:player.speed==6?12:2;
            else if(x>=698 && x<828) arrows=!arrows;
            else if(x>=836 && x<950) resetCamera();
            else if(x>=width-170) {battle=true;DestroyWindow(window);return;}
        }
        if(y>=height-84 && y<=height-23) {scrubbing=true;SetCapture(window);seekPixel(x);}
        const int sx=width-sidebar;
        if(x>=sx && y>=266 && y<266+static_cast<int>(scenario.events.size())*27) {
            const auto index=static_cast<std::size_t>((y-266)/27); player.playing=false;player.seek(scenario,scenario.events[index].minute);
        }
        if(x>=sx && y>=490 && y<490+7*29) {
            const int index=(y-490)/29+(x>=sx+156?7:0);
            if(index<static_cast<int>(scenario.armies.size())) selected=index;
        }
        InvalidateRect(window,nullptr,FALSE);
    }
    void paint(HDC target=nullptr) {
        PAINTSTRUCT ps{}; HDC dc=target?target:BeginPaint(window,&ps);
        RECT all{0,0,width,height}; const auto bg=CreateSolidBrush(RGB(19,27,32));FillRect(dc,&all,bg);DeleteObject(bg);
        SetBkMode(dc,TRANSPARENT);
        auto text=[&](int x,int y,int w,int h,const std::wstring& value,HFONT font,COLORREF color=RGB(226,227,218)) {
            SelectObject(dc,font);SetTextColor(dc,color);RECT r{x,y,x+w,y+h};DrawTextW(dc,value.c_str(),-1,&r,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
        };
        auto box=[&](RECT r,COLORREF c) {auto b=CreateSolidBrush(c);FillRect(dc,&r,b);DeleteObject(b);};
        auto button=[&](int x,int w,const std::wstring& name) {box({x,22,x+w,60},RGB(43,56,63));text(x+12,30,w-20,26,name,normal);};
        text(24,15,282,35,L"関ヶ原  /  1600",heading);
        text(25,52,290,24,L"史実再生 · 通説に基づく概略デモ",smallFont,RGB(180,173,142));
        button(320,122,player.playing?L"一時停止":L"再生する");button(450,112,L"最初に戻る");
        button(570,120,std::to_wstring(static_cast<int>(player.speed))+L" 分 / 秒");button(698,130,arrows?L"進軍矢印 ON":L"進軍矢印 OFF");button(836,114,L"全景へ");
        button(width-170,150,L"戦闘試作へ");
        const int sx=width-sidebar;
        const auto& event=scenario.events[history::eventIndex(scenario,player.minute)];
        text(sx,91,300,24,L"局面  /  推定時刻",smallFont,RGB(165,179,183));
        text(sx,116,300,42,history::clockText(player.minute),heading,RGB(240,213,145));
        text(sx,164,300,34,event.title,normal);
        text(sx,201,300,63,event.description,smallFont);
        for(unsigned i=0;i<scenario.events.size();++i) {
            const int y=266+static_cast<int>(i)*27;
            if(i==history::eventIndex(scenario,player.minute)) box({sx-5,y,sx+306,y+26},RGB(49,62,66));
            text(sx+4,y+3,300,24,history::clockText(scenario.events[i].minute)+L"  "+scenario.events[i].title,smallFont);
        }
        text(sx,463,300,23,L"軍勢を選択  /  地図の旗もクリック可",smallFont,RGB(165,179,183));
        for(unsigned i=0;i<scenario.armies.size();++i) {
            const int x=sx+static_cast<int>(i/7)*156,y=490+static_cast<int>(i%7)*29;
            if(selected==static_cast<int>(i)) box({x-3,y,x+149,y+27},RGB(58,67,70));
            const auto p=history::sample(scenario.armies[i],player.minute);
            const COLORREF c=p.side==history::Side::East?RGB(114,184,250):p.side==history::Side::West?RGB(247,134,119):RGB(240,208,100);
            text(x+3,y+3,146,25,scenario.armies[i].name,smallFont,c);
        }
        if(selected>=0 && height>900) {
            const auto& army=scenario.armies[static_cast<unsigned>(selected)];const auto p=history::sample(army,player.minute);
            text(sx,712,302,26,army.name+L"  /  "+history::actionText(p.action),normal);
            text(sx,747,300,68,army.note,smallFont);
        }
        const auto m=map();
        text(22,m.bottom+12,m.right-28,24,L"青：東軍    赤：西軍    金：転進した軍勢    帯：軍勢の正面    暗い矢印：移動履歴",smallFont);
        text(22,m.bottom+40,m.right-28,25,L"WASD：移動   Q/E・中ドラッグ：回転   ホイール：拡大   Space：再生   ←/→：10分移動   R：全景",smallFont,RGB(161,176,180));
        text(24,height-97,width-48,26,L"模式地形・推定経路／兵数を表す縮尺ではありません。自律戦闘・介入は今後の実装です。",smallFont,RGB(178,174,152));
        const auto r=timeline();box(r,RGB(55,70,76));
        const int pos=r.left+static_cast<int>((player.minute-scenario.startMinute)/(scenario.endMinute-scenario.startMinute)*(r.right-r.left));
        box({r.left,r.top,pos,r.bottom},RGB(165,139,84));
        for(const auto& e:scenario.events) {
            const int x=r.left+static_cast<int>((e.minute-scenario.startMinute)/(scenario.endMinute-scenario.startMinute)*(r.right-r.left));
            box({x,r.top-4,x+2,r.bottom+4},RGB(211,204,172));
        }
        box({pos-3,r.top-6,pos+4,r.bottom+6},RGB(255,235,173));
        text(30,height-27,250,24,L"08:00  開戦",smallFont);text(width-190,height-27,160,24,L"14:30  再生終了",smallFont);
        if(!target)EndPaint(window,&ps);
    }
    void captureUI(const std::filesystem::path& mapCapture) {
        // テストでは通常のUI描画とGPU読み戻しを同じ画像へ合成する。
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        void* pixels=nullptr;HDC dc=CreateCompatibleDC(nullptr),source=CreateCompatibleDC(nullptr);
        auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        auto mapBitmap=static_cast<HBITMAP>(LoadImageW(nullptr,mapCapture.c_str(),IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_CREATEDIBSECTION));
        if(!dc||!source||!bitmap||!mapBitmap) {
            if(bitmap)DeleteObject(bitmap);if(mapBitmap)DeleteObject(mapBitmap);if(dc)DeleteDC(dc);if(source)DeleteDC(source);
            throw std::runtime_error("Cannot capture historical UI");
        }
        const auto old=SelectObject(dc,bitmap),oldMap=SelectObject(source,mapBitmap);paint(dc);
        const auto m=map();BitBlt(dc,m.left,m.top,m.right-m.left,m.bottom-m.top,source,0,0,SRCCOPY);GdiFlush();
        BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info.bmiHeader);
        file.bfSize=file.bfOffBits+width*height*4;
        std::ofstream output(mapCapture.wstring()+L".ui.bmp",std::ios::binary);
        output.write(reinterpret_cast<const char*>(&file),sizeof(file));output.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(info.bmiHeader));
        output.write(static_cast<const char*>(pixels),static_cast<std::streamsize>(width)*height*4);
        const bool ok=static_cast<bool>(output);
        SelectObject(dc,old);SelectObject(source,oldMap);DeleteObject(bitmap);DeleteObject(mapBitmap);DeleteDC(dc);DeleteDC(source);
        if(!ok)throw std::runtime_error("Cannot write historical UI capture");
    }
};
LRESULT CALLBACK demoProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    auto* d=reinterpret_cast<Demo*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE) {d=static_cast<Demo*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(d));}
    if(!d) return DefWindowProcW(window,message,wparam,lparam);
    const bool child=window==d->viewport || (GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD)!=0;
    switch(message) {
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:if(!child){d->paint();return 0;}break;
    case WM_SIZE:if(!child && d->window)d->layout();return 0;
    case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize={1200,920};return 0;
    case WM_KEYDOWN:if(!(lparam&(1LL<<30)))d->key(wparam);return 0;
    case WM_MOUSEWHEEL:d->camera.zoom(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam))/WHEEL_DELTA);return 0;
    case WM_LBUTTONDOWN:SetFocus(d->window);if(child)d->pick(GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam));else d->click(GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam));return 0;
    case WM_LBUTTONUP:d->scrubbing=false;if(GetCapture()==window)ReleaseCapture();return 0;
    case WM_MBUTTONDOWN:if(child){d->orbit=true;d->dragX=GET_X_LPARAM(lparam);SetCapture(window);}return 0;
    case WM_MOUSEMOVE:
        if(d->orbit) {const int x=GET_X_LPARAM(lparam);d->camera.rotate((x-d->dragX)*.006f);d->dragX=x;}
        if(d->scrubbing && !child)d->seekPixel(GET_X_LPARAM(lparam));return 0;
    case WM_MBUTTONUP:d->orbit=false;if(GetCapture()==window)ReleaseCapture();return 0;
    case WM_CAPTURECHANGED:d->orbit=false;d->scrubbing=false;return 0;
    case WM_KILLFOCUS:d->orbit=false;d->scrubbing=false;if(GetCapture()==window)ReleaseCapture();return 0;
    case WM_DESTROY:if(!child)PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
}
int runHistoricalDemo(HINSTANCE instance,int show,bool smoke,bool warp,const std::filesystem::path& capture,
    const std::filesystem::path& directory,float yawDegrees) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    Demo d;d.resetCamera();d.camera.rotate(DirectX::XMConvertToRadians(yawDegrees));
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=demoProc;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.lpszClassName=L"SengokuHistoricalDemo";wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(IDI_SENGOKU_APP));
    if(!RegisterClassExW(&wc))throw std::runtime_error("Cannot register historical demo window");
    RECT bounds{0,0,d.width,d.height};AdjustWindowRectEx(&bounds,WS_OVERLAPPEDWINDOW,FALSE,0);
    d.window=CreateWindowExW(0,wc.lpszClassName,L"関ヶ原 | 史実再生デモ",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
        CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,instance,&d);
    if(!d.window)throw std::runtime_error("Cannot create historical demo window");
    d.viewport=CreateWindowExW(0,wc.lpszClassName,L"戦場",WS_CHILD|WS_VISIBLE,0,0,100,100,d.window,nullptr,instance,&d);
    if(!d.viewport){DestroyWindow(d.window);throw std::runtime_error("Cannot create historical viewport");}
    auto makeFont=[](int size,int weight){return CreateFontW(size,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Yu Gothic UI");};
    d.normal=makeFont(18,FW_NORMAL);d.heading=makeFont(30,FW_BOLD);d.smallFont=makeFont(15,FW_NORMAL);d.layout();
    try {
        const auto m=d.map();Renderer renderer(d.viewport,m.right-m.left,m.bottom-m.top,warp,directory/L"shaders/battlefield.hlsl");
        SceneOptions options;
        const auto idle=directory/L"assets/sprites/ashigaru_idle.png",walk=directory/L"assets/sprites/ashigaru_walk.png",attack=directory/L"assets/sprites/ashigaru_attack.png";
        if(std::filesystem::exists(idle))options.soldierSheet=idle;
        if(std::filesystem::exists(walk))options.walkSheet=walk;
        if(std::filesystem::exists(attack))options.attackSheet=attack;
        HistoricalScene scene(d.scenario,options);d.visual=&scene;renderer.setScene(scene.scene);
        if(!smoke)ShowWindow(d.window,show);
        auto previous=std::chrono::steady_clock::now();double repaint=0;unsigned frame=0;bool running=true;
        while(running) {
            MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {if(msg.message==WM_QUIT){running=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}
            if(!running)break;
            if(IsIconic(d.window)) {WaitMessage();previous=std::chrono::steady_clock::now();continue;}
            const auto now=std::chrono::steady_clock::now();const double dt=std::min(std::chrono::duration<double>(now-previous).count(),.1);previous=now;
            if(!smoke && GetForegroundWindow()==d.window) {
                auto down=[](int k){return GetAsyncKeyState(k)&0x8000?1.0f:0.0f;};
                d.camera.pan(down('D')-down('A'),down('W')-down('S'),static_cast<float>(dt));
                d.camera.rotate((down('E')-down('Q'))*static_cast<float>(dt));
            }
            d.player.update(d.scenario,dt);
            if(smoke) {
                const double times[]={480,650,735,775,820,870,480};d.player.seek(d.scenario,times[frame]);
                if(frame==0) {
                    SendMessageW(d.window,WM_KEYDOWN,VK_SPACE,0);
                    if(!d.player.playing)throw std::runtime_error("Historical play input failed");
                    SendMessageW(d.window,WM_KEYDOWN,VK_SPACE,0);
                    const auto r=d.timeline();SendMessageW(d.window,WM_LBUTTONDOWN,0,MAKELPARAM((r.left+r.right)/2,r.top));
                    SendMessageW(d.window,WM_LBUTTONUP,0,0);
                    if(std::abs(d.player.minute-675)>1 || d.player.playing)throw std::runtime_error("Historical seek input failed");
                    SendMessageW(d.window,WM_KEYDOWN,VK_HOME,0);
                    if(d.player.minute!=480)throw std::runtime_error("Historical reset input failed");
                }
                if(frame==2) {
                    SendMessageW(d.window,WM_LBUTTONDOWN,0,MAKELPARAM(d.width-sidebar+12,490+5*29+5));
                    if(d.selected!=5)throw std::runtime_error("Historical army selection failed");
                    SendMessageW(d.window,WM_KEYDOWN,'G',0);
                    if(d.arrows)throw std::runtime_error("Historical arrow toggle failed");
                    SendMessageW(d.window,WM_KEYDOWN,'G',0);
                }
            }
            scene.update(d.scenario,d.player.minute,d.camera,d.selected,d.arrows);
            const auto rect=d.map();renderer.resize(rect.right-rect.left,rect.bottom-rect.top);renderer.updateSprites(scene.scene.sprites);
            std::filesystem::path shot;
            if(smoke && !capture.empty())shot=frame==0?capture:std::filesystem::path(capture.wstring()+L"."+std::to_wstring(static_cast<int>(d.player.minute))+L".bmp");
            else if(frame==0)shot=capture;
            renderer.render(d.camera,shot);if(smoke)renderer.checkDebugMessages();
            if(smoke && (frame==0 || frame==2))d.captureUI(shot);
            repaint+=dt;if(repaint>.1 || frame==0){InvalidateRect(d.window,nullptr,FALSE);repaint=0;}
            ++frame;if(smoke && frame==7)break;
        }
        if(smoke){std::ofstream report(capture.wstring()+L".txt");report<<"PASS: 7 timeline snapshots; play/pause/seek/reset/selection/arrows input; GPU readback\n"
            <<"D3D12 debug layer: "<<(renderer.debugEnabled()?"enabled, no warnings/errors":"unavailable")<<'\n';
            if(!report)throw std::runtime_error("Cannot write historical smoke report");}
    } catch(...) {
        if(IsWindow(d.window))DestroyWindow(d.window);DeleteObject(d.normal);DeleteObject(d.heading);DeleteObject(d.smallFont);throw;
    }
    if(IsWindow(d.window))DestroyWindow(d.window);DeleteObject(d.normal);DeleteObject(d.heading);DeleteObject(d.smallFont);
    // 次のモードへWM_QUITを持ち越さない。
    MSG message{};while(PeekMessageW(&message,nullptr,WM_QUIT,WM_QUIT,PM_REMOVE)){}
    return d.battle?2:0;
}
