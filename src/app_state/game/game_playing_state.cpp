#include "game.h"
#include "../../appconfig.h"
#include "../menu.h"

Game::PlayingState::PlayingState(Game *ps) : ContextState(ps, ps->m_game_state_machine) {}

void Game::PlayingState::draw(Renderer &renderer)
{
    m_context->drawScene(renderer);
    // Victory banner of the last classic stage (the game ends after it)
    m_context->drawYouWinMessage(renderer);
}

void Game::PlayingState::update(const UpdateState &updateState)
{
    m_context->updateScene(updateState.delta_time);

    if (m_context->m_you_won)
    {
        // The last stage was already won: show the "YOU WIN!" banner for
        // a moment, then finish the game (scores screen, then the menu).
        m_context->m_you_win_time += updateState.delta_time;
        if (m_context->m_you_win_time > AppConfig::Game::you_win_show_time)
            transiteTo(new Game::StageEndingState(m_context, true, true));
    }
    else if (m_context->hostileEnemiesEmpty() && m_context->m_enemies_to_kill_count <= 0)
    {
        if (m_context->isLastClassicStage())
        {
            // Campaign cleared: the "YOU WIN!" banner goes up first, the
            // game itself finishes right after it.
            m_context->startYouWin();
        }
        else
        {
            transiteTo(new Game::StageEndingState(m_context, false, false));
        }
    }

    if (m_context->m_players.empty())
    {
        transiteTo(new Game::GameOverState(m_context));
    }
}

void Game::PlayingState::eventProcess(const Event &event)
{
    if (event.type() == Event::KEYBOARD)
    {
        const KeyboardEvent &event_key = static_cast<const KeyboardEvent &>(event);
        if (event_key.isPressed(KEY_N) && !m_context->m_survival)
        {
            transiteTo(new Game::StageEndingState(m_context, true, false));
        }
        else if (event_key.isPressed(KEY_B) && !m_context->m_survival)
        {
            m_context->m_current_stage -= 2;
            transiteTo(new Game::StageEndingState(m_context, true, false));
        }
        else if (event_key.isPressed(KEY_T))
        {
            m_context->m_show_enemies_targets = !m_context->m_show_enemies_targets;
        }
        else if (event_key.isPressed(KEY_RETURN))
        {
            transiteTo(new Game::PauseState(m_context));
        }
#ifndef CONSOLE_PORT
        // On PS Vita/PSP ESCAPE is mapped to the Circle button, which is too easy
        // to press by accident; there the level can be left from the pause menu.
        else if (event_key.isPressed(KEY_ESCAPE))
        {
            m_context->transiteTo(new Menu(m_context->m_interactive_components, m_context->m_state_machine));
        }
#endif

        for (auto player : m_context->m_players)
            player->handleKeyboardEvent(event_key);
    }
}