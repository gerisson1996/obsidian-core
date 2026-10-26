#include "ui.hpp"
#include "font.hpp"
#include "installer.hpp"
#include "network.hpp"
#include <iomanip>
#include <sstream>

#ifdef __SWITCH__
#include <switch.h>
#endif

// Paleta de cores do NX Port Hub
static const SDL_Color COLOR_BG           = { 0x10, 0x13, 0x1B, 0xFF }; // #10131b
static const SDL_Color COLOR_PANEL        = { 0x19, 0x1F, 0x2B, 0xFF }; // #191f2b
static const SDL_Color COLOR_PANEL_SEL    = { 0x25, 0x30, 0x43, 0xFF }; // #253043
static const SDL_Color COLOR_ACCENT       = { 0x79, 0xE0, 0xC3, 0xFF }; // #79e0c3
static const SDL_Color COLOR_TEXT         = { 0xF4, 0xF6, 0xFB, 0xFF }; // #f4f6fb
static const SDL_Color COLOR_MUTED        = { 0xAA, 0xB4, 0xC5, 0xFF }; // #aab4c5
static const SDL_Color COLOR_GREEN        = { 0x38, 0xC1, 0x72, 0xFF }; // #38c172
static const SDL_Color COLOR_WARN         = { 0xF6, 0x99, 0x3F, 0xFF }; // #f6993f
static const SDL_Color COLOR_DANGER       = { 0xE3, 0x34, 0x2F, 0xFF }; // #e3342f

UI::UI() {}

UI::~UI() {
    shutdown();
}

bool UI::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        return false;
    }

    // Modo 1280x720 nativo para Switch Handheld e Docked
    m_window = SDL_CreateWindow("NX Port Hub", 0, 0, 1280, 720, 0);
    if (!m_window) {
        return false;
    }

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer) {
        return false;
    }

    for (int i = 0; i < 2; i++) {
        SDL_JoystickOpen(i);
    }

    return true;
}

void UI::shutdown() {
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
}

void UI::setCatalog(const Catalog& cat) {
    m_catalog = cat;
    m_selected_index = 0;
    m_scroll_offset = 0;
}

void UI::showModal(const std::string& title, const std::string& message) {
    m_modal_title = title;
    m_modal_message = message;
    m_prev_state = m_state;
    m_state = VIEW_MESSAGE;
}

void UI::update() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_exit = true;
        } else if (event.type == SDL_JOYBUTTONDOWN) {
            onButtonDown(event.jbutton.button);
        } else if (event.type == SDL_KEYDOWN) {
            // Suporte a teclado no PC / emulador
            switch (event.key.keysym.sym) {
                case SDLK_UP: onButtonDown(13); break;     // D-Pad Up
                case SDLK_DOWN: onButtonDown(15); break;   // D-Pad Down
                case SDLK_RETURN:
                case SDLK_a: onButtonDown(0); break;       // A
                case SDLK_ESCAPE:
                case SDLK_b: onButtonDown(1); break;       // B
                case SDLK_x: onButtonDown(2); break;       // X
                case SDLK_y: onButtonDown(3); break;       // Y
                case SDLK_PLUS: onButtonDown(10); break;   // Plus
                default: break;
            }
        }
    }
}

void UI::onButtonDown(int button) {
    // Mapeamento Switch SDL:
    // 0 = A, 1 = B, 2 = X, 3 = Y, 10 = Plus (+), 13 = DPad Up, 15 = DPad Down

    if (m_state == VIEW_MESSAGE) {
        if (button == 0 || button == 1) { // A ou B fecha o modal
            m_state = m_prev_state;
        }
        return;
    }

    if (button == 10) { // Botão Plus (+) sempre sai
        m_exit = true;
        return;
    }

    if (m_state == VIEW_CATALOG) {
        if (button == 13) { // Cima
            if (m_selected_index > 0) {
                m_selected_index--;
                if (m_selected_index < m_scroll_offset) {
                    m_scroll_offset = m_selected_index;
                }
            }
        } else if (button == 15) { // Baixo
            if (!m_catalog.ports.empty() && m_selected_index < (int)m_catalog.ports.size() - 1) {
                m_selected_index++;
                if (m_selected_index >= m_scroll_offset + 4) {
                    m_scroll_offset = m_selected_index - 3;
                }
            }
        } else if (button == 0) { // A - Abrir detalhes
            if (!m_catalog.ports.empty()) {
                m_state = VIEW_DETAILS;
            }
        } else if (button == 2) { // X - Recarregar
            std::string content, err;
            if (Network::fetchString(Catalog::getDefaultCatalogUrl(), content, err)) {
                m_catalog.parse(content, err);
                showModal("Catálogo Atualizado", "Os dados foram sincronizados com sucesso.");
            } else {
                showModal("Erro de Conexão", "Não foi possível carregar o catálogo remoto:\n" + err);
            }
        } else if (button == 3) { // Y - Configurações
            m_state = VIEW_SETTINGS;
        }
    } else if (m_state == VIEW_DETAILS) {
        if (button == 1) { // B - Voltar
            m_state = VIEW_CATALOG;
        } else if (button == 0) { // A - Baixar & Instalar
            startInstallCurrentPort();
        } else if (button == 2) { // X - Desinstalar executável se já estiver instalado
            if (!m_catalog.ports.empty()) {
                const auto& port = m_catalog.ports[m_selected_index];
                if (port.is_installed()) {
                    std::string msg;
                    Installer::uninstallPort(port, msg);
                    showModal("Desinstalação", msg);
                }
            }
        }
    } else if (m_state == VIEW_DOWNLOADING) {
        if (button == 1) { // B - Cancelar download
            m_download_cancel_req = true;
        }
    } else if (m_state == VIEW_SETTINGS) {
        if (button == 1 || button == 0) { // B ou A - Voltar
            m_state = VIEW_CATALOG;
        }
    }
}

void UI::startInstallCurrentPort() {
    if (m_catalog.ports.empty() || m_selected_index >= (int)m_catalog.ports.size()) return;

    const PortItem& port = m_catalog.ports[m_selected_index];
    m_state = VIEW_DOWNLOADING;
    m_download_in_progress = true;
    m_download_cancel_req = false;
    m_dl_current = 0;
    m_dl_total = port.get_total_download_size();
    m_dl_speed = 0.0;
    m_dl_status_text = "Iniciando download...";

    // Renderiza o primeiro frame da tela de progresso
    render();

    std::string result_msg;
    Uint32 last_render_tick = SDL_GetTicks();

    bool success = Installer::installPort(port, 
        [this, &last_render_tick](uint64_t cur, uint64_t total, double speed) {
            m_dl_current = cur;
            if (total > 0) m_dl_total = total;
            m_dl_speed = speed;

            Uint32 now = SDL_GetTicks();
            // Atualiza a tela a cada ~80ms (12 FPS na barra) para NAO travar o download esperando VSYNC
            if (now - last_render_tick >= 80 || cur == total) {
                last_render_tick = now;
                render();

                // Processa eventos durante o download para permitir cancelar pelo botão B
                SDL_Event ev;
                while (SDL_PollEvent(&ev)) {
                    if (ev.type == SDL_JOYBUTTONDOWN && ev.jbutton.button == 1) {
                        m_download_cancel_req = true;
                    }
                }
            }
        }, 
        &m_download_cancel_req, 
        result_msg);

    m_download_in_progress = false;

    if (m_download_cancel_req) {
        showModal("Cancelado", "O download do port foi cancelado.");
    } else if (success) {
        showModal("Instalação Concluída!", result_msg);
    } else {
        showModal("Erro na Instalação", result_msg);
    }
}

void UI::renderHeader(const std::string& title, const std::string& subtitle) {
    // Barra superior
    SDL_Rect top_bar = { 0, 0, 1280, 72 };
    SDL_SetRenderDrawColor(m_renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(m_renderer, &top_bar);

    SDL_SetRenderDrawColor(m_renderer, 0x30, 0x3A, 0x4B, 255);
    SDL_RenderDrawLine(m_renderer, 0, 72, 1280, 72);

    FontRenderer::drawText(m_renderer, 32, 16, title, COLOR_ACCENT, 2);
    if (!subtitle.empty()) {
        FontRenderer::drawText(m_renderer, 32 + FontRenderer::getTextWidth(title, 2) + 20, 20, "• " + subtitle, COLOR_MUTED, 2);
    }

    // Status da rede no canto direito
    std::string net_str = Network::is_online() ? "[ Wi-Fi Conectado ]" : "[ Sem Conexão ]";
    SDL_Color net_color = Network::is_online() ? COLOR_GREEN : COLOR_WARN;
    int net_w = FontRenderer::getTextWidth(net_str, 2);
    FontRenderer::drawText(m_renderer, 1280 - net_w - 32, 20, net_str, net_color, 2);
}

void UI::renderFooter(const std::string& hints) {
    // Barra inferior
    SDL_Rect bot_bar = { 0, 660, 1280, 60 };
    SDL_SetRenderDrawColor(m_renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(m_renderer, &bot_bar);

    SDL_SetRenderDrawColor(m_renderer, 0x30, 0x3A, 0x4B, 255);
    SDL_RenderDrawLine(m_renderer, 0, 660, 1280, 660);

    FontRenderer::drawText(m_renderer, 32, 680, hints, COLOR_MUTED, 2);
}

void UI::renderCatalog() {
    renderHeader("NX PORT HUB", "Explorar Ports Comunitários");

    if (m_catalog.ports.empty()) {
        FontRenderer::drawText(m_renderer, 32, 180, "Nenhum port disponível no catálogo.", COLOR_TEXT, 2);
        FontRenderer::drawText(m_renderer, 32, 220, "Pressione (X) para tentar atualizar via Wi-Fi.", COLOR_MUTED, 2);
        renderFooter("(X) Atualizar Catálogo   (+) Sair");
        return;
    }

    int start_y = 96;
    int item_h = 130;
    int visible_count = 4;

    for (int i = 0; i < visible_count && (m_scroll_offset + i) < (int)m_catalog.ports.size(); i++) {
        int idx = m_scroll_offset + i;
        const auto& port = m_catalog.ports[idx];
        bool selected = (idx == m_selected_index);

        SDL_Rect item_rect = { 32, start_y + i * (item_h + 12), 1216, item_h };
        SDL_Color bg = selected ? COLOR_PANEL_SEL : COLOR_PANEL;
        SDL_SetRenderDrawColor(m_renderer, bg.r, bg.g, bg.b, 255);
        SDL_RenderFillRect(m_renderer, &item_rect);

        // Borda destacada quando selecionado
        if (selected) {
            SDL_SetRenderDrawColor(m_renderer, COLOR_ACCENT.r, COLOR_ACCENT.g, COLOR_ACCENT.b, 255);
            SDL_RenderDrawRect(m_renderer, &item_rect);
        }

        // Mini banner / Placeholder da capa
        SDL_Rect thumb_rect = { item_rect.x + 12, item_rect.y + 12, 106, 106 };
        SDL_SetRenderDrawColor(m_renderer, 0x12, 0x16, 0x20, 255);
        SDL_RenderFillRect(m_renderer, &thumb_rect);
        FontRenderer::drawText(m_renderer, thumb_rect.x + 36, thumb_rect.y + 40, "NX", COLOR_ACCENT, 3);

        // Título e Autor
        int text_x = thumb_rect.x + thumb_rect.w + 20;
        FontRenderer::drawText(m_renderer, text_x, item_rect.y + 16, port.title, COLOR_TEXT, 2);
        
        std::string meta_line = "v" + port.version + " por " + port.author;
        FontRenderer::drawText(m_renderer, text_x, item_rect.y + 44, meta_line, COLOR_MUTED, 2);

        // Descrição curta (cortada se longa)
        std::string desc = port.short_description;
        if (desc.size() > 70) desc = desc.substr(0, 67) + "...";
        FontRenderer::drawText(m_renderer, text_x, item_rect.y + 72, desc, COLOR_MUTED, 2);

        // Badges no lado direito
        int badge_x = item_rect.x + item_rect.w - 200;

        // Status de instalação
        if (port.is_installed()) {
            FontRenderer::drawText(m_renderer, badge_x, item_rect.y + 20, "[ INSTALADO ]", COLOR_GREEN, 2);
        } else {
            FontRenderer::drawText(m_renderer, badge_x, item_rect.y + 20, "[ DISPONIVEL ]", COLOR_MUTED, 2);
        }

        // Status de compatibilidade
        if (port.compatibility.status == "jogavel") {
            FontRenderer::drawText(m_renderer, badge_x, item_rect.y + 54, "* JOGAVEL (60FPS)", COLOR_ACCENT, 2);
        } else {
            FontRenderer::drawText(m_renderer, badge_x, item_rect.y + 54, "! EXPERIMENTAL", COLOR_WARN, 2);
        }

        // Tamanho
        if (port.package.download_size_bytes > 0) {
            double mb = (double)port.package.download_size_bytes / (1024.0 * 1024.0);
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1) << mb << " MB";
            FontRenderer::drawText(m_renderer, badge_x, item_rect.y + 88, ss.str(), COLOR_MUTED, 2);
        }
    }

    renderFooter("(A) Ver Detalhes & Instalar   (X) Atualizar Catálogo   (Y) Ajustes   (+) Sair");
}

void UI::renderDetails() {
    if (m_selected_index >= (int)m_catalog.ports.size()) return;
    const auto& port = m_catalog.ports[m_selected_index];

    renderHeader(port.title, "Detalhes do Port");

    // Painel principal
    SDL_Rect main_panel = { 32, 96, 1216, 544 };
    SDL_SetRenderDrawColor(m_renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(m_renderer, &main_panel);

    int cur_y = 116;
    FontRenderer::drawText(m_renderer, 56, cur_y, "TITULO: " + port.title + " (v" + port.version + ")", COLOR_ACCENT, 2);
    cur_y += 32;
    FontRenderer::drawText(m_renderer, 56, cur_y, "AUTOR: " + port.author, COLOR_TEXT, 2);
    cur_y += 32;

    // Modo de execução e notas
    std::string req_notes = "MODO RECOMENDADO: " + port.compatibility.recommended_launch_mode;
    if (!port.compatibility.applet_mode_supported) req_notes += " (Obrigatorio Title Takeover / Segurar R)";
    FontRenderer::drawText(m_renderer, 56, cur_y, req_notes, COLOR_WARN, 2);
    cur_y += 36;

    // Caminho no SD
    FontRenderer::drawText(m_renderer, 56, cur_y, "DESTINO NO SD: " + port.installation.install_dir, COLOR_MUTED, 2);
    cur_y += 40;

    // Verificação de arquivos originais
    SDL_Rect orig_box = { 56, cur_y, 1168, 140 };
    SDL_SetRenderDrawColor(m_renderer, 0x14, 0x18, 0x22, 255);
    SDL_RenderFillRect(m_renderer, &orig_box);

    FontRenderer::drawText(m_renderer, orig_box.x + 16, orig_box.y + 16, "ARQUIVOS ORIGINAIS DO JOGO:", COLOR_ACCENT, 2);

    if (port.original_files.required) {
        std::vector<std::string> missing;
        bool files_ok = port.check_original_files_present(missing);

        if (files_ok) {
            FontRenderer::drawText(m_renderer, orig_box.x + 16, orig_box.y + 48, "[OK] Todos os arquivos necessarios estao no cartao SD!", COLOR_GREEN, 2);
        } else {
            std::string miss_text = "FALTANDO: ";
            for (size_t m = 0; m < missing.size(); m++) {
                miss_text += missing[m] + (m + 1 < missing.size() ? ", " : "");
            }
            FontRenderer::drawText(m_renderer, orig_box.x + 16, orig_box.y + 48, miss_text, COLOR_DANGER, 2);
            FontRenderer::drawText(m_renderer, orig_box.x + 16, orig_box.y + 76, "Copie os arquivos do seu disco original para: " + port.original_files.target_directory, COLOR_MUTED, 2);
        }
    } else {
        FontRenderer::drawText(m_renderer, orig_box.x + 16, orig_box.y + 48, "Este port e standalone (nao requer arquivos extras).", COLOR_GREEN, 2);
    }

    cur_y += 164;

    // Botões de ação
    if (port.is_installed()) {
        FontRenderer::drawText(m_renderer, 56, cur_y, "[ (A) REINSTALAR / ATUALIZAR ]     [ (X) DESINSTALAR ]", COLOR_ACCENT, 2);
    } else {
        FontRenderer::drawText(m_renderer, 56, cur_y, "[ (A) BAIXAR E INSTALAR PORT NO SD ]", COLOR_ACCENT, 2);
    }

    renderFooter("(A) Instalar   (B) Voltar   (X) Desinstalar   (+) Sair");
}

void UI::renderDownloading() {
    renderHeader("NX PORT HUB", "Download & Instalacao");

    SDL_Rect main_panel = { 100, 150, 1080, 420 };
    SDL_SetRenderDrawColor(m_renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(m_renderer, &main_panel);

    FontRenderer::drawText(m_renderer, 140, 190, m_dl_status_text, COLOR_ACCENT, 2);

    // Barra de progresso
    SDL_Rect bar_bg = { 140, 240, 1000, 36 };
    SDL_SetRenderDrawColor(m_renderer, 0x12, 0x16, 0x20, 255);
    SDL_RenderFillRect(m_renderer, &bar_bg);

    double percent = 0.0;
    if (m_dl_total > 0) {
        percent = (double)m_dl_current / (double)m_dl_total;
        if (percent > 1.0) percent = 1.0;
    }

    SDL_Rect bar_fill = { 140, 240, (int)(1000 * percent), 36 };
    SDL_SetRenderDrawColor(m_renderer, COLOR_ACCENT.r, COLOR_ACCENT.g, COLOR_ACCENT.b, 255);
    SDL_RenderFillRect(m_renderer, &bar_fill);

    // Texto de porcentagem e velocidade
    int pct_int = (int)(percent * 100.0);
    double cur_mb = (double)m_dl_current / (1024.0 * 1024.0);
    double tot_mb = (double)m_dl_total / (1024.0 * 1024.0);
    double spd_mb = m_dl_speed / (1024.0 * 1024.0);

    std::ostringstream ss;
    ss << pct_int << "% (" << std::fixed << std::setprecision(1) << cur_mb << " MB / " << tot_mb << " MB) - " << spd_mb << " MB/s";
    FontRenderer::drawText(m_renderer, 140, 296, ss.str(), COLOR_TEXT, 2);

    FontRenderer::drawText(m_renderer, 140, 380, "Nao desligue o console nem retire o cartao MicroSD.", COLOR_WARN, 2);

    renderFooter("(B) Cancelar Download");
}

void UI::renderSettings() {
    renderHeader("CONFIGURACOES", "NX Port Hub v0.1.0");

    SDL_Rect main_panel = { 50, 100, 1180, 530 };
    SDL_SetRenderDrawColor(m_renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(m_renderer, &main_panel);

    int cur_y = 130;
    FontRenderer::drawText(m_renderer, 80, cur_y, "URL DO CATALOGO REMOTO:", COLOR_ACCENT, 2);
    cur_y += 30;
    FontRenderer::drawText(m_renderer, 80, cur_y, Catalog::getDefaultCatalogUrl(), COLOR_TEXT, 2);
    cur_y += 50;

    // Espaço livre no SD
    uint64_t free_bytes = 0;
    Installer::checkFreeSpace("sdmc:/", 0, free_bytes);
    double free_gb = (double)free_bytes / (1024.0 * 1024.0 * 1024.0);
    std::ostringstream ss;
    ss << "ESPACO LIVRE NO CARTAO SD: " << std::fixed << std::setprecision(2) << free_gb << " GB";
    FontRenderer::drawText(m_renderer, 80, cur_y, ss.str(), COLOR_GREEN, 2);
    cur_y += 50;

    FontRenderer::drawText(m_renderer, 80, cur_y, "SOBRE O PROJETO:", COLOR_ACCENT, 2);
    cur_y += 30;
    FontRenderer::drawText(m_renderer, 80, cur_y, "NX Port Hub e um ecossistema livre para facilitar a instalacao", COLOR_MUTED, 2);
    cur_y += 26;
    FontRenderer::drawText(m_renderer, 80, cur_y, "e atualizacao de ports de jogos de Xbox e PC no Nintendo Switch.", COLOR_MUTED, 2);
    cur_y += 26;
    FontRenderer::drawText(m_renderer, 80, cur_y, "Desenvolvido por gerisson1996.", COLOR_MUTED, 2);

    renderFooter("(B) Voltar ao Catálogo");
}

void UI::renderMessageModal() {
    // Escurecer fundo
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 180);
    SDL_Rect full_rect = { 0, 0, 1280, 720 };
    SDL_RenderFillRect(m_renderer, &full_rect);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

    // Caixa do modal
    SDL_Rect modal_box = { 180, 180, 920, 360 };
    SDL_SetRenderDrawColor(m_renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(m_renderer, &modal_box);

    SDL_SetRenderDrawColor(m_renderer, COLOR_ACCENT.r, COLOR_ACCENT.g, COLOR_ACCENT.b, 255);
    SDL_RenderDrawRect(m_renderer, &modal_box);

    FontRenderer::drawText(m_renderer, modal_box.x + 36, modal_box.y + 36, m_modal_title, COLOR_ACCENT, 2);
    FontRenderer::drawText(m_renderer, modal_box.x + 36, modal_box.y + 86, m_modal_message, COLOR_TEXT, 2);

    FontRenderer::drawText(m_renderer, modal_box.x + 36, modal_box.y + modal_box.h - 50, "Pressione (A) ou (B) para continuar.", COLOR_MUTED, 2);
}

void UI::render() {
    SDL_SetRenderDrawColor(m_renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(m_renderer);

    switch (m_state) {
        case VIEW_CATALOG: renderCatalog(); break;
        case VIEW_DETAILS: renderDetails(); break;
        case VIEW_DOWNLOADING: renderDownloading(); break;
        case VIEW_SETTINGS: renderSettings(); break;
        case VIEW_MESSAGE: 
            // Renderiza tela anterior por baixo
            if (m_prev_state == VIEW_DETAILS) renderDetails();
            else if (m_prev_state == VIEW_SETTINGS) renderSettings();
            else renderCatalog();
            renderMessageModal(); 
            break;
    }

    SDL_RenderPresent(m_renderer);
}
