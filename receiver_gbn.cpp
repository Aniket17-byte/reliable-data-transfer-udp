#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include "packet.h"
#include "include/gbn_window.hpp"

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

int main(int argc, char* argv[]) {
    int port = (argc > 1) ? std::stoi(argv[1]) : DEFAULT_PORT;
    const char* output_file = (argc > 2) ? argv[2] : "test_output_gbn.txt";

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

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed on port " << port << "\n";
        closesocket(sockfd);
        return 1;
    }

    std::ofstream outfile(output_file, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "Could not open output file: " << output_file << "\n";
        closesocket(sockfd);
        return 1;
    }

    rdt::GbnReceiverWindow window(0);

    std::cout << "========================================================\n";
    std::cout << " [Go-Back-N Receiver] Listening on port " << port << "...\n";
    std::cout << " Output File: " << output_file << "\n";
    std::cout << "========================================================\n";

    uint64_t total_bytes_received = 0;
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    while (true) {
        Packet net_pkt{};
        int bytes_recv = recvfrom(sockfd, reinterpret_cast<char*>(&net_pkt), sizeof(Packet), 0,
                                  reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);

        if (bytes_recv <= 0) continue;

        Packet pkt = net_pkt;
        if (!deserialize_packet(pkt)) {
            std::cout << "[GBN Receiver] Checksum mismatch! Corrupted packet dropped.\n";
            continue;
        }

        // Check for FIN packet
        if (pkt.header.flags & FLAG_FIN) {
            std::cout << "[GBN Receiver] Received FIN. Sending FIN-ACK and closing.\n";
            Packet ack_pkt{};
            ack_pkt.header.seq_num = 0;
            ack_pkt.header.ack_num = pkt.header.seq_num;
            ack_pkt.header.flags = FLAG_ACK | FLAG_FIN;
            ack_pkt.header.payload_len = 0;
            serialize_packet(ack_pkt);
            sendto(sockfd, reinterpret_cast<const char*>(&ack_pkt), sizeof(Header), 0,
                   reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
            break;
        }

        if (pkt.header.flags & FLAG_DATA) {
            std::vector<uint8_t> payload(pkt.payload, pkt.payload + pkt.header.payload_len);
            auto res = window.process_packet(pkt.header.seq_num, payload);

            if (res.accepted) {
                outfile.write(reinterpret_cast<const char*>(res.payload.data()), res.payload.size());
                total_bytes_received += res.payload.size();
                std::cout << "[GBN Receiver] Accepted in-order Packet #" << pkt.header.seq_num 
                          << " (" << pkt.header.payload_len << " B) -> Sending Cumulative ACK #" << res.ack_to_send << "\n";
            } else {
                std::cout << "[GBN Receiver] Out-of-order Packet #" << pkt.header.seq_num 
                          << " (Expected #" << window.get_expected_seq_num() << ") -> Discarded & Re-ACKing #" << res.ack_to_send << "\n";
            }

            // Send ACK packet back to sender
            Packet ack_pkt{};
            ack_pkt.header.seq_num = 0;
            ack_pkt.header.ack_num = res.ack_to_send;
            ack_pkt.header.flags = FLAG_ACK;
            ack_pkt.header.payload_len = 0;
            serialize_packet(ack_pkt);

            sendto(sockfd, reinterpret_cast<const char*>(&ack_pkt), sizeof(Header), 0,
                   reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
        }
    }

    std::cout << "\n[GBN Receiver] Transfer complete. Saved " << total_bytes_received << " bytes to " << output_file << ".\n";

    outfile.close();
    closesocket(sockfd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
