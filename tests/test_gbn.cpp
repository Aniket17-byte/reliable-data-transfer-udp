#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include "gbn_window.hpp"

namespace {

void test_gbn_sender_window_flow() {
    size_t win_size = 4;
    rdt::GbnSenderWindow sender(win_size, 0);

    assert(sender.get_base() == 0);
    assert(sender.get_next_seq_num() == 0);
    assert(sender.can_send());
    assert(sender.is_empty());

    // Push 4 packets to fill window
    for (int i = 0; i < 4; ++i) {
        assert(sender.can_send());
        std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
        uint32_t seq = sender.push_packet(data, std::chrono::milliseconds(500));
        assert(seq == static_cast<uint32_t>(i));
    }

    // Window should now be full
    assert(sender.is_full());
    assert(!sender.can_send());
    assert(sender.get_in_flight_count() == 4);

    // Cumulative ACK for packet 1 arrives (acknowledges both 0 and 1)
    auto res = sender.process_ack(1, std::chrono::milliseconds(500));
    assert(res.is_valid);
    assert(!res.is_duplicate);
    assert(res.packets_acknowledged == 2);
    assert(sender.get_base() == 2);
    assert(sender.get_in_flight_count() == 2);
    assert(sender.can_send()); // 2 new slots opened!

    // Duplicate ACK for 0 arrives
    auto dup_res = sender.process_ack(0, std::chrono::milliseconds(500));
    assert(dup_res.is_duplicate);
    assert(!dup_res.is_valid);
    assert(sender.get_base() == 2); // Unchanged

    // ACK remaining packets (ACK 3 arrives)
    auto final_res = sender.process_ack(3, std::chrono::milliseconds(500));
    assert(final_res.is_valid);
    assert(final_res.window_empty);
    assert(sender.is_empty());
    assert(sender.get_base() == 4);

    std::cout << "  [PASS] test_gbn_sender_window_flow\n";
}

void test_gbn_sender_timeout() {
    rdt::GbnSenderWindow sender(3, 10);
    sender.push_packet({1}, std::chrono::milliseconds(40));
    sender.push_packet({2}, std::chrono::milliseconds(40));

    assert(!sender.check_timeout());

    // Wait for timer to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    assert(sender.check_timeout());

    // Handle timeout: must return ALL in-flight unacked packets (10 and 11)
    auto retransmit_pkts = sender.handle_timeout(std::chrono::milliseconds(100));
    assert(retransmit_pkts.size() == 2);
    assert(retransmit_pkts[0].seq_num == 10);
    assert(retransmit_pkts[1].seq_num == 11);

    // Timer re-armed, so not expired immediately
    assert(!sender.check_timeout());

    std::cout << "  [PASS] test_gbn_sender_timeout (Bulk retransmission of window)\n";
}

void test_gbn_receiver_flow() {
    rdt::GbnReceiverWindow receiver(0);

    // Packet 0 arrives (in-order)
    auto res0 = receiver.process_packet(0, {'A'});
    assert(res0.accepted);
    assert(res0.ack_to_send == 0);
    assert(receiver.get_expected_seq_num() == 1);

    // Packet 2 arrives (out-of-order! Packet 1 missing)
    auto res2 = receiver.process_packet(2, {'C'});
    assert(!res2.accepted);
    assert(res2.ack_to_send == 0); // Re-ACKs last in-order packet (0)
    assert(receiver.get_expected_seq_num() == 1); // Expected unchanged

    // Packet 1 arrives
    auto res1 = receiver.process_packet(1, {'B'});
    assert(res1.accepted);
    assert(res1.ack_to_send == 1);
    assert(receiver.get_expected_seq_num() == 2);

    std::cout << "  [PASS] test_gbn_receiver_flow\n";
}

} // namespace

void run_gbn_tests() {
    std::cout << "=== Running Go-Back-N (GBN) Window Tests ===\n";
    test_gbn_sender_window_flow();
    test_gbn_sender_timeout();
    test_gbn_receiver_flow();
    std::cout << "All GBN tests passed successfully!\n\n";
}
