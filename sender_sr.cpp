#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>
#include "packet.h"
#include "include/sr_window.hpp"
#include "include/rtt_estimator.hpp"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define closesocket close
#endif

#define DEFAULT_PORT 8083
#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_WINDOW_SIZE 8

int main(int argc, char* argv[]) {
    const char* filename = (argc > 1) ? argv[1] : "test_input.txt";
    int port = (argc > 2) ? std::stoi(argv[2]) : DEFAULT_PORT;
    const char* receiver_ip = (argc > 3) ? argv[3] : DEFAULT_IP;
    size_t win_size = (argc > 4) ? std::stoul(argv[4]) : DEFAULT_WINDOW_SIZE;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error creating socket\n";
        return 1;
    }

#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sockfd, FIONBIO, &mode);
#else
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
#endif

    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    inet_pton(AF_INET, receiver_ip, &dest_addr.sin_addr);

    std::ifstream infile(filename, std::ios::binary);
    if (!infile.is_open()) {
        std::cerr << "Could not open input file: " << filename << "\n";
        closesocket(sockfd);
        return 1;
    }

    rdt::SrSenderWindow window(win_size, 0);
    rdt::RttEstimator rtt_est;

    std::cout << "========================================================\n";
    std::cout << " [Selective Repeat Sender] Initialized (Window Size: " << win_size << ")\n";
    std::cout << " Target: " << receiver_ip << ":" << port << " | File: " << filename << "\n";
    std::cout << "========================================================\n";

    auto start_time = std::chrono::steady_clock::now();
    uint64_t total_bytes_sent = 0;
    uint64_t total_retransmissions = 0;
    bool file_read_complete = false;

    char file_buffer[MAX_PAYLOAD_SIZE];

    while (!file_read_complete || !window.is_empty()) {
        // 1. Send packets while sender window allows
        while (!file_read_complete && window.can_send()) {
            infile.read(file_buffer, MAX_PAYLOAD_SIZE);
            std::streamsize bytes_read = infile.gcount();

            if (bytes_read <= 0) {
                file_read_complete = true;
                break;
            }

            std::vector<uint8_t> payload(file_buffer, file_buffer + bytes_read);
            uint32_t seq = window.push_packet(payload, rtt_est.get_rto_duration());

            Packet pkt{};
            pkt.header.seq_num = seq;
            pkt.header.ack_num = 0;
            pkt.header.flags = FLAG_DATA;
            pkt.header.payload_len = static_cast<uint16_t>(bytes_read);
            std::memcpy(pkt.payload, payload.data(), bytes_read);

            serialize_packet(pkt);
            sendto(sockfd, reinterpret_cast<const char*>(&pkt), sizeof(Header) + bytes_read, 0,
                   reinterpret_cast<struct sockaddr*>(&dest_addr), sizeof(dest_addr));

            rtt_est.record_send(seq);
            total_bytes_sent += bytes_read;

            std::cout << "[SR Sender] Sent Packet #" << seq 
                      << " (" << bytes_read << " B) | In-flight: " << window.get_in_flight_count() 
                      << " | Current RTO: " << rtt_est.get_rto_ms() << "ms\n";
        }

        // 2. Poll for incoming selective ACKs
        Packet ack_pkt{};
        sockaddr_in from_addr{};
        socklen_t from_len = sizeof(from_addr);

        int bytes_recv = recvfrom(sockfd, reinterpret_cast<char*>(&ack_pkt), sizeof(Packet), 0,
                                  reinterpret_cast<struct sockaddr*>(&from_addr), &from_len);

        if (bytes_recv > 0) {
            if (deserialize_packet(ack_pkt) && (ack_pkt.header.flags & FLAG_ACK)) {
                uint32_t ack_seq = ack_pkt.header.ack_num;
                bool advanced = window.process_ack(ack_seq);
                auto sample = rtt_est.record_ack(ack_seq);

                std::cout << "[SR Sender] Received Selective ACK #" << ack_seq;
                if (advanced) {
                    std::cout << " -> Window base advanced to " << window.get_send_base();
                }
                std::cout << " | SRTT: " << rtt_est.get_srtt_ms() << "ms\n";
            }
        }

        // 3. Isolated retransmission of only expired packets
        auto expired = window.get_expired_packets();
        for (uint32_t seq : expired) {
            rtt_est.handle_timeout(); // Karn's backoff
            auto pkt_opt = window.handle_timeout_packet(seq, rtt_est.get_rto_duration());

            if (pkt_opt.has_value()) {
                total_retransmissions++;
                std::cout << ">>> [SR Sender] TIMEOUT for Packet #" << seq 
                          << "! (Retransmitting ONLY this packet, tx=" << pkt_opt->tx_count 
                          << ", RTO=" << rtt_est.get_rto_ms() << "ms) <<<\n";

                Packet pkt{};
                pkt.header.seq_num = pkt_opt->seq_num;
                pkt.header.ack_num = 0;
                pkt.header.flags = FLAG_DATA;
                pkt.header.payload_len = static_cast<uint16_t>(pkt_opt->payload.size());
                std::memcpy(pkt.payload, pkt_opt->payload.data(), pkt_opt->payload.size());

                serialize_packet(pkt);
                sendto(sockfd, reinterpret_cast<const char*>(&pkt), sizeof(Header) + pkt_opt->payload.size(), 0,
                       reinterpret_cast<struct sockaddr*>(&dest_addr), sizeof(dest_addr));

                rtt_est.record_send(seq); // Flags retransmitted for Karn
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 4. Send FIN
    for (int i = 0; i < 3; ++i) {
        Packet fin_pkt{};
        fin_pkt.header.seq_num = window.get_next_seq_num();
        fin_pkt.header.flags = FLAG_FIN;
        fin_pkt.header.payload_len = 0;
        serialize_packet(fin_pkt);
        sendto(sockfd, reinterpret_cast<const char*>(&fin_pkt), sizeof(Header), 0,
               reinterpret_cast<struct sockaddr*>(&dest_addr), sizeof(dest_addr));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto end_time = std::chrono::steady_clock::now();
    double duration_sec = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() / 1000.0;
    double goodput_kbps = (total_bytes_sent / 1024.0) / (duration_sec > 0 ? duration_sec : 0.001);

    std::cout << "\n========================================================\n";
    std::cout << " [Selective Repeat Transfer Complete]\n";
    std::cout << " Transferred: " << total_bytes_sent << " bytes in " << duration_sec << " seconds\n";
    std::cout << " Goodput: " << goodput_kbps << " KB/s\n";
    std::cout << " Total Retransmissions: " << total_retransmissions << "\n";
    std::cout << " Final SRTT: " << rtt_est.get_srtt_ms() << "ms | Final RTO: " << rtt_est.get_rto_ms() << "ms\n";
    std::cout << "========================================================\n";

    infile.close();
    closesocket(sockfd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
