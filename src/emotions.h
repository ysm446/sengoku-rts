#pragma once
#include "simulation.h"

enum class Emotion { None, Motivation, Anxiety, Fear };

// 観察専用。シミュレーションへ書き戻さず、停止中は表示時間も止める。
struct EmotionSignals {
    struct Signal {
        Emotion state = Emotion::None, visible = Emotion::None;
        double until = 0, cooldown = 0;
        bool recovering = false;
    };
    std::array<Signal, 50> signals{};
    std::uint64_t generation = 0;
    bool initialized = false;
    static Emotion classify(const SmallGroup& group, Emotion previous) {
        if (group.routed || group.morale <= 25 || (previous == Emotion::Fear && group.morale < 35)) return Emotion::Fear;
        if (group.morale <= 50 || (previous == Emotion::Anxiety && group.morale < 60)) return Emotion::Anxiety;
        return Emotion::None;
    }
    void update(const BattleSimulation& battle) {
        const bool reset = !initialized || generation != battle.generation;
        if (reset) { signals = {}; generation = battle.generation; initialized = true; }
        for (unsigned i = 0; i < signals.size(); ++i) {
            auto& s = signals[i];
            const auto& g = battle.formations[i / 25].organization.smallGroups[i % 25];
            const auto next = classify(g, s.state);
            if (battle.time >= s.until) s.visible = Emotion::None;
            const bool recovered = s.recovering && !g.resting && !g.routed && g.morale >= 70;
            Emotion event = next != s.state ? next : Emotion::None;
            if (recovered) event = Emotion::Motivation;
            if (!reset && event != Emotion::None && (battle.time >= s.cooldown || event == Emotion::Fear)) {
                s.visible = event; s.until = battle.time + 2.5; s.cooldown = battle.time + 8;
            }
            s.state = next; s.recovering = g.resting;
        }
    }
};
