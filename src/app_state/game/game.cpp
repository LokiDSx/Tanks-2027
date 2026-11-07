#include "game.h"
#include "../menu.h"
#include "../scores.h"
#include "../../engine/engine.h"
#include "../../engine/data/data.h"
#include "../../appconfig.h"
#include "../../spriteconfig.h"
#include "../../soundconfig.h"

#include <stdlib.h>
#include <ctime>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <climits>

Game::Game(int players_count, int starting_stage, InteractiveComponents interactive_components, StateMachine *state_machine, bool generated, bool survival, bool deathmatch, int deathmatch_difficulty, bool friend_enabled, bool modern_classic)
    : AppState(interactive_components, state_machine),
      m_game_state_machine(new StateMachine()),
      m_generated(generated),
      m_survival(survival),
      m_deathmatch(deathmatch),
      m_deathmatch_difficulty(deathmatch_difficulty),
      m_friend_enabled(friend_enabled),
      m_modern_classic(modern_classic),
      m_you_won(false),
      m_you_win_time(0),
      m_stage_finished(false),
      m_friend(nullptr),
      m_friend_respawn_time(0),
      m_pending_drone_player(nullptr)
{
    m_current_stage = starting_stage;
    m_players_count = players_count;
    m_enemy_respown_position = 0;
    m_hunter_spawn_counter = 0;
    m_stage_environment = new StageEnvironment(m_current_stage, interactive_components, m_generated, m_survival, m_deathmatch);

    m_enemies_to_kill_count = (m_survival || m_deathmatch) ? INT_MAX : (int)AppConfig::enemies_to_kill_total_count;
    m_kills = 0;
    m_new_enemy_cooldown = 0;
    m_show_enemies_targets = false;
}

Game::Game(std::vector<Player *> players, int previous_stage, InteractiveComponents interactive_components, StateMachine *state_machine, bool generated, bool survival, bool deathmatch, int deathmatch_difficulty, bool friend_enabled, bool modern_classic)
    : AppState(interactive_components, state_machine),
      m_game_state_machine(new StateMachine()),
      m_generated(generated),
      m_survival(survival),
      m_deathmatch(deathmatch),
      m_deathmatch_difficulty(deathmatch_difficulty),
      m_friend_enabled(friend_enabled),
      m_modern_classic(modern_classic),
      m_you_won(false),
      m_you_win_time(0),
      m_stage_finished(false),
      m_friend(nullptr),
      m_friend_respawn_time(0),
      m_pending_drone_player(nullptr)
{
    m_current_stage = previous_stage + 1;
    m_players = players;
    m_players_count = m_players.size();
    for (auto player : m_players)
    {
        player->moveToNextStage();
    }

    m_enemy_respown_position = 0;
    m_hunter_spawn_counter = 0;
    m_stage_environment = new StageEnvironment(m_current_stage, interactive_components, m_generated, m_survival, m_deathmatch);
    m_enemies_to_kill_count = (m_survival || m_deathmatch) ? INT_MAX : (int)AppConfig::enemies_to_kill_total_count;
    m_kills = 0;
    m_new_enemy_cooldown = 0;
    m_show_enemies_targets = false;
}

Game::~Game()
{
    clearAll();
}

void Game::onInitialize()
{
    createPlayersIfNeeded();
    createFriendlyIfNeeded();

    // Deathmatch: every map opens with a drone lying in the middle, so the
    // strike can be used (and tested) right from the start of the stage
    if (m_deathmatch)
        spawnBonusAtCenter(ST_BONUS_DRONE);
    // Survival: the ally bonus is dropped in the middle the same way
    if (m_survival)
        spawnBonusAtCenter(ST_BONUS_FRIEND);

    playSound(SoundConfig::STAGE_START_UP);

    m_game_state_machine->setState(new StartingState(this));
}

void Game::draw(Renderer &renderer)
{
    renderer.clear();

    m_game_state_machine->draw(renderer);

    renderer.flush();
}

void Game::update(const UpdateState &updateState)
{
    Uint32 dt = updateState.delta_time;

    if (dt > 40)
        return;

    m_game_state_machine->update(updateState);
}

void Game::eventProcess(const Event &event)
{
    m_game_state_machine->eventProcess(event);
}

void Game::clearAll()
{
    delete m_game_state_machine;

    for (auto enemy : m_enemies)
        delete enemy;
    m_enemies.clear();

    for (auto player : m_players)
        delete player;
    m_players.clear();

    for (auto bonus : m_bonuses)
        delete bonus;
    m_bonuses.clear();

    delete m_stage_environment;
    m_stage_environment = nullptr;
}

void Game::createPlayersIfNeeded()
{
    if (m_players.empty())
    {
        auto p1_starting_point = AppConfig::player_starting_point.at(0);
        auto p2_starting_point = AppConfig::player_starting_point.at(1);
        if (m_players_count == 2)
        {
            Player *p1 = new Player(p1_starting_point.x, p1_starting_point.y, ST_PLAYER_1, AppConfig::player_1_keys, m_interactive_components);
            Player *p2 = new Player(p2_starting_point.x, p2_starting_point.y, ST_PLAYER_2, AppConfig::player_2_keys, m_interactive_components);
            m_players.push_back(p1);
            m_players.push_back(p2);
        }
        else
        {
            Player *p1 = new Player(p1_starting_point.x, p1_starting_point.y, ST_PLAYER_1, AppConfig::player_1_keys, m_interactive_components);
            m_players.push_back(p1);
        }
    }
}

void Game::createFriendlyIfNeeded()
{
    // The friendly tank only spawns in Deathmatch, and only when the
    // "Friend bot" option was enabled on the setup screen
    if (!m_deathmatch || !m_friend_enabled)
        return;

    spawnFriendly();
}

void Game::spawnFriendly()
{
    // A single ally is allowed on the map at a time
    if (m_friend != nullptr)
        return;

    // The ally spawns on the second player's point; its stats are strong,
    // but below a fully upgraded player
    auto friendly_starting_point = AppConfig::player_starting_point.at(1);
    m_friend = new Enemy(friendly_starting_point.x, friendly_starting_point.y,
                         ST_PLAYER_2, 3, m_interactive_components, true);
    m_friend_respawn_time = 0;
    m_enemies.push_back(m_friend);
}

void Game::drawScene(Renderer &renderer)
{
    renderer.drawRect(AppConfig::map_rect, {0, 0, 0, 0}, true);
    drawObjects(renderer);
    drawGameStatusPanel(renderer);
}

void Game::drawObjects(Renderer &renderer)
{
    m_stage_environment->drawFirstLayer(renderer);

    for (auto player : m_players)
        player->draw(renderer);
    for (auto enemy : m_enemies)
        drawEnemy(renderer, enemy);

    for (auto bonus : m_bonuses)
        bonus->draw(renderer);

    m_stage_environment->drawSecondLayer(renderer);
}

void Game::drawEnemy(Renderer &renderer, Enemy *enemy)
{
    if (m_show_enemies_targets)
    {
        SpriteType type = enemy->type;
        Rect dest_rect = enemy->dest_rect;
        Point target_position = enemy->target_position;
        Color c;
        if (type == ST_TANK_A)
            c = {250, 0, 0, 250};
        if (type == ST_TANK_B)
            c = {0, 0, 250, 255};
        if (type == ST_TANK_C)
            c = {0, 255, 0, 250};
        if (type == ST_TANK_D)
            c = {250, 0, 255, 250};
        if (enemy->hunter())
            c = {255, 0, 0, 255}; // the hunter is marked red
        if (enemy->baseKiller())
            c = {80, 140, 255, 255}; // the base killer is marked blue
        if (enemy->friendly())
            c = {255, 255, 0, 255}; // the ally is marked yellow
        Rect r = {min(target_position.x, dest_rect.x + dest_rect.w / 2), dest_rect.y + dest_rect.h / 2, abs(target_position.x - (dest_rect.x + dest_rect.w / 2)), 1};
        renderer.drawRect(r, c, true);
        r = {target_position.x, min(target_position.y, dest_rect.y + dest_rect.h / 2), 1, abs(target_position.y - (dest_rect.y + dest_rect.h / 2))};
        renderer.drawRect(r, c, true);
    }

    enemy->draw(renderer);
}

void Game::drawGameStatusPanel(Renderer &renderer)
{
    // enemies left to kill (not shown in survival and deathmatch - there are
    // infinitely many; those modes and the endless "YOU WIN!" play show the
    // destroyed tanks counter instead)
    Rect enemy_left_src = SpriteConfig::getInstance().getSpriteData(ST_LEFT_ENEMY).rect;
    if (!m_survival && !m_deathmatch && !m_you_won)
    {
        for (int i = 0; i < m_enemies_to_kill_count; i++)
        {
            Rect enemy_status_dst = {
                AppConfig::status_rect.x + 8 + enemy_left_src.w * (i % 2),
                5 + enemy_left_src.h * (i / 2),
                enemy_left_src.w,
                enemy_left_src.h};
            renderer.drawObject(enemy_left_src, enemy_status_dst);
        }
    }
    else
    {
        // Survival/deathmatch: destroyed tanks counter instead of the
        // "enemies left" icons. The white variant is used here: the black
        // one blends into the dark status panel on the Vita.
        Rect kill_icon_src = SpriteConfig::getInstance().getSpriteData(ST_LEFT_ENEMY_WHITE).rect;
        Rect kill_icon_dst = {
            AppConfig::status_rect.x + (AppConfig::status_rect.w - kill_icon_src.w) / 2,
            5,
            kill_icon_src.w,
            kill_icon_src.h};
        renderer.drawObject(kill_icon_src, kill_icon_dst);

        const std::string kills_text = std::to_string(m_kills);
        Size kills_size = renderer.measureText(kills_text, FontSize::NORMAL);
        Point kills_text_dst = {
            AppConfig::status_rect.x + (AppConfig::status_rect.w - kills_size.w) / 2,
            5 + kill_icon_src.h + 4};
        renderer.drawText(kills_text_dst, kills_text, {0, 0, 0, 255}, FontSize::NORMAL);
    }
    // players' lives
    int player_y_offset = (AppConfig::enemies_to_kill_total_count / 2) * enemy_left_src.h + 20;
    int i = 0;
    for (auto player : m_players)
    {
        Rect player_status_dst = {AppConfig::status_rect.x + 5, player_y_offset + i * (AppConfig::tile_size.h + 2), AppConfig::tile_size.w, AppConfig::tile_size.h};
        Point player_lives_dst = {player_status_dst.x + player_status_dst.w + 2, player_status_dst.y + 3};
        i++;
        renderer.drawObject(player->src_rect, player_status_dst);
        renderer.drawText(player_lives_dst, std::to_string(player->lives()), {0, 0, 0, 255}, FontSize::NORMAL);
    }
    // current stage
    Rect stage_status_src = SpriteConfig::getInstance().getSpriteData(ST_STAGE_STATUS).rect;

    int stage_status_y_offset = player_y_offset + i * ((m_players.size() + m_killed_players.size()) * (AppConfig::tile_size.h + 2) + 10);
    Rect stage_status_dst = {AppConfig::status_rect.x + 8, stage_status_y_offset, stage_status_src.w, stage_status_src.h};
    Point stage_number_dst = {stage_status_dst.x + 10, stage_status_dst.y + 26};

    renderer.drawObject(stage_status_src, stage_status_dst);
    renderer.drawText(stage_number_dst, std::to_string(m_current_stage), {0, 0, 0, 255}, FontSize::NORMAL);
}

void Game::updateScene(Uint32 dt)
{
    checkCollisions(dt);
    updateObjects(dt);
    calculateEnemiesTargets();
    generateEnemyIfPossible(dt);

    // Deathmatch: the ally respawns like a player, a couple of seconds
    // after its death
    if (m_deathmatch && m_friend == nullptr)
    {
        m_friend_respawn_time += dt;
        if (m_friend_respawn_time >= AppConfig::Game::friend_respawn_time)
            createFriendlyIfNeeded();
    }

    // Drone bonus: grab it and the game freezes for the target pick
    if (m_pending_drone_player != nullptr)
        startDroneTargeting(m_pending_drone_player);
}

void Game::checkCollisions(Uint32 dt)
{
    std::vector<Player *>::iterator pl1, pl2;
    std::vector<Enemy *>::iterator en1, en2;

    for (pl1 = m_players.begin(); pl1 != m_players.end(); pl1++)
        for (pl2 = pl1 + 1; pl2 != m_players.end(); pl2++)
            checkCollisionTwoTanks(*pl1, *pl2, dt);

    for (en1 = m_enemies.begin(); en1 != m_enemies.end(); en1++)
        for (en2 = en1 + 1; en2 != m_enemies.end(); en2++)
            checkCollisionTwoTanks(*en1, *en2, dt);

    for (auto enemy : m_enemies)
    {
        m_stage_environment->checkCollisionTankWithStage(enemy, dt);
        for (auto bullet : enemy->bullets)
        {
            m_stage_environment->checkCollisionBulletWithStage(bullet);
            if (m_stage_environment->checkCollisionBulletWithEagle(bullet))
            {
                transiteToGameOver();
            }
            // The ally's shells can also destroy the enemy base (stage won)
            if (m_stage_environment->checkCollisionBulletWithEnemyBase(bullet))
            {
                transiteToStageComplete();
            }
        }
    }
    for (auto player : m_players)
    {
        m_stage_environment->checkCollisionTankWithStage(player, dt);
        for (auto bullet : player->bullets)
        {
            m_stage_environment->checkCollisionBulletWithStage(bullet);
            if (m_stage_environment->checkCollisionBulletWithEagle(bullet))
            {
                transiteToGameOver();
            }
            if (m_stage_environment->checkCollisionBulletWithEnemyBase(bullet))
            {
                transiteToStageComplete();
            }
        }
    }

    for (auto player : m_players)
        for (auto enemy : m_enemies)
        {
            checkCollisionTwoTanks(player, enemy, dt);

            checkCollisionPlayerBulletsWithEnemy(player, enemy);
            checkCollisionEnemyBulletsWithPlayer(enemy, player);

            // The ally's shells fly through the player's shells
            if (!enemy->friendly())
                for (auto bullet1 : player->bullets)
                    for (auto bullet2 : enemy->bullets)
                        checkCollisionTwoBullets(bullet1, bullet2);
        }

    // Deathmatch: enemy shells can hit the friendly tank
    if (m_friend != nullptr && !m_friend->to_erase)
    {
        for (auto enemy : m_enemies)
        {
            if (enemy == m_friend || enemy->to_erase)
                continue;
            if (!enemy->alive())
                continue; // already blowing up, do not count it twice

            for (auto bullet : enemy->bullets)
            {
                if (bullet->to_erase || bullet->isColide())
                    continue;
                Rect intersect_rect = bullet->collision_rect.intersection(m_friend->collision_rect);
                if (intersect_rect.isNotEmpty())
                {
                    bullet->destroy();
                    m_friend->hit();
                }
            }

            // The ally's shells cut down the enemies standing in its way
            for (auto bullet : m_friend->bullets)
            {
                if (bullet->to_erase || bullet->isColide())
                    continue;
                Rect intersect_rect = bullet->collision_rect.intersection(enemy->collision_rect);
                if (intersect_rect.isNotEmpty())
                {
                    if (enemy->bonus())
                        generateBonus();

                    bullet->destroy();
                    enemy->hit();
                    if (!enemy->alive())
                        countKill();
                }
            }
        }
    }

    for (auto player : m_players)
        for (auto bonus : m_bonuses)
            checkCollisionPlayerWithBonus(player, bonus);
}

void Game::checkCollisionTwoTanks(Tank *tank1, Tank *tank2, Uint32 dt)
{
    Rect cr1 = tank1->nextCollisionRect(dt);
    Rect cr2 = tank2->nextCollisionRect(dt);
    Rect intersect_rect = cr1.intersection(cr2);

    if (intersect_rect.isNotEmpty())
    {
        tank1->collide(intersect_rect);
        tank2->collide(intersect_rect);
    }
}

void Game::checkCollisionPlayerBulletsWithEnemy(Player *player, Enemy *enemy)
{
    if (player->to_erase || enemy->to_erase)
        return;
    // The ally is on our side: the player's shells never hit it
    if (enemy->friendly())
        return;
    Rect intersect_rect;

    for (auto bullet : player->bullets)
    {
        if (!bullet->to_erase && !bullet->isColide())
        {
            intersect_rect = bullet->collision_rect.intersection(enemy->collision_rect);
            if (intersect_rect.isNotEmpty())
            {
                if (enemy->bonus())
                    generateBonus();

                bullet->destroy();
                enemy->hit();
                player->addScore(enemy->scoreForHit());

                if (!enemy->alive())
                {
                    m_enemies_to_kill_count--;
                    countKill();
                }
            }
        }
    }
}

void Game::checkCollisionEnemyBulletsWithPlayer(Enemy *enemy, Player *player)
{
    if (enemy->to_erase || player->to_erase)
        return;
    // The ally's shells never hurt the player
    if (enemy->friendly())
        return;
    Rect intersect_rect;

    for (auto bullet : enemy->bullets)
    {
        if (!bullet->to_erase && !bullet->isColide())
        {
            intersect_rect = bullet->collision_rect.intersection(player->collision_rect);
            if (intersect_rect.isNotEmpty())
            {
                bullet->destroy();
                player->hit();
            }
        }
    }
}

void Game::checkCollisionTwoBullets(Bullet *bullet1, Bullet *bullet2)
{
    if (bullet1 == nullptr || bullet2 == nullptr)
        return;
    if (bullet1->to_erase || bullet2->to_erase)
        return;

    Rect intersect_rect = bullet1->collision_rect.intersection(bullet2->collision_rect);

    if (intersect_rect.isNotEmpty())
    {
        playSound(SoundConfig::BULLET_HIT_BULLET);
        bullet1->destroy();
        bullet2->destroy();
    }
}

void Game::checkCollisionPlayerWithBonus(Player *player, Bonus *bonus)
{
    if (player->to_erase || bonus->to_erase)
        return;

    Rect intersect_rect = player->collision_rect.intersection(bonus->collision_rect);
    if (intersect_rect.isNotEmpty())
    {
        player->addScore(300);

        if (bonus->type == ST_BONUS_GRENADE)
        {
            for (auto enemy : m_enemies)
            {
                // The ally is not an enemy: grenades pass it by
                if (enemy->friendly())
                    continue;
                if (!enemy->to_erase)
                {
                    player->addScore(200);
                    while (enemy->alive())
                    {
                        enemy->hit();
                    }
                    m_enemies_to_kill_count--;
                    countKill();
                }
            }
        }
        else if (bonus->type == ST_BONUS_HELMET)
        {
            player->activateShieldEffect();
        }
        else if (bonus->type == ST_BONUS_CLOCK)
        {
            for (auto enemy : m_enemies)
                if (!enemy->to_erase && !enemy->friendly())
                    enemy->activateFrozenEffect();
        }
        else if (bonus->type == ST_BONUS_SHOVEL)
        {
            m_stage_environment->startEagleProtection();
        }
        else if (bonus->type == ST_BONUS_TANK)
        {
            player->addLife();
        }
        else if (bonus->type == ST_BONUS_STAR)
        {
            player->changeStarCountBy(1);
        }
        else if (bonus->type == ST_BONUS_GUN)
        {
            player->changeStarCountBy(3);
        }
        else if (bonus->type == ST_BONUS_BOAT)
        {
            player->activateBoatEffect();
        }
        else if (bonus->type == ST_BONUS_DRONE)
        {
            // The drone does not fire right away: the game freezes and the
            // player picks the tank to destroy (see DroneTargetState).
            m_pending_drone_player = player;
        }
        else if (bonus->type == ST_BONUS_FRIEND)
        {
            // Generated/survival: the ally joins the fight. It lives until
            // its first death and does not come back on its own afterwards,
            // so a second friend bonus can bring a fresh one.
            spawnFriendly();
        }
        bonus->to_erase = true;

        if (bonus->type == ST_BONUS_TANK)
        {
            playSound(SoundConfig::PLAYER_LIFE_UP);
        }
        else
        {
            playSound(SoundConfig::BONUS_OBTAINED);
        }
    }
}

void Game::updateObjects(Uint32 dt)
{
    for (auto enemy : m_enemies)
        enemy->update(dt);
    for (auto player : m_players)
        player->update(dt);
    for (auto bonus : m_bonuses)
        bonus->update(dt);

    bool player_muted = false;
    bool player_tried_to_move = false;
    for (auto player : m_players)
    {
        MovingState ms = player->movingState();
        if (ms == MovingState::CREATING)
        {
            player_muted = true;
        }
        if (ms == MovingState::TRYING_TO_MOVE)
        {
            player_tried_to_move = true;
        }
    }

    if (player_muted)
    {
        stopSound(SoundConfig::PLAYER_IDLE);
        stopSound(SoundConfig::PLAYER_MOVING);
    }
    else
    {
        if (player_tried_to_move)
        {
            stopSound(SoundConfig::PLAYER_IDLE);
            playSound(SoundConfig::PLAYER_MOVING);
        }
        else
        {
            stopSound(SoundConfig::PLAYER_MOVING);
            playSound(SoundConfig::PLAYER_IDLE);
        }
    }

    m_stage_environment->update(dt);

    m_enemies.erase(std::remove_if(m_enemies.begin(), m_enemies.end(), [this](Enemy *e)
                                   {if(e->to_erase) {if (e == m_friend) {m_friend = nullptr; m_friend_respawn_time = 0;} delete e; return true;} return false; }),
                    m_enemies.end());
    m_players.erase(std::remove_if(m_players.begin(), m_players.end(), [this](Player *p)
                                   {if(p->to_erase) {m_killed_players.push_back(p); return true;} return false; }),
                    m_players.end());
    m_bonuses.erase(std::remove_if(m_bonuses.begin(), m_bonuses.end(), [](Bonus *b)
                                   {if(b->to_erase) {delete b; return true;} return false; }),
                    m_bonuses.end());
}

void Game::countKill()
{
    // The kill counter drives the survival/deathmatch waves and the status
    // panel of the endless "YOU WIN!" play; it is tracked in every mode.
    m_kills++;
    // Survival/deathmatch waves: every 5 kills the enemies get tougher.
    if ((m_survival || m_deathmatch) && m_kills % 5 == 0 && m_current_stage < (int)AppConfig::stages_count)
        m_current_stage++;
}

void Game::calculateEnemiesTargets()
{
    int min_metric;
    Point target;

    const bool has_eagle = m_stage_environment->hasEagle();
    Point default_target = m_players.empty()
                               ? Point{AppConfig::map_rect.w / 2, AppConfig::map_rect.h / 2}
                               : m_players[0]->center();

    for (auto enemy : m_enemies)
    {
        Point enemy_center = enemy->center();
        min_metric = AppConfig::map_rect.w + AppConfig::map_rect.h;
        target = default_target;

        if (enemy->friendly())
        {
            // Deathmatch: the ally hunts enemy tanks; when the map is
            // momentarily clear it falls back to the enemy base
            bool target_found = false;
            for (auto other : m_enemies)
            {
                if (other == enemy || other->friendly() || other->to_erase || !other->alive())
                    continue;
                Point other_center = other->center();
                int metric = fabs(other_center.x - enemy_center.x) + fabs(other_center.y - enemy_center.y);
                if (metric < min_metric)
                {
                    min_metric = metric;
                    target = other_center;
                    target_found = true;
                }
            }
            if (!target_found && m_stage_environment->hasEnemyBase())
                target = m_stage_environment->getEnemyBaseCenter();

            enemy->target_position = target;
            continue;
        }

        if (enemy->hunter())
        {
            // The hunter's one and only goal is the player: it locks onto
            // the nearest one and ignores the eagle and the base entirely
            for (auto player : m_players)
            {
                if (player->to_erase)
                    continue;
                Point player_center = player->center();
                int metric = fabs(player_center.x - enemy_center.x) + fabs(player_center.y - enemy_center.y);
                if (metric < min_metric)
                {
                    min_metric = metric;
                    target = player_center;
                }
            }

            enemy->target_position = target;
            continue;
        }

        if (enemy->baseKiller())
        {
            // The base killer ignores the fight and makes straight for
            // the player's eagle; when it is already gone it joins the
            // regular pack
            if (has_eagle)
                target = m_stage_environment->getEagleCenter();

            enemy->target_position = target;
            continue;
        }

        if (enemy->type == ST_TANK_A || enemy->type == ST_TANK_D)
        {
            for (auto player : m_players)
            {
                Point player_center = player->center();
                int metric = fabs(player_center.x - enemy_center.x) + fabs(player_center.y - enemy_center.y);
                if (metric < min_metric)
                {
                    min_metric = metric;
                    target = player_center;
                }
            }
        }

        if (has_eagle)
        {
            Point eagle_center = m_stage_environment->getEagleCenter();
            int metric = fabs(eagle_center.x - enemy_center.x) + fabs(eagle_center.y - enemy_center.y);
            if (metric < min_metric)
            {
                min_metric = metric;
                target = eagle_center;
            }
        }

        enemy->target_position = target;
    }
}

void Game::generateEnemyIfPossible(Uint32 dt)
{
    // The stage is already over — no reinforcements are sent
    if (m_stage_finished)
        return;

    m_new_enemy_cooldown += dt;
    long unsigned allowed_enemies_count = AppConfig::enemies_max_count_on_map < m_enemies_to_kill_count ? AppConfig::enemies_max_count_on_map : m_enemies_to_kill_count;

    // The ally does not take a spawn slot of the real enemies
    long unsigned active_enemies_count = 0;
    for (auto enemy : m_enemies)
        if (!enemy->friendly())
            active_enemies_count++;

    if (active_enemies_count >= allowed_enemies_count || m_new_enemy_cooldown < AppConfig::new_enemy_cooldown)
        return;

    m_new_enemy_cooldown = 0;

    SpriteType type;
    unsigned armor_count;
    // Deathmatch role of the spawned tank: 0 - regular, 1 - hunter
    // (stalks the player), 2 - base killer (storms the eagle)
    int dm_role = 0;

    if (m_deathmatch)
    {
        switch (m_deathmatch_difficulty)
        {
        case DM_EASY:
            // Easy: weak tanks only, no special AI roles on the map
            type = (rand() % 2 == 0) ? ST_TANK_A : ST_TANK_B;
            armor_count = 1;
            break;
        case DM_MEDIUM:
            // Medium: mid-tier tanks; every third enemy is a hunter
            type = (rand() % 2 == 0) ? ST_TANK_B : ST_TANK_C;
            armor_count = 2 + rand() % 2;
            m_hunter_spawn_counter++;
            if (m_hunter_spawn_counter >= 3)
            {
                m_hunter_spawn_counter = 0;
                dm_role = 1;
            }
            break;
        default: // DM_HARD
            // Hard: the spawns rotate a max-stats tank, a hunter and a
            // base killer (the weakest of the three)
            m_hunter_spawn_counter = (m_hunter_spawn_counter + 1) % 3;
            if (m_hunter_spawn_counter == 1)
            {
                type = ST_TANK_D;
                armor_count = 4;
            }
            else if (m_hunter_spawn_counter == 2)
            {
                type = ST_TANK_D;
                armor_count = 4;
                dm_role = 1;
            }
            else
            {
                // The base killer trades stats for its one-way ticket
                // to the player's eagle
                type = ST_TANK_A;
                armor_count = 1;
                dm_role = 2;
            }
            break;
        }
    }
    else
    {
        float p = static_cast<float>(rand()) / RAND_MAX;
        // magic numbers to calculate enemy type probabilities
        type = static_cast<SpriteType>(p < (0.00735 * m_current_stage + 0.09265) ? ST_TANK_D : (rand() % (ST_TANK_C - ST_TANK_A + 1) + ST_TANK_A));

        // magic numbers to calculate enemy armor probabilities
        double a, b, c;
        if (m_current_stage <= 17)
        {
            a = -0.040625 * m_current_stage + 0.940625;
            b = -0.028125 * m_current_stage + 0.978125;
            c = -0.014375 * m_current_stage + 0.994375;
        }
        else
        {
            a = -0.012778 * m_current_stage + 0.467222;
            b = -0.025000 * m_current_stage + 0.925000;
            c = -0.036111 * m_current_stage + 1.363889;
        }

        p = static_cast<float>(rand()) / RAND_MAX;
        if (p < a)
            armor_count = 1;
        else if (p < b)
            armor_count = 2;
        else if (p < c)
            armor_count = 3;
        else
            armor_count = 4;
    }

    auto starting_points = m_deathmatch ? AppConfig::enemy_starting_point_deathmatch : AppConfig::enemy_starting_point;
    auto starting_point = starting_points.at(m_enemy_respown_position);
    Enemy *e = new Enemy(starting_point.x, starting_point.y, type, armor_count, m_interactive_components);
    if (m_deathmatch)
    {
        if (m_deathmatch_difficulty != DM_EASY)
        {
            // Medium and Hard enemies move at the top speed of their class
            e->makeMaxStats();
        }

        if (dm_role == 1)
        {
            // The hunter: the ally's discipline, but it stalks the player
            e->makeHunter();
        }
        else if (dm_role == 2)
        {
            // The base killer: makes straight for the player's eagle
            e->makeBaseKiller();
        }
    }
    else if (m_modern_classic)
    {
        // Modern Classic keeps exactly one hunter and one base killer on
        // the field: a killed specialist takes the next free spawn, the
        // rest of the wave spawns as usual.
        bool has_hunter = false;
        bool has_base_killer = false;
        for (auto other : m_enemies)
        {
            if (other->friendly() || other->to_erase)
                continue;
            if (other->hunter())
                has_hunter = true;
            if (other->baseKiller())
                has_base_killer = true;
        }
        if (!has_hunter)
            e->makeHunter();
        else if (!has_base_killer)
            e->makeBaseKiller();
    }
    m_enemy_respown_position++;
    if (m_enemy_respown_position >= starting_points.size())
        m_enemy_respown_position = 0;

    float p = static_cast<float>(rand()) / RAND_MAX;
    // Easy deathmatch drops bonus tanks far more often
    const float bonus_tank_chance = (m_deathmatch && m_deathmatch_difficulty == DM_EASY) ? 0.35f : 0.12f;
    if (p < bonus_tank_chance)
        e->makeWithBonus();

    m_enemies.push_back(e);
}

void Game::generateBonus()
{
    // Bonus pools per mode. The new bonuses were appended after ST_BONUS_BOAT
    // in the enum, so the range has to end on the last one available here:
    //   classic .............. original set, up to the boat
    //   modern classic ....... + drone strike and the ally bonus (all the
    //                          new ones - the ally itself only ever comes
    //                          from a bonus in this mode)
    //   generated ............ + drone strike and the ally bonus
    //   survival ............. like generated, but without the boat bonus
    //   deathmatch ........... + drone strike only (the ally is already a
    //                          permanent resident there)
    SpriteType bonus_type;
    if (m_survival)
    {
        // Survival drops every bonus except the boat (water is pointless in
        // an endless run with no base to defend). DRONE and FRIEND still drop.
        const int pool_size = (ST_BONUS_FRIEND - ST_BONUS_GRENADE + 1) - 1;
        int idx = rand() % pool_size;
        if (idx >= (ST_BONUS_BOAT - ST_BONUS_GRENADE))
            idx += 1; // skip the boat
        bonus_type = static_cast<SpriteType>(ST_BONUS_GRENADE + idx);
    }
    else
    {
        SpriteType last_bonus;
        if (m_deathmatch)
            last_bonus = ST_BONUS_DRONE;
        else if (m_generated || m_modern_classic)
            last_bonus = ST_BONUS_FRIEND;
        else
            last_bonus = ST_BONUS_BOAT;
        bonus_type = static_cast<SpriteType>(rand() % (last_bonus - ST_BONUS_GRENADE + 1) + ST_BONUS_GRENADE);
    }

    Bonus *b = new Bonus(0, 0, bonus_type);

    if ((m_deathmatch && b->type == ST_BONUS_DRONE) ||
        (m_survival && b->type == ST_BONUS_FRIEND))
    {
        // Deathmatch drones and survival ally bonuses are dropped over the
        // middle of the map. Every other bonus spawns the usual way.
        placeBonusInMapCenter(b);
    }
    else
    {
        do
        {
            b->pos_x = rand() % (AppConfig::map_rect.x + AppConfig::map_rect.w - 2 * AppConfig::tile_size.w);
            b->pos_y = rand() % (AppConfig::map_rect.y + AppConfig::map_rect.h - 2 * AppConfig::tile_size.h);
            b->update(0);
        } while (m_stage_environment->checkCollisionWithEagle(b->collision_rect));
    }

    playSound(SoundConfig::BONUS_APPEARED);

    m_bonuses.push_back(b);
}

void Game::placeBonusInMapCenter(Bonus *b)
{
    // The middle of the map, within reach of both sides. It may land right
    // on a brick or a stone block on purpose: the strike is an airstrike,
    // it does not care what is underneath, and the open middle makes it
    // easy to grab during a firefight.
    const int margin = AppConfig::map_rect.w / 4;
    const int span = AppConfig::map_rect.w - 2 * margin;

    b->pos_x = margin + rand() % span;
    b->pos_y = margin + rand() % span;
    b->update(0);
}

void Game::spawnBonusAtCenter(SpriteType type)
{
    Bonus *b = new Bonus(0, 0, type);
    placeBonusInMapCenter(b);

    playSound(SoundConfig::BONUS_APPEARED);
    m_bonuses.push_back(b);
}

void Game::transiteToGameOver()
{
    m_game_state_machine->setState(new Game::GameOverState(this));
}

void Game::transiteToStageComplete()
{
    // Destroying the enemy base wipes every enemy tank off the map:
    // they all blow up at once during the stage ending (the ally is spared,
    // tanks already blowing up are not counted twice)
    for (auto enemy : m_enemies)
    {
        if (enemy->friendly() || enemy->to_erase)
            continue;
        if (!enemy->alive())
            continue;

        while (enemy->alive())
            enemy->hit();
        countKill();
    }

    m_stage_finished = true;
    m_game_state_machine->setState(new Game::StageEndingState(this, false, false));
}

void Game::startDroneTargeting(Player *player)
{
    m_pending_drone_player = nullptr;
    m_game_state_machine->setState(new Game::DroneTargetState(this));
    static_cast<Game::DroneTargetState *>(m_game_state_machine->current_state)->setOwner(player);
}

bool Game::isLastClassicStage() const
{
    // Both classic modes share the 35 stage files: the campaign is won on
    // the stage right before the files would wrap around to stage 1.
    return !m_generated && !m_survival && !m_deathmatch &&
           (m_current_stage % (int)AppConfig::stages_count) == 0;
}

void Game::startYouWin()
{
    // The banner goes up now; PlayingState finishes the game once it has
    // been shown (scores screen, then back to the menu).
    m_you_won = true;
    m_you_win_time = 0;

    stopSound(SoundConfig::PLAYER_IDLE);
    stopSound(SoundConfig::PLAYER_MOVING);
    playSound(SoundConfig::HIGHSCORE_BEATEN);
}

void Game::drawYouWinMessage(Renderer &renderer)
{
    if (!m_you_won || m_you_win_time > AppConfig::Game::you_win_show_time)
        return;

    // Same slide-up as the GAME OVER text, then the banner holds for the
    // rest of the show time while the endless wave keeps coming.
    double y = AppConfig::map_rect.h - m_you_win_time * AppConfig::Game::game_over_message_speed;
    if (y < 10)
        y = 10;
    renderer.drawText({-1, static_cast<int>(y)}, AppConfig::you_win_text, {255, 220, 0, 255}, FontSize::BIGGEST);
}

bool Game::hostileEnemiesEmpty() const
{
    // The ally lives in m_enemies too, but it never ends a stage: a modern
    // classic run with a friend bonus out must still detect the victory.
    for (auto enemy : m_enemies)
        if (!enemy->friendly())
            return false;
    return true;
}