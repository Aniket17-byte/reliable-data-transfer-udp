#include "gbn_window.hpp"

namespace rdt {

// ================= GbnSenderWindow Implementation =================

GbnSenderWindow::GbnSenderWindow(size_t window_size, uint32_t initial_seq)
    : window_size_(window_size),
      base_(initial_seq),
      next_seq_num_(initial_seq) {}

bool GbnSenderWindow::can_send() const {
    return next_seq_num_ < (base_ + window_size_);
}

bool GbnSenderWindow::is_empty() const {
    return base_ == next_seq_num_;
}

bool GbnSenderWindow::is_full() const {
    return !can_send();
}

size_t GbnSenderWindow::get_available_window_slots() const {
    if (next_seq_num_ >= base_ + window_size_) {
        return 0;
    }
    return (base_ + window_size_) - next_seq_num_;
}

uint32_t GbnSenderWindow::push_packet(std::vector<uint8_t> payload, std::chrono::milliseconds current_rto) {
    uint32_t seq = next_seq_num_++;
    
    BufferedPacket pkt;
    pkt.seq_num = seq;
    pkt.payload = std::move(payload);
    pkt.sent_time = std::chrono::steady_clock::now();
    buffer_.push_back(std::move(pkt));

    // If this is the oldest unacknowledged packet in the window, start the timer
    if (!timer_.is_running()) {
        timer_.start(current_rto);
    }

    return seq;
}

GbnAckResult GbnSenderWindow::process_ack(uint32_t ack_seq, std::chrono::milliseconds current_rto) {
    GbnAckResult result;
    result.old_base = base_;
    result.new_base = base_;

    // If ACK is behind the current base, it is a duplicate ACK
    if (ack_seq < base_) {
        result.is_duplicate = true;
        result.is_valid = false;
        return result;
    }

    // Cumulative ACK acknowledging packets up to ack_seq within current in-flight window
    if (ack_seq >= base_ && ack_seq < next_seq_num_) {
        result.is_valid = true;
        size_t count = static_cast<size_t>(ack_seq - base_ + 1);
        result.packets_acknowledged = count;

        // Slide window by removing all acknowledged packets from buffer
        for (size_t i = 0; i < count && !buffer_.empty(); ++i) {
            buffer_.pop_front();
        }

        base_ = ack_seq + 1;
        result.new_base = base_;
        last_ack_received_ = ack_seq;
        has_received_any_ack_ = true;

        if (base_ == next_seq_num_) {
            // All outstanding packets have been acknowledged
            timer_.stop();
            result.window_empty = true;
        } else {
            // Unacknowledged packets remain in window; restart timer for the new base
            timer_.restart(current_rto);
        }
    }

    return result;
}

bool GbnSenderWindow::check_timeout() const {
    return timer_.is_expired();
}

std::vector<BufferedPacket> GbnSenderWindow::handle_timeout(std::chrono::milliseconds current_rto) {
    // Go-Back-N retransmission: retransmit ALL unacknowledged packets in [base, next_seq_num - 1]
    std::vector<BufferedPacket> retransmit_list;
    retransmit_list.reserve(buffer_.size());

    auto now = std::chrono::steady_clock::now();
    for (auto& pkt : buffer_) {
        pkt.sent_time = now;
        retransmit_list.push_back(pkt);
    }

    // Restart timer for the window
    if (!buffer_.empty()) {
        timer_.restart(current_rto);
    } else {
        timer_.stop();
    }

    return retransmit_list;
}

std::chrono::milliseconds GbnSenderWindow::get_timer_remaining() const {
    return timer_.remaining_time();
}

void GbnSenderWindow::reset(uint32_t initial_seq) {
    base_ = initial_seq;
    next_seq_num_ = initial_seq;
    buffer_.clear();
    timer_.stop();
    last_ack_received_ = 0;
    has_received_any_ack_ = false;
}

// ================= GbnReceiverWindow Implementation =================

GbnReceiverWindow::GbnReceiverWindow(uint32_t initial_seq)
    : expected_seq_num_(initial_seq) {}

GbnReceiverWindow::ReceiveResult GbnReceiverWindow::process_packet(uint32_t seq_num, std::vector<uint8_t> payload) {
    ReceiveResult res;

    if (seq_num == expected_seq_num_) {
        // In-order packet arrived
        res.accepted = true;
        res.ack_to_send = seq_num;
        res.payload = std::move(payload);

        last_ack_sent_ = seq_num;
        has_sent_any_ack_ = true;
        expected_seq_num_++;
    } else {
        // Out-of-order or duplicate packet: drop payload and re-send last ACK
        res.accepted = false;
        if (has_sent_any_ack_) {
            res.ack_to_send = last_ack_sent_;
        } else {
            // No packets acknowledged yet: re-acknowledge before initial sequence
            res.ack_to_send = expected_seq_num_ > 0 ? expected_seq_num_ - 1 : 0;
        }
    }

    return res;
}

void GbnReceiverWindow::reset(uint32_t initial_seq) {
    expected_seq_num_ = initial_seq;
    last_ack_sent_ = 0;
    has_sent_any_ack_ = false;
}

} // namespace rdt
