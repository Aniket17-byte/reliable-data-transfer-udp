# CS-30003: Reliable Data Transfer (RDT) over UDP
Group Project Repository | Course: CS-30003 (Computer Networks)  
Project Proposal: P3 · Reliable Data Transfer over UDP  

## Team Members & Responsibilities
* **Aditya Bajpai (2405704 - [@AdityaBajpai07](https://github.com/AdityaBajpai07)):** Week 1: Core UDP socket architecture, packet format, checksum, file I/O.
* **Iti Sharma (24052005 - [@iti1503](https://github.com/iti1503)):** Week 2: Stop-and-Wait ARQ state machine, ACK processing, integrity checks.
* **Aniket Dwivedy (24041683 - [@Aniket17-byte](https://github.com/Aniket17-byte)):** Week 3 & 4: Sliding window management (GBN, SR), timers, adaptive RTO (Jacobson/Karels RFC 6298), Karn's algorithm.
* **Ananya Batra (24051681 - [@ananya92-arch](https://github.com/ananya92-arch)):** Week 5: C++ Channel emulator engine (loss, duplication, reordering, delay, jitter), seed-based PRNG, automated test matrix pipeline, performance benchmarking, plotting scripts.

---

## Repository Structure

```
reliable-data-transfer-udp/
├── CMakeLists.txt                # Unified CMake build configuration
├── README.md                     # Project overview and instructions
├── AI-USE.md                     # Required AI disclosure statement (Course Ground Rules)
├── include/                      # C++ Header files
│   ├── packet.h / packet.hpp     # Packet header format, serialization & MTU constants
│   ├── checksum.hpp              # IEEE 802.3 CRC-32 & checksum computation
│   ├── rtt_estimator.hpp         # Jacobson/Karels RFC 6298 & Karn's Algorithm (Aniket)
│   ├── timer_manager.hpp         # SingleTimer (GBN) & MultiTimerManager (SR) (Aniket)
│   ├── gbn_window.hpp            # Go-Back-N Sender & Receiver Windows (Aniket)
│   ├── sr_window.hpp             # Selective Repeat Sender/Receiver & SACK (Aniket)
│   ├── channel_emulator.hpp      # Channel Emulator core class & scheduler (Ananya)
│   ├── adaptive_rto.hpp          # Jacobson-Karels algorithm (RFC 6298)
│   └── socket_utils.hpp          # Cross-platform socket abstraction (Windows/Linux)
├── src/                          # C++ Source files
│   ├── rtt_estimator.cpp         # RTO adaptation & exponential backoff (Aniket)
│   ├── timer_manager.cpp         # High-resolution retransmission timers (Aniket)
│   ├── gbn_window.cpp            # GBN sliding window logic (Aniket)
│   ├── sr_window.cpp             # SR sliding window & SACK tracking (Aniket)
│   ├── channel_emulator.cpp      # Priority-queue packet scheduler & impairment engine (Ananya)
│   ├── main_emulator.cpp         # Emulator CLI entrypoint (Ananya)
│   ├── sender_sw.cpp             # Stop-and-Wait Sender (Iti)
│   ├── receiver_sw.cpp           # Stop-and-Wait Receiver (Iti)
│   ├── sender_gbn.cpp            # Go-Back-N Sender
│   ├── receiver_gbn.cpp          # Go-Back-N Receiver
│   ├── sender_sr.cpp             # Selective Repeat Sender
│   └── receiver_sr.cpp           # Selective Repeat Receiver
├── tests/                        # Comprehensive Unit Verification Suites
│   ├── test_main.cpp             # Master test entrypoint (Aniket)
│   ├── test_rtt.cpp              # Jacobson/Karels & Karn's algorithm tests (Aniket)
│   ├── test_timers.cpp           # Single & Multi-timer lifecycle tests (Aniket)
│   ├── test_gbn.cpp              # Go-Back-N window edge-case tests (Aniket)
│   ├── test_sr.cpp               # Selective Repeat out-of-order & SACK tests (Aniket)
│   └── test_emulator.cpp         # Channel Emulator unit test suite (Ananya)
├── benchmark/
│   └── benchmark_matrix.py       # Automated multi-parameter test runner & SHA-256 verifier
├── analysis/
│   └── plot_benchmarks.py        # Academic-grade Matplotlib plotting suite (300 DPI & PDF)
└── docs/
    ├── WEEK5_REPORT.md           # Full technical report, math derivations & viva Q&A
    └── viva_prep_guide.md        # Comprehensive viva defense & live code alteration guide
```

---

## Verification & Testing

### 1. Run Complete Unit Test Suite (Weeks 3 & 4 - Aniket Dwivedy)
```powershell
g++ -std=c++17 -Iinclude src/gbn_window.cpp src/rtt_estimator.cpp src/sr_window.cpp src/timer_manager.cpp tests/test_gbn.cpp tests/test_main.cpp tests/test_rtt.cpp tests/test_sr.cpp tests/test_timers.cpp -o run_tests.exe
.\run_tests.exe
```

### 2. Run Channel Emulator Unit Test Suite (Week 5 - Ananya Batra)
```powershell
g++ -std=c++17 -Iinclude tests/test_emulator.cpp src/channel_emulator.cpp -o test_emulator.exe -lws2_32
.\test_emulator.exe
```

---

## End-to-End Simulation & Benchmarks

### 1. Build All Targets with CMake
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
cd ..
```

### 2. Run the Channel Emulator
```bash
./build/channel_emulator \
  --loss 0.05 \
  --delay 50 \
  --jitter 10 \
  --corrupt 0.01 \
  --reorder 0.05 \
  --seed 42 \
  --verbose
```

### 3. Run Automated Python Benchmark Matrix
```bash
# Quick validation test (< 1 min)
python benchmark/benchmark_matrix.py --bin-dir ./build --quick

# Full parameter sweep (SW, GBN, SR)
python benchmark/benchmark_matrix.py --bin-dir ./build --size-mb 2.0 --results-dir ./results
```

### 4. Generate Performance Plots
```bash
python analysis/plot_benchmarks.py ./results
```
This produces 5 publication-grade figures in `./results/`:
- `fig1_throughput_vs_loss.png` / `.pdf`
- `fig2_retransmission_overhead.png` / `.pdf`
- `fig3_transfer_time_vs_delay.png` / `.pdf`
- `fig4_window_size_scaling.png` / `.pdf`
- `fig5_jitter_reordering.png` / `.pdf`
