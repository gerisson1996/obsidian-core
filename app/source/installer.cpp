#include "installer.hpp"
#include "sha256.hpp"
#include <minizip/unzip.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <algorithm>
#include <vector>

bool Installer::checkFreeSpace(const std::string& path, uint64_t required_bytes, uint64_t& available_bytes) {
    struct statvfs vfs;
    std::string check_dir = path.empty() ? "sdmc:/" : path;
    if (statvfs(check_dir.c_str(), &vfs) != 0) {
        if (statvfs("sdmc:/", &vfs) != 0) {
            available_bytes = 0;
            return true;
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
    if (target_path.find("..") != std::string::npos) return false;
    if (target_path.find('\\') != std::string::npos) return false;
    return true;
}

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

bool Installer::extractZip(const std::string& zip_path, const std::string& target_base_dir, std::string& err_msg) {
    unzFile uf = unzOpen64(zip_path.c_str());
    if (!uf) {
        err_msg = "Falha ao abrir pacote compactado .zip.";
        return false;
    }

    unz_global_info64 gi;
    if (unzGetGlobalInfo64(uf, &gi) != UNZ_OK) {
        unzClose(uf);
        err_msg = "Arquivo .zip corrompido ou inválido.";
        return false;
    }

    std::string base = target_base_dir;
    if (!base.empty() && base.back() != '/') base += '/';

    ensureDirectoryExists(base);

    const size_t BUFFER_SIZE = 64 * 1024;
    std::vector<char> buffer(BUFFER_SIZE);

    for (uLong i = 0; i < gi.number_entry; i++) {
        unz_file_info64 file_info;
        char filename_inzip[512];

        if (unzGetCurrentFileInfo64(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0) != UNZ_OK) {
            unzClose(uf);
            err_msg = "Erro ao ler entrada interna do .zip.";
            return false;
        }

        std::string entry_name = filename_inzip;
        if (!isPathSafe(base, entry_name)) {
            unzClose(uf);
            err_msg = "Caminho inseguro detectado dentro do arquivo compactado.";
            return false;
        }

        std::string full_dest_path = base + entry_name;

        // Se for um diretório
        if (entry_name.back() == '/') {
            ensureDirectoryExists(full_dest_path);
        } else {
            // Garante que a pasta pai exista
            size_t last_slash = full_dest_path.find_last_of('/');
            if (last_slash != std::string::npos) {
                ensureDirectoryExists(full_dest_path.substr(0, last_slash));
            }

            if (unzOpenCurrentFile(uf) != UNZ_OK) {
                unzClose(uf);
                err_msg = "Falha ao descompactar: " + entry_name;
                return false;
            }

            FILE* out_file = fopen(full_dest_path.c_str(), "wb");
            if (!out_file) {
                unzCloseCurrentFile(uf);
                unzClose(uf);
                err_msg = "Falha ao gravar no SD: " + full_dest_path;
                return false;
            }

            int bytes_read = 0;
            do {
                bytes_read = unzReadCurrentFile(uf, buffer.data(), BUFFER_SIZE);
                if (bytes_read < 0) {
                    fclose(out_file);
                    unzCloseCurrentFile(uf);
                    unzClose(uf);
                    err_msg = "Erro de leitura nos dados compactados de: " + entry_name;
                    return false;
                }
                if (bytes_read > 0) {
                    fwrite(buffer.data(), 1, bytes_read, out_file);
                }
            } while (bytes_read > 0);

            fclose(out_file);
            unzCloseCurrentFile(uf);
        }

        if ((i + 1) < gi.number_entry) {
            if (unzGoToNextFile(uf) != UNZ_OK) {
                unzClose(uf);
                err_msg = "Erro ao avançar para próximo arquivo no .zip.";
                return false;
            }
        }
    }

    unzClose(uf);
    return true;
}

bool Installer::installPort(const PortItem& port, 
                            DownloadProgressCallback progress_cb, 
                            bool* cancel_flag, 
                            std::string& status_msg) {
    if (port.package.download_url.empty()) {
        status_msg = "Este port ainda não possui URL de download configurada.";
        return false;
    }

    bool is_zip = (port.package.archive_type == "zip" || 
                   port.package.download_url.find(".zip") != std::string::npos ||
                   port.package.file_name.find(".zip") != std::string::npos);

    // 1. Checagem de espaço disponível
    uint64_t free_bytes = 0;
    uint64_t needed = port.package.download_size_bytes > 0 ? (port.package.download_size_bytes * 2 + 50 * 1024 * 1024) : (100 * 1024 * 1024);
    if (!checkFreeSpace(port.installation.install_dir, needed, free_bytes)) {
        status_msg = "Espaço insuficiente no cartão MicroSD!\nNecessário: " + 
                     std::to_string(needed / (1024 * 1024)) + " MB, Disponível: " + 
                     std::to_string(free_bytes / (1024 * 1024)) + " MB.";
        return false;
    }

    // 2. Garantir diretório base
    ensureDirectoryExists(port.installation.install_dir);

    // 3. Download do arquivo
    std::string temp_path;
    if (is_zip) {
        temp_path = port.installation.install_dir + "/bundle.zip.tmp";
    } else {
        temp_path = port.installation.entry_nro + ".tmp";
    }

    std::string err;
    status_msg = "Baixando pacote em alta velocidade...";
    if (!Network::downloadFile(port.package.download_url, temp_path, progress_cb, cancel_flag, err)) {
        status_msg = err;
        remove(temp_path.c_str());
        return false;
    }

    // 4. Verificação de integridade SHA-256 (se cadastrado)
    if (!port.package.sha256.empty()) {
        status_msg = "Verificando integridade dos dados...";
        std::string computed_hash = SHA256::hashFile(temp_path);
        if (toLower(computed_hash) != toLower(port.package.sha256)) {
            remove(temp_path.c_str());
            status_msg = "Falha no download: SHA-256 não confere!\nO arquivo pode estar corrompido.";
            return false;
        }
    }

    // 5. Instalação e Extração
    if (is_zip) {
        status_msg = "Extraindo arquivos do jogo e montando pastas...";
        std::string extract_err;
        // Extrai na pasta switch/ (onde os subdiretórios como switch/black/... se instalam)
        std::string base_dir = "sdmc:/switch/";
        if (!extractZip(temp_path, base_dir, extract_err)) {
            remove(temp_path.c_str());
            status_msg = "Erro ao extrair pacote: " + extract_err;
            return false;
        }
        remove(temp_path.c_str());
        status_msg = "Jogo instalado e configurado com sucesso!\nPastas e arquivos prontos para jogar.";
    } else {
        // Substituição atômica de .nro
        remove(port.installation.entry_nro.c_str());
        if (rename(temp_path.c_str(), port.installation.entry_nro.c_str()) != 0) {
            remove(temp_path.c_str());
            status_msg = "Erro ao mover arquivo para o destino final.";
            return false;
        }
        status_msg = "Port instalado com sucesso em:\n" + port.installation.entry_nro;
    }

    return true;
}

bool Installer::uninstallPort(const PortItem& port, std::string& status_msg) {
    if (port.installation.entry_nro.empty()) {
        status_msg = "Port não configurado.";
        return false;
    }

    if (remove(port.installation.entry_nro.c_str()) == 0) {
        status_msg = "Executável .nro removido com sucesso.\n(Arquivos de jogo e configurações foram preservados).";
        return true;
    }

    status_msg = "Falha ao remover o executável do port.";
    return false;
}
