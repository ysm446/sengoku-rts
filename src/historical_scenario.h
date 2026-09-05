#pragma once
#include <string>
#include <vector>

// 描画や戦闘計算に依存しない、再生用のシナリオデータ。
namespace history {
enum class Side { East, West, Changed };
enum class Action { Waiting, Marching, Fighting, Retreating, Dispersed };
struct Keyframe { double minute; float x, z; Action action; };
struct Army {
    std::wstring name;
    Side side;
    double changeMinute = 10000;
    std::wstring note;
    std::vector<Keyframe> route;
};
struct Hill { std::wstring name; float x, z, height, radiusX, radiusZ; };
struct Event { double minute; std::wstring title, description; };
struct Scenario {
    std::wstring title;
    double startMinute, endMinute;
    std::vector<Hill> hills;
    std::vector<Army> armies;
    std::vector<Event> events;
};
struct Pose { float x, z, heading; Action action; Side side; bool moving; };
Scenario sekigahara();
Pose sample(const Army& army, double minute);
float height(const Scenario& scenario, float x, float z);
std::size_t eventIndex(const Scenario& scenario, double minute);
std::wstring clockText(double minute);
const wchar_t* actionText(Action action);
struct Player {
    double minute = 480;
    double speed = 2; // 1実秒あたりの史実上の分数。
    bool playing = false;
    void seek(const Scenario& scenario, double target);
    void update(const Scenario& scenario, double seconds);
};
}
