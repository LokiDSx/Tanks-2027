#ifndef HIGH_SCORE_H
#define HIGH_SCORE_H

#include "appconfig.h"

namespace HighScore {

    // Each game mode keeps its own high score (0 until the first played game)
    enum class Mode { Classic = 0, Generated = 1, Survival = 2, Deathmatch = 3 };

    inline unsigned scores[4] = {0, 0, 0, 0};

    // Survival: record of destroyed tanks (a tank counter shown on the
    // status panel during the game, fixed like a high score)
    inline unsigned survival_tanks = 0;

    inline unsigned &forMode(Mode mode) {
        return scores[static_cast<int>(mode)];
    }

    // The best score across all modes (displayed at the top of the menu)
    inline unsigned best() {
        unsigned best_score = scores[0];
        for (int i = 1; i < 4; i++)
            if (scores[i] > best_score)
                best_score = scores[i];
        return best_score;
    }

    inline bool checkAndUpdate(Mode mode, unsigned new_score) {
        unsigned &score = forMode(mode);
        if (new_score > 0 && new_score > score) {
            score = new_score;
            return true;
        }
        return false;
    }
}

#endif // HIGH_SCORE_H
