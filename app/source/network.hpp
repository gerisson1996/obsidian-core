#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <string>
#include <functional>
#include <cstdint>

typedef std::function<void(uint64_t current, uint64_t total, double speed_bytes_sec)> DownloadProgressCallback;

class Network {
public:
    static bool initialize();
    static void cleanup();
    static bool is_online();

    static bool fetchString(const std::string& url, std::string& out_data, std::string& err_msg, int timeout_seconds = 15);
    static bool downloadFile(const std::string& url, 
                             const std::string& dest_path, 
                             DownloadProgressCallback progress_cb, 
                             bool* cancel_flag, 
                             std::string& err_msg);

private:
    static bool s_initialized;
};

#endif // NETWORK_HPP
