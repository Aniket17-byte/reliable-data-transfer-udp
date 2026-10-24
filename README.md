# CS-30003: Reliable Data Transfer (RDT) over UDP
Group Project Repository | Course: CS-30003 (Computer Networks)  
Project Proposal: P3 · Reliable Data Transfer over UDP  

## Team Members & Responsibilities
* **Aditya Bajpai (2405704):** Core UDP socket architecture, packet format, checksum, file I/O.
* **Iti Sharma (24052005):** Stop-and-Wait ARQ state machine, ACK processing, integrity checks.
* **Aniket Dwivedy (24041683):** Sliding window management (GBN, SR), timers, adaptive RTO (Jacobson/Karels), Karn's algorithm.
* **Ananya Batra (24051681 - [@ananya92-arch](https://github.com/ananya92-arch)):** C++ Channel emulator engine (loss, duplication, reordering, delay, jitter), seed-based PRNG, automated test matrix pipeline, performance benchmarking, plotting scripts.

---

## Repository Structure

```
reliable-data-transfer-udp/
├── CMakeLists.txt                # Unified CMake build configuration
├── README.md                     # Project overview and instructions
├── AI-USE.md                     # Required AI disclosure statement (Ground Rules)
├── include/                      # C++ Header files
│   ├── packet.hpp                # Packet header format, serialization & MTU constants
│   ├── checksum.hpp              # IEEE 802.3 CRC-32 & checksum computation
│   ├── channel_emulator.hpp      # Channel Emulator core class & scheduler definitions
│   ├── adaptive_rto.hpp          # Jacobson-Karels algorithm (RFC 6298)
│   └── socket_utils.hpp          # Cross-platform socket abstraction (Windows/Linux)
├── src/                          # C++ Source files
│   ├── channel_emulator.cpp      # Priority-queue packet scheduler & impairment engine
│   ├── main_emulator.cpp         # Emulator CLI entrypoint
│   ├── sender_sw.cpp             # Stop-and-Wait Sender (N=1)
│   ├── receiver_sw.cpp           # Stop-and-Wait Receiver (N=1)
│   ├── sender_gbn.cpp            # Reference Go-Back-N Sender
│   ├── receiver_gbn.cpp          # Reference Go-Back-N Receiver
│   ├── sender_sr.cpp             # Reference Selective Repeat Sender
│   └── receiver_sr.cpp           # Reference Selective Repeat Receiver
├── tests/
│   └── test_emulator.cpp         # Unit test suite (CRC32, PRNG seed determinism)
├── benchmark/
│   └── benchmark_matrix.py       # Automated multi-parameter test runner & SHA-256 verifier
├── analysis/
│   └── plot_benchmarks.py        # Academic-grade Matplotlib plotting suite (300 DPI & PDF)
└── docs/
    └── WEEK5_REPORT.md           # Full technical report, math derivations & viva Q&A
```

---

## Quick Start Guide

### 1. Build the Project with CMake
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
This generates 5 high-resolution figures in `./results/`:
- `fig1_throughput_vs_loss.png` / `.pdf`
- `fig2_retransmission_overhead.png` / `.pdf`
- `fig3_transfer_time_vs_delay.png` / `.pdf`
- `fig4_window_size_scaling.png` / `.pdf`
- `fig5_jitter_reordering.png` / `.pdf`
