#ifndef GAME_H
#define GAME_H
#include "../appstate.h"
#include "../../objects/object.h"
#include "../../objects/tank/player/player.h"
#include "../../objects/tank/enemy/enemy.h"
#include "../../objects/tank/bullet.h"
#include "../../objects/brick.h"
#include "../../objects/eagle.h"
#include "../../objects/bonus.h"
#include "../../engine/state_machine/context_state.h"
#include "stage_environment.h"
#include <vector>
#include <string>

class Game : public AppState
{
public:
    // Deathmatch difficulties (chosen on the Deathmatch setup screen)
    enum DeathmatchDifficulty
    {
        DM_EASY = 0,
        DM_MEDIUM = 1,
        DM_HARD = 2,
    };

    // modern_classic: the 1.2 mode with all the levels of the classic
    // campaign, the new bonus pool and the hunter / base-killer pair
    Game(int players_count, int starting_stage, InteractiveComponents interactive_components, StateMachine *state_machine, bool generated = false, bool survival = false, bool deathmatch = false, int deathmatch_difficulty = DM_MEDIUM, bool friend_enabled = true, bool modern_classic = false);
    Game(std::vector<Player *> players, int previous_stage, InteractiveComponents interactive_components, StateMachine *state_machine, bool generated = false, bool survival = false, bool deathmatch = false, int deathmatch_difficulty = DM_MEDIUM, bool friend_enabled = true, bool modern_classic = false);
    ~Game();

    void draw(Renderer &renderer) override;
    void update(const UpdateState &updateState) override;

    /**
     * Key press handling function.
     * Enters - pause mode,
     * ESC - returns to menu,
     * N - jump to next stage
     * B - jump to previous stage
     * T - show enemy target paths
     */
    void eventProcess(const Event &event) override;

protected:
    void onInitialize() override;

private:
    void clearAll();
    void createPlayersIfNeeded();
    // Deathmatch: spawn the friendly tank at the second player's point
    void createFriendlyIfNeeded();
    // Spawn the ally (no-op while one is already alive on the map)
    void spawnFriendly();

    void drawScene(Renderer &renderer);
    void drawObjects(Renderer &renderer);
    void drawEnemy(Renderer &renderer, Enemy *enemy);
    void drawGameOver(Renderer &renderer);
    void drawGameStatusPanel(Renderer &renderer);

    void updateScene(Uint32 dt);
    void checkCollisions(Uint32 dt);
    void checkCollisionTwoTanks(Tank *tank1, Tank *tank2, Uint32 dt);
    void checkCollisionPlayerBulletsWithEnemy(Player *player, Enemy *enemy);
    void checkCollisionEnemyBulletsWithPlayer(Enemy *enemy, Player *player);
    void checkCollisionTwoBullets(Bullet *bullet1, Bullet *bullet2);
    void checkCollisionPlayerWithBonus(Player *player, Bonus *bonus);

    void updateObjects(Uint32 dt);

    void calculateEnemiesTargets();
    void generateEnemyIfPossible(Uint32 dt);
    void countKill();

    void generateBonus();
    // Put the given bonus in the middle of the map
    void placeBonusInMapCenter(Bonus *b);
    // Drop a fresh bonus of the given type in the middle of the map
    void spawnBonusAtCenter(SpriteType type);

    void transiteToGameOver();
    // Deathmatch: the enemy base is destroyed — the stage is won
    void transiteToStageComplete();
    // Drone bonus: freeze the game and let the player pick a tank to destroy
    void startDroneTargeting(Player *player);

    // Classic / Modern Classic: this stage is the last one of the campaign
    // (35, 70, ... - the stage files cycle, but the count never resets)
    bool isLastClassicStage() const;
    // The last stage is cleared: the campaign is won, but the game goes on
    void startYouWin();
    // Draws the "YOU WIN!" banner over the scene (PlayingState only)
    void drawYouWinMessage(Renderer &renderer);
    // True when no hostile tank is left (the ally does not count)
    bool hostileEnemiesEmpty() const;

    StateMachine *m_game_state_machine;
    StageEnvironment *m_stage_environment;

    std::vector<Enemy *> m_enemies;
    std::vector<Player *> m_players;
    std::vector<Player *> m_killed_players;
    std::vector<Bonus *> m_bonuses;

    int m_current_stage;
    int m_players_count;
    int m_enemies_to_kill_count;

    Uint32 m_new_enemy_cooldown;
    unsigned m_enemy_respown_position;
    // Deathmatch: spawn rotation counter (every third spawned enemy gets
    // the hunter's brain on Medium, on Hard the rotation is
    // max-stats / hunter / base-killer)
    unsigned m_hunter_spawn_counter;

    bool m_show_enemies_targets;
    bool m_generated;
    bool m_survival;
    bool m_deathmatch;
    // Deathmatch setup: difficulty and whether the friendly tank spawns
    int m_deathmatch_difficulty;
    bool m_friend_enabled;
    // 1.2: Modern Classic (classic stages, the new bonus pool and the
    // hunter / base-killer pair kept on the field)
    bool m_modern_classic;
    // The last stage of the classic campaign is won: the level does not
    // finish, enemies keep spawning without a limit and the "YOU WIN!"
    // banner is shown once
    bool m_you_won;
    Uint32 m_you_win_time;
    // No new enemies spawn once the stage is over (e.g. the Deathmatch base
    // was destroyed while the stage ending state is still updating the scene)
    bool m_stage_finished;
    int m_kills;

    // Deathmatch: the friendly tank (an entry of m_enemies while it is alive)
    Enemy *m_friend;
    Uint32 m_friend_respawn_time;

    // Drone bonus: the player that picked it up, waiting for DroneTargetState
    Player *m_pending_drone_player;

    // Sub-states
    class StartingState : public ContextState<Game>
    {
    public:
        StartingState(Game *ps);
        void draw(Renderer &renderer) override;
        void update(const UpdateState &updateState) override;

    private:
        Uint32 m_stage_start_time;
    };

    class PlayingState : public ContextState<Game>
    {
    public:
        PlayingState(Game *ps);
        void draw(Renderer &renderer) override;
        void update(const UpdateState &updateState) override;
        void eventProcess(const Event &event) override;
    };

    class StageEndingState : public ContextState<Game>
    {
    public:
        StageEndingState(Game *ps, bool no_waiting, bool game_over);
        void draw(Renderer &renderer) override;
        void update(const UpdateState &updateState) override;
        void eventProcess(const Event &event) override;

    private:
        Uint32 m_stage_end_time;
        bool m_no_waiting;
        bool m_game_over;
    };

    // Pause menu: RETURN goes back to the game, MAIN MENU leaves the
    // level and opens the main menu.
    class PauseState : public ContextState<Game>
    {
    public:
        PauseState(Game *ps);
        void draw(Renderer &renderer) override;
        void update(const UpdateState &updateState) override;
        void eventProcess(const Event &event) override;

    protected:
        void onInitialize() override;

    private:
        // Index of the selected menu item (0 - RETURN, 1 - MAIN MENU)
        int m_selected;

        /** Applies the selected menu item. */
        void activateSelected();
    };

    // Drone bonus: the game freezes and the player steers a crosshair over
    // the map; the tank under the crosshair is destroyed on the fire button.
    class DroneTargetState : public ContextState<Game>
    {
    public:
        DroneTargetState(Game *ps);
        void draw(Renderer &renderer) override;
        void update(const UpdateState &updateState) override;
        void eventProcess(const Event &event) override;

        // Player that picked the drone up; it becomes the operator here.
        // Public: Game::startDroneTargeting() hands the remote over right
        // after the state is created.
        void setOwner(Player *player);

    protected:
        void onInitialize() override;

    private:
        // Pointer to the tank currently under the crosshair; null if none.
        // Enemies are never deleted while this state runs (the scene is
        // frozen), so the pointer stays valid for the whole state.
        Tank *tankUnderCursor() const;
        void destroyTarget();
        void finish();

        Point m_cursor;
        Player *m_owner;
    };

    class GameOverState : public ContextState<Game>
    {
    public:
        GameOverState(Game *ps);
        void draw(Renderer &renderer) override;
        void update(const UpdateState &updateState) override;
        void eventProcess(const Event &event) override;

    protected:
        void onInitialize() override;

    private:
        double m_game_over_message_position;
    };
};

#endif // GAME_H
