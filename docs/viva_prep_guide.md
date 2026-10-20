# CS-30003 Viva Preparation & Live Modification Guide
**Author**: Aniket Dwivedy (Roll No: 24041683)  
**Topic**: Sliding Windows (GBN & Selective Repeat), SACK, Timers, Dynamic RTT (Jacobson/Karels), and Karn's Algorithm.

---

## 1. Top Theoretical Viva Questions & Core Concepts

### Q1. Why did the original RFC 793 RTT estimation fail, and how does Jacobson/Karels fix it?
- **RFC 793 flaw**: Only tracked Smoothed RTT ($SRTT$) with a fixed multiplier ($RTO = \beta \times SRTT$). It completely ignored **RTT variance**.
- In networks with high delay variance (jitter), a fixed multiplier causes **premature timeouts**, triggering unnecessary retransmissions and causing network congestion collapse.
- **Jacobson/Karels (RFC 6298)** introduces $RTTVAR$ (mean deviation).
  - First sample:
    $$SRTT = R, \quad RTTVAR = R / 2, \quad RTO = SRTT + \max(G, 4 \times RTTVAR)$$
  - Subsequent samples:
    $$RTTVAR = (1 - \beta) \times RTTVAR + \beta \times |SRTT - R| \quad (\beta = 0.25)$$
    $$SRTT = (1 - \alpha) \times SRTT + \alpha \times R \quad (\alpha = 0.125)$$
    $$RTO = SRTT + \max(G, 4 \times RTTVAR)$$
  - When jitter is high, $4 \times RTTVAR$ expands the timeout buffer automatically.

---

### Q2. What is the Retransmission Ambiguity Problem, and how does Karn's Algorithm solve it?
- **The Problem**: A packet is sent at $t_1$. Its timer expires, and it is retransmitted at $t_2$. An ACK arrives at $t_3$.
  - Does this ACK belong to transmission 1 or transmission 2?
  - If we assume transmission 1, $SampleRTT = t_3 - t_1$ (overestimation if it was actually for transmission 2).
  - If we assume transmission 2, $SampleRTT = t_3 - t_2$ (underestimation if the original packet was just delayed).
- **Karn's Algorithm Rule 1**: **NEVER** calculate SampleRTT for any packet that was retransmitted (`tx_count > 1`). Discard the sample.
- **Karn's Algorithm Rule 2**: On timeout, apply **exponential timer backoff**:
  $$RTO = \min(RTO \times 2, RTO_{max})$$
  Double the timeout on successive failures until an un-retransmitted packet receives an unambiguous ACK.

---

### Q3. In Selective Repeat, why MUST the receiver send an ACK for packets in $[rcv\_base - N, rcv\_base - 1]$?
- **Examiner Trap**: Many students think "the packet was already delivered, so drop it."
- **Crucial Truth**: If the receiver delivered packet $k$ and sent ACK($k$), but that ACK was lost or delayed in transit, the sender still thinks packet $k$ is unacknowledged.
- If the receiver drops the retransmitted packet without re-sending ACK($k$), the sender's window will **never advance**, causing permanent deadlock or endless retransmissions.
- Hence, the receiver must re-send ACK($k$) even though the packet has already been passed to the application.

---

### Q4. What is the maximum allowable window size $N$ for a sequence number space of $M$?
- **Stop-and-Wait**: $N = 1, M \ge 2$.
- **Go-Back-N**: $N \le M - 1$.
- **Selective Repeat**: $N \le M / 2$ ($2N \le M$).
  - *Why?* If $N > M/2$, the receiver window $[rcv\_base, rcv\_base + N - 1]$ overlaps with the next cycle of the sender window, making it impossible for the receiver to distinguish between a new packet and an old retransmission.

---

## 2. Likely Live Code Modifications During Viva Demo

The professor noted: *"I will ask you to modify your code live during the demo."* Here is how to perform the most common live requests:

### Modification 1: "Change the exponential backoff multiplier from 2.0 to 1.5"
- **File**: [`include/rtt_estimator.hpp`](file:///C:/Users/anike/.gemini/antigravity/scratch/reliable-data-transfer-udp/include/rtt_estimator.hpp) / [`src/rtt_estimator.cpp`](file:///C:/Users/anike/.gemini/antigravity/scratch/reliable-data-transfer-udp/src/rtt_estimator.cpp)
- **Code**:
  ```cpp
  // In rtt_estimator.hpp:
  double backoff_factor_{1.5}; // Changed from 2.0
  // Or call live via setter:
  estimator.set_backoff_factor(1.5);
  ```

### Modification 2: "Make RTTVAR adaptation more aggressive ($\beta = 0.5$)"
- **File**: [`src/rtt_estimator.cpp`](file:///C:/Users/anike/.gemini/antigravity/scratch/reliable-data-transfer-udp/src/rtt_estimator.cpp)
- **Code**:
  ```cpp
  // In constructor or setter:
  estimator.set_beta(0.5); // Default is 0.25
  ```

### Modification 3: "Add Fast Retransmit (Trigger retransmission on 3 duplicate ACKs in GBN)"
- **File**: [`src/gbn_window.cpp`](file:///C:/Users/anike/.gemini/antigravity/scratch/reliable-data-transfer-udp/src/gbn_window.cpp)
- **Code**:
  ```cpp
  if (result.is_duplicate) {
      duplicate_ack_count_++;
      if (duplicate_ack_count_ == 3) {
          // Fast Retransmit: trigger handle_timeout without waiting for timer expiry!
          return handle_timeout(current_rto);
      }
  } else {
      duplicate_ack_count_ = 0;
  }
  ```

### Modification 4: "Change minimum clamp for RTO to 50ms for low-latency emulator"
- **File**: [`include/rtt_estimator.hpp`](file:///C:/Users/anike/.gemini/antigravity/scratch/reliable-data-transfer-udp/include/rtt_estimator.hpp)
- **Code**:
  ```cpp
  estimator.set_min_rto_ms(50.0);
  ```

---

## 3. How Aniket's Code Integrates with Teammates

| Teammate | Deliverable | Integration Point |
|---|---|---|
| **Aditya Bajpai** | Socket & Packet Layout | Calls `GbnSenderWindow::push_packet()` or `SrSenderWindow::push_packet()`. Encodes `seq_num` and `SackInfo` into packet headers. |
| **Iti Sharma** | Protocol State Machine | Inspects `GbnSenderWindow::is_empty()`, `can_send()`, calls `process_ack()`, routes received packets to receiver windows. |
| **Ananya Batra** | Channel Emulator & Test Matrix | Introduces loss, delay, and reordering. Evaluates goodput vs loss rate using Aniket's adaptive RTO and SR vs GBN retransmissions. |
