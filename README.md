# Reliable Data Transfer over UDP (P3)

Implementation of reliable data transfer protocols over UDP featuring Stop-and-Wait, Go-Back-N, and Selective Repeat ARQ, dynamic RTO estimation (Jacobson/Karels), Karn's algorithm, high-precision timers, and network impairment emulation.

---

## Week 1: Baseline Implementation
**Author:** Aditya Bajpai (2405704)

Cross-platform C++ baseline for reliable file transfer over UDP sockets. Implements core packet structures, network byte-order serialization, 16-bit Internet Checksum verification, and binary file chunking with reconstruction.

### 📁 Files Implemented
* **`packet.h`** — Defines the packed 13-byte header structure (`Header`), full packet format (`Packet`), control flags, net-to-host serialization logic, and checksum calculations.
* **`sender_base.cpp`** — Handles cross-platform UDP socket setup, binary file reading in 1024-byte chunks, packet header creation, checksum generation, and transmission.
* **`receiver_base.cpp`** — Binds to port `8080`, listens for incoming UDP packets, validates checksum integrity, and reconstructs the output file.

### 📋 Packet Header Format (13 Bytes Packed)
| Field | Type | Size | Description |
| :--- | :--- | :--- | :--- |
| `seq_num` | `uint32_t` | 4 Bytes | Sequence number of the payload byte stream |
| `ack_num` | `uint32_t` | 4 Bytes | Acknowledgement number |
| `checksum` | `uint16_t` | 2 Bytes | 1's complement Internet Checksum |
| `payload_len` | `uint16_t` | 2 Bytes | Size of payload chunk (Max 1024 bytes) |
| `flags` | `uint8_t` | 1 Byte | Packet control flags (`FLAG_DATA`, `FLAG_ACK`, `FLAG_SYN`, `FLAG_FIN`) |

### 🚀 How to Run Local Baseline Test
```powershell
# Terminal 1 - Build and Run Receiver
g++ receiver_base.cpp -o receiver.exe -lws2_32
.\receiver.exe

# Terminal 2 - Build and Run Sender
g++ sender_base.cpp -o sender.exe -lws2_32
.\sender.exe
```

---

## Week 2: Stop-and-Wait ARQ
**Author:** Iti Sharma (24052005)

Stop-and-Wait ARQ protocol implementation featuring basic timeout and retransmission, checksum integrity verification, and alternating ACK validation.

### 📁 Files Implemented
* **`sender_saw.cpp`** — Stop-and-Wait sender with socket timeout and single-packet retransmission loop.
* **`receiver_saw.cpp`** — Stop-and-Wait receiver validating checksum and returning ACKs for expected sequence numbers.

### 🚀 How to Run Stop-and-Wait ARQ
```powershell
# Terminal 1 - Run SAW Receiver
g++ receiver_saw.cpp -o receiver_saw.exe -lws2_32
.\receiver_saw.exe

# Terminal 2 - Run SAW Sender
g++ sender_saw.cpp -o sender_saw.exe -lws2_32
.\sender_saw.exe
```

---

## Week 3 & 4: Sliding Windows, Retransmission Timers & Adaptive RTO
**Author:** Aniket Dwivedy (24041683)

Full modular implementations of sliding window buffers (Go-Back-N and Selective Repeat), dynamic RTT estimation using Jacobson/Karels algorithm (RFC 6298), Karn's algorithm for retransmission ambiguity resolution, high-precision timers, and SACK state tracking.

### 📁 Files & Modules Implemented
* **`include/rtt_estimator.hpp` & `src/rtt_estimator.cpp`** — Jacobson/Karels adaptive RTO with Smoothed RTT ($SRTT$) and RTT Variation ($RTTVAR$) smoothing; Karn's algorithm sample discard and exponential timer backoff.
* **`include/timer_manager.hpp` & `src/timer_manager.cpp`** — High-precision `SingleTimer` (GBN oldest unacked packet) and `MultiTimerManager` (Selective Repeat per-packet deadlines).
* **`include/gbn_window.hpp` & `src/gbn_window.cpp`** — Go-Back-N sender sliding window buffer, cumulative ACK sliding, and bulk timeout retransmission.
* **`include/sr_window.hpp` & `src/sr_window.cpp`** — Selective Repeat sender/receiver sliding windows, out-of-order packet buffering, contiguous delivery sliding, and SACK block/bitmask generation.
* **`sender_gbn.cpp` & `receiver_gbn.cpp`** — End-to-end runnable network file transfer program using Go-Back-N ARQ.
* **`sender_sr.cpp` & `receiver_sr.cpp`** — End-to-end runnable network file transfer program using Selective Repeat ARQ.
* **`tests/`** — Standalone unit test suite verifying protocol edge cases and timing with 100% pass rate.
* **`AI-USE.md`** — Mandatory course AI disclosure log.
* **`docs/viva_prep_guide.md`** — Comprehensive viva defense preparation and live code modification guide.

### 🚀 How to Run Go-Back-N (GBN) Network Transfer
```powershell
# Terminal 1 - Run GBN Receiver (Port 8082)
g++ -std=c++17 -Iinclude receiver_gbn.cpp src/gbn_window.cpp -o receiver_gbn.exe -lws2_32
.\receiver_gbn.exe

# Terminal 2 - Run GBN Sender
g++ -std=c++17 -Iinclude sender_gbn.cpp src/gbn_window.cpp src/rtt_estimator.cpp src/timer_manager.cpp -o sender_gbn.exe -lws2_32
.\sender_gbn.exe test_input.txt 8082 127.0.0.1 8
```

### 🚀 How to Run Selective Repeat (SR) Network Transfer
```powershell
# Terminal 1 - Run SR Receiver (Port 8083)
g++ -std=c++17 -Iinclude receiver_sr.cpp src/sr_window.cpp -o receiver_sr.exe -lws2_32
.\receiver_sr.exe

# Terminal 2 - Run SR Sender
g++ -std=c++17 -Iinclude sender_sr.cpp src/sr_window.cpp src/rtt_estimator.cpp src/timer_manager.cpp -o sender_sr.exe -lws2_32
.\sender_sr.exe test_input.txt 8083 127.0.0.1 8
```

### 🧪 How to Run Protocol Verification Tests
```powershell
g++ -std=c++17 -Iinclude src/gbn_window.cpp src/rtt_estimator.cpp src/sr_window.cpp src/timer_manager.cpp tests/test_gbn.cpp tests/test_main.cpp tests/test_rtt.cpp tests/test_sr.cpp tests/test_timers.cpp -o run_tests.exe
.\run_tests.exe
```
