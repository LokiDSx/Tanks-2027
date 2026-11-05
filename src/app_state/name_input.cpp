#include "name_input.h"
#include "../engine/renderer.h"
#include "../appconfig.h"
#include "../soundconfig.h"
#include "../profile.h"
#include "profile_select.h"
#include "settings.h"
#include "menu.h"

#include <SDL2/SDL.h>

#ifdef VITA_PORT
#include <psp2/apputil.h>
#include <psp2/common_dialog.h>
#include <psp2/ime_dialog.h>
#endif

#ifdef PSP_PORT
namespace
{
    // On-screen keyboard grid for the PSP (no system keyboard there).
    // '\x01' = delete-last cell, '\x02' = OK cell, '\x00' = empty slot.
    const int KB_ROWS = 4;
    const int KB_COLS = 10;
    const char KB_GRID[4][10] = {
        {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'},
        {'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T'},
        {'U', 'V', 'W', 'X', 'Y', 'Z', '0', '1', '2', '3'},
        {'4', '5', '6', '7', '8', '9', '_', '\x01', '\x02', '\x00'},
    };
    const char KB_DELETE = '\x01';
    const char KB_OK = '\x02';
    const char KB_EMPTY = '\x00';
} // namespace
#endif

NameInput::NameInput(InteractiveComponents interactive_components, StateMachine *state_machine, Context context)
    : AppState(interactive_components, state_machine),
      m_context(context),
      m_dialog_finished(false),
      m_close_grace(0),
      m_blink(0)
#ifdef PSP_PORT
      ,
      m_kb_row(0),
      m_kb_col(0)
#endif
{
}

void NameInput::onInitialize()
{
    stopAllSounds();
    // Hack to fill sound device buffer and avoid sound delay on first play
    playSound(SoundConfig::MENU_ITEM_SELECTED);

#ifdef VITA_PORT
    // One-time dialog bootstrap mirroring the official VitaSDK IME sample.
    // Repeating the calls later fails harmlessly.
    static bool s_dialog_configured = false;
    if (!s_dialog_configured)
    {
        SceAppUtilInitParam init_param;
        SceAppUtilBootParam boot_param;
        SDL_memset(&init_param, 0, sizeof(init_param));
        SDL_memset(&boot_param, 0, sizeof(boot_param));
        sceAppUtilInit(&init_param, &boot_param);

        SceCommonDialogConfigParam config_param;
        sceCommonDialogConfigParamInit(&config_param);
        sceCommonDialogSetConfigParam(&config_param);

        s_dialog_configured = true;
    }
#endif

    startKeyboard();
}

void NameInput::startKeyboard()
{
    m_dialog_finished = false;
    m_close_grace = 0;
#ifdef PSP_PORT
    // Reset the on-screen keyboard cursor
    m_kb_row = 0;
    m_kb_col = 0;
#else
    // Opens the system keyboard on the PS Vita, only enables text events
    // on desktop systems. Not called on the PSP: SDL_StartTextInput() there
    // opens the system OSK dialog on top of our own on-screen keyboard.
    SDL_StartTextInput();
#endif
}

void NameInput::draw(Renderer &renderer)
{
    renderer.clear();

    renderer.drawRect(AppConfig::windows_rect, {180, 180, 180, 255}, true);

#ifdef PSP_PORT
    // Compact layout for the 480x272 screen with an on-screen keyboard
    renderer.drawText({-1, 30}, "ENTER NAME", {0, 0, 0, 255}, FontSize::BIG);

    const std::string shown = m_name + (m_blink < 500 ? "_" : " ");
    renderer.drawRect({116, 58, 232, 34}, {0, 0, 0, 255}, true);
    renderer.drawText({-1, 64}, shown, {255, 255, 255, 255}, FontSize::BIGGER);

    renderer.drawText({-1, 98}, "MAX 6 CHARS: A-Z 0-9 _", {0, 0, 0, 255}, FontSize::NORMAL);

    if (!m_error.empty())
        renderer.drawText({-1, 116}, m_error, {170, 0, 0, 255}, FontSize::NORMAL);

    // On-screen keyboard grid
    const int grid_x = 42, grid_y = 140, cell_w = 36, cell_h = 24, gap = 2;
    for (int r = 0; r < KB_ROWS; ++r)
    {
        for (int c = 0; c < KB_COLS; ++c)
        {
            const char cell = KB_GRID[r][c];
            if (cell == KB_EMPTY)
                continue;

            const int x = grid_x + c * (cell_w + gap);
            const int y = grid_y + r * (cell_h + gap);
            const bool selected = (r == m_kb_row && c == m_kb_col);

            renderer.drawRect({x, y, cell_w, cell_h},
                              selected ? Color{0, 0, 0, 255} : Color{210, 210, 210, 255}, true);

            std::string label;
            if (cell == KB_DELETE)
                label = "<";
            else if (cell == KB_OK)
                label = "OK";
            else
                label = std::string(1, cell);

            const Size ts = renderer.measureText(label, FontSize::NORMAL);
            renderer.drawText({x + (cell_w - ts.w) / 2, y + (cell_h - ts.h) / 2}, label,
                              selected ? Color{255, 255, 255, 255} : Color{0, 0, 0, 255}, FontSize::NORMAL);
        }
    }

    renderer.drawText({-1, 280}, "X - SELECT   START - OK   O - BACK", {0, 0, 0, 255}, FontSize::NORMAL);
#else
    renderer.drawText({-1, 70}, "ENTER NAME", {0, 0, 0, 255}, FontSize::BIG);

    // The name typed so far with a blinking cursor
    const std::string shown = m_name + (m_blink < 500 ? "_" : " ");
    renderer.drawRect({116, 130, 232, 44}, {0, 0, 0, 255}, true);
    renderer.drawText({-1, 145}, shown, {255, 255, 255, 255}, FontSize::BIGGER);

    renderer.drawText({-1, 210}, "MAX 6 CHARS: A-Z 0-9 _", {0, 0, 0, 255}, FontSize::NORMAL);

    if (!m_error.empty())
        renderer.drawText({-1, 250}, m_error, {170, 0, 0, 255}, FontSize::BIG);

#ifdef VITA_PORT
    renderer.drawText({-1, 310}, "TYPE WITH THE SYSTEM KEYBOARD", {0, 0, 0, 255}, FontSize::NORMAL);
    renderer.drawText({-1, 330}, "ENTER - OK   O - BACK", {0, 0, 0, 255}, FontSize::NORMAL);
#else
    renderer.drawText({-1, 310}, "SIMPLY TYPE THE NAME", {0, 0, 0, 255}, FontSize::NORMAL);
    renderer.drawText({-1, 330}, "ENTER - OK   ESC - BACK", {0, 0, 0, 255}, FontSize::NORMAL);
#endif
#endif

    renderer.flush();
}

void NameInput::update(const UpdateState &updateState)
{
    m_blink = (m_blink + updateState.delta_time) % 1000;

#ifdef VITA_PORT
    // The system keyboard delivered its text but Enter never followed:
    // the player closed the keyboard, cancel the whole name input.
    if (m_dialog_finished)
    {
        m_close_grace += updateState.delta_time;
        if (m_close_grace > 200)
        {
            cancel();
        }
    }
#endif
}

void NameInput::eventProcess(const Event &event)
{
    if (event.type() == Event::TEXT_INPUT)
    {
        const TextInputEvent &ev = static_cast<const TextInputEvent &>(event);

#ifdef VITA_PORT
        if (!m_dialog_finished)
        {
            // The system keyboard delivers the whole string at once
            m_name = Profile::sanitize(ev.text());
            m_dialog_finished = true;
        }
#else
        for (char c : ev.text())
        {
            const std::string piece = Profile::sanitize(std::string(1, c));
            if (!piece.empty() && m_name.size() < 6)
                m_name += piece;
        }
        m_error.clear();
#endif
        return;
    }

    if (event.type() != Event::KEYBOARD)
        return;

    const KeyboardEvent &ev = static_cast<const KeyboardEvent &>(event);
    if (ev.keyState() != KeyboardEvent::PRESSED)
        return;

#ifdef VITA_PORT
    // While the system keyboard is on screen it owns the input. Keys are
    // only processed when no keyboard dialog is up, which also lets the
    // player back out with Circle if the keyboard failed to open.
    if (!m_dialog_finished && sceImeDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING)
        return;
#endif

#ifdef PSP_PORT
    // On-screen keyboard driven by the pad: D-pad moves the cursor and
    // Cross/Square/R (SPACE) selects the cell under it.
    if (ev.isPressed(KeyCode::KEY_UP))
    {
        adjustCursor(-1, 0);
        return;
    }
    if (ev.isPressed(KeyCode::KEY_DOWN))
    {
        adjustCursor(1, 0);
        return;
    }
    if (ev.isPressed(KeyCode::KEY_LEFT))
    {
        adjustCursor(0, -1);
        return;
    }
    if (ev.isPressed(KeyCode::KEY_RIGHT))
    {
        adjustCursor(0, 1);
        return;
    }
    if (ev.isPressed(KeyCode::KEY_SPACE))
    {
        activateCursorCell();
        return;
    }
#endif

    if (ev.isPressed(KeyCode::KEY_RETURN)
#ifdef ANDROID_PORT
        // FIRE confirms the entered name
        || ev.isPressed(KeyCode::KEY_SPACE)
#endif
    )
    {
        tryConfirm();
    }
    else if (ev.isPressed(KeyCode::KEY_ESCAPE))
    {
        cancel();
    }
#if !defined(VITA_PORT) && !defined(PSP_PORT)
    else if (ev.isPressed(KeyCode::KEY_BACKSPACE))
    {
        if (!m_name.empty())
        {
            m_name.pop_back();
            m_error.clear();
        }
    }
#endif
}

void NameInput::tryConfirm()
{
    const std::string name = Profile::sanitize(m_name);
    if (name.empty())
    {
        fail("ENTER A NAME");
        return;
    }
    if (Profile::exists(name))
    {
        fail("NAME ALREADY TAKEN");
        return;
    }
    if (!Profile::create(name))
    {
        fail("COULD NOT CREATE PROFILE");
        return;
    }

    // The new profile brings its own settings
    m_interactive_components.sound_manager->setMuted(!AppConfig::sound_enabled);
    SDL_StopTextInput();

    if (m_context == Context::SETTINGS)
    {
        transiteTo(new Settings(m_interactive_components, m_state_machine));
    }
    else
    {
        transiteTo(new Menu(m_interactive_components, m_state_machine));
    }
}

void NameInput::cancel()
{
    SDL_StopTextInput();

    if (m_context == Context::SETTINGS)
    {
        transiteTo(new Settings(m_interactive_components, m_state_machine));
    }
    else
    {
        transiteTo(new ProfileSelect(m_interactive_components, m_state_machine));
    }
}

void NameInput::fail(const std::string &message)
{
    m_error = message;
    playSound(SoundConfig::BONUS_APPEARED);
#ifdef VITA_PORT
    // Ask for the name again with a fresh system keyboard
    startKeyboard();
#endif
}

#ifdef PSP_PORT
void NameInput::adjustCursor(int dr, int dc)
{
    int r = m_kb_row + dr;
    int c = m_kb_col + dc;

    // Skip empty slots while moving in the same direction (bounded)
    int steps = 0;
    while (r >= 0 && r < KB_ROWS && c >= 0 && c < KB_COLS && KB_GRID[r][c] == KB_EMPTY && steps < 10)
    {
        r += dr;
        c += dc;
        ++steps;
    }

    if (r < 0 || r >= KB_ROWS || c < 0 || c >= KB_COLS)
        return; // out of the grid, keep the cursor in place

    m_kb_row = r;
    m_kb_col = c;
    playSound(SoundConfig::MENU_ITEM_SELECTED);
}

void NameInput::activateCursorCell()
{
    const char cell = KB_GRID[m_kb_row][m_kb_col];

    if (cell == KB_DELETE)
    {
        if (!m_name.empty())
        {
            m_name.pop_back();
            m_error.clear();
            playSound(SoundConfig::MENU_ITEM_SELECTED);
        }
    }
    else if (cell == KB_OK)
    {
        tryConfirm();
    }
    else if (cell != KB_EMPTY && m_name.size() < 6)
    {
        m_name += cell;
        m_error.clear();
        playSound(SoundConfig::MENU_ITEM_SELECTED);
    }
}
#endif
