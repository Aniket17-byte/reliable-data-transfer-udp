#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>
#include "channel_emulator.hpp"

static std::atomic<bool> g_shutdown_requested{false};

void signal_handler(int sig) {
    g_shutdown_requested = true;
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  --listen-ip <ip>          Bind IP address (default: 127.0.0.1)\n"
              << "  --sender-in <port>        Port to receive DATA from sender (default: 9001)\n"
              << "  --receiver-out <port>     Port to forward DATA to receiver (default: 9002)\n"
              << "  --receiver-in <port>      Port to receive ACKs from receiver (default: 9003)\n"
              << "  --sender-out <port>       Port to forward ACKs to sender (default: 9000)\n"
              << "  --receiver-ip <ip>        Target receiver IP (default: 127.0.0.1)\n"
              << "  --sender-ip <ip>          Target sender IP (default: 127.0.0.1)\n"
              << "  --loss <float>            Packet loss probability [0.0 - 1.0] (default: 0.0)\n"
              << "  --corrupt <float>         Packet corruption probability [0.0 - 1.0] (default: 0.0)\n"
              << "  --delay <ms>              Base one-way propagation delay in ms (default: 0)\n"
              << "  --jitter <ms>             Jitter range in ms (+/- jitter) (default: 0)\n"
              << "  --reorder <float>         Reordering probability [0.0 - 1.0] (default: 0.0)\n"
              << "  --reorder-delay <ms>      Extra delay for reordered packets (default: 20)\n"
              << "  --duplicate <float>       Duplicate packet probability (default: 0.0)\n"
              << "  --seed <uint64>           PRNG Seed for reproducibility (default: 42)\n"
              << "  --asymmetric              Apply impairment to forward path only\n"
              << "  --verbose                 Print per-packet event logs\n"
              << "  --help                    Show this help message\n";
}

int main(int argc, char* argv[]) {
    ChannelConfig config;
    std::string listen_ip = "127.0.0.1";
    std::string sender_ip = "127.0.0.1";
    std::string receiver_ip = "127.0.0.1";

    uint16_t sender_in_port = 9001;
    uint16_t receiver_out_port = 9002;
    uint16_t receiver_in_port = 9003;
    uint16_t sender_out_port = 9000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--listen-ip" && i + 1 < argc) {
            listen_ip = argv[++i];
        } else if (arg == "--sender-ip" && i + 1 < argc) {
            sender_ip = argv[++i];
        } else if (arg == "--receiver-ip" && i + 1 < argc) {
            receiver_ip = argv[++i];
        } else if (arg == "--sender-in" && i + 1 < argc) {
            sender_in_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--receiver-out" && i + 1 < argc) {
            receiver_out_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--receiver-in" && i + 1 < argc) {
            receiver_in_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--sender-out" && i + 1 < argc) {
            sender_out_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--loss" && i + 1 < argc) {
            config.loss_rate = std::stod(argv[++i]);
        } else if (arg == "--corrupt" && i + 1 < argc) {
            config.corrupt_rate = std::stod(argv[++i]);
        } else if (arg == "--delay" && i + 1 < argc) {
            config.delay_ms = std::stoi(argv[++i]);
        } else if (arg == "--jitter" && i + 1 < argc) {
            config.jitter_ms = std::stoi(argv[++i]);
        } else if (arg == "--reorder" && i + 1 < argc) {
            config.reorder_rate = std::stod(argv[++i]);
        } else if (arg == "--reorder-delay" && i + 1 < argc) {
            config.reorder_delay_ms = std::stoi(argv[++i]);
        } else if (arg == "--duplicate" && i + 1 < argc) {
            config.duplicate_rate = std::stod(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            config.seed = std::stoull(argv[++i]);
        } else if (arg == "--asymmetric") {
            config.symmetrical_impairment = false;
        } else if (arg == "--verbose") {
            config.verbose = true;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << ">>> Initializing CS-30003 C++ Channel Emulator <<<\n";
    ChannelEmulator emulator(config);

    if (!emulator.start(listen_ip,
                        sender_in_port,
                        receiver_ip,
                        receiver_out_port,
                        receiver_in_port,
                        sender_ip,
                        sender_out_port)) {
        std::cerr << "[Fatal] Emulator failed to start sockets. Check port availability.\n";
        return 1;
    }

    std::cout << "[Emulator] Online and actively intercepting packets. Press Ctrl+C to terminate.\n";

    while (!g_shutdown_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n[Emulator] Shutting down gracefully...\n";
    emulator.stop();
    emulator.print_stats();

    return 0;
}
