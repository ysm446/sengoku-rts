#include "renderer.h"
#include "audio.h"
#include "resource.h"
#include <shellapi.h>
#include <windowsx.h>
#include <chrono>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
constexpr unsigned initialWidth = 1920;
constexpr unsigned initialHeight = 1080;

std::wstring battleStatus(const BattleSimulation& simulation) {
    const wchar_t* result = simulation.result == BattleResult::RedVictory ? L"赤勝利" :
        simulation.result == BattleResult::BlueVictory ? L"青勝利" : simulation.result == BattleResult::Draw ? L"双方敗走" : L"決着前";
    std::wstring text = std::wstring(simulation.running ? L"進行中 " : L"一時停止 ") + result;
    for (unsigned i = 0; i < simulation.formations.size(); ++i) {
        const auto& f = simulation.formations[i];
        const wchar_t* state = f.state == FormationState::Engaged ? L"交戦" : f.state == FormationState::Retreating ? L"撤退中" :
            f.state == FormationState::Routed ? L"敗走済" : f.state == FormationState::Marching ? L"進軍" : L"待機";
        text += std::wstring(i == 0 ? L" | 赤 " : L" | 青 ") + state + L" 兵力" + std::to_wstring(static_cast<int>(std::ceil(f.strength))) +
            L" 士気" + std::to_wstring(static_cast<int>(f.morale)) + L" 隊列" + std::to_wstring(static_cast<int>(f.cohesion)) +
            L" 敗走" + std::to_wstring(f.routedGroups()) + L"/25小組";
        unsigned shaken = 0;
        for (unsigned id = 0; id < f.organization.smallGroups.size(); ++id)
            shaken += simulation.nearbyRouts(i, id) > 0;
        text += L" 動揺" + std::to_wstring(shaken) + L"小組";
        if (f.movementBlocked) text += L" [備の進路閉塞]";
        else if (f.detouring) text += L" [備の迂回中]";
    }
    return text;
}

struct WindowState {
    Camera camera;
    unsigned width = initialWidth, height = initialHeight;
    unsigned requestedSoldiers = 1000;
    bool minimized = false;
    bool inspect = false, placeholder = false, sceneDirty = false, inspectAttack = false;
    unsigned direction = 0;
    BattleSimulation simulation;
    const Scene* scene = nullptr;
    int selected = -1;
    int selectedGroup = -1;
    bool muted = false;
    AudioSettings audioSettings;
    std::filesystem::path audioSettingsPath;
    bool audioSaveFailed = false;
    BattleAudio* audio = nullptr;
    void click(int x, int y, bool move) {
        if (inspect || rotating || !scene) return;
        const auto point = pickTerrain(*scene, camera, static_cast<float>(x), static_cast<float>(y), width, height);
        if (move) {
            if (point && selected >= 0) simulation.move(static_cast<unsigned>(selected), point->x, point->z);
            return;
        }
        selected = selectedGroup = -1;
        float nearest = 1000;
        if (!point) return;
        // 本隊の範囲より先に、移動中の各小組の現在位置を調べる。
        for (unsigned team = 0; team < simulation.formations.size(); ++team) {
            const auto& f = simulation.formations[team];
            for (unsigned id = 0; id < f.organization.smallGroups.size(); ++id) {
                const auto& g = f.organization.smallGroups[id];
                const float dx = point->x - (f.x + (static_cast<float>(id % 5) - 2) * 5.2f + g.displacementX(id));
                const float dz = point->z - (f.z + (static_cast<float>(id / 5) - 2) * 5.2f + g.displacementZ(id));
                const float distance = std::hypot(dx, dz);
                if (std::abs(dx) <= 2.6f && std::abs(dz) <= 2.6f && distance < nearest) {
                    nearest = distance; selected = static_cast<int>(team); selectedGroup = static_cast<int>(id);
                }
            }
        }
        if (selected >= 0) return;
        for (unsigned i = 0; i < simulation.formations.size(); ++i) {
            const auto& formation = simulation.formations[i];
            const float dx = point->x - formation.x, dz = point->z - formation.z;
            const float distance = std::hypot(dx, dz);
            if (std::abs(dx) <= 14 && std::abs(dz) <= 14 && distance < nearest) {
                nearest = distance; selected = static_cast<int>(i);
            }
        }
    }
    std::wstring selectionStatus() const {
        if (selected < 0) return L"選択なし";
        std::wstring text = selected == 0 ? L"赤備" : L"青備";
        if (selectedGroup < 0) return text;
        const auto& organization = simulation.formations[static_cast<unsigned>(selected)].organization;
        const auto& group = organization.smallGroups[static_cast<unsigned>(selectedGroup)];
        const auto& company = organization.companies[group.company];
        const unsigned nearby = simulation.nearbyRouts(static_cast<unsigned>(selected), static_cast<unsigned>(selectedGroup));
        if (nearby > 0) text += L" [動揺:近隣" + std::to_wstring(nearby) + L"小組敗走・士気-" +
            std::to_wstring(nearby * 6) + L"/秒]";
        else if (group.routed && group.routShock > 0 && simulation.result == BattleResult::Ongoing)
            text += L" [動揺源:残り" + std::to_wstring(static_cast<int>(std::ceil(group.routShock))) + L"秒]";
        if (nearby > 0) text += L" [水色:周囲の範囲 橙:動揺源]";
        else if (group.routed && group.routShock > 0 && simulation.result == BattleResult::Ongoing)
            text += L" [黄:影響範囲 橙:影響先]";
        text += L" 第" + std::to_wstring(company.troop + 1) + L"隊 第" + std::to_wstring(group.company + 1) +
            L"組 小組" + std::to_wstring(selectedGroup + 1);
        const wchar_t* action = group.state == SmallGroupState::Fleeing ? (group.fleeBlocked ? L"敗走中・退路閉塞" : L"敗走中") :
            group.state == SmallGroupState::Routed ? L"敗走済" : group.route == SmallGroupRoute::Returning ? L"復帰中" :
            group.route == SmallGroupRoute::ReliefReserve ? L"前列交代中" :
            group.route == SmallGroupRoute::ReliefWithdraw ? L"交代後退中" :
            group.canAttack && simulation.result == BattleResult::Ongoing ? L"攻撃" : group.state == SmallGroupState::Engaged ? L"接敵" : group.state == SmallGroupState::Retreating ? L"撤退" :
            group.state == SmallGroupState::Advancing ? L"前進" : L"待機";
        text += L" " + std::wstring(action);
        const auto fronts = simulation.contactFronts(static_cast<unsigned>(selected), static_cast<unsigned>(selectedGroup));
        float totalWidth = 0;
        for (const auto& face : fronts.faces) totalWidth += face.width();
        if (totalWidth > 0 || group.faceDeployment.total() > 0.01f) {
            const bool returning = group.route == SmallGroupRoute::Returning || group.route == SmallGroupRoute::ReliefReserve ||
                group.route == SmallGroupRoute::ReliefWithdraw;
            const auto allocation = allocateContactFronts(returning ? ContactFronts{} : fronts, group.strength);
            const auto decimal = [](float value) {
                const int tenths = static_cast<int>(std::round(value * 10));
                return std::to_wstring(tenths / 10) + L"." + std::to_wstring(tenths % 10);
            };
            const wchar_t* names[] = {L"前", L"右", L"後", L"左"};
            text += L" [配置試算 現在→目標";
            for (unsigned face = 0; face < 4; ++face) if (fronts.faces[face].width() > 0 || group.faceDeployment.deployed[face] > 0.01f) {
                text += std::wstring(L" ") + names[face] + decimal(group.faceDeployment.deployed[face]) + L"→" + decimal(allocation.faces[face].fighters()) + L"人";
            }
            text += L" 予備" + decimal(group.faceDeployment.reserve()) + L"人]";
        }
        if (simulation.result == BattleResult::Ongoing && !group.routed && !group.canAttack && group.route == SmallGroupRoute::None && !simulation.formations[static_cast<unsigned>(selected)].defeated()) {
            const wchar_t* reason = group.combatWait == CombatWait::NoTarget ? L"近くに攻撃対象なし" :
                group.combatWait == CombatWait::OutOfRange ? L"射程外" :
                group.combatWait == CombatWait::Turning ? L"旋回中" :
                group.combatWait == CombatWait::Obstructed ? L"攻撃線が塞がれている" :
                group.combatWait == CombatWait::PathBlocked ? L"進路閉塞" :
                group.combatWait == CombatWait::Detouring ? L"迂回中" :
                group.combatWait == CombatWait::Held ? L"停止命令中" : L"";
            if (*reason) text += L" [" + std::wstring(reason) + L"]";
        }
        return text + L" 兵力" + std::to_wstring(static_cast<int>(std::ceil(group.strength))) +
            L"/" + std::to_wstring(group.nominalStrength) + L" 士気" + std::to_wstring(static_cast<int>(group.morale)) +
            L" 疲労" + std::to_wstring(static_cast<int>(group.fatigue));
    }
    bool rotating = false, inspectPlaying = false;
    bool orbitLeft = false, orbitRight = false;
    int dragX = 0;
    double inspectTime = 0;
    void resetCamera() { camera = Camera{}; if (inspect) camera.span = 26; }
    void updateOrbit(float seconds) {
        const float axis = static_cast<float>(orbitRight) - static_cast<float>(orbitLeft);
        camera.rotate(axis * DirectX::XM_PIDIV2 * seconds);
    }
};

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<WindowState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        state = static_cast<WindowState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (state) {
        switch (message) {
        case WM_SIZE:
            state->minimized = wparam == SIZE_MINIMIZED;
            state->width = LOWORD(lparam); state->height = HIWORD(lparam);
            return 0;
        case WM_MOUSEWHEEL:
            state->camera.zoom(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA);
            return 0;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
            state->click(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), message == WM_RBUTTONDOWN);
            return 0;
        case WM_MBUTTONDOWN:
            state->rotating = true; state->dragX = GET_X_LPARAM(lparam); SetCapture(window);
            return 0;
        case WM_MOUSEMOVE:
            if (state->rotating) {
                const int x = GET_X_LPARAM(lparam);
                state->camera.rotate(static_cast<float>(x - state->dragX) * 0.006f);
                state->dragX = x;
            }
            return 0;
        case WM_MBUTTONUP:
            state->rotating = false; if (GetCapture() == window) ReleaseCapture();
            return 0;
        case WM_CAPTURECHANGED:
            state->rotating = false; return 0;
        case WM_KILLFOCUS:
            if (state->audio) state->audio->silence();
            state->orbitLeft = state->orbitRight = false;
            state->rotating = false;
            if (GetCapture() == window) ReleaseCapture();
            return 0;
        case WM_KEYUP:
            if (wparam == 'Q') state->orbitLeft = false;
            if (wparam == 'E') state->orbitRight = false;
            return 0;
        case WM_KEYDOWN:
            if (wparam == 'Q') state->orbitLeft = true;
            if (wparam == 'E') state->orbitRight = true;
            if (wparam == VK_ESCAPE) DestroyWindow(window);
            if (wparam == 'R') state->resetCamera();
            if (wparam == '1') state->requestedSoldiers = 1000;
            if (wparam == '2') state->requestedSoldiers = 5000;
            if (wparam == '3') state->requestedSoldiers = 10000;
            if (!(lparam & (1LL << 30))) {
                if (wparam == VK_F5) state->audioSettings.selectNext();
                if (wparam == VK_OEM_PLUS || wparam == VK_ADD) state->audioSettings.adjust(1);
                if (wparam == VK_OEM_MINUS || wparam == VK_SUBTRACT) state->audioSettings.adjust(-1);
                if ((wparam == VK_OEM_PLUS || wparam == VK_ADD || wparam == VK_OEM_MINUS || wparam == VK_SUBTRACT) &&
                    !state->audioSettingsPath.empty())
                    state->audioSaveFailed = !state->audioSettings.save(state->audioSettingsPath);
                if (wparam == 'M') { state->muted = !state->muted; if (state->muted && state->audio) state->audio->silence(); }
                if (wparam == 'H' && !state->inspect && state->selected >= 0)
                    state->simulation.hold(static_cast<unsigned>(state->selected));
                if (wparam == VK_F2) { state->inspect = !state->inspect; state->resetCamera(); state->sceneDirty = true; }
                if (wparam == VK_F3 && state->inspect) { state->inspectAttack = !state->inspectAttack; state->sceneDirty = true; }
                if (wparam == 'V') { state->placeholder = !state->placeholder; state->sceneDirty = true; }
                if (state->inspect && (wparam == 'Z' || wparam == 'C')) {
                    state->direction = (state->direction + (wparam == 'Z' ? 7 : 1)) % 8;
                    state->sceneDirty = true;
                }
                if (wparam == VK_SPACE) {
                    if (state->inspect) state->inspectPlaying = !state->inspectPlaying;
                    else state->simulation.toggle();
                }
                if (wparam == VK_HOME) {
                    state->selected = state->selectedGroup = -1;
                    state->simulation.reset(); state->inspectTime = 0; state->inspectPlaying = false;
                }
            }
            return 0;
        case WM_GETMINMAXINFO: {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
            limits->ptMinTrackSize = {640, 480};
            return 0;
        }
        }
    }
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wparam, lparam);
}

struct Options {
    bool smoke = false, warp = false, inspect = false, placeholder = false;
    bool march = false, motionTest = false, combatTest = false, inspectAttack = false;
    float yawDegrees = 0;
    unsigned soldiers = 1000;
    std::filesystem::path capture;
};
Options parseOptions() {
    Options options;
    int count = 0;
    auto* args = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!args) throw std::runtime_error("CommandLineToArgvW failed");
    try {
        for (int i = 1; i < count; ++i) {
            const std::wstring arg = args[i];
            if (arg == L"--smoke-test") options.smoke = true;
            else if (arg == L"--warp") options.warp = true;
            else if (arg == L"--inspect") options.inspect = true;
            else if (arg == L"--inspect-attack") { options.inspect = true; options.inspectAttack = true; options.march = true; }
            else if (arg == L"--placeholder") options.placeholder = true;
            else if (arg == L"--march") options.march = true;
            else if (arg == L"--motion-test") { options.motionTest = true; options.smoke = true; options.march = true; }
            else if (arg == L"--combat-test") { options.combatTest = true; options.smoke = true; options.march = true; }
            else if (arg == L"--yaw" && i + 1 < count) {
                const std::wstring value = args[++i]; std::size_t consumed = 0;
                options.yawDegrees = std::stof(value, &consumed);
                if (consumed != value.size() || !std::isfinite(options.yawDegrees) || std::abs(options.yawDegrees) > 360)
                    throw std::runtime_error("--yaw requires degrees between -360 and 360");
            }
            else if (arg == L"--capture" && i + 1 < count) options.capture = args[++i];
            else if (arg == L"--soldiers" && i + 1 < count) {
                const std::wstring value = args[++i];
                if (value != L"1000" && value != L"5000" && value != L"10000")
                    throw std::runtime_error("--soldiers accepts 1000, 5000 or 10000");
                options.soldiers = static_cast<unsigned>(std::stoul(value));
            } else throw std::runtime_error("Unknown or incomplete option");
        }
    } catch (...) { LocalFree(args); throw; }
    LocalFree(args);
    if (options.combatTest && (options.inspect || options.motionTest)) throw std::runtime_error("Combat test requires battlefield mode");
    if (options.smoke && options.capture.empty()) options.capture = L"smoke.bmp";
    return options;
}
std::filesystem::path executableDirectory() {
    wchar_t path[32768]{};
    const auto length = GetModuleFileNameW(nullptr, path, 32768);
    if (length == 0 || length >= 32768) throw std::runtime_error("Cannot resolve executable path");
    return std::filesystem::path(path).parent_path();
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    Options options;
    HWND window = nullptr;
    try {
        options = parseOptions();
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        WindowState state; state.requestedSoldiers = options.soldiers;
        if (!options.smoke) {
            state.audioSettingsPath = executableDirectory() / L"audio-settings.txt";
            state.audioSettings.load(state.audioSettingsPath);
        }
        state.inspect = options.inspect; state.placeholder = options.placeholder; state.resetCamera();
        state.inspectAttack = options.inspectAttack;
        state.camera.rotate(DirectX::XMConvertToRadians(options.yawDegrees));
        state.simulation.running = options.march; state.inspectPlaying = options.march;
        WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = windowProc;
        wc.hInstance = instance; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.lpszClassName = L"SengokuPrototype";
        wc.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_SENGOKU_APP), IMAGE_ICON,
            GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
        wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_SENGOKU_APP), IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
        if (!wc.hIcon || !wc.hIconSm) throw std::runtime_error("Cannot load application icons");
        if (!RegisterClassExW(&wc)) throw std::runtime_error("RegisterClassEx failed");
        RECT bounds{0, 0, static_cast<LONG>(state.width), static_cast<LONG>(state.height)};
        AdjustWindowRectEx(&bounds, WS_OVERLAPPEDWINDOW, FALSE, 0);
        window = CreateWindowExW(0, wc.lpszClassName, L"Sengoku RTS | Visual Prototype", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top,
            nullptr, nullptr, instance, &state);
        if (!window) throw std::runtime_error("CreateWindowEx failed");
        // ウィンドウ枠を除いたコンテンツ領域を、実際のモニターDPIで指定する。
        RECT contentBounds{0, 0, static_cast<LONG>(initialWidth), static_cast<LONG>(initialHeight)};
        if (!AdjustWindowRectExForDpi(&contentBounds, WS_OVERLAPPEDWINDOW, FALSE, 0, GetDpiForWindow(window)) ||
            !SetWindowPos(window, nullptr, 0, 0, contentBounds.right - contentBounds.left,
                contentBounds.bottom - contentBounds.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE))
            throw std::runtime_error("Cannot set initial content size");
        RECT actualContent{};
        if (!GetClientRect(window, &actualContent) || actualContent.right != initialWidth || actualContent.bottom != initialHeight)
            throw std::runtime_error("Initial content size does not match the requested resolution");
        Renderer renderer(window, state.width, state.height, options.warp, executableDirectory() / L"shaders/battlefield.hlsl");
        BattleAudio audio(!options.smoke);
        BattleImpactTracker impactTracker;
        state.audio = &audio;
        const auto sheetPath = executableDirectory() / L"assets/sprites/ashigaru_idle.png";
        const auto walkPath = executableDirectory() / L"assets/sprites/ashigaru_walk.png";
        const auto attackPath = executableDirectory() / L"assets/sprites/ashigaru_attack.png";
        Scene activeScene;
        state.scene = &activeScene;
        unsigned requestedSoldiers = options.soldiers, displayedSoldiers = 0;
        bool generatedSoldiers = false;
        const auto rebuildScene = [&] {
            SceneOptions sceneOptions;
            if (!state.placeholder && std::filesystem::exists(sheetPath)) {
                sceneOptions.soldierSheet = sheetPath;
                if (std::filesystem::exists(walkPath)) sceneOptions.walkSheet = walkPath;
                if (std::filesystem::exists(attackPath)) sceneOptions.attackSheet = attackPath;
            }
            sceneOptions.inspect = state.inspect; sceneOptions.directionOffset = state.inspect ? state.direction : 0;
            sceneOptions.inspectAttack = state.inspectAttack;
            auto individuals = activeScene.individuals;
            activeScene = makeScene(state.requestedSoldiers, sceneOptions);
            activeScene.individuals = std::move(individuals);
            displayedSoldiers = activeScene.soldierCount; generatedSoldiers = activeScene.generatedSoldiers;
            renderer.setScene(activeScene); state.sceneDirty = false; requestedSoldiers = state.requestedSoldiers;
        };
        rebuildScene();
        if ((options.combatTest || options.inspectAttack) && !options.placeholder && !activeScene.attackSoldiers)
            throw std::runtime_error("Attack sprite sheet is required for this test");
        if (options.smoke && !state.inspect) {
            const auto clickWorld = [&](UINT message, float x, float z) {
                const auto projected = DirectX::XMVector3TransformCoord(
                    DirectX::XMVectorSet(x, terrainHeight(x, z), z, 1),
                    state.camera.matrix(static_cast<float>(state.width) / state.height));
                const int px = static_cast<int>((DirectX::XMVectorGetX(projected) + 1) * state.width * 0.5f);
                const int py = static_cast<int>((1 - DirectX::XMVectorGetY(projected)) * state.height * 0.5f);
                SendMessageW(window, message, 0, MAKELPARAM(px, py));
            };
            clickWorld(WM_RBUTTONDOWN, 10, -10);
            if (state.simulation.formations[0].targetX != 0) throw std::runtime_error("Unselected move accepted");
            clickWorld(WM_LBUTTONDOWN, 0, -22);
            if (state.selected != 0) throw std::runtime_error("Formation selection failed");
            clickWorld(WM_RBUTTONDOWN, 10, -10);
            if (std::abs(state.simulation.formations[0].targetX - 10) > 0.2f ||
                std::abs(state.simulation.formations[0].targetZ + 10) > 0.2f)
                throw std::runtime_error("Mouse move command failed");
            SendMessageW(window, WM_KEYDOWN, 'H', 0);
            if (state.simulation.formations[0].targetZ != -22) throw std::runtime_error("Hold input failed");
            clickWorld(WM_LBUTTONDOWN, 0, 22);
            if (state.selected != 1) throw std::runtime_error("Second formation selection failed");
            clickWorld(WM_LBUTTONDOWN, 0, 0);
            if (state.selected != -1) throw std::runtime_error("Empty terrain did not clear selection");
            state.simulation.formations[0].organization.smallGroups[15].offsetX = -6.1f;
            clickWorld(WM_LBUTTONDOWN, -16.5f, -16.8f);
            if (state.selected != 0 || state.selectedGroup != 15 || state.selectionStatus().find(L"小組16") == std::wstring::npos)
                throw std::runtime_error("Flanking small group selection or hierarchy status failed");
            clickWorld(WM_RBUTTONDOWN, 10, -10);
            if (std::abs(state.simulation.formations[0].targetX - 10) > 0.2f)
                throw std::runtime_error("Small group selection did not command its formation");
            SendMessageW(window, WM_KEYDOWN, VK_HOME, 0);
            if (state.selected != -1 || state.selectedGroup != -1)
                throw std::runtime_error("Reset did not clear small group selection");
            state.simulation.reset(); state.simulation.running = options.march;
        }
        if (options.motionTest && !options.placeholder && !activeScene.animatedSoldiers)
            throw std::runtime_error("Motion test requires the packaged idle and walk sprite sheets");
        if (!options.smoke) ShowWindow(window, show);
        auto previous = std::chrono::steady_clock::now();
        double titleSeconds = 0; unsigned titleFrames = 0, frame = 0;
        bool running = true;
        const unsigned smokeFrames = options.combatTest ? 80 : options.motionTest ? 16 : 5;
        bool observedCombat = false, observedRetreat = false;
        bool observedAttack = false;
        bool observedFrontRelief = false;
        bool observedLocalRout = false;
        unsigned routCaptureFrame = smokeFrames;
        while (running) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) { running = false; break; }
                TranslateMessage(&message); DispatchMessageW(&message);
            }
            if (!running) break;
            if (state.minimized || !state.width || !state.height) {
                audio.silence();
                WaitMessage(); previous = std::chrono::steady_clock::now(); continue;
            }
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - previous).count(); previous = now;
            const float dt = static_cast<float>(std::min(elapsed, 0.1));
            if (!options.smoke && GetForegroundWindow() == window) {
                state.updateOrbit(dt);
                const auto down = [](int key) { return (GetAsyncKeyState(key) & 0x8000) ? 1.0f : 0.0f; };
                float horizontal = std::clamp(down('D') + down(VK_RIGHT), 0.0f, 1.0f) - std::clamp(down('A') + down(VK_LEFT), 0.0f, 1.0f);
                float vertical = std::clamp(down('W') + down(VK_UP), 0.0f, 1.0f) - std::clamp(down('S') + down(VK_DOWN), 0.0f, 1.0f);
                const float length = std::sqrt(horizontal * horizontal + vertical * vertical);
                if (length > 1) { horizontal /= length; vertical /= length; }
                state.camera.pan(horizontal, vertical, dt);
            }
            if (requestedSoldiers != state.requestedSoldiers || state.sceneDirty) rebuildScene();
            if (options.smoke) {
                // 縮小・復元・Camera変更・Scene再転送を同じ描画経路で検証する。
                if (frame == 1) {
                    state.width = 960; state.height = 640; state.camera.zoom(1);
                    const float previousYaw = state.camera.yaw;
                    SendMessageW(window, WM_MBUTTONDOWN, MK_MBUTTON, MAKELPARAM(100, 100));
                    SendMessageW(window, WM_MOUSEMOVE, MK_MBUTTON, MAKELPARAM(130, 100));
                    SendMessageW(window, WM_MBUTTONUP, 0, MAKELPARAM(130, 100));
                    if (std::abs(state.camera.yaw - previousYaw) < 0.1f || state.rotating)
                        throw std::runtime_error("Mouse orbit input failed");
                }
                if (frame == 2) {
                    state.camera.pan(1, 1, 0.1f);
                    const float previousYaw = state.camera.yaw;
                    SendMessageW(window, WM_KEYDOWN, 'E', 0);
                    SendMessageW(window, WM_KEYDOWN, 'E', 1LL << 30);
                    if (state.camera.yaw != previousYaw)
                        throw std::runtime_error("Orbit key must not snap or depend on key repeat");
                    for (unsigned step = 0; step < 6; ++step) state.updateOrbit(1.0f / 60.0f);
                    if (std::abs(state.camera.yaw - previousYaw - DirectX::XM_PIDIV2 * 0.1f) > 0.00001f)
                        throw std::runtime_error("Held orbit key did not rotate continuously");
                    SendMessageW(window, WM_KEYDOWN, 'Q', 0);
                    const float heldYaw = state.camera.yaw;
                    state.updateOrbit(0.1f);
                    if (state.camera.yaw != heldYaw) throw std::runtime_error("Opposing orbit keys must cancel");
                    SendMessageW(window, WM_KEYUP, 'E', 0);
                    state.updateOrbit(0.1f);
                    if (std::abs(state.camera.yaw - previousYaw) > 0.00001f)
                        throw std::runtime_error("Left orbit key failed");
                    SendMessageW(window, WM_KEYUP, 'Q', 0);
                    const float releasedYaw = state.camera.yaw;
                    state.updateOrbit(0.1f);
                    if (state.camera.yaw != releasedYaw) throw std::runtime_error("Orbit did not stop on key release");
                    SendMessageW(window, WM_KEYDOWN, 'E', 0);
                    SendMessageW(window, WM_KILLFOCUS, 0, 0);
                    state.updateOrbit(0.1f);
                    if (state.camera.yaw != releasedYaw) throw std::runtime_error("Orbit did not stop on focus loss");
                    rebuildScene();
                }
                if (frame == 3) {
                    state.width = initialWidth; state.height = initialHeight; state.resetCamera();
                    state.camera.rotate(DirectX::XMConvertToRadians(options.yawDegrees));
                    rebuildScene();
                }
                if (options.motionTest) {
                    const float targetYaw = Camera::initialYaw + static_cast<float>(frame) * DirectX::XM_PIDIV4;
                    state.camera.rotate(targetYaw - state.camera.yaw);
                }
            }
            const float simulationDt = options.combatTest ? 1.0f : options.smoke ? 0.125f : dt;
            if (state.inspect) {
                if (state.inspectPlaying) state.inspectTime += simulationDt;
            } else if (options.combatTest) {
                // 描画間にも個体状態を更新し、接敵・補充・攻撃を通常実行に近い間隔で確認する。
                for (unsigned step = 0; step < 10; ++step) {
                    state.simulation.update(0.1f);
                    updateSceneSprites(activeScene, state.simulation, state.camera, state.selected, state.selectedGroup);
                    observedRetreat |= state.simulation.formations[1].state == FormationState::Retreating;
                    for (const auto& f : state.simulation.formations) for (unsigned id = 0; id < 25; ++id)
                        observedFrontRelief |= f.organization.smallGroups[id].slot != id;
                }
            } else state.simulation.update(simulationDt);
            observedCombat |= state.simulation.formations[0].state == FormationState::Engaged;
            observedRetreat |= state.simulation.formations[1].state == FormationState::Retreating;
            if (options.combatTest && !observedLocalRout && state.simulation.result == BattleResult::Ongoing)
                for (unsigned team = 0; team < 2; ++team) if (state.simulation.formations[team].routedGroups() > 0 && !state.simulation.formations[team].defeated()) {
                    observedLocalRout = true; routCaptureFrame = frame;
                    for (unsigned id = 0; id < 25; ++id) if (state.simulation.formations[team].organization.smallGroups[id].routed &&
                        state.simulation.formations[team].organization.smallGroups[id].routShock > 0) {
                        state.selected = static_cast<int>(team); state.selectedGroup = static_cast<int>(id); break;
                    }
                }
            auto visualSimulation = state.simulation;
            if (state.inspect) visualSimulation.time = state.inspectTime;
            updateSceneSprites(activeScene, visualSimulation, state.camera, state.selected, state.selectedGroup);
            const bool audible = !state.muted && !state.inspect && GetForegroundWindow() == window && state.simulation.running;
            const auto impacts = impactTracker.update(*activeScene.individuals, audible);
            if (!audible) audio.silence();
            else audio.update(state.audioSettings.apply(battleSoundMix(*activeScene.individuals, state.simulation, state.camera, true)), dt, impacts);
            if (options.combatTest) for (const auto& soldier : activeScene.individuals->soldiers)
                observedAttack |= soldier.attacking;
            renderer.updateSprites(activeScene.sprites);
            renderer.resize(state.width, state.height);
            const bool captureNow = !options.capture.empty() && (options.smoke ? frame == smokeFrames - 1 : frame == 0);
            const auto capturePath = options.combatTest && frame == routCaptureFrame ?
                std::filesystem::path(options.capture.wstring() + L".rout.bmp") : options.combatTest && frame == 19 ?
                std::filesystem::path(options.capture.wstring() + L".engaged.bmp") : captureNow ? options.capture : std::filesystem::path{};
            renderer.render(state.camera, capturePath);
            if (options.smoke) renderer.checkDebugMessages();
            titleSeconds += elapsed; ++titleFrames; ++frame;
            if (titleSeconds >= 0.5 || frame == 1) {
                const int fps = titleSeconds > 0 ? static_cast<int>(titleFrames / titleSeconds) : 0;
                const auto title = std::wstring(L"戦国合戦 | ") + (state.inspect ? L"素材確認" : state.selectionStatus()) + L" | " +
                    (state.inspect ? L"" : battleStatus(state.simulation)) + L" | " +
                    (generatedSoldiers ? L"Blender槍足軽" : L"仮素材") +
                    (state.inspect ? L" 素材確認 | " : L" 戦場 | ") + L"表示上限 " + std::to_wstring(displayedSoldiers) + L" | " +
                    std::to_wstring(fps) + L" fps | " + (state.inspect ? L"素材確認" :
                        (state.selected == 0 ? L"赤部隊を選択" : state.selected == 1 ? L"青部隊を選択" : L"選択なし")) +
                    (audio.available() ? (state.muted ? L" | 消音" : L" | 音ON") : L" | 音声出力なし") +
                    L" 音量[" + (state.audioSettings.selected == AudioBus::Master ? L"全体 " :
                        state.audioSettings.selected == AudioBus::Environment ? L"環境 " : L"効果音 ") +
                    std::to_wstring(state.audioSettings.percent[static_cast<unsigned>(state.audioSettings.selected)]) + L"%]" +
                    (state.audioSaveFailed ? L" 音量保存失敗" : L"") +
                    L" | 左:選択 右:移動 H:停止 Space:再生/停止 Q/E:回転 Home:リセット F2:素材 F3:歩行/攻撃 M:消音 F5:音量対象 +/-:調整";
                SetWindowTextW(window, title.c_str()); titleSeconds = 0; titleFrames = 0;
            }
            if (options.smoke && frame >= smokeFrames) break;
        }
        if (options.smoke) {
            if (options.combatTest && (!observedCombat || !observedRetreat || !observedAttack || !observedFrontRelief || !observedLocalRout || state.simulation.result != BattleResult::RedVictory ||
                !state.simulation.formations[1].defeated()))
                throw std::runtime_error("Combat smoke failed: combat=" + std::to_string(observedCombat) +
                    " relief=" + std::to_string(observedFrontRelief) + " localRout=" + std::to_string(observedLocalRout) +
                    " retreat=" + std::to_string(observedRetreat) + " attack=" + std::to_string(observedAttack) +
                    " result=" + std::to_string(static_cast<int>(state.simulation.result)));
            std::ofstream report(std::filesystem::path(options.capture.wstring() + L".txt"));
            report << "PASS: " << frame << " frames; resize; camera pan/zoom/orbit; sprite updates; GPU readback\n"
                   << "Soldiers: " << displayedSoldiers << "\nCapture: " << state.width << 'x' << state.height << '\n'
                   << "Soldier sprites: " << (generatedSoldiers ? "Blender 8-direction PNG" : "placeholder") << '\n'
                   << "Walk atlas: " << (activeScene.animatedSoldiers ? "loaded" : "unavailable") << '\n'
                   << "Formation Z: " << state.simulation.formations[0].z << ", " << state.simulation.formations[1].z << '\n'
                   << "Combat/retreat observed: " << observedCombat << '/' << observedRetreat << '\n'
                   << "Individual attack observed: " << observedAttack << '\n'
                   << "D3D12 debug layer: " << (renderer.debugEnabled() ? "enabled, no warnings/errors" : "unavailable") << '\n';
            if (!report) throw std::runtime_error("Cannot write smoke report");
        }
        if (IsWindow(window)) DestroyWindow(window);
        return 0;
    } catch (const std::exception& error) {
        const auto logPath = options.capture.empty() ? executableDirectory() / L"error.log" : std::filesystem::path(options.capture.wstring() + L".error.log");
        std::ofstream log(logPath); log << error.what() << '\n';
        OutputDebugStringA(error.what());
        if (!options.smoke) MessageBoxA(window, error.what(), "Sengoku RTS", MB_OK | MB_ICONERROR);
        if (IsWindow(window)) DestroyWindow(window);
        return 1;
    }
}
