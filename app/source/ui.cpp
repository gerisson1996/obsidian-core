#include "ui.hpp"
#include "font.hpp"
#include "installer.hpp"
#include "network.hpp"
#include <iomanip>
#include <sstream>

#ifdef __SWITCH__
#include <switch.h>
#endif

// Paleta de cores moderna do NX Port Hub
static const SDL_Color COLOR_BG           = { 0x0E, 0x11, 0x17, 0xFF }; // #0e1117 fundo escuro profundo
static const SDL_Color COLOR_PANEL        = { 0x16, 0x1B, 0x24, 0xFF }; // #161b24 painéis
static const SDL_Color COLOR_PANEL_ALT    = { 0x1E, 0x25, 0x33, 0xFF }; // #1e2533
static const SDL_Color COLOR_ACCENT       = { 0x79, 0xE0, 0xC3, 0xFF }; // #79e0c3 ciano / menta
static const SDL_Color COLOR_TEXT         = { 0xF4, 0xF6, 0xFB, 0xFF }; // #f4f6fb branco suave
static const SDL_Color COLOR_MUTED        = { 0x9A, 0xA6, 0xBB, 0xFF }; // #9aa6bb cinza texto
static const SDL_Color COLOR_GREEN        = { 0x38, 0xC1, 0x72, 0xFF }; // #38c172 verde vibrante
static const SDL_Color COLOR_WARN         = { 0xF6, 0x99, 0x3F, 0xFF }; // #f6993f âmbar
static const SDL_Color COLOR_DANGER       = { 0xE3, 0x34, 0x2F, 0xFF }; // #e3342f vermelho

UI::UI() {}

UI::~UI() {
    shutdown();
}

bool UI::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        return false;
    }

    m_window = SDL_CreateWindow("NX Port Hub", 0, 0, 1280, 720, 0);
    if (!m_window) {
        return false;
    }

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer) {
        return false;
    }

    m_covers.init(m_renderer);

    for (int i = 0; i < 2; i++) {
        SDL_JoystickOpen(i);
    }

    return true;
}

void UI::shutdown() {
    m_covers.cleanup();
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
            switch (event.key.keysym.sym) {
                case SDLK_LEFT: onButtonDown(12); break;   // D-Pad Left
                case SDLK_UP: onButtonDown(13); break;     // D-Pad Up
                case SDLK_RIGHT: onButtonDown(14); break;  // D-Pad Right
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
    if (m_state == VIEW_MESSAGE) {
        if (button == 0 || button == 1) {
            m_state = m_prev_state;
        }
        return;
    }

    if (button == 10) { // Plus (+)
        m_exit = true;
        return;
    }

    if (m_state == VIEW_CATALOG) {
        // Navegação horizontal/vertical entre os cards de capas
        if (button == 12 || button == 13) { // Esquerda ou Cima
            if (m_selected_index > 0) {
                m_selected_index--;
                if (m_selected_index < m_scroll_offset) {
                    m_scroll_offset = m_selected_index;
                }
            }
        } else if (button == 14 || button == 15) { // Direita ou Baixo
            if (!m_catalog.ports.empty() && m_selected_index < (int)m_catalog.ports.size() - 1) {
                m_selected_index++;
                if (m_selected_index >= m_scroll_offset + 4) {
                    m_scroll_offset = m_selected_index - 3;
                }
            }
        } else if (button == 0) { // A - Detalhes
            if (!m_catalog.ports.empty()) {
                m_state = VIEW_DETAILS;
            }
        } else if (button == 2) { // X - Atualizar catálogo
            std::string content, err;
            if (Network::fetchString(Catalog::getDefaultCatalogUrl(), content, err)) {
                m_catalog.parse(content, err);
                showModal("Catálogo Sincronizado", "Lista de jogos e atualizações atualizada com sucesso!");
            } else {
                showModal("Sem Conexão", "Não foi possível sincronizar o catálogo remoto:\n" + err);
            }
        } else if (button == 3) { // Y - Ajustes
            m_state = VIEW_SETTINGS;
        }
    } else if (m_state == VIEW_DETAILS) {
        if (button == 1) { // B - Voltar
            m_state = VIEW_CATALOG;
        } else if (button == 0) { // A - Instalar
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
        if (button == 1) { // B - Cancelar
            m_download_cancel_req = true;
        }
    } else if (m_state == VIEW_SETTINGS) {
        if (button == 1 || button == 0) {
            m_state = VIEW_CATALOG;
        }
    }
}

void UI::startInstallCurrentPort() {
    if (m_catalog.ports.empty() || m_selected_index >= (int)m_catalog.ports.size()) return;

    const PortItem& port = m_catalog.ports[m_selected_index];
    m_state = VIEW_DOWNLOADING;
    m_stage = STAGE_DOWNLOADING;
    m_download_in_progress = true;
    m_download_cancel_req = false;
    m_dl_current = 0;
    m_dl_total = port.get_total_download_size();
    m_dl_speed = 0.0;
    m_dl_status_text = "Iniciando download do jogo...";
    m_extract_current = 0;
    m_extract_total = 0;
    m_extract_filename = "";

    render();

    std::string result_msg;
    Uint32 last_render_tick = SDL_GetTicks();

    bool success = Installer::installPort(port, 
        [this, &last_render_tick](uint64_t cur, uint64_t total, double speed) {
            m_stage = STAGE_DOWNLOADING;
            m_dl_current = cur;
            if (total > 0) m_dl_total = total;
            m_dl_speed = speed;

            Uint32 now = SDL_GetTicks();
            if (now - last_render_tick >= 80 || cur == total) {
                last_render_tick = now;
                render();

                SDL_Event ev;
                while (SDL_PollEvent(&ev)) {
                    if (ev.type == SDL_JOYBUTTONDOWN && ev.jbutton.button == 1) {
                        m_download_cancel_req = true;
                    }
                }
            }
        },
        [this, &last_render_tick](const std::string& filename, int cur, int tot) {
            m_stage = STAGE_EXTRACTING;
            m_extract_current = cur;
            m_extract_total = tot;
            m_extract_filename = filename;

            Uint32 now = SDL_GetTicks();
            if (now - last_render_tick >= 50 || cur == tot) {
                last_render_tick = now;
                render();
            }
        },
        &m_download_cancel_req, 
        result_msg);

    m_download_in_progress = false;

    if (m_download_cancel_req) {
        showModal("Cancelado", "O download foi interrompido.");
    } else if (success) {
        showModal("Instalação Concluída!", result_msg);
    } else {
        showModal("Erro na Instalação", result_msg);
    }
}

void UI::renderHeader(const std::string& title, const std::string& subtitle) {
    SDL_Rect top_bar = { 0, 0, 1280, 68 };
    SDL_SetRenderDrawColor(m_renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(m_renderer, &top_bar);

    SDL_SetRenderDrawColor(m_renderer, 0x2A, 0x34, 0x46, 255);
    SDL_RenderDrawLine(m_renderer, 0, 68, 1280, 68);

    FontRenderer::drawText(m_renderer, 32, 18, title, COLOR_ACCENT, 2);
    if (!subtitle.empty()) {
        FontRenderer::drawText(m_renderer, 32 + FontRenderer::getTextWidth(title, 2) + 20, 20, "• " + subtitle, COLOR_MUTED, 2);
    }

    std::string net_str = Network::is_online() ? "[ Wi-Fi Conectado ]" : "[ Sem Wi-Fi ]";
    SDL_Color net_color = Network::is_online() ? COLOR_GREEN : COLOR_WARN;
    int net_w = FontRenderer::getTextWidth(net_str, 2);
    FontRenderer::drawText(m_renderer, 1280 - net_w - 32, 20, net_str, net_color, 2);
}

void UI::renderFooter(const std::string& hints) {
    SDL_Rect bot_bar = { 0, 664, 1280, 56 };
    SDL_SetRenderDrawColor(m_renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(m_renderer, &bot_bar);

    SDL_SetRenderDrawColor(m_renderer, 0x2A, 0x34, 0x46, 255);
    SDL_RenderDrawLine(m_renderer, 0, 664, 1280, 664);

    FontRenderer::drawText(m_renderer, 32, 682, hints, COLOR_MUTED, 2);
}

void UI::renderCatalog() {
    renderHeader("NX PORT HUB", "Galeria de Jogos");

    if (m_catalog.ports.empty()) {
        FontRenderer::drawText(m_renderer, 40, 200, "Nenhum jogo encontrado no catálogo.", COLOR_TEXT, 2);
        FontRenderer::drawText(m_renderer, 40, 240, "Aperte (X) para buscar ports via Wi-Fi.", COLOR_MUTED, 2);
        renderFooter("(X) Sincronizar Catálogo   (+) Sair");
        return;
    }

    // 1. CARROSSEL DE CAPAS (BOXARTS)
    int card_w = 210;
    int card_h = 280;
    int spacing = 26;
    int start_x = 40;
    int start_y = 88;
    int visible_cards = 4;

    for (int i = 0; i < visible_cards && (m_scroll_offset + i) < (int)m_catalog.ports.size(); i++) {
        int idx = m_scroll_offset + i;
        const auto& port = m_catalog.ports[idx];
        bool selected = (idx == m_selected_index);

        int cur_x = start_x + i * (card_w + spacing);
        int cur_y = start_y + (selected ? 0 : 8); // Card selecionado se eleva sutilmente
        int cur_h = selected ? card_h : (card_h - 16);

        SDL_Rect card_rect = { cur_x, cur_y, card_w, cur_h };

        // Sombra de fundo
        SDL_Rect shadow_rect = { cur_x + 4, cur_y + 4, card_w, cur_h };
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 100);
        SDL_RenderFillRect(m_renderer, &shadow_rect);
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

        // Fundo do card
        SDL_SetRenderDrawColor(m_renderer, COLOR_PANEL_ALT.r, COLOR_PANEL_ALT.g, COLOR_PANEL_ALT.b, 255);
        SDL_RenderFillRect(m_renderer, &card_rect);

        // Tenta carregar e desenhar a capa real do jogo
        SDL_Texture* cover_tex = m_covers.getCover(port.id, port.cover_url);
        if (cover_tex) {
            SDL_RenderCopy(m_renderer, cover_tex, NULL, &card_rect);
        } else {
            // Placeholder estilizado se a capa ainda estiver baixando
            SDL_Rect inner = { cur_x + 10, cur_y + 10, card_w - 20, cur_h - 20 };
            SDL_SetRenderDrawColor(m_renderer, 0x12, 0x17, 0x22, 255);
            SDL_RenderFillRect(m_renderer, &inner);
            FontRenderer::drawText(m_renderer, cur_x + 45, cur_y + cur_h / 2 - 20, "NX PORT", COLOR_ACCENT, 2);
            FontRenderer::drawText(m_renderer, cur_x + 20, cur_y + cur_h / 2 + 10, port.title, COLOR_TEXT, 2);
        }

        // Borda destacada com brilho se selecionado
        if (selected) {
            SDL_SetRenderDrawColor(m_renderer, COLOR_ACCENT.r, COLOR_ACCENT.g, COLOR_ACCENT.b, 255);
            for (int b = 0; b < 3; b++) {
                SDL_Rect border_r = { card_rect.x - b, card_rect.y - b, card_rect.w + b * 2, card_rect.h + b * 2 };
                SDL_RenderDrawRect(m_renderer, &border_r);
            }
        }

        // Tag "INSTALADO" no topo da capa
        if (port.is_installed()) {
            SDL_Rect tag_r = { card_rect.x + 8, card_rect.y + 8, 120, 24 };
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(m_renderer, 0x10, 0x38, 0x24, 230);
            SDL_RenderFillRect(m_renderer, &tag_r);
            SDL_SetRenderDrawColor(m_renderer, COLOR_GREEN.r, COLOR_GREEN.g, COLOR_GREEN.b, 255);
            SDL_RenderDrawRect(m_renderer, &tag_r);
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
            FontRenderer::drawText(m_renderer, tag_r.x + 8, tag_r.y + 4, "[ INSTALADO ]", COLOR_GREEN, 1);
        }
    }

    // 2. PAINEL DE INFORMAÇÕES DO JOGO SELECIONADO
    if (m_selected_index < (int)m_catalog.ports.size()) {
        const auto& sel = m_catalog.ports[m_selected_index];
        SDL_Rect info_box = { 40, 388, 1200, 260 };

        SDL_SetRenderDrawColor(m_renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
        SDL_RenderFillRect(m_renderer, &info_box);

        SDL_SetRenderDrawColor(m_renderer, 0x2A, 0x34, 0x46, 255);
        SDL_RenderDrawRect(m_renderer, &info_box);

        int text_x = 64;
        int text_y = 404;

        // Título em destaque (escala 3)
        FontRenderer::drawText(m_renderer, text_x, text_y, sel.title, COLOR_TEXT, 3);
        text_y += 36;

        // Badges na mesma linha: status, versão, tamanho
        std::string status_badge = (sel.compatibility.status == "jogavel") ? "[ * 60 FPS JOGAVEL ]" : "[ ! EXPERIMENTAL ]";
        SDL_Color badge_col = (sel.compatibility.status == "jogavel") ? COLOR_ACCENT : COLOR_WARN;
        FontRenderer::drawText(m_renderer, text_x, text_y, status_badge, badge_col, 2);

        int off_x = text_x + FontRenderer::getTextWidth(status_badge, 2) + 16;
        std::string ver_badge = "v" + sel.version + " por " + sel.author;
        FontRenderer::drawText(m_renderer, off_x, text_y, ver_badge, COLOR_MUTED, 2);

        uint64_t total_bytes = sel.get_total_download_size();
        if (total_bytes > 0) {
            double mb = (double)total_bytes / (1024.0 * 1024.0);
            std::ostringstream ss;
            if (mb >= 1000.0) ss << std::fixed << std::setprecision(2) << (mb / 1024.0) << " GB";
            else ss << std::fixed << std::setprecision(1) << mb << " MB";
            
            int off_x2 = off_x + FontRenderer::getTextWidth(ver_badge, 2) + 16;
            FontRenderer::drawText(m_renderer, off_x2, text_y, "[ " + ss.str() + " ]", COLOR_TEXT, 2);
        }

        text_y += 32;

        // Descrição do jogo
        std::string desc = sel.short_description.empty() ? sel.description : sel.short_description;
        // Divide em linhas se for muito longa
        if (desc.size() > 95) {
            size_t break_pt = desc.find_last_of(' ', 95);
            if (break_pt == std::string::npos) break_pt = 95;
            std::string line1 = desc.substr(0, break_pt);
            std::string line2 = desc.substr(break_pt + 1);
            if (line2.size() > 95) line2 = line2.substr(0, 92) + "...";

            FontRenderer::drawText(m_renderer, text_x, text_y, line1, COLOR_MUTED, 2);
            text_y += 24;
            FontRenderer::drawText(m_renderer, text_x, text_y, line2, COLOR_MUTED, 2);
        } else {
            FontRenderer::drawText(m_renderer, text_x, text_y, desc, COLOR_MUTED, 2);
        }

        text_y += 32;

        // Status dos arquivos no SD
        std::vector<std::string> missing;
        if (sel.original_files.required) {
            if (sel.check_original_files_present(missing)) {
                FontRenderer::drawText(m_renderer, text_x, text_y, "[OK] Todos os arquivos do jogo estao prontos no cartao SD.", COLOR_GREEN, 2);
            } else {
                FontRenderer::drawText(m_renderer, text_x, text_y, "[!] Download inclui instalacao automatica completa via Google Drive.", COLOR_ACCENT, 2);
            }
        } else {
            FontRenderer::drawText(m_renderer, text_x, text_y, "[OK] Jogo standalone (pronto para instalar e jogar).", COLOR_GREEN, 2);
        }
    }

    renderFooter("(D-Pad / Analógico) Navegar   (A) Ver Detalhes & Baixar   (X) Sincronizar   (Y) Ajustes   (+) Sair");
}

void UI::renderDetails() {
    if (m_selected_index >= (int)m_catalog.ports.size()) return;
    const auto& port = m_catalog.ports[m_selected_index];

    renderHeader(port.title, "Ficha do Jogo");

    // Capa grande à esquerda (240x336)
    SDL_Rect big_cover = { 48, 96, 240, 336 };
    SDL_SetRenderDrawColor(m_renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(m_renderer, &big_cover);

    SDL_Texture* cover_tex = m_covers.getCover(port.id, port.cover_url);
    if (cover_tex) {
        SDL_RenderCopy(m_renderer, cover_tex, NULL, &big_cover);
    } else {
        FontRenderer::drawText(m_renderer, big_cover.x + 60, big_cover.y + 150, "NX PORT", COLOR_ACCENT, 2);
    }

    SDL_SetRenderDrawColor(m_renderer, COLOR_ACCENT.r, COLOR_ACCENT.g, COLOR_ACCENT.b, 255);
    SDL_RenderDrawRect(m_renderer, &big_cover);

    // Painel de detalhes à direita
    SDL_Rect right_panel = { 312, 96, 920, 544 };
    SDL_SetRenderDrawColor(m_renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(m_renderer, &right_panel);
    SDL_SetRenderDrawColor(m_renderer, 0x2A, 0x34, 0x46, 255);
    SDL_RenderDrawRect(m_renderer, &right_panel);

    int cur_x = 336;
    int cur_y = 118;

    FontRenderer::drawText(m_renderer, cur_x, cur_y, port.title, COLOR_TEXT, 3);
    cur_y += 36;
    FontRenderer::drawText(m_renderer, cur_x, cur_y, "Versão: " + port.version + " • Desenvolvido por: " + port.author, COLOR_MUTED, 2);
    cur_y += 32;

    // Tamanho do download
    uint64_t total_bytes = port.get_total_download_size();
    double total_mb = (double)total_bytes / (1024.0 * 1024.0);
    std::ostringstream ss_sz;
    if (total_mb >= 1000.0) ss_sz << std::fixed << std::setprecision(2) << (total_mb / 1024.0) << " GB";
    else ss_sz << std::fixed << std::setprecision(1) << total_mb << " MB";
    FontRenderer::drawText(m_renderer, cur_x, cur_y, "Tamanho Total do Download: " + ss_sz.str(), COLOR_ACCENT, 2);
    cur_y += 32;

    FontRenderer::drawText(m_renderer, cur_x, cur_y, "Destino no Cartão SD: " + port.installation.install_dir, COLOR_MUTED, 2);
    cur_y += 36;

    // Caixa de arquivos e instalação automática
    SDL_Rect box_auto = { cur_x, cur_y, 872, 160 };
    SDL_SetRenderDrawColor(m_renderer, 0x11, 0x15, 0x1F, 255);
    SDL_RenderFillRect(m_renderer, &box_auto);
    SDL_SetRenderDrawColor(m_renderer, 0x2A, 0x34, 0x46, 255);
    SDL_RenderDrawRect(m_renderer, &box_auto);

    FontRenderer::drawText(m_renderer, box_auto.x + 16, box_auto.y + 16, "INSTALAÇÃO 1-CLIQUE AUTOMATIZADA:", COLOR_ACCENT, 2);

    if (!port.extra_downloads.empty()) {
        FontRenderer::drawText(m_renderer, box_auto.x + 16, box_auto.y + 48, "✓ O aplicativo baixará o executável e todo o pacote do jogo direto do Google Drive.", COLOR_GREEN, 2);
        FontRenderer::drawText(m_renderer, box_auto.x + 16, box_auto.y + 76, "✓ As pastas e arquivos serão extraídos e organizados automaticamente no MicroSD.", COLOR_GREEN, 2);
        FontRenderer::drawText(m_renderer, box_auto.x + 16, box_auto.y + 104, "✓ Compatível com Title Takeover (segure R ao iniciar qualquer jogo no Switch).", COLOR_MUTED, 2);
    } else {
        FontRenderer::drawText(m_renderer, box_auto.x + 16, box_auto.y + 48, "✓ Download direto do executável oficial homebrew.", COLOR_GREEN, 2);
    }

    cur_y += 190;

    // Botão de Ação Destacado
    SDL_Rect btn_install = { cur_x, cur_y, 420, 54 };
    SDL_SetRenderDrawColor(m_renderer, COLOR_ACCENT.r, COLOR_ACCENT.g, COLOR_ACCENT.b, 255);
    SDL_RenderFillRect(m_renderer, &btn_install);

    std::string btn_label = port.is_installed() ? "(A) REINSTALAR / ATUALIZAR" : "(A) BAIXAR E INSTALAR JOGO";
    FontRenderer::drawText(m_renderer, btn_install.x + 24, btn_install.y + 18, btn_label, { 0x0C, 0x19, 0x13, 0xFF }, 2);

    if (port.is_installed()) {
        SDL_Rect btn_del = { cur_x + 440, cur_y, 240, 54 };
        SDL_SetRenderDrawColor(m_renderer, 0x3A, 0x18, 0x18, 255);
        SDL_RenderFillRect(m_renderer, &btn_del);
        SDL_SetRenderDrawColor(m_renderer, COLOR_DANGER.r, COLOR_DANGER.g, COLOR_DANGER.b, 255);
        SDL_RenderDrawRect(m_renderer, &btn_del);
        FontRenderer::drawText(m_renderer, btn_del.x + 20, btn_del.y + 18, "(X) DESINSTALAR", COLOR_DANGER, 2);
    }

    renderFooter("(A) Instalar Jogo Completo   (B) Voltar   (+) Sair");
}

void UI::renderDownloading() {
    if (m_stage == STAGE_EXTRACTING) {
        renderHeader("NX PORT HUB", "Descompactando e Instalando Arquivos");

        SDL_Rect main_panel = { 100, 150, 1080, 420 };
        SDL_SetRenderDrawColor(m_renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
        SDL_RenderFillRect(m_renderer, &main_panel);
        SDL_SetRenderDrawColor(m_renderer, 0x2A, 0x34, 0x46, 255);
        SDL_RenderDrawRect(m_renderer, &main_panel);

        FontRenderer::drawText(m_renderer, 140, 185, "Descompactando arquivos e configurando pastas...", COLOR_ACCENT, 2);

        // Barra de progresso da extração
        SDL_Rect bar_bg = { 140, 230, 1000, 36 };
        SDL_SetRenderDrawColor(m_renderer, 0x10, 0x14, 0x1D, 255);
        SDL_RenderFillRect(m_renderer, &bar_bg);

        double percent = 0.0;
        if (m_extract_total > 0) {
            percent = (double)m_extract_current / (double)m_extract_total;
            if (percent > 1.0) percent = 1.0;
        }

        SDL_Rect bar_fill = { 140, 230, (int)(1000 * percent), 36 };
        SDL_SetRenderDrawColor(m_renderer, COLOR_GREEN.r, COLOR_GREEN.g, COLOR_GREEN.b, 255);
        SDL_RenderFillRect(m_renderer, &bar_fill);

        // Detalhes dos arquivos
        int pct_int = (int)(percent * 100.0);
        std::ostringstream ss_info;
        ss_info << pct_int << "% concluido (" << m_extract_current << " de " << m_extract_total << " arquivos extraidos)";
        FontRenderer::drawText(m_renderer, 140, 285, ss_info.str(), COLOR_TEXT, 2);

        // Nome do arquivo atual sendo gravado
        std::string fname_display = m_extract_filename;
        if (fname_display.size() > 65) {
            fname_display = "..." + fname_display.substr(fname_display.size() - 62);
        }
        FontRenderer::drawText(m_renderer, 140, 325, "Gravando: " + fname_display, COLOR_MUTED, 2);

        FontRenderer::drawText(m_renderer, 140, 390, "[!] Gravando no cartao MicroSD. Nao desligue o console.", COLOR_WARN, 2);

        renderFooter("Aguarde a finalizacao da gravacao...");
    } else {
        renderHeader("NX PORT HUB", "Download em Alta Velocidade");

        SDL_Rect main_panel = { 100, 150, 1080, 420 };
        SDL_SetRenderDrawColor(m_renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
        SDL_RenderFillRect(m_renderer, &main_panel);
        SDL_SetRenderDrawColor(m_renderer, 0x2A, 0x34, 0x46, 255);
        SDL_RenderDrawRect(m_renderer, &main_panel);

        FontRenderer::drawText(m_renderer, 140, 185, m_dl_status_text, COLOR_ACCENT, 2);

        // Barra de progresso
        SDL_Rect bar_bg = { 140, 230, 1000, 36 };
        SDL_SetRenderDrawColor(m_renderer, 0x10, 0x14, 0x1D, 255);
        SDL_RenderFillRect(m_renderer, &bar_bg);

        double percent = 0.0;
        if (m_dl_total > 0) {
            percent = (double)m_dl_current / (double)m_dl_total;
            if (percent > 1.0) percent = 1.0;
        }

        SDL_Rect bar_fill = { 140, 230, (int)(1000 * percent), 36 };
        SDL_SetRenderDrawColor(m_renderer, COLOR_ACCENT.r, COLOR_ACCENT.g, COLOR_ACCENT.b, 255);
        SDL_RenderFillRect(m_renderer, &bar_fill);

        // Texto de porcentagem e velocidade
        int pct_int = (int)(percent * 100.0);
        double cur_mb = (double)m_dl_current / (1024.0 * 1024.0);
        double tot_mb = (double)m_dl_total / (1024.0 * 1024.0);
        double spd_mb = m_dl_speed / (1024.0 * 1024.0);

        std::ostringstream ss;
        ss << pct_int << "% (" << std::fixed << std::setprecision(1) << cur_mb << " MB / " << tot_mb << " MB) • Velocidade: " << spd_mb << " MB/s";
        FontRenderer::drawText(m_renderer, 140, 285, ss.str(), COLOR_TEXT, 2);

        FontRenderer::drawText(m_renderer, 140, 390, "Modo CPU Boost ativo (1.785 GHz). Nao remova o cartao MicroSD.", COLOR_WARN, 2);

        renderFooter("(B) Cancelar Download");
    }
}

void UI::renderSettings() {
    renderHeader("CONFIGURAÇÕES", "NX Port Hub");

    SDL_Rect main_panel = { 50, 100, 1180, 530 };
    SDL_SetRenderDrawColor(m_renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(m_renderer, &main_panel);

    int cur_y = 130;
    FontRenderer::drawText(m_renderer, 80, cur_y, "URL DO CATÁLOGO:", COLOR_ACCENT, 2);
    cur_y += 30;
    FontRenderer::drawText(m_renderer, 80, cur_y, Catalog::getDefaultCatalogUrl(), COLOR_TEXT, 2);
    cur_y += 50;

    uint64_t free_bytes = 0;
    Installer::checkFreeSpace("sdmc:/", 0, free_bytes);
    double free_gb = (double)free_bytes / (1024.0 * 1024.0 * 1024.0);
    std::ostringstream ss;
    ss << "ESPAÇO DISPONÍVEL NO MICROSD: " << std::fixed << std::setprecision(2) << free_gb << " GB";
    FontRenderer::drawText(m_renderer, 80, cur_y, ss.str(), COLOR_GREEN, 2);
    cur_y += 50;

    FontRenderer::drawText(m_renderer, 80, cur_y, "NX PORT HUB - SISTEMA OFICIAL", COLOR_ACCENT, 2);
    cur_y += 30;
    FontRenderer::drawText(m_renderer, 80, cur_y, "Desenvolvido por gerisson1996.", COLOR_MUTED, 2);

    renderFooter("(B) Voltar à Galeria");
}

void UI::renderMessageModal() {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 190);
    SDL_Rect full_rect = { 0, 0, 1280, 720 };
    SDL_RenderFillRect(m_renderer, &full_rect);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

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
            if (m_prev_state == VIEW_DETAILS) renderDetails();
            else if (m_prev_state == VIEW_SETTINGS) renderSettings();
            else renderCatalog();
            renderMessageModal(); 
            break;
    }

    SDL_RenderPresent(m_renderer);
}
