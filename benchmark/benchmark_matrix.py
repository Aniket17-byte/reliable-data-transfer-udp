#!/usr/bin/env python3
"""
CS-30003: Reliable Data Transfer over UDP
Automated Benchmark Test Matrix Orchestrator
Author: Ananya Batra (24051681)
"""

import os
import sys
import time
import json
import csv
import hashlib
import argparse
import subprocess
from pathlib import Path

def generate_payload_file(filepath: Path, size_bytes: int, seed: int = 42) -> str:
    """Generate deterministic pseudo-random binary payload and return its SHA-256."""
    filepath.parent.mkdir(parents=True, exist_ok=True)
    hasher = hashlib.sha256()
    
    state = seed & 0xFFFFFFFF
    chunk_size = 65536
    written = 0
    
    with open(filepath, "wb") as f:
        while written < size_bytes:
            to_write = min(chunk_size, size_bytes - written)
            state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
            block = bytearray((state >> (i % 24)) & 0xFF for i in range(to_write))
            hasher.update(block)
            f.write(block)
            written += to_write
            
    return hasher.hexdigest()

def verify_file_sha256(filepath: Path) -> str:
    """Compute SHA-256 hash of a file."""
    if not filepath.exists():
        return ""
    hasher = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            hasher.update(chunk)
    return hasher.hexdigest()

def find_executable(name: str, bin_dir: Path) -> Path:
    """Locate binary executable across Windows (.exe) and Unix platforms."""
    candidates = [
        bin_dir / name,
        bin_dir / f"{name}.exe",
        bin_dir / "Release" / f"{name}.exe",
        bin_dir / "Debug" / f"{name}.exe"
    ]
    for c in candidates:
        if c.exists() and os.access(c, os.X_OK):
            return c
        if c.exists() and sys.platform.startswith("win"):
            return c
    raise FileNotFoundError(f"Binary '{name}' not found in {bin_dir} or subdirectories.")

def run_single_trial(
    protocol: str,
    bin_dir: Path,
    payload_file: Path,
    expected_sha256: str,
    loss_rate: float,
    delay_ms: int,
    jitter_ms: int,
    corrupt_rate: float,
    reorder_rate: float,
    window_size: int,
    seed: int,
    timeout_sec: float = 60.0
) -> dict:
    """Execute one end-to-end trial: Receiver -> Emulator -> Sender."""
    output_file = payload_file.parent / f"received_{protocol}_{seed}.dat"
    if output_file.exists():
        try: output_file.unlink()
        except OSError: pass

    emu_bin = find_executable("channel_emulator", bin_dir)
    recv_bin = find_executable(f"receiver_{protocol.lower()}", bin_dir)
    send_bin = find_executable(f"sender_{protocol.lower()}", bin_dir)

    sender_out_port = 9000
    emu_sender_in_port = 9001
    emu_receiver_out_port = 9002
    emu_receiver_in_port = 9003
    receiver_listen_port = 9002

    # 1. Launch Receiver
    recv_cmd = [
        str(recv_bin),
        "--output", str(output_file),
        "--port", str(receiver_listen_port),
        "--emu-port", str(emu_receiver_in_port)
    ]
    if protocol.upper() in ["GBN", "SR"]:
        recv_cmd.extend(["--window", str(window_size)])

    recv_proc = subprocess.Popen(recv_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.1)

    # 2. Launch Emulator
    emu_cmd = [
        str(emu_bin),
        "--sender-in", str(emu_sender_in_port),
        "--receiver-out", str(emu_receiver_out_port),
        "--receiver-in", str(emu_receiver_in_port),
        "--sender-out", str(sender_out_port),
        "--loss", str(loss_rate),
        "--delay", str(delay_ms),
        "--jitter", str(jitter_ms),
        "--corrupt", str(corrupt_rate),
        "--reorder", str(reorder_rate),
        "--seed", str(seed)
    ]
    emu_proc = subprocess.Popen(emu_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.15)

    # 3. Launch Sender
    send_cmd = [
        str(send_bin),
        "--file", str(payload_file),
        "--emu-port", str(emu_sender_in_port),
        "--port", str(sender_out_port)
    ]
    if protocol.upper() in ["GBN", "SR"]:
        send_cmd.extend(["--window", str(window_size)])

    trial_result = {
        "protocol": protocol,
        "loss_rate": loss_rate,
        "delay_ms": delay_ms,
        "jitter_ms": jitter_ms,
        "corrupt_rate": corrupt_rate,
        "reorder_rate": reorder_rate,
        "window_size": window_size if protocol.upper() in ["GBN", "SR"] else 1,
        "seed": seed,
        "success": False,
        "integrity_verified": False,
        "error_msg": ""
    }

    send_proc = None
    try:
        send_proc = subprocess.Popen(send_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        stdout, stderr = send_proc.communicate(timeout=timeout_sec)

        json_start = stdout.find("{")
        json_end = stdout.rfind("}")
        if json_start != -1 and json_end != -1:
            metrics = json.loads(stdout[json_start:json_end+1])
            trial_result.update(metrics)
            trial_result["success"] = (metrics.get("status") == "SUCCESS")
        else:
            trial_result["error_msg"] = f"No JSON output from sender. Raw stdout: {stdout[:200]}"

        recv_proc.wait(timeout=5.0)

        actual_sha256 = verify_file_sha256(output_file)
        trial_result["integrity_verified"] = (actual_sha256 == expected_sha256)

    except subprocess.TimeoutExpired:
        trial_result["error_msg"] = f"Trial timed out after {timeout_sec}s"
    finally:
        for proc in (send_proc, recv_proc, emu_proc):
            if proc and proc.poll() is None:
                proc.terminate()
                try: proc.wait(timeout=1.0)
                except subprocess.TimeoutExpired: proc.kill()
        if output_file.exists():
            try: output_file.unlink()
            except OSError: pass

    return trial_result

def run_benchmark_matrix(args):
    bin_dir = Path(args.bin_dir).resolve()
    results_dir = Path(args.results_dir).resolve()
    results_dir.mkdir(parents=True, exist_ok=True)

    payload_size = int(args.size_mb * 1024 * 1024)
    payload_file = results_dir / f"test_payload_{args.size_mb}mb.bin"
    print(f"[*] Generating {args.size_mb} MB test payload ({payload_size} bytes)...")
    expected_sha256 = generate_payload_file(payload_file, payload_size, seed=1337)
    print(f"[+] Payload SHA-256: {expected_sha256}")

    if args.quick:
        protocols = ["SW", "GBN", "SR"]
        loss_rates = [0.0, 0.05, 0.10]
        delays = [20]
        windows = [16]
        seeds = [42]
    else:
        protocols = ["SW", "GBN", "SR"]
        loss_rates = [0.0, 0.01, 0.03, 0.05, 0.08, 0.10, 0.15, 0.20]
        delays = [10, 30, 50, 100]
        windows = [4, 8, 16, 32, 64]
        seeds = [42, 101, 2026]

    results = []
    trial_idx = 0

    print("\n--- Running Experiment 1: Packet Loss Sweep (SW vs GBN vs SR) ---")
    for proto in protocols:
        for loss in loss_rates:
            for s in seeds:
                trial_idx += 1
                w = 16 if proto in ["GBN", "SR"] else 1
                print(f"[{trial_idx}] {proto:3s} | Loss: {loss*100:4.1f}% | Latency: 30ms | Window: {w:2d} | Seed: {s} ... ", end="", flush=True)
                res = run_single_trial(
                    protocol=proto,
                    bin_dir=bin_dir,
                    payload_file=payload_file,
                    expected_sha256=expected_sha256,
                    loss_rate=loss,
                    delay_ms=30,
                    jitter_ms=5,
                    corrupt_rate=0.0,
                    reorder_rate=0.0,
                    window_size=w,
                    seed=s,
                    timeout_sec=args.timeout
                )
                status = "OK" if res["integrity_verified"] else f"FAIL ({res.get('error_msg')})"
                throughput = res.get("throughput_mbps", 0.0)
                retrans = res.get("retransmissions", 0)
                print(f"{status} | Throughput: {throughput:6.2f} Mbps | Retrans: {retrans}")
                results.append(res)

    print("\n--- Running Experiment 2: Propagation Delay Sweep ---")
    for proto in protocols:
        for d in (delays if not args.quick else [20, 50]):
            for s in seeds:
                trial_idx += 1
                w = 16 if proto in ["GBN", "SR"] else 1
                print(f"[{trial_idx}] {proto:3s} | Loss: 2.0% | Latency: {d}ms | Window: {w:2d} | Seed: {s} ... ", end="", flush=True)
                res = run_single_trial(
                    protocol=proto,
                    bin_dir=bin_dir,
                    payload_file=payload_file,
                    expected_sha256=expected_sha256,
                    loss_rate=0.02,
                    delay_ms=d,
                    jitter_ms=5,
                    corrupt_rate=0.0,
                    reorder_rate=0.0,
                    window_size=w,
                    seed=s,
                    timeout_sec=args.timeout
                )
                print(f"OK | Throughput: {res.get('throughput_mbps', 0.0):6.2f} Mbps")
                results.append(res)

    print("\n--- Running Experiment 3: Window Size Sweep (GBN vs SR) ---")
    for proto in ["GBN", "SR"]:
        for w in (windows if not args.quick else [4, 16]):
            for s in seeds:
                trial_idx += 1
                print(f"[{trial_idx}] {proto:3s} | Loss: 5.0% | Latency: 50ms | Window: {w:2d} | Seed: {s} ... ", end="", flush=True)
                res = run_single_trial(
                    protocol=proto,
                    bin_dir=bin_dir,
                    payload_file=payload_file,
                    expected_sha256=expected_sha256,
                    loss_rate=0.05,
                    delay_ms=50,
                    jitter_ms=5,
                    corrupt_rate=0.0,
                    reorder_rate=0.0,
                    window_size=w,
                    seed=s,
                    timeout_sec=args.timeout
                )
                print(f"OK | Throughput: {res.get('throughput_mbps', 0.0):6.2f} Mbps")
                results.append(res)

    # Save outputs
    json_path = results_dir / "benchmark_results.json"
    with open(json_path, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\n[+] Saved complete results to {json_path}")

    csv_path = results_dir / "benchmark_results.csv"
    if results:
        fieldnames = list(results[0].keys())
        with open(csv_path, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(results)
        print(f"[+] Saved CSV summary to {csv_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="CS-30003 Automated Benchmark Matrix")
    parser.add_argument("--bin-dir", default="./build", help="Path to compiled C++ executables")
    parser.add_argument("--results-dir", default="./results", help="Path to store output figures and data")
    parser.add_argument("--size-mb", type=float, default=2.0, help="Payload size in megabytes")
    parser.add_argument("--timeout", type=float, default=90.0, help="Per-trial timeout in seconds")
    parser.add_argument("--quick", action="store_true", help="Run rapid smoke test matrix subset")
    args = parser.parse_args()

    run_benchmark_matrix(args)
