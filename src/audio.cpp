#include "audio.h"
#include <algorithm>
#include <cmath>
#include <fstream>

bool AudioSettings::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    int version = 0;
    std::array<int, 3> values{};
    if (!(input >> version >> values[0] >> values[1] >> values[2]) || version != 1) return false;
    for (int value : values) if (value < 0 || value > 100) return false;
    input >> std::ws;
    if (!input.eof()) return false;
    percent = values;
    return true;
}
bool AudioSettings::save(const std::filesystem::path& path) const {
    auto temporary = path;
    temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) return false;
        output << "1 " << percent[0] << ' ' << percent[1] << ' ' << percent[2] << '\n';
        output.close();
        if (!output) { DeleteFileW(temporary.c_str()); return false; }
    }
    if (MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    DeleteFileW(temporary.c_str());
    return false;
}

void AudioSettings::adjust(int direction) {
    auto& value = percent[static_cast<unsigned>(selected)];
    value = std::clamp(value + (direction > 0 ? 10 : direction < 0 ? -10 : 0), 0, 100);
}
BattleSoundMix AudioSettings::apply(BattleSoundMix mix) const {
    for (unsigned i = 0; i < mix.size(); ++i)
        mix[i].volume *= (percent[0] / 100.0f) * (percent[i == 0 ? 1 : 2] / 100.0f);
    return mix;
}

BattleSoundMix battleSoundMix(const SoldierVisuals& visuals, const BattleSimulation& simulation,
    const Camera& camera, bool audible) {
    BattleSoundMix mix{};
    if (!audible || !simulation.running) return mix;
    mix[0].volume = 0.18f;
    std::array<bool, 2> walking{};
    for (unsigned i = 0; i < visuals.soldiers.size(); ++i) {
        const auto& soldier = visuals.soldiers[i];
        if (soldier.life != SoldierLife::Alive) continue;
        const unsigned team = i / SoldierVisuals::perTeam;
        walking[team] = walking[team] || soldier.walking;
    }
    const auto right = camera.right();
    for (unsigned team = 0; team < 2; ++team) {
        const auto& f = simulation.formations[team];
        const float dx = f.x - camera.x, dz = f.z - camera.z;
        const float pan = std::clamp((dx * right.x + dz * right.z) / (camera.span * 0.5f), -1.0f, 1.0f);
        const float gain = std::clamp(64 / camera.span, 0.25f, 1.0f) / (1 + std::hypot(dx, dz) / 45);
        mix[1 + team] = {walking[team] ? gain * 0.35f : 0, pan};
        const auto& impact = visuals.impactPositions[team];
        const float ix = impact.x - camera.x, iz = impact.z - camera.z;
        const float impactPan = std::clamp((ix * right.x + iz * right.z) / (camera.span * 0.5f), -1.0f, 1.0f);
        const float impactGain = std::clamp(64 / camera.span, 0.25f, 1.0f) / (1 + std::hypot(ix, iz) / 45);
        mix[3 + team] = {visuals.impacts[team] ? impactGain * 0.3f : 0, impactPan};
    }
    return mix;
}

std::array<bool, 2> BattleImpactTracker::update(const SoldierVisuals& visuals, bool audible) {
    std::array<bool, 2> play{};
    if (generation != visuals.generation || time < 0 || visuals.time < time) {
        generation = visuals.generation; seen = visuals.impacts; lastPlayed = {-1, -1}; time = visuals.time;
        return play;
    }
    for (unsigned team = 0; team < 2; ++team) {
        if (audible && visuals.time > time && visuals.impacts[team] > seen[team] &&
            visuals.time - lastPlayed[team] >= 0.25) {
            play[team] = true; lastPlayed[team] = visuals.time;
        }
    }
    // 間引いた音と消音中の音も消費し、再開時の遅延再生を防ぐ。
    seen = visuals.impacts; time = visuals.time;
    return play;
}

std::vector<std::int16_t> makeBattleSound(BattleSound sound) {
    constexpr unsigned rate = 48000;
    const unsigned length = sound == BattleSound::Combat ? rate / 4 : rate * 8;
    std::vector<std::int16_t> result(length);
    std::uint32_t random = 0x53454e47;
    float low = 0;
    for (unsigned i = 0; i < length; ++i) {
        random = random * 1664525u + 1013904223u;
        const float noise = static_cast<float>(random >> 8) / 8388607.5f - 1;
        const float t = static_cast<float>(i) / rate;
        low += (noise - low) * 0.035f;
        float value = 0;
        if (sound == BattleSound::Wind) {
            value = low * (0.6f + 0.2f * std::sin(t * 2.35619449f));
        } else {
            const float beat = std::fmod(t, sound == BattleSound::March ? 0.4f : 0.5f);
            const float attack = std::min(beat / 0.008f, 1.0f);
            if (sound == BattleSound::March) {
                // 土を踏む鈍い音に装備の擦れを混ぜる仮素材。
                value = attack * (std::exp(-beat * 28) * (0.35f * std::sin(beat * 565) + low) +
                    std::exp(-beat * 15) * noise * 0.08f);
            } else {
                value = attack * std::exp(-beat * 22) * (noise * 0.25f +
                    std::sin(beat * 1950) * 0.18f + std::sin(beat * 3270) * 0.1f);
            }
        }
        // ループ端をゼロへ寄せ、継ぎ目の段差を抑える。
        const float edge = std::min({1.0f, static_cast<float>(i) / (sound == BattleSound::Combat ? 240.0f : 2400.0f),
            static_cast<float>(length - 1 - i) / 2400});
        result[i] = static_cast<std::int16_t>(std::clamp(value * edge, -0.9f, 0.9f) * 32767);
    }
    return result;
}

BattleAudio::BattleAudio(bool enabled) {
    if (!enabled) return;
    samples[0] = makeBattleSound(BattleSound::Wind);
    samples[1] = makeBattleSound(BattleSound::March);
    samples[2] = makeBattleSound(BattleSound::Combat);
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    comInitialized = SUCCEEDED(com);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) return;
    if (FAILED(XAudio2Create(&engine)) || FAILED(engine->CreateMasteringVoice(&master, 2, 48000))) {
        release(); return;
    }
    const WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 48000, 96000, 2, 16, 0};
    for (unsigned i = 0; i < voices.size(); ++i) {
        if (FAILED(engine->CreateSourceVoice(&voices[i], &format))) { release(); return; }
        const auto& pcm = samples[i == 0 ? 0 : i < 3 ? 1 : 2];
        XAUDIO2_BUFFER buffer{};
        buffer.AudioBytes = static_cast<UINT32>(pcm.size() * sizeof(std::int16_t));
        buffer.pAudioData = reinterpret_cast<const BYTE*>(pcm.data());
        if (i < 3) {
            buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
            buffer.LoopLength = static_cast<UINT32>(pcm.size());
        }
        // 両軍の代表音が完全に重ならないよう、再生速度を少し変える。
        voices[i]->SetFrequencyRatio((i == 2 || i == 4) ? 1.03f : 1.0f);
        voices[i]->SetVolume(0);
        if ((i < 3 && FAILED(voices[i]->SubmitSourceBuffer(&buffer))) || FAILED(voices[i]->Start())) { release(); return; }
    }
}

BattleAudio::~BattleAudio() { release(); }
void BattleAudio::release() {
    for (auto*& voice : voices) if (voice) { voice->DestroyVoice(); voice = nullptr; }
    if (master) { master->DestroyVoice(); master = nullptr; }
    if (engine) { engine->Release(); engine = nullptr; }
    if (comInitialized) { CoUninitialize(); comInitialized = false; }
}
void BattleAudio::silence() {
    volumes.fill(0);
    for (unsigned i = 0; i < voices.size(); ++i) if (voices[i]) {
        voices[i]->SetVolume(0);
        if (i >= 3) { voices[i]->Stop(); voices[i]->FlushSourceBuffers(); voices[i]->Start(); }
    }
}
void BattleAudio::update(const BattleSoundMix& mix, float seconds, std::array<bool, 2> impacts) {
    if (!available()) return;
    const float blend = 1 - std::exp(-std::clamp(seconds, 0.0f, 0.1f) * 18);
    for (unsigned i = 0; i < voices.size(); ++i) {
        volumes[i] += (mix[i].volume - volumes[i]) * blend;
        const float pan = mix[i].pan;
        const float channels[]{std::sqrt((1 - pan) * 0.5f), std::sqrt((1 + pan) * 0.5f)};
        if (FAILED(voices[i]->SetVolume(volumes[i])) || FAILED(voices[i]->SetOutputMatrix(master, 1, 2, channels))) {
            release(); return;
        }
        if (i >= 3 && impacts[i - 3]) {
            XAUDIO2_VOICE_STATE state{};
            voices[i]->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
            if (state.BuffersQueued) continue;
            const auto& pcm = samples[2];
            XAUDIO2_BUFFER buffer{};
            buffer.Flags = XAUDIO2_END_OF_STREAM;
            buffer.AudioBytes = static_cast<UINT32>(pcm.size() * sizeof(std::int16_t));
            buffer.pAudioData = reinterpret_cast<const BYTE*>(pcm.data());
            volumes[i] = mix[i].volume;
            if (FAILED(voices[i]->SetVolume(volumes[i])) || FAILED(voices[i]->SubmitSourceBuffer(&buffer))) {
                release(); return;
            }
        }
    }
}
