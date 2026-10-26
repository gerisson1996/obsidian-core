#include "network.hpp"
#include <curl/curl.h>
#include <cstdio>
#include <chrono>
#include <regex>

#ifdef __SWITCH__
#include <switch.h>
#endif

bool Network::s_initialized = false;

bool Network::initialize() {
    if (s_initialized) return true;

#ifdef __SWITCH__
    // Configuração de alto desempenho para a camada BSD/Sockets do Switch (TCP Window 8 MB)
    SocketInitConfig sock_cfg = {
        .tcp_tx_buf_size = 0x40000,       // 256 KB
        .tcp_rx_buf_size = 0x100000,      // 1 MB
        .tcp_tx_buf_max_size = 0x80000,   // 512 KB
        .tcp_rx_buf_max_size = 0x800000,  // 8 MB de janela TCP ampla
        .udp_tx_buf_size = 0x2400,
        .udp_rx_buf_size = 0xA500,
        .sb_efficiency = 4,
        .num_bsd_sessions = 3,
        .bsd_service_type = BsdServiceType_User,
    };
    Result rc = socketInitialize(&sock_cfg);
    if (R_FAILED(rc)) {
        rc = socketInitializeDefault();
        if (R_FAILED(rc)) return false;
    }
#endif

    curl_global_init(CURL_GLOBAL_DEFAULT);
    s_initialized = true;
    return true;
}

void Network::cleanup() {
    if (!s_initialized) return;

    curl_global_cleanup();

#ifdef __SWITCH__
    socketExit();
#endif

    s_initialized = false;
}

bool Network::is_online() {
#ifdef __SWITCH__
    NifmInternetConnectionType conn_type;
    uint32_t wifi_strength = 0;
    NifmInternetConnectionStatus status;
    Result rc = nifmGetInternetConnectionStatus(&conn_type, &wifi_strength, &status);
    if (R_SUCCEEDED(rc)) {
        return (status == NifmInternetConnectionStatus_Connected);
    }
    return true;
#else
    return true;
#endif
}

std::string Network::normalizeDownloadUrl(const std::string& url) {
    if (url.find("drive.google.com") != std::string::npos || url.find("drive.usercontent.google.com") != std::string::npos) {
        std::string file_id;
        
        size_t d_pos = url.find("/d/");
        if (d_pos != std::string::npos) {
            size_t start = d_pos + 3;
            size_t end = url.find('/', start);
            if (end == std::string::npos) end = url.find('?', start);
            if (end == std::string::npos) end = url.size();
            file_id = url.substr(start, end - start);
        } else {
            size_t id_pos = url.find("id=");
            if (id_pos != std::string::npos) {
                size_t start = id_pos + 3;
                size_t end = url.find('&', start);
                if (end == std::string::npos) end = url.size();
                file_id = url.substr(start, end - start);
            }
        }

        if (!file_id.empty()) {
            return "https://drive.usercontent.google.com/download?id=" + file_id + "&export=download&confirm=t";
        }
    }
    return url;
}

static size_t writeStringCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), total_size);
    return total_size;
}

bool Network::fetchString(const std::string& url, std::string& out_data, std::string& err_msg, int timeout_seconds) {
    if (!s_initialized && !initialize()) {
        err_msg = "Falha ao inicializar subsistema de rede.";
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        err_msg = "Falha ao criar instância do cURL.";
        return false;
    }

    std::string direct_url = normalizeDownloadUrl(url);

    out_data.clear();
    curl_easy_setopt(curl, CURLOPT_URL, direct_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeStringCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out_data);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Nintendo Switch; NXPortHub/1.0)");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout_seconds));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        err_msg = std::string("Erro de rede: ") + curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        return false;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code < 200 || http_code >= 300) {
        err_msg = "Servidor respondeu com HTTP " + std::to_string(http_code);
        return false;
    }

    return true;
}

struct DownloadContext {
    FILE* fp = nullptr;
    DownloadProgressCallback progress_cb;
    bool* cancel_flag = nullptr;
    std::chrono::steady_clock::time_point last_time;
    std::chrono::steady_clock::time_point last_ui_time;
    curl_off_t last_dl = 0;
    double current_speed = 0.0;
};

static size_t writeFileCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    DownloadContext* ctx = static_cast<DownloadContext*>(userdata);
    return fwrite(ptr, size, nmemb, ctx->fp);
}

static int xferInfoCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    DownloadContext* ctx = static_cast<DownloadContext*>(clientp);

    if (ctx->cancel_flag && *(ctx->cancel_flag)) {
        return 1;
    }

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - ctx->last_time;
    if (elapsed.count() >= 0.4) {
        double bytes_diff = static_cast<double>(dlnow - ctx->last_dl);
        ctx->current_speed = bytes_diff / elapsed.count();
        ctx->last_dl = dlnow;
        ctx->last_time = now;
    }

    // Intervalo de 60ms para UI (~16 FPS na barra de progresso) garantindo taxa máxima de recepção de rede
    std::chrono::duration<double> ui_elapsed = now - ctx->last_ui_time;
    if (ui_elapsed.count() >= 0.06 || dlnow == dltotal) {
        ctx->last_ui_time = now;
        if (ctx->progress_cb) {
            ctx->progress_cb(static_cast<uint64_t>(dlnow), static_cast<uint64_t>(dltotal), ctx->current_speed);
        }
    }

    return 0;
}

bool Network::downloadFile(const std::string& url, 
                           const std::string& dest_path, 
                           DownloadProgressCallback progress_cb, 
                           bool* cancel_flag, 
                           std::string& err_msg) {
    if (!s_initialized && !initialize()) {
        err_msg = "Rede não inicializada.";
        return false;
    }

#ifdef __SWITCH__
    // Ativa o modo CPU Boost (1.785 GHz) do Switch para acelerar SSL e downloads pesados
    appletSetCpuBoostMode(ApmCpuBoostMode_FastLoad);
#endif

    std::string direct_url = normalizeDownloadUrl(url);

    FILE* fp = fopen(dest_path.c_str(), "wb");
    if (!fp) {
#ifdef __SWITCH__
        appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
#endif
        err_msg = "Falha ao abrir arquivo para escrita: " + dest_path;
        return false;
    }

    // Buffer de 2 MB de escrita contínua para máxima velocidade no MicroSD
    setvbuf(fp, NULL, _IOFBF, 2 * 1024 * 1024);

    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
#ifdef __SWITCH__
        appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
#endif
        err_msg = "Falha ao inicializar cURL.";
        return false;
    }

    DownloadContext ctx;
    ctx.fp = fp;
    ctx.progress_cb = progress_cb;
    ctx.cancel_flag = cancel_flag;
    ctx.last_time = std::chrono::steady_clock::now();
    ctx.last_ui_time = ctx.last_time;

    curl_easy_setopt(curl, CURLOPT_URL, direct_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferInfoCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    // Buffer de recepção TCP de 1 MB para estourar o limite de 5 MB/s
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 1024 * 1024);
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Nintendo Switch; NXPortHub/1.0)");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);

    CURLcode res = curl_easy_perform(curl);
    fflush(fp);
    fclose(fp);

#ifdef __SWITCH__
    // Retorna a CPU ao modo normal
    appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
#endif

    if (res == CURLE_ABORTED_BY_CALLBACK) {
        remove(dest_path.c_str());
        err_msg = "Download cancelado pelo usuário.";
        curl_easy_cleanup(curl);
        return false;
    }

    if (res != CURLE_OK) {
        remove(dest_path.c_str());
        err_msg = std::string("Falha no download: ") + curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        return false;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code < 200 || http_code >= 300) {
        remove(dest_path.c_str());
        err_msg = "Servidor respondeu com código HTTP " + std::to_string(http_code);
        return false;
    }

    return true;
}
