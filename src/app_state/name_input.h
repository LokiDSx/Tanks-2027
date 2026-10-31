#ifndef NAME_INPUT_H
#define NAME_INPUT_H

#include "appstate.h"

#include <string>

/**
 * @brief
 * A screen asking for a player name (at most 6 characters: A-Z, 0-9 and
 * underscore). On the PS Vita the system on-screen keyboard is used
 * (SDL_StartTextInput opens the sceIme dialog, its result arrives as a
 * text event), on desktop systems the player simply types on the physical
 * keyboard.
 *
 * On confirm a fresh profile with the entered name is created and becomes
 * the active one. The screen can be opened at game startup (from the
 * profile list) or from Settings; Context decides where the screen
 * returns on success and on cancel.
 */
class NameInput : public AppState
{
public:
    enum class Context
    {
        STARTUP, // created -> Menu, cancelled -> back to the profile list
        SETTINGS // created -> Settings, cancelled -> Settings
    };

    NameInput(InteractiveComponents interactive_components, StateMachine *state_machine, Context context);

    void draw(Renderer &renderer) override;
    void update(const UpdateState &updateState) override;
    void eventProcess(const Event &event) override;

protected:
    void onInitialize() override;

private:
    Context m_context;
    // The entered name (sanitized, at most 6 characters)
    std::string m_name;
    // Error message shown on screen, empty when there is none
    std::string m_error;
    // PS Vita: the system keyboard has delivered its result
    bool m_dialog_finished;
    // Grace period after the keyboard result when Enter never followed
    // (the player closed the keyboard instead of confirming)
    Uint32 m_close_grace;
    // Cursor blink timer
    Uint32 m_blink;
#ifdef PSP_PORT
    // On-screen keyboard cursor position in the character grid (the PSP has
    // no system keyboard, so names are picked from an on-screen grid)
    int m_kb_row;
    int m_kb_col;

    /**
     * Move the on-screen keyboard cursor by one cell, skipping empty cells.
     */
    void adjustCursor(int dr, int dc);

    /**
     * Apply the on-screen keyboard cell under the cursor (insert a character,
     * delete the last one, or confirm the name).
     */
    void activateCursorCell();
#endif

    /**
     * (Re)open the system keyboard on the PS Vita; clears the pending
     * dialog state. On desktop it only re-enables text input events.
     */
    void startKeyboard();

    /**
     * Validate the entered name and create the profile. Shows an error
     * and asks again when the name is empty or already taken.
     */
    void tryConfirm();

    /**
     * Leave the screen without creating a profile.
     */
    void cancel();

    /**
     * Show an error message and (on the PS Vita) ask for the name again
     * with a fresh system keyboard.
     */
    void fail(const std::string &message);
};

#endif // NAME_INPUT_H
