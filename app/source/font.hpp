#ifndef FONT_HPP
#define FONT_HPP

#include <SDL.h>
#include <string>
#include <cstdint>

enum FontSize {
    FONT_SIZE_SMALL = 0,   // ~16px (tags, datas, paths)
    FONT_SIZE_MEDIUM = 1,  // ~22px (texto padrão, botões, descrições)
    FONT_SIZE_LARGE = 2,   // ~30px (subtítulos e títulos de cards)
    FONT_SIZE_TITLE = 3,   // ~40px (títulos principais)
    FONT_SIZE_COUNT = 4
};

class FontRenderer {
public:
    static bool init(SDL_Renderer* renderer);
    static void cleanup();

    static void drawChar(SDL_Renderer* renderer, int x, int y, char c, SDL_Color color, int scale = 2);
    static void drawText(SDL_Renderer* renderer, int x, int y, const std::string& text, SDL_Color color, int scale = 2);
    static int getTextWidth(const std::string& text, int scale = 2);
    static int getTextHeight(int scale = 2);

    static bool isTtfLoaded();
};

#endif // FONT_HPP
