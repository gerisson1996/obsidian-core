#ifndef SHA256_HPP
#define SHA256_HPP

#include <string>
#include <cstdint>
#include <cstddef>

class SHA256 {
public:
    SHA256();
    void update(const uint8_t* data, size_t length);
    void update(const std::string& data);
    std::string finalize();

    static std::string hashFile(const std::string& filepath);
    static std::string hashString(const std::string& str);

private:
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];

    void transform(const uint8_t block[64]);
};

#endif // SHA256_HPP
