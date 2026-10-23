#pragma once

#include <cstdint>
#include <vector>
#include <deque>
#include <map>
#include <unordered_map>
#include <chrono>
#include <optional>
#include "timer_manager.hpp"

namespace rdt {

/**
 * @brief Status of a sequence slot in the Selective Repeat Sender Window.
 */
enum class SrPacketState {
    UNSENT,
    SENT_UNACKED,
    ACKED
};

/**
 * @brief Packet entry stored in the sender or receiver buffer.
 */
struct SrPacketEntry {
    uint32_t seq_num{0};
    std::vector<uint8_t> payload;
    std::chrono::steady_clock::time_point sent_time;
    SrPacketState state{SrPacketState::UNSENT};
    uint32_t tx_count{0};
};

/**
 * @brief Represents a contiguous range of received sequence numbers for SACK blocks (RFC 2018 style).
 */
struct SackBlock {
    uint32_t start_seq{0}; // Inclusive
    uint32_t end_seq{0};   // Inclusive
};

/**
 * @brief SACK structure containing cumulative base acknowledgment plus selective blocks.
 */
struct SackInfo {
    bool has_cumulative_ack{false};    // True if at least one packet has been cumulatively received
    uint32_t cumulative_ack{0};        // All packets up to and including this are received
    std::vector<SackBlock> blocks;     // Disjoint out-of-order received blocks
    uint64_t bitmask{0};               // Compact 64-bit window mask relative to cumulative_ack + 1 (or rcv_base)
};

/**
 * @brief Selective Repeat Sender Sliding Window.
 * 
 * Responsibilities (Assigned to Aniket Dwivedy):
 * - Maintain window bounds: [send_base, send_base + window_size - 1].
 * - Individual packet state tracking (UNSENT, SENT_UNACKED, ACKED).
 * - Per-packet retransmission timers via MultiTimerManager.
 * - Sliding window forward past consecutively ACKed packets when base is acknowledged.
 * - Processing both individual selective ACKs and SACK blocks / bitmasks.
 * - Isolated retransmission of only timed-out packets (avoiding redundant retransmissions).
 */
class SrSenderWindow {
public:
    explicit SrSenderWindow(size_t window_size = 10, uint32_t initial_seq = 0);

    [[nodiscard]] bool can_send() const;
    [[nodiscard]] bool is_empty() const;
    [[nodiscard]] bool is_full() const;
    [[nodiscard]] size_t get_available_window_slots() const;

    /**
     * @brief Buffer and transmit a new packet, arming its individual timer.
     * @param payload Packet data.
     * @param rto Retransmission timeout duration for this packet.
     * @return uint32_t Assigned sequence number.
     */
    uint32_t push_packet(std::vector<uint8_t> payload, std::chrono::milliseconds rto);

    /**
     * @brief Process an individual selective ACK for seq_num.
     * @param ack_seq Acknowledged sequence number.
     * @return bool True if send_base advanced (window slid forward).
     */
    bool process_ack(uint32_t ack_seq);

    /**
     * @brief Process SACK information (cumulative ACK + SACK blocks / bitmask).
     * @param sack SACK structure received from receiver.
     * @return bool True if send_base advanced.
     */
    bool process_sack(const SackInfo& sack);

    /**
     * @brief Find all packets whose individual retransmission timers have expired.
     */
    [[nodiscard]] std::vector<uint32_t> get_expired_packets() const;

    /**
     * @brief Prepare a timed-out packet for retransmission: re-arms timer and increments tx_count.
     * @param seq_num Sequence number to retransmit.
     * @param rto Current backoff RTO to arm the new timer with.
     * @return std::optional<SrPacketEntry> The packet to send, or nullopt if already acked.
     */
    std::optional<SrPacketEntry> handle_timeout_packet(uint32_t seq_num, std::chrono::milliseconds rto);

    /**
     * @brief Check time until next earliest timeout across all in-flight packets.
     */
    [[nodiscard]] std::optional<std::chrono::milliseconds> time_until_earliest_timeout() const;

    // --- State Getters ---
    [[nodiscard]] uint32_t get_send_base() const { return send_base_; }
    [[nodiscard]] uint32_t get_next_seq_num() const { return next_seq_num_; }
    [[nodiscard]] size_t get_window_size() const { return window_size_; }
    [[nodiscard]] size_t get_in_flight_count() const;
    [[nodiscard]] std::optional<SrPacketState> get_packet_state(uint32_t seq_num) const;

    void set_window_size(size_t window_size) { window_size_ = window_size; }
    void reset(uint32_t initial_seq = 0);

private:
    void slide_window();

    size_t window_size_;
    uint32_t send_base_;
    uint32_t next_seq_num_;

    std::map<uint32_t, SrPacketEntry> window_buffer_;
    MultiTimerManager timer_manager_;
};

/**
 * @brief Selective Repeat Receiver Sliding Window.
 * 
 * Responsibilities (Assigned to Aniket Dwivedy):
 * - Maintain window bounds: [rcv_base, rcv_base + window_size - 1].
 * - Buffer out-of-order packets falling inside window.
 * - Immediately re-acknowledge duplicate packets falling in [rcv_base - window_size, rcv_base - 1].
 * - Slide window and deliver in-order contiguous packets when rcv_base arrives.
 * - Generate SACK state blocks and bitmasks.
 */
class SrReceiverWindow {
public:
    explicit SrReceiverWindow(size_t window_size = 10, uint32_t initial_seq = 0);

    enum class ProcessAction {
        DELIVER_IN_ORDER,        // rcv_base arrived; deliver one or more contiguous packets
        BUFFER_OUT_OF_ORDER,     // Inside window, but gaps exist; buffered
        DUPLICATE_ALREADY_RCVD,  // Inside window, but already buffered before
        DUPLICATE_PAST_WINDOW,   // Behind rcv_base; must still ACK to unblock sender
        OUT_OF_WINDOW_IGNORE     // Ahead of rcv_base + window_size - 1; dropped
    };

    struct ProcessResult {
        ProcessAction action;
        uint32_t ack_to_send{0};
        std::vector<SrPacketEntry> deliverable_packets; // Packets ready to write to disk
        SackInfo sack_info;                             // Comprehensive SACK state
    };

    /**
     * @brief Process incoming packet.
     */
    ProcessResult process_packet(uint32_t seq_num, std::vector<uint8_t> payload);

    /**
     * @brief Compute current SACK information for inclusion in outgoing packet headers.
     */
    [[nodiscard]] SackInfo generate_sack_info() const;

    [[nodiscard]] uint32_t get_rcv_base() const { return rcv_base_; }
    [[nodiscard]] size_t get_window_size() const { return window_size_; }
    [[nodiscard]] size_t get_buffered_out_of_order_count() const { return rcv_buffer_.size(); }

    void set_window_size(size_t window_size) { window_size_ = window_size; }
    void reset(uint32_t initial_seq = 0);

private:
    size_t window_size_;
    uint32_t rcv_base_;
    std::map<uint32_t, std::vector<uint8_t>> rcv_buffer_;
};

} // namespace rdt
