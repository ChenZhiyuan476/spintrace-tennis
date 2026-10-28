#pragma once

#include "ball_physics.hpp"
#include "gameplay_config.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tennis {

// Small procedural sound engine. Gameplay submits physical event values; this
// class turns them into short mono PCM clips, keeping SDL details out of the
// match and physics systems.
class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    AudioSystem(const AudioSystem &) = delete;
    AudioSystem &operator=(const AudioSystem &) = delete;

    bool available() const { return stream_ != nullptr; }
    void playRacketHit(double power, double spinAmount, double accuracy,
                       bool serve = false);
    void playCourtBounce(CourtPreset court, double verticalImpactMps,
                         double horizontalSpeedMps);
    void playNetHit(NetCollisionOutcome outcome, double speedMps);

private:
    static constexpr int SampleRate = 48'000;

    SDL_AudioStream *stream_ = nullptr;
    std::uint32_t noiseState_ = 0x73A4C219u;
    std::array<std::vector<float>, 3> racketHitSamples_;
    std::array<std::vector<float>, 3> matchForehandSamples_;
    std::array<std::vector<float>, 3> matchServeSamples_;
    std::array<std::vector<float>, 3> hardBounceSamples_;
    std::array<std::vector<float>, 3> clayBounceSamples_;
    std::array<std::vector<float>, 3> grassBounceSamples_;
    std::vector<float> netImpactSample_;

    double noise();
    bool loadMonoPcm16Wav(const std::string &path, std::vector<float> &samples);
    bool loadAudioSamples();
    void submit(std::vector<float> &samples);
};

} // namespace tennis
