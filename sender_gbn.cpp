#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>
#include "packet.h"
#include "include/gbn_window.hpp"
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

#define DEFAULT_PORT 8082
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

    // Set non-blocking socket so we can send & poll for ACKs concurrently
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

    rdt::GbnSenderWindow window(win_size, 0);
    rdt::RttEstimator rtt_est;

    std::cout << "========================================================\n";
    std::cout << " [Go-Back-N Sender] Initialized (Window Size: " << win_size << ")\n";
    std::cout << " Target: " << receiver_ip << ":" << port << " | File: " << filename << "\n";
    std::cout << "========================================================\n";

    auto start_time = std::chrono::steady_clock::now();
    uint64_t total_bytes_sent = 0;
    uint64_t total_retransmissions = 0;
    bool file_read_complete = false;

    char file_buffer[MAX_PAYLOAD_SIZE];

    while (!file_read_complete || !window.is_empty()) {
        // 1. Send new packets while window allows
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

            std::cout << "[GBN Sender] Sent Packet #" << seq 
                      << " (" << bytes_read << " B) | In-flight: " << window.get_in_flight_count() 
                      << " | Current RTO: " << rtt_est.get_rto_ms() << "ms\n";
        }

        // 2. Poll for incoming ACKs (non-blocking recv)
        Packet ack_pkt{};
        sockaddr_in from_addr{};
        socklen_t from_len = sizeof(from_addr);

        int bytes_recv = recvfrom(sockfd, reinterpret_cast<char*>(&ack_pkt), sizeof(Packet), 0,
                                  reinterpret_cast<struct sockaddr*>(&from_addr), &from_len);

        if (bytes_recv > 0) {
            if (deserialize_packet(ack_pkt) && (ack_pkt.header.flags & FLAG_ACK)) {
                uint32_t ack_seq = ack_pkt.header.ack_num;
                auto ack_res = window.process_ack(ack_seq, rtt_est.get_rto_duration());

                if (ack_res.is_valid) {
                    auto sample = rtt_est.record_ack(ack_seq);
                    std::cout << "[GBN Sender] Received Cumulative ACK #" << ack_seq 
                              << " -> Window base slid to " << window.get_base() 
                              << " | SRTT: " << rtt_est.get_srtt_ms() << "ms\n";
                } else if (ack_res.is_duplicate) {
                    std::cout << "[GBN Sender] Duplicate ACK #" << ack_seq << " ignored.\n";
                }
            }
        }

        // 3. Check for timeout on oldest unacknowledged packet
        if (window.check_timeout()) {
            rtt_est.handle_timeout(); // Karn's exponential backoff!
            auto pkts_to_retransmit = window.handle_timeout(rtt_est.get_rto_duration());
            total_retransmissions += pkts_to_retransmit.size();

            std::cout << ">>> [GBN Sender] TIMEOUT! Retransmitting window of " 
                      << pkts_to_retransmit.size() << " packets from base #" << window.get_base() 
                      << " | Backoff RTO: " << rtt_est.get_rto_ms() << "ms <<<\n";

            for (const auto& buffered_pkt : pkts_to_retransmit) {
                Packet pkt{};
                pkt.header.seq_num = buffered_pkt.seq_num;
                pkt.header.ack_num = 0;
                pkt.header.flags = FLAG_DATA;
                pkt.header.payload_len = static_cast<uint16_t>(buffered_pkt.payload.size());
                std::memcpy(pkt.payload, buffered_pkt.payload.data(), buffered_pkt.payload.size());

                serialize_packet(pkt);
                sendto(sockfd, reinterpret_cast<const char*>(&pkt), sizeof(Header) + buffered_pkt.payload.size(), 0,
                       reinterpret_cast<struct sockaddr*>(&dest_addr), sizeof(dest_addr));

                rtt_est.record_send(buffered_pkt.seq_num); // Flags as retransmitted for Karn
            }
        }

        // Brief sleep to avoid 100% CPU busy-wait
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 4. Send FIN to cleanly signal end of transfer
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
    std::cout << " [GBN Transfer Complete]\n";
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
