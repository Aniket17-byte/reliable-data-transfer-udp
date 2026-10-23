#include <cassert>
#include <iostream>
#include <cmath>
#include "rtt_estimator.hpp"

namespace {

void test_initial_state() {
    rdt::RttEstimator estimator(1000.0, 200.0, 60000.0);
    assert(estimator.get_rto_ms() == 1000.0);
    assert(!estimator.has_valid_sample());
    std::cout << "  [PASS] test_initial_state\n";
}

void test_first_sample_rfc6298() {
    rdt::RttEstimator estimator(1000.0, 200.0, 60000.0, 0.125, 0.25, 4.0, 1.0);
    
    estimator.record_send(0);
    // Simulate ACK arrival after 100ms
    // To make it deterministic, we can test using update_estimates or measure elapsed
    auto sample = estimator.record_ack(0);
    assert(sample.has_value());
    assert(estimator.has_valid_sample());
    
    // First sample R: SRTT = R, RTTVAR = R/2, RTO = SRTT + max(G, 4*RTTVAR)
    double r = *sample;
    double expected_srtt = r;
    double expected_rttvar = r / 2.0;
    double expected_rto = expected_srtt + 4.0 * expected_rttvar; // clamped between 200 and 60000

    assert(std::abs(estimator.get_srtt_ms() - expected_srtt) < 0.1);
    assert(std::abs(estimator.get_rttvar_ms() - expected_rttvar) < 0.1);
    std::cout << "  [PASS] test_first_sample_rfc6298 (SampleRTT=" << r 
              << "ms, SRTT=" << estimator.get_srtt_ms() 
              << "ms, RTO=" << estimator.get_rto_ms() << "ms)\n";
}

void test_karns_algorithm_retransmission_ambiguity() {
    rdt::RttEstimator estimator;

    // Send packet 42
    estimator.record_send(42);

    // Retransmit packet 42 (e.g. after timeout)
    estimator.record_send(42);

    // ACK arrives for packet 42
    auto sample = estimator.record_ack(42);

    // Karn's Rule 1: Must be ignored due to ambiguity!
    assert(!sample.has_value());
    assert(!estimator.has_valid_sample());
    assert(estimator.get_stats().ignored_samples_karn == 1);
    std::cout << "  [PASS] test_karns_algorithm_retransmission_ambiguity (Ignored retransmitted ACK)\n";
}

void test_karns_algorithm_exponential_backoff() {
    rdt::RttEstimator estimator(1000.0, 200.0, 8000.0); // max 8000ms

    // Timeout 1: 1000 * 2 = 2000
    estimator.handle_timeout();
    assert(estimator.get_rto_ms() == 2000.0);

    // Timeout 2: 2000 * 2 = 4000
    estimator.handle_timeout();
    assert(estimator.get_rto_ms() == 4000.0);

    // Timeout 3: 4000 * 2 = 8000
    estimator.handle_timeout();
    assert(estimator.get_rto_ms() == 8000.0);

    // Timeout 4: Capped at max_rto (8000ms)
    estimator.handle_timeout();
    assert(estimator.get_rto_ms() == 8000.0);
    assert(estimator.get_stats().timeout_backoffs == 4);

    std::cout << "  [PASS] test_karns_algorithm_exponential_backoff\n";
}

} // namespace

void run_rtt_tests() {
    std::cout << "=== Running RTT & Karn's Algorithm Tests ===\n";
    test_initial_state();
    test_first_sample_rfc6298();
    test_karns_algorithm_retransmission_ambiguity();
    test_karns_algorithm_exponential_backoff();
    std::cout << "All RTT & Karn tests passed successfully!\n\n";
}
