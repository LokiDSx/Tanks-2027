#include "game.h"
#include "../../appconfig.h"
#include "../../soundconfig.h"
#include "../menu.h"

#include <algorithm>
#include <cmath>

Game::DroneTargetState::DroneTargetState(Game *ps)
    : ContextState<Game>(ps, ps->m_game_state_machine),
      m_owner(nullptr),
      m_cursor({AppConfig::map_rect.w / 2, AppConfig::map_rect.h / 2})
{
}

void Game::DroneTargetState::onInitialize()
{
    // State::initialize() runs before every processing round, so the actual
    // one-time setup lives in setOwner(); this is just a safety net in case
    // the state gets processed without an owner (there is nobody to strike
    // for, so the state bows out right away)
    if (m_owner == nullptr && m_context->m_pending_drone_player == nullptr)
    {
        m_owner = nullptr;
        for (auto p : m_context->m_players)
            if (!p->to_erase)
            {
                m_owner = p;
                break;
            }

        if (m_owner == nullptr)
            finish();
    }
}

void Game::DroneTargetState::setOwner(Player *player)
{
    if (player == nullptr || player->to_erase)
    {
        // The drone operator is already gone: any other living player takes
        // the remote, and with nobody left there is no strike at all
        for (auto p : m_context->m_players)
            if (!p->to_erase)
            {
                player = p;
                break;
            }
    }

    if (player == nullptr)
        return;

    m_owner = player;
    m_context->m_pending_drone_player = nullptr;

    // The drone is called in (the explosion sound comes from Enemy::hit())
    m_context->stopAllSounds();
    m_context->playSound(SoundConfig::BONUS_OBTAINED);

    // The crosshair starts on the nearest live enemy: more often than not
    // that is the tank the player wants gone, so one X press is enough
    Enemy *nearest = nullptr;
    int best_metric = 0;

    Point operator_center = m_owner->center();
    for (auto enemy : m_context->m_enemies)
    {
        if (enemy->to_erase || !enemy->alive())
            continue;

        Point c = enemy->center();
        int metric = std::abs(c.x - operator_center.x) + std::abs(c.y - operator_center.y);
        if (nearest == nullptr || metric < best_metric)
        {
            nearest = enemy;
            best_metric = metric;
        }
    }

    if (nearest != nullptr)
        m_cursor = nearest->center();
}

Tank *Game::DroneTargetState::tankUnderCursor() const
{
    Tank *best = nullptr;
    int best_metric = 0;

    for (auto enemy : m_context->m_enemies)
    {
        if (enemy->to_erase || !enemy->alive())
            continue;

        Rect r = enemy->dest_rect;

        if (m_cursor.x < r.x || m_cursor.x > r.x + r.w)
            continue;
        if (m_cursor.y < r.y || m_cursor.y > r.y + r.h)
            continue;

        // Among overlapping tanks the closest one to the crosshair wins
        int metric = std::abs(r.x + r.w / 2 - m_cursor.x) + std::abs(r.y + r.h / 2 - m_cursor.y);
        if (best == nullptr || metric < best_metric)
        {
            best = enemy;
            best_metric = metric;
        }
    }

    return best;
}

void Game::DroneTargetState::destroyTarget()
{
    Enemy *enemy = dynamic_cast<Enemy *>(tankUnderCursor());

    // The ally is on our side: the drone never strikes it
    if (enemy == nullptr || enemy->friendly())
        return;

    // The bonus tank drops its payload where it stands
    if (enemy->bonus())
        m_context->generateBonus();

    if (m_owner != nullptr)
        m_owner->addScore(enemy->scoreForHit());

    while (enemy->alive())
        enemy->hit();

    // The drone lands on the picked tank; the explosion sound comes from
    // Enemy::hit() itself
    m_context->stopAllSounds();

    m_context->m_enemies_to_kill_count--;
    m_context->countKill();
}

void Game::DroneTargetState::finish()
{
    // The remote goes back to the keyboard: the operator must not keep
    // moving after the strike, so all of its keys are released
    if (m_owner != nullptr)
    {
        for (auto key : {KeyCode::KEY_UP, KeyCode::KEY_DOWN, KeyCode::KEY_LEFT, KeyCode::KEY_RIGHT, P1_FIRE_KEY})
            m_owner->handleKeyboardEvent(KeyboardEvent(key, KeyboardEvent::RELEASED, 0));
    }

    m_owner = nullptr;
    m_context->m_pending_drone_player = nullptr;
    m_context->stopAllSounds();
    transiteTo(new Game::PlayingState(m_context));
}

void Game::DroneTargetState::draw(Renderer &renderer)
{
    m_context->drawScene(renderer);

    Tank *target = tankUnderCursor();

    // The frame around the tank that is about to be struck
    if (target != nullptr)
    {
        Rect r = target->dest_rect;
        Color frame_color = {255, 60, 60, 255};
        const int t = 2;

        renderer.drawRect({r.x - t, r.y - t, r.w + 2 * t, t}, frame_color, true);
        renderer.drawRect({r.x - t, r.y + r.h, r.w + 2 * t, t}, frame_color, true);
        renderer.drawRect({r.x - t, r.y, t, r.h}, frame_color, true);
        renderer.drawRect({r.x + r.w, r.y, t, r.h}, frame_color, true);
    }

    // The crosshair itself
    {
        Color cursor_color = {255, 255, 255, 255};
        const int half = 5;
        const int len = 7;

        renderer.drawRect({m_cursor.x - half, m_cursor.y, len, 1}, cursor_color, true);
        renderer.drawRect({m_cursor.x - half, m_cursor.y - 1, len, 1}, cursor_color, true);
        renderer.drawRect({m_cursor.x, m_cursor.y - half, 1, len}, cursor_color, true);
        renderer.drawRect({m_cursor.x - 1, m_cursor.y - half, 1, len}, cursor_color, true);
    }

    // The banner at the top of the screen; a dark plate behind the letters
    // keeps it readable over any terrain
    {
        const std::string line1 = "SELECT A TANK";
        Size s1 = renderer.measureText(line1, FontSize::NORMAL);
        Point p1 = {(AppConfig::windows_rect.w - s1.w) / 2, 6};
        renderer.drawRect({p1.x - 6, p1.y - 3, s1.w + 12, s1.h + 6}, {0, 0, 0, 190}, true);
        renderer.drawText(p1, line1, {255, 255, 255, 255}, FontSize::NORMAL);

        const std::string line2 = "X - DESTROY   O - CANCEL";
        Size s2 = renderer.measureText(line2, FontSize::NORMAL);
        Point p2 = {(AppConfig::windows_rect.w - s2.w) / 2, p1.y + s1.h + 8};
        renderer.drawRect({p2.x - 6, p2.y - 3, s2.w + 12, s2.h + 6}, {0, 0, 0, 190}, true);
        renderer.drawText(p2, line2, {255, 255, 255, 255}, FontSize::NORMAL);
    }
}

void Game::DroneTargetState::update(const UpdateState &updateState)
{
    // The battlefield is frozen while the target is picked; only the
    // crosshair is live, and it is driven by the keyboard events below
}

void Game::DroneTargetState::eventProcess(const Event &event)
{
    if (event.type() != Event::KEYBOARD)
        return;

    const KeyboardEvent &ev = static_cast<const KeyboardEvent &>(event);
    if (ev.keyState() != KeyboardEvent::PRESSED)
        return;

    // Cross / Square / R all act as "fire" in the pad mapping
    if (ev.isPressed(KeyCode::KEY_SPACE) || ev.isPressed(KeyCode::KEY_RCTRL))
    {
        if (tankUnderCursor() != nullptr)
        {
            destroyTarget();
            finish();
        }
        return;
    }

    // Circle cancels the strike: the bonus is spent, nothing happens
    if (ev.isPressed(KeyCode::KEY_ESCAPE))
    {
        finish();
        return;
    }

    // The pad directions (with the auto-repeat built into the pad layer)
    // nudge the crosshair; the tank closest in the pressed direction
    // becomes the new pick
    Direction dir = D_UP;
    if (ev.isPressed(KeyCode::KEY_DOWN))
        dir = D_DOWN;
    else if (ev.isPressed(KeyCode::KEY_LEFT))
        dir = D_LEFT;
    else if (ev.isPressed(KeyCode::KEY_RIGHT))
        dir = D_RIGHT;
    else if (!ev.isPressed(KeyCode::KEY_UP))
        return;

    Enemy *nearest = nullptr;
    int best_metric = 0;

    for (auto enemy : m_context->m_enemies)
    {
        if (enemy->to_erase || !enemy->alive())
            continue;

        Point c = enemy->center();
        int dx = c.x - m_cursor.x;
        int dy = c.y - m_cursor.y;

        // Only tanks beyond the crosshair in the pressed direction qualify
        bool in_front;
        switch (dir)
        {
        case D_UP:
            in_front = dy < -4;
            break;
        case D_DOWN:
            in_front = dy > 4;
            break;
        case D_LEFT:
            in_front = dx < -4;
            break;
        default:
            in_front = dx > 4;
            break;
        }
        if (!in_front)
            continue;

        // Mostly across the pressed axis, a bit along the other one
        int across = (dir == D_UP || dir == D_DOWN) ? std::abs(dx) : std::abs(dy);
        int along = (dir == D_UP || dir == D_DOWN) ? std::abs(dy) : std::abs(dx);
        int metric = across + along * 2;

        if (nearest == nullptr || metric < best_metric)
        {
            nearest = enemy;
            best_metric = metric;
        }
    }

    if (nearest != nullptr)
        m_cursor = nearest->center();
}
