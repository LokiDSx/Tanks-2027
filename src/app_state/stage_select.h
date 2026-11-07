#ifndef STAGE_SELECT_H
#define STAGE_SELECT_H

#include "appstate.h"

class StageSelect : public AppState
{
public:
    // modern_classic: the stage is played under the Modern Classic rules
    // (all the new bonuses, the hunter and the base killer on the field)
    StageSelect(int players_count, InteractiveComponents interactive_components, StateMachine *state_machine, bool modern_classic = false);

    void draw(Renderer &renderer) override;
    void update(const UpdateState &updateState) override;
    void eventProcess(const Event &event) override;

protected:
    void onInitialize() override;

private:
    int m_players_count;
    int m_selected_stage;
    bool m_modern_classic;
};

#endif // STAGE_SELECT_H
