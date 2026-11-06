#ifndef CONSOLE_INPUT_H
#define CONSOLE_INPUT_H

#include "../data/event.h"
#include "../data/keycode.h"

#include <SDL2/SDL.h>
#include <vector>

// Console gamepad input (shared by the PS Vita and PSP ports).
//
// Both consoles are driven through SDL2's native joystick drivers, whose
// button indices are identical for the Vita and the PSP, so a single mapping
// serves both. The whole game logic is keyboard driven, so pad input is
// translated into synthetic keyboard events. Button indices follow SDL's
// native Vita/PSP joystick drivers (src/joystick/vita, src/joystick/psp):
//
//   D-Pad / left stick ... arrows (movement, menu navigation, with key repeat)
//   Cross / Square / R ... fire (RCTRL) + confirm (SPACE)
//   Circle ............... ESCAPE (back / quit to menu)
//   Start ................ RETURN (pause / confirm)
//   Select ............... T (show/hide enemy targets)

namespace ConsoleInput
{
    enum JoyButton
    {
        BTN_TRIANGLE = 0,
        BTN_CIRCLE   = 1,
        BTN_CROSS    = 2,
        BTN_SQUARE   = 3,
        BTN_L        = 4,
        BTN_R        = 5,
        BTN_DOWN     = 6,
        BTN_LEFT     = 7,
        BTN_UP       = 8,
        BTN_RIGHT    = 9,
        BTN_SELECT   = 10,
        BTN_START    = 11
    };

    constexpr int AXIS_LEFT_X = 0;
    constexpr int AXIS_LEFT_Y = 1;

    constexpr Sint16 AXIS_PRESS_THRESHOLD   = 12000;
    constexpr Sint16 AXIS_RELEASE_THRESHOLD = 6000;

    constexpr Uint32 REPEAT_FIRST_DELAY_MS = 350;
    constexpr Uint32 REPEAT_INTERVAL_MS    = 90;

    constexpr int DIR_UP = 0;
    constexpr int DIR_DOWN = 1;
    constexpr int DIR_LEFT = 2;
    constexpr int DIR_RIGHT = 3;

    struct DirState
    {
        bool dpad = false;
        bool stick = false;
        bool held = false;
        Uint32 next_repeat_at = 0;
    };

    inline DirState s_dirs[4];
    inline SDL_Joystick *s_joystick = nullptr;

    inline KeyCode dirKeyCode(int dir)
    {
        static const KeyCode keys[4] = {KeyCode::KEY_UP, KeyCode::KEY_DOWN, KeyCode::KEY_LEFT, KeyCode::KEY_RIGHT};
        return keys[dir];
    }

    inline void emitKeyboard(std::vector<Event *> &out, KeyCode key, KeyboardEvent::KeyState state)
    {
        out.push_back(new KeyboardEvent(key, state, 0));
    }

    inline void emitFireAndConfirm(std::vector<Event *> &out, KeyboardEvent::KeyState state)
    {
        emitKeyboard(out, KeyCode::KEY_RCTRL, state);
        emitKeyboard(out, KeyCode::KEY_SPACE, state);
    }

    inline void updateDir(std::vector<Event *> &out, int dir, bool dpad, bool stick, Uint32 now)
    {
        DirState &d = s_dirs[dir];
        d.dpad = dpad;
        d.stick = stick;

        const bool down = d.dpad || d.stick;
        if (down && !d.held)
        {
            d.held = true;
            d.next_repeat_at = now + REPEAT_FIRST_DELAY_MS;
            emitKeyboard(out, dirKeyCode(dir), KeyboardEvent::PRESSED);
        }
        else if (!down && d.held)
        {
            d.held = false;
            emitKeyboard(out, dirKeyCode(dir), KeyboardEvent::RELEASED);
        }
    }

    inline void handleJoyButton(std::vector<Event *> &out, Uint8 button, Uint8 value, Uint32 now)
    {
        const bool down = (value == SDL_PRESSED);
        const KeyboardEvent::KeyState state = down ? KeyboardEvent::PRESSED : KeyboardEvent::RELEASED;

        switch (button)
        {
        case BTN_UP:
            updateDir(out, DIR_UP, down, s_dirs[DIR_UP].stick, now);
            break;
        case BTN_DOWN:
            updateDir(out, DIR_DOWN, down, s_dirs[DIR_DOWN].stick, now);
            break;
        case BTN_LEFT:
            updateDir(out, DIR_LEFT, down, s_dirs[DIR_LEFT].stick, now);
            break;
        case BTN_RIGHT:
            updateDir(out, DIR_RIGHT, down, s_dirs[DIR_RIGHT].stick, now);
            break;
        case BTN_CROSS:
        case BTN_SQUARE:
        case BTN_R:
            emitFireAndConfirm(out, state);
            break;
        case BTN_CIRCLE:
            emitKeyboard(out, KeyCode::KEY_ESCAPE, state);
            break;
        case BTN_START:
            emitKeyboard(out, KeyCode::KEY_RETURN, state);
            break;
        case BTN_SELECT:
            emitKeyboard(out, KeyCode::KEY_T, state);
            break;
        default:
            break;
        }
    }

    inline void handleJoyAxis(std::vector<Event *> &out, Uint8 axis, Sint16 value, Uint32 now)
    {
        if (axis == AXIS_LEFT_Y)
        {
            // Stick up is negative
            const bool push_up = value <= -AXIS_PRESS_THRESHOLD;
            const bool push_down = value >= AXIS_PRESS_THRESHOLD;

            if (!push_up && s_dirs[DIR_UP].stick && value > -AXIS_RELEASE_THRESHOLD)
                updateDir(out, DIR_UP, s_dirs[DIR_UP].dpad, false, now);
            if (!push_down && s_dirs[DIR_DOWN].stick && value < AXIS_RELEASE_THRESHOLD)
                updateDir(out, DIR_DOWN, s_dirs[DIR_DOWN].dpad, false, now);

            if (push_up)
                updateDir(out, DIR_UP, s_dirs[DIR_UP].dpad, true, now);
            if (push_down)
                updateDir(out, DIR_DOWN, s_dirs[DIR_DOWN].dpad, true, now);
        }
        else if (axis == AXIS_LEFT_X)
        {
            const bool push_left = value <= -AXIS_PRESS_THRESHOLD;
            const bool push_right = value >= AXIS_PRESS_THRESHOLD;

            if (!push_left && s_dirs[DIR_LEFT].stick && value > -AXIS_RELEASE_THRESHOLD)
                updateDir(out, DIR_LEFT, s_dirs[DIR_LEFT].dpad, false, now);
            if (!push_right && s_dirs[DIR_RIGHT].stick && value < AXIS_RELEASE_THRESHOLD)
                updateDir(out, DIR_RIGHT, s_dirs[DIR_RIGHT].dpad, false, now);

            if (push_left)
                updateDir(out, DIR_LEFT, s_dirs[DIR_LEFT].dpad, true, now);
            if (push_right)
                updateDir(out, DIR_RIGHT, s_dirs[DIR_RIGHT].dpad, true, now);
        }
    }

    inline void releaseEverything(std::vector<Event *> &out, Uint32 now)
    {
        updateDir(out, DIR_UP, false, false, now);
        updateDir(out, DIR_DOWN, false, false, now);
        updateDir(out, DIR_LEFT, false, false, now);
        updateDir(out, DIR_RIGHT, false, false, now);
        emitFireAndConfirm(out, KeyboardEvent::RELEASED);
        emitKeyboard(out, KeyCode::KEY_ESCAPE, KeyboardEvent::RELEASED);
        emitKeyboard(out, KeyCode::KEY_RETURN, KeyboardEvent::RELEASED);
        emitKeyboard(out, KeyCode::KEY_T, KeyboardEvent::RELEASED);
    }

    inline void init()
    {
        if (SDL_NumJoysticks() > 0)
        {
            s_joystick = SDL_JoystickOpen(0);
        }
    }

    inline void handleEvent(const SDL_Event &event, std::vector<Event *> &out)
    {
        const Uint32 now = SDL_GetTicks();

        switch (event.type)
        {
        case SDL_JOYBUTTONDOWN:
            handleJoyButton(out, event.jbutton.button, SDL_PRESSED, now);
            break;
        case SDL_JOYBUTTONUP:
            handleJoyButton(out, event.jbutton.button, SDL_RELEASED, now);
            break;
        case SDL_JOYAXISMOTION:
            handleJoyAxis(out, event.jaxis.axis, event.jaxis.value, now);
            break;
        case SDL_JOYDEVICEADDED:
            if (s_joystick == nullptr)
                s_joystick = SDL_JoystickOpen(event.jdevice.which);
            break;
        case SDL_JOYDEVICEREMOVED:
            releaseEverything(out, now);
            if (s_joystick != nullptr)
            {
                SDL_JoystickClose(s_joystick);
                s_joystick = nullptr;
            }
            break;
        default:
            break;
        }
    }

    // Called once per frame, generates key-repeat presses for held directions
    // (matches the auto-repeat of a real keyboard on the desktop build).
    inline void pollRepeats(std::vector<Event *> &out)
    {
        const Uint32 now = SDL_GetTicks();

        for (int dir = 0; dir < 4; ++dir)
        {
            DirState &d = s_dirs[dir];
            if (d.held && now >= d.next_repeat_at)
            {
                emitKeyboard(out, dirKeyCode(dir), KeyboardEvent::PRESSED);
                d.next_repeat_at = now + REPEAT_INTERVAL_MS;
            }
        }
    }
}

#endif // CONSOLE_INPUT_H
