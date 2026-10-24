#pragma once
#include <cstdint>
#include <cstddef>
#include <array>
#include "packet.hpp"

namespace Checksum {

// Precomputed CRC-32 (IEEE 802.3 standard polynomial: 0xEDB88320)
inline const std::array<uint32_t, 256>& get_crc32_table() {
    static const auto table = []() {
        std::array<uint32_t, 256> tbl{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (size_t j = 0; j < 8; ++j) {
                if (c & 1) {
                    c = 0xEDB88320L ^ (c >> 1);
                } else {
                    c = c >> 1;
                }
            }
            tbl[i] = c;
        }
        return tbl;
    }();
    return table;
}

inline uint32_t calculate_crc32(const uint8_t* data, size_t length) {
    const auto& table = get_crc32_table();
    uint32_t crc = 0xFFFFFFFFL;
    for (size_t i = 0; i < length; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFL;
}

// Compute CRC32 for an entire Packet:
// The header checksum field is treated as 0 during computation.
inline uint32_t compute_packet_checksum(const Packet& pkt) {
    PacketHeader hdr_copy = pkt.header;
    hdr_copy.checksum = 0;

    const auto& table = get_crc32_table();
    uint32_t crc = 0xFFFFFFFFL;

    const uint8_t* hdr_bytes = reinterpret_cast<const uint8_t*>(&hdr_copy);
    for (size_t i = 0; i < sizeof(PacketHeader); ++i) {
        crc = table[(crc ^ hdr_bytes[i]) & 0xFF] ^ (crc >> 8);
    }

    for (size_t i = 0; i < pkt.payload.size(); ++i) {
        crc = table[(crc ^ pkt.payload[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFFL;
}

inline bool verify_packet_checksum(const Packet& pkt) {
    uint32_t expected = pkt.header.checksum;
    uint32_t calculated = compute_packet_checksum(pkt);
    return expected == calculated;
}

inline void stamp_packet_checksum(Packet& pkt) {
    pkt.header.checksum = compute_packet_checksum(pkt);
}

} // namespace Checksum
