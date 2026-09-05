#include "audio.h"
#include <iostream>
#include <stdexcept>
#include <fstream>

void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    try {
        for (auto sound : {BattleSound::Wind, BattleSound::March, BattleSound::Combat}) {
            const auto samples = makeBattleSound(sound);
            require(samples.size() == (sound == BattleSound::Combat ? 12000u : 48000u * 8) && samples.front() == 0 && samples.back() == 0,
                "Sound length or loop boundary invalid");
            double energy = 0;
            for (auto sample : samples) {
                require(std::abs(static_cast<int>(sample)) < 30000, "Clipped sound");
                energy += static_cast<double>(sample) * sample;
            }
            require(energy > 1000000, "Silent generated sound");
        }
        BattleSimulation battle;
        SoldierVisuals visuals;
        visuals.update(battle);
        Camera camera;
        visuals.soldiers[0].walking = true;
        visuals.soldiers[SoldierVisuals::perTeam].attacking = true;
        visuals.impacts[1] = 1;
        visuals.impactPositions[1] = {0, 0, 14};
        auto mix = battleSoundMix(visuals, battle, camera, true);
        for (const auto& level : mix) require(level.volume == 0, "Paused battle makes sound");
        battle.running = true;
        mix = battleSoundMix(visuals, battle, camera, true);
        require(mix[0].volume > 0 && mix[1].volume > 0 && mix[2].volume == 0 &&
            mix[3].volume == 0 && mix[4].volume > 0, "Sounds do not match team actions");
        const auto initial = mix;
        AudioSettings settings;
        settings.adjust(-1);
        auto adjusted = settings.apply(initial);
        require(std::abs(adjusted[0].volume - initial[0].volume * 0.9f) < 0.00001f &&
            std::abs(adjusted[4].volume - initial[4].volume * 0.9f) < 0.00001f, "Master volume missed a sound bus");
        settings.selectNext();
        for (unsigned i = 0; i < 20; ++i) settings.adjust(-1);
        adjusted = settings.apply(initial);
        require(settings.percent[1] == 0 && adjusted[0].volume == 0 && adjusted[4].volume > 0,
            "Environment volume changed combat sound or exceeded its lower limit");
        settings.selectNext();
        for (unsigned i = 0; i < 20; ++i) settings.adjust(1);
        require(settings.percent[2] == 100, "Volume exceeded its upper limit");
        for (unsigned i = 0; i < 10; ++i) settings.adjust(-1);
        settings.selectNext(); settings.selectNext(); settings.adjust(1);
        adjusted = settings.apply(initial);
        require(adjusted[0].volume > 0 && adjusted[1].volume == 0 && adjusted[4].volume == 0,
            "Effects volume did not cover both marching and impact sound");
        require(adjusted[4].pan == initial[4].pan, "Volume adjustment changed sound position");
        const auto settingsPath = std::filesystem::current_path() / (L"audio-settings-test-" + std::to_wstring(GetCurrentProcessId()) + L".txt");
        require(settings.save(settingsPath), "Audio settings could not be saved");
        AudioSettings loaded;
        require(loaded.load(settingsPath) && loaded.percent == settings.percent, "Audio settings did not survive reload");
        loaded.adjust(-1);
        require(loaded.save(settingsPath), "Existing settings could not be replaced");
        AudioSettings replaced;
        require(replaced.load(settingsPath) && replaced.percent == loaded.percent, "Replacement settings were lost");
        for (const char* invalid : {"1 90 101 70", "2 90 80 70", "1 90 80", "1 90 80 70 garbage"}) {
            { std::ofstream output(settingsPath); output << invalid; }
            const auto previous = loaded.percent;
            require(!loaded.load(settingsPath) && loaded.percent == previous, "Invalid settings partially changed live volume");
        }
        std::filesystem::remove(settingsPath);
        require(!loaded.load(settingsPath), "Missing settings unexpectedly loaded");
        camera.rotate(DirectX::XM_PI);
        mix = battleSoundMix(visuals, battle, camera, true);
        require(std::abs(mix[1].pan + initial[1].pan) < 0.0001f, "Pan did not follow camera rotation");
        camera.span = 160;
        mix = battleSoundMix(visuals, battle, camera, true);
        require(mix[1].volume < initial[1].volume, "Zoom out did not attenuate sound");
        for (auto& soldier : visuals.soldiers) soldier.walking = true;
        camera = Camera{};
        mix = battleSoundMix(visuals, battle, camera, true);
        require(mix[1].volume == initial[1].volume, "Dense soldiers amplified representative sound");
        mix = battleSoundMix(visuals, battle, camera, false);
        for (const auto& level : mix) require(level.volume == 0, "Muted or inactive window makes sound");
        BattleImpactTracker tracker;
        require(tracker.update(visuals, true) == std::array<bool, 2>{}, "Initial observation replayed old impacts");
        visuals.time = 0.1; ++visuals.impacts[0];
        require(tracker.update(visuals, true)[0], "New impact did not trigger sound");
        require(!tracker.update(visuals, true)[0], "Redraw repeated impact sound");
        visuals.time = 0.2; visuals.impacts[0] += 1000;
        require(!tracker.update(visuals, true)[0], "Dense impacts bypassed rate limit");
        visuals.time = 0.4;
        require(!tracker.update(visuals, true)[0], "Skipped impacts were queued for later");
        visuals.time = 0.5; ++visuals.impacts[0];
        require(!tracker.update(visuals, false)[0], "Muted impact played");
        visuals.time = 0.6;
        require(!tracker.update(visuals, true)[0], "Unmute replayed old impact");
        visuals.time = 0.7; ++visuals.impacts[0];
        require(tracker.update(visuals, true)[0], "New impact after unmute was lost");
        ++visuals.generation; visuals.time = 0; visuals.impacts = {};
        require(tracker.update(visuals, true) == std::array<bool, 2>{}, "Reset replayed impact");
        BattleAudio disabled(false);
        require(!disabled.available(), "Disabled audio opened a device");
        disabled.update(mix, 0.1f); disabled.silence();
        // 実デバイスは無音で初期化・停止を確認。音声出力がない環境でも単体検証は可能。
        BattleAudio device;
        device.update({}, 0.1f, {true, true}); device.silence();
        std::cout << "Audio synthesis and mix checks passed; device: " << (device.available() ? "ready" : "unavailable") << '\n';
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
