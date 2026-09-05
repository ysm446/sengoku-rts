#pragma once
#include <array>
#include <cstdint>
#include "organization.h"

enum class FormationState { Idle, Marching, Engaged, Retreating, Routed };
enum class BattleResult { Ongoing, RedVictory, BlueVictory, Draw };

struct Formation {
    float x = 0, z = 0;
    float targetX = 0, targetZ = 0;
    float heading = 0;
    float speed = 1.8f;
    bool moving = false;
    float strength = 500;
    float cohesion = 100;
    float morale = 100;
    FormationState state = FormationState::Idle;
    Organization organization;
    bool maneuverEnabled = true;
    bool defeated() const { return state == FormationState::Retreating || state == FormationState::Routed; }
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
    std::uint64_t generation = 0;
    std::array<Formation, 2> formations;
    BattleResult result = BattleResult::Ongoing;
private:
    double accumulator = 0;
    void step(float seconds);
    void updateSmallGroups(float seconds);
};
