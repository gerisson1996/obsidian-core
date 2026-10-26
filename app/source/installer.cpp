#include "installer.hpp"
#include "sha256.hpp"
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <algorithm>

bool Installer::checkFreeSpace(const std::string& path, uint64_t required_bytes, uint64_t& available_bytes) {
    struct statvfs vfs;
    // No Switch normalmente "sdmc:/"
    std::string check_dir = path.empty() ? "sdmc:/" : path;
    if (statvfs(check_dir.c_str(), &vfs) != 0) {
        // Se falhar a verificação exata de partição, tenta na raiz
        if (statvfs("sdmc:/", &vfs) != 0) {
            available_bytes = 0;
            return true; // Fallback permissivo se statvfs não responder
        }
    }

    available_bytes = static_cast<uint64_t>(vfs.f_bavail) * static_cast<uint64_t>(vfs.f_bsize);
    return (available_bytes >= required_bytes);
}

bool Installer::ensureDirectoryExists(const std::string& dir_path) {
    if (dir_path.empty()) return true;

    std::string current;
    for (size_t i = 0; i < dir_path.size(); i++) {
        current += dir_path[i];
        if (dir_path[i] == '/' || i == dir_path.size() - 1) {
            if (current == "sdmc:/" || current == "sdmc:") continue;
            struct stat st;
            if (stat(current.c_str(), &st) != 0) {
                mkdir(current.c_str(), 0777);
            }
        }
    }
    return true;
}

bool Installer::isPathSafe(const std::string& base_dir, const std::string& target_path) {
    // Evita '..' e caracteres ilegais para prevenir Path Traversal
    if (target_path.find("..") != std::string::npos) return false;
    if (target_path.find('\\') != std::string::npos) return false;
    return true;
}

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

bool Installer::installPort(const PortItem& port, 
                            DownloadProgressCallback progress_cb, 
                            bool* cancel_flag, 
                            std::string& status_msg) {
    if (port.package.download_url.empty()) {
        status_msg = "Este port ainda não possui URL de download configurada.";
        return false;
    }

    if (!isPathSafe(port.installation.install_dir, port.installation.entry_nro)) {
        status_msg = "Caminho de instalação inseguro detectado.";
        return false;
    }

    // 1. Checagem de espaço disponível
    uint64_t free_bytes = 0;
    uint64_t needed = port.package.download_size_bytes > 0 ? (port.package.download_size_bytes + 10 * 1024 * 1024) : (50 * 1024 * 1024);
    if (!checkFreeSpace(port.installation.install_dir, needed, free_bytes)) {
        status_msg = "Espaço insuficiente no cartão SD! Necessário: " + 
                     std::to_string(needed / (1024 * 1024)) + " MB, Disponível: " + 
                     std::to_string(free_bytes / (1024 * 1024)) + " MB.";
        return false;
    }

    // 2. Garantir que as pastas de destino existam
    ensureDirectoryExists(port.installation.install_dir);

    // Se requer arquivos originais, já cria a subpasta esperada (ex: sdmc:/switch/<port>/game/)
    if (port.original_files.required && !port.original_files.target_directory.empty()) {
        ensureDirectoryExists(port.original_files.target_directory);
    }

    // 3. Download para arquivo temporário (.tmp)
    std::string temp_path = port.installation.entry_nro + ".tmp";
    std::string err;

    status_msg = "Baixando pacote do port...";
    if (!Network::downloadFile(port.package.download_url, temp_path, progress_cb, cancel_flag, err)) {
        status_msg = err;
        remove(temp_path.c_str());
        return false;
    }

    // 4. Verificação de integridade SHA-256
    if (!port.package.sha256.empty()) {
        status_msg = "Verificando integridade (SHA-256)...";
        std::string computed_hash = SHA256::hashFile(temp_path);
        if (toLower(computed_hash) != toLower(port.package.sha256)) {
            remove(temp_path.c_str());
            status_msg = "Falha de integridade: SHA-256 não confere!\nEsperado: " + 
                         port.package.sha256.substr(0, 12) + "...\nCalculado: " + 
                         computed_hash.substr(0, 12) + "...";
            return false;
        }
    }

    // 5. Instalação atômica: substituir pelo .nro final
    remove(port.installation.entry_nro.c_str());
    if (rename(temp_path.c_str(), port.installation.entry_nro.c_str()) != 0) {
        remove(temp_path.c_str());
        status_msg = "Erro ao mover arquivo temporário para o destino final.";
        return false;
    }

    status_msg = "Port instalado com sucesso em:\n" + port.installation.entry_nro;
    return true;
}

bool Installer::uninstallPort(const PortItem& port, std::string& status_msg) {
    if (port.installation.entry_nro.empty()) {
        status_msg = "Port não configurado.";
        return false;
    }

    // Preserva arquivos de save e da pasta original game/
    if (remove(port.installation.entry_nro.c_str()) == 0) {
        status_msg = "Executável .nro removido com sucesso.\n(Arquivos de jogo e configurações foram preservados).";
        return true;
    }

    status_msg = "Falha ao remover o executável do port.";
    return false;
}
