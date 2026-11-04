#include "sdl_renderer.h"
#include "../data/error.h"
#include "../../appconfig.h"
#include <SDL2/SDL_image.h>
#include <iostream>
#include <algorithm>

#ifdef ANDROID_PORT
#include "touch_overlay.h"
#endif

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

    m_fullscreen = false;
}

SDLRenderer::~SDLRenderer()
{
    if (m_renderer != nullptr)
        SDL_DestroyRenderer(m_renderer);
    if (m_texture != nullptr)
        SDL_DestroyTexture(m_texture);
    for (auto &[key, cached] : m_text_cache)
        if (cached.texture != nullptr)
            SDL_DestroyTexture(cached.texture);
#ifdef PSP_PORT
    for (SDL_Texture *tile : m_tiles)
        if (tile != nullptr)
            SDL_DestroyTexture(tile);
#endif
    for (auto &[key, font] : m_fonts)
        if (font != nullptr)
            TTF_CloseFont(font);

    TTF_Quit();
    IMG_Quit();
}

void SDLRenderer::loadTexture(SDL_Window *window)
{
    m_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

#ifdef CONSOLE_PORT
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
#ifndef PSP_PORT
    m_texture = IMG_LoadTexture(m_renderer, AppConfig::texture_path.c_str());
    if (m_texture == nullptr)
    {
        throw Error("Failed to load texture: " + AppConfig::texture_path, SDL_GetError());
    }
#else
    // The PSP render driver rejects textures larger than 512x512, while the
    // atlas is 1216x512. Load the PNG as a surface and slice it into tiles
    // of at most 512x512; drawObject() renders through copyViaTiles().
    SDL_Surface *atlas = IMG_Load(AppConfig::texture_path.c_str());
    if (atlas == nullptr)
    {
        throw Error("Failed to load texture: " + AppConfig::texture_path, SDL_GetError());
    }

    m_atlas_w = atlas->w;
    m_atlas_h = atlas->h;
    const int max_tile = 512;
    const int rows = (m_atlas_h + max_tile - 1) / max_tile;
    m_tile_cols = (m_atlas_w + max_tile - 1) / max_tile;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < m_tile_cols; ++c)
        {
            const int tw = std::min(max_tile, m_atlas_w - c * max_tile);
            const int th = std::min(max_tile, m_atlas_h - r * max_tile);

            SDL_Surface *tile = SDL_CreateRGBSurfaceWithFormat(0, tw, th, 32, SDL_PIXELFORMAT_RGBA8888);
            if (tile == nullptr)
            {
                SDL_FreeSurface(atlas);
                throw Error("Failed to allocate a texture atlas tile", SDL_GetError());
            }

            SDL_Rect src = {c * max_tile, r * max_tile, tw, th};
            if (SDL_BlitSurface(atlas, &src, tile, nullptr) != 0)
            {
                SDL_FreeSurface(tile);
                SDL_FreeSurface(atlas);
                throw Error("Failed to slice the texture atlas", SDL_GetError());
            }

            SDL_Texture *tile_texture = SDL_CreateTextureFromSurface(m_renderer, tile);
            SDL_FreeSurface(tile);
            if (tile_texture == nullptr)
            {
                SDL_FreeSurface(atlas);
                throw Error("Failed to upload a texture atlas tile", SDL_GetError());
            }
            m_tiles.push_back(tile_texture);
        }
    }

    SDL_FreeSurface(atlas);
#endif
}

void SDLRenderer::clear()
{
    // Text cache bookkeeping: drop strings that have not been drawn for a
    // couple of frames (score animations produce many unique strings).
    ++m_text_tick;
    if (m_text_cache.size() > TEXT_CACHE_LIMIT)
    {
        for (auto it = m_text_cache.begin(); it != m_text_cache.end();)
        {
            if (it->second.last_used_tick + 2 < m_text_tick)
                it = m_text_cache.erase(it);
            else
                ++it;
        }
        // All entries are recent (unlikely, but possible): drop the oldest half.
        if (m_text_cache.size() > TEXT_CACHE_LIMIT)
        {
            size_t to_drop = m_text_cache.size() / 2;
            auto it = m_text_cache.begin();
            while (to_drop-- > 0 && it != m_text_cache.end())
                it = m_text_cache.erase(it);
        }
    }

    if (m_widescreen != AppConfig::widescreen)
    {
        m_widescreen = AppConfig::widescreen;
        applyViewport();
    }

#ifdef CONSOLE_PORT
    // Black bars around the aspect-ratio-corrected game viewport, grey
    // background inside the viewport. Clear ignores the viewport, so it is
    // explicitly reset and restored here.
    SDL_RenderSetViewport(m_renderer, nullptr);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    SDL_RenderSetViewport(m_renderer, &m_viewport);
    SDL_SetRenderDrawColor(m_renderer, 110, 110, 110, 255);
    SDL_Rect background = {m_offset_x, m_offset_y, m_viewport_base_size.w, m_viewport_base_size.h};
    SDL_RenderFillRect(m_renderer, &background);
#else
    SDL_SetRenderDrawColor(m_renderer, 110, 110, 110, 255);
    SDL_RenderClear(m_renderer);
#endif
}

void SDLRenderer::flush()
{
#ifdef ANDROID_PORT
    // The on-screen controls are drawn in raw window coordinates on top of
    // the finished frame: the letterbox viewport and scale are reset first
    // and restored afterwards (the next clear() resets them anyway).
    if (m_window_size.w > 0 && m_window_size.h > 0)
    {
        TouchOverlay::draw(m_renderer, m_window_size.w, m_window_size.h);
        applyViewport();
    }
#endif
    SDL_RenderPresent(m_renderer);
}

void SDLRenderer::drawObject(const Rect &texture_src, const Rect &window_dest)
{
    // m_offset_* is the manual letterbox shift on the PSP (0 elsewhere)
    SDL_Rect t = {texture_src.x, texture_src.y, texture_src.w, texture_src.h};
    SDL_Rect w = {window_dest.x + m_offset_x, window_dest.y + m_offset_y, window_dest.w, window_dest.h};
#ifdef PSP_PORT
    copyViaTiles(t, w, nullptr);
#else
    SDL_RenderCopy(m_renderer, m_texture, &t, &w);
#endif
}

void SDLRenderer::drawObject(const Rect &texture_src, const Rect &window_dest, const Color &tint)
{
    SDL_Rect t = {texture_src.x, texture_src.y, texture_src.w, texture_src.h};
    SDL_Rect w = {window_dest.x + m_offset_x, window_dest.y + m_offset_y, window_dest.w, window_dest.h};

#ifdef PSP_PORT
    copyViaTiles(t, w, &tint);
#else
    // The tint multiplies the sprite pixels (white = unchanged). The whole
    // texture is shared, so the modulation is reset right after the draw.
    SDL_SetTextureColorMod(m_texture, static_cast<Uint8>(tint.r), static_cast<Uint8>(tint.g), static_cast<Uint8>(tint.b));
    SDL_SetTextureAlphaMod(m_texture, static_cast<Uint8>(tint.a));
    SDL_RenderCopy(m_renderer, m_texture, &t, &w);
    SDL_SetTextureColorMod(m_texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(m_texture, 255);
#endif
}

#ifdef PSP_PORT
void SDLRenderer::copyViaTiles(const SDL_Rect &src, const SDL_Rect &dst, const Color *tint)
{
    if (m_tiles.empty() || src.w <= 0 || src.h <= 0)
        return;

    const int max_tile = 512;
    const int rows = static_cast<int>(m_tiles.size()) / m_tile_cols;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < m_tile_cols; ++c)
        {
            // Atlas-space rectangle covered by this tile
            SDL_Rect tile = {c * max_tile, r * max_tile,
                             std::min(max_tile, m_atlas_w - c * max_tile),
                             std::min(max_tile, m_atlas_h - r * max_tile)};

            // Intersection of the requested sprite rect with the tile
            const int x0 = std::max(src.x, tile.x);
            const int y0 = std::max(src.y, tile.y);
            const int x1 = std::min(src.x + src.w, tile.x + tile.w);
            const int y1 = std::min(src.y + src.h, tile.y + tile.h);
            if (x0 >= x1 || y0 >= y1)
                continue;

            // Source part inside the tile texture
            SDL_Rect part = {x0 - tile.x, y0 - tile.y, x1 - x0, y1 - y0};

            // Matching destination part, keeping the stretch proportions
            SDL_Rect out;
            out.x = dst.x + static_cast<int>((x0 - src.x) * static_cast<long long>(dst.w) / src.w);
            out.y = dst.y + static_cast<int>((y0 - src.y) * static_cast<long long>(dst.h) / src.h);
            out.w = std::max(1, static_cast<int>((x1 - x0) * static_cast<long long>(dst.w) / src.w));
            out.h = std::max(1, static_cast<int>((y1 - y0) * static_cast<long long>(dst.h) / src.h));

            SDL_Texture *tile_texture = m_tiles[r * m_tile_cols + c];
            if (tint != nullptr)
            {
                SDL_SetTextureColorMod(tile_texture, static_cast<Uint8>(tint->r), static_cast<Uint8>(tint->g), static_cast<Uint8>(tint->b));
                SDL_SetTextureAlphaMod(tile_texture, static_cast<Uint8>(tint->a));
            }
            SDL_RenderCopy(m_renderer, tile_texture, &part, &out);
            if (tint != nullptr)
            {
                SDL_SetTextureColorMod(tile_texture, 255, 255, 255);
                SDL_SetTextureAlphaMod(tile_texture, 255);
            }
        }
    }
}
#endif

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

#ifdef PSP_PORT
    // The PSP SDL driver always renders at the top-left corner and ignores
    // the viewport position (worse, its x/y are misused as the scissor
    // origin, which clipped the left/top part of the picture). Keep the
    // viewport full-screen and apply the letterbox offset manually in the
    // draw calls through m_offset_x/y.
    float scale_x, scale_y;
    if (m_widescreen)
    {
        scale_x = width / (float)m_viewport_base_size.w;
        scale_y = height / (float)m_viewport_base_size.h;
        m_offset_x = 0;
        m_offset_y = 0;
    }
    else
    {
        float scale = std::min(width / (float)m_viewport_base_size.w, height / (float)m_viewport_base_size.h);
        if (scale < 0.1)
            return;
        scale_x = scale_y = scale;
        m_offset_x = static_cast<int>((width / scale - m_viewport_base_size.w) / 2.0);
        m_offset_y = static_cast<int>((height / scale - m_viewport_base_size.h) / 2.0);
    }

    SDL_RenderSetScale(m_renderer, scale_x, scale_y);
    // Full-screen viewport in logical units (SDL multiplies it by the scale)
    m_viewport = {0, 0, static_cast<int>(width / scale_x + 0.5f), static_cast<int>(height / scale_y + 0.5f)};
    SDL_RenderSetViewport(m_renderer, &m_viewport);
#else
    if (m_widescreen)
    {
        // Stretch to fill the entire screen. The game's native 464x416 aspect
        // ratio doesn't match Vita's 16:9, so X and Y are scaled independently
        // to eliminate black bars (slight horizontal stretch, typical for NES ports).
        SDL_Rect viewport = {0, 0, width, height};
        SDL_RenderSetScale(m_renderer, width / (float)m_viewport_base_size.w, height / (float)m_viewport_base_size.h);
        SDL_RenderSetViewport(m_renderer, &viewport);
#ifdef CONSOLE_PORT
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
#ifdef CONSOLE_PORT
        m_viewport = viewport;
#endif
    }
#endif
}

void SDLRenderer::drawText(const Point &start, std::string text, Color text_color, FontSize font_size)
{
    if (m_fonts.find(font_size) == m_fonts.end())
        m_fonts[font_size] = TTF_OpenFont(AppConfig::font_path.c_str(), static_cast<int>(font_size));

    TTF_Font *font = m_fonts[font_size];
    if (font == nullptr)
        return;

    TextCacheKey key;
    key.text = std::move(text);
    key.r = text_color.r;
    key.g = text_color.g;
    key.b = text_color.b;
    key.a = text_color.a;
    key.size = static_cast<int>(font_size);

    auto it = m_text_cache.find(key);
    if (it == m_text_cache.end())
    {
        SDL_Color sdl_text_color = {static_cast<Uint8>(key.r), static_cast<Uint8>(key.g), static_cast<Uint8>(key.b), static_cast<Uint8>(key.a)};
        SDL_Surface *text_surface = TTF_RenderText_Solid(font, key.text.c_str(), sdl_text_color);

        if (text_surface == nullptr)
            return;

        CachedTextTexture cached;
        cached.texture = SDL_CreateTextureFromSurface(m_renderer, text_surface);
        cached.width = text_surface->w;
        cached.height = text_surface->h;
        SDL_FreeSurface(text_surface);
        if (cached.texture == nullptr)
            return;

#ifdef PSP_PORT
        // The console downscales the 464x416 picture to fit 272 lines of the
        // PSP screen (~0.65x); nearest filtering drops rows of small glyphs
        // and made the text look torn. Linear keeps it intact (the driver
        // switches GU_NEAREST/GU_LINEAR from the texture's scale mode).
        SDL_SetTextureScaleMode(cached.texture, SDL_ScaleModeLinear);
#endif

        it = m_text_cache.emplace(std::move(key), std::move(cached)).first;
    }

    CachedTextTexture &cached = it->second;
    cached.last_used_tick = m_text_tick;

    SDL_Rect window_dest;
    if (start.x < 0)
        window_dest.x = (m_viewport_base_size.w - cached.width) / 2;
    else
        window_dest.x = start.x;

    if (start.y < 0)
        window_dest.y = (m_viewport_base_size.h - cached.height) / 2;
    else
        window_dest.y = start.y;

    window_dest.w = cached.width;
    window_dest.h = cached.height;
    window_dest.x += m_offset_x;
    window_dest.y += m_offset_y;

    SDL_RenderCopy(m_renderer, cached.texture, NULL, &window_dest);
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
    SDL_Rect r = {rect.x + m_offset_x, rect.y + m_offset_y, rect.w, rect.h};
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
