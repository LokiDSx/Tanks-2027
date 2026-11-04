#ifndef APPCONFIG_H
#define APPCONFIG_H

#include "engine/data/data.h"
#include "engine/data/keycode.h"
#include <iostream>
#include <vector>

using namespace std;

// Macbook default keyboard does not has a right ctrl key
#if defined(__APPLE__) && defined(__MACH__)
#define P1_FIRE_KEY KeyCode::KEY_RALT
#define P2_FIRE_KEY KeyCode::KEY_LALT
#else
#define P1_FIRE_KEY KeyCode::KEY_RCTRL
#define P2_FIRE_KEY KeyCode::KEY_LCTRL
#endif

namespace AppConfig
{
#ifdef VITA_PORT
    // Assets are packed into the VPK under the app root: ux0:app/TANK00001/assets/
    inline string assets_dir = "app0:/assets/";
#elif defined(PSP_PORT)
    // Assets sit next to EBOOT.PBP in the install folder (default: ms0:/PSP/GAME/TANKS/)
    inline string assets_dir = "ms0:/PSP/GAME/TANKS/assets/";
#elif defined(ANDROID_PORT)
    // Assets live at the root of the read-only APK and are opened through
    // SDL_RW (IMG_Load, TTF_OpenFont, Mix_LoadWAV, SDL_RWFromFile), which
    // falls back to the APK asset manager for relative paths.
    inline string assets_dir = "";
#else
    inline string assets_dir = "";
#endif

    inline string texture_path = assets_dir + "texture.png";
    inline string stages_path = assets_dir + "stages/";
    inline string font_path = assets_dir + "kongtext.ttf";

    inline string game_over_text = "GAME OVER";

    // Display mode: true = stretch to fill the screen (widescreen),
    // false = keep native 464x416 aspect ratio (black bars on the sides).
    // Default is standard; toggled at runtime from the Settings menu.
    inline bool widescreen = false;

    // Sound toggle (Settings menu). Applied to the sound manager on startup.
    inline bool sound_enabled = true;

    inline Size map_size  = {26, 26};
    inline Size tile_size = {16, 16};

    inline Rect map_rect     = {0, 0, map_size.w * tile_size.w, map_size.h * tile_size.h};
    inline Rect status_rect  = {map_rect.w, 0, 3 * tile_size.w, map_rect.h};
    inline Rect windows_rect = {0, 0, map_rect.w + status_rect.w, map_rect.h};

    inline vector<Point> player_starting_point = []
    {
        vector<Point> v;
        v.push_back({8  * tile_size.w, 24 * tile_size.h});
        v.push_back({16 * tile_size.w, 24 * tile_size.h});
        return v;
    }();

    inline vector<Point> enemy_starting_point = []
    {
        vector<Point> v;
        v.push_back({1, 1});
        v.push_back({12 * tile_size.w, 1});
        v.push_back({24 * tile_size.w, 1});
        return v;
    }();

    // Deathmatch: the middle spawn point would land inside the enemy base,
    // so enemies pour out from the corridor right below it instead
    inline vector<Point> enemy_starting_point_deathmatch = []
    {
        vector<Point> v;
        v.push_back({1, 1});
        v.push_back({12 * tile_size.w, 3 * tile_size.h});
        v.push_back({24 * tile_size.w, 1});
        return v;
    }();

    inline vector<KeyCode> player_1_keys = []
    {
        vector<KeyCode> v;
        v.push_back(KeyCode::KEY_UP);
        v.push_back(KeyCode::KEY_DOWN);
        v.push_back(KeyCode::KEY_LEFT);
        v.push_back(KeyCode::KEY_RIGHT);
        v.push_back(P1_FIRE_KEY);
        return v;
    }();

    inline vector<KeyCode> player_2_keys = []
    {
        vector<KeyCode> v;
        v.push_back(KeyCode::KEY_W);
        v.push_back(KeyCode::KEY_S);
        v.push_back(KeyCode::KEY_A);
        v.push_back(KeyCode::KEY_D);
        v.push_back(P2_FIRE_KEY);
        return v;
    }();

    inline unsigned slip_time = 380;

    inline unsigned enemies_to_kill_total_count = 20;
    inline unsigned enemies_max_count_on_map    = 4;

    inline unsigned stages_count             = 35;
    inline unsigned new_enemy_cooldown       = 500;
    inline unsigned player_bullet_base_count = 2;
    inline unsigned bonus_show_time          = 10000;
    inline unsigned tank_shield_time         = 10000;
    inline unsigned tank_frozen_time         = 8000;
    inline unsigned protect_eagle_time       = 15000;
    inline unsigned bonus_blink_time         = 350;
    inline unsigned player_reload_time       = 120;
    inline double   tank_default_speed       = 0.08;
    inline double   bullet_default_speed     = 0.23;

    // Drone bonus: the game freezes and the player picks a tank to destroy
    inline int      drone_cursor_speed       = 3;

    namespace Score
    {
        inline unsigned single_count_time  = 100;
        inline unsigned idle_time          = 5000;
        inline unsigned high_score_time    = 5200;
    }

    namespace Game
    {
        inline unsigned stage_start_time        = 3200;
        inline unsigned stage_end_time          = 5000;
        inline double   game_over_message_speed = 0.13;
        // Deathmatch: respawn delay of the friendly tank after its death
        inline unsigned friend_respawn_time     = 2000;
    }
}

#endif // APPCONFIG_H
