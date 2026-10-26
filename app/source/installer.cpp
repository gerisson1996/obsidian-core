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

bool Installer::extractZip(const std::string& zip_path, 
                           const std::string& target_base_dir, 
                           const std::string& strip_prefix,
                           ExtractProgressCallback extract_cb,
                           std::string& err_msg) {
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

    const size_t BUFFER_SIZE = 128 * 1024;
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

        // Notifica a UI sobre o arquivo atual e o progresso
        if (extract_cb) {
            extract_cb(entry_name, (int)(i + 1), (int)gi.number_entry);
        }

        // Remove o prefixo se solicitado (ex: "Black PT-BR/")
        if (!strip_prefix.empty()) {
            if (entry_name.compare(0, strip_prefix.size(), strip_prefix) == 0) {
                entry_name = entry_name.substr(strip_prefix.size());
            } else if (entry_name == strip_prefix || entry_name == strip_prefix.substr(0, strip_prefix.size() - 1)) {
                if ((i + 1) < gi.number_entry) unzGoToNextFile(uf);
                continue;
            }
        }

        if (entry_name.empty()) {
            if ((i + 1) < gi.number_entry) unzGoToNextFile(uf);
            continue;
        }

        if (!isPathSafe(base, entry_name)) {
            unzClose(uf);
            err_msg = "Caminho inseguro detectado dentro do compactado: " + entry_name;
            return false;
        }

        std::string full_dest_path = base + entry_name;

        if (entry_name.back() == '/') {
            ensureDirectoryExists(full_dest_path);
        } else {
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

            setvbuf(out_file, NULL, _IOFBF, 512 * 1024);

            int bytes_read = 0;
            do {
                bytes_read = unzReadCurrentFile(uf, buffer.data(), BUFFER_SIZE);
                if (bytes_read < 0) {
                    fclose(out_file);
                    unzCloseCurrentFile(uf);
                    unzClose(uf);
                    err_msg = "Erro de leitura nos dados de: " + entry_name;
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
                err_msg = "Erro ao avançar no arquivo compactado.";
                return false;
            }
        }
    }

    unzClose(uf);
    return true;
}

static bool downloadAndVerify(const std::string& url, 
                             const std::string& final_dest_path, 
                             const std::string& expected_sha256, 
                             DownloadProgressCallback progress_cb, 
                             bool* cancel_flag, 
                             std::string& err_msg) {
    std::string temp_path = final_dest_path + ".tmp";
    
    size_t last_slash = final_dest_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        Installer::ensureDirectoryExists(final_dest_path.substr(0, last_slash));
    }

    if (!Network::downloadFile(url, temp_path, progress_cb, cancel_flag, err_msg)) {
        remove(temp_path.c_str());
        return false;
    }

    if (!expected_sha256.empty()) {
        std::string computed = SHA256::hashFile(temp_path);
        if (toLower(computed) != toLower(expected_sha256)) {
            remove(temp_path.c_str());
            err_msg = "SHA-256 incorreto no arquivo baixado.";
            return false;
        }
    }

    remove(final_dest_path.c_str());
    if (rename(temp_path.c_str(), final_dest_path.c_str()) != 0) {
        remove(temp_path.c_str());
        err_msg = "Erro ao mover arquivo temporário para o destino final.";
        return false;
    }

    return true;
}

bool Installer::installPort(const PortItem& port, 
                            DownloadProgressCallback progress_cb, 
                            ExtractProgressCallback extract_cb,
                            bool* cancel_flag, 
                            std::string& status_msg) {
    if (port.package.download_url.empty()) {
        status_msg = "Este port ainda não possui URL de download configurada.";
        return false;
    }

    uint64_t total_needed = port.get_total_download_size() + 200 * 1024 * 1024;
    uint64_t free_bytes = 0;
    if (!checkFreeSpace(port.installation.install_dir, total_needed, free_bytes)) {
        status_msg = "Espaço insuficiente no MicroSD!\nNecessário: " + 
                     std::to_string(total_needed / (1024 * 1024)) + " MB, Disponível: " + 
                     std::to_string(free_bytes / (1024 * 1024)) + " MB.";
        return false;
    }

    ensureDirectoryExists(port.installation.install_dir);

    // 1. Baixar executável principal (.nro)
    status_msg = "Baixando executável " + port.package.file_name + "...";
    std::string err;
    if (!downloadAndVerify(port.package.download_url, 
                           port.installation.entry_nro, 
                           port.package.sha256, 
                           progress_cb, 
                           cancel_flag, 
                           err)) {
        status_msg = err;
        return false;
    }

    // 2. Baixar arquivos e dados do jogo
    for (size_t i = 0; i < port.extra_downloads.size(); i++) {
        if (cancel_flag && *cancel_flag) {
            status_msg = "Download cancelado.";
            return false;
        }

        const auto& extra = port.extra_downloads[i];

        if (extra.archive_type == "zip") {
            status_msg = "Baixando pacote " + extra.name + " (" + std::to_string(i + 1) + "/" + std::to_string(port.extra_downloads.size()) + ")...";
            std::string temp_zip = port.installation.install_dir + "/download_data.zip";
            if (!downloadAndVerify(extra.download_url, 
                                   temp_zip, 
                                   extra.sha256, 
                                   progress_cb, 
                                   cancel_flag, 
                                   err)) {
                remove(temp_zip.c_str());
                status_msg = "Erro ao baixar " + extra.name + ":\n" + err;
                return false;
            }

            status_msg = "Descompactando arquivos e configurando pastas...";
            std::string extract_err;
            if (!extractZip(temp_zip, extra.target_path, extra.strip_prefix, extract_cb, extract_err)) {
                remove(temp_zip.c_str());
                status_msg = "Erro ao extrair pacote do jogo:\n" + extract_err;
                return false;
            }

            remove(temp_zip.c_str());
        } else {
            status_msg = "Baixando " + extra.name + "...";
            if (!downloadAndVerify(extra.download_url, 
                                   extra.target_path, 
                                   extra.sha256, 
                                   progress_cb, 
                                   cancel_flag, 
                                   err)) {
                status_msg = "Erro ao baixar " + extra.name + ":\n" + err;
                return false;
            }
        }
    }

    status_msg = "Instalação concluída com sucesso!\nTodos os arquivos foram extraídos e organizados.";
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
