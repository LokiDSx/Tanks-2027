#include "stage_environment.h"
#include "../../appconfig.h"
#include "../../soundconfig.h"
#include "../../stage_generator.h"
#include "../../objects/brick.h"
#include "../../objects/tank/player/player.h"
#include "../../objects/tank/enemy/enemy.h"

#include <sstream>
#include <stdexcept>
#include <SDL2/SDL.h>

StageEnvironment::StageEnvironment(int current_stage, InteractiveComponents interactive_components, bool generated, bool survival, bool deathmatch)
    : InteractiveComponentsHolder(interactive_components),
      m_stage_number(current_stage),
      m_generated(generated),
      m_survival(survival),
      m_deathmatch(deathmatch)
{

    m_loaded_stage = (current_stage - 1) % AppConfig::stages_count + 1;
    m_eagle_protected = false;
    m_eagle_protection_time = 0;
    m_eagle_tile_rect = {AppConfig::map_size.w / 2 - 1, AppConfig::map_size.h - 2, 2, 2};

    // Deathmatch enemy base: a mirror of our eagle at the very top edge
    m_enemy_eagle_tile_rect = {AppConfig::map_size.w / 2 - 1, 0, 2, 2};
    m_enemy_eagle = nullptr;

    m_map_outside_colision_left_rect = {-AppConfig::tile_size.w, -AppConfig::tile_size.h, AppConfig::tile_size.w, AppConfig::map_rect.h + 2 * AppConfig::tile_size.h};
    m_map_outside_colision_right_rect = {AppConfig::map_rect.w, -AppConfig::tile_size.h, AppConfig::tile_size.w, AppConfig::map_rect.h + 2 * AppConfig::tile_size.h};
    m_map_outside_colision_top_rect = {-AppConfig::tile_size.w, -AppConfig::tile_size.h, AppConfig::map_rect.w + 2 * AppConfig::tile_size.w, AppConfig::tile_size.h};
    m_map_outside_colision_bottom_rect = {-AppConfig::tile_size.w, AppConfig::map_rect.h, AppConfig::map_rect.w + 2 * AppConfig::tile_size.w, AppConfig::tile_size.h};

    loadStage();
}

StageEnvironment::~StageEnvironment()
{
    for (auto &row : m_tile_objects)
    {
        for (auto obj : row)
        {
            if (obj != nullptr)
                delete obj;
        }
        row.clear();
    }
    m_tile_objects.clear();

    if (m_eagle != nullptr)
        delete m_eagle;
    m_eagle = nullptr;

    if (m_enemy_eagle != nullptr)
        delete m_enemy_eagle;
    m_enemy_eagle = nullptr;
}

void StageEnvironment::drawFirstLayer(Renderer &renderer)
{
    for (const auto &row : m_tile_objects)
    {
        for (const auto obj : row)
        {
            if (obj != nullptr && obj->type != ST_BUSH)
                obj->draw(renderer);
        }
    }
}

void StageEnvironment::drawSecondLayer(Renderer &renderer)
{
    for (const auto &row : m_tile_objects)
    {
        for (const auto obj : row)
        {
            if (obj != nullptr && (obj->type == ST_BUSH))
                obj->draw(renderer);
        }
    }

    if (m_eagle != nullptr)
        m_eagle->draw(renderer);

    if (m_enemy_eagle != nullptr)
        m_enemy_eagle->draw(renderer);
}

void StageEnvironment::update(Uint32 dt)
{
    for (auto &row : m_tile_objects)
    {
        for (auto obj : row)
        {
            if (obj != nullptr)
                obj->update(dt);
        }
    }

    if (m_eagle != nullptr)
        m_eagle->update(dt);

    if (m_enemy_eagle != nullptr)
        m_enemy_eagle->update(dt);

    if (m_eagle_protected)
    {
        m_eagle_protection_time += dt;
        if (m_eagle_protection_time >= AppConfig::protect_eagle_time)
        {
            m_eagle_protected = false;
            m_eagle_protection_time = 0;

            changeEagleSurroundingsToBrickWalls();
        }

        if (m_eagle_protection_time > AppConfig::protect_eagle_time * 3 / 4)
        {
            if ((m_eagle_protection_time / AppConfig::bonus_blink_time) % 2)
            {
                changeEagleSurroundingsToBrickWalls();
            }
            else
            {
                changeEagleSurroundingsToStoneWalls();
            }
        }
        else if (m_eagle_protected)
        {
            changeEagleSurroundingsToStoneWalls();
        }
    }
}

void StageEnvironment::destroyEagle()
{
    if (m_eagle != nullptr)
        m_eagle->destroy();
}

void StageEnvironment::startEagleProtection()
{
    if (m_eagle == nullptr)
        return;

    m_eagle_protected = true;
    m_eagle_protection_time = 0;

    changeEagleSurroundingsToStoneWalls();
}

void StageEnvironment::checkCollisionTankWithStage(Tank *tank, Uint32 dt)
{
    if (tank->to_erase)
        return;

    checkCollisionTankWithEnteringEnv(tank, dt);
    checkCollisionTankWithLeavingEnv(tank, dt);

    Rect intersect_rect;
    Rect pr = tank->nextCollisionRect(dt);

    if (m_eagle != nullptr)
    {
        intersect_rect = m_eagle->collision_rect.intersection(pr);
        if (intersect_rect.isNotEmpty())
            tank->collide(intersect_rect);
    }

    if (m_enemy_eagle != nullptr)
    {
        intersect_rect = m_enemy_eagle->collision_rect.intersection(pr);
        if (intersect_rect.isNotEmpty())
            tank->collide(intersect_rect);
    }

    intersect_rect = m_map_outside_colision_top_rect.intersection(pr);
    if (intersect_rect.isNotEmpty())
        tank->collide(intersect_rect);

    intersect_rect = m_map_outside_colision_bottom_rect.intersection(pr);
    if (intersect_rect.isNotEmpty())
        tank->collide(intersect_rect);

    intersect_rect = m_map_outside_colision_left_rect.intersection(pr);
    if (intersect_rect.isNotEmpty())
        tank->collide(intersect_rect);

    intersect_rect = m_map_outside_colision_right_rect.intersection(pr);
    if (intersect_rect.isNotEmpty())
        tank->collide(intersect_rect);
}

void StageEnvironment::checkCollisionBulletWithStage(Bullet *bullet)
{
    if (bullet == nullptr)
        return;
    if (bullet->isColide())
        return;

    bool is_player_bullet = (dynamic_cast<Player *>(bullet->owner()) != nullptr);
    Enemy *bullet_owner_enemy = dynamic_cast<Enemy *>(bullet->owner());
    // The ally's shells are punching, but they cannot break stone walls
    const bool is_friendly_bullet = (bullet_owner_enemy != nullptr && bullet_owner_enemy->friendly());
    int row_start, row_end;
    int column_start, column_end;

    //========================kolizja z elementami mapy========================
    switch (bullet->direction())
    {
    case D_UP:
        row_start = row_end = bullet->collision_rect.y / AppConfig::tile_size.h;
        column_start = bullet->collision_rect.x / AppConfig::tile_size.w;
        column_end = (bullet->collision_rect.x + bullet->collision_rect.w) / AppConfig::tile_size.w;
        break;
    case D_RIGHT:
        column_start = column_end = (bullet->collision_rect.x + bullet->collision_rect.w) / AppConfig::tile_size.w;
        row_start = bullet->collision_rect.y / AppConfig::tile_size.h;
        row_end = (bullet->collision_rect.y + bullet->collision_rect.h) / AppConfig::tile_size.h;
        break;
    case D_DOWN:
        row_start = row_end = (bullet->collision_rect.y + bullet->collision_rect.h) / AppConfig::tile_size.h;
        column_start = bullet->collision_rect.x / AppConfig::tile_size.w;
        column_end = (bullet->collision_rect.x + bullet->collision_rect.w) / AppConfig::tile_size.w;
        break;
    case D_LEFT:
        column_start = column_end = bullet->collision_rect.x / AppConfig::tile_size.w;
        row_start = bullet->collision_rect.y / AppConfig::tile_size.h;
        row_end = (bullet->collision_rect.y + bullet->collision_rect.h) / AppConfig::tile_size.h;
        break;
    }
    if (column_start < 0)
        column_start = 0;
    if (row_start < 0)
        row_start = 0;
    if (column_end >= AppConfig::map_size.w)
        column_end = AppConfig::map_size.w - 1;
    if (row_end >= AppConfig::map_size.h)
        row_end = AppConfig::map_size.h - 1;

    Rect intersect_rect;
    Object *o;

    bool shoudl_destroy_bulet = false;
    Rect &br = bullet->collision_rect;

    for (int i = row_start; i <= row_end; i++)
        for (int j = column_start; j <= column_end; j++)
        {
            o = m_tile_objects.at(i).at(j);
            if (o == nullptr)
                continue;
            if (o->type == ST_ICE || o->type == ST_WATER)
                continue;

            Rect &lr = o->collision_rect;
            intersect_rect = lr.intersection(br);

            if (intersect_rect.isNotEmpty())
            {
                if (bullet->isDamageIncreased() && !(is_friendly_bullet && o->type == ST_STONE_WALL))
                {
                    delete o;
                    m_tile_objects.at(i).at(j) = nullptr;
                }
                else if (o->type == ST_BRICK_WALL)
                {
                    Brick *brick = dynamic_cast<Brick *>(o);
                    brick->bulletHit(bullet->direction());
                    if (is_player_bullet || is_friendly_bullet)
                            playSound(SoundConfig::BULLET_HIT_BRICK);
                    if (brick->to_erase)
                    {
                        delete brick;
                        m_tile_objects.at(i).at(j) = nullptr;
                    }
                }
                else if (o->type == ST_STONE_WALL)
                {
                    // stone wall - do nothing, just play sound
                    if (is_player_bullet || is_friendly_bullet)
                        playSound(SoundConfig::BULLET_HIT_STONE);
                }
                // Do not stop bullet on bush, it allows to destroy all of bushes on the bullet way,
                // when damages are increased
                if (o->type != ST_BUSH)
                {
                    shoudl_destroy_bulet = true;
                }
            }
        }

    if (shoudl_destroy_bulet)
    {
        bullet->destroy();
    }

    //========================kolizja z granicami mapy========================
    if (br.x < 0 || br.y < 0 || br.x + br.w > AppConfig::map_rect.w || br.y + br.h > AppConfig::map_rect.h)
    {
        bullet->destroy();
        if (is_player_bullet)
            playSound(SoundConfig::BULLET_HIT_MAP_BOUNDARIES);
    }
}

bool StageEnvironment::checkCollisionBulletWithEagle(Bullet *bullet)
{
    if (bullet == nullptr)
        return false;
    if (bullet->isColide())
        return false;
    if (m_eagle == nullptr)
        return false;

    Rect intersect_rect = m_eagle->collision_rect.intersection(bullet->collision_rect);
    if (intersect_rect.isNotEmpty())
    {
        bullet->destroy();

        // Deathmatch: the ally's shells can never destroy our own eagle
        Enemy *bullet_owner_enemy = dynamic_cast<Enemy *>(bullet->owner());
        if (bullet_owner_enemy != nullptr && bullet_owner_enemy->friendly())
            return false;

        m_eagle->destroy();
        playSound(SoundConfig::EAGLE_DESTROYED);
        return true;
    }
    return false;
}

bool StageEnvironment::checkCollisionBulletWithEnemyBase(Bullet *bullet)
{
    if (bullet == nullptr)
        return false;
    if (bullet->isColide())
        return false;
    if (m_enemy_eagle == nullptr)
        return false;

    Rect intersect_rect = m_enemy_eagle->collision_rect.intersection(bullet->collision_rect);
    if (intersect_rect.isNotEmpty())
    {
        bullet->destroy();

        // The base is already a wreck — nothing left to destroy
        if (m_enemy_eagle->type != ST_EAGLE)
            return false;

        // Only the player and the ally may destroy the base;
        // enemy shells just die against their own defenses
        Enemy *bullet_owner_enemy = dynamic_cast<Enemy *>(bullet->owner());
        const bool friendly_fire = (bullet_owner_enemy != nullptr && bullet_owner_enemy->friendly());
        const bool player_fire = (dynamic_cast<Player *>(bullet->owner()) != nullptr);
        if (!friendly_fire && !player_fire)
            return false;

        m_enemy_eagle->destroy();
        playSound(SoundConfig::EAGLE_DESTROYED);
        return true;
    }
    return false;
}

bool StageEnvironment::checkCollisionWithEagle(Rect &rect)
{
    if (m_eagle != nullptr)
    {
        Rect intersect_rect = m_eagle->collision_rect.intersection(rect);
        if (intersect_rect.isNotEmpty())
            return true;
    }

    if (m_enemy_eagle != nullptr)
    {
        Rect intersect_rect = m_enemy_eagle->collision_rect.intersection(rect);
        if (intersect_rect.isNotEmpty())
            return true;
    }
    return false;
}

Point StageEnvironment::getEagleCenter() const
{
    return {m_eagle->pos_x + (int)(m_eagle->dest_rect.w / 2), m_eagle->pos_y + (int)(m_eagle->dest_rect.h / 2)};
}

bool StageEnvironment::hasEagle() const
{
    return m_eagle != nullptr;
}

Point StageEnvironment::getEnemyBaseCenter() const
{
    return {m_enemy_eagle->pos_x + (int)(m_enemy_eagle->dest_rect.w / 2), m_enemy_eagle->pos_y + (int)(m_enemy_eagle->dest_rect.h / 2)};
}

bool StageEnvironment::hasEnemyBase() const
{
    return m_enemy_eagle != nullptr && m_enemy_eagle->type == ST_EAGLE;
}

void StageEnvironment::checkCollisionTankWithEnteringEnv(Tank *tank, Uint32 dt)
{
    int row_start, row_end;
    int column_start, column_end;

    Object *o;

    // we check the range of tiles in front of the tank based on its direction
    switch (tank->movingDirection())
    {
    case D_UP:
        row_end = tank->collision_rect.y / AppConfig::tile_size.h;
        row_start = row_end - 1;
        column_start = tank->collision_rect.x / AppConfig::tile_size.w - 1;
        column_end = (tank->collision_rect.x + tank->collision_rect.w) / AppConfig::tile_size.w + 1;
        break;
    case D_RIGHT:
        column_start = (tank->collision_rect.x + tank->collision_rect.w) / AppConfig::tile_size.w;
        column_end = column_start + 1;
        row_start = tank->collision_rect.y / AppConfig::tile_size.h - 1;
        row_end = (tank->collision_rect.y + tank->collision_rect.h) / AppConfig::tile_size.h + 1;
        break;
    case D_DOWN:
        row_start = (tank->collision_rect.y + tank->collision_rect.h) / AppConfig::tile_size.h;
        row_end = row_start + 1;
        column_start = tank->collision_rect.x / AppConfig::tile_size.w - 1;
        column_end = (tank->collision_rect.x + tank->collision_rect.w) / AppConfig::tile_size.w + 1;
        break;
    case D_LEFT:
        column_end = tank->collision_rect.x / AppConfig::tile_size.w;
        column_start = column_end - 1;
        row_start = tank->collision_rect.y / AppConfig::tile_size.h - 1;
        row_end = (tank->collision_rect.y + tank->collision_rect.h) / AppConfig::tile_size.h + 1;
        break;
    }
    if (column_start < 0)
        column_start = 0;
    if (row_start < 0)
        row_start = 0;
    if (column_end >= AppConfig::map_size.w)
        column_end = AppConfig::map_size.w - 1;
    if (row_end >= AppConfig::map_size.h)
        row_end = AppConfig::map_size.h - 1;

    Rect pr = tank->nextCollisionRect(dt);
    Rect intersect_rect;

    bool on_ice = false;

    for (int i = row_start; i <= row_end; i++)
    {
        for (int j = column_start; j <= column_end; j++)
        {
            o = m_tile_objects.at(i).at(j);
            if (o == nullptr)
                continue;
            if (o->type == ST_BUSH)
                continue;
            if (tank->hasBoatEffect() && o->type == ST_WATER)
                continue;

            Rect &lr = o->collision_rect;

            intersect_rect = lr.intersection(pr);
            if (intersect_rect.isNotEmpty())
            {
                if (o->type == ST_ICE)
                {
                    if (intersect_rect.w > 10 && intersect_rect.h > 10)
                        on_ice = true;
                    continue;
                }
                else
                    tank->collide(intersect_rect);
                break;
            }
        }
    }

    if (on_ice)
    {
        tank->activateOnIceEffect();
    }
}

void StageEnvironment::checkCollisionTankWithLeavingEnv(Tank *tank, Uint32 dt)
{
    int row_start, row_end;
    int column_start, column_end;

    Object *o;

    // we check the range of tiles in rear of the tank based on its direction
    switch (tank->movingDirection())
    {
    case D_UP:
        row_end = (tank->collision_rect.y + tank->collision_rect.h) / AppConfig::tile_size.h;
        row_start = row_end - 1;
        column_start = tank->collision_rect.x / AppConfig::tile_size.w - 1;
        column_end = (tank->collision_rect.x + tank->collision_rect.w) / AppConfig::tile_size.w + 1;
        break;
    case D_RIGHT:
        column_start = (tank->collision_rect.x) / AppConfig::tile_size.w;
        column_end = column_start + 1;
        row_start = tank->collision_rect.y / AppConfig::tile_size.h - 1;
        row_end = (tank->collision_rect.y + tank->collision_rect.h) / AppConfig::tile_size.h + 1;
        break;
    case D_DOWN:
        row_start = (tank->collision_rect.y) / AppConfig::tile_size.h;
        row_end = row_start + 1;
        column_start = tank->collision_rect.x / AppConfig::tile_size.w - 1;
        column_end = (tank->collision_rect.x + tank->collision_rect.w) / AppConfig::tile_size.w + 1;
        break;
    case D_LEFT:
        column_end = (tank->collision_rect.x + tank->collision_rect.w) / AppConfig::tile_size.w;
        column_start = column_end - 1;
        row_start = tank->collision_rect.y / AppConfig::tile_size.h - 1;
        row_end = (tank->collision_rect.y + tank->collision_rect.h) / AppConfig::tile_size.h + 1;
        break;
    }
    if (column_start < 0)
        column_start = 0;
    if (row_start < 0)
        row_start = 0;
    if (column_end >= AppConfig::map_size.w)
        column_end = AppConfig::map_size.w - 1;
    if (row_end >= AppConfig::map_size.h)
        row_end = AppConfig::map_size.h - 1;

    Rect pr = tank->nextCollisionRect(dt);
    Rect intersect_rect;

    bool on_ice = false;

    for (int i = row_start; i <= row_end; i++)
    {
        for (int j = column_start; j <= column_end; j++)
        {
            o = m_tile_objects.at(i).at(j);
            if (o == nullptr)
                continue;

            if (o->type == ST_ICE)
            {
                Rect &lr = o->collision_rect;
                intersect_rect = lr.intersection(pr);
                if (intersect_rect.isNotEmpty())
                {
                    if (intersect_rect.w > 10 && intersect_rect.h > 10)
                        on_ice = true;
                }
            }
        }
    }

    if (on_ice)
    {
        tank->activateOnIceEffect();
    }
    else
    {
        tank->deactivateOnIceEffect();
    }
}

void StageEnvironment::loadStage()
{
    std::vector<std::string> lines;

    if (m_generated)
    {
        // Survival: no eagle nest, no water and no stone blocks.
        // Deathmatch: our eagle is here, but the map itself is survival-like
        // (water and stone are replaced by the fortified enemy base).
        const bool with_eagle = !m_survival;
        const bool with_water_stone = !m_survival && !m_deathmatch;
        lines = StageGenerator::generate(m_stage_number, with_eagle, with_water_stone);
    }
    else
    {
        // SDL_RWFromFile also reaches into the APK assets on Android, where
        // plain std::ifstream cannot open files.
        std::string stage_file_name = AppConfig::stages_path + std::to_string(m_loaded_stage);
        SDL_RWops *stage_file = SDL_RWFromFile(stage_file_name.c_str(), "rb");
        if (stage_file == nullptr)
        {
            throw std::runtime_error("Cannot open stage file: " + stage_file_name);
        }

        std::string stage_data;
        char buffer[1024];
        size_t bytes_read;
        while ((bytes_read = SDL_RWread(stage_file, buffer, 1, sizeof(buffer))) > 0)
        {
            stage_data.append(buffer, bytes_read);
        }
        SDL_RWclose(stage_file);

        std::istringstream stage_stream(stage_data);
        std::string line;
        while (std::getline(stage_stream, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back(line);
        }
    }

    m_tile_objects.clear();

    size_t row_index = 0;

    for (const std::string &line : lines)
    {
        std::vector<Object *> row;
        for (size_t col_index = 0; col_index < line.length() && col_index < (size_t)AppConfig::map_size.w; col_index++)
        {
            // Skip eagle area (there is none in survival mode)
            if (!m_survival &&
                col_index >= (size_t)m_eagle_tile_rect.x && col_index < (size_t)m_eagle_tile_rect.x + (size_t)m_eagle_tile_rect.w &&
                row_index >= (size_t)m_eagle_tile_rect.y && row_index < (size_t)m_eagle_tile_rect.y + (size_t)m_eagle_tile_rect.h)
            {
                row.push_back(nullptr);
                continue;
            }

            Object *obj = nullptr;
            switch (line.at(col_index))
            {
            case '#':
                obj = new Brick(col_index * AppConfig::tile_size.w, row_index * AppConfig::tile_size.h);
                break;
            case '@':
                obj = new Object(col_index * AppConfig::tile_size.w, row_index * AppConfig::tile_size.h, ST_STONE_WALL);
                break;
            case '%':
                obj = new Object(col_index * AppConfig::tile_size.w, row_index * AppConfig::tile_size.h, ST_BUSH);
                break;
            case '~':
                obj = new Object(col_index * AppConfig::tile_size.w, row_index * AppConfig::tile_size.h, ST_WATER);
                break;
            case '-':
                obj = new Object(col_index * AppConfig::tile_size.w, row_index * AppConfig::tile_size.h, ST_ICE);
                break;
            default:
                obj = nullptr;
            }
            row.push_back(obj);
        }
        m_tile_objects.push_back(row);
        row_index++;

        if (row_index >= (size_t)AppConfig::map_size.h)
            break;
    }

    if (m_survival)
        m_eagle = nullptr;
    else
        m_eagle = new Eagle(m_eagle_tile_rect.x * AppConfig::tile_size.w, m_eagle_tile_rect.y * AppConfig::tile_size.h);

    if (m_deathmatch)
        createEnemyBase();
}

void StageEnvironment::createEnemyBase()
{
    // The enemy base mirrors our eagle at the top edge (columns 11-14):
    //   row 0: metal  E E  metal
    //   row 1: brick  E E  brick
    //   row 2: brick  brick brick brick
    const int x = m_enemy_eagle_tile_rect.x;
    const int y = m_enemy_eagle_tile_rect.y;
    const int ts_w = AppConfig::tile_size.w;
    const int ts_h = AppConfig::tile_size.h;

    auto setTile = [&](int col, int row, Object *obj)
    {
        if (col < 0 || col >= AppConfig::map_size.w || row < 0 || row >= AppConfig::map_size.h)
        {
            delete obj;
            return;
        }
        Object *&cell = m_tile_objects.at(row).at(col);
        if (cell != nullptr)
            delete cell;
        cell = obj;
    };

    // The nest itself (the base occupies the two topmost cells)
    m_enemy_eagle = new Eagle(x * ts_w, y * ts_h, true);

    // Brick on the top and at the sides, metal at the bottom: the base
    // cannot be shot straight through from below
    setTile(x - 1, y,     new Brick((x - 1) * ts_w, y * ts_h));
    setTile(x + 2, y,     new Brick((x + 2) * ts_w, y * ts_h));
    setTile(x - 1, y + 1, new Brick((x - 1) * ts_w, (y + 1) * ts_h));
    setTile(x + 2, y + 1, new Brick((x + 2) * ts_w, (y + 1) * ts_h));
    for (int col = x - 1; col <= x + 2; col++)
        setTile(col, y + 2, new Object(col * ts_w, (y + 2) * ts_h, ST_STONE_WALL));

    // Clear the spawn corridor right below the base so enemies can roll out
    for (int row = y + 3; row <= y + 4; row++)
        for (int col = x - 1; col <= x + 2; col++)
        {
            Object *&cell = m_tile_objects.at(row).at(col);
            if (cell != nullptr)
            {
                delete cell;
                cell = nullptr;
            }
        }
}

void StageEnvironment::changeEagleSurroundingsToStoneWalls()
{
    for (int i = 0; i < m_eagle_tile_rect.h + 1; i++)
    {
        int row_index = AppConfig::map_size.h - i - 1;
        if (m_tile_objects.at(row_index).at(m_eagle_tile_rect.x - 1) != nullptr)
            delete m_tile_objects.at(row_index).at(m_eagle_tile_rect.x - 1);
        m_tile_objects.at(row_index).at(m_eagle_tile_rect.x - 1) = new Object((m_eagle_tile_rect.x - 1) * AppConfig::tile_size.w, row_index * AppConfig::tile_size.h, ST_STONE_WALL);

        if (m_tile_objects.at(row_index).at(m_eagle_tile_rect.x + m_eagle_tile_rect.w) != nullptr)
            delete m_tile_objects.at(row_index).at(m_eagle_tile_rect.x + m_eagle_tile_rect.w);
        m_tile_objects.at(row_index).at(m_eagle_tile_rect.x + m_eagle_tile_rect.w) = new Object((m_eagle_tile_rect.x + m_eagle_tile_rect.w) * AppConfig::tile_size.w, row_index * AppConfig::tile_size.h, ST_STONE_WALL);
    }
    for (int i = m_eagle_tile_rect.x - 1; i < m_eagle_tile_rect.x + m_eagle_tile_rect.w + 1; i++)
    {
        int row_index = AppConfig::map_size.h - m_eagle_tile_rect.h - 1;
        if (m_tile_objects.at(row_index).at(i) != nullptr)
            delete m_tile_objects.at(row_index).at(i);
        m_tile_objects.at(row_index).at(i) = new Object(i * AppConfig::tile_size.w, row_index * AppConfig::tile_size.h, ST_STONE_WALL);
    }
}

void StageEnvironment::changeEagleSurroundingsToBrickWalls()
{
    for (int i = 0; i < m_eagle_tile_rect.h + 1; i++)
    {
        int row_index = AppConfig::map_size.h - i - 1;
        if (m_tile_objects.at(row_index).at(m_eagle_tile_rect.x - 1) != nullptr)
            delete m_tile_objects.at(row_index).at(m_eagle_tile_rect.x - 1);
        m_tile_objects.at(row_index).at(m_eagle_tile_rect.x - 1) = new Brick((m_eagle_tile_rect.x - 1) * AppConfig::tile_size.w, row_index * AppConfig::tile_size.h);

        if (m_tile_objects.at(row_index).at(m_eagle_tile_rect.x + m_eagle_tile_rect.w) != nullptr)
            delete m_tile_objects.at(row_index).at(m_eagle_tile_rect.x + m_eagle_tile_rect.w);
        m_tile_objects.at(row_index).at(m_eagle_tile_rect.x + m_eagle_tile_rect.w) = new Brick((m_eagle_tile_rect.x + m_eagle_tile_rect.w) * AppConfig::tile_size.w, row_index * AppConfig::tile_size.h);
    }
    for (int i = m_eagle_tile_rect.x - 1; i < m_eagle_tile_rect.x + m_eagle_tile_rect.w + 1; i++)
    {
        int row_index = AppConfig::map_size.h - m_eagle_tile_rect.h - 1;
        if (m_tile_objects.at(row_index).at(i) != nullptr)
            delete m_tile_objects.at(row_index).at(i);
        m_tile_objects.at(row_index).at(i) = new Brick(i * AppConfig::tile_size.w, row_index * AppConfig::tile_size.h);
    }
};
