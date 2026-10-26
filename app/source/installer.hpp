#ifndef INSTALLER_HPP
#define INSTALLER_HPP

#include "catalog.hpp"
#include "network.hpp"
#include <string>

class Installer {
public:
    static bool checkFreeSpace(const std::string& path, uint64_t required_bytes, uint64_t& available_bytes);
    static bool ensureDirectoryExists(const std::string& dir_path);
    static bool isPathSafe(const std::string& base_dir, const std::string& target_path);

    static bool extractZip(const std::string& zip_path, 
                           const std::string& target_base_dir, 
                           const std::string& strip_prefix,
                           std::string& err_msg);

    static bool installPort(const PortItem& port, 
                            DownloadProgressCallback progress_cb, 
                            bool* cancel_flag, 
                            std::string& status_msg);

    static bool uninstallPort(const PortItem& port, std::string& status_msg);
};

#endif // INSTALLER_HPP
