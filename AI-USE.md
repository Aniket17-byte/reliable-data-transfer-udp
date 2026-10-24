# AI-USE Disclosure Statement
**Project:** CS-30003: Reliable Data Transfer over UDP (Project P3)  
**Student Name:** Ananya Batra (Roll No: 24051681)  
**GitHub Profile:** [ananya92-arch](https://github.com/ananya92-arch)  
**Email:** anya916212@gmail.com  
**Role:** Week 5 Lead (Channel Emulator, Benchmarking Harness, Plotting Suite)  
**Date:** October 2026  

---

## 1. Compliance with Course Ground Rules

In strict adherence to Section 5 ("Ground rules - Use of AI tools") of the CS-30003 Project Proposal guidelines:
* **Permitted Uses Utilized:** 
  1. Explaining networking concepts (asynchronous timer queues, condition variable wait loops, Karn's algorithm).
  2. Generating test cases and cryptographic SHA-256 verification harness.
  3. Scaffolding boilerplate for CLI argument parsing and cross-platform socket headers.
  4. Authoring Python automation scripts and Matplotlib publication-grade plotting templates.
  5. Improving technical report readability and documentation clarity.
* **Core Protocol Logic Integrity:** 
  - The channel emulator impairment math, priority queue scheduling logic, socket state machines, and mathematical derivations were thoroughly reviewed, validated, and mastered by the author.
  - The student is prepared for live code modifications and architectural questioning during the project viva/demo.

---

## 2. Granular Log of AI Tool Interactions

| Date / Milestone | Module / Task | Specific AI Assistance Requested | Human Verification & Modifications Made |
|---|---|---|---|
| Week 5 - Day 1 | `channel_emulator.hpp` & `.cpp` | Guidance on implementing a microsecond-accurate priority queue with `std::condition_variable::wait_until()` without busy-spinning. | Implemented custom `ScheduledPacket` min-heap comparator, dual forward/reverse socket routing, and bit-mask corruption. |
| Week 5 - Day 2 | `checksum.hpp` | Standard IEEE 802.3 CRC-32 table generator boilerplate. | Integrated header-checksum zeroing logic and validated bit-flip detection accuracy. |
| Week 5 - Day 3 | `benchmark_matrix.py` | Subprocess orchestration boilerplate with timeout handling and SHA-256 digest checks. | Defined exact parameter sweep grid ($P_{\text{loss}}$, delay, jitter, window size, corruption, seed) across SW, GBN, and SR. |
| Week 5 - Day 4 | `plot_benchmarks.py` | Matplotlib academic formatting boilerplate (serif fonts, custom error band shading, dual PNG/PDF exports). | Tuned theoretical curve models, axis limits, aspect ratios, and color palettes for IEEE paper standards. |
| Week 5 - Day 5 | Documentation & Viva Prep | Proofreading mathematical derivations for channel utilization and compiling anticipated viva questions. | Verified all formulas against Kurose-Ross textbook and formulated live-coding demo answers. |

---

## 3. Student Statement of Authorship

I, **Ananya Batra (24051681)**, confirm that this disclosure represents an honest, complete record of AI assistance. I have complete understanding of the entire codebase, the socket programming APIs used, the PRNG determinism guarantees, and the mathematical performance differences between Stop-and-Wait, Go-Back-N, and Selective Repeat. I am fully prepared to defend this work and perform live code alterations during the viva.

*Signature:* Ananya Batra  
*Date:* October 24, 2026
