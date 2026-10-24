#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <string>
#include <cstring>
#include <map>
#include <thread>
#include "packet.hpp"
#include "checksum.hpp"
#include "socket_utils.hpp"
#include "adaptive_rto.hpp"

struct PacketInfo {
    Packet packet;
    std::chrono::steady_clock::time_point send_time;
    int transmit_count = 0;
};

int main(int argc, char* argv[]) {
    NetUtils::init_networking();

    std::string filename = "input.dat";
    std::string emulator_ip = "127.0.0.1";
    uint16_t emulator_port = 9001; // Forward port on emulator
    uint16_t sender_port = 9000;   // Port to receive ACKs from emulator
    size_t window_size = 16;
    bool use_adaptive_rto = true;
    double fixed_timeout_ms = 200.0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--file" && i + 1 < argc) filename = argv[++i];
        else if (arg == "--window" && i + 1 < argc) window_size = std::stoul(argv[++i]);
        else if (arg == "--emu-port" && i + 1 < argc) emulator_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (arg == "--port" && i + 1 < argc) sender_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (arg == "--fixed-rto" && i + 1 < argc) {
            use_adaptive_rto = false;
            fixed_timeout_ms = std::stod(argv[++i]);
        }
    }

    // Read input file into memory
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "[Sender-GBN] Could not open file: " << filename << "\n";
        return 1;
    }
    std::vector<uint8_t> file_bytes((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
    file.close();

    size_t total_file_size = file_bytes.size();
    size_t total_packets = (total_file_size + MAX_PAYLOAD_SIZE - 1) / MAX_PAYLOAD_SIZE;
    if (total_packets == 0) total_packets = 1;

    // Sockets
    socket_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in bind_addr = NetUtils::create_address("0.0.0.0", sender_port);
    if (bind(sock, (sockaddr*)&bind_addr, sizeof(bind_addr)) != 0) {
        std::cerr << "[Sender-GBN] Failed to bind socket to port " << sender_port << "\n";
        return 1;
    }
    NetUtils::set_socket_nonblocking(sock, true);

    sockaddr_in emu_dest = NetUtils::create_address(emulator_ip, emulator_port);

    AdaptiveRTO rto_tracker(fixed_timeout_ms);
    uint32_t base = 0;
    uint32_t next_seq_num = 0;
    std::chrono::steady_clock::time_point timer_start;
    bool timer_running = false;

    std::map<uint32_t, PacketInfo> window_buffer;

    uint64_t total_transmissions = 0;
    uint64_t retransmissions = 0;
    auto transfer_start = std::chrono::steady_clock::now();

    // Helper to send packet
    auto transmit = [&](uint32_t seq) {
        auto& pinfo = window_buffer[seq];
        pinfo.send_time = std::chrono::steady_clock::now();
        pinfo.transmit_count++;
        total_transmissions++;
        if (pinfo.transmit_count > 1) retransmissions++;

        auto bytes = pinfo.packet.serialize();
        sendto(sock, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()),
               0, (sockaddr*)&emu_dest, sizeof(emu_dest));
    };

    while (base < total_packets) {
        // 1. Send packets while window is not full
        while (next_seq_num < base + window_size && next_seq_num < total_packets) {
            size_t offset = static_cast<size_t>(next_seq_num) * MAX_PAYLOAD_SIZE;
            size_t chunk_len = std::min(MAX_PAYLOAD_SIZE, total_file_size - offset);
            const uint8_t* chunk_ptr = (chunk_len > 0) ? (file_bytes.data() + offset) : nullptr;

            Packet pkt(PacketType::DATA, next_seq_num, 0, chunk_ptr, static_cast<uint16_t>(chunk_len));
            Checksum::stamp_packet_checksum(pkt);

            window_buffer[next_seq_num] = PacketInfo{pkt, {}, 0};
            transmit(next_seq_num);

            if (base == next_seq_num) {
                timer_start = std::chrono::steady_clock::now();
                timer_running = true;
            }
            next_seq_num++;
        }

        // 2. Receive ACKs
        uint8_t rx_buf[65535];
        sockaddr_in from_addr{};
        socklen_t from_len = sizeof(from_addr);

        while (true) {
            int r = recvfrom(sock, (char*)rx_buf, sizeof(rx_buf), 0, (sockaddr*)&from_addr, &from_len);
            if (r <= 0) break;

            Packet ack_pkt;
            if (Packet::deserialize(rx_buf, r, ack_pkt)) {
                if (Checksum::verify_packet_checksum(ack_pkt) &&
                    ack_pkt.header.type == static_cast<uint16_t>(PacketType::ACK)) {
                    
                    uint32_t ack_num = ack_pkt.header.ack_num;
                    if (ack_num >= base && ack_num < next_seq_num) {
                        // Karn's algorithm: Only measure RTT if packet was not retransmitted
                        if (use_adaptive_rto && window_buffer.count(ack_num)) {
                            if (window_buffer[ack_num].transmit_count == 1) {
                                auto now = std::chrono::steady_clock::now();
                                double sample_rtt = std::chrono::duration<double, std::milli>(now - window_buffer[ack_num].send_time).count();
                                rto_tracker.update_rtt(sample_rtt);
                            }
                        }

                        // Cumulative ACK advances base
                        while (base <= ack_num) {
                            window_buffer.erase(base);
                            base++;
                        }

                        if (base == next_seq_num) {
                            timer_running = false;
                        } else {
                            timer_start = std::chrono::steady_clock::now();
                            timer_running = true;
                        }
                    }
                }
            }
        }

        // 3. Check for timeout on oldest unACKed packet
        if (timer_running) {
            auto now = std::chrono::steady_clock::now();
            double elapsed_ms = std::chrono::duration<double, std::milli>(now - timer_start).count();
            double current_rto = use_adaptive_rto ? rto_tracker.get_rto_ms() : fixed_timeout_ms;

            if (elapsed_ms >= current_rto) {
                // Timeout! Go-Back-N retransmits all packets in [base, next_seq_num - 1]
                if (use_adaptive_rto) rto_tracker.on_timeout();
                timer_start = std::chrono::steady_clock::now();

                for (uint32_t i = base; i < next_seq_num; ++i) {
                    transmit(i);
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }

    // 4. Send FIN to notify receiver of completion
    Packet fin_pkt(PacketType::FIN, next_seq_num, 0, nullptr, 0);
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

    // Structured JSON output for Python test harness
    std::cout << "{\n"
              << "  \"protocol\": \"GBN\",\n"
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
