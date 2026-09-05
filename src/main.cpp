#include "renderer.h"
#include <shellapi.h>
#include <chrono>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
constexpr unsigned initialWidth = 1600;
constexpr unsigned initialHeight = 1200;

struct WindowState {
    Camera camera;
    unsigned width = initialWidth, height = initialHeight;
    unsigned requestedSoldiers = 1000;
    bool minimized = false;
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
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) DestroyWindow(window);
            if (wparam == 'R') state->camera = Camera{};
            if (wparam == '1') state->requestedSoldiers = 1000;
            if (wparam == '2') state->requestedSoldiers = 5000;
            if (wparam == '3') state->requestedSoldiers = 10000;
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
    bool smoke = false, warp = false;
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
        WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = windowProc;
        wc.hInstance = instance; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.lpszClassName = L"SengokuPrototype";
        if (!RegisterClassExW(&wc)) throw std::runtime_error("RegisterClassEx failed");
        RECT bounds{0, 0, static_cast<LONG>(state.width), static_cast<LONG>(state.height)};
        AdjustWindowRectEx(&bounds, WS_OVERLAPPEDWINDOW, FALSE, 0);
        window = CreateWindowExW(0, wc.lpszClassName, L"Sengoku RTS | Visual Prototype", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top,
            nullptr, nullptr, instance, &state);
        if (!window) throw std::runtime_error("CreateWindowEx failed");
        Renderer renderer(window, state.width, state.height, options.warp, executableDirectory() / L"shaders/battlefield.hlsl");
        renderer.setScene(makeScene(options.soldiers));
        unsigned displayedSoldiers = options.soldiers;
        if (!options.smoke) ShowWindow(window, show);
        auto previous = std::chrono::steady_clock::now();
        double titleSeconds = 0; unsigned titleFrames = 0, frame = 0;
        bool running = true;
        while (running) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) { running = false; break; }
                TranslateMessage(&message); DispatchMessageW(&message);
            }
            if (!running) break;
            if (state.minimized || !state.width || !state.height) {
                WaitMessage(); previous = std::chrono::steady_clock::now(); continue;
            }
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - previous).count(); previous = now;
            const float dt = static_cast<float>(std::min(elapsed, 0.1));
            if (!options.smoke && GetForegroundWindow() == window) {
                const auto down = [](int key) { return (GetAsyncKeyState(key) & 0x8000) ? 1.0f : 0.0f; };
                float horizontal = std::clamp(down('D') + down(VK_RIGHT), 0.0f, 1.0f) - std::clamp(down('A') + down(VK_LEFT), 0.0f, 1.0f);
                float vertical = std::clamp(down('W') + down(VK_UP), 0.0f, 1.0f) - std::clamp(down('S') + down(VK_DOWN), 0.0f, 1.0f);
                const float length = std::sqrt(horizontal * horizontal + vertical * vertical);
                if (length > 1) { horizontal /= length; vertical /= length; }
                state.camera.pan(horizontal, vertical, dt);
            }
            if (displayedSoldiers != state.requestedSoldiers) {
                displayedSoldiers = state.requestedSoldiers; renderer.setScene(makeScene(displayedSoldiers));
            }
            if (options.smoke) {
                // 縮小・復元・Camera変更・Scene再転送を同じ描画経路で検証する。
                if (frame == 1) { state.width = 960; state.height = 640; state.camera.zoom(1); }
                if (frame == 2) { state.camera.pan(1, 1, 0.1f); renderer.setScene(makeScene(displayedSoldiers)); }
                if (frame == 3) { state.width = initialWidth; state.height = initialHeight; state.camera = Camera{}; }
            }
            renderer.resize(state.width, state.height);
            const bool captureNow = !options.capture.empty() && (options.smoke ? frame == 4 : frame == 0);
            renderer.render(state.camera, captureNow ? options.capture : std::filesystem::path{});
            if (options.smoke) renderer.checkDebugMessages();
            titleSeconds += elapsed; ++titleFrames; ++frame;
            if (titleSeconds >= 0.5 || frame == 1) {
                const int fps = titleSeconds > 0 ? static_cast<int>(titleFrames / titleSeconds) : 0;
                const auto title = L"戦国合戦 | Visual Prototype | " + std::to_wstring(displayedSoldiers) + L" soldiers | " +
                    std::to_wstring(fps) + L" fps | WASD/矢印:移動  Wheel:拡大  R:初期位置  1/2/3:兵士数 | " + renderer.adapterName();
                SetWindowTextW(window, title.c_str()); titleSeconds = 0; titleFrames = 0;
            }
            if (options.smoke && frame >= 5) break;
        }
        if (options.smoke) {
            std::ofstream report(std::filesystem::path(options.capture.wstring() + L".txt"));
            report << "PASS: 5 frames; resize; camera pan/zoom; scene re-upload; GPU readback\n"
                   << "Soldiers: " << displayedSoldiers << "\nCapture: " << state.width << 'x' << state.height << '\n'
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
