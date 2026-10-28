#include "audio_system.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tennis {
namespace {

constexpr double Pi = 3.141592653589793;

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

double attackDecay(double time, double attackSeconds, double decayRate) {
    return std::min(1.0, time / attackSeconds) * std::exp(-time * decayRate);
}

} // namespace

AudioSystem::AudioSystem() {
    const SDL_AudioSpec sourceSpec{SDL_AUDIO_F32, 1, SampleRate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                        &sourceSpec, nullptr, nullptr);
    if (!stream_) {
        std::fprintf(stderr, "Audio disabled: %s\n", SDL_GetError());
        return;
    }
    if (!SDL_ResumeAudioStreamDevice(stream_)) {
        std::fprintf(stderr, "Audio resume failed: %s\n", SDL_GetError());
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
        return;
    }
    loadAudioSamples();
}

AudioSystem::~AudioSystem() {
    if (stream_) SDL_DestroyAudioStream(stream_);
}

double AudioSystem::noise() {
    noiseState_ ^= noiseState_ << 13;
    noiseState_ ^= noiseState_ >> 17;
    noiseState_ ^= noiseState_ << 5;
    return static_cast<double>(noiseState_) /
               static_cast<double>(0xFFFFFFFFu) * 2.0 - 1.0;
}

bool AudioSystem::loadMonoPcm16Wav(const std::string &path,
                                   std::vector<float> &samples) {
    SDL_AudioSpec spec{};
    Uint8 *data = nullptr;
    Uint32 byteCount = 0;
    if (!SDL_LoadWAV(path.c_str(), &spec, &data, &byteCount)) return false;
    const bool compatible = spec.format == SDL_AUDIO_S16 && spec.channels == 1 &&
                            spec.freq == SampleRate && byteCount % sizeof(Sint16) == 0;
    if (compatible) {
        const auto *pcm = reinterpret_cast<const Sint16 *>(data);
        const std::size_t sampleCount = byteCount / sizeof(Sint16);
        samples.resize(sampleCount);
        for (std::size_t index = 0; index < sampleCount; ++index) {
            samples[index] = static_cast<float>(pcm[index] / 32768.0);
        }
    }
    SDL_free(data);
    return compatible;
}

bool AudioSystem::loadAudioSamples() {
    const char *basePath = SDL_GetBasePath();
    if (!basePath) return false;
    const std::string audioDirectory = std::string(basePath) + "../Resources/audio/";
    bool loadedAll = true;
    auto loadSet = [&](const char *prefix,
                       std::array<std::vector<float>, 3> &destination) {
        for (std::size_t index = 0; index < destination.size(); ++index) {
            const std::string path = audioDirectory + prefix +
                                     std::to_string(index + 1) + ".wav";
            if (!loadMonoPcm16Wav(path, destination[index])) {
                loadedAll = false;
                std::fprintf(stderr, "Could not load sound: %s (%s)\n",
                             path.c_str(), SDL_GetError());
            }
        }
    };
    loadSet("tennis_hit_", racketHitSamples_);
    loadSet("match_forehand_", matchForehandSamples_);
    loadSet("match_serve_", matchServeSamples_);
    loadSet("bounce_hard_", hardBounceSamples_);
    loadSet("bounce_clay_", clayBounceSamples_);
    loadSet("bounce_grass_", grassBounceSamples_);
    const std::string netPath = audioDirectory + "net_impact.wav";
    if (!loadMonoPcm16Wav(netPath, netImpactSample_)) {
        loadedAll = false;
        std::fprintf(stderr, "Could not load sound: %s (%s)\n",
                     netPath.c_str(), SDL_GetError());
    }
    return loadedAll;
}

void AudioSystem::submit(std::vector<float> &samples) {
    if (!stream_ || samples.empty()) return;
    for (float &sample : samples) sample = std::clamp(sample, -0.92f, 0.92f);
    if (!SDL_PutAudioStreamData(stream_, samples.data(),
                                static_cast<int>(samples.size() * sizeof(float)))) {
        std::fprintf(stderr, "Audio queue failed: %s\n", SDL_GetError());
    }
}

void AudioSystem::playRacketHit(double power, double spinAmount, double accuracy,
                                bool serve) {
    power = clamp01(power);
    spinAmount = clamp01(spinAmount);
    accuracy = clamp01(accuracy);
    const double mishit = 1.0 - accuracy;
    const std::size_t sampleIndex = static_cast<std::size_t>(
        (noiseState_ + static_cast<std::uint32_t>(power * 17.0)) % 3u);
    const std::vector<float> &brightRecording = racketHitSamples_[sampleIndex];
    const std::vector<float> &bodyRecording = serve
        ? matchServeSamples_[sampleIndex] : matchForehandSamples_[sampleIndex];
    if (!brightRecording.empty() && !bodyRecording.empty()) {
        // The natural court recording supplies the loud, dark body heard in
        // men's match broadcasts. A quieter close professional recording adds
        // the short clean string transient without turning into a metallic ring.
        const double playbackRate = 0.975 + power * 0.018 + accuracy * 0.007;
        const std::size_t sourceCount = std::max(brightRecording.size(),
                                                 bodyRecording.size());
        const std::size_t outputCount = static_cast<std::size_t>(
            std::ceil(sourceCount / playbackRate));
        std::vector<float> output(outputCount);
        double filtered = 0.0;
        const double lowPassAmount = 0.82 - mishit * 0.48;
        const double gain = 0.42 + power * 0.56;
        double brush = 0.0;
        auto interpolated = [&](const std::vector<float> &source,
                                double position) {
            if (source.empty() || position >= source.size() - 1) return 0.0;
            const std::size_t first = static_cast<std::size_t>(position);
            const double fraction = position - first;
            return source[first] * (1.0 - fraction) +
                   source[first + 1] * fraction;
        };
        for (std::size_t index = 0; index < outputCount; ++index) {
            const double sourcePosition = index * playbackRate;
            const double body = interpolated(bodyRecording, sourcePosition);
            const double bright = interpolated(brightRecording, sourcePosition);
            const double source = body * 0.78 + bright * (0.22 + accuracy * 0.08);
            filtered += lowPassAmount * (source - filtered);
            brush += 0.10 * (noise() - brush);
            const double time = static_cast<double>(index) / SampleRate;
            const double spinLayer = brush * spinAmount * 0.020 * std::exp(-time * 64.0);
            output[index] = static_cast<float>(gain * filtered + spinLayer);
        }
        submit(output);
        return;
    }

    // Development fallback if packaged audio assets cannot be opened.
    const double duration = 0.052 + mishit * 0.018;
    const int count = static_cast<int>(duration * SampleRate);
    std::vector<float> samples(static_cast<std::size_t>(count));

    // Real tennis recordings are a very short broadband "thwack", not a low
    // note and not a ringing string tone. Most energy sits between roughly
    // 100 and 1800 Hz, with only a brief upper transient. Shape white noise
    // into those bands, then add heavily damped 500/900 Hz string responses.
    const double amplitude = 0.13 + power * 0.31;
    const double alpha250 = 1.0 - std::exp(-2.0 * Pi * 250.0 / SampleRate);
    const double alpha1800 = 1.0 - std::exp(-2.0 * Pi * 1800.0 / SampleRate);
    const double alpha3000 = 1.0 - std::exp(-2.0 * Pi * 3000.0 / SampleRate);
    double low250 = 0.0;
    double low1800 = 0.0;
    double low3000 = 0.0;
    for (int i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / SampleRate;
        const double raw = noise();
        low250 += alpha250 * (raw - low250);
        low1800 += alpha1800 * (raw - low1800);
        low3000 += alpha3000 * (raw - low3000);
        const double lowBody = low250;
        const double mainImpact = low1800 - low250;
        const double upperImpact = low3000 - low1800;

        const double attack = std::min(1.0, t / 0.00028);
        const double impactEnvelope = attack * std::exp(-t * (82.0 + accuracy * 22.0));
        const double frameEnvelope = attack * std::exp(-t * (48.0 - mishit * 12.0));
        const double stringEnvelope = attack * std::exp(-t * 155.0);
        const double frame = std::sin(2.0 * Pi * (128.0 - mishit * 16.0) * t);
        const double stringResponse =
            std::sin(2.0 * Pi * 505.0 * t) * 0.54 +
            std::sin(2.0 * Pi * (900.0 + spinAmount * 105.0) * t) * 0.32;

        // Sweet-spot contact is compact and focused. A mishit leaves more
        // low frame vibration; spin reinforces a short 1 kHz brushing band.
        const double broadband = impactEnvelope *
            (mainImpact * (0.92 - mishit * 0.16) +
             upperImpact * (0.20 + spinAmount * 0.18));
        const double lowThud = frameEnvelope *
            (lowBody * (0.30 + mishit * 0.54) + frame * (0.09 + mishit * 0.16));
        const double strings = stringEnvelope * stringResponse *
                               (0.13 + accuracy * 0.12 + spinAmount * 0.05);
        samples[static_cast<std::size_t>(i)] = static_cast<float>(
            amplitude * (broadband + lowThud + strings));
    }
    submit(samples);
}

void AudioSystem::playCourtBounce(CourtPreset court, double verticalImpactMps,
                                  double horizontalSpeedMps) {
    const double impact = clamp01((verticalImpactMps - 1.0) / 10.0);
    const double pace = clamp01(horizontalSpeedMps / 32.0);
    const std::array<std::vector<float>, 3> *sampleSet = &hardBounceSamples_;
    if (court == CourtPreset::Clay) sampleSet = &clayBounceSamples_;
    if (court == CourtPreset::Grass) sampleSet = &grassBounceSamples_;
    const std::size_t sampleIndex = static_cast<std::size_t>(noiseState_ % 3u);
    const std::vector<float> &recording = (*sampleSet)[sampleIndex];
    if (!recording.empty()) {
        const double surfaceRate = court == CourtPreset::FastHard ? 1.035 :
                                   (court == CourtPreset::BouncyHard ? 0.99 : 1.0);
        const double playbackRate = surfaceRate * (0.97 + impact * 0.05);
        const std::size_t outputCount = static_cast<std::size_t>(
            std::ceil(recording.size() / playbackRate));
        std::vector<float> output(outputCount);
        const double gain = 0.16 + impact * 0.34 + pace * 0.05;
        for (std::size_t index = 0; index < outputCount; ++index) {
            const double position = index * playbackRate;
            const std::size_t first = std::min(
                static_cast<std::size_t>(position), recording.size() - 1);
            const std::size_t second = std::min(first + 1, recording.size() - 1);
            const double fraction = position - first;
            output[index] = static_cast<float>(gain *
                (recording[first] * (1.0 - fraction) + recording[second] * fraction));
        }
        submit(output);
        return;
    }
    double bodyFrequency = 122.0;
    double noiseFilter = 0.15;
    double noiseMix = 0.16;
    double duration = 0.060;
    double decay = 46.0;
    switch (court) {
        case CourtPreset::Clay:
            bodyFrequency = 92.0; noiseFilter = 0.075;
            noiseMix = 0.48; duration = 0.090; decay = 32.0;
            break;
        case CourtPreset::Grass:
            bodyFrequency = 76.0; noiseFilter = 0.060;
            noiseMix = 0.34; duration = 0.055; decay = 52.0;
            break;
        case CourtPreset::FastHard:
            bodyFrequency = 145.0; noiseFilter = 0.22;
            noiseMix = 0.14; duration = 0.052; decay = 56.0;
            break;
        default: // Bouncy hard court: fuller and slightly lower than fast hard.
            bodyFrequency = 115.0; noiseFilter = 0.17;
            break;
    }

    const int count = static_cast<int>(duration * SampleRate);
    std::vector<float> samples(static_cast<std::size_t>(count));
    const double amplitude = 0.09 + impact * 0.22 + pace * 0.045;
    double surfaceNoise = 0.0;
    for (int i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / SampleRate;
        const double envelope = attackDecay(t, 0.0007, decay);
        surfaceNoise += noiseFilter * (noise() - surfaceNoise);
        const double body = std::sin(2.0 * Pi * bodyFrequency * t) * std::exp(-t * 44.0);
        const double contact = surfaceNoise * std::exp(-t * 105.0);
        const double grit = surfaceNoise * noiseMix * (0.65 + pace * 0.35);
        samples[static_cast<std::size_t>(i)] = static_cast<float>(amplitude * envelope *
            (body * 0.58 + contact * 0.46 + grit));
    }
    submit(samples);
}

void AudioSystem::playNetHit(NetCollisionOutcome outcome, double speedMps) {
    if (outcome == NetCollisionOutcome::None) return;
    const double pace = clamp01(speedMps / 35.0);
    const bool cord = outcome == NetCollisionOutcome::CordForward ||
                      outcome == NetCollisionOutcome::CordBack;
    if (!netImpactSample_.empty()) {
        std::vector<float> output(netImpactSample_.size());
        double filtered = 0.0;
        const double filterAmount = cord ? 0.82 : 0.38;
        const double gain = 0.18 + pace * 0.34;
        for (std::size_t index = 0; index < output.size(); ++index) {
            filtered += filterAmount * (netImpactSample_[index] - filtered);
            output[index] = static_cast<float>(filtered * gain);
        }
        submit(output);
        return;
    }
    const double duration = cord ? 0.095 : 0.075;
    const int count = static_cast<int>(duration * SampleRate);
    std::vector<float> samples(static_cast<std::size_t>(count));
    const double amplitude = 0.10 + pace * 0.18;
    double meshNoise = 0.0;
    for (int i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / SampleRate;
        meshNoise += 0.09 * (noise() - meshNoise);
        const double envelope = attackDecay(t, 0.0008, cord ? 34.0 : 44.0);
        const double cable = std::sin(2.0 * Pi * (cord ? 96.0 : 72.0) * t) *
                             std::exp(-t * 38.0);
        const double mesh = meshNoise * (cord ? 0.30 : 0.58);
        samples[static_cast<std::size_t>(i)] = static_cast<float>(amplitude * envelope *
            (cable * (cord ? 0.52 : 0.28) + mesh));
    }
    submit(samples);
}

} // namespace tennis
