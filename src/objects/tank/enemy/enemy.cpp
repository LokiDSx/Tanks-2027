#include "enemy.h"
#include "../../../appconfig.h"
#include "../../../soundconfig.h"
#include <stdlib.h>
#include <ctime>
#include <iostream>

Enemy::Enemy(double x, double y, SpriteType type, unsigned armor_count, InteractiveComponents interactive_components, bool friendly)
    : Tank(x, y, friendly ? ST_PLAYER_2 : type, interactive_components),
      m_enemy_state_machine(new StateMachine()),
      m_friendly(friendly)
{
    // The ally heads straight for the enemy base at the top of the map
    m_moving_direction = friendly ? D_UP : D_DOWN;
    m_tank_direction = m_moving_direction;
    m_direction_time = 0;
    m_keep_direction_time = 100;

    m_bonus = false;
    m_hunter = false;
    m_base_killer = false;
    m_speed_time = 0;
    m_try_to_go_time = 100;

    m_fire_time = 0;
    m_reload_time = 100;

    m_lives_count = 1;
    m_armor_count = armor_count;

    m_bullet_max_count = 1;

    if (friendly)
    {
        // Deathmatch ally: a strong fighter, but below a fully upgraded
        // player (speed, magazine size and fire rate are toned down)
        m_max_speed = AppConfig::tank_default_speed * 1.15;
        m_bullet_max_count = 2;
        m_reload_time = 200;
    }
    else if (type == ST_TANK_B)
        m_max_speed = AppConfig::tank_default_speed * 1.3;
    else
        m_max_speed = AppConfig::tank_default_speed;

    target_position = {-1, -1};

    m_enemy_state_machine->setState(new CreatingState(this));
}

Enemy::~Enemy()
{
    delete m_enemy_state_machine;
}

void Enemy::draw(Renderer &renderer)
{
    m_enemy_state_machine->draw(renderer);
}

void Enemy::drawEnemy(Renderer &renderer)
{
    Tank::draw(renderer);
}

void Enemy::update(Uint32 dt)
{
    m_enemy_state_machine->update({dt});
}

Bullet *Enemy::fire()
{
    Bullet *b = Tank::fire();

    if (b != nullptr && m_friendly)
    {
        // The ally's shells punch through bricks, but a stone wall
        // (the metal bottom of the enemy base) stops them
        b->increaseDamage();
        playSound(SoundConfig::PLAYER_FIRED);
    }
    else if (b != nullptr && type == ST_TANK_C)
    {
        b->increaseSpeed(1.3);
    }

    return b;
}

void Enemy::hit()
{
    if (m_armor_count == 1)
    {
        m_armor_count = 0;
        m_enemy_state_machine->setState(new DestroyedState(this));
        playSound(m_friendly ? SoundConfig::PLAYER_DESTROYED : SoundConfig::ENEMY_DESTROYED);
    }
    else if (m_armor_count > 1)
    {
        m_armor_count--;
        playSound(SoundConfig::ENEMY_HIT);
    }
}

unsigned Enemy::scoreForHit()
{
    if (m_lives_count > 0)
        return 50;
    return 100;
}

void Enemy::makeWithBonus()
{
    m_bonus = true;
}

bool Enemy::bonus() const
{
    return m_bonus;
}

void Enemy::makeMaxStats()
{
    m_max_speed = AppConfig::tank_default_speed * 1.3;
}

bool Enemy::friendly() const
{
    return m_friendly;
}

void Enemy::makeHunter()
{
    m_hunter = true;
    // Red tank: the hunter is out for the player
    m_tint = {255, 80, 80, 255};
}

bool Enemy::hunter() const
{
    return m_hunter;
}

void Enemy::makeBaseKiller()
{
    m_base_killer = true;
    // Blue tank: the base killer is out for the player's eagle
    m_tint = {80, 140, 255, 255};
}

bool Enemy::baseKiller() const
{
    return m_base_killer;
}

void Enemy::updateBehavior(Uint32 dt)
{
    if (m_frozen)
        return;

    m_direction_time += dt;
    m_speed_time += dt;
    m_fire_time += dt;
    if (m_direction_time > m_keep_direction_time)
    {
        m_direction_time = 0;
        m_keep_direction_time = rand() % 800 + 100;

        float p = static_cast<float>(rand()) / RAND_MAX;

        // The ally, the deathmatch hunter and the base killer almost never
        // wander off course: one guards the base, the other two pursue
        // their designated target
        const bool on_mission = m_friendly || m_hunter || m_base_killer;
        const float seek_probability = on_mission ? 0.9f : (type == ST_TANK_A ? 0.8f : 0.5f);
        if (p < seek_probability && target_position.x > 0 && target_position.y > 0)
        {
            int dx = target_position.x - (dest_rect.x + dest_rect.w / 2);
            int dy = target_position.y - (dest_rect.y + dest_rect.h / 2);

            p = static_cast<float>(rand()) / RAND_MAX;

            if (abs(dx) > abs(dy))
                setDirection(p < 0.7 ? (dx < 0 ? D_LEFT : D_RIGHT) : (dy < 0 ? D_UP : D_DOWN));
            else
                setDirection(p < 0.7 ? (dy < 0 ? D_UP : D_DOWN) : (dx < 0 ? D_LEFT : D_RIGHT));
        }
        else
            setDirection(static_cast<Direction>(rand() % 4));
    }
    if (m_speed_time > m_try_to_go_time)
    {
        m_speed_time = 0;
        m_try_to_go_time = rand() % 300;
        m_speed = m_max_speed;
    }
    if (m_fire_time > m_reload_time)
    {
        m_fire_time = 0;
        if (type == ST_TANK_D || m_friendly || m_hunter || m_base_killer)
        {
            m_reload_time = (m_friendly || m_hunter || m_base_killer) ? (rand() % 350) : (rand() % 400);
            int dx = target_position.x - (dest_rect.x + dest_rect.w / 2);
            int dy = target_position.y - (dest_rect.y + dest_rect.h / 2);

            if (m_blocked)
                fire();
            else
                switch (m_moving_direction)
                {
                case D_UP:
                    if (dy < 0 && abs(dx) < dest_rect.w)
                        fire();
                    break;
                case D_RIGHT:
                    if (dx > 0 && abs(dy) < dest_rect.h)
                        fire();
                    break;
                case D_DOWN:
                    if (dy > 0 && abs(dx) < dest_rect.w)
                        fire();
                    break;
                case D_LEFT:
                    if (dx < 0 && abs(dy) < dest_rect.h)
                        fire();
                    break;
                }
        }
        else if (type == ST_TANK_C)
        {
            m_reload_time = rand() % 800;
            fire();
        }
        else
        {
            m_reload_time = rand() % 1000;
            fire();
        }
    }
}