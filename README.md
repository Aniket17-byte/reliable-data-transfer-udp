# CS-30003: Reliable Data Transfer (RDT) over UDP
**Course:** CS-30003 (Computer Networks Laboratory)  
**Project Proposal:** P3 · Reliable Data Transfer over UDP | **Path:** A (Catalogue)  
**Repository:** [https://github.com/Aniket17-byte/reliable-data-transfer-udp](https://github.com/Aniket17-byte/reliable-data-transfer-udp)

---

## 1. Project Overview & What We Built
This project implements a high-performance reliable file transfer application over UDP that guarantees in-order, error-free delivery over lossy, delayed, and reordered networks. We implement and evaluate three classic Automatic Repeat reQuest (ARQ) protocols:
- **Stop-and-Wait (SW)** — Alternating Bit Protocol ($N = 1$)
- **Go-Back-N (GBN)** — Cumulative sliding window with bulk recovery & Fast Retransmit
- **Selective Repeat (SR)** — Independent per-packet buffering, selective timers, and SACK tracking

A standalone **C++ Channel Emulator** sits between sender and receiver instances, introducing deterministic packet loss, bit corruption, latency delay, jitter, and packet reordering using a seeded pseudo-random number generator (PRNG).

---

## 2. Core Deliverables Checklist

- [x] **UDP-based reliable file transfer application** (Native C++ socket API, POSIX/Winsock abstraction)
- [x] **Three ARQ Protocols:** Stop-and-Wait, Go-Back-N, and Selective Repeat
- [x] **Sequence numbering & validation logic** (Header packaging and in-order enforcement)
- [x] **Error detection:** IEEE 802.3 CRC-32 checksum calculation and verification
- [x] **Acknowledgements:** Cumulative ACKs for GBN and Selective ACKs (SACK) for SR
- [x] **Retransmission timers:** SingleTimer for GBN and MultiTimerManager for SR (`std::chrono` steady clock)
- [x] **Dynamic RTT & Adaptive RTO:** Jacobson/Karels RFC 6298 algorithm ($\alpha = 0.125, \beta = 0.25$)
- [x] **Karn's algorithm:** Ambiguity elimination (no RTT samples on retransmitted packets) + exponential backoff
- [x] **C++ Channel Emulator:** Priority queue scheduler simulating loss, delay, jitter, reorder, corrupt, duplicate
- [x] **Deterministic testing:** Seed-based PRNG reproducibility (`--seed <val>`)
- [x] **Cryptographic file integrity verification:** End-to-end SHA-256 digest validation
- [x] **Performance analysis & plotting suite:** Automated benchmark matrix and academic Matplotlib figures

---

## 3. The Claim Tested (Scientific Hypothesis)

> **Hypothesis:** *Selective Repeat will achieve significantly higher goodput and require fewer unnecessary retransmissions than Go-Back-N and Stop-and-Wait as packet loss rate and network reordering increase.*

We validated this hypothesis across systematic sweeps of packet loss ($0\%\dots20\%$), propagation delay ($10\text{ms}\dots200\text{ms}$), and window sizes ($N \in \{4, 8, 16, 32, 64\}$).

---

## 4. Team Members & Work Split

| Member | Roll Number | GitHub Profile | Responsibilities as per Proposal |
|:---|:---:|:---:|:---|
| **Aditya Bajpai** | 2405704 | [@AdityaBajpai07](https://github.com/AdityaBajpai07) | Core UDP socket architecture, sender/receiver application flow, packet format design, sequence numbering, checksum computation, file I/O operations. |
| **Iti Sharma** | 24052005 | [@iti1503](https://github.com/iti1503) | Protocol state machine design, Stop-and-Wait ARQ, basic ACK processing, packet integrity verification logic, protocol comparative analysis, report synthesis. |
| **Aniket Dwivedy** | 24051683 | [@Aniket17-byte](https://github.com/Aniket17-byte) | Sliding window management (Go-Back-N, Selective Repeat), selective ACK (SACK) state tracking, retransmission timers, dynamic RTT measurement, adaptive RTO (Jacobson/Karels), and Karn's algorithm. |
| **Ananya Batra** | 24051681 | [@ananya92-arch](https://github.com/ananya92-arch) | Channel emulator engine (loss, duplication, reordering, delay, jitter), seed-based impairment logic, automated test matrix pipeline, performance benchmarking, plotting scripts. |

---

## 5. Technology Stack & API Justification

- **Core Implementation:** Native C++17 (`std::thread`, `std::chrono`, `std::priority_queue`, standard Berkeley/Winsock sockets).
  - *Justification:* Precise control over memory packing, sub-millisecond timer resolution, non-blocking socket I/O, zero-overhead bit manipulation.
- **Verification & Analysis:** Python 3 (`numpy`, `matplotlib`, `hashlib`, `subprocess`).
  - *Justification:* Automated orchestration of multi-process trials, SHA-256 hash checks, and 300 DPI vector PDF plotting.

---

## 6. Repository Structure

```
reliable-data-transfer-udp/
├── CMakeLists.txt                # Unified CMake build configuration
├── README.md                     # Complete project documentation & guide
├── AI-USE.md                     # Mandatory course AI disclosure statement
├── include/                      # C++ Header declarations
│   ├── packet.h / packet.hpp     # Packet header layout (16-byte packed) & serialization
│   ├── checksum.hpp              # IEEE 802.3 CRC-32 implementation
│   ├── rtt_estimator.hpp         # RFC 6298 Jacobson/Karels dynamic RTO (Aniket)
│   ├── timer_manager.hpp         # High-resolution Single & Multi-Timer queues (Aniket)
│   ├── gbn_window.hpp            # Go-Back-N sliding window definitions (Aniket)
│   ├── sr_window.hpp             # Selective Repeat sender/receiver & SACK tracking (Aniket)
│   ├── channel_emulator.hpp      # Channel Emulator core class & priority queue scheduler (Ananya)
│   ├── adaptive_rto.hpp          # Adaptive RTO helper class
│   └── socket_utils.hpp          # Cross-platform socket abstraction (Windows/Linux)
├── src/                          # C++ Source implementations
│   ├── rtt_estimator.cpp         # Dynamic RTO math & Karn's algorithm (Aniket)
│   ├── timer_manager.cpp         # Steady-clock timer managers (Aniket)
│   ├── gbn_window.cpp            # GBN cumulative window logic (Aniket)
│   ├── sr_window.cpp             # SR sliding window & buffer management (Aniket)
│   ├── channel_emulator.cpp      # Priority queue impairment engine (Ananya)
│   ├── main_emulator.cpp         # Emulator CLI entrypoint (Ananya)
│   ├── sender_sw.cpp             # Stop-and-Wait Sender (Iti)
│   ├── receiver_sw.cpp           # Stop-and-Wait Receiver (Iti)
│   ├── sender_gbn.cpp            # Go-Back-N Sender with Fast Retransmit
│   ├── receiver_gbn.cpp          # Go-Back-N Receiver with in-order ACK logic
│   ├── sender_sr.cpp             # Selective Repeat Sender
│   └── receiver_sr.cpp           # Selective Repeat Receiver with out-of-order buffering
├── tests/                        # Standalone Unit Test Suites
│   ├── test_main.cpp             # Master unit verification runner (Aniket)
│   ├── test_rtt.cpp              # Jacobson/Karels & Karn's algorithm unit tests
│   ├── test_timers.cpp           # SingleTimer & MultiTimerManager lifecycle tests
│   ├── test_gbn.cpp              # GBN sliding window edge case tests
│   ├── test_sr.cpp               # Selective Repeat & SACK unit tests
│   └── test_emulator.cpp         # Channel Emulator CRC32 & PRNG determinism tests
├── benchmark/
│   └── benchmark_matrix.py       # Automated test matrix runner & SHA-256 verifier
├── analysis/
│   └── plot_benchmarks.py        # Publication-grade Matplotlib plotting suite (300 DPI PNG & PDF)
├── results/                      # Generated CSV, JSON benchmarks and publication figures
└── docs/
    ├── WEEK5_REPORT.md           # Technical milestone report, math proofs & derivations
    └── viva_prep_guide.md        # Comprehensive viva defense & live code alteration guide
```

---

## 7. How to Build & Run

### A. Run Standalone Unit Tests
```powershell
# 1. Aniket's Core ARQ & Timer Unit Tests
g++ -std=c++17 -Iinclude src/gbn_window.cpp src/rtt_estimator.cpp src/sr_window.cpp src/timer_manager.cpp tests/test_gbn.cpp tests/test_main.cpp tests/test_rtt.cpp tests/test_sr.cpp tests/test_timers.cpp -o run_tests.exe
.\run_tests.exe

# 2. Ananya's Channel Emulator Unit Tests
g++ -std=c++17 -Iinclude tests/test_emulator.cpp src/channel_emulator.cpp -o test_emulator.exe -lws2_32
.\test_emulator.exe
```

### B. Compile All Executables for Benchmarks
```powershell
New-Item -ItemType Directory -Force -Path build

# Compile Emulator, SW, GBN, and SR executables
g++ -std=c++17 -O2 -Iinclude src/main_emulator.cpp src/channel_emulator.cpp -o build/channel_emulator.exe -lws2_32
g++ -std=c++17 -O2 -Iinclude src/sender_sw.cpp -o build/sender_sw.exe -lws2_32
g++ -std=c++17 -O2 -Iinclude src/receiver_sw.cpp -o build/receiver_sw.exe -lws2_32
g++ -std=c++17 -O2 -Iinclude src/sender_gbn.cpp -o build/sender_gbn.exe -lws2_32
g++ -std=c++17 -O2 -Iinclude src/receiver_gbn.cpp -o build/receiver_gbn.exe -lws2_32
g++ -std=c++17 -O2 -Iinclude src/sender_sr.cpp -o build/sender_sr.exe -lws2_32
g++ -std=c++17 -O2 -Iinclude src/receiver_sr.cpp -o build/receiver_sr.exe -lws2_32
```

### C. Run the Automated Benchmark Suite & Plot Figures
```powershell
# Run the 19-trial automated matrix (SW vs GBN vs SR) with SHA-256 verification
python benchmark/benchmark_matrix.py --quick --size-mb 0.2

# Generate the 5 publication figures into ./results/
python analysis/plot_benchmarks.py ./results
```

Generated plots in `results/`:
- `fig1_throughput_vs_loss.png` / `.pdf` — Goodput comparison under increasing packet loss ($0\%\dots20\%$)
- `fig2_retransmission_overhead.png` / `.pdf` — Total retransmissions across protocols
- `fig3_transfer_time_vs_delay.png` / `.pdf` — Transfer latency under increasing propagation delay
- `fig4_window_size_scaling.png` / `.pdf` — Window size scaling ($N \in \{4, 8, 16, 32, 64\}$)
- `fig5_jitter_reordering.png` / `.pdf` — Resilience against delay jitter and out-of-order packets
