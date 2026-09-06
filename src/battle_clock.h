#pragma once
#include "scene.h"

// 描画頻度と独立して小組と表示兵士を一緒に進める。小組内部は既存の60Hz更新。
class BattleClock {
public:
    static constexpr double interval = 1.0 / 30;
    void advance(BattleSimulation& battle, SoldierVisuals& visuals, double seconds, EmotionSignals* emotions = nullptr) {
        if (generation != battle.generation) { generation = battle.generation; remainder = 0; }
        visuals.update(battle);
        if (emotions) emotions->update(battle);
        if (!battle.running || !std::isfinite(seconds) || seconds <= 0) return;
        remainder += seconds;
        while (remainder + 1e-8 >= interval) {
            battle.update(static_cast<float>(interval));
            visuals.update(battle);
            if (emotions) emotions->update(battle);
            remainder -= interval;
        }
    }
private:
    std::uint64_t generation = 0;
    double remainder = 0;
};
