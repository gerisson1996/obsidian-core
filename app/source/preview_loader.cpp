#include "preview_loader.hpp"
#include "stb_image.h"
#include "network.hpp"
#include "installer.hpp"
#include <fstream>
#include <cstdlib>

PreviewLoader::PreviewLoader() {}

PreviewLoader::~PreviewLoader() {
    cleanup();
}

void PreviewLoader::init(SDL_Renderer* renderer) {
    m_renderer = renderer;
    Installer::ensureDirectoryExists("sdmc:/switch/nx-port-hub/previews");
}

void PreviewLoader::cleanup() {
    for (auto& pair : m_animations) {
        for (SDL_Texture* tex : pair.second.frames) {
            if (tex) {
                SDL_DestroyTexture(tex);
            }
        }
    }
    m_animations.clear();
    m_attempted.clear();
}

bool PreviewLoader::loadAnimationFromMemory(const unsigned char* data, int len, AnimationData& anim) {
    if (!data || len <= 0 || !m_renderer) return false;

    // 1. Tenta carregar como GIF animado
    int* delays = nullptr;
    int w = 0, h = 0, z = 0, comp = 0;
    unsigned char* pixels = stbi_load_gif_from_memory(data, len, &delays, &w, &h, &z, &comp, 4);

    if (pixels && z > 0) {
        anim.width = w;
        anim.height = h;
        anim.frames.reserve(z);
        anim.delays_ms.reserve(z);
        anim.total_duration_ms = 0;

        int frame_bytes = w * h * 4;
        for (int f = 0; f < z; f++) {
            SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
                pixels + (f * frame_bytes), w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
            if (surface) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, surface);
                SDL_FreeSurface(surface);
                if (tex) {
                    anim.frames.push_back(tex);
                    int delay_ms = (delays && delays[f] > 0) ? (delays[f] * 10) : 100;
                    if (delay_ms < 30) delay_ms = 100;
                    anim.delays_ms.push_back(delay_ms);
                    anim.total_duration_ms += delay_ms;
                }
            }
        }

        if (delays) free(delays);
        stbi_image_free(pixels);
        return !anim.frames.empty();
    }

    if (delays) free(delays);

    // 2. Se não for GIF animado, tenta como imagem estática (PNG/JPG) de gameplay
    pixels = stbi_load_from_memory(data, len, &w, &h, &comp, 4);
    if (pixels) {
        anim.width = w;
        anim.height = h;
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
            pixels, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
        if (surface) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, surface);
            SDL_FreeSurface(surface);
            if (tex) {
                anim.frames.push_back(tex);
                anim.delays_ms.push_back(1000);
                anim.total_duration_ms = 1000;
            }
        }
        stbi_image_free(pixels);
        return !anim.frames.empty();
    }

    return false;
}

bool PreviewLoader::loadAnimationFromFile(const std::string& filepath, AnimationData& anim) {
    std::ifstream in(filepath, std::ios::binary | std::ios::ate);
    if (!in) return false;

    std::streamsize size = in.tellg();
    if (size <= 0) return false;
    in.seekg(0, std::ios::beg);

    std::vector<unsigned char> buffer(size);
    if (!in.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return false;
    }

    return loadAnimationFromMemory(buffer.data(), buffer.size(), anim);
}

void PreviewLoader::saveCache(const std::string& port_id, const std::string& data) {
    std::string cache_path = "sdmc:/switch/nx-port-hub/previews/" + port_id + ".gif";
    std::ofstream out(cache_path, std::ios::binary);
    if (out) {
        out.write(data.data(), data.size());
    }
}

SDL_Texture* PreviewLoader::getCurrentFrame(const std::string& port_id, const std::string& preview_url) {
    if (port_id.empty()) return nullptr;

    auto it = m_animations.find(port_id);
    if (it != m_animations.end() && !it->second.frames.empty()) {
        const auto& anim = it->second;
        if (anim.frames.size() == 1 || anim.total_duration_ms <= 0) {
            return anim.frames[0];
        }

        uint32_t ticks = SDL_GetTicks() % anim.total_duration_ms;
        int accumulated = 0;
        for (size_t f = 0; f < anim.frames.size(); f++) {
            accumulated += anim.delays_ms[f];
            if (ticks < static_cast<uint32_t>(accumulated)) {
                return anim.frames[f];
            }
        }
        return anim.frames.back();
    }

    if (m_attempted[port_id]) {
        return nullptr;
    }
    m_attempted[port_id] = true;

    // 1. Tenta carregar do cache local no MicroSD
    std::string local_path = "sdmc:/switch/nx-port-hub/previews/" + port_id + ".gif";
    AnimationData anim;
    if (loadAnimationFromFile(local_path, anim)) {
        m_animations[port_id] = anim;
        return anim.frames[0];
    }

    // 2. Se não estiver em cache e houver URL válida, faz download
    if (!preview_url.empty()) {
        std::string raw_data, err;
        if (Network::fetchString(preview_url, raw_data, err, 8)) {
            if (loadAnimationFromMemory(reinterpret_cast<const unsigned char*>(raw_data.data()), raw_data.size(), anim)) {
                saveCache(port_id, raw_data);
                m_animations[port_id] = anim;
                return anim.frames[0];
            }
        }
    }

    return nullptr;
}
