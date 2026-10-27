#include "catalog.hpp"
#include <unistd.h>
#include <sys/stat.h>

static bool fileExists(const std::string& path) {
    struct stat st;
    return (stat(path.c_str(), &st) == 0);
}

static bool writeVersionFile(const std::string& install_dir, const std::string& version) {
    if (install_dir.empty() || version.empty()) return false;
    std::string ver_path = install_dir + "/version.txt";
    FILE* fp = fopen(ver_path.c_str(), "w");
    if (!fp) return false;
    fprintf(fp, "%s\n", version.c_str());
    fclose(fp);
    return true;
}

std::string Catalog::getDefaultCatalogUrl() {
    return "https://gerisson1996.github.io/obsidian-core/catalog/ports.json";
}

uint64_t PortItem::get_total_download_size() const {
    uint64_t total = package.download_size_bytes;
    for (const auto& ed : extra_downloads) {
        total += ed.size_bytes;
    }
    return total;
}

uint64_t PortItem::get_update_download_size() const {
    return package.download_size_bytes;
}

bool PortItem::is_installed() const {
    if (installation.entry_nro.empty()) return false;
    return fileExists(installation.entry_nro);
}

std::string PortItem::get_installed_version() const {
    if (!is_installed()) return "";

    std::string ver_path = installation.install_dir + "/version.txt";
    FILE* fp = fopen(ver_path.c_str(), "r");
    if (fp) {
        char buf[64];
        if (fgets(buf, sizeof(buf), fp)) {
            std::string ver = buf;
            while (!ver.empty() && (ver.back() == '\r' || ver.back() == '\n' || ver.back() == ' ')) {
                ver.pop_back();
            }
            fclose(fp);
            if (!ver.empty()) return ver;
        } else {
            fclose(fp);
        }
    }

    // Se version.txt não existe mas o .nro está instalado, verifica se o tamanho confere com a versão atual
    struct stat st;
    if (stat(installation.entry_nro.c_str(), &st) == 0) {
        if (package.installed_size_bytes > 0 && (uint64_t)st.st_size == package.installed_size_bytes) {
            writeVersionFile(installation.install_dir, version);
            return version;
        }
    }

    return "antiga";
}

bool PortItem::is_update_available() const {
    if (!is_installed()) return false;
    std::string inst_ver = get_installed_version();
    if (inst_ver.empty() || inst_ver == "antiga") return true;
    return (inst_ver != version);
}

bool PortItem::has_game_files() const {
    if (extra_downloads.empty() && !original_files.required) {
        return true;
    }

    // 1. Verifica se os arquivos originais requeridos já estão presentes
    if (original_files.required && !original_files.required_files.empty()) {
        for (const auto& req : original_files.required_files) {
            if (!req.optional) {
                std::string full_path = original_files.target_directory + "/" + req.relative_path;
                if (fileExists(full_path)) return true;
            }
        }
    }

    // 2. Verifica se a pasta de destino dos arquivos extras contém default.xbe ou arquivos instalados
    for (const auto& ed : extra_downloads) {
        if (!ed.target_path.empty()) {
            std::string test_file = ed.target_path + "/default.xbe";
            if (fileExists(test_file)) return true;
            if (fileExists(ed.target_path)) return true;
        }
    }

    return false;
}

bool PortItem::check_original_files_present(std::vector<std::string>& missing_files) const {
    if (!original_files.required) return true;

    bool all_ok = true;
    for (const auto& req : original_files.required_files) {
        if (req.optional) continue;
        std::string full_path = original_files.target_directory + "/" + req.relative_path;
        if (!fileExists(full_path)) {
            missing_files.push_back(req.relative_path);
            all_ok = false;
        }
    }
    return all_ok;
}

bool Catalog::parse(const std::string& json_content, std::string& err_msg) {
    try {
        JsonValue root = JsonValue::parse(json_content);
        if (!root.is_object()) {
            err_msg = "Formato JSON inválido: raiz não é um objeto.";
            return false;
        }

        schema_version = root["schema_version"].as_int64(0);
        project_name = root["project"].as_string("NX Port Hub");
        updated_at = root["updated_at"].as_string("");

        categories.clear();
        const auto& cat_array = root["categories"];
        if (cat_array.is_array()) {
            for (size_t i = 0; i < cat_array.arr_val.size(); i++) {
                const auto& c = cat_array[i];
                Category cat;
                cat.id = c["id"].as_string();
                cat.name = c["name"].as_string();
                categories.push_back(cat);
            }
        }

        ports.clear();
        const auto& ports_array = root["ports"];
        if (ports_array.is_array()) {
            for (size_t i = 0; i < ports_array.arr_val.size(); i++) {
                const auto& p = ports_array[i];
                PortItem item;
                item.id = p["id"].as_string();
                item.title = p["title"].as_string();
                item.short_description = p["short_description"].as_string();
                item.description = p["description"].as_string();
                item.category = p["category"].as_string();
                item.version = p["version"].as_string();
                item.release_date = p["release_date"].as_string();
                item.author = p["author"].as_string();
                item.upstream_url = p["upstream_url"].as_string();
                item.thread_url = p["thread_url"].as_string();
                item.cover_url = p["cover_url"].as_string();
                item.icon_url = p["icon_url"].as_string();
                item.preview_url = p["preview_url"].as_string();
                if (item.preview_url.empty()) {
                    item.preview_url = p["gameplay_gif"].as_string();
                }
                item.forwarder_url = p["forwarder_url"].as_string();
                if (item.forwarder_url.empty()) {
                    item.forwarder_url = p["forwarder_nsp"].as_string();
                }

                // Compatibilidade
                const auto& comp = p["compatibility"];
                item.compatibility.status = comp["status"].as_string("desconhecido");
                item.compatibility.min_hos_version = comp["min_hos_version"].as_string();
                item.compatibility.atmosphere_required = comp["atmosphere_required"].as_bool(true);
                item.compatibility.applet_mode_supported = comp["applet_mode_supported"].as_bool(false);
                item.compatibility.recommended_launch_mode = comp["recommended_launch_mode"].as_string("title_takeover");
                item.compatibility.notes = comp["notes"].as_string();

                // Pacote
                const auto& pkg = p["package"];
                item.package.provider = pkg["provider"].as_string();
                item.package.releases_page_url = pkg["releases_page_url"].as_string();
                item.package.download_url = pkg["download_url"].as_string();
                item.package.file_name = pkg["file_name"].as_string();
                item.package.archive_type = pkg["archive_type"].as_string("nro");
                item.package.download_size_bytes = pkg["download_size_bytes"].as_int64(0);
                item.package.installed_size_bytes = pkg["installed_size_bytes"].as_int64(0);
                item.package.sha256 = pkg["sha256"].as_string();

                // Arquivos adicionais do Google Drive ou da nuvem
                const auto& extras = p["extra_downloads"];
                if (extras.is_array()) {
                    for (size_t e = 0; e < extras.arr_val.size(); e++) {
                        const auto& ed = extras[e];
                        ExtraDownload extra;
                        extra.name = ed["name"].as_string();
                        extra.download_url = ed["download_url"].as_string();
                        extra.target_path = ed["target_path"].as_string();
                        extra.archive_type = ed["archive_type"].as_string("file");
                        extra.strip_prefix = ed["strip_prefix"].as_string("");
                        extra.size_bytes = ed["size_bytes"].as_int64(0);
                        extra.sha256 = ed["sha256"].as_string();
                        item.extra_downloads.push_back(extra);
                    }
                }

                // Instalação
                const auto& inst = p["installation"];
                item.installation.profile = inst["profile"].as_string("xbox_recomp_nro");
                item.installation.install_dir = inst["install_dir"].as_string();
                item.installation.entry_nro = inst["entry_nro"].as_string();
                
                const auto& pres = inst["preserve_paths"];
                if (pres.is_array()) {
                    for (size_t k = 0; k < pres.arr_val.size(); k++) {
                        item.installation.preserve_paths.push_back(pres[k].as_string());
                    }
                }

                const auto& steps = inst["steps_pt_br"];
                if (steps.is_array()) {
                    for (size_t k = 0; k < steps.arr_val.size(); k++) {
                        item.installation.steps_pt_br.push_back(steps[k].as_string());
                    }
                }

                // Arquivos originais
                const auto& orig = p["original_game_files"];
                item.original_files.required = orig["required"].as_bool(false);
                item.original_files.explanation_pt_br = orig["explanation_pt_br"].as_string();
                item.original_files.target_directory = orig["target_directory"].as_string();
                item.original_files.drive_folder_url = orig["drive_folder_url"].as_string();

                const auto& req_files = orig["required_files"];
                if (req_files.is_array()) {
                    for (size_t k = 0; k < req_files.arr_val.size(); k++) {
                        const auto& rf = req_files[k];
                        RequiredFile file;
                        file.relative_path = rf["relative_path"].as_string();
                        file.description = rf["description"].as_string();
                        file.expected_size_bytes = rf["expected_size_bytes"].as_int64(0);
                        file.sha256 = rf["sha256"].as_string();
                        file.optional = rf["optional"].as_bool(false);
                        item.original_files.required_files.push_back(file);
                    }
                }

                ports.push_back(item);
            }
        }
        return true;
    } catch (const std::exception& ex) {
        err_msg = std::string("Exceção ao processar catálogo: ") + ex.what();
        return false;
    }
}
