#ifndef TOUCH_OVERLAY_H
#define TOUCH_OVERLAY_H

// On-screen touch controls for the Android port.
//
// The whole game logic is keyboard driven, so the overlay translates touches
// into the same synthetic keyboard events that console_input.h generates for
// the physical gamepad: directions go through ConsoleInput::updateDir (which
// also provides the key repeat used for menu navigation), FIRE emits both
// RCTRL and SPACE, PAUSE is RETURN and BACK is ESCAPE.
//
// Layout (landscape): a big D-pad in the bottom-left corner, a vertical stack
// of round buttons in the bottom-right corner, everything drawn
// semi-transparent on top of the finished frame by TouchOverlay::draw().

#include "console_input.h"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <utility>
#include <vector>

namespace TouchOverlay
{
    enum Element
    {
        EL_NONE = 0,
        EL_DPAD,
        EL_FIRE,
        EL_PAUSE,
        EL_BACK,
        EL_COUNT
    };

    struct Touch
    {
        Element el = EL_NONE;
        int dir = -1; // ConsoleInput::DIR_* while el == EL_DPAD
    };

    inline std::map<SDL_FingerID, Touch> s_fingers;
    inline int s_held[EL_COUNT] = {0, 0, 0, 0, 0};

    // SDL_FingerID is Sint64; mouse input (emulator, desktop testing) is
    // tracked as a pseudo-finger with this id.
    constexpr SDL_FingerID MOUSE_FINGER = -1;

    struct Layout
    {
        SDL_Point dpad_center{0, 0};
        int dpad_radius = 0;
        SDL_Rect fire{0, 0, 0, 0};
        SDL_Rect pause{0, 0, 0, 0};
        SDL_Rect back{0, 0, 0, 0};
        int fire_radius = 0;
        int small_radius = 0;
    };

    inline SDL_Rect circleRect(int cx, int cy, int r)
    {
        return SDL_Rect{cx - r, cy - r, r * 2, r * 2};
    }

    inline Layout computeLayout(int w, int h)
    {
        Layout l;

        const int margin = h / 22;
        // Testers asked for a bigger D-pad: ~42% of the screen height (was 32%)
        // so the thumb rests comfortably on it on a 6"+ phone.
        const int dpad_side = std::min(h * 42 / 100, w * 40 / 100);
        l.dpad_radius = dpad_side / 2;
        l.dpad_center = {margin + l.dpad_radius, h - margin - l.dpad_radius};

        l.fire_radius = h * 75 / 1000;
        l.small_radius = h * 55 / 1000;
        const int gap = h * 25 / 1000;

        const int cx = w - margin - l.small_radius;
        int cy = h - margin - l.fire_radius;
        l.fire = circleRect(w - margin - l.fire_radius, cy, l.fire_radius);

        cy -= l.fire_radius + gap + l.small_radius;
        l.pause = circleRect(cx, cy, l.small_radius);

        cy -= l.small_radius + gap + l.small_radius;
        l.back = circleRect(cx, cy, l.small_radius);

        return l;
    }

    inline bool rectContains(const SDL_Rect &r, int x, int y)
    {
        return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    }

    // Round buttons accept a slightly larger hit area than they occupy
    // visually, so taps near the edge still register.
    inline bool circleContains(int cx, int cy, int r, int x, int y)
    {
        const int dx = x - cx;
        const int dy = y - cy;
        const int hit = r + r / 3;
        return dx * dx + dy * dy <= hit * hit;
    }

    inline std::pair<Element, int> hitTestAt(const Layout &l, int w, int h, int x, int y)
    {
        (void)w;
        (void)h;

        const int ddx = x - l.dpad_center.x;
        const int ddy = y - l.dpad_center.y;
        if (ddx * ddx + ddy * ddy <= l.dpad_radius * l.dpad_radius)
        {
            const int dead = l.dpad_radius / 4;
            if (std::abs(ddx) <= dead && std::abs(ddy) <= dead)
                return {EL_DPAD, -1};

            if (std::abs(ddx) > std::abs(ddy))
                return {EL_DPAD, ddx > 0 ? ConsoleInput::DIR_RIGHT : ConsoleInput::DIR_LEFT};
            return {EL_DPAD, ddy > 0 ? ConsoleInput::DIR_DOWN : ConsoleInput::DIR_UP};
        }

        if (circleContains(l.fire.x + l.fire.w / 2, l.fire.y + l.fire.h / 2, l.fire_radius, x, y))
            return {EL_FIRE, -1};
        if (circleContains(l.pause.x + l.pause.w / 2, l.pause.y + l.pause.h / 2, l.small_radius, x, y))
            return {EL_PAUSE, -1};
        if (circleContains(l.back.x + l.back.w / 2, l.back.y + l.back.h / 2, l.small_radius, x, y))
            return {EL_BACK, -1};

        return {EL_NONE, -1};
    }

    inline void emitButton(std::vector<Event *> &out, Element el, KeyboardEvent::KeyState state)
    {
        switch (el)
        {
        case EL_FIRE:
            ConsoleInput::emitFireAndConfirm(out, state);
            break;
        case EL_PAUSE:
            ConsoleInput::emitKeyboard(out, KeyCode::KEY_RETURN, state);
            break;
        case EL_BACK:
            ConsoleInput::emitKeyboard(out, KeyCode::KEY_ESCAPE, state);
            break;
        default:
            break;
        }
    }

    inline void pressTouch(std::vector<Event *> &out, const Touch &t, Uint32 now)
    {
        if (t.el == EL_DPAD)
        {
            if (t.dir >= 0)
                ConsoleInput::updateDir(out, t.dir, true, ConsoleInput::s_dirs[t.dir].stick, now);
        }
        else if (t.el != EL_NONE)
        {
            if (s_held[t.el]++ == 0)
                emitButton(out, t.el, KeyboardEvent::PRESSED);
        }
    }

    inline void releaseTouch(std::vector<Event *> &out, const Touch &t, Uint32 now)
    {
        if (t.el == EL_DPAD)
        {
            if (t.dir >= 0)
                ConsoleInput::updateDir(out, t.dir, false, ConsoleInput::s_dirs[t.dir].stick, now);
        }
        else if (t.el != EL_NONE)
        {
            if (s_held[t.el] > 0 && --s_held[t.el] == 0)
                emitButton(out, t.el, KeyboardEvent::RELEASED);
        }
    }

    // Only one finger may own the D-pad at a time, otherwise two fingers
    // would fight over the shared ConsoleInput direction state.
    inline bool dpadOwnedByOther(SDL_FingerID id)
    {
        for (const auto &pair : s_fingers)
            if (pair.first != id && pair.second.el == EL_DPAD)
                return true;
        return false;
    }

    inline void setFinger(std::vector<Event *> &out, SDL_FingerID id, Element el, int dir, Uint32 now)
    {
        if (el == EL_DPAD && dir >= 0 && dpadOwnedByOther(id))
        {
            el = EL_NONE;
            dir = -1;
        }

        auto it = s_fingers.find(id);
        Touch prev = (it != s_fingers.end()) ? it->second : Touch{};

        if (prev.el == el && prev.dir == dir)
            return;

        // Diagnostic: shows what each finger is bound to (logcat only),
        // makes multitouch problems traceable.
        SDL_Log("Touch %lld: %d/%d -> %d/%d", static_cast<long long>(id),
                static_cast<int>(prev.el), prev.dir, static_cast<int>(el), dir);

        releaseTouch(out, prev, now);
        Touch next{el, dir};
        pressTouch(out, next, now);

        if (it != s_fingers.end())
            it->second = next;
        else
            s_fingers.emplace(id, next);
    }

    inline void releaseFinger(std::vector<Event *> &out, SDL_FingerID id, Uint32 now)
    {
        auto it = s_fingers.find(id);
        if (it == s_fingers.end())
            return;
        SDL_Log("Touch %lld: release %d/%d", static_cast<long long>(id),
                static_cast<int>(it->second.el), it->second.dir);
        releaseTouch(out, it->second, now);
        s_fingers.erase(it);
    }

    // Drops every held direction and button (focus loss, app going to the
    // background). Directions that a physical joystick is holding are kept:
    // ConsoleInput::updateDir only clears the touch (dpad) half of the state.
    inline void releaseAll(std::vector<Event *> &out, Uint32 now)
    {
        for (auto &pair : s_fingers)
            releaseTouch(out, pair.second, now);
        s_fingers.clear();

        for (int dir = 0; dir < 4; ++dir)
            ConsoleInput::updateDir(out, dir, false, ConsoleInput::s_dirs[dir].stick, now);

        for (int el = EL_FIRE; el < EL_COUNT; ++el)
        {
            if (s_held[el] > 0)
            {
                s_held[el] = 0;
                emitButton(out, static_cast<Element>(el), KeyboardEvent::RELEASED);
            }
        }
    }

    inline void handleEvent(const SDL_Event &event, std::vector<Event *> &out, int window_w, int window_h)
    {
        const Uint32 now = SDL_GetTicks();

        switch (event.type)
        {
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION:
        case SDL_FINGERUP:
        {
            if (window_w <= 0 || window_h <= 0)
                break;

            const SDL_FingerID id = event.tfinger.fingerId;
            const int x = static_cast<int>(event.tfinger.x * window_w);
            const int y = static_cast<int>(event.tfinger.y * window_h);

            // Diagnostic: proves that touches arrive and shows the coordinate
            // space they are mapped into (logcat only).
            if (event.type == SDL_FINGERDOWN)
                SDL_Log("Touch down: (%d,%d) window %dx%d", x, y, window_w, window_h);

            if (event.type == SDL_FINGERUP)
            {
                releaseFinger(out, id, now);
                break;
            }

            // A finger that starts outside every control stays untracked
            // until it is lifted; only an already tracked finger can move
            // between controls.
            if (event.type == SDL_FINGERMOTION && s_fingers.find(id) == s_fingers.end())
                break;

            Layout l = computeLayout(window_w, window_h);
            auto hit = hitTestAt(l, window_w, window_h, x, y);
            setFinger(out, id, hit.first, hit.second, now);
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONUP:
        {
            if (window_w <= 0 || window_h <= 0)
                break;

            // Mouse events synthesized from touch carry SDL_TOUCH_MOUSEID.
            // Every finger already arrives as its own SDL_Finger* event, so
            // these duplicates are dropped: the pseudo-mouse record sharing
            // the finger map with real fingers is what made multitouch
            // unreliable (a second finger could hijack the D-pad binding).
            const Uint32 which = (event.type == SDL_MOUSEMOTION)
                                     ? static_cast<Uint32>(event.motion.which)
                                     : static_cast<Uint32>(event.button.which);
            if (which == SDL_TOUCH_MOUSEID)
                break;

            const bool tracked = s_fingers.find(MOUSE_FINGER) != s_fingers.end();

            if (event.type != SDL_MOUSEMOTION && event.button.button != SDL_BUTTON_LEFT)
                break;
            if (event.type == SDL_MOUSEMOTION && !tracked)
                break;

            int x = 0, y = 0;
            if (event.type == SDL_MOUSEMOTION)
            {
                x = static_cast<int>(event.motion.x);
                y = static_cast<int>(event.motion.y);
            }
            else
            {
                x = static_cast<int>(event.button.x);
                y = static_cast<int>(event.button.y);
            }

            if (event.type == SDL_MOUSEBUTTONUP)
            {
                releaseFinger(out, MOUSE_FINGER, now);
                break;
            }

            Layout l = computeLayout(window_w, window_h);
            auto hit = hitTestAt(l, window_w, window_h, x, y);
            setFinger(out, MOUSE_FINGER, hit.first, hit.second, now);
            break;
        }

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                releaseAll(out, now);
            break;

        case SDL_APP_WILLENTERBACKGROUND:
            releaseAll(out, now);
            break;

        default:
            break;
        }
    }

    // --- drawing -----------------------------------------------------------

    inline void fillCircle(SDL_Renderer *r, int cx, int cy, int radius)
    {
        for (int dy = -radius; dy <= radius; ++dy)
        {
            const float f = static_cast<float>(radius * radius - dy * dy);
            const int dx = static_cast<int>(std::sqrt(f) + 0.5f);
            SDL_Rect strip = {cx - dx, cy + dy, dx * 2, 1};
            SDL_RenderFillRect(r, &strip);
        }
    }

    inline void drawButton(SDL_Renderer *r, const SDL_Rect &rect, int radius, bool held,
                           Uint8 cr, Uint8 cg, Uint8 cb)
    {
        const int cx = rect.x + rect.w / 2;
        const int cy = rect.y + rect.h / 2;

        SDL_SetRenderDrawColor(r, cr, cg, cb, held ? 140 : 70);
        fillCircle(r, cx, cy, radius);
    }

    inline void draw(SDL_Renderer *renderer, int w, int h)
    {
        if (w <= 0 || h <= 0)
            return;

        SDL_BlendMode prev_blend = SDL_BLENDMODE_NONE;
        SDL_GetRenderDrawBlendMode(renderer, &prev_blend);

        // The overlay lives in raw window coordinates, outside of the
        // letterbox viewport and its scale (restored by the caller).
        SDL_RenderSetViewport(renderer, nullptr);
        SDL_RenderSetScale(renderer, 1.0f, 1.0f);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        const Layout l = computeLayout(w, h);

        // D-pad: translucent disc, four directional pads, pressed one lit up.
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 40);
        fillCircle(renderer, l.dpad_center.x, l.dpad_center.y, l.dpad_radius);

        const int arm = l.dpad_radius * 62 / 100;
        const int dist = l.dpad_radius * 55 / 100;

        const bool held_dir[4] = {
            ConsoleInput::s_dirs[ConsoleInput::DIR_UP].held,
            ConsoleInput::s_dirs[ConsoleInput::DIR_DOWN].held,
            ConsoleInput::s_dirs[ConsoleInput::DIR_LEFT].held,
            ConsoleInput::s_dirs[ConsoleInput::DIR_RIGHT].held,
        };

        const SDL_Point arm_pos[4] = {
            {l.dpad_center.x, l.dpad_center.y - dist},
            {l.dpad_center.x, l.dpad_center.y + dist},
            {l.dpad_center.x - dist, l.dpad_center.y},
            {l.dpad_center.x + dist, l.dpad_center.y},
        };

        for (int i = 0; i < 4; ++i)
        {
            SDL_Rect arm_rect = {arm_pos[i].x - arm / 2, arm_pos[i].y - arm / 2, arm, arm};
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, held_dir[i] ? 130 : 70);
            SDL_RenderFillRect(renderer, &arm_rect);
        }

        const int center_sq = l.dpad_radius * 40 / 100;
        SDL_Rect center = {l.dpad_center.x - center_sq / 2, l.dpad_center.y - center_sq / 2, center_sq, center_sq};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 55);
        SDL_RenderFillRect(renderer, &center);

        // Round action buttons with simple icons.
        drawButton(renderer, l.fire, l.fire_radius, s_held[EL_FIRE] > 0, 220, 70, 70);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, s_held[EL_FIRE] > 0 ? 220 : 150);
        fillCircle(renderer, l.fire.x + l.fire.w / 2, l.fire.y + l.fire.h / 2, l.fire_radius * 45 / 100);

        drawButton(renderer, l.pause, l.small_radius, s_held[EL_PAUSE] > 0, 70, 120, 220);
        {
            const int cx = l.pause.x + l.pause.w / 2;
            const int cy = l.pause.y + l.pause.h / 2;
            const int bar_w = l.small_radius / 4;
            const int bar_h = l.small_radius;
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
            SDL_Rect left_bar = {cx - bar_w - bar_w / 2, cy - bar_h / 2, bar_w, bar_h};
            SDL_Rect right_bar = {cx + bar_w / 2, cy - bar_h / 2, bar_w, bar_h};
            SDL_RenderFillRect(renderer, &left_bar);
            SDL_RenderFillRect(renderer, &right_bar);
        }

        drawButton(renderer, l.back, l.small_radius, s_held[EL_BACK] > 0, 100, 100, 100);
        {
            const int cx = l.back.x + l.back.w / 2;
            const int cy = l.back.y + l.back.h / 2;
            const int shaft_h = l.small_radius / 4;
            const int shaft_w = l.small_radius;
            const int head_w = l.small_radius / 4;
            const int head_h = l.small_radius;
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
            SDL_Rect shaft = {cx - shaft_w / 2, cy - shaft_h / 2, shaft_w, shaft_h};
            SDL_Rect head = {cx - shaft_w / 2, cy - head_h / 2, head_w, head_h};
            SDL_RenderFillRect(renderer, &shaft);
            SDL_RenderFillRect(renderer, &head);
        }

        SDL_SetRenderDrawBlendMode(renderer, prev_blend);
    }
}

#endif // TOUCH_OVERLAY_H
