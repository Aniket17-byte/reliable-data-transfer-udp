#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <string>
#include <cstring>
#include <thread>
#include "packet.hpp"
#include "checksum.hpp"
#include "socket_utils.hpp"
#include "adaptive_rto.hpp"

int main(int argc, char* argv[]) {
    NetUtils::init_networking();

    std::string filename = "input.dat";
    std::string emulator_ip = "127.0.0.1";
    uint16_t emulator_port = 9001;
    uint16_t sender_port = 9000;
    bool use_adaptive_rto = true;
    double timeout_ms = 200.0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--file" && i + 1 < argc) filename = argv[++i];
        else if (arg == "--emu-port" && i + 1 < argc) emulator_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (arg == "--port" && i + 1 < argc) sender_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (arg == "--fixed-rto" && i + 1 < argc) {
            use_adaptive_rto = false;
            timeout_ms = std::stod(argv[++i]);
        }
    }

    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "[Sender-SW] Could not open file: " << filename << "\n";
        return 1;
    }
    std::vector<uint8_t> file_bytes((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
    file.close();

    size_t total_file_size = file_bytes.size();
    size_t total_packets = (total_file_size + MAX_PAYLOAD_SIZE - 1) / MAX_PAYLOAD_SIZE;
    if (total_packets == 0) total_packets = 1;

    socket_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in bind_addr = NetUtils::create_address("0.0.0.0", sender_port);
    if (bind(sock, (sockaddr*)&bind_addr, sizeof(bind_addr)) != 0) {
        std::cerr << "[Sender-SW] Failed to bind to port " << sender_port << "\n";
        return 1;
    }
    NetUtils::set_socket_nonblocking(sock, true);
    sockaddr_in emu_dest = NetUtils::create_address(emulator_ip, emulator_port);

    AdaptiveRTO rto_tracker(timeout_ms);
    uint32_t current_seq = 0;
    uint64_t total_transmissions = 0;
    uint64_t retransmissions = 0;

    auto transfer_start = std::chrono::steady_clock::now();

    while (current_seq < total_packets) {
        size_t offset = static_cast<size_t>(current_seq) * MAX_PAYLOAD_SIZE;
        size_t chunk_len = std::min(MAX_PAYLOAD_SIZE, total_file_size - offset);
        const uint8_t* chunk_ptr = (chunk_len > 0) ? (file_bytes.data() + offset) : nullptr;

        Packet pkt(PacketType::DATA, current_seq, 0, chunk_ptr, static_cast<uint16_t>(chunk_len));
        Checksum::stamp_packet_checksum(pkt);
        auto pkt_bytes = pkt.serialize();

        bool ack_received = false;
        int packet_attempts = 0;

        while (!ack_received) {
            // Transmit packet
            auto send_time = std::chrono::steady_clock::now();
            sendto(sock, (const char*)pkt_bytes.data(), static_cast<int>(pkt_bytes.size()),
                   0, (sockaddr*)&emu_dest, sizeof(emu_dest));
            total_transmissions++;
            packet_attempts++;
            if (packet_attempts > 1) retransmissions++;

            double current_rto = use_adaptive_rto ? rto_tracker.get_rto_ms() : timeout_ms;

            // Wait for matching ACK until timeout
            while (true) {
                auto now = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double, std::milli>(now - send_time).count();
                if (elapsed >= current_rto) {
                    // Timeout occurred
                    if (use_adaptive_rto) rto_tracker.on_timeout();
                    break; // Retransmit
                }

                uint8_t rx_buf[2048];
                int r = recvfrom(sock, (char*)rx_buf, sizeof(rx_buf), 0, nullptr, nullptr);
                if (r > 0) {
                    Packet ack_pkt;
                    if (Packet::deserialize(rx_buf, r, ack_pkt)) {
                        if (Checksum::verify_packet_checksum(ack_pkt) &&
                            ack_pkt.header.type == static_cast<uint16_t>(PacketType::ACK) &&
                            ack_pkt.header.ack_num == current_seq) {
                            
                            // Success!
                            ack_received = true;
                            if (use_adaptive_rto && packet_attempts == 1) {
                                double sample_rtt = std::chrono::duration<double, std::milli>(now - send_time).count();
                                rto_tracker.update_rtt(sample_rtt);
                            }
                            break;
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::microseconds(300));
            }
        }
        current_seq++;
    }

    // Send FIN
    Packet fin_pkt(PacketType::FIN, current_seq, 0, nullptr, 0);
    Checksum::stamp_packet_checksum(fin_pkt);
    auto fin_bytes = fin_pkt.serialize();

    bool fin_acked = false;
    for (int retry = 0; retry < 10 && !fin_acked; ++retry) {
        sendto(sock, (const char*)fin_bytes.data(), static_cast<int>(fin_bytes.size()), 0,
               (sockaddr*)&emu_dest, sizeof(emu_dest));
        auto wait_start = std::chrono::steady_clock::now();
        while (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - wait_start).count() < 100.0) {
            uint8_t rx_buf[1024];
            int r = recvfrom(sock, (char*)rx_buf, sizeof(rx_buf), 0, nullptr, nullptr);
            if (r > 0) {
                Packet p;
                if (Packet::deserialize(rx_buf, r, p) && p.header.type == static_cast<uint16_t>(PacketType::FIN_ACK)) {
                    fin_acked = true;
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    auto transfer_end = std::chrono::steady_clock::now();
    double total_time_sec = std::chrono::duration<double>(transfer_end - transfer_start).count();
    double throughput_kbps = (total_file_size / 1024.0) / total_time_sec;
    double throughput_mbps = (total_file_size * 8.0 / 1e6) / total_time_sec;

    std::cout << "{\n"
              << "  \"protocol\": \"SW\",\n"
              << "  \"status\": \"SUCCESS\",\n"
              << "  \"file_size_bytes\": " << total_file_size << ",\n"
              << "  \"transfer_time_sec\": " << total_time_sec << ",\n"
              << "  \"throughput_kbps\": " << throughput_kbps << ",\n"
              << "  \"throughput_mbps\": " << throughput_mbps << ",\n"
              << "  \"total_transmissions\": " << total_transmissions << ",\n"
              << "  \"retransmissions\": " << retransmissions << ",\n"
              << "  \"retransmission_ratio\": " << (total_transmissions > 0 ? (double)retransmissions / total_transmissions : 0.0) << ",\n"
              << "  \"final_rto_ms\": " << rto_tracker.get_rto_ms() << "\n"
              << "}\n";

    NetUtils::close_socket(sock);
    NetUtils::cleanup_networking();
    return 0;
}
