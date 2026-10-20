# AI Assistance Disclosure Log (AI-USE.md)

In accordance with the course ground rules for CS-30003 Coding Assignment 1, this document transparently records all AI assistance utilized during the development of this project.

---

## Log Entry: Sliding Window, Timers, and Adaptive RTO Modules

- **Team Member**: Aniket Dwivedy (Roll No: 24041683)
- **Role & Deliverables**: Sliding Window Management (GBN & Selective Repeat), SACK State Tracking, Retransmission Timers, Dynamic RTT Measurement (Jacobson/Karels), Karn's Algorithm.
- **Date**: October 2026
- **AI Tool Used**: Antigravity Assistant (Google DeepMind)

### 1. Permitted Use Areas Addressed
- **Concept Explanation & Mathematical Formulas**:
  - RFC 6298 Jacobson/Karels equations for Smoothed RTT ($SRTT$) and RTT Variation ($RTTVAR$).
  - Karn's algorithm rules for retransmission ambiguity and exponential backoff ($RTO \times 2$).
- **Boilerplate & Interface Design**:
  - Modular C++ header file skeletons (`rtt_estimator.hpp`, `timer_manager.hpp`, `gbn_window.hpp`, `sr_window.hpp`) to cleanly decouple window logic from raw UDP sockets.
- **Test Generation & Verification Harness**:
  - Standalone unit tests (`tests/test_rtt.cpp`, `tests/test_timers.cpp`, `tests/test_gbn.cpp`, `tests/test_sr.cpp`, `tests/test_main.cpp`) to verify protocol edge cases (retransmission backoff, cumulative ACK sliding, out-of-order buffering, SACK blocks) without requiring network sockets.
- **Viva Defense Preparation**:
  - Detailed examination questions, live code modification scenarios, and edge-case behavior explanations (`docs/viva_prep_guide.md`).

### 2. Implementation Integrity & Understanding
All algorithmic logic, state transitions, boundary conditions (such as $next\_seq\_num \ge base + N$, receiver duplicate ACK generation for past windows $[rcv\_base - N, rcv\_base - 1]$, and SACK bitmask tracking) were reviewed, traced, and understood in full detail by the author to support live modification during the viva demo.
