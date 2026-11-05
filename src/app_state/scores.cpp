#include "scores.h"
#include "../engine/engine.h"
#include "../engine/data/data.h"
#include "../appconfig.h"
#include "../soundconfig.h"
#include "../high_score.h"
#include "../profile.h"
#include "game/game.h"
#include "menu.h"

Scores::Scores(std::vector<Player *> players, int stage, bool game_over, InteractiveComponents interactive_components, StateMachine *state_machine, bool generated, bool survival, bool deathmatch, unsigned tanks_killed, int deathmatch_difficulty, bool friend_enabled)
    : AppState(interactive_components, state_machine),
      m_scores_state_machine(new StateMachine())
{
    m_players = players;
    m_stage = stage;
    m_game_over = game_over;
    m_generated = generated;
    m_survival = survival;
    m_deathmatch = deathmatch;
    m_tanks_killed = tanks_killed;
    m_deathmatch_difficulty = deathmatch_difficulty;
    m_friend_enabled = friend_enabled;

    m_score_counter = 0;
    m_max_score = 0;
}

Scores::~Scores()
{
    delete m_scores_state_machine;
}

HighScore::Mode Scores::highScoreMode() const
{
    if (m_survival)
        return HighScore::Mode::Survival;
    if (m_deathmatch)
        return HighScore::Mode::Deathmatch;
    if (m_generated)
        return HighScore::Mode::Generated;
    return HighScore::Mode::Classic;
}

void Scores::onInitialize()
{
    for (auto player : m_players)
    {
        player->to_erase = false;
        player->startPreview();

        if (player->score() > m_max_score)
            m_max_score = player->score();
    }

    stopAllSounds();

    m_scores_state_machine->setState(new CountingState(this));
}

void Scores::draw(Renderer &renderer)
{
    renderer.clear();

    m_scores_state_machine->draw(renderer);

    renderer.flush();
}

void Scores::drawScore(Renderer &renderer)
{
    renderer.drawRect(AppConfig::map_rect, {0, 0, 0, 255}, true);
    renderer.drawRect(AppConfig::status_rect, {0, 0, 0, 255}, true);

    Point p_dst;
    Rect dst;

    p_dst = {-1, 15};
    Color text_color = {255, 255, 255, 255};
    renderer.drawText(p_dst, "HI- " + std::to_string(HighScore::forMode(highScoreMode())), text_color, FontSize::NORMAL);

    if (m_survival)
    {
        // Tanks destroyed during this run, top right of the screen
        const std::string tanks_text = "TANKS- " + std::to_string(m_tanks_killed);
        Size tanks_size = renderer.measureText(tanks_text, FontSize::NORMAL);
        Point tanks_dst = {AppConfig::windows_rect.w - 8 - tanks_size.w, 15};
        renderer.drawText(tanks_dst, tanks_text, text_color, FontSize::NORMAL);
    }

    p_dst = {-1, 40};
    renderer.drawText(p_dst, std::string("STAGE ") + std::to_string(m_stage), {255, 255, 220, 255}, FontSize::BIGGEST);
    p_dst = {100, 80};
    renderer.drawText(p_dst, std::string("PLAYER"), {255, 255, 255, 255}, FontSize::NORMAL);
    p_dst = {270, 80};
    renderer.drawText(p_dst, std::string("SCORE"), {255, 255, 255, 255}, FontSize::NORMAL);
    dst = {75, 105, 300, 2};
    renderer.drawRect(dst, {250, 250, 200, 255}, true);
    int i = 0;
    for (auto player : m_players)
    {
        dst = {100, 120 + i * (player->src_rect.h), player->src_rect.w, player->src_rect.h};
        renderer.drawObject(player->src_rect, dst);
        p_dst = {140, 128 + i * (player->src_rect.h)};
        renderer.drawText(p_dst, std::string("x") + std::to_string(player->lives()), {255, 255, 255, 255}, FontSize::NORMAL);
        p_dst = {270, 128 + i * (player->src_rect.h)};
        renderer.drawText(p_dst, (m_score_counter < player->score() ? std::to_string(m_score_counter) : std::to_string(player->score())), {255, 255, 255, 255}, FontSize::NORMAL);
        i++;
    }
}

void Scores::update(const UpdateState &updateState)
{
    Uint32 dt = updateState.delta_time;

    for (auto player : m_players)
    {
        player->setDirection(D_RIGHT);
        player->update(dt);
    }

    m_scores_state_machine->update(updateState);
}

void Scores::eventProcess(const Event &event)
{
    m_scores_state_machine->eventProcess(event);
}

void Scores::transiteToNextState()
{
    if (m_game_over)
    {
        Menu *m = new Menu(m_interactive_components, m_state_machine);
        transiteTo(m);
    }
    else
    {
        Game *g = new Game(m_players, m_stage, m_interactive_components, m_state_machine, m_generated, m_survival, m_deathmatch, m_deathmatch_difficulty, m_friend_enabled);
        transiteTo(g);
    }
}

Scores::CountingState::CountingState(Scores *ps) : ContextState<Scores>(ps, ps->m_scores_state_machine), m_single_score_count_time(0) {}

void Scores::CountingState::draw(Renderer &renderer)
{
    m_context->drawScore(renderer);
}

void Scores::CountingState::update(const UpdateState &updateState)
{
    Uint32 dt = updateState.delta_time;

    m_single_score_count_time += dt;

    if (m_single_score_count_time > AppConfig::Score::single_count_time)
    {
        if (m_context->m_score_counter < 10)
            m_context->m_score_counter++;
        else if (m_context->m_score_counter < 100)
            m_context->m_score_counter += 10;
        else if (m_context->m_score_counter < 1000)
            m_context->m_score_counter += 100;
        else if (m_context->m_score_counter < 10000)
            m_context->m_score_counter += 1000;
        else if (m_context->m_score_counter < 100000)
            m_context->m_score_counter += 10000;
        else
            m_context->m_score_counter += 100000;

        m_context->playSound(SoundConfig::SCORE_POINT_COUNTED);

        m_single_score_count_time = 0;
    }

    if (m_context->m_score_counter >= m_context->m_max_score)
    {
        transiteTo(new Scores::IdleState(m_context));
    }
}

void Scores::CountingState::eventProcess(const Event &event)
{
    if (event.type() == Event::KEYBOARD)
    {
        const KeyboardEvent &ev = static_cast<const KeyboardEvent &>(event);

        if (ev.isPressed(KeyCode::KEY_RETURN)
#ifdef ANDROID_PORT
            // FIRE also skips the score count / continues
            || ev.isPressed(KeyCode::KEY_SPACE)
#endif
        )
        {
            m_context->m_score_counter = m_context->m_max_score;
            transiteTo(new Scores::IdleState(m_context));
        }
    }
}

Scores::IdleState::IdleState(Scores *ps) : ContextState<Scores>(ps, ps->m_scores_state_machine), m_idle_time(0) {}

void Scores::IdleState::draw(Renderer &renderer)
{
    m_context->drawScore(renderer);
}

void Scores::IdleState::update(const UpdateState &updateState)
{
    Uint32 dt = updateState.delta_time;

    m_idle_time += dt;

    if (m_idle_time > AppConfig::Score::idle_time)
    {
        checkHighScoreAndTransite();
    }
}

void Scores::IdleState::eventProcess(const Event &event)
{
    if (event.type() == Event::KEYBOARD)
    {
        const KeyboardEvent &ev = static_cast<const KeyboardEvent &>(event);

        if (ev.isPressed(KeyCode::KEY_RETURN)
#ifdef ANDROID_PORT
            // FIRE continues from the score screen
            || ev.isPressed(KeyCode::KEY_SPACE)
#endif
        )
        {
            checkHighScoreAndTransite();
        }
    }
}

void Scores::IdleState::checkHighScoreAndTransite()
{
    // Survival: the destroyed tanks counter is fixed as a record
    if (m_context->m_survival && m_context->m_tanks_killed > HighScore::survival_tanks)
    {
        HighScore::survival_tanks = m_context->m_tanks_killed;
        Profile::save();
    }

    if (m_context->m_game_over)
    {
        unsigned max_score = 0;
        for (auto player : m_context->m_players)
        {
            if (player->score() > max_score)
                max_score = player->score();
        }

        if (HighScore::checkAndUpdate(m_context->highScoreMode(), max_score))
        {
            Profile::save();
            transiteTo(new Scores::HighScoreState(m_context));
            return;
        }
    }

    m_context->transiteToNextState();
}

Scores::HighScoreState::HighScoreState(Scores *ps) : ContextState<Scores>(ps, ps->m_scores_state_machine), m_display_time(0) {}

void Scores::HighScoreState::onInitialize()
{
    m_context->stopAllSounds();
    m_context->playSound(SoundConfig::HIGHSCORE_BEATEN);
}

void Scores::HighScoreState::draw(Renderer &renderer)
{
    renderer.drawRect(AppConfig::map_rect, {0, 0, 0, 255}, true);
    renderer.drawRect(AppConfig::status_rect, {0, 0, 0, 255}, true);

    const Color flash_colors[] = {
        {255, 220,   0, 255},
        {255,  80,  80, 255},
        {255, 255, 255, 255},
        { 80, 255,  80, 255},
        { 80, 180, 255, 255},
    };
    Color text_color = flash_colors[(m_display_time / 20) % 5];

    Point p_dst;
    p_dst = {-1, 80};
    renderer.drawText(p_dst, "HISCORE", text_color, FontSize::FS_60);

    p_dst = {-1, 180};
    renderer.drawText(p_dst, std::to_string(HighScore::forMode(m_context->highScoreMode())), text_color, FontSize::FS_48);
}

void Scores::HighScoreState::update(const UpdateState &updateState)
{
    m_display_time += updateState.delta_time;

    if (m_display_time > AppConfig::Score::high_score_time)
    {
        m_context->transiteToNextState();
    }
}

void Scores::HighScoreState::eventProcess(const Event &event)
{
    if (event.type() == Event::KEYBOARD)
    {
        const KeyboardEvent &ev = static_cast<const KeyboardEvent &>(event);

        if (ev.isPressed(KeyCode::KEY_RETURN)
#ifdef ANDROID_PORT
            // FIRE skips the hiscore banner
            || ev.isPressed(KeyCode::KEY_SPACE)
#endif
        )
        {
            m_context->transiteToNextState();
        }
    }
}