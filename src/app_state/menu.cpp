#include "menu.h"
#include "../engine/engine.h"
#include "../engine/renderer.h"
#include "../appconfig.h"
#include "../soundconfig.h"
#include "../spriteconfig.h"
#include "../high_score.h"
#include "../profile.h"
#include "stage_select.h"
#include "deathmatch_setup.h"
#include "settings.h"
#include "game/game.h"

#include <iostream>

Menu::Menu(InteractiveComponents interactive_components, StateMachine *state_machine)
    : AppState(interactive_components, state_machine)
{
    m_menu_items.push_back("Classic Mode");
    m_menu_items.push_back("Modern Classic");
    m_menu_items.push_back("Generated Mode");
    m_menu_items.push_back("Survival Mode");
    m_menu_items.push_back("Deathmatch Mode");
    m_menu_items.push_back("Settings");
    m_menu_items.push_back("Exit");

    m_current_menu_index = 0;

    m_menu_item_height = AppConfig::tile_size.h * 2;
    m_first_menu_item_offset = {180, 120};
    m_tank_menu_pointer_offset = {140, 110};

    m_tank_menu_pointer = new Player(0, 0, ST_PLAYER_1, AppConfig::player_1_keys, m_interactive_components);
}

Menu::~Menu()
{
    delete m_tank_menu_pointer;
}

void Menu::onInitialize()
{
    m_tank_menu_pointer->pos_x = m_tank_menu_pointer_offset.x;
    m_tank_menu_pointer->pos_y = (m_current_menu_index + 1) * m_menu_item_height + m_tank_menu_pointer_offset.y;
    m_tank_menu_pointer->startPreview();

    stopAllSounds();
    // Hack to fill sound device buffer and avoid sound delay on first play
    playSound(SoundConfig::MENU_ITEM_SELECTED);
}

void Menu::draw(Renderer &renderer)
{
    renderer.clear();

    renderer.drawRect(AppConfig::map_rect, {0, 0, 0, 255}, true);
    renderer.drawRect(AppConfig::status_rect, {0, 0, 0, 255}, true);

    Point p_dst = {-1, 15};
    Color text_color = {255, 255, 255, 255};
    renderer.drawText(p_dst, "HI- " + std::to_string(HighScore::best()), text_color, FontSize::NORMAL);

    // Name of the open profile, top right corner of the status bar
    const std::string profile_name = Profile::current();
    if (!profile_name.empty())
    {
        Size name_size = renderer.measureText(profile_name, FontSize::NORMAL);
        renderer.drawText({AppConfig::windows_rect.w - 8 - name_size.w, 15}, profile_name, {200, 200, 200, 255}, FontSize::NORMAL);
    }

    // LOGO
    const SpriteData &logo = SpriteConfig::getInstance().getSpriteData(ST_TANKS_LOGO);
    Rect dst = {(AppConfig::map_rect.w + AppConfig::status_rect.w - logo.rect.w) / 2, 50, logo.rect.w, logo.rect.h};
    renderer.drawObject(logo.rect, dst);

    int i = 0;
    Point text_start;
    for (auto text : m_menu_items)
    {
        text_start = {m_first_menu_item_offset.x, (i + 1) * m_menu_item_height + m_first_menu_item_offset.y};
        i++;
        renderer.drawText(text_start, text, {255, 255, 255, 255}, FontSize::BIG);
    }

    // High score of each game mode, right-aligned next to its menu item
    // (the score rows follow the order of the game mode menu items, the
    // first five items are the five recorded modes).
    // The survival row also shows the destroyed tanks record ([tanks][icon] score).
    // The white tank icon is used: the black one blends into the dark menu.
    const int score_right_margin = 8;
    Rect enemy_icon_src = SpriteConfig::getInstance().getSpriteData(ST_LEFT_ENEMY_WHITE).rect;
    for (int mode = 0; mode < 5; mode++)
    {
        const std::string score_text = std::to_string(HighScore::scores[mode]);
        Size score_size = renderer.measureText(score_text, FontSize::NORMAL);
        int row_top = (mode + 1) * m_menu_item_height + m_first_menu_item_offset.y;
        int score_y = row_top + (m_menu_item_height - score_size.h) / 2;
        int score_x = AppConfig::windows_rect.w - score_right_margin - score_size.w;
        renderer.drawText({score_x, score_y}, score_text, {200, 200, 200, 255}, FontSize::NORMAL);

        if (mode == (int)HighScore::Mode::Survival)
        {
            const int icon_gap = 4;
            const std::string tanks_text = std::to_string(HighScore::survival_tanks);
            Size tanks_size = renderer.measureText(tanks_text, FontSize::NORMAL);

            int icon_x = score_x - icon_gap - enemy_icon_src.w;
            Rect icon_dst = {icon_x, row_top + (m_menu_item_height - enemy_icon_src.h) / 2, enemy_icon_src.w, enemy_icon_src.h};
            renderer.drawObject(enemy_icon_src, icon_dst);

            int tanks_x = icon_x - icon_gap - tanks_size.w;
            int tanks_y = row_top + (m_menu_item_height - tanks_size.h) / 2;
            renderer.drawText({tanks_x, tanks_y}, tanks_text, {200, 200, 200, 255}, FontSize::NORMAL);
        }
    }

    m_tank_menu_pointer->draw(renderer);

    renderer.flush();
}

void Menu::update(const UpdateState &updateState)
{
    m_tank_menu_pointer->update(updateState.delta_time);
}

void Menu::eventProcess(const Event &event)
{
    if (event.type() == Event::KEYBOARD)
    {
        const KeyboardEvent &ev = static_cast<const KeyboardEvent &>(event);

        if (ev.isPressed(KeyCode::KEY_UP))
        {
            if (m_current_menu_index == 0)
            {
                m_current_menu_index = (int)m_menu_items.size() - 1;
            }
            else
            {
                m_current_menu_index--;
            }

            m_tank_menu_pointer->pos_y = (m_current_menu_index + 1) * m_menu_item_height + m_tank_menu_pointer_offset.y;
            playSound(SoundConfig::MENU_ITEM_SELECTED);
        }
        else if (ev.isPressed(KeyCode::KEY_DOWN))
        {

            if (m_current_menu_index >= (int)m_menu_items.size() - 1)
            {
                m_current_menu_index = 0;
            }
            else
            {
                m_current_menu_index++;
            }

            m_tank_menu_pointer->pos_y = (m_current_menu_index + 1) * m_menu_item_height + m_tank_menu_pointer_offset.y;
            playSound(SoundConfig::MENU_ITEM_SELECTED);
        }
        else if (ev.isPressed(KeyCode::KEY_SPACE) || ev.isPressed(KeyCode::KEY_RETURN))
        {
            if (m_current_menu_index == 0)
            {
                transiteTo(new StageSelect(1, m_interactive_components, m_state_machine));
            }
            else if (m_current_menu_index == 1)
            {
                transiteTo(new StageSelect(1, m_interactive_components, m_state_machine, true));
            }
            else if (m_current_menu_index == 2)
            {
                transiteTo(new Game(1, 1, m_interactive_components, m_state_machine, true));
            }
            else if (m_current_menu_index == 3)
            {
                transiteTo(new Game(1, 1, m_interactive_components, m_state_machine, true, true));
            }
            else if (m_current_menu_index == 4)
            {
                transiteTo(new DeathmatchSetup(m_interactive_components, m_state_machine));
            }
            else if (m_current_menu_index == 5)
            {
                transiteTo(new Settings(m_interactive_components, m_state_machine));
            }
            else
            {
                transiteToNullAndStop();
            }
        }
        else if (ev.isPressed(KeyCode::KEY_ESCAPE))
        {
            transiteToNullAndStop();
        }
    }
}