#include "rtt_estimator.hpp"
#include <algorithm>
#include <cmath>

namespace rdt {

RttEstimator::RttEstimator(double initial_rto_ms,
                           double min_rto_ms,
                           double max_rto_ms,
                           double alpha,
                           double beta,
                           double k,
                           double clock_granularity_ms)
    : initial_rto_ms_(initial_rto_ms),
      min_rto_ms_(min_rto_ms),
      max_rto_ms_(max_rto_ms),
      alpha_(alpha),
      beta_(beta),
      k_(k),
      clock_granularity_ms_(clock_granularity_ms),
      rto_ms_(initial_rto_ms) {
    stats_.rto_ms = rto_ms_;
}

void RttEstimator::record_send(uint32_t seq_num) {
    auto now = std::chrono::steady_clock::now();
    auto it = in_flight_packets_.find(seq_num);
    if (it == in_flight_packets_.end()) {
        // First transmission of this sequence number
        PacketTxInfo info;
        info.seq_num = seq_num;
        info.first_sent_time = now;
        info.last_sent_time = now;
        info.tx_count = 1;
        info.is_retransmitted = false;
        in_flight_packets_[seq_num] = info;
    } else {
        // Retransmission: mark retransmitted per Karn's algorithm
        it->second.tx_count++;
        it->second.is_retransmitted = true;
        it->second.last_sent_time = now;
    }
}

std::optional<double> RttEstimator::record_ack(uint32_t seq_num) {
    auto now = std::chrono::steady_clock::now();
    auto it = in_flight_packets_.find(seq_num);
    if (it == in_flight_packets_.end()) {
        // Unknown or already cleared sequence number
        return std::nullopt;
    }

    PacketTxInfo info = it->second;
    in_flight_packets_.erase(it);

    // Karn's Algorithm Rule 1:
    // Retransmitted packets MUST NOT be used for RTT sampling due to acknowledgment ambiguity
    // (cannot know if ACK is for original or retransmission).
    if (info.is_retransmitted) {
        stats_.ignored_samples_karn++;
        return std::nullopt;
    }

    // Valid sample measured from original transmission
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - info.first_sent_time).count();
    double sample_rtt_ms = static_cast<double>(elapsed_us) / 1000.0;

    update_estimates(sample_rtt_ms);
    return sample_rtt_ms;
}

void RttEstimator::update_estimates(double sample_rtt_ms) {
    stats_.total_samples++;
    stats_.last_sample_rtt_ms = sample_rtt_ms;

    if (!has_sample_) {
        // RFC 6298 Section 2.2: First RTT measurement
        // SRTT <- R
        // RTTVAR <- R / 2
        // RTO <- SRTT + max(G, K * RTTVAR)
        srtt_ms_ = sample_rtt_ms;
        rttvar_ms_ = sample_rtt_ms / 2.0;
        rto_ms_ = srtt_ms_ + std::max(clock_granularity_ms_, k_ * rttvar_ms_);
        has_sample_ = true;
    } else {
        // RFC 6298 Section 2.3: Subsequent RTT measurements
        // RTTVAR <- (1 - beta) * RTTVAR + beta * |SRTT - R|
        // SRTT   <- (1 - alpha) * SRTT + alpha * R
        // RTO    <- SRTT + max(G, K * RTTVAR)
        rttvar_ms_ = (1.0 - beta_) * rttvar_ms_ + beta_ * std::abs(srtt_ms_ - sample_rtt_ms);
        srtt_ms_ = (1.0 - alpha_) * srtt_ms_ + alpha_ * sample_rtt_ms;
        rto_ms_ = srtt_ms_ + std::max(clock_granularity_ms_, k_ * rttvar_ms_);
    }

    clamp_rto();

    stats_.srtt_ms = srtt_ms_;
    stats_.rttvar_ms = rttvar_ms_;
    stats_.rto_ms = rto_ms_;
}

void RttEstimator::handle_timeout() {
    stats_.timeout_backoffs++;
    // Karn's Algorithm Rule 2:
    // When a timeout occurs, double the RTO (exponential backoff)
    rto_ms_ *= backoff_factor_;
    clamp_rto();
    stats_.rto_ms = rto_ms_;
}

void RttEstimator::clamp_rto() {
    rto_ms_ = std::clamp(rto_ms_, min_rto_ms_, max_rto_ms_);
}

void RttEstimator::remove_seq(uint32_t seq_num) {
    in_flight_packets_.erase(seq_num);
}

void RttEstimator::reset() {
    in_flight_packets_.clear();
    has_sample_ = false;
    srtt_ms_ = 0.0;
    rttvar_ms_ = 0.0;
    rto_ms_ = initial_rto_ms_;
    stats_ = RttStats{};
    stats_.rto_ms = rto_ms_;
}

std::chrono::milliseconds RttEstimator::get_rto_duration() const {
    return std::chrono::milliseconds(static_cast<int64_t>(std::ceil(rto_ms_)));
}

} // namespace rdt
