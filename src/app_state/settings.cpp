#include "settings.h"
#include "../engine/renderer.h"
#include "../appconfig.h"
#include "../soundconfig.h"
#include "../profile.h"
#include "name_input.h"
#include "profile_select.h"
#include "menu.h"

#include <string>

namespace
{
    const int ITEM_COUNT = 6;
}

Settings::Settings(InteractiveComponents interactive_components, StateMachine *state_machine)
    : AppState(interactive_components, state_machine),
      m_current_index(0),
      m_confirm_delete(false)
{
}

void Settings::onInitialize()
{
    stopAllSounds();
    // Hack to fill sound device buffer and avoid sound delay on first play
    playSound(SoundConfig::MENU_ITEM_SELECTED);
}

void Settings::draw(Renderer &renderer)
{
    renderer.clear();

    renderer.drawRect(AppConfig::windows_rect, {180, 180, 180, 255}, true);

    renderer.drawText({-1, 60}, "SETTINGS", {0, 0, 0, 255}, FontSize::BIG);

    const bool widescreen_on = AppConfig::widescreen;
    const bool sound_on = AppConfig::sound_enabled;
    const std::string profile_name = Profile::current().empty() ? "-" : Profile::current();

    const std::string items[] = {
        "Widescreen  [" + std::string(widescreen_on ? "ON" : "OFF") + "]",
        "Sound  [" + std::string(sound_on ? "ON" : "OFF") + "]",
        "Profile  " + profile_name,
        "New profile",
        "Switch profile",
        "Delete profile",
    };

    int i = 0;
    for (const auto &item : items)
    {
        int y = 110 + i * 30;
        if (i == m_current_index)
            renderer.drawText({120, y}, "> " + item, {255, 255, 255, 255}, FontSize::BIG);
        else
            renderer.drawText({120, y}, "  " + item, {0, 0, 0, 255}, FontSize::BIG);
        i++;
    }

    if (m_confirm_delete)
        renderer.drawText({-1, 310}, "X AGAIN - DELETE   O - CANCEL", {170, 0, 0, 255}, FontSize::NORMAL);
    else if (!m_error.empty())
        renderer.drawText({-1, 310}, m_error, {170, 0, 0, 255}, FontSize::NORMAL);

    renderer.drawText({-1, 340}, "LEFT/RIGHT OR X - SELECT", {0, 0, 0, 255}, FontSize::NORMAL);
    renderer.drawText({-1, 360}, "O - BACK", {0, 0, 0, 255}, FontSize::NORMAL);

    renderer.flush();
}

void Settings::update(const UpdateState &)
{
}

void Settings::toggleCurrent()
{
    if (m_current_index == 0)
    {
        // The renderer re-applies the viewport on the next frame
        AppConfig::widescreen = !AppConfig::widescreen;
        playSound(SoundConfig::MENU_ITEM_SELECTED);
    }
    else
    {
        AppConfig::sound_enabled = !AppConfig::sound_enabled;
        m_interactive_components.sound_manager->setMuted(!AppConfig::sound_enabled);
        if (AppConfig::sound_enabled)
        {
            // Confirm the sound is back with a click
            playSound(SoundConfig::MENU_ITEM_SELECTED);
        }
        else
        {
            stopAllSounds();
        }
    }

    Profile::save();
}

void Settings::activateCurrent()
{
    if (m_current_index <= 1)
    {
        toggleCurrent();
    }
    else if (m_current_index == 2)
    {
        // Information row: the name of the active profile
    }
    else if (m_current_index == 3)
    {
        transiteTo(new NameInput(m_interactive_components, m_state_machine, NameInput::Context::SETTINGS));
    }
    else if (m_current_index == 4)
    {
        transiteTo(new ProfileSelect(m_interactive_components, m_state_machine, true));
    }
    else
    {
        deleteCurrentProfile();
    }
}

void Settings::deleteCurrentProfile()
{
    if (!m_confirm_delete)
    {
        m_confirm_delete = true;
        m_error.clear();
        playSound(SoundConfig::MENU_ITEM_SELECTED);
        return;
    }

    m_confirm_delete = false;

    if (Profile::list().size() <= 1)
    {
        m_error = "CAN'T DELETE THE LAST PROFILE";
        playSound(SoundConfig::BONUS_APPEARED);
        return;
    }

    Profile::remove(Profile::current());
    // The profile that became active brings its own settings
    m_interactive_components.sound_manager->setMuted(!AppConfig::sound_enabled);
    m_error.clear();
}

void Settings::eventProcess(const Event &event)
{
    if (event.type() != Event::KEYBOARD)
        return;

    const KeyboardEvent &ev = static_cast<const KeyboardEvent &>(event);
    if (ev.keyState() != KeyboardEvent::PRESSED)
        return;

    if (ev.isPressed(KeyCode::KEY_UP))
    {
        m_current_index = (m_current_index + ITEM_COUNT - 1) % ITEM_COUNT;
        m_confirm_delete = false;
        m_error.clear();
        playSound(SoundConfig::MENU_ITEM_SELECTED);
    }
    else if (ev.isPressed(KeyCode::KEY_DOWN))
    {
        m_current_index = (m_current_index + 1) % ITEM_COUNT;
        m_confirm_delete = false;
        m_error.clear();
        playSound(SoundConfig::MENU_ITEM_SELECTED);
    }
    else if (ev.isPressed(KeyCode::KEY_ESCAPE))
    {
        if (m_confirm_delete)
        {
            m_confirm_delete = false;
        }
        else
        {
            transiteTo(new Menu(m_interactive_components, m_state_machine));
        }
    }
    else if (ev.isPressed(KeyCode::KEY_RETURN) || ev.isPressed(KeyCode::KEY_SPACE))
    {
        activateCurrent();
    }
    else if (ev.isPressed(KeyCode::KEY_LEFT) || ev.isPressed(KeyCode::KEY_RIGHT))
    {
        // Left/Right only makes sense outside of the delete confirmation
        if (!m_confirm_delete)
        {
            activateCurrent();
        }
    }
}
