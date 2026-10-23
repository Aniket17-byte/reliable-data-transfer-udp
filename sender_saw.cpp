#include <iostream>
#include <fstream>
#include <vector>
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
    #include <sys/time.h>
#endif

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define TIMEOUT_SEC 1

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

#ifdef _WIN32
    DWORD timeout = TIMEOUT_SEC * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    std::ifstream inFile("test_input.txt", std::ios::binary);
    if (!inFile.is_open()) {
        std::cerr << "Failed to open input file! Make sure test_input.txt exists.\n";
        return 1;
    }

    uint32_t seqNum = 0;
    char buffer[MAX_PAYLOAD_SIZE];

    while (!inFile.eof()) {
        inFile.read(buffer, MAX_PAYLOAD_SIZE);
        std::streamsize bytesRead = inFile.gcount();
        if (bytesRead <= 0) break;

        Packet pkt{};
        pkt.header.seq_num = seqNum;
        pkt.header.ack_num = 0;
        pkt.header.payload_len = static_cast<uint16_t>(bytesRead);
        pkt.header.flags = FLAG_DATA;
        std::memcpy(pkt.payload, buffer, bytesRead);

        pkt.header.checksum = 0;
        pkt.header.checksum = compute_checksum(&pkt, sizeof(Header) + bytesRead);

        Packet netPkt = pkt;
        serialize_packet(netPkt); // Fixed: removed &

        bool ackReceived = false;
        while (!ackReceived) {
            sendto(sockfd, reinterpret_cast<char*>(&netPkt), sizeof(Header) + bytesRead, 0,
                   (sockaddr*)&serverAddr, sizeof(serverAddr));
            std::cout << "[Sender] Sent Packet Seq: " << seqNum << " (" << bytesRead << " bytes). Waiting for ACK...\n";

            Packet ackPkt{};
            sockaddr_in ackAddr{};
            sockaddr_in* pAckAddr = &ackAddr;
            socklen_t ackAddrLen = sizeof(ackAddr);

            int res = recvfrom(sockfd, reinterpret_cast<char*>(&ackPkt), sizeof(Packet), 0,
                               (sockaddr*)pAckAddr, &ackAddrLen);

            if (res > 0) {
                deserialize_packet(ackPkt); // Fixed: removed &
                if ((ackPkt.header.flags & FLAG_ACK) && ackPkt.header.ack_num == seqNum) {
                    std::cout << "[Sender] ACK " << ackPkt.header.ack_num << " Received!\n";
                    ackReceived = true;
                    seqNum++;
                }
            } else {
                std::cout << "[Sender] TIMEOUT! Resending Packet Seq: " << seqNum << "...\n";
            }
        }
    }

    std::cout << "[Sender] File transfer complete using Stop-and-Wait ARQ.\n";

    inFile.close();
#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
    return 0;
}