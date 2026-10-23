#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include "packet.h"
#include "include/sr_window.hpp"

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
#define DEFAULT_WINDOW_SIZE 8

int main(int argc, char* argv[]) {
    int port = (argc > 1) ? std::stoi(argv[1]) : DEFAULT_PORT;
    const char* output_file = (argc > 2) ? argv[2] : "test_output_sr.txt";
    size_t win_size = (argc > 3) ? std::stoul(argv[3]) : DEFAULT_WINDOW_SIZE;

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

    rdt::SrReceiverWindow window(win_size, 0);

    std::cout << "========================================================\n";
    std::cout << " [Selective Repeat Receiver] Listening on port " << port << " (Window Size: " << win_size << ")...\n";
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
            std::cout << "[SR Receiver] Checksum mismatch! Corrupted packet dropped.\n";
            continue;
        }

        // Check for FIN packet
        if (pkt.header.flags & FLAG_FIN) {
            std::cout << "[SR Receiver] Received FIN. Sending FIN-ACK and closing.\n";
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

            if (!res.deliverable_packets.empty()) {
                for (const auto& delivered : res.deliverable_packets) {
                    outfile.write(reinterpret_cast<const char*>(delivered.payload.data()), delivered.payload.size());
                    total_bytes_received += delivered.payload.size();
                }
                std::cout << "[SR Receiver] Base arrived! Delivered " << res.deliverable_packets.size() 
                          << " contiguous packets to file. Window base advanced to " << window.get_rcv_base() << "\n";
            } else if (res.action == rdt::SrReceiverWindow::ProcessAction::BUFFER_OUT_OF_ORDER) {
                std::cout << "[SR Receiver] Buffered out-of-order Packet #" << pkt.header.seq_num 
                          << " (Buffer count: " << window.get_buffered_out_of_order_count() << ")\n";
            } else if (res.action == rdt::SrReceiverWindow::ProcessAction::DUPLICATE_PAST_WINDOW) {
                std::cout << "[SR Receiver] Duplicate packet #" << pkt.header.seq_num 
                          << " from past window -> Re-sending ACK to unblock sender\n";
            }

            // Send Selective ACK
            Packet ack_pkt{};
            ack_pkt.header.seq_num = 0;
            ack_pkt.header.ack_num = res.ack_to_send;
            ack_pkt.header.flags = FLAG_ACK;
            ack_pkt.header.payload_len = 0;
            serialize_packet(ack_pkt);

            sendto(sockfd, reinterpret_cast<const char*>(&ack_pkt), sizeof(Header), 0,
                   reinterpret_cast<struct sockaddr*>(&client_addr), client_len);

            std::cout << "[SR Receiver] Sent Selective ACK #" << res.ack_to_send << "\n";
        }
    }

    std::cout << "\n[SR Receiver] Transfer complete. Saved " << total_bytes_received << " bytes to " << output_file << ".\n";

    outfile.close();
    closesocket(sockfd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
