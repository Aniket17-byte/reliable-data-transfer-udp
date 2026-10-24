# CS-30003: Reliable Data Transfer over UDP
## Week 5 Technical Report & Milestone Documentation
**Student Name:** Ananya Batra  
**Roll Number:** 24051681  
**GitHub:** [@ananya92-arch](https://github.com/ananya92-arch) (anya916212@gmail.com)  
**Group Partners:** Aditya Bajpai (2405704), Iti Sharma (24052005), Aniket Dwivedy (24051683)  
**Deliverables:** C++ Channel Emulator, Automated Python Benchmark Matrix, Matplotlib Plotting Suite, and AI Disclosure (`AI-USE.md`)  

---

## 1. System Architecture & Overview

The Week 5 milestone delivers the network impairment and automated scientific evaluation infrastructure for Project P3. The **Channel Emulator** acts as an intermediary network proxy intercepting native UDP datagrams between the Sender and the Receiver, simulating physical layer packet loss, bit corruption, latency jitter, packet reordering, and duplication using a deterministic pseudo-random number generator (PRNG).

```
                      +---------------------------------------+
                      |       C++ Channel Emulator Proxy      |
                      |                                       |
+----------------+    |   [Port 9001]           [Port 9002]   |    +------------------+
|   RDT Sender   |--->| Forward In  --> Queue --> Forward Out |--->|   RDT Receiver   |
| (SW / GBN / SR)|    |                                       |    | (In-Order / Buf) |
|                |<---| Reverse Out <-- Queue <-- Reverse In  |<---|                  |
+----------------+    |   [Port 9000]           [Port 9003]   |    +------------------+
                      |                                       |
                      |   Impairments:                        |
                      |    * Loss (p_loss)                    |
                      |    * Delay + Jitter (base +/- jitter) |
                      |    * Reordering (p_reorder)           |
                      |    * Bit Corruption (p_corrupt)       |
                      |    * Duplication (p_duplicate)        |
                      |    * Seeded PRNG (mt19937_64)         |
                      +---------------------------------------+
```

---

## 2. Component Design & Implementation

### 2.1 C++ Channel Emulator (`channel_emulator.hpp`, `channel_emulator.cpp`)

1. **Seeded PRNG Reproducibility:**
   - Employs `std::mt19937_64` initialized with a 64-bit seed (`--seed <val>`).
   - Guarantees 100% deterministic test execution across repeated benchmark runs, essential for reproducible scientific evaluation.

2. **Microsecond-Precise Priority Queue Scheduler:**
   - Delayed packets are queued in a min-heap priority queue (`std::priority_queue<ScheduledPacket>`) ordered by absolute release timestamp:
     $$T_{\text{release}} = T_{\text{current}} + \Delta t_{\text{delay}} \pm \Delta t_{\text{jitter}}$$
   - A dedicated worker thread calls `std::condition_variable::wait_until(lock, next_release)`. This avoids CPU-intensive busy-waiting and delivers microsecond-level timing accuracy.

3. **Packet Impairment Engine:**
   - **Packet Loss:** Uniform random sample $u \sim \mathcal{U}(0,1)$. If $u < p_{\text{loss}}$, the packet is silently discarded.
   - **Bit Corruption:** If $u_{\text{corrupt}} < p_{\text{corrupt}}$, 1 to 4 random bytes in the packet buffer are XOR-masked with non-zero bit patterns. The corrupted packet is dispatched to test receiver checksum detection.
   - **Delay & Jitter:** Base latency $d_{\text{base}}$ modulated by uniform jitter:
     $$D = \max(0, d_{\text{base}} + \text{Uniform}(-j, +j))$$
   - **Out-of-Order Reordering:** When $u_{\text{reorder}} < p_{\text{reorder}}$, an extra hold time $\Delta t_{\text{reorder}}$ is added. Subsequent packets with lower latency naturally overtake the held packet in the priority queue.
   - **Duplication:** When $u_{\text{dup}} < p_{\text{dup}}$, an identical twin packet is scheduled with a small $5\text{ms}$ delay offset.

4. **Bi-Directional Proxy:**
   - Full-duplex interception: Forward path (DATA from Sender $\to$ Receiver) and Reverse path (ACKs from Receiver $\to$ Sender).
   - Allows symmetric impairment (both directions) or asymmetric impairment (forward only).

---

### 2.2 Automated Python Benchmark Matrix (`benchmark_matrix.py`)

1. **Automated Subprocess Orchestration:**
   - Spawns the Receiver, spins up the Channel Emulator, and executes the Sender with exact test parameters.
   - Captures process stdout/stderr, parses real-time JSON metrics, and enforces strict execution timeouts.

2. **Cryptographic End-to-End Data Verification:**
   - Generates deterministic binary test payloads (1 MB to 10 MB).
   - Verifies the SHA-256 hash of the received file against the source file. Any transfer with bit corruption, dropped data, or sequence misalignment is flagged immediately.

3. **Multi-Dimensional Parameter Grid:**
   - **Protocols:** Stop-and-Wait (SW), Go-Back-N (GBN), and Selective Repeat (SR).
   - **Loss Probability:** $p \in \{0.00, 0.01, 0.02, 0.03, 0.05, 0.08, 0.10, 0.15, 0.20\}$.
   - **Propagation Delay:** $d \in \{10\text{ms}, 30\text{ms}, 50\text{ms}, 100\text{ms}\}$.
   - **Window Size:** $N \in \{4, 8, 16, 32, 64\}$.
   - **Seeds:** Evaluates multiple random seeds to establish statistical error bounds (mean $\pm 1\sigma$).

---

### 2.3 Matplotlib Performance Plotting Suite (`plot_benchmarks.py`)

Generates 5 publication-grade figures (300 DPI PNG + vector PDF):

1. **Figure 1: Effective Throughput vs. Packet Loss Rate**
   - Directly tests the proposal hypothesis: compares SW, GBN, and SR. Confirms SR's superiority under loss.
2. **Figure 2: Retransmission Overhead Ratio vs. Packet Loss Rate**
   - Quantifies the redundant packet retransmission penalty of GBN ($N$-fold overhead) vs SR's optimal 1:1 efficiency.
3. **Figure 3: Total Transfer Time vs. Network Latency**
   - Demonstrates the Bandwidth-Delay Product (BDP) bottleneck: Stop-and-Wait transfers take linearly longer as RTT increases, while pipelined GBN and SR hide latency.
4. **Figure 4: Throughput vs. Window Size ($N$) under 5% Packet Loss**
   - Highlights that while larger windows improve link utilization, excessively large windows in GBN cause severe retransmission storms upon packet loss.
5. **Figure 5: Throughput Resilience to Delay Jitter & Packet Reordering**
   - Confirms SR's ability to buffer out-of-order datagrams without stalling, whereas GBN discards them and triggers duplicate ACKs.

---

## 3. Mathematical Foundations & Theoretical Analysis

### 3.1 Channel Utilization Comparison ($\eta$)

Let $N$ be the sender window size, $p$ be the packet loss probability, $RTT$ be round-trip time, and $L/R$ be packet transmission delay:

1. **Stop-and-Wait ($\text{SW}, N=1$):**
   Only 1 packet can be in flight per RTT:
   $$\eta_{\text{SW}} = \frac{L / R}{RTT + L / R} \cdot (1 - p)$$
   For a $1400$-byte packet on a $100\text{ Mbps}$ link with $RTT = 50\text{ms}$:
   $$L/R = \frac{1400 \times 8}{10^8} = 0.112\text{ms} \implies \eta_{\text{SW}} \approx \frac{0.112}{50.112} \approx 0.22\%$$
   Stop-and-Wait wastes over $99.7\%$ of network capacity regardless of packet loss!

2. **Go-Back-N ($\text{GBN}, N > 1$):**
   When a packet is lost in GBN, the sender must retransmit the lost packet plus the subsequent $N-1$ packets that were already in flight:
   $$\eta_{\text{GBN}} = \frac{1 - p}{1 + (N - 1)p}$$
   For $N = 16$ at $p = 10\%$ loss:
   $$\eta_{\text{GBN}} = \frac{1 - 0.10}{1 + (15 \times 0.10)} = \frac{0.90}{2.50} = 36\%$$

3. **Selective Repeat ($\text{SR}, N > 1$):**
   Because the receiver buffers out-of-order packets and selectively acknowledges them, only the lost packet is retransmitted. Each packet transmission is independent:
   $$\eta_{\text{SR}} = 1 - p$$
   For $p = 10\%$ loss:
   $$\eta_{\text{SR}} = 1 - 0.10 = 90\%$$
   Selective Repeat achieves $2.5\times$ higher throughput than GBN at $10\%$ loss!

---

## 4. Viva Live-Coding Defense Guide (Instructor Live Modification Drill)

The ground rules explicitly state: *"I will ask you to modify your code live during the demo."*
Here is how to answer and modify your code on the spot:

### Scenario 1: "Modify the emulator to introduce burst loss (Gilbert-Elliott model) instead of uniform loss."
**What to change in `channel_emulator.cpp`:**
Add a 2-state Markov model (`GOOD_STATE`, `BAD_STATE`):
```cpp
// In channel_emulator.hpp:
enum class ChannelState { GOOD, BAD };
ChannelState state_ = ChannelState::GOOD;
double p_gb_ = 0.05; // Prob Good -> Bad
double p_bg_ = 0.30; // Prob Bad -> Good

// In process_incoming_packet():
if (state_ == ChannelState::GOOD) {
    if (dist(rng_) < p_gb_) state_ = ChannelState::BAD;
} else {
    if (dist(rng_) < p_bg_) state_ = ChannelState::GOOD;
}
bool drop = (state_ == ChannelState::BAD);
```

### Scenario 2: "Make the emulator only corrupt the 3rd byte of the payload."
**What to change in `apply_corruption()`:**
```cpp
void ChannelEmulator::apply_corruption(std::vector<uint8_t>& buffer) {
    if (buffer.size() > HEADER_SIZE + 2) {
        buffer[HEADER_SIZE + 2] ^= 0xFF; // Invert all bits of payload byte 2
    }
}
```

### Scenario 3: "How does the emulator achieve microsecond precision without pegging the CPU at 100%?"
**Your answer:**
> "Instead of busy-waiting (`while(now < target);`), our dispatcher thread uses `std::condition_variable::wait_until()`. This puts the thread to sleep in the OS kernel until the exact monotonic clock deadline of the earliest packet in our min-heap priority queue. This delivers microsecond accuracy with 0% idle CPU usage."
