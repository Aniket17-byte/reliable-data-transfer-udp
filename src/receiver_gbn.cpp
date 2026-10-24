#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include "packet.hpp"
#include "checksum.hpp"
#include "socket_utils.hpp"

int main(int argc, char* argv[]) {
    NetUtils::init_networking();

    std::string output_filename = "output.dat";
    std::string emulator_ip = "127.0.0.1";
    uint16_t listen_port = 9002;       // Port where emulator forwards DATA
    uint16_t emulator_rev_port = 9003; // Port where emulator listens for ACKs

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) output_filename = argv[++i];
        else if (arg == "--port" && i + 1 < argc) listen_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (arg == "--emu-port" && i + 1 < argc) emulator_rev_port = static_cast<uint16_t>(std::stoi(argv[++i]));
    }

    socket_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in bind_addr = NetUtils::create_address("0.0.0.0", listen_port);
    if (bind(sock, (sockaddr*)&bind_addr, sizeof(bind_addr)) != 0) {
        std::cerr << "[Receiver-GBN] Failed to bind to port " << listen_port << "\n";
        return 1;
    }

    sockaddr_in emu_rev_dest = NetUtils::create_address(emulator_ip, emulator_rev_port);

    std::ofstream outfile(output_filename, std::ios::binary | std::ios::trunc);
    if (!outfile) {
        std::cerr << "[Receiver-GBN] Could not open output file: " << output_filename << "\n";
        return 1;
    }

    uint32_t expected_seq_num = 0;
    uint8_t rx_buffer[65535];

    auto send_ack = [&](uint32_t ack_num) {
        Packet ack(PacketType::ACK, 0, ack_num, nullptr, 0);
        Checksum::stamp_packet_checksum(ack);
        auto bytes = ack.serialize();
        sendto(sock, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()),
               0, (sockaddr*)&emu_rev_dest, sizeof(emu_rev_dest));
    };

    while (true) {
        int bytes_read = recvfrom(sock, (char*)rx_buffer, sizeof(rx_buffer), 0,
                                  nullptr, nullptr);
        if (bytes_read <= 0) continue;

        Packet pkt;
        if (!Packet::deserialize(rx_buffer, bytes_read, pkt)) {
            continue; // Malformed packet
        }

        // Checksum verification
        if (!Checksum::verify_packet_checksum(pkt)) {
            // Corrupted packet detected! Discard silently.
            continue;
        }

        if (pkt.header.type == static_cast<uint16_t>(PacketType::DATA)) {
            if (pkt.header.seq_num == expected_seq_num) {
                // In-order packet received
                if (!pkt.payload.empty()) {
                    outfile.write(reinterpret_cast<const char*>(pkt.payload.data()), pkt.payload.size());
                }
                send_ack(expected_seq_num);
                expected_seq_num++;
            } else if (pkt.header.seq_num < expected_seq_num) {
                // Duplicate packet: re-send ACK
                send_ack(pkt.header.seq_num);
            } else {
                // Out-of-order packet: re-send ACK for last in-order packet
                if (expected_seq_num > 0) {
                    send_ack(expected_seq_num - 1);
                }
            }
        } else if (pkt.header.type == static_cast<uint16_t>(PacketType::FIN)) {
            // Acknowledge FIN
            Packet fin_ack(PacketType::FIN_ACK, 0, pkt.header.seq_num, nullptr, 0);
            Checksum::stamp_packet_checksum(fin_ack);
            auto bytes = fin_ack.serialize();
            for (int i = 0; i < 3; ++i) {
                sendto(sock, (const char*)bytes.data(), static_cast<int>(bytes.size()), 0,
                       (sockaddr*)&emu_rev_dest, sizeof(emu_rev_dest));
            }
            break; // Finished
        }
    }

    outfile.close();
    NetUtils::close_socket(sock);
    NetUtils::cleanup_networking();
    return 0;
}
