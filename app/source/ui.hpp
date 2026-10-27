#ifndef UI_HPP
#define UI_HPP

#include <SDL.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include "catalog.hpp"
#include "cover_loader.hpp"
#include "preview_loader.hpp"

enum ViewState {
    VIEW_CATALOG,
    VIEW_DETAILS,
    VIEW_DOWNLOADING,
    VIEW_SETTINGS,
    VIEW_MESSAGE
};

enum InstallStage {
    STAGE_DOWNLOADING,
    STAGE_EXTRACTING
};

class UI {
public:
    UI();
    ~UI();

    bool init();
    void shutdown();

    void renderSplashScreen(const std::string& status, float progress);

    void setCatalog(const Catalog& cat);
    void update();
    void render();

    bool shouldExit() const { return m_exit; }
    void onButtonDown(int button);

private:
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    CoverLoader m_covers;
    PreviewLoader m_previews;

    ViewState m_state = VIEW_CATALOG;
    Catalog m_catalog;

    int m_selected_index = 0;
    int m_scroll_offset = 0;
    bool m_exit = false;

    // Thread de instalação assíncrona de alto desempenho
    std::thread m_worker_thread;
    std::mutex m_status_mutex;

    // Estado do download e extração
    std::atomic<InstallStage> m_stage{STAGE_DOWNLOADING};
    std::atomic<bool> m_download_in_progress{false};
    std::atomic<bool> m_download_cancel_req{false};
    std::atomic<bool> m_install_finished{false};
    std::atomic<bool> m_install_success{false};
    std::string m_install_result_msg;

    std::atomic<uint64_t> m_dl_current{0};
    std::atomic<uint64_t> m_dl_total{0};
    std::atomic<double> m_dl_speed{0.0};
    std::string m_dl_status_text;

    // Estado da extração
    std::atomic<int> m_extract_current{0};
    std::atomic<int> m_extract_total{0};
    std::string m_extract_filename;

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
