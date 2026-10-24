# AI-USE Disclosure Statement
**Course:** CS-30003: Computer Networks Laboratory  
**Project:** Reliable Data Transfer (RDT) over UDP (Proposal P3)  
**Team Repository:** [Aniket17-byte/reliable-data-transfer-udp](https://github.com/Aniket17-byte/reliable-data-transfer-udp)  

---

## Team Compliance Statement

In strict adherence to Section 5 ("Ground rules - Use of AI tools") of the CS-30003 Course Project Guidelines:
1. **Permitted AI Uses:**
   - Scaffolding boilerplate code (CLI parsers, cross-platform socket abstractions).
   - Designing unit test cases, edge case suites, and SHA-256 verification harnesses.
   - Clarifying complex RFC algorithms (RFC 6298 Jacobson/Karels, Karn's algorithm, priority queue schedulers).
   - Python automation for experiment matrix runs and Matplotlib publication-grade plotting.
   - Formatting and structuring technical reports and viva preparation guides.
2. **Core Logic Integrity:**
   - All protocol state machines, sliding window algorithms, retransmission timers, SACK tracking, and channel impairment engines were written, reviewed, tested, and thoroughly mastered by their respective team members.
   - Every team member is prepared to defend their modules and make live code modifications during the oral viva and demo.

---

## 1. Aniket Dwivedy (Roll No: 24051683) — Week 3 & Week 4 Lead
**Role:** Sliding Window Management (GBN & SR), Multi-Timer Manager, Dynamic RTO (Jacobson/Karels RFC 6298), Karn's Algorithm  
**GitHub:** [@Aniket17-byte](https://github.com/Aniket17-byte) | **Email:** 24051683@kiit.ac.in  

| Date / Milestone | Module / Task | Specific AI Assistance Requested | Human Verification & Code Authored |
|---|---|---|---|
| Week 3 - Day 1 | Dynamic RTO (`rtt_estimator.hpp/.cpp`) | Explanation and formula verification of RFC 6298 $(\alpha=0.125, \beta=0.25)$ and Karn's ambiguity rule. | Implemented `RttEstimator` class, sample RTT tracking, exponential timer backoff (`2.0 * RTO`), and edge-case clamping. |
| Week 3 - Day 3 | Retransmission Timers (`timer_manager.hpp/.cpp`) | Boilerplate for std::chrono high-resolution steady clock. | Developed `SingleTimer` (GBN) and `MultiTimerManager` (SR) with per-packet deadline tracking and earliest timeout search. |
| Week 4 - Day 1 | Go-Back-N Window (`gbn_window.hpp/.cpp`) | Logic check on cumulative ACK processing edge cases. | Authored sender sliding window, base advancement, and receiver in-order expected sequence enforcement. |
| Week 4 - Day 3 | Selective Repeat (`sr_window.hpp/.cpp`) | Bitmask and contiguous SACK block calculation strategies. | Built `SrSenderWindow` and `SrReceiverWindow`, out-of-order buffer management, duplicate detection, and SACK block generation. |
| Week 4 - Day 5 | Standalone Unit Tests (`tests/test_*.cpp`) | Generating comprehensive edge case test scenarios. | Wrote full test assertions (`test_rtt`, `test_timers`, `test_gbn`, `test_sr`), fixed zero-ack edge case, verified 100% test pass. |

**Statement of Authorship:**  
I, **Aniket Dwivedy (24051683)**, confirm full understanding of all sliding window invariants, timer queues, and RTO adaptation formulas. I am prepared for live viva modifications.

---

## 2. Ananya Batra (Roll No: 24051681) — Week 5 Lead
**Role:** C++ Channel Emulator, Priority Queue Scheduler, Automated Benchmark Matrix, Matplotlib Plotting Suite  
**GitHub:** [@ananya92-arch](https://github.com/ananya92-arch) | **Email:** anya916212@gmail.com  

| Date / Milestone | Module / Task | Specific AI Assistance Requested | Human Verification & Modifications Made |
|---|---|---|---|
| Week 5 - Day 1 | `channel_emulator.hpp/.cpp` | Guidance on implementing microsecond priority queue with `std::condition_variable` without busy-spinning. | Implemented custom `ScheduledPacket` min-heap comparator, dual forward/reverse socket routing, and bit-mask corruption. |
| Week 5 - Day 2 | `checksum.hpp` | Standard IEEE 802.3 CRC-32 table generator boilerplate. | Integrated header-checksum zeroing logic and validated bit-flip detection accuracy. |
| Week 5 - Day 3 | `benchmark_matrix.py` | Subprocess orchestration boilerplate with timeout handling and SHA-256 digest checks. | Defined exact parameter sweep grid ($P_{\text{loss}}$, delay, jitter, window size, corruption, seed) across SW, GBN, and SR. |
| Week 5 - Day 4 | `plot_benchmarks.py` | Matplotlib academic formatting boilerplate (serif fonts, custom error band shading, dual PNG/PDF exports). | Tuned theoretical curve models, axis limits, aspect ratios, and color palettes for IEEE paper standards. |
| Week 5 - Day 5 | Documentation & Viva Prep | Proofreading mathematical derivations for channel utilization and compiling anticipated viva questions. | Verified all formulas against Kurose-Ross textbook and formulated live-coding demo answers. |

**Statement of Authorship:**  
I, **Ananya Batra (24051681)**, confirm complete understanding of the entire codebase, socket APIs, PRNG determinism guarantees, and mathematical performance differences between SW, GBN, and SR.

---

## 3. Aditya Bajpai (Roll No: 2405704) — Week 1 Lead
**Role:** Core UDP Socket Architecture, Packet Header Format, Checksum, File I/O  
**GitHub:** [@AdityaBajpai07](https://github.com/AdityaBajpai07)  

- Scaffolding of basic UDP socket wrappers, packet serialization/deserialization into 13-byte packed network headers, and file chunking loops.
- All core networking calls and socket binding verified and authored by the student.

---

## 4. Iti Sharma (Roll No: 24052005) — Week 2 Lead
**Role:** Stop-and-Wait ARQ Protocol State Machine, ACK Processing  
**GitHub:** [@iti1503](https://github.com/iti1503)  

- State machine transitions for Alternating Bit Protocol (Stop-and-Wait, $N=1$), single packet timeout retransmission, and duplicate ACK rejection.
- All core protocol state logic authored and defended by the student.
