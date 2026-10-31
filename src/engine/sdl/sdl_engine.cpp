#include "sdl_engine.h"
#include "sdl_event_mapper.h"

#ifdef CONSOLE_PORT
#include "console_input.h"
#endif

#include <random>
#include <stdlib.h>
#include <ctime>
#include <vector>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>

#if defined(CONSOLE_PORT)
// Native display resolution of the target console
static const int CONSOLE_DISPLAY_WIDTH =
#if defined(PSP_PORT)
    480
#else
    960
#endif
    ;
static const int CONSOLE_DISPLAY_HEIGHT =
#if defined(PSP_PORT)
    272
#else
    544
#endif
    ;
#endif

SDLEngine::~SDLEngine()
{
}

void SDLEngine::initComponents()
{
    m_renderer = new SDLRenderer(m_config.initial_window_size);
    m_renderer->loadTexture(m_window);

    m_sound_manager = new SDLSoundManager();
}

void SDLEngine::destroyComponents()
{
    delete m_renderer;
    m_renderer = nullptr;

    delete m_sound_manager;
    m_sound_manager = nullptr;
}

InteractiveComponents SDLEngine::getInteractiveComponents() const
{
    return {m_sound_manager};
}

void SDLEngine::startMainLoop(OnEngineInitFunc onEngineInit, HandleEventFunc handleEvent, UpdateStateFunc updateState, DrawFunc draw)
{
    is_main_loop_running = true;

    Uint32 init_flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO;
#ifdef CONSOLE_PORT
    init_flags |= SDL_INIT_JOYSTICK;
#endif

    if (SDL_Init(init_flags) == 0)
    {

        unsigned int seed;
        try
        {
            std::random_device dev;
            seed = dev();
        }
        catch (...)
        {
            // std::random_device is not reliable on every platform (e.g. PS Vita)
            seed = static_cast<unsigned int>(time(nullptr));
        }
        srand(seed);

#ifdef CONSOLE_PORT
        // The game renders its native 464x416 viewport scaled to the whole
        // display with aspect-ratio-corrected black bars (see SDLRenderer).
        m_window = SDL_CreateWindow(m_config.window_title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    CONSOLE_DISPLAY_WIDTH, CONSOLE_DISPLAY_HEIGHT, SDL_WINDOW_SHOWN);
#else
        m_window = SDL_CreateWindow(m_config.window_title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    m_config.initial_window_size.w, m_config.initial_window_size.h, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
#endif

        if (m_window == nullptr)
        {
            throw Error("SDL Window could not be created!", SDL_GetError());
        }

        initComponents();

#ifdef CONSOLE_PORT
        m_renderer->setViewportForWindowSize({CONSOLE_DISPLAY_WIDTH, CONSOLE_DISPLAY_HEIGHT});
        ConsoleInput::init();
#endif

        if (onEngineInit(*this) == ProcessingResult::STOP)
        {
            is_main_loop_running = false;
            return;
        }

        Uint32 probe_time = 0, frames_count = 0, delay = 15;
        Uint32 current_time = SDL_GetTicks();
        Uint32 previous_time = current_time;
        while (is_main_loop_running)
        {
            current_time = SDL_GetTicks();
            Uint32 delta_time = current_time - previous_time;
            previous_time = current_time;

            if (handleEvents(handleEvent) == ProcessingResult::STOP)
            {
                is_main_loop_running = false;
                break;
            }

            UpdateState us{delta_time};

            if (updateState(us) == ProcessingResult::STOP)
            {
                is_main_loop_running = false;
                break;
            }

            if (draw(*m_renderer) == ProcessingResult::STOP)
            {
                is_main_loop_running = false;
                break;
            }

            SDL_Delay(delay);

            // FPS
            probe_time += delta_time;
            frames_count++;
            if (probe_time > 200)
            {
                const double fps = static_cast<double>(frames_count) / probe_time * 1000;
                if (fps > 60)
                    delay++;
                else if (delay > 0)
                    delay--;
                probe_time = 0;
                frames_count = 0;
            }
        }

        destroyComponents();

        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        SDL_Quit();
    }
}

ProcessingResult SDLEngine::handleEvents(HandleEventFunc handleEvent)
{
    std::vector<Event *> events;

    SDL_Event sdl_event;
    while (SDL_PollEvent(&sdl_event))
    {
        if (sdl_event.type == SDL_QUIT)
        {
            for (Event *pending : events)
                delete pending;
            return ProcessingResult::STOP;
        }

#ifdef CONSOLE_PORT
        ConsoleInput::handleEvent(sdl_event, events);
#endif

        Event *e = mapSDLEventToEngineEvent(sdl_event);
        if (e->type() != Event::UNKNOWN)
        {
            events.push_back(e);
        }
        else
        {
            delete e;
        }
    }

#ifdef CONSOLE_PORT
    ConsoleInput::pollRepeats(events);
#endif

    for (size_t i = 0; i < events.size(); ++i)
    {
        Event *e = events[i];

        ProcessingResult result = handleInternalEvents(*e);
        if (result == ProcessingResult::CONTINUE)
        {
            result = handleEvent(*e);
        }

        delete e;

        if (result == ProcessingResult::STOP)
        {
            for (size_t j = i + 1; j < events.size(); ++j)
                delete events[j];
            return ProcessingResult::STOP;
        }
    }

    return ProcessingResult::CONTINUE;
}

ProcessingResult SDLEngine::handleInternalEvents(const Event &event)
{
    if (event.type() == Event::WINDOW)
    {
        const WindowEvent &we = static_cast<const WindowEvent &>(event);
        if (we.windowEventType() == WindowEvent::RESIZED ||
            we.windowEventType() == WindowEvent::MAXIMIZED ||
            we.windowEventType() == WindowEvent::RESTORED ||
            we.windowEventType() == WindowEvent::SHOWN)
        {
            m_renderer->setViewportForWindowSize(we.windowSize());
        }
    }
    else if (event.type() == Event::KEYBOARD)
    {
        const KeyboardEvent &ke = static_cast<const KeyboardEvent &>(event);
        if (ke.isPressed(KeyCode::KEY_F11))
        {
            m_renderer->toggleFullscreen(m_window);
        }
    }

    return ProcessingResult::CONTINUE;
}

void SDLEngine::setConfig(SDLEngineConfig config)
{
    m_config = config;
}