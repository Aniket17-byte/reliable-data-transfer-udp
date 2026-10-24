#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

// Protocol Packet Types
enum class PacketType : uint16_t {
    DATA = 1,
    ACK = 2,
    NACK = 3,
    FIN = 4,
    FIN_ACK = 5
};

// Fixed header size: 16 bytes
#pragma pack(push, 1)
struct PacketHeader {
    uint16_t type;       // PacketType
    uint16_t length;     // Payload length in bytes (0 to MAX_PAYLOAD_SIZE)
    uint32_t seq_num;    // Sequence number (in packets or byte offset)
    uint32_t ack_num;    // Acknowledgement number
    uint32_t checksum;   // CRC32 checksum over header (with checksum=0) + payload
};
#pragma pack(pop)

constexpr size_t MAX_PAYLOAD_SIZE = 1400; // Fits well within standard 1500-byte MTU
constexpr size_t HEADER_SIZE = sizeof(PacketHeader);
constexpr size_t MAX_PACKET_SIZE = HEADER_SIZE + MAX_PAYLOAD_SIZE;

struct Packet {
    PacketHeader header{};
    std::vector<uint8_t> payload;

    Packet() {
        std::memset(&header, 0, sizeof(header));
    }

    Packet(PacketType type, uint32_t seq, uint32_t ack, const void* data, uint16_t len) {
        header.type = static_cast<uint16_t>(type);
        header.length = len;
        header.seq_num = seq;
        header.ack_num = ack;
        header.checksum = 0;
        if (data && len > 0) {
            payload.assign(reinterpret_cast<const uint8_t*>(data),
                           reinterpret_cast<const uint8_t*>(data) + len);
        }
    }

    // Serialize packet into continuous byte buffer
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buffer(HEADER_SIZE + payload.size());
        std::memcpy(buffer.data(), &header, HEADER_SIZE);
        if (!payload.empty()) {
            std::memcpy(buffer.data() + HEADER_SIZE, payload.data(), payload.size());
        }
        return buffer;
    }

    // Deserialize byte buffer into Packet
    static bool deserialize(const uint8_t* buffer, size_t size, Packet& out_pkt) {
        if (size < HEADER_SIZE) return false;
        std::memcpy(&out_pkt.header, buffer, HEADER_SIZE);
        if (out_pkt.header.length > MAX_PAYLOAD_SIZE) return false;
        if (size < HEADER_SIZE + out_pkt.header.length) return false;

        out_pkt.payload.resize(out_pkt.header.length);
        if (out_pkt.header.length > 0) {
            std::memcpy(out_pkt.payload.data(), buffer + HEADER_SIZE, out_pkt.header.length);
        }
        return true;
    }
};
