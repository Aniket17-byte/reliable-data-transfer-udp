#include <iostream>

void run_rtt_tests();
void run_timer_tests();
void run_gbn_tests();
void run_sr_tests();

int main() {
    std::cout << "========================================================\n";
    std::cout << " CS-30003: RELIABLE DATA TRANSFER OVER UDP\n";
    std::cout << " Verification Suite for Aniket Dwivedy's Modules\n";
    std::cout << " - Dynamic RTT Measurement (Jacobson/Karels, RFC 6298)\n";
    std::cout << " - Karn's Algorithm (Ambiguity Handling & Backoff)\n";
    std::cout << " - Retransmission Timers (Single & Multi-packet)\n";
    std::cout << " - Sliding Window Management (Go-Back-N & Selective Repeat)\n";
    std::cout << " - SACK State Tracking (Blocks & Bitmask)\n";
    std::cout << "========================================================\n\n";

    try {
        run_rtt_tests();
        run_timer_tests();
        run_gbn_tests();
        run_sr_tests();

        std::cout << "========================================================\n";
        std::cout << " >>> ALL UNIT TESTS PASSED SUCCESSFULLY! <<<\n";
        std::cout << "========================================================\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION DURING TEST EXECUTION: " << e.what() << "\n";
        return 1;
    }
}
