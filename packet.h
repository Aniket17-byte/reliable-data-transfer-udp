#ifndef PACKET_H
#define PACKET_H

#include <cstdint>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
#endif

#define MAX_PAYLOAD_SIZE 1024

// Packet Type Flags
#define FLAG_DATA 0x01
#define FLAG_ACK  0x02
#define FLAG_SYN  0x04
#define FLAG_FIN  0x08

#pragma pack(push, 1)
struct Header {
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t checksum;
    uint16_t payload_len;
    uint8_t flags;
};
#pragma pack(pop)

struct Packet {
    Header header;
    char payload[MAX_PAYLOAD_SIZE];
};

// Internet Checksum Algorithm (16-bit One's Complement)
inline uint16_t compute_checksum(const void* data, size_t length) {
    const uint16_t* ptr = static_cast<const uint16_t*>(data);
    uint32_t sum = 0;

    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }

    if (length > 0) {
        sum += *reinterpret_cast<const uint8_t*>(ptr);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum);
}

// Convert packet header to Network Byte Order before sending
inline void serialize_packet(Packet& pkt) {
    pkt.header.checksum = 0; // Clear checksum field for calculation
    pkt.header.checksum = compute_checksum(&pkt, sizeof(Header) + pkt.header.payload_len);
    
    pkt.header.seq_num = htonl(pkt.header.seq_num);
    pkt.header.ack_num = htonl(pkt.header.ack_num);
    pkt.header.checksum = htons(pkt.header.checksum);
    pkt.header.payload_len = htons(pkt.header.payload_len);
}

// Convert packet header back to Host Byte Order after receiving
inline bool deserialize_packet(Packet& pkt) {
    uint16_t received_checksum = ntohs(pkt.header.checksum);
    
    // Restore host order for fields needed to recalculate checksum (matching serialize_packet)
    pkt.header.seq_num = ntohl(pkt.header.seq_num);
    pkt.header.ack_num = ntohl(pkt.header.ack_num);
    pkt.header.payload_len = ntohs(pkt.header.payload_len);
    
    // Checksum verification
    pkt.header.checksum = 0;
    uint16_t calculated_checksum = compute_checksum(&pkt, sizeof(Header) + pkt.header.payload_len);
    pkt.header.checksum = received_checksum;

    return (received_checksum == calculated_checksum);
}

#endif
