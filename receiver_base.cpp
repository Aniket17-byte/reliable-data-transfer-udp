#include <iostream>
#include <fstream>
#include "packet.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define closesocket close
#endif

void receive_file(const char* output_filepath, int listen_port) {
#ifdef _WIN32
    // Initialize Winsock for Windows
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return;
    }
#endif

    // 1. Create UDP Socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error creating socket\n";
        return;
    }

    // 2. Bind Socket to Local Address & Port
    sockaddr_in server_addr{}, client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(listen_port);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed\n";
        closesocket(sockfd);
        return;
    }

    // 3. Open Output File in Binary Mode
    std::ofstream outfile(output_filepath, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "Failed to open output file: " << output_filepath << "\n";
        closesocket(sockfd);
        return;
    }

    std::cout << "[Receiver] Listening on port " << listen_port << "...\n";

    // 4. Listen for incoming packets
    while (true) {
        Packet pkt{};
        ssize_t bytes_received = recvfrom(sockfd, (char*)&pkt, sizeof(Packet), 0,
                                          (struct sockaddr*)&client_addr, &addr_len);

        if (bytes_received <= 0) break;

        // Verify Checksum
        if (!deserialize_packet(pkt)) {
            std::cerr << "[Receiver] Packet corrupted! Checksum mismatch.\n";
            continue;
        }

        std::cout << "[Receiver] Received Packet Seq: " << pkt.header.seq_num 
                  << " | Payload Size: " << pkt.header.payload_len << " bytes\n";

        // Write verified payload chunk directly to output file
        outfile.write(pkt.payload, pkt.header.payload_len);
        outfile.flush();
    }

    outfile.close();
    closesocket(sockfd);

#ifdef _WIN32
    WSACleanup();
#endif
}

int main() {
    // Listens on port 8080 and writes received data to test_output.txt
    receive_file("test_output.txt", 8080);
    return 0;
}