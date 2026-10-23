#pragma once

#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <optional>

namespace rdt {

/**
 * @brief Metadata tracked per in-flight packet for RTT calculation and Karn's algorithm.
 */
struct PacketTxInfo {
    uint32_t seq_num{0};
    std::chrono::steady_clock::time_point first_sent_time;
    std::chrono::steady_clock::time_point last_sent_time;
    uint32_t tx_count{0};           // Number of times this packet was sent (1 = original, >1 = retransmitted)
    bool is_retransmitted{false};   // Flagged if sent more than once
};

/**
 * @brief Statistics structure for logging and viva demonstration.
 */
struct RttStats {
    double srtt_ms{0.0};
    double rttvar_ms{0.0};
    double rto_ms{0.0};
    uint64_t total_samples{0};
    uint64_t ignored_samples_karn{0};
    uint64_t timeout_backoffs{0};
    double last_sample_rtt_ms{0.0};
};

/**
 * @brief Dynamic RTT Estimator implementing Jacobson/Karels Algorithm (RFC 6298)
 *        and Karn's Algorithm for retransmission ambiguity resolution.
 * 
 * Responsibilities (Assigned to Aniket Dwivedy):
 * - Track send timestamps and retransmission counts per packet sequence number.
 * - Measure SampleRTT on ACK arrival.
 * - Enforce Karn's Rule 1: Do not compute SampleRTT for retransmitted packets.
 * - Enforce Karn's Rule 2: Exponential backoff on retransmission timeout (RTO *= 2).
 * - Smooth RTT using SRTT and compute variance RTTVAR.
 * - Calculate adaptive Retransmission Timeout (RTO) with configurable bounds.
 */
class RttEstimator {
public:
    /**
     * @brief Constructor with default RFC 6298 parameters.
     * @param initial_rto_ms Initial RTO before any RTT sample is collected (default: 1000ms).
     * @param min_rto_ms Lower bound for RTO clamp (default: 200ms).
     * @param max_rto_ms Upper bound for RTO clamp (default: 60000ms).
     * @param alpha Jacobson/Karels smoothing factor for SRTT (default: 0.125 = 1/8).
     * @param beta Jacobson/Karels smoothing factor for RTTVAR (default: 0.25 = 1/4).
     * @param k Variance multiplier (default: 4.0).
     * @param clock_granularity_ms Clock granularity G (default: 1.0ms).
     */
    explicit RttEstimator(double initial_rto_ms = 1000.0,
                          double min_rto_ms = 200.0,
                          double max_rto_ms = 60000.0,
                          double alpha = 0.125,
                          double beta = 0.25,
                          double k = 4.0,
                          double clock_granularity_ms = 1.0);

    // --- Transmission & ACK Tracking (Karn's Algorithm) ---

    /**
     * @brief Record the transmission of a packet.
     *        If called for the first time for seq_num, tx_count becomes 1.
     *        If called again for seq_num, tx_count increments and is_retransmitted becomes true.
     */
    void record_send(uint32_t seq_num);

    /**
     * @brief Record the receipt of an ACK for seq_num.
     *        Computes SampleRTT and updates Jacobson/Karels estimates IF AND ONLY IF
     *        the packet was NOT retransmitted (Karn's Rule 1).
     * @param seq_num The sequence number acknowledged.
     * @return std::optional<double> The measured SampleRTT in milliseconds, or std::nullopt if ignored by Karn.
     */
    std::optional<double> record_ack(uint32_t seq_num);

    /**
     * @brief Handles a retransmission timeout (RTO expiration).
     *        Enforces Karn's Rule 2: Exponential timer backoff (RTO = min(RTO * 2, max_rto)).
     */
    void handle_timeout();

    /**
     * @brief Explicitly purge tracking for an acknowledged or discarded sequence number.
     */
    void remove_seq(uint32_t seq_num);

    /**
     * @brief Reset estimator state (e.g. at connection start or protocol switch).
     */
    void reset();

    // --- Getters for Protocol State & Viva Defense ---

    [[nodiscard]] double get_rto_ms() const { return rto_ms_; }
    [[nodiscard]] std::chrono::milliseconds get_rto_duration() const;
    [[nodiscard]] double get_srtt_ms() const { return srtt_ms_; }
    [[nodiscard]] double get_rttvar_ms() const { return rttvar_ms_; }
    [[nodiscard]] bool has_valid_sample() const { return has_sample_; }
    [[nodiscard]] const RttStats& get_stats() const { return stats_; }

    // --- Live viva tuners (allows changing parameters on the spot during demo) ---
    void set_alpha(double alpha) { alpha_ = alpha; }
    void set_beta(double beta) { beta_ = beta; }
    void set_k(double k) { k_ = k; }
    void set_min_rto_ms(double min_ms) { min_rto_ms_ = min_ms; clamp_rto(); }
    void set_max_rto_ms(double max_ms) { max_rto_ms_ = max_ms; clamp_rto(); }
    void set_backoff_factor(double factor) { backoff_factor_ = factor; }

private:
    void update_estimates(double sample_rtt_ms);
    void clamp_rto();

    double initial_rto_ms_;
    double min_rto_ms_;
    double max_rto_ms_;
    double alpha_;
    double beta_;
    double k_;
    double clock_granularity_ms_;
    double backoff_factor_{2.0};

    double srtt_ms_{0.0};
    double rttvar_ms_{0.0};
    double rto_ms_{1000.0};
    bool has_sample_{false};

    std::unordered_map<uint32_t, PacketTxInfo> in_flight_packets_;
    RttStats stats_;
};

} // namespace rdt
