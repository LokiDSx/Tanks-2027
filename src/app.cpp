#include "app.h"
#include "appconfig.h"
#include "profile.h"
#include "soundconfig.h"
#include "engine/engine.h"
#include "engine/sdl/sdl_engine.h"
#include "app_state/profile_select.h"

#include <ctime>
#include <iostream>
#include <random>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

App::App()
{
    m_app_state_machine = new StateMachine;
}

App::~App()
{
    if (m_app_state_machine != nullptr)
        delete m_app_state_machine;
}

void App::run()
{
    // Read the profile list; on the very first launch the data saved by
    // older builds is kept in memory to be seeded into the first profile
    Profile::init();

    SDLEngine sdl_engine;

    sdl_engine.setConfig({"Tanks 2027", Size{AppConfig::windows_rect.w, AppConfig::windows_rect.h}});

    Engine &engine = sdl_engine;

    engine.startMainLoop(
        [&](const Engine &engine)
        { return onEngineInit(engine); },
        [&](const Event &event)
        { return handleEvent(event); },
        [&](const UpdateState &us)
        { return updateState(us); },
        [&](Renderer &renderer)
        { return draw(renderer); });
}

ProcessingResult App::onEngineInit(const Engine &engine)
{
    InteractiveComponents components = engine.getInteractiveComponents();

    // No profile is active yet at startup, so apply the plain defaults
    // until the player opens a profile on the selection screen
    components.sound_manager->setMuted(!AppConfig::sound_enabled);

    // Decode all sound effects up front: a lazy load inside the game loop
    // stalls the PSP while it reads the memory stick (first shot, first
    // brick hit, ...). This happens while the screen is still black, so a
    // slightly longer startup is the only visible cost.
    SoundConfig::preloadAll(*components.sound_manager);

    // Ask which profile to open (or to create the first one)
    m_app_state_machine->setState(new ProfileSelect(components, m_app_state_machine));

    return ProcessingResult::CONTINUE;
}

ProcessingResult App::handleEvent(const Event &event)
{
    m_app_state_machine->eventProcess(event);
    return ProcessingResult::CONTINUE;
}

ProcessingResult App::updateState(const UpdateState &updateState)
{
    if (m_app_state_machine->stopped())
    {
        return ProcessingResult::STOP;
    }
    m_app_state_machine->update(updateState);
    return ProcessingResult::CONTINUE;
}

ProcessingResult App::draw(Renderer &renderer)
{
    m_app_state_machine->draw(renderer);
    return ProcessingResult::CONTINUE;
}
