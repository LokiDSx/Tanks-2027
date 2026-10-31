#ifndef SDL_RENDERER_H
#define SDL_RENDERER_H

#include "../renderer.h"
#include "../data/font_size.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <unordered_map>

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

private:
    void applyViewport();

    Size m_viewport_base_size;
    Size m_window_size{0, 0};
    bool m_widescreen = true;
    SDL_Renderer *m_renderer;
    SDL_Texture *m_texture;
    SDL_Texture *m_text_texture;
    std::unordered_map<FontSize, TTF_Font *> m_fonts;
    bool m_fullscreen;
#ifdef VITA_PORT
    SDL_Rect m_viewport{0, 0, 0, 0};
#endif
};

#endif // SDL_RENDERER_H
