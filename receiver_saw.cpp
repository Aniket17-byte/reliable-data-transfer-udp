#include <iostream>
#include <fstream>
#include <cstring>
#include "packet.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

#define PORT 8080

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error creating socket\n";
        return 1;
    }

    sockaddr_in serverAddr{}, clientAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Bind failed\n";
        return 1;
    }

    std::ofstream outFile("test_output.txt", std::ios::binary);
    std::cout << "[Receiver] Listening for Stop-and-Wait ARQ on port " << PORT << "...\n";

    uint32_t expectedSeq = 0;

    while (true) {
        Packet netPkt{};
        socklen_t clientLen = sizeof(clientAddr);

        int bytesReceived = recvfrom(sockfd, reinterpret_cast<char*>(&netPkt), sizeof(Packet), 0,
                                   (sockaddr*)&clientAddr, &clientLen);
        if (bytesReceived <= 0) continue;

        Packet pkt = netPkt;
        deserialize_packet(pkt); // Fixed: removed &

        uint16_t recvChecksum = pkt.header.checksum;
        pkt.header.checksum = 0;
        uint16_t calcChecksum = compute_checksum(&pkt, sizeof(Header) + pkt.header.payload_len);

        if (recvChecksum != calcChecksum) {
            std::cout << "[Receiver] Corrupted packet detected! Ignoring...\n";
            continue;
        }

        if (pkt.header.seq_num == expectedSeq) {
            std::cout << "[Receiver] Received Expected Packet Seq: " << pkt.header.seq_num << "\n";
            outFile.write(pkt.payload, pkt.header.payload_len);
            outFile.flush();
            expectedSeq++;
        } else {
            std::cout << "[Receiver] Duplicate Packet Seq: " << pkt.header.seq_num << ". Resending ACK...\n";
        }

        Packet ackPkt{};
        ackPkt.header.seq_num = 0;
        ackPkt.header.ack_num = pkt.header.seq_num;
        ackPkt.header.payload_len = 0;
        ackPkt.header.flags = FLAG_ACK;
        ackPkt.header.checksum = 0;
        ackPkt.header.checksum = compute_checksum(&ackPkt, sizeof(Header));

        serialize_packet(ackPkt); // Fixed: removed &
        sendto(sockfd, reinterpret_cast<char*>(&ackPkt), sizeof(Header), 0,
               (sockaddr*)&clientAddr, clientLen);
    }

    outFile.close();
#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
    return 0;
}