#pragma once

#include "vec2.hpp"

#include <array>

namespace tennis {

enum class BoundaryMode { Singles, Doubles, None };
enum class ServeMode { Alternate, Player, Cpu };
enum class ScoreMode { Unlimited, Race, Sets };

struct BallState {
    Vec2 position;
    Vec2 velocity;
    double height = 0.9;
    double verticalVelocity = 0.0;
    double spinRpm = 0.0;
    double sideSpinRpm = 0.0;
    int bounceCount = 0;
    int lastHitter = 1;
    bool inPlay = false;
    bool serviceShot = false;
    bool serviceTargetLeft = true;
    bool serviceFromBottom = true;
    bool serveReturnPending = false;
    bool netHit = false;
};

struct RuleSettings {
    bool serveRulesEnabled = true;
    bool multiBounceAllowed = false;
    bool customRules = false;
    bool matchPresentationEnabled = true;
    BoundaryMode boundaryMode = BoundaryMode::Singles;
    ServeMode serveMode = ServeMode::Alternate;
};

struct ScoringState {
    ScoreMode mode = ScoreMode::Sets;
    int raceTarget = 11;
    int bestOfSets = 3;
    int playerPoints = 0;
    int opponentPoints = 0;
    int playerSets = 0;
    int opponentSets = 0;
    int playerGames = 0;
    int opponentGames = 0;
    std::array<int, 5> playerSetGames{};
    std::array<int, 5> opponentSetGames{};
    int completedSetCount = 0;
    bool newMatchPending = false;
};

// This is the serializable gameplay snapshot used by saves, replays and,
// eventually, network synchronization. Presentation-only timers stay in Game.
struct MatchState {
    BallState ball;
    Vec2 playerPosition{640.0, 665.0};
    Vec2 opponentPosition{640.0, 35.0};
    double playerStamina = 100.0;
    double opponentStamina = 100.0;
    int completedRallies = 0;
    int completedGames = 0;
    int serveFaultCount = 0;
    bool currentServerPlayer = true;
    bool endsSwapped = false;
    RuleSettings rules;
    ScoringState score;
};

} // namespace tennis
