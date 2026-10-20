#include "sr_window.hpp"

namespace rdt {

// ================= SrSenderWindow Implementation =================

SrSenderWindow::SrSenderWindow(size_t window_size, uint32_t initial_seq)
    : window_size_(window_size),
      send_base_(initial_seq),
      next_seq_num_(initial_seq) {}

bool SrSenderWindow::can_send() const {
    return next_seq_num_ < (send_base_ + window_size_);
}

bool SrSenderWindow::is_empty() const {
    return window_buffer_.empty();
}

bool SrSenderWindow::is_full() const {
    return !can_send();
}

size_t SrSenderWindow::get_available_window_slots() const {
    if (next_seq_num_ >= send_base_ + window_size_) {
        return 0;
    }
    return (send_base_ + window_size_) - next_seq_num_;
}

size_t SrSenderWindow::get_in_flight_count() const {
    size_t count = 0;
    for (const auto& [seq, entry] : window_buffer_) {
        if (entry.state == SrPacketState::SENT_UNACKED) {
            count++;
        }
    }
    return count;
}

std::optional<SrPacketState> SrSenderWindow::get_packet_state(uint32_t seq_num) const {
    auto it = window_buffer_.find(seq_num);
    if (it != window_buffer_.end()) {
        return it->second.state;
    }
    return std::nullopt;
}

uint32_t SrSenderWindow::push_packet(std::vector<uint8_t> payload, std::chrono::milliseconds rto) {
    uint32_t seq = next_seq_num_++;

    SrPacketEntry entry;
    entry.seq_num = seq;
    entry.payload = std::move(payload);
    entry.sent_time = std::chrono::steady_clock::now();
    entry.state = SrPacketState::SENT_UNACKED;
    entry.tx_count = 1;

    window_buffer_[seq] = std::move(entry);
    timer_manager_.start_timer(seq, rto);

    return seq;
}

bool SrSenderWindow::process_ack(uint32_t ack_seq) {
    auto it = window_buffer_.find(ack_seq);
    if (it == window_buffer_.end()) {
        return false;
    }

    // Mark slot as acknowledged and stop its independent timer
    it->second.state = SrPacketState::ACKED;
    timer_manager_.stop_timer(ack_seq);

    // If the acknowledged packet is at the base, slide the window
    uint32_t old_base = send_base_;
    if (ack_seq == send_base_) {
        slide_window();
    }

    return send_base_ != old_base;
}

void SrSenderWindow::slide_window() {
    // Slide send_base forward past all consecutive ACKED packets
    while (!window_buffer_.empty()) {
        auto it = window_buffer_.find(send_base_);
        if (it != window_buffer_.end() && it->second.state == SrPacketState::ACKED) {
            window_buffer_.erase(it);
            send_base_++;
        } else {
            break;
        }
    }
}

bool SrSenderWindow::process_sack(const SackInfo& sack) {
    uint32_t old_base = send_base_;

    // 1. Process cumulative ACK (all packets up to and including cumulative_ack are ACKed)
    if (sack.has_cumulative_ack) {
        for (uint32_t s = send_base_; s <= sack.cumulative_ack; ++s) {
            auto it = window_buffer_.find(s);
            if (it != window_buffer_.end()) {
                it->second.state = SrPacketState::ACKED;
                timer_manager_.stop_timer(s);
            }
        }
    }

    // 2. Process SACK blocks
    for (const auto& block : sack.blocks) {
        for (uint32_t s = block.start_seq; s <= block.end_seq; ++s) {
            auto it = window_buffer_.find(s);
            if (it != window_buffer_.end()) {
                it->second.state = SrPacketState::ACKED;
                timer_manager_.stop_timer(s);
            }
        }
    }

    // 3. Process bitmask
    if (sack.bitmask != 0) {
        uint32_t base_ref = sack.has_cumulative_ack ? (sack.cumulative_ack + 1) : send_base_;
        for (uint64_t i = 0; i < 64; ++i) {
            if (sack.bitmask & (1ULL << i)) {
                uint32_t s = base_ref + static_cast<uint32_t>(i);
                auto it = window_buffer_.find(s);
                if (it != window_buffer_.end()) {
                    it->second.state = SrPacketState::ACKED;
                    timer_manager_.stop_timer(s);
                }
            }
        }
    }

    slide_window();
    return send_base_ != old_base;
}

std::vector<uint32_t> SrSenderWindow::get_expired_packets() const {
    return timer_manager_.get_expired_packets();
}

std::optional<SrPacketEntry> SrSenderWindow::handle_timeout_packet(uint32_t seq_num, std::chrono::milliseconds rto) {
    auto it = window_buffer_.find(seq_num);
    if (it == window_buffer_.end() || it->second.state != SrPacketState::SENT_UNACKED) {
        return std::nullopt;
    }

    it->second.tx_count++;
    it->second.sent_time = std::chrono::steady_clock::now();
    timer_manager_.start_timer(seq_num, rto);

    return it->second;
}

std::optional<std::chrono::milliseconds> SrSenderWindow::time_until_earliest_timeout() const {
    return timer_manager_.time_until_earliest_timeout();
}

void SrSenderWindow::reset(uint32_t initial_seq) {
    send_base_ = initial_seq;
    next_seq_num_ = initial_seq;
    window_buffer_.clear();
    timer_manager_.clear();
}

// ================= SrReceiverWindow Implementation =================

SrReceiverWindow::SrReceiverWindow(size_t window_size, uint32_t initial_seq)
    : window_size_(window_size),
      rcv_base_(initial_seq) {}

SrReceiverWindow::ProcessResult SrReceiverWindow::process_packet(uint32_t seq_num, std::vector<uint8_t> payload) {
    ProcessResult res;
    res.ack_to_send = seq_num;

    // Case 1: Packet falls inside the current receive window [rcv_base, rcv_base + window_size - 1]
    if (seq_num >= rcv_base_ && seq_num < rcv_base_ + window_size_) {
        if (rcv_buffer_.find(seq_num) != rcv_buffer_.end()) {
            res.action = ProcessAction::DUPLICATE_ALREADY_RCVD;
        } else {
            rcv_buffer_[seq_num] = std::move(payload);

            if (seq_num == rcv_base_) {
                // Base arrived: deliver rcv_base and any contiguous buffered packets
                res.action = ProcessAction::DELIVER_IN_ORDER;

                while (true) {
                    auto it = rcv_buffer_.find(rcv_base_);
                    if (it != rcv_buffer_.end()) {
                        SrPacketEntry delivered;
                        delivered.seq_num = rcv_base_;
                        delivered.payload = std::move(it->second);
                        res.deliverable_packets.push_back(std::move(delivered));

                        rcv_buffer_.erase(it);
                        rcv_base_++;
                    } else {
                        break;
                    }
                }
            } else {
                // Out-of-order packet buffered inside window
                res.action = ProcessAction::BUFFER_OUT_OF_ORDER;
            }
        }
    }
    // Case 2: Packet falls behind the receive window [rcv_base - window_size, rcv_base - 1]
    // Crucial Selective Repeat rule: An ACK was previously sent, but sender might have lost it.
    // MUST re-ACK this packet so the sender can advance its window.
    else if (seq_num < rcv_base_) {
        res.action = ProcessAction::DUPLICATE_PAST_WINDOW;
    }
    // Case 3: Packet is ahead of current window [rcv_base + window_size, ...]
    else {
        res.action = ProcessAction::OUT_OF_WINDOW_IGNORE;
    }

    res.sack_info = generate_sack_info();
    return res;
}

SackInfo SrReceiverWindow::generate_sack_info() const {
    SackInfo sack;
    if (rcv_base_ > 0) {
        sack.has_cumulative_ack = true;
        sack.cumulative_ack = rcv_base_ - 1;
    } else {
        sack.has_cumulative_ack = false;
        sack.cumulative_ack = 0;
    }
    sack.bitmask = 0;

    if (rcv_buffer_.empty()) {
        return sack;
    }

    // Build contiguous SACK blocks from sorted map
    std::optional<SackBlock> current_block;
    uint32_t base_ref = sack.has_cumulative_ack ? (sack.cumulative_ack + 1) : rcv_base_;

    for (const auto& [seq, _] : rcv_buffer_) {
        // Also update 64-bit mask relative to base_ref
        if (seq >= base_ref) {
            uint64_t diff = static_cast<uint64_t>(seq - base_ref);
            if (diff < 64) {
                sack.bitmask |= (1ULL << diff);
            }
        }

        if (!current_block.has_value()) {
            current_block = SackBlock{seq, seq};
        } else if (seq == current_block->end_seq + 1) {
            current_block->end_seq = seq;
        } else {
            sack.blocks.push_back(*current_block);
            current_block = SackBlock{seq, seq};
        }
    }

    if (current_block.has_value()) {
        sack.blocks.push_back(*current_block);
    }

    return sack;
}

void SrReceiverWindow::reset(uint32_t initial_seq) {
    rcv_base_ = initial_seq;
    rcv_buffer_.clear();
}

} // namespace rdt
