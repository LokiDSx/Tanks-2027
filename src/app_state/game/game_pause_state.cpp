#include "game.h"
#include "../../appconfig.h"
#include "../../soundconfig.h"
#include "../menu.h"

Game::PauseState::PauseState(Game *ps) : ContextState(ps, ps->m_game_state_machine), m_selected(0)
{
}

void Game::PauseState::onInitialize()
{
    m_context->stopAllSounds();
    m_context->playSound(SoundConfig::PAUSE);
}

void Game::PauseState::draw(Renderer &renderer)
{
    m_context->drawScene(renderer);

    renderer.drawText({-1, 130}, std::string("PAUSE"), {200, 0, 0, 255}, FontSize::BIGGEST);

    const std::string items[] = {
        "RETURN",
        "MAIN MENU",
    };

    int i = 0;
    for (const auto &item : items)
    {
        const int y = 220 + i * 40;
        if (i == m_selected)
            renderer.drawText({-1, y}, "> " + item, {255, 255, 255, 255}, FontSize::BIG);
        else
            renderer.drawText({-1, y}, "  " + item, {170, 170, 170, 255}, FontSize::BIG);
        i++;
    }

    renderer.drawText({-1, 340}, "X - SELECT   O - BACK", {200, 200, 200, 255}, FontSize::NORMAL);
}

void Game::PauseState::update(const UpdateState &updateState)
{
}

void Game::PauseState::activateSelected()
{
    if (m_selected == 0)
    {
        // Close the pause menu and continue playing
        transiteTo(new Game::PlayingState(m_context));
    }
    else
    {
        m_context->transiteTo(new Menu(m_context->m_interactive_components, m_context->m_state_machine));
    }
}

void Game::PauseState::eventProcess(const Event &event)
{
    if (event.type() != Event::KEYBOARD)
        return;

    const KeyboardEvent &event_key = static_cast<const KeyboardEvent &>(event);
    if (event_key.keyState() != KeyboardEvent::PRESSED)
        return;

    if (event_key.isPressed(KEY_UP))
    {
        m_selected = (m_selected + 1) % 2;
        m_context->playSound(SoundConfig::MENU_ITEM_SELECTED);
    }
    else if (event_key.isPressed(KEY_DOWN))
    {
        m_selected = (m_selected + 1) % 2;
        m_context->playSound(SoundConfig::MENU_ITEM_SELECTED);
    }
    else if (event_key.isPressed(KEY_RETURN) || event_key.isPressed(KEY_SPACE))
    {
        activateSelected();
    }
    else if (event_key.isPressed(KEY_ESCAPE))
    {
        // Circle closes the pause menu the same way as the RETURN item;
        // leaving the level is done through MAIN MENU
        transiteTo(new Game::PlayingState(m_context));
    }
}
