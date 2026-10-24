#pragma once
#include <chrono>
#include <algorithm>
#include <cmath>

class AdaptiveRTO {
public:
    AdaptiveRTO(double initial_rto_ms = 200.0,
                double min_rto_ms = 50.0,
                double max_rto_ms = 3000.0)
        : rto_ms_(initial_rto_ms),
          min_rto_ms_(min_rto_ms),
          max_rto_ms_(max_rto_ms),
          srtt_ms_(0.0),
          rttvar_ms_(0.0),
          first_measurement_(true) {}

    // Jacobson-Karels algorithm (RFC 6298)
    void update_rtt(double sample_rtt_ms) {
        if (first_measurement_) {
            srtt_ms_ = sample_rtt_ms;
            rttvar_ms_ = sample_rtt_ms / 2.0;
            first_measurement_ = false;
        } else {
            constexpr double alpha = 0.125; // 1/8
            constexpr double beta = 0.25;   // 1/4
            rttvar_ms_ = (1.0 - beta) * rttvar_ms_ + beta * std::abs(srtt_ms_ - sample_rtt_ms);
            srtt_ms_ = (1.0 - alpha) * srtt_ms_ + alpha * sample_rtt_ms;
        }

        rto_ms_ = srtt_ms_ + std::max(1.0, 4.0 * rttvar_ms_);
        clamp_rto();
    }

    // Exponential backoff on timeout
    void on_timeout() {
        rto_ms_ = std::min(rto_ms_ * 2.0, max_rto_ms_);
    }

    double get_rto_ms() const { return rto_ms_; }
    double get_srtt_ms() const { return srtt_ms_; }
    double get_rttvar_ms() const { return rttvar_ms_; }

private:
    void clamp_rto() {
        if (rto_ms_ < min_rto_ms_) rto_ms_ = min_rto_ms_;
        if (rto_ms_ > max_rto_ms_) rto_ms_ = max_rto_ms_;
    }

    double rto_ms_;
    double min_rto_ms_;
    double max_rto_ms_;
    double srtt_ms_;
    double rttvar_ms_;
    bool first_measurement_;
};
