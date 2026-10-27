#ifndef CATALOG_HPP
#define CATALOG_HPP

#include <string>
#include <vector>
#include "json.hpp"

struct RequiredFile {
    std::string relative_path;
    std::string description;
    uint64_t expected_size_bytes = 0;
    std::string sha256;
    bool optional = false;
};

struct OriginalGameFiles {
    bool required = false;
    std::string explanation_pt_br;
    std::string target_directory;
    std::string drive_folder_url;
    std::vector<RequiredFile> required_files;
};

struct PackageInfo {
    std::string provider;
    std::string releases_page_url;
    std::string download_url;
    std::string file_name;
    std::string archive_type;
    uint64_t download_size_bytes = 0;
    uint64_t installed_size_bytes = 0;
    std::string sha256;
};

struct ExtraDownload {
    std::string name;
    std::string download_url;
    std::string target_path;
    std::string archive_type; // "file" ou "zip"
    std::string strip_prefix; // ex: "Black PT-BR/"
    uint64_t size_bytes = 0;
    std::string sha256;
};

struct InstallationInfo {
    std::string profile;
    std::string install_dir;
    std::string entry_nro;
    std::vector<std::string> preserve_paths;
    std::vector<std::string> steps_pt_br;
};

struct CompatibilityInfo {
    std::string status;
    std::string min_hos_version;
    bool atmosphere_required = true;
    bool applet_mode_supported = false;
    std::string recommended_launch_mode;
    std::string notes;
};

struct PortItem {
    std::string id;
    std::string title;
    std::string short_description;
    std::string description;
    std::string category;
    std::string version;
    std::string release_date;
    std::string author;
    std::string upstream_url;
    std::string thread_url;
    std::string cover_url;
    std::string icon_url;
    std::string preview_url;
    std::string forwarder_url;
    CompatibilityInfo compatibility;
    PackageInfo package;
    std::vector<ExtraDownload> extra_downloads;
    InstallationInfo installation;
    OriginalGameFiles original_files;

    uint64_t get_total_download_size() const;
    uint64_t get_update_download_size() const;
    std::string get_installed_version() const;
    bool is_installed() const;
    bool is_update_available() const;
    bool has_game_files() const;
    bool check_original_files_present(std::vector<std::string>& missing_files) const;
};

struct Category {
    std::string id;
    std::string name;
};

class Catalog {
public:
    int schema_version = 0;
    std::string project_name;
    std::string updated_at;
    std::vector<Category> categories;
    std::vector<PortItem> ports;

    bool parse(const std::string& json_content, std::string& err_msg);
    static std::string getDefaultCatalogUrl();
};

#endif // CATALOG_HPP
