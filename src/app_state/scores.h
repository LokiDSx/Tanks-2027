#ifndef SCORES_H
#define SCORES_H
#include "appstate.h"
#include "game/game.h"
#include "../objects/tank/player/player.h"
#include "../engine/state_machine/context_state.h"
#include "../high_score.h"
#include <vector>
#include <string>

class Scores : public AppState
{
public:
    Scores(std::vector<Player *> players, int stage, bool game_over, InteractiveComponents interactive_components, StateMachine *parent_state_machine, bool generated = false, bool survival = false, bool deathmatch = false, unsigned tanks_killed = 0, int deathmatch_difficulty = Game::DM_MEDIUM, bool friend_enabled = true);
    ~Scores() override;

    void draw(Renderer &renderer) override;
    void update(const UpdateState &updateState) override;
    void eventProcess(const Event &event) override;

protected:
    void onInitialize() override;

private:
    void transiteToNextState();
    void drawScore(Renderer &renderer);

    // The game mode this scores screen belongs to (its own high score)
    HighScore::Mode highScoreMode() const;

    StateMachine* m_scores_state_machine;

    std::vector<Player *> m_players;
    int m_stage;
    bool m_game_over;
    bool m_generated;
    bool m_survival;
    bool m_deathmatch;
    unsigned m_tanks_killed; // survival: tanks destroyed during this run
    // Deathmatch: setup options carried over to the next stage
    int m_deathmatch_difficulty;
    bool m_friend_enabled;
    unsigned m_score_counter;
    unsigned m_max_score;

    // Sub-states for score counting and idle state after counting
    class CountingState : public ContextState<Scores>
    {
    public:
        CountingState(Scores *ps);
        void draw(Renderer &renderer) override;
        void update(const UpdateState &updateState) override;
        void eventProcess(const Event &event) override;

    private:
        Uint32 m_single_score_count_time;
    };

    class IdleState : public ContextState<Scores>
    {
    public:
        IdleState(Scores *ps);
        void draw(Renderer &renderer) override;
        void update(const UpdateState &updateState) override;
        void eventProcess(const Event &event) override;

    private:
        void checkHighScoreAndTransite();
        Uint32 m_idle_time;
    };

    class HighScoreState : public ContextState<Scores>
    {
    public:
        HighScoreState(Scores *ps);
        void draw(Renderer &renderer) override;
        void update(const UpdateState &updateState) override;
        void eventProcess(const Event &event) override;

    protected:
        void onInitialize() override;

    private:
        Uint32 m_display_time;
    };
};

#endif // SCORES_H
