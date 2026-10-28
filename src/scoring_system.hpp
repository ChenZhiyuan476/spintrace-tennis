#pragma once

#include "match_state.hpp"

#include <string>

namespace tennis {

enum class ScoreMilestone { Point, Game, Set, Match, Race };

struct ScoreResult {
    ScoreMilestone milestone = ScoreMilestone::Point;
    bool playerWon = false;
};

class ScoringSystem {
public:
    static ScoreResult recordPoint(ScoringState &score, bool playerWon);
    static std::string displayPoint(int ownPoints, int opponentPoints, ScoreMode mode);
};

} // namespace tennis
