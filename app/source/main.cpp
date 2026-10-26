#include "ui.hpp"
#include "network.hpp"
#include "catalog.hpp"
#include "installer.hpp"
#include <fstream>
#include <sstream>

#ifdef __SWITCH__
#include <switch.h>
#endif

static const char* CACHE_DIR = "sdmc:/switch/nx-port-hub";
static const char* CACHE_FILE = "sdmc:/switch/nx-port-hub/ports_cache.json";

static void saveCache(const std::string& content) {
    Installer::ensureDirectoryExists(CACHE_DIR);
    std::ofstream out(CACHE_FILE);
    if (out) {
        out << content;
    }
}

static std::string loadCache() {
    std::ifstream in(CACHE_FILE);
    if (!in) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[]) {
    // Inicialização da interface
    UI ui;
    if (!ui.init()) {
        return -1;
    }

    // Inicialização de rede
    Network::initialize();

    Catalog catalog;
    std::string catalog_json;
    std::string err_msg;

    // 1. Tentar baixar o catálogo mais recente do GitHub Pages
    bool loaded = false;
    if (Network::fetchString(Catalog::getDefaultCatalogUrl(), catalog_json, err_msg, 8)) {
        if (catalog.parse(catalog_json, err_msg)) {
            saveCache(catalog_json);
            loaded = true;
        }
    }

    // 2. Se offline ou falhou, tenta carregar do cache local no cartão SD
    if (!loaded) {
        std::string cached = loadCache();
        if (!cached.empty() && catalog.parse(cached, err_msg)) {
            loaded = true;
        }
    }

    ui.setCatalog(catalog);

    // Loop principal de renderização e eventos
    while (!ui.shouldExit()) {
#ifdef __SWITCH__
        if (!appletMainLoop()) break;
#endif
        ui.update();
        ui.render();
        SDL_Delay(16); // ~60 FPS
    }

    // Encerramento
    ui.shutdown();
    Network::cleanup();

    return 0;
}
