#include "sdl_renderer.h"
#include "../data/error.h"
#include "../../appconfig.h"
#include <SDL2/SDL_image.h>
#include <iostream>
#include <algorithm>

SDLRenderer::SDLRenderer(Size viewport_base_size)
    : m_viewport_base_size(viewport_base_size)
{
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
    {
        throw Error("SDL IMG could not initialize!", SDL_GetError());
    }

    if (TTF_Init() == -1)
    {
        throw Error("SDL TTF could not initialize!", SDL_GetError());
    }

    m_texture = nullptr;
    m_renderer = nullptr;
    m_text_texture = nullptr;

    m_fullscreen = false;
}

SDLRenderer::~SDLRenderer()
{
    if (m_renderer != nullptr)
        SDL_DestroyRenderer(m_renderer);
    if (m_texture != nullptr)
        SDL_DestroyTexture(m_texture);
    if (m_text_texture != nullptr)
        SDL_DestroyTexture(m_text_texture);
    for (auto &[key, font] : m_fonts)
        if (font != nullptr)
            TTF_CloseFont(font);

    TTF_Quit();
    IMG_Quit();
}

void SDLRenderer::loadTexture(SDL_Window *window)
{
    m_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

#ifdef VITA_PORT
    if (m_renderer == nullptr)
    {
        m_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }
    if (m_renderer == nullptr)
    {
        m_renderer = SDL_CreateRenderer(window, -1, 0);
    }
#endif

    // load surface
    if (m_renderer == nullptr)
    {
        throw Error("SDL Renderer could not be created!", SDL_GetError());
    }
    m_texture = IMG_LoadTexture(m_renderer, AppConfig::texture_path.c_str());
    if (m_texture == nullptr)
    {
        throw Error("Failed to load texture: " + AppConfig::texture_path, SDL_GetError());
    }
}

void SDLRenderer::clear()
{
    if (m_widescreen != AppConfig::widescreen)
    {
        m_widescreen = AppConfig::widescreen;
        applyViewport();
    }

#ifdef VITA_PORT
    // Black bars around the aspect-ratio-corrected game viewport, grey
    // background inside the viewport. Clear ignores the viewport, so it is
    // explicitly reset and restored here.
    SDL_RenderSetViewport(m_renderer, nullptr);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    SDL_RenderSetViewport(m_renderer, &m_viewport);
    SDL_SetRenderDrawColor(m_renderer, 110, 110, 110, 255);
    SDL_Rect background = {0, 0, m_viewport_base_size.w, m_viewport_base_size.h};
    SDL_RenderFillRect(m_renderer, &background);
#else
    SDL_SetRenderDrawColor(m_renderer, 110, 110, 110, 255);
    SDL_RenderClear(m_renderer);
#endif
}

void SDLRenderer::flush()
{
    SDL_RenderPresent(m_renderer);
}

void SDLRenderer::drawObject(const Rect &texture_src, const Rect &window_dest)
{
    SDL_Rect t = {texture_src.x, texture_src.y, texture_src.w, texture_src.h};
    SDL_Rect w = {window_dest.x, window_dest.y, window_dest.w, window_dest.h};
    SDL_RenderCopy(m_renderer, m_texture, &t, &w);
}

void SDLRenderer::drawObject(const Rect &texture_src, const Rect &window_dest, const Color &tint)
{
    SDL_Rect t = {texture_src.x, texture_src.y, texture_src.w, texture_src.h};
    SDL_Rect w = {window_dest.x, window_dest.y, window_dest.w, window_dest.h};

    // The tint multiplies the sprite pixels (white = unchanged). The whole
    // texture is shared, so the modulation is reset right after the draw.
    SDL_SetTextureColorMod(m_texture, static_cast<Uint8>(tint.r), static_cast<Uint8>(tint.g), static_cast<Uint8>(tint.b));
    SDL_SetTextureAlphaMod(m_texture, static_cast<Uint8>(tint.a));
    SDL_RenderCopy(m_renderer, m_texture, &t, &w);
    SDL_SetTextureColorMod(m_texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(m_texture, 255);
}

void SDLRenderer::setViewportForWindowSize(Size window_size)
{
    if (window_size.w <= 0 || window_size.h <= 0)
        return;

    m_window_size = window_size;
    applyViewport();
}

void SDLRenderer::setWidescreen(bool widescreen)
{
    if (m_widescreen == widescreen)
        return;

    m_widescreen = widescreen;
    applyViewport();
}

void SDLRenderer::applyViewport()
{
    int width = m_window_size.w;
    int height = m_window_size.h;

    if (width <= 0 || height <= 0)
        return;

    if (m_widescreen)
    {
        // Stretch to fill the entire screen. The game's native 464x416 aspect
        // ratio doesn't match Vita's 16:9, so X and Y are scaled independently
        // to eliminate black bars (slight horizontal stretch, typical for NES ports).
        SDL_Rect viewport = {0, 0, width, height};
        SDL_RenderSetScale(m_renderer, width / (float)m_viewport_base_size.w, height / (float)m_viewport_base_size.h);
        SDL_RenderSetViewport(m_renderer, &viewport);
#ifdef VITA_PORT
        m_viewport = viewport;
#endif
    }
    else
    {
        // Calculate the scale to maintain aspect ratio
        float scale = std::min(width / (float)m_viewport_base_size.w, height / (float)m_viewport_base_size.h);
        if (scale < 0.1)
            return;

        SDL_Rect viewport;
        viewport.x = (width / scale - m_viewport_base_size.w) / 2.0;
        viewport.y = (height / scale - m_viewport_base_size.h) / 2.0;
        if (viewport.x < 0)
            viewport.x = 0;
        if (viewport.y < 0)
            viewport.y = 0;
        viewport.w = m_viewport_base_size.w;
        viewport.h = m_viewport_base_size.h;

        SDL_RenderSetScale(m_renderer, scale, scale);
        SDL_RenderSetViewport(m_renderer, &viewport);
#ifdef VITA_PORT
        m_viewport = viewport;
#endif
    }
}

void SDLRenderer::drawText(const Point &start, std::string text, Color text_color, FontSize font_size)
{
    if (m_fonts.find(font_size) == m_fonts.end())
        m_fonts[font_size] = TTF_OpenFont(AppConfig::font_path.c_str(), static_cast<int>(font_size));

    TTF_Font *font = m_fonts[font_size];
    if (font == nullptr)
        return;
        
    if (m_text_texture != nullptr)
        SDL_DestroyTexture(m_text_texture);

    SDL_Color sdl_text_color = {static_cast<Uint8>(text_color.r), static_cast<Uint8>(text_color.g), static_cast<Uint8>(text_color.b), static_cast<Uint8>(text_color.a)};
    SDL_Surface *text_surface = TTF_RenderText_Solid(font, text.c_str(), sdl_text_color);

    if (text_surface == nullptr)
        return;

    m_text_texture = SDL_CreateTextureFromSurface(m_renderer, text_surface);
    int text_width = text_surface->w;
    int text_height = text_surface->h;
    SDL_FreeSurface(text_surface);
    if (m_text_texture == nullptr)
        return;

    SDL_Rect window_dest;
    if (start.x < 0)
        window_dest.x = (m_viewport_base_size.w - text_width) / 2;
    else
        window_dest.x = start.x;

    if (start.y < 0)
        window_dest.y = (m_viewport_base_size.h - text_height) / 2;
    else
        window_dest.y = start.y;

    window_dest.w = text_width;
    window_dest.h = text_height;

    SDL_RenderCopy(m_renderer, m_text_texture, NULL, &window_dest);
}

Size SDLRenderer::measureText(const std::string &text, FontSize font_size)
{
    if (m_fonts.find(font_size) == m_fonts.end())
        m_fonts[font_size] = TTF_OpenFont(AppConfig::font_path.c_str(), static_cast<int>(font_size));

    TTF_Font *font = m_fonts[font_size];
    if (font == nullptr)
        return {0, 0};

    int w = 0, h = 0;
    if (TTF_SizeText(font, text.c_str(), &w, &h) != 0)
        return {0, 0};

    return {w, h};
}

void SDLRenderer::drawRect(const Rect &rect, Color rect_color, bool fill)
{
    SDL_Rect r = {rect.x, rect.y, rect.w, rect.h};
    SDL_SetRenderDrawColor(m_renderer, rect_color.r, rect_color.g, rect_color.b, rect_color.a);

    if (fill)
        SDL_RenderFillRect(m_renderer, &r);
    else
        SDL_RenderDrawRects(m_renderer, &r, 1);
}

void SDLRenderer::toggleFullscreen(SDL_Window *window)
{
    m_fullscreen = !m_fullscreen;

    if (m_fullscreen)
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    else
        SDL_SetWindowFullscreen(window, 0);
}
