#ifndef UI_HPP
#define UI_HPP

#include <SDL.h>
#include <vector>
#include <string>
#include "catalog.hpp"

enum ViewState {
    VIEW_CATALOG,
    VIEW_DETAILS,
    VIEW_DOWNLOADING,
    VIEW_SETTINGS,
    VIEW_MESSAGE
};

class UI {
public:
    UI();
    ~UI();

    bool init();
    void shutdown();

    void setCatalog(const Catalog& cat);
    void update();
    void render();

    bool shouldExit() const { return m_exit; }

    // Ações acionadas pelo gamepad
    void onButtonDown(int button);

private:
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    ViewState m_state = VIEW_CATALOG;
    Catalog m_catalog;

    int m_selected_index = 0;
    int m_scroll_offset = 0;
    bool m_exit = false;

    // Estado do download
    bool m_download_in_progress = false;
    bool m_download_cancel_req = false;
    uint64_t m_dl_current = 0;
    uint64_t m_dl_total = 0;
    double m_dl_speed = 0.0;
    std::string m_dl_status_text;

    // Modal de mensagem
    std::string m_modal_title;
    std::string m_modal_message;
    ViewState m_prev_state = VIEW_CATALOG;

    void renderCatalog();
    void renderDetails();
    void renderDownloading();
    void renderSettings();
    void renderMessageModal();
    void renderHeader(const std::string& title, const std::string& subtitle);
    void renderFooter(const std::string& hints);

    void showModal(const std::string& title, const std::string& message);
    void startInstallCurrentPort();
};

#endif // UI_HPP
