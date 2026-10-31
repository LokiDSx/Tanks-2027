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

NameInput::NameInput(InteractiveComponents interactive_components, StateMachine *state_machine, Context context)
    : AppState(interactive_components, state_machine),
      m_context(context),
      m_dialog_finished(false),
      m_close_grace(0),
      m_blink(0)
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
    // Opens the system keyboard on the PS Vita, only enables text events
    // on desktop systems
    SDL_StartTextInput();
}

void NameInput::draw(Renderer &renderer)
{
    renderer.clear();

    renderer.drawRect(AppConfig::windows_rect, {180, 180, 180, 255}, true);

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

    if (ev.isPressed(KeyCode::KEY_RETURN))
    {
        tryConfirm();
    }
    else if (ev.isPressed(KeyCode::KEY_ESCAPE))
    {
        cancel();
    }
#ifndef VITA_PORT
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
