#include "profile_select.h"
#include "../engine/renderer.h"
#include "../appconfig.h"
#include "../soundconfig.h"
#include "../profile.h"
#include "name_input.h"
#include "settings.h"
#include "menu.h"

#include <algorithm>

namespace
{
    const int VISIBLE_ROWS = 7;
}

ProfileSelect::ProfileSelect(InteractiveComponents interactive_components, StateMachine *state_machine, bool switch_mode)
    : AppState(interactive_components, state_machine),
      m_switch_mode(switch_mode),
      m_current_index(0),
      m_offset(0)
{
}

void ProfileSelect::onInitialize()
{
    stopAllSounds();
    // Hack to fill sound device buffer and avoid sound delay on first play
    playSound(SoundConfig::MENU_ITEM_SELECTED);

    const std::vector<std::string> &profiles = Profile::list();
    m_scores.clear();
    for (const auto &name : profiles)
        m_scores.push_back(Profile::bestScore(name));

    // Start with the currently active profile highlighted
    auto it = std::find(profiles.begin(), profiles.end(), Profile::current());
    if (it != profiles.end())
        m_current_index = static_cast<int>(it - profiles.begin());
    adjustOffset();
}

int ProfileSelect::rowsCount() const
{
    // Every profile plus the "NEW PROFILE" row
    return static_cast<int>(Profile::list().size()) + 1;
}

void ProfileSelect::adjustOffset()
{
    if (m_current_index < m_offset)
        m_offset = m_current_index;
    if (m_current_index >= m_offset + VISIBLE_ROWS)
        m_offset = m_current_index - VISIBLE_ROWS + 1;
    if (m_offset < 0)
        m_offset = 0;
}

void ProfileSelect::draw(Renderer &renderer)
{
    renderer.clear();

    renderer.drawRect(AppConfig::windows_rect, {180, 180, 180, 255}, true);

    renderer.drawText({-1, 50}, "SELECT PROFILE", {0, 0, 0, 255}, FontSize::BIG);

    // Header of the records column
    const std::string best_header = "BEST";
    Size header_size = renderer.measureText(best_header, FontSize::NORMAL);
    renderer.drawText({AppConfig::windows_rect.w - 8 - header_size.w, 78}, best_header, {100, 100, 100, 255}, FontSize::NORMAL);

    const std::vector<std::string> &profiles = Profile::list();
    const int first_row = 100;
    const int row_height = 34;

    for (int i = m_offset; i < rowsCount() && i < m_offset + VISIBLE_ROWS; i++)
    {
        const bool selected = (i == m_current_index);
        const int y = first_row + (i - m_offset) * row_height;

        std::string label;
        if (i < static_cast<int>(profiles.size()))
        {
            label = profiles[i];
            // Mark the active profile with a star
            if (profiles[i] == Profile::current())
                label += " *";
        }
        else
        {
            label = "NEW PROFILE";
        }

        label = (selected ? "> " : "  ") + label;
        renderer.drawText({60, y}, label, selected ? Color{255, 255, 255, 255} : Color{0, 0, 0, 255}, FontSize::BIG);

        if (i < static_cast<int>(profiles.size()))
        {
            const std::string score = std::to_string(m_scores[i]);
            Size score_size = renderer.measureText(score, FontSize::NORMAL);
            const int score_y = y + (row_height - score_size.h) / 2;
            renderer.drawText({AppConfig::windows_rect.w - 8 - score_size.w, score_y}, score, {100, 100, 100, 255}, FontSize::NORMAL);
        }
    }

    if (profiles.empty())
        renderer.drawText({-1, 200}, "NO PROFILES YET", {0, 0, 0, 255}, FontSize::NORMAL);

    renderer.drawText({-1, 350}, m_switch_mode ? "X - OPEN   O - BACK" : "X - OPEN   O - EXIT",
                      {0, 0, 0, 255}, FontSize::NORMAL);

    renderer.flush();
}

void ProfileSelect::update(const UpdateState &)
{
}

void ProfileSelect::eventProcess(const Event &event)
{
    if (event.type() != Event::KEYBOARD)
        return;

    const KeyboardEvent &ev = static_cast<const KeyboardEvent &>(event);
    if (ev.keyState() != KeyboardEvent::PRESSED)
        return;

    if (ev.isPressed(KeyCode::KEY_UP))
    {
        m_current_index = (m_current_index + rowsCount() - 1) % rowsCount();
        adjustOffset();
        playSound(SoundConfig::MENU_ITEM_SELECTED);
    }
    else if (ev.isPressed(KeyCode::KEY_DOWN))
    {
        m_current_index = (m_current_index + 1) % rowsCount();
        adjustOffset();
        playSound(SoundConfig::MENU_ITEM_SELECTED);
    }
    else if (ev.isPressed(KeyCode::KEY_SPACE) || ev.isPressed(KeyCode::KEY_RETURN))
    {
        openCurrent();
    }
    else if (ev.isPressed(KeyCode::KEY_ESCAPE))
    {
        if (m_switch_mode)
        {
            transiteTo(new Settings(m_interactive_components, m_state_machine));
        }
        else
        {
            transiteToNullAndStop();
        }
    }
}

void ProfileSelect::openCurrent()
{
    const std::vector<std::string> &profiles = Profile::list();

    if (m_current_index < static_cast<int>(profiles.size()))
    {
        // Open the selected profile: its settings and records replace the
        // current in-memory values
        Profile::activate(profiles[m_current_index]);
        m_interactive_components.sound_manager->setMuted(!AppConfig::sound_enabled);
        playSound(SoundConfig::MENU_ITEM_SELECTED);

        if (m_switch_mode)
        {
            transiteTo(new Settings(m_interactive_components, m_state_machine));
        }
        else
        {
            transiteTo(new Menu(m_interactive_components, m_state_machine));
        }
    }
    else
    {
        const NameInput::Context context = m_switch_mode ? NameInput::Context::SETTINGS : NameInput::Context::STARTUP;
        transiteTo(new NameInput(m_interactive_components, m_state_machine, context));
    }
}
