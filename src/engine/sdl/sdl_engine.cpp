#include "sdl_engine.h"
#include "sdl_event_mapper.h"

#ifdef CONSOLE_PORT
#include "console_input.h"
#endif

#ifdef ANDROID_PORT
#include "touch_overlay.h"
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

#ifdef ANDROID_PORT
    // SDL registers the phone's accelerometer as joystick #0 by default.
    // Gravity then reads as a constant stick deflection (the axis aligned
    // with gravity saturates to ~32767, far above the 12000 threshold):
    // the tank would drive itself and the on-screen D-pad would show the
    // direction permanently lit. Real game controllers are enumerated
    // separately and keep working with this hint off.
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");

    // Keep touch finger-only: SDL normally synthesizes a mouse "click" for
    // every finger on top of the SDL_Finger* event. Those pseudo-mouse events
    // fight with the real finger records in TouchOverlay (the synthetic mouse
    // can steal the D-pad's pseudo-finger from the second real finger), which
    // broke multitouch - holding D-pad left the tank unable to fire. Real
    // mice on the desktop are unaffected, only touch-derived mouse events
    // are dropped.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
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
                                    CONSOLE_DISPLAY_WIDTH, CONSOLE_DISPLAY_HEIGHT, SDL_WINDOW_SHOWN
#ifdef ANDROID_PORT
                                    // Android's screen is much bigger than the console's: desktop
                                    // fullscreen makes SDL switch the activity into immersive mode
                                    // (status and navigation bars hidden) instead of leaving the
                                    // picture in a corner with the system bars visible.
                                    | SDL_WINDOW_FULLSCREEN_DESKTOP
#endif
        );
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
#ifdef ANDROID_PORT
        // The native surface already has the real screen size by the time the
        // window is created (Android_CreateWindow copies it), so the viewport
        // is taken from the window itself. Hardcoding the console's 960x544
        // here would letterbox the picture into the top-left corner and make
        // the touch overlay's hit tests (which use SDL_GetWindowSize) disagree
        // with the coordinates the buttons are drawn at.
        int win_w = 0, win_h = 0;
        SDL_GetWindowSize(m_window, &win_w, &win_h);
        m_renderer->setViewportForWindowSize({win_w, win_h});
#else
        m_renderer->setViewportForWindowSize({CONSOLE_DISPLAY_WIDTH, CONSOLE_DISPLAY_HEIGHT});
#endif
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
#ifdef ANDROID_PORT
            // The Android surface can be resized at any moment (immersive mode
            // engaging, rotation, gesture bar) and the resize event does not
            // always arrive (the first surfaceChanged happens before the window
            // exists). Re-check the real window size every frame so the
            // viewport and the touch overlay (drawn and hit-tested in window
            // coordinates) stay in sync with the screen.
            {
                int win_w = 0, win_h = 0;
                SDL_GetWindowSize(m_window, &win_w, &win_h);
                const Size current = m_renderer->windowSize();
                if (win_w > 0 && win_h > 0 && (win_w != current.w || win_h != current.h))
                {
                    m_renderer->setViewportForWindowSize({win_w, win_h});
                }
            }
#endif

            current_time = SDL_GetTicks();
            Uint32 delta_time = current_time - previous_time;
            previous_time = current_time;

            if (handleEvents(handleEvent) == ProcessingResult::STOP)
            {
                is_main_loop_running = false;
                break;
            }

#ifdef ANDROID_PORT
            // The app is in the background: keep pumping events (so the
            // foreground transition is noticed) but freeze the game. The
            // delta discarded here would otherwise make the game jump by
            // the whole background time once the player returns.
            if (m_backgrounded)
            {
                SDL_Delay(100);
                continue;
            }
#endif

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

#ifdef ANDROID_PORT
        // The OS is tearing the activity down: exit the loop cleanly so the
        // renderer, audio and window are destroyed in order.
        if (sdl_event.type == SDL_APP_TERMINATING)
        {
            for (Event *pending : events)
                delete pending;
            return ProcessingResult::STOP;
        }

        if (sdl_event.type == SDL_APP_WILLENTERBACKGROUND)
            m_backgrounded = true;
        else if (sdl_event.type == SDL_APP_DIDENTERFOREGROUND)
            m_backgrounded = false;

        int window_w = 0;
        int window_h = 0;
        SDL_GetWindowSize(m_window, &window_w, &window_h);
        if (window_w <= 0 || window_h <= 0)
        {
            // SDL can briefly report a zero-sized window (surface being
            // recreated, immersive mode toggling). Touch events are dropped
            // when the size is unknown, so fall back to the last known good
            // size from the renderer instead of swallowing the press.
            const Size sz = m_renderer->windowSize();
            window_w = sz.w;
            window_h = sz.h;
        }
        TouchOverlay::handleEvent(sdl_event, events, window_w, window_h);
#endif

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
            we.windowEventType() == WindowEvent::SIZE_CHANGED ||
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