#ifndef DEATHMATCH_SETUP_H
#define DEATHMATCH_SETUP_H

#include "appstate.h"

/**
 * @brief
 * A setup screen for the Deathmatch mode: pick the difficulty (which tanks
 * spawn and whether the special AI roles appear) and whether the friendly
 * tank (bot) joins the fight. X starts the match, O returns to the menu.
 */
class DeathmatchSetup : public AppState
{
public:
    DeathmatchSetup(InteractiveComponents interactive_components, StateMachine *state_machine);

    void draw(Renderer &renderer) override;
    void update(const UpdateState &updateState) override;
    void eventProcess(const Event &event) override;

protected:
    void onInitialize() override;

private:
    // 0 - difficulty row, 1 - friend bot row
    int m_selected_row;
    // Game::DM_EASY / DM_MEDIUM / DM_HARD
    int m_difficulty;
    bool m_friend_enabled;

    /** Starts the Deathmatch game with the selected options. */
    void startGame();
};

#endif // DEATHMATCH_SETUP_H
