#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>
#include "packet.hpp"
#include "checksum.hpp"
#include "channel_emulator.hpp"

void test_checksum_and_serialization() {
    std::cout << "[Test 1] Testing Packet Serialization & Checksum... ";
    std::string test_data = "Hello CS-30003! Reliable Data Transfer UDP test packet.";
    Packet p1(PacketType::DATA, 42, 10, test_data.data(), static_cast<uint16_t>(test_data.size()));
    Checksum::stamp_packet_checksum(p1);

    assert(Checksum::verify_packet_checksum(p1) == true);

    auto serialized = p1.serialize();
    Packet p2;
    bool des_ok = Packet::deserialize(serialized.data(), serialized.size(), p2);
    assert(des_ok == true);
    assert(p2.header.seq_num == 42);
    assert(p2.header.ack_num == 10);
    assert(p2.header.length == test_data.size());
    assert(Checksum::verify_packet_checksum(p2) == true);

    // Test corruption detection
    serialized[HEADER_SIZE + 2] ^= 0xFF; // Flip bits in payload
    Packet p3;
    assert(Packet::deserialize(serialized.data(), serialized.size(), p3) == true);
    assert(Checksum::verify_packet_checksum(p3) == false); // Checksum MUST fail!

    std::cout << "PASSED\n";
}

void test_prng_reproducibility() {
    std::cout << "[Test 2] Testing Seeded PRNG Determinism... ";
    ChannelConfig cfg1;
    cfg1.seed = 12345;
    ChannelConfig cfg2;
    cfg2.seed = 12345;

    std::mt19937_64 rng1(cfg1.seed);
    std::mt19937_64 rng2(cfg2.seed);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    for (int i = 0; i < 1000; ++i) {
        assert(dist(rng1) == dist(rng2));
    }
    std::cout << "PASSED\n";
}

int main() {
    std::cout << "=== Running CS-30003 Unit Verification Suite ===\n";
    test_checksum_and_serialization();
    test_prng_reproducibility();
    std::cout << "=== All Unit Tests Passed! ===\n";
    return 0;
}
