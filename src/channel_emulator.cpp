#include "channel_emulator.hpp"
#include <iomanip>
#include <algorithm>

ChannelEmulator::ChannelEmulator(const ChannelConfig& config)
    : config_(config), rng_(config.seed) {
    NetUtils::init_networking();
}

ChannelEmulator::~ChannelEmulator() {
    stop();
    NetUtils::cleanup_networking();
}

bool ChannelEmulator::start(const std::string& listen_ip,
                           uint16_t sender_in_port,
                           const std::string& receiver_ip,
                           uint16_t receiver_port,
                           uint16_t receiver_in_port,
                           const std::string& sender_ip,
                           uint16_t sender_port) {
    if (running_) return false;

    // 1. Create Forward In socket (Listens for Sender's DATA)
    forward_in_sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (forward_in_sock_ == INVALID_SOCK) {
        std::cerr << "[Emulator] Failed to create forward_in_sock\n";
        return false;
    }
    sockaddr_in fwd_bind = NetUtils::create_address(listen_ip, sender_in_port);
    if (bind(forward_in_sock_, (sockaddr*)&fwd_bind, sizeof(fwd_bind)) != 0) {
        std::cerr << "[Emulator] Failed to bind forward_in_sock to port " << sender_in_port << "\n";
        NetUtils::close_socket(forward_in_sock_);
        return false;
    }

    // 2. Create Forward Out socket (Transmits to Receiver)
    forward_out_sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    receiver_target_addr_ = NetUtils::create_address(receiver_ip, receiver_port);

    // 3. Create Reverse In socket (Listens for Receiver's ACKs)
    reverse_in_sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (reverse_in_sock_ == INVALID_SOCK) {
        std::cerr << "[Emulator] Failed to create reverse_in_sock\n";
        NetUtils::close_socket(forward_in_sock_);
        NetUtils::close_socket(forward_out_sock_);
        return false;
    }
    sockaddr_in rev_bind = NetUtils::create_address(listen_ip, receiver_in_port);
    if (bind(reverse_in_sock_, (sockaddr*)&rev_bind, sizeof(rev_bind)) != 0) {
        std::cerr << "[Emulator] Failed to bind reverse_in_sock to port " << receiver_in_port << "\n";
        NetUtils::close_socket(forward_in_sock_);
        NetUtils::close_socket(forward_out_sock_);
        NetUtils::close_socket(reverse_in_sock_);
        return false;
    }

    // 4. Create Reverse Out socket (Transmits back to Sender)
    reverse_out_sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sender_target_addr_ = NetUtils::create_address(sender_ip, sender_port);

    // Set receive timeouts so threads can check running_ flag periodically
    NetUtils::set_socket_timeout(forward_in_sock_, 200);
    NetUtils::set_socket_timeout(reverse_in_sock_, 200);

    running_ = true;

    // Launch worker threads
    dispatcher_thread_ = std::thread(&ChannelEmulator::dispatcher_loop, this);
    forward_thread_ = std::thread(&ChannelEmulator::forward_listener_loop, this);
    reverse_thread_ = std::thread(&ChannelEmulator::reverse_listener_loop, this);

    if (config_.verbose) {
        std::cout << "[Emulator] Started on ports Forward=" << sender_in_port
                  << " -> " << receiver_port << ", Reverse=" << receiver_in_port
                  << " -> " << sender_port << "\n"
                  << "[Emulator] Loss=" << (config_.loss_rate * 100.0) << "%"
                  << " Corrupt=" << (config_.corrupt_rate * 100.0) << "%"
                  << " Delay=" << config_.delay_ms << "ms (+/- " << config_.jitter_ms << "ms)"
                  << " Reorder=" << (config_.reorder_rate * 100.0) << "%"
                  << " Seed=" << config_.seed << "\n";
    }

    return true;
}

void ChannelEmulator::stop() {
    if (!running_) return;
    running_ = false;

    // Wake up dispatcher thread
    queue_cv_.notify_all();

    if (forward_thread_.joinable()) forward_thread_.join();
    if (reverse_thread_.joinable()) reverse_thread_.join();
    if (dispatcher_thread_.joinable()) dispatcher_thread_.join();

    if (forward_in_sock_ != INVALID_SOCK) NetUtils::close_socket(forward_in_sock_);
    if (forward_out_sock_ != INVALID_SOCK) NetUtils::close_socket(forward_out_sock_);
    if (reverse_in_sock_ != INVALID_SOCK) NetUtils::close_socket(reverse_in_sock_);
    if (reverse_out_sock_ != INVALID_SOCK) NetUtils::close_socket(reverse_out_sock_);

    forward_in_sock_ = INVALID_SOCK;
    forward_out_sock_ = INVALID_SOCK;
    reverse_in_sock_ = INVALID_SOCK;
    reverse_out_sock_ = INVALID_SOCK;
}

void ChannelEmulator::forward_listener_loop() {
    uint8_t buffer[65535];
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    while (running_) {
        int bytes = recvfrom(forward_in_sock_, (char*)buffer, sizeof(buffer), 0,
                             (sockaddr*)&client_addr, &addr_len);
        if (bytes > 0) {
            stats_.total_forward_received++;
            process_incoming_packet(buffer, static_cast<size_t>(bytes),
                                    receiver_target_addr_, forward_out_sock_, true);
        }
    }
}

void ChannelEmulator::reverse_listener_loop() {
    uint8_t buffer[65535];
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    while (running_) {
        int bytes = recvfrom(reverse_in_sock_, (char*)buffer, sizeof(buffer), 0,
                             (sockaddr*)&client_addr, &addr_len);
        if (bytes > 0) {
            stats_.total_reverse_received++;
            process_incoming_packet(buffer, static_cast<size_t>(bytes),
                                    sender_target_addr_, reverse_out_sock_, false);
        }
    }
}

void ChannelEmulator::apply_corruption(std::vector<uint8_t>& buffer) {
    if (buffer.empty()) return;
    std::lock_guard<std::mutex> lock(rng_mutex_);
    
    // Choose how many bytes to corrupt (1 to 4)
    std::uniform_int_distribution<size_t> byte_dist(0, buffer.size() - 1);
    std::uniform_int_distribution<uint8_t> bit_mask_dist(1, 255);

    size_t num_corruptions = std::min<size_t>(buffer.size(), 2);
    for (size_t i = 0; i < num_corruptions; ++i) {
        size_t idx = byte_dist(rng_);
        uint8_t mask = bit_mask_dist(rng_);
        buffer[idx] ^= mask;
    }
}

void ChannelEmulator::process_incoming_packet(const uint8_t* buffer, size_t size,
                                             const sockaddr_in& dest_addr,
                                             socket_t out_sock,
                                             bool is_forward) {
    uint64_t pid = ++packet_counter_;

    // Check if impairment should be applied (forward always, reverse if symmetrical)
    bool apply_impairment = is_forward || config_.symmetrical_impairment;

    double p_loss = apply_impairment ? config_.loss_rate : 0.0;
    double p_corrupt = apply_impairment ? config_.corrupt_rate : 0.0;
    double p_reorder = apply_impairment ? config_.reorder_rate : 0.0;
    double p_dup = apply_impairment ? config_.duplicate_rate : 0.0;

    double rand_val = 0.0;
    {
        std::lock_guard<std::mutex> lock(rng_mutex_);
        rand_val = uniform_dist_(rng_);
    }

    // 1. Packet Loss Check
    if (rand_val < p_loss) {
        if (is_forward) stats_.total_forward_dropped++;
        else stats_.total_reverse_dropped++;

        if (config_.verbose) {
            std::cout << "[Emulator] [DROP] " << (is_forward ? "Forward" : "Reverse")
                      << " Pkt #" << pid << " (" << size << " bytes) dropped.\n";
        }
        return; // Packet dropped!
    }

    // Prepare packet copy
    std::vector<uint8_t> pkt_data(buffer, buffer + size);

    // 2. Packet Corruption Check
    bool was_corrupted = false;
    double corrupt_rand = 0.0;
    {
        std::lock_guard<std::mutex> lock(rng_mutex_);
        corrupt_rand = uniform_dist_(rng_);
    }
    if (corrupt_rand < p_corrupt) {
        apply_corruption(pkt_data);
        was_corrupted = true;
        if (is_forward) stats_.total_forward_corrupted++;
        else stats_.total_reverse_corrupted++;

        if (config_.verbose) {
            std::cout << "[Emulator] [CORRUPT] " << (is_forward ? "Forward" : "Reverse")
                      << " Pkt #" << pid << " corrupted with bit flips.\n";
        }
    }

    // 3. Compute Propagation Delay & Jitter
    int total_delay = config_.delay_ms;
    if (config_.jitter_ms > 0) {
        int jitter_offset = 0;
        {
            std::lock_guard<std::mutex> lock(rng_mutex_);
            std::uniform_int_distribution<int> jitter_dist(-config_.jitter_ms, config_.jitter_ms);
            jitter_offset = jitter_dist(rng_);
        }
        total_delay = std::max(0, total_delay + jitter_offset);
    }

    // 4. Packet Reordering Check
    double reorder_rand = 0.0;
    {
        std::lock_guard<std::mutex> lock(rng_mutex_);
        reorder_rand = uniform_dist_(rng_);
    }
    if (reorder_rand < p_reorder) {
        total_delay += config_.reorder_delay_ms;
        if (is_forward) stats_.total_forward_reordered++;
        if (config_.verbose) {
            std::cout << "[Emulator] [REORDER] Pkt #" << pid
                      << " held back by +" << config_.reorder_delay_ms << "ms\n";
        }
    }

    auto now = std::chrono::steady_clock::now();
    auto release_time = now + std::chrono::milliseconds(total_delay);

    ScheduledPacket item{
        release_time,
        pkt_data,
        dest_addr,
        out_sock,
        pid,
        is_forward
    };

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push(item);
    }
    queue_cv_.notify_one();

    // 5. Duplicate Check
    double dup_rand = 0.0;
    {
        std::lock_guard<std::mutex> lock(rng_mutex_);
        dup_rand = uniform_dist_(rng_);
    }
    if (dup_rand < p_dup) {
        if (is_forward) stats_.total_forward_duplicated++;
        ScheduledPacket dup_item = item;
        dup_item.release_time += std::chrono::milliseconds(5); // 5ms trailing duplicate
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            queue_.push(dup_item);
        }
        queue_cv_.notify_one();
        if (config_.verbose) {
            std::cout << "[Emulator] [DUPLICATE] Pkt #" << pid << " duplicated.\n";
        }
    }
}

void ChannelEmulator::dispatcher_loop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        if (queue_.empty()) {
            queue_cv_.wait_for(lock, std::chrono::milliseconds(50), [this] {
                return !running_ || !queue_.empty();
            });
            if (!running_) break;
            if (queue_.empty()) continue;
        }

        auto next_release = queue_.top().release_time;
        auto now = std::chrono::steady_clock::now();

        if (now < next_release) {
            queue_cv_.wait_until(lock, next_release, [this, next_release] {
                return !running_ || (queue_.empty() ? false : queue_.top().release_time < next_release);
            });
            if (!running_) break;
            continue;
        }

        // Pop packet due for departure
        ScheduledPacket pkt = queue_.top();
        queue_.pop();
        lock.unlock();

        // Transmit packet through UDP socket
        int sent = sendto(pkt.send_socket,
                          reinterpret_cast<const char*>(pkt.data.data()),
                          static_cast<int>(pkt.data.size()),
                          0,
                          reinterpret_cast<const sockaddr*>(&pkt.dest_addr),
                          sizeof(pkt.dest_addr));

        if (sent > 0) {
            if (pkt.is_forward) stats_.total_forward_delivered++;
            else stats_.total_reverse_delivered++;

            if (config_.verbose) {
                std::cout << "[Emulator] [DISPATCH] "
                          << (pkt.is_forward ? "Forward" : "Reverse")
                          << " Pkt #" << pkt.packet_id << " dispatched ("
                          << sent << " bytes)\n";
            }
        }
    }
}

void ChannelEmulator::print_stats() const {
    std::cout << "\n================= CHANNEL EMULATOR STATISTICS =================\n"
              << " Forward Path (Sender -> Receiver):\n"
              << "   Received:   " << stats_.total_forward_received.load() << "\n"
              << "   Delivered:  " << stats_.total_forward_delivered.load() << "\n"
              << "   Dropped:    " << stats_.total_forward_dropped.load() << "\n"
              << "   Corrupted:  " << stats_.total_forward_corrupted.load() << "\n"
              << "   Reordered:  " << stats_.total_forward_reordered.load() << "\n"
              << "   Duplicated: " << stats_.total_forward_duplicated.load() << "\n"
              << " Reverse Path (Receiver -> Sender):\n"
              << "   Received:   " << stats_.total_reverse_received.load() << "\n"
              << "   Delivered:  " << stats_.total_reverse_delivered.load() << "\n"
              << "   Dropped:    " << stats_.total_reverse_dropped.load() << "\n"
              << "   Corrupted:  " << stats_.total_reverse_corrupted.load() << "\n"
              << "===============================================================\n" << std::endl;
}
