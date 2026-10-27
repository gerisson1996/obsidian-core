#ifndef PREVIEW_LOADER_HPP
#define PREVIEW_LOADER_HPP

#include <SDL.h>
#include <string>
#include <vector>
#include <map>

struct AnimationData {
    std::vector<SDL_Texture*> frames;
    std::vector<int> delays_ms;
    int total_duration_ms = 0;
    int width = 0;
    int height = 0;
};

class PreviewLoader {
public:
    PreviewLoader();
    ~PreviewLoader();

    void init(SDL_Renderer* renderer);
    void cleanup();

    // Obtém textura do frame atual da animação/gameplay
    SDL_Texture* getCurrentFrame(const std::string& port_id, const std::string& preview_url);

private:
    SDL_Renderer* m_renderer = nullptr;
    std::map<std::string, AnimationData> m_animations;
    std::map<std::string, bool> m_attempted;

    bool loadAnimationFromFile(const std::string& filepath, AnimationData& anim);
    bool loadAnimationFromMemory(const unsigned char* data, int len, AnimationData& anim);
    void saveCache(const std::string& port_id, const std::string& data);
};

#endif // PREVIEW_LOADER_HPP
