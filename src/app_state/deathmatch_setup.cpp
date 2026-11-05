#include "deathmatch_setup.h"
#include "../engine/renderer.h"
#include "../appconfig.h"
#include "../soundconfig.h"
#include "game/game.h"
#include "menu.h"

DeathmatchSetup::DeathmatchSetup(InteractiveComponents interactive_components, StateMachine *state_machine)
    : AppState(interactive_components, state_machine),
      m_selected_row(0),
      m_difficulty(Game::DM_MEDIUM),
      m_friend_enabled(true)
{
}

void DeathmatchSetup::onInitialize()
{
    stopAllSounds();
    // Hack to fill sound device buffer and avoid sound delay on first play
    playSound(SoundConfig::MENU_ITEM_SELECTED);
}

void DeathmatchSetup::draw(Renderer &renderer)
{
    renderer.clear();

    renderer.drawRect(AppConfig::windows_rect, {180, 180, 180, 255}, true);

    renderer.drawText({-1, 80}, "DEATHMATCH", {0, 0, 0, 255}, FontSize::BIG);

    const char *difficulty_names[] = {"EASY", "MEDIUM", "HARD"};
    const std::string items[] = {
        "Difficulty  [" + std::string(difficulty_names[m_difficulty]) + "]",
        "Friend bot  [" + std::string(m_friend_enabled ? "YES" : "NO") + "]",
    };

    int i = 0;
    for (const auto &item : items)
    {
        int y = 160 + i * 36;
        if (i == m_selected_row)
            renderer.drawText({110, y}, "> " + item, {255, 255, 255, 255}, FontSize::BIG);
        else
            renderer.drawText({110, y}, "  " + item, {0, 0, 0, 255}, FontSize::BIG);
        i++;
    }

#ifdef ANDROID_PORT
    renderer.drawText({-1, 300}, "LEFT/RIGHT - CHANGE   FIRE - PLAY", {0, 0, 0, 255}, FontSize::NORMAL);
    renderer.drawText({-1, 320}, "BACK - MENU", {0, 0, 0, 255}, FontSize::NORMAL);
#else
    renderer.drawText({-1, 300}, "X/LEFT/RIGHT - CHANGE   START - PLAY", {0, 0, 0, 255}, FontSize::NORMAL);
    renderer.drawText({-1, 320}, "O - BACK", {0, 0, 0, 255}, FontSize::NORMAL);
#endif

    renderer.flush();
}

void DeathmatchSetup::update(const UpdateState &)
{
}

void DeathmatchSetup::startGame()
{
    transiteTo(new Game(1, 1, m_interactive_components, m_state_machine,
                        true, false, true, m_difficulty, m_friend_enabled));
}

void DeathmatchSetup::eventProcess(const Event &event)
{
    if (event.type() != Event::KEYBOARD)
        return;

    const KeyboardEvent &ev = static_cast<const KeyboardEvent &>(event);
    if (ev.keyState() != KeyboardEvent::PRESSED)
        return;

    if (ev.isPressed(KeyCode::KEY_UP) || ev.isPressed(KeyCode::KEY_DOWN))
    {
        m_selected_row = (m_selected_row + 1) % 2;
        playSound(SoundConfig::MENU_ITEM_SELECTED);
    }
    else if (ev.isPressed(KeyCode::KEY_LEFT) || ev.isPressed(KeyCode::KEY_RIGHT))
    {
        if (m_selected_row == 0)
        {
            m_difficulty = (m_difficulty + 1) % 3;
        }
        else
        {
            m_friend_enabled = !m_friend_enabled;
        }
        playSound(SoundConfig::MENU_ITEM_SELECTED);
    }
    else if (ev.isPressed(KeyCode::KEY_SPACE))
    {
#ifdef ANDROID_PORT
        // The FIRE button plays the match (mirrors START on the console);
        // the on-screen PAUSE button does nothing on this screen.
        startGame();
#else
        // X changes the value of the selected row
        if (m_selected_row == 0)
        {
            m_difficulty = (m_difficulty + 1) % 3;
        }
        else
        {
            m_friend_enabled = !m_friend_enabled;
        }
        playSound(SoundConfig::MENU_ITEM_SELECTED);
#endif
    }
    else if (ev.isPressed(KeyCode::KEY_RETURN))
    {
#ifndef ANDROID_PORT
        // Start plays the match with the selected options. On Android the
        // PAUSE button (RETURN) must not launch the match - players expect
        // FIRE to start and PAUSE to be inert here.
        startGame();
#endif
    }
    else if (ev.isPressed(KeyCode::KEY_ESCAPE))
    {
        transiteTo(new Menu(m_interactive_components, m_state_machine));
    }
}
