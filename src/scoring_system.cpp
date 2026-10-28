#include "scoring_system.hpp"

#include <algorithm>

namespace tennis {

ScoreResult ScoringSystem::recordPoint(ScoringState &score, bool playerWon) {
    if (score.newMatchPending) {
        score.playerPoints = 0;
        score.opponentPoints = 0;
        score.playerGames = 0;
        score.opponentGames = 0;
        score.playerSets = 0;
        score.opponentSets = 0;
        score.playerSetGames.fill(0);
        score.opponentSetGames.fill(0);
        score.completedSetCount = 0;
        score.newMatchPending = false;
    }
    if (playerWon) ++score.playerPoints;
    else ++score.opponentPoints;
    ScoreResult result{ScoreMilestone::Point, playerWon};

    if (score.mode == ScoreMode::Race) {
        if (score.playerPoints >= score.raceTarget || score.opponentPoints >= score.raceTarget) {
            result = {ScoreMilestone::Race, score.playerPoints > score.opponentPoints};
            score.playerPoints = 0;
            score.opponentPoints = 0;
        }
        return result;
    }
    if (score.mode != ScoreMode::Sets) return result;

    const bool playerGame = score.playerPoints >= 4 &&
                            score.playerPoints - score.opponentPoints >= 2;
    const bool opponentGame = score.opponentPoints >= 4 &&
                              score.opponentPoints - score.playerPoints >= 2;
    if (!playerGame && !opponentGame) return result;

    if (playerGame) ++score.playerGames;
    else ++score.opponentGames;
    score.playerPoints = 0;
    score.opponentPoints = 0;
    result = {ScoreMilestone::Game, playerGame};

    const bool playerSet = score.playerGames >= 6 && score.playerGames - score.opponentGames >= 2;
    const bool opponentSet = score.opponentGames >= 6 && score.opponentGames - score.playerGames >= 2;
    if (!playerSet && !opponentSet) return result;

    if (score.completedSetCount < static_cast<int>(score.playerSetGames.size())) {
        score.playerSetGames[score.completedSetCount] = score.playerGames;
        score.opponentSetGames[score.completedSetCount] = score.opponentGames;
        ++score.completedSetCount;
    }
    if (playerSet) ++score.playerSets;
    else ++score.opponentSets;
    score.playerGames = 0;
    score.opponentGames = 0;
    result = {ScoreMilestone::Set, playerSet};

    const int setsNeeded = score.bestOfSets / 2 + 1;
    if (score.playerSets < setsNeeded && score.opponentSets < setsNeeded) return result;

    result = {ScoreMilestone::Match, score.playerSets > score.opponentSets};
    score.newMatchPending = true;
    return result;
}

std::string ScoringSystem::displayPoint(int ownPoints, int opponentPoints, ScoreMode mode) {
    if (mode != ScoreMode::Sets) return std::to_string(ownPoints);
    static constexpr const char *labels[] = {"0", "15", "30", "40"};
    if (ownPoints >= 3 && opponentPoints >= 3) {
        if (ownPoints == opponentPoints) return "40";
        return ownPoints > opponentPoints ? "AD" : "40";
    }
    return labels[std::clamp(ownPoints, 0, 3)];
}

} // namespace tennis
