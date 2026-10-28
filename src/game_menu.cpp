#include "game.hpp"

#include <algorithm>
#include <array>

void Game::handlePauseClick(double x, double y) {
    auto inside = [x, y](double left, double top, double width, double height) {
        return x >= left && x <= left + width && y >= top && y <= top + height;
    };
    if (inside(785, 96, 95, 36)) {
        paused = false;
        pauseDrawing = false;
        return;
    }
    if (pausePage == PausePage::Main) {
        if (inside(500, 210, 280, 44)) {
            pausePage = PausePage::SwingGuide;
            gesturePointCount = 0;
            gesturePathLength = 0.0;
        }
        else if (inside(500, 270, 280, 44)) pausePage = PausePage::Rules;
        else if (inside(500, 330, 280, 44)) pausePage = PausePage::Scoring;
        else if (inside(500, 390, 280, 44)) pausePage = PausePage::Style;
        else if (inside(500, 450, 280, 44)) quitRequested = true;
        return;
    }
    if (inside(400, 96, pausePage == PausePage::MappingSettings ? 150 : 100, 36)) {
        pausePage = pausePage == PausePage::MappingSettings ? PausePage::SwingGuide : PausePage::Main;
        pauseDrawing = false;
        return;
    }
    if (pausePage == PausePage::SwingGuide) {
        if (inside(690, 150, 180, 36)) {
            pausePage = PausePage::MappingSettings;
            pauseDrawing = false;
        } else if (inside(690, 350, 180, 34)) {
            controlSettings.autoHit = !controlSettings.autoHit;
        }
    } else if (pausePage == PausePage::MappingSettings) {
        if (inside(620, 195, 240, 30)) {
            selectedMappingProfile = x < 735.0
                ? (selectedMappingProfile + 4) % 5
                : (selectedMappingProfile + 1) % 5;
            return;
        }
        MappingProfile &profile = mappingProfiles[selectedMappingProfile];
        auto adjust = [&](double &value, double top, double minimum, double maximum) {
            if (!inside(620, top, 240, 30)) return false;
            value = std::clamp(value + (x < 740.0 ? -0.05 : 0.05), minimum, maximum);
            return true;
        };
        if (adjust(profile.power, 235, 0.25, 2.00)) return;
        if (adjust(profile.powerMin, 270, 0.00, profile.powerMax - 0.05)) return;
        if (adjust(profile.powerMax, 305, profile.powerMin + 0.05, 1.25)) return;
        if (adjust(profile.verticalSpin, 340, 0.25, 2.00) ||
            adjust(profile.sideSpin, 375, 0.25, 2.00) ||
            adjust(profile.flatDrive, 410, 0.25, 2.00) ||
            adjust(profile.direction, 445, 0.25, 2.00) ||
            adjust(profile.height, 480, 0.25, 2.00)) return;
        if (adjust(profile.heightMin, 515, -1.00, profile.heightMax - 0.05)) return;
        if (adjust(profile.heightMax, 550, profile.heightMin + 0.05, 1.25)) return;
        if (inside(620, 590, 240, 30)) {
            static constexpr std::array<MappingProfile, 5> defaults{{
                {1.00, 0.02, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, -0.70, 1.00},
                {1.03, 0.05, 1.00, 0.95, 1.02, 1.00, 1.00, 1.00, -0.22, 0.90},
                {0.94, 0.06, 0.96, 0.88, 0.90, 0.95, 1.00, 0.95, -0.70, 0.85},
                {0.97, 0.04, 0.98, 0.94, 0.94, 0.98, 1.00, 1.00, -0.70, 0.90},
                {1.05, 0.10, 1.00, 0.88, 0.90, 1.05, 1.00, 1.05, -0.70, 0.75}
            }};
            profile = defaults[selectedMappingProfile];
        }
    } else if (pausePage == PausePage::Rules) {
        if (inside(620, 155, 230, 36)) {
            applyMatchPreset();
            return;
        }
        if (inside(620, 205, 230, 36)) {
            match.rules.matchPresentationEnabled = !match.rules.matchPresentationEnabled;
            return;
        }
        bool changed = false;
        if (inside(620, 265, 230, 36)) {
            match.rules.serveRulesEnabled = !match.rules.serveRulesEnabled;
            changed = true;
        } else if (inside(620, 325, 230, 36)) {
            match.rules.boundaryMode = match.rules.boundaryMode == tennis::BoundaryMode::Singles ? tennis::BoundaryMode::Doubles :
                           (match.rules.boundaryMode == tennis::BoundaryMode::Doubles ? tennis::BoundaryMode::None : tennis::BoundaryMode::Singles);
            changed = true;
        } else if (inside(620, 385, 230, 36)) {
            match.rules.multiBounceAllowed = !match.rules.multiBounceAllowed;
            changed = true;
        } else if (inside(620, 445, 230, 36)) {
            match.rules.serveMode = match.rules.serveMode == tennis::ServeMode::Alternate ? tennis::ServeMode::Player :
                        (match.rules.serveMode == tennis::ServeMode::Player ? tennis::ServeMode::Cpu : tennis::ServeMode::Alternate);
            changed = true;
        } else if (inside(620, 505, 42, 36) || inside(808, 505, 42, 36)) {
            int difficulty = static_cast<int>(aiDifficulty);
            difficulty = x < 700.0 ? (difficulty + 3) % 4 : (difficulty + 1) % 4;
            aiDifficulty = static_cast<tennis::AiDifficulty>(difficulty);
        }
        if (changed) {
            match.rules.customRules = true;
            if (!match.ball.inPlay) resetRally(true);
        }
    } else if (pausePage == PausePage::Scoring) {
        if (inside(620, 220, 230, 36)) {
            match.score.mode = match.score.mode == tennis::ScoreMode::Unlimited ? tennis::ScoreMode::Race :
                        (match.score.mode == tennis::ScoreMode::Race ? tennis::ScoreMode::Sets : tennis::ScoreMode::Unlimited);
            match.score.playerPoints = 0;
            match.score.opponentPoints = 0;
            match.score.playerGames = 0;
            match.score.opponentGames = 0;
            match.score.playerSets = 0;
            match.score.opponentSets = 0;
            match.score.playerSetGames.fill(0);
            match.score.opponentSetGames.fill(0);
            match.score.completedSetCount = 0;
            match.score.newMatchPending = false;
            match.endsSwapped = false;
            match.completedGames = 0;
            match.completedRallies = 0;
        } else if (inside(620, 280, 230, 36)) {
            match.score.raceTarget = match.score.raceTarget == 7 ? 11 : (match.score.raceTarget == 11 ? 21 : 7);
        } else if (inside(620, 340, 230, 36)) {
            match.score.bestOfSets = match.score.bestOfSets == 1 ? 3 : (match.score.bestOfSets == 3 ? 5 : 1);
        }
    } else if (pausePage == PausePage::Style) {
        if (inside(650, 168, 200, 34)) {
            hudSettings.showScoreboard = !hudSettings.showScoreboard;
        } else if (inside(650, 228, 200, 34)) {
            hudSettings.showContactAssist = !hudSettings.showContactAssist;
        } else if (inside(650, 288, 200, 34)) {
            hudSettings.showTopLeftInfo = !hudSettings.showTopLeftInfo;
        } else if (inside(620, 366, 42, 38) || inside(808, 366, 42, 38)) {
            int presetIndex = activeCourtPreset == tennis::CourtPreset::BouncyHard ? 0 :
                              (activeCourtPreset == tennis::CourtPreset::Clay ? 1 :
                               (activeCourtPreset == tennis::CourtPreset::Grass ? 2 : 3));
            presetIndex = x < 700.0 ? (presetIndex + 3) % 4 : (presetIndex + 1) % 4;
            setCourtPreset(presetIndex == 0 ? tennis::CourtPreset::BouncyHard :
                           (presetIndex == 1 ? tennis::CourtPreset::Clay :
                            (presetIndex == 2 ? tennis::CourtPreset::Grass :
                                                tennis::CourtPreset::FastHard)));
        }
    }
}
