#include <src/uuid.h>
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include <array>

namespace orion {

std::string
generate_uuid_v4(const std::string& seed)
{
    std::array<uint32_t, 4> data = {
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF
    };

    for(int i = 0; i < (int)seed.length(); ++i) {
        uint8_t byte = ((uint32_t)seed[i]) & 0xff;
        ((uint8_t*)data.data())[i % 16] ^= byte;
    }

    // Apply RFC 4122 variant and version
    unsigned char* bytes = reinterpret_cast<unsigned char*>(data.data());
    bytes[6] = (bytes[6] & 0x0F) | 0x40; // version 4
    bytes[8] = (bytes[8] & 0x3F) | 0x80; // variant 1 (10xx)

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << data[0] << "-"
        << std::setw(4) << (data[1] >> 16) << "-"
        << std::setw(4) << (data[1] & 0xFFFF) << "-"
        << std::setw(4) << (data[2] >> 16) << "-"
        << std::setw(4) << (data[2] & 0xFFFF)
        << std::setw(8) << data[3];

    return oss.str();
}

std::string
generate_uuid_v4()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

    std::array<uint32_t, 4> data;
    for (auto &d : data) {
        d = dist(gen);
    }

    // Apply RFC 4122 variant and version
    unsigned char* bytes = reinterpret_cast<unsigned char*>(data.data());
    bytes[6] = (bytes[6] & 0x0F) | 0x40; // version 4
    bytes[8] = (bytes[8] & 0x3F) | 0x80; // variant 1 (10xx)

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << data[0] << "-"
        << std::setw(4) << (data[1] >> 16) << "-"
        << std::setw(4) << (data[1] & 0xFFFF) << "-"
        << std::setw(4) << (data[2] >> 16) << "-"
        << std::setw(4) << (data[2] & 0xFFFF)
        << std::setw(8) << data[3];

    return oss.str();
}

} // namespace orion