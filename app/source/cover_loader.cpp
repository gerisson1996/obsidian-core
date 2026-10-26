#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "cover_loader.hpp"
#include "network.hpp"
#include "installer.hpp"
#include <fstream>
#include <sstream>

CoverLoader::CoverLoader() {}

CoverLoader::~CoverLoader() {
    cleanup();
}

void CoverLoader::init(SDL_Renderer* renderer) {
    m_renderer = renderer;
    Installer::ensureDirectoryExists("sdmc:/switch/nx-port-hub/covers");
}

void CoverLoader::cleanup() {
    for (auto& pair : m_textures) {
        if (pair.second) {
            SDL_DestroyTexture(pair.second);
        }
    }
    m_textures.clear();
}

SDL_Texture* CoverLoader::loadTextureFromFile(const std::string& filepath) {
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load(filepath.c_str(), &w, &h, &channels, 4);
    if (!pixels) return nullptr;

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        stbi_image_free(pixels);
        return nullptr;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_FreeSurface(surface);
    stbi_image_free(pixels);
    return tex;
}

SDL_Texture* CoverLoader::loadTextureFromMemory(const unsigned char* data, int len) {
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(data, len, &w, &h, &channels, 4);
    if (!pixels) return nullptr;

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        stbi_image_free(pixels);
        return nullptr;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_FreeSurface(surface);
    stbi_image_free(pixels);
    return tex;
}

void CoverLoader::saveCache(const std::string& port_id, const std::string& ext, const std::string& data) {
    std::string cache_path = "sdmc:/switch/nx-port-hub/covers/" + port_id + ext;
    std::ofstream out(cache_path, std::ios::binary);
    if (out) {
        out.write(data.data(), data.size());
    }
}

SDL_Texture* CoverLoader::getCover(const std::string& port_id, const std::string& cover_url) {
    if (port_id.empty()) return nullptr;

    auto it = m_textures.find(port_id);
    if (it != m_textures.end()) {
        return it->second;
    }

    // 1. Tenta carregar do cache local no MicroSD (PNG ou JPG)
    std::string path_png = "sdmc:/switch/nx-port-hub/covers/" + port_id + ".png";
    SDL_Texture* tex = loadTextureFromFile(path_png);
    if (tex) {
        m_textures[port_id] = tex;
        return tex;
    }

    std::string path_jpg = "sdmc:/switch/nx-port-hub/covers/" + port_id + ".jpg";
    tex = loadTextureFromFile(path_jpg);
    if (tex) {
        m_textures[port_id] = tex;
        return tex;
    }

    // 2. Se não estiver em cache e tiver URL de capa, faz download da imagem
    if (!cover_url.empty()) {
        std::string img_data, err;
        if (Network::fetchString(cover_url, img_data, err, 8)) {
            tex = loadTextureFromMemory(reinterpret_cast<const unsigned char*>(img_data.data()), img_data.size());
            if (tex) {
                std::string ext = (cover_url.find(".png") != std::string::npos) ? ".png" : ".jpg";
                saveCache(port_id, ext, img_data);
                m_textures[port_id] = tex;
                return tex;
            }
        }
    }

    m_textures[port_id] = nullptr;
    return nullptr;
}
