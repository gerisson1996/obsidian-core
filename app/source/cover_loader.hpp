#ifndef COVER_LOADER_HPP
#define COVER_LOADER_HPP

#include <SDL.h>
#include <string>
#include <map>

class CoverLoader {
public:
    CoverLoader();
    ~CoverLoader();

    void init(SDL_Renderer* renderer);
    void cleanup();

    // Obtém textura da capa (carrega do cache do SD ou baixa da web)
    SDL_Texture* getCover(const std::string& port_id, const std::string& cover_url);

private:
    SDL_Renderer* m_renderer = nullptr;
    std::map<std::string, SDL_Texture*> m_textures;

    SDL_Texture* loadTextureFromFile(const std::string& filepath);
    SDL_Texture* loadTextureFromMemory(const unsigned char* data, int len);
    void saveCache(const std::string& port_id, const std::string& ext, const std::string& data);
};

#endif // COVER_LOADER_HPP
