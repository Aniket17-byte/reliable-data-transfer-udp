#include <cassert>
#include <iostream>
#include <thread>
#include "timer_manager.hpp"

namespace {

void test_single_timer_lifecycle() {
    rdt::SingleTimer timer;
    assert(!timer.is_running());
    assert(!timer.is_expired());

    timer.start(std::chrono::milliseconds(50));
    assert(timer.is_running());
    assert(!timer.is_expired());
    assert(timer.remaining_time().count() > 0);

    // Wait until expired
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    assert(timer.is_running());
    assert(timer.is_expired());
    assert(timer.remaining_time().count() == 0);

    timer.stop();
    assert(!timer.is_running());
    assert(!timer.is_expired());

    std::cout << "  [PASS] test_single_timer_lifecycle\n";
}

void test_multi_timer_manager() {
    rdt::MultiTimerManager mgr;
    assert(mgr.active_count() == 0);

    // Start timer for packet 10 (short: 30ms) and packet 20 (long: 200ms)
    mgr.start_timer(10, std::chrono::milliseconds(30));
    mgr.start_timer(20, std::chrono::milliseconds(200));

    assert(mgr.is_running(10));
    assert(mgr.is_running(20));
    assert(mgr.active_count() == 2);

    auto earliest = mgr.time_until_earliest_timeout();
    assert(earliest.has_value());
    assert(earliest->count() <= 30);

    // Wait for packet 10 to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(40));

    auto expired = mgr.get_expired_packets();
    assert(expired.size() == 1);
    assert(expired[0] == 10);

    // Stop timer 10 (ACK received)
    mgr.stop_timer(10);
    assert(!mgr.is_running(10));
    assert(mgr.active_count() == 1);

    // Earliest should now reflect packet 20
    earliest = mgr.time_until_earliest_timeout();
    assert(earliest.has_value());
    assert(earliest->count() > 0);

    std::cout << "  [PASS] test_multi_timer_manager\n";
}

} // namespace

void run_timer_tests() {
    std::cout << "=== Running Timer Manager Tests ===\n";
    test_single_timer_lifecycle();
    test_multi_timer_manager();
    std::cout << "All Timer Manager tests passed successfully!\n\n";
}
