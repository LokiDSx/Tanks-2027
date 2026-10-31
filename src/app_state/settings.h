#ifndef SETTINGS_H
#define SETTINGS_H

#include "appstate.h"

#include <string>

/**
 * @brief
 * A state with application settings: widescreen display mode (stretch to
 * fill), sound on/off and player profile management (view the active
 * profile, create a new one, switch to another one, delete the current
 * one). Opened from the main menu, Circle/Esc returns back to the menu.
 */
class Settings : public AppState
{
public:
    Settings(InteractiveComponents interactive_components, StateMachine *state_machine);

    void draw(Renderer &renderer) override;
    void update(const UpdateState &updateState) override;
    void eventProcess(const Event &event) override;

protected:
    void onInitialize() override;

private:
    int m_current_index;
    // Delete confirmation sub-state ("X again to confirm")
    bool m_confirm_delete;
    // Feedback for actions that could not be performed, empty when none
    std::string m_error;

    /**
     * Toggles the selected setting (Widescreen or Sound) or opens the
     * screen of the selected profile item (New/Switch/Delete).
     */
    void activateCurrent();

    /**
     * Toggles Widescreen or Sound and saves the profile.
     */
    void toggleCurrent();

    /**
     * First press arms the confirmation, the second one deletes the
     * current profile (the last remaining profile cannot be deleted).
     */
    void deleteCurrentProfile();
};

#endif // SETTINGS_H
