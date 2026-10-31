#ifndef PROFILE_SELECT_H
#define PROFILE_SELECT_H

#include "appstate.h"

#include <string>
#include <vector>

/**
 * @brief
 * A screen listing all player profiles (with the best score of each) and
 * a "NEW PROFILE" row. Used at game startup to choose the profile to open
 * and from Settings to switch the active profile.
 *
 * From the startup list Circle/Esc quits the application; from Settings it
 * returns back to Settings.
 */
class ProfileSelect : public AppState
{
public:
    /**
     * @param switch_mode true when opened from Settings (Circle/Esc and a
     * chosen profile lead back to Settings), false at game startup
     * (Circle/Esc quits, a chosen profile opens the main menu).
     */
    ProfileSelect(InteractiveComponents interactive_components, StateMachine *state_machine, bool switch_mode = false);

    void draw(Renderer &renderer) override;
    void update(const UpdateState &updateState) override;
    void eventProcess(const Event &event) override;

protected:
    void onInitialize() override;

private:
    bool m_switch_mode;
    int m_current_index;
    // Index of the first visible row (the list scrolls when it is long)
    int m_offset;
    // Best scores cached per profile for drawing
    std::vector<unsigned> m_scores;

    /** Profiles plus the "NEW PROFILE" row. */
    int rowsCount() const;

    /** Activate the selected profile or start creating a new one. */
    void openCurrent();

    /** Keep the selection inside the visible window. */
    void adjustOffset();
};

#endif // PROFILE_SELECT_H
