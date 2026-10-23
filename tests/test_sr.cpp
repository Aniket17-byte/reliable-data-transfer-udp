#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include "sr_window.hpp"

namespace {

void test_sr_sender_independent_acks_and_sliding() {
    rdt::SrSenderWindow sender(4, 0);

    // Send packets 0, 1, 2, 3
    for (int i = 0; i < 4; ++i) {
        sender.push_packet({static_cast<uint8_t>(i)}, std::chrono::milliseconds(500));
    }
    assert(sender.is_full());
    assert(sender.get_send_base() == 0);

    // 1. ACK for packet 2 arrives first (Selective ACK)
    bool slid = sender.process_ack(2);
    assert(!slid); // Base cannot advance yet because packets 0 and 1 are still unacked
    assert(sender.get_send_base() == 0);
    assert(sender.get_packet_state(2) == rdt::SrPacketState::ACKED);
    assert(sender.get_packet_state(0) == rdt::SrPacketState::SENT_UNACKED);

    // 2. ACK for packet 0 arrives
    slid = sender.process_ack(0);
    assert(slid);
    assert(sender.get_send_base() == 1); // Advances to 1!

    // 3. ACK for packet 1 arrives -> base should skip past packet 2 (already ACKed) and jump to 3!
    slid = sender.process_ack(1);
    assert(slid);
    assert(sender.get_send_base() == 3); // Jumped to 3!
    assert(sender.get_in_flight_count() == 1); // Only packet 3 remains

    // 4. ACK packet 3 -> buffer becomes empty
    slid = sender.process_ack(3);
    assert(slid);
    assert(sender.is_empty());
    assert(sender.get_send_base() == 4);

    std::cout << "  [PASS] test_sr_sender_independent_acks_and_sliding\n";
}

void test_sr_sender_isolated_timeout() {
    rdt::SrSenderWindow sender(3, 10);
    // Packet 10 (long timeout: 500ms), Packet 11 (short timeout: 40ms)
    sender.push_packet({1}, std::chrono::milliseconds(500));
    sender.push_packet({2}, std::chrono::milliseconds(40));

    // Wait 50ms for packet 11 to timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto expired = sender.get_expired_packets();
    assert(expired.size() == 1);
    assert(expired[0] == 11); // Only packet 11 timed out! Packet 10 is untouched!

    // Retransmit only packet 11
    auto retransmit_pkt = sender.handle_timeout_packet(11, std::chrono::milliseconds(100));
    assert(retransmit_pkt.has_value());
    assert(retransmit_pkt->seq_num == 11);
    assert(retransmit_pkt->tx_count == 2);

    std::cout << "  [PASS] test_sr_sender_isolated_timeout (No redundant retransmission)\n";
}

void test_sr_receiver_out_of_order_buffering_and_delivery() {
    rdt::SrReceiverWindow receiver(5, 0);

    // Packet 1 arrives before 0
    auto res1 = receiver.process_packet(1, {'B'});
    assert(res1.action == rdt::SrReceiverWindow::ProcessAction::BUFFER_OUT_OF_ORDER);
    assert(res1.deliverable_packets.empty());
    assert(receiver.get_rcv_base() == 0);
    assert(receiver.get_buffered_out_of_order_count() == 1);

    // Packet 2 arrives
    auto res2 = receiver.process_packet(2, {'C'});
    assert(res2.action == rdt::SrReceiverWindow::ProcessAction::BUFFER_OUT_OF_ORDER);
    assert(res2.deliverable_packets.empty());
    assert(receiver.get_rcv_base() == 0);
    assert(receiver.get_buffered_out_of_order_count() == 2);

    // Packet 0 arrives -> base arrived! Should deliver 0, 1, and 2 in order!
    auto res0 = receiver.process_packet(0, {'A'});
    assert(res0.action == rdt::SrReceiverWindow::ProcessAction::DELIVER_IN_ORDER);
    assert(res0.deliverable_packets.size() == 3);
    assert(res0.deliverable_packets[0].seq_num == 0);
    assert(res0.deliverable_packets[1].seq_num == 1);
    assert(res0.deliverable_packets[2].seq_num == 2);
    assert(receiver.get_rcv_base() == 3);
    assert(receiver.get_buffered_out_of_order_count() == 0);

    // Duplicate packet 1 arrives (past base) -> must still send ACK 1 to prevent deadlock!
    auto res_dup = receiver.process_packet(1, {'B'});
    assert(res_dup.action == rdt::SrReceiverWindow::ProcessAction::DUPLICATE_PAST_WINDOW);
    assert(res_dup.ack_to_send == 1);

    std::cout << "  [PASS] test_sr_receiver_out_of_order_buffering_and_delivery\n";
}

void test_sack_blocks_and_processing() {
    rdt::SrReceiverWindow receiver(10, 0);

    // Arrive: 1, 3, 4
    receiver.process_packet(1, {'X'});
    receiver.process_packet(3, {'Y'});
    receiver.process_packet(4, {'Z'});

    auto sack = receiver.generate_sack_info();
    assert(!sack.has_cumulative_ack);
    // Should have 2 blocks: [1, 1] and [3, 4]
    assert(sack.blocks.size() == 2);
    assert(sack.blocks[0].start_seq == 1 && sack.blocks[0].end_seq == 1);
    assert(sack.blocks[1].start_seq == 3 && sack.blocks[1].end_seq == 4);

    // Sender processes this SACK info
    rdt::SrSenderWindow sender(10, 0);
    for (int i = 0; i < 6; ++i) {
        sender.push_packet({static_cast<uint8_t>(i)}, std::chrono::milliseconds(500));
    }

    sender.process_sack(sack);
    assert(sender.get_packet_state(1) == rdt::SrPacketState::ACKED);
    assert(sender.get_packet_state(3) == rdt::SrPacketState::ACKED);
    assert(sender.get_packet_state(4) == rdt::SrPacketState::ACKED);
    assert(sender.get_packet_state(0) == rdt::SrPacketState::SENT_UNACKED);
    assert(sender.get_packet_state(2) == rdt::SrPacketState::SENT_UNACKED);

    std::cout << "  [PASS] test_sack_blocks_and_processing\n";
}

} // namespace

void run_sr_tests() {
    std::cout << "=== Running Selective Repeat (SR) & SACK Tests ===\n";
    test_sr_sender_independent_acks_and_sliding();
    test_sr_sender_isolated_timeout();
    test_sr_receiver_out_of_order_buffering_and_delivery();
    test_sack_blocks_and_processing();
    std::cout << "All Selective Repeat & SACK tests passed successfully!\n\n";
}
