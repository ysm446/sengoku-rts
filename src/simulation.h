#pragma once
#include <array>

struct Formation {
    float x = 0, z = 0;
    float targetX = 0, targetZ = 0;
    float heading = 0;
    float speed = 1.8f;
    bool moving = false;
};

// シミュレーションは部隊だけを更新する。表示兵士数には依存しない。
class BattleSimulation {
public:
    BattleSimulation();
    void update(float seconds);
    void reset();
    void move(unsigned index, float x, float z);
    void hold(unsigned index);
    void toggle() { running = !running; }
    bool running = false;
    double time = 0;
    std::array<Formation, 2> formations;
};
