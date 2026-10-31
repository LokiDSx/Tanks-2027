#ifndef TANK_ENEMY_H
#define TANK_ENEMY_H

#include "../tank.h"

class Enemy : public Tank
{
public:
    // friendly = true builds the Deathmatch ally: ST_PLAYER_2 sprite and
    // strong (but not maximum) stats, its goal is to hunt enemy tanks
    Enemy(double x, double y, SpriteType type, unsigned armor_count, InteractiveComponents interactive_components, bool friendly = false);
    ~Enemy();

    void draw(Renderer &renderer) override;
    void update(Uint32 dt) override;

    Bullet *fire() override;

    void hit();
    unsigned scoreForHit();

    void makeWithBonus();
    bool bonus() const;

    // Deathmatch hunter: every third enemy spawns with the ally's logic —
    // it relentlessly chases the player and fires with aimed shots
    void makeHunter();
    bool hunter() const;

    // Deathmatch base killer: storms the player's base (the eagle) with
    // aimed shots; spawns with the weakest stats on Hard
    void makeBaseKiller();
    bool baseKiller() const;

    // The strongest speed available to a tank of this class
    void makeMaxStats();

    bool friendly() const;

    Point target_position;

private:
    void drawEnemy(Renderer &renderer);
    void updateBehavior(Uint32 dt);

    StateMachine *m_enemy_state_machine;

    bool m_bonus;
    bool m_friendly;
    // Deathmatch hunter: the ally's brain, but out for the player
    bool m_hunter;
    // Deathmatch base killer: out for the player's eagle
    bool m_base_killer;

    Uint32 m_direction_time;
    Uint32 m_keep_direction_time;
    Uint32 m_speed_time;
    Uint32 m_try_to_go_time;

    Uint32 m_fire_time;
    Uint32 m_reload_time;

    // Sub-states
    class CreatingState : public ContextState<Enemy>
    {
    public:
        CreatingState(Enemy *enemy);
        void draw(Renderer &renderer) override;
        void update(const UpdateState &updateState) override;

    protected:
        void onInitialize() override;
    };

    class AliveState : public ContextState<Enemy>
    {
    public:
        AliveState(Enemy *enemy);
        void draw(Renderer &renderer) override;
        void update(const UpdateState &updateState) override;

    protected:
        void onInitialize() override;

    private:
        void checkKeyStates(const UpdateState &updateState);
    };

    class DestroyedState : public ContextState<Enemy>
    {
    public:
        DestroyedState(Enemy *enemy);
        void draw(Renderer &renderer) override;
        void update(const UpdateState &updateState) override;

    protected:
        void onInitialize() override;
    };
};

#endif // TANK_ENEMY_H
