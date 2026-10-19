# Reliable Data Transfer over UDP (P3)

## Week 1: Baseline Implementation
**Author:** Aditya Bajpai

Cross-platform C++ baseline for reliable file transfer over UDP sockets. Implements core packet structures, network byte-order serialization, 16-bit Internet Checksum verification, and binary file chunking with reconstruction.

---

### 📁 Files Implemented

* **`packet.h`** — Defines the packed 13-byte header structure (`Header`), full packet format (`Packet`), control flags, net-to-host serialization logic, and checksum calculations.
* **`sender_base.cpp`** — Handles cross-platform UDP socket setup, binary file reading in 1024-byte chunks, packet header creation, checksum generation, and transmission.
* **`receiver_base.cpp`** — Binds to port `8080`, listens for incoming UDP packets, validates checksum integrity, and reconstructs the output file.

---

### 📋 Packet Header Format (13 Bytes Packed)

| Field | Type | Size | Description |
| :--- | :--- | :--- | :--- |
| `seq_num` | `uint32_t` | 4 Bytes | Sequence number of the payload byte stream |
| `ack_num` | `uint32_t` | 4 Bytes | Acknowledgement number |
| `checksum` | `uint16_t` | 2 Bytes | 1's complement Internet Checksum |
| `payload_len` | `uint16_t` | 2 Bytes | Size of payload chunk (Max 1024 bytes) |
| `flags` | `uint8_t` | 1 Byte | Packet control flags (`FLAG_DATA`, `FLAG_ACK`, etc.) |

---

### 🚀 How to Run Local Baseline Test

#### 1. Build and Run Receiver
```powershell
g++ receiver_base.cpp -o receiver.exe -lws2_32
.\receiver.exe
