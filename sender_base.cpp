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

void send_file(const char* filepath, const char* receiver_ip, int receiver_port) {
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

    // 2. Set Receiver Address Details
    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(receiver_port);
    dest_addr.sin_addr.s_addr = inet_addr(receiver_ip);

    // 3. Open Input File in Binary Mode
    std::ifstream infile(filepath, std::ios::binary);
    if (!infile.is_open()) {
        std::cerr << "Failed to open file: " << filepath << "\n";
        closesocket(sockfd);
        return;
    }

    uint32_t seq_num = 0;

    // 4. Read File in Chunks and Send
    while (!infile.eof()) {
        Packet pkt{};
        infile.read(pkt.payload, MAX_PAYLOAD_SIZE);
        std::streamsize bytes_read = infile.gcount();

        if (bytes_read <= 0) break;

        // Build Packet Header
        pkt.header.seq_num = seq_num;
        pkt.header.ack_num = 0;
        pkt.header.flags = FLAG_DATA;
        pkt.header.payload_len = static_cast<uint16_t>(bytes_read);

        // Serialize fields to Network Byte Order & calculate Checksum
        serialize_packet(pkt);

        // Send over UDP socket
        sendto(sockfd, (const char*)&pkt, sizeof(Header) + bytes_read, 0, 
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));

        std::cout << "[Sender] Sent Packet Seq: " << seq_num 
                  << " | Payload Size: " << bytes_read << " bytes\n";
        
        seq_num += bytes_read;
    }

    infile.close();
    closesocket(sockfd);

#ifdef _WIN32
    WSACleanup();
#endif

    std::cout << "[Sender] File sending complete.\n";
}

int main() {
    // Reads test_input.txt and sends it to localhost port 8080
    send_file("test_input.txt", "127.0.0.1", 8080);
    return 0;
}