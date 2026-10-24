#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <queue>
#include <random>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include "socket_utils.hpp"
#include "packet.hpp"

// Impairment configuration parameters
struct ChannelConfig {
    double loss_rate = 0.0;           // Probability of dropping a packet [0.0, 1.0]
    double corrupt_rate = 0.0;        // Probability of bit-flipping/corrupting payload [0.0, 1.0]
    double reorder_rate = 0.0;        // Probability of artificially delaying a packet to cause reordering
    double duplicate_rate = 0.0;      // Probability of duplicating a packet [0.0, 1.0]
    
    int delay_ms = 0;                 // Base latency (milliseconds)
    int jitter_ms = 0;                // Latency jitter (+/- ms)
    int reorder_delay_ms = 20;        // Extra hold time when reordering is triggered (ms)
    
    uint64_t seed = 42;               // Seed for std::mt19937_64 PRNG
    bool verbose = false;             // Verbose logging flag
    bool symmetrical_impairment = true; // Apply impairment to both Forward (DATA) and Reverse (ACK) paths
};

// Runtime statistics collected by emulator
struct ChannelStats {
    std::atomic<uint64_t> total_forward_received{0};
    std::atomic<uint64_t> total_forward_delivered{0};
    std::atomic<uint64_t> total_forward_dropped{0};
    std::atomic<uint64_t> total_forward_corrupted{0};
    std::atomic<uint64_t> total_forward_reordered{0};
    std::atomic<uint64_t> total_forward_duplicated{0};

    std::atomic<uint64_t> total_reverse_received{0};
    std::atomic<uint64_t> total_reverse_delivered{0};
    std::atomic<uint64_t> total_reverse_dropped{0};
    std::atomic<uint64_t> total_reverse_corrupted{0};
};

struct ScheduledPacket {
    std::chrono::steady_clock::time_point release_time;
    std::vector<uint8_t> data;
    sockaddr_in dest_addr;
    socket_t send_socket;
    uint64_t packet_id;
    bool is_forward;

    // Priority queue comparator (min-heap: smallest release_time has highest priority)
    bool operator>(const ScheduledPacket& other) const {
        return release_time > other.release_time;
    }
};

class ChannelEmulator {
public:
    ChannelEmulator(const ChannelConfig& config);
    ~ChannelEmulator();

    // Start emulator on specified local listening ports and forward targets
    // forward: sender -> emulator_fwd_port -> receiver_addr
    // reverse: receiver -> emulator_rev_port -> sender_addr
    bool start(const std::string& listen_ip,
               uint16_t sender_in_port,     // Listen for sender packets (e.g. 9001)
               const std::string& receiver_ip,
               uint16_t receiver_port,      // Forward sender packets to receiver (e.g. 9002)
               uint16_t receiver_in_port,   // Listen for receiver ACKs (e.g. 9003)
               const std::string& sender_ip,
               uint16_t sender_port);       // Forward ACKs back to sender (e.g. 9000)

    void stop();

    const ChannelStats& get_stats() const { return stats_; }
    void print_stats() const;

private:
    void forward_listener_loop();
    void reverse_listener_loop();
    void dispatcher_loop();

    void process_incoming_packet(const uint8_t* buffer, size_t size,
                                const sockaddr_in& dest_addr,
                                socket_t out_sock,
                                bool is_forward);

    void apply_corruption(std::vector<uint8_t>& buffer);

    ChannelConfig config_;
    ChannelStats stats_;

    std::mt19937_64 rng_;
    std::uniform_real_distribution<double> uniform_dist_{0.0, 1.0};
    std::mutex rng_mutex_;

    socket_t forward_in_sock_{INVALID_SOCK};
    socket_t forward_out_sock_{INVALID_SOCK};
    socket_t reverse_in_sock_{INVALID_SOCK};
    socket_t reverse_out_sock_{INVALID_SOCK};

    sockaddr_in receiver_target_addr_{};
    sockaddr_in sender_target_addr_{};

    std::priority_queue<ScheduledPacket,
                        std::vector<ScheduledPacket>,
                        std::greater<ScheduledPacket>> queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::atomic<bool> running_{false};
    std::atomic<uint64_t> packet_counter_{0};

    std::thread forward_thread_;
    std::thread reverse_thread_;
    std::thread dispatcher_thread_;
};
