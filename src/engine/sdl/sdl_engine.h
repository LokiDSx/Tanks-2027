#ifndef SDL_ENGINE_H
#define SDL_ENGINE_H

#include "../data/error.h"
#include "../engine.h"
#include "sdl_renderer.h"
#include "sdl_sound_manager.h"

struct SDLEngineConfig
{
    std::string window_title;
    Size initial_window_size;
};

class SDLEngine : public Engine
{
public:
    ~SDLEngine();

    InteractiveComponents getInteractiveComponents() const override;
    void startMainLoop(OnEngineInitFunc onEngineInit, HandleEventFunc handleEvent, UpdateStateFunc updateState, DrawFunc draw) override;

    void setConfig(SDLEngineConfig config);

private:
    bool is_main_loop_running = false;

#ifdef ANDROID_PORT
    // True while the app is in the background (SDL_APP_WILLENTERBACKGROUND
    // ... SDL_APP_DIDENTERFOREGROUND): the main loop keeps polling events
    // but freezes updates and rendering.
    bool m_backgrounded = false;
#endif

    SDL_Window *m_window;
    SDLRenderer *m_renderer;
    SDLSoundManager *m_sound_manager;

    SDLEngineConfig m_config;

    void initComponents();
    void destroyComponents();

    ProcessingResult handleEvents(HandleEventFunc handleEvent);
    ProcessingResult handleInternalEvents(const Event &event);
};

#endif // SDL_ENGINE_H