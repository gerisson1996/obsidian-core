#ifndef FONT_HPP
#define FONT_HPP

#include <SDL.h>
#include <string>
#include <cstdint>

// Fonte básica 8x8 padrão para homebrew
extern const uint8_t font8x8_basic[128][8];

class FontRenderer {
public:
    static void drawChar(SDL_Renderer* renderer, int x, int y, char c, SDL_Color color, int scale = 2);
    static void drawText(SDL_Renderer* renderer, int x, int y, const std::string& text, SDL_Color color, int scale = 2);
    static int getTextWidth(const std::string& text, int scale = 2);
    static int getTextHeight(int scale = 2);
};

#endif // FONT_HPP
