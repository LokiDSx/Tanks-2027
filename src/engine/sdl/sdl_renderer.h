#ifndef SDL_RENDERER_H
#define SDL_RENDERER_H

#include "../renderer.h"
#include "../data/font_size.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <map>
#include <string>
#include <unordered_map>
#ifdef PSP_PORT
#include <vector>
#endif

class SDLRenderer : public Renderer
{
public:
    SDLRenderer(Size viewport_base_size);
    ~SDLRenderer();

    void clear() override;
    void flush() override;
    void drawObject(const Rect &texture_src, const Rect &window_dest) override;
    void drawObject(const Rect &texture_src, const Rect &window_dest, const Color &tint) override;
    void drawText(const Point &start, std::string text, Color text_color, FontSize font_size) override;
    Size measureText(const std::string &text, FontSize font_size) override;
    void drawRect(const Rect &rect, Color rect_color, bool fill = false) override;


    void loadTexture(SDL_Window *window);
    void toggleFullscreen(SDL_Window *window);
    void setViewportForWindowSize(Size window_size);
    void setWidescreen(bool widescreen);
    Size windowSize() const { return m_window_size; }

    // Frame rate of the main loop. SDLEngine updates it once per probe and
    // flush() shows it on Android, so a screenshot of the phone is enough to
    // tell whether input problems coincide with frame drops.
    static inline double s_fps = 0.0;

private:
    void applyViewport();

    Size m_viewport_base_size;
    Size m_window_size{0, 0};
    bool m_widescreen = true;
    SDL_Renderer *m_renderer;
    SDL_Texture *m_texture;
    std::unordered_map<FontSize, TTF_Font *> m_fonts;
    bool m_fullscreen;

    // Rasterized strings are cached: re-rendering the same text with TTF and
    // re-uploading a texture every frame is very slow on the PSP (and forces
    // mid-frame command queue flushes when the single text texture is
    // destroyed), which made small text shimmer. Key = string + color + size.
    struct CachedTextTexture
    {
        SDL_Texture *texture = nullptr;
        int width = 0;
        int height = 0;
        Uint32 last_used_tick = 0;
    };

    struct TextCacheKey
    {
        std::string text;
        unsigned int r = 0, g = 0, b = 0, a = 0;
        int size = 0;

        bool operator<(const TextCacheKey &other) const
        {
            if (size != other.size) return size < other.size;
            if (r != other.r) return r < other.r;
            if (g != other.g) return g < other.g;
            if (b != other.b) return b < other.b;
            if (a != other.a) return a < other.a;
            return text < other.text;
        }
    };

    std::map<TextCacheKey, CachedTextTexture> m_text_cache;
    Uint32 m_text_tick = 0;

    // Animations like the score count generate many unique strings; evict
    // entries unused for a couple of frames once the cache outgrows this.
    static constexpr size_t TEXT_CACHE_LIMIT = 96;
    // Letterbox offset in logical units; nonzero only on the PSP, whose SDL
    // driver draws at the top-left corner and ignores the viewport position
    // (see applyViewport), so the centering is applied manually.
    int m_offset_x = 0;
    int m_offset_y = 0;
#ifdef CONSOLE_PORT
    SDL_Rect m_viewport{0, 0, 0, 0};
#endif
#ifdef PSP_PORT
    // The PSP render driver rejects textures larger than 512x512, so the
    // 1216x512 atlas is sliced into tiles at load time and drawn tile by tile.
    std::vector<SDL_Texture *> m_tiles;
    int m_tile_cols = 0;
    int m_atlas_w = 0;
    int m_atlas_h = 0;

    void copyViaTiles(const SDL_Rect &src, const SDL_Rect &dst, const Color *tint);
#endif
};

#endif // SDL_RENDERER_H
