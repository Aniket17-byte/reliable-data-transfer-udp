Week-1(By Aditya Bajpai)
# Reliable Data Transfer over UDP (P3) - Baseline Implementation

Cross-platform C++ foundational baseline for reliable file transfer over UDP sockets. Handles packet structure, network byte-order serialization, 16-bit Internet Checksum verification, and binary file chunking/assembly.

---

## 📁 Files Implemented

* **`packet.h`** — Defines the packed 13-byte header structure (`Header`), overall packet structure (`Packet`), packet flag macros (`FLAG_DATA`, `FLAG_ACK`, etc.), net-to-host serialization logic, and 16-bit checksum calculation.
* **`sender_base.cpp`** — Handles cross-platform UDP socket initialization, binary file reading in 1024-byte chunks, packet header creation, checksum calculation, and socket transmission.
* **`receiver_base.cpp`** — Binds to port `8080`, listens for incoming UDP packets, verifies the checksum for data corruption, and reconstructs the received binary payload directly into an output file.

---

## 📋 Packet Header Format (13 Bytes Packed)

| Field | Type | Size | Description |
| :--- | :--- | :--- | :--- |
| `seq_num` | `uint32_t` | 4 Bytes | Sequence number of the payload byte stream |
| `ack_num` | `uint32_t` | 4 Bytes | Acknowledgement number |
| `checksum` | `uint16_t` | 2 Bytes | 1's complement Internet Checksum |
| `payload_len` | `uint16_t` | 2 Bytes | Size of current payload chunk (Max 1024 bytes) |
| `flags` | `uint8_t` | 1 Byte | Packet control flags (`FLAG_DATA`, `FLAG_ACK`, `FLAG_SYN`, `FLAG_FIN`) |

---

## How to Run Local Baseline Test

Build and Run Receiver
# On Windows (MinGW/PowerShell)
g++ receiver_base.cpp -o receiver.exe -lws2_32
.\receiver.exe

Build and Run Sender
# On Windows (MinGW/PowerShell)
g++ sender_base.cpp -o sender.exe -lws2_32
.\sender.exe
