#pragma once
#include "scene.h"
#include <xaudio2.h>
#include <array>
#include <cstdint>
#include <vector>

enum class BattleSound { Wind, March, Combat };
struct SoundLevel { float volume = 0, pan = 0; };
using BattleSoundMix = std::array<SoundLevel, 5>;
enum class AudioBus { Master, Environment, Effects };
struct AudioSettings {
    AudioBus selected = AudioBus::Master;
    std::array<int, 3> percent{{100, 100, 100}};
    void selectNext() { selected = static_cast<AudioBus>((static_cast<unsigned>(selected) + 1) % 3); }
    void adjust(int direction);
    BattleSoundMix apply(BattleSoundMix mix) const;
    bool load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path) const;
};

// 兵士数ではなく動作の有無から、風と両軍の代表音を混ぜる。
BattleSoundMix battleSoundMix(const SoldierVisuals& visuals, const BattleSimulation& simulation,
    const Camera& camera, bool audible);
std::vector<std::int16_t> makeBattleSound(BattleSound sound);
class BattleImpactTracker {
public:
    std::array<bool, 2> update(const SoldierVisuals& visuals, bool audible);
private:
    std::uint64_t generation = 0;
    double time = -1;
    std::array<std::uint64_t, 2> seen{};
    std::array<double, 2> lastPlayed{{-1, -1}};
};

class BattleAudio {
public:
    explicit BattleAudio(bool enabled = true);
    ~BattleAudio();
    BattleAudio(const BattleAudio&) = delete;
    BattleAudio& operator=(const BattleAudio&) = delete;
    bool available() const { return master != nullptr; }
    void update(const BattleSoundMix& mix, float seconds, std::array<bool, 2> impacts = {});
    void silence();
private:
    void release();
    bool comInitialized = false;
    IXAudio2* engine = nullptr;
    IXAudio2MasteringVoice* master = nullptr;
    std::array<IXAudio2SourceVoice*, 5> voices{};
    std::array<std::vector<std::int16_t>, 3> samples;
    std::array<float, 5> volumes{};
};
