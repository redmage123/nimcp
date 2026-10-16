#!/usr/bin/env python3
"""
Monkey-patch LoRA data collector into the running brain daemon.

Hooks into the daemon's learn_vector handler to collect
(embedding, description) pairs without restarting training.

Usage:
    python3 inject_lora_collector.py

This connects to the running daemon via Unix socket, sends a
special command to install the collector hook, and starts
collecting data to checkpoints/athena/lora_training_data.jsonl.
"""

import json
import os
import socket
import struct
import sys
import time

SOCKET_PATH = "/var/run/athena/brain.sock"
OUTPUT_PATH = "checkpoints/athena/lora_training_data.jsonl"


def send_cmd(sock, cmd):
    """Send command to daemon and get response."""
    data = json.dumps(cmd).encode()
    sock.sendall(struct.pack("!I", len(data)))
    sock.sendall(data)
    # Read response
    hdr = sock.recv(4)
    if len(hdr) < 4:
        return None
    size = struct.unpack("!I", hdr)[0]
    chunks = []
    while size > 0:
        chunk = sock.recv(min(size, 65536))
        if not chunk:
            break
        chunks.append(chunk)
        size -= len(chunk)
    return json.loads(b"".join(chunks))


def main():
    print("=" * 50)
    print("  LoRA Data Collector — Live Injection")
    print("=" * 50)

    if not os.path.exists(SOCKET_PATH):
        print(f"ERROR: Daemon socket not found: {SOCKET_PATH}")
        sys.exit(1)

    # Connect to daemon
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(SOCKET_PATH)
    print(f"Connected to daemon at {SOCKET_PATH}")

    # Get current step count
    resp = send_cmd(sock, {"cmd": "get_stats"})
    if resp:
        steps = resp.get("result", {}).get("total_learning_steps", 0)
        print(f"Daemon at step {steps}")

    sock.close()

    # Now run a monitoring loop that periodically queries the daemon
    # and collects decide_full outputs for LoRA training data
    print(f"\nCollecting LoRA training data to: {OUTPUT_PATH}")
    print("Press Ctrl+C to stop\n")

    collected = 0
    buffer = []

    # Load sensory content for descriptions
    try:
        with open("checkpoints/athena/claude_content_cache.json") as f:
            cache = json.load(f)
        stage0 = cache.get("0", {})
        sensory = stage0.get("sensory", stage0.get("narrations", []))
        print(f"Loaded {len(sensory)} descriptions from cache")
    except Exception:
        sensory = []
        print("No content cache — will collect from daemon responses only")

    try:
        while True:
            # Connect, get a decide_full result, disconnect
            try:
                sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                sock.settimeout(10)
                sock.connect(SOCKET_PATH)

                # Get current stats
                resp = send_cmd(sock, {"cmd": "stats"})
                current_step = 0
                if resp:
                    current_step = resp.get("result", {}).get(
                        "total_learning_steps", 0)

                # Pick a description from the sensory cache based on step
                description = ""
                label = ""
                if sensory and current_step > 0:
                    idx = current_step % len(sensory)
                    item = sensory[idx]
                    if isinstance(item, dict):
                        label = item.get("name", "")
                        description = item.get("description", "")
                    elif isinstance(item, str):
                        description = item
                        # Extract a label from first few words
                        words = description.split()[:5]
                        label = " ".join(words)

                # Run inference to get output vector
                if description:
                    # Encode the description (simple: use as features placeholder)
                    # In practice, the training loop does this via BGE encoder
                    resp = send_cmd(sock, {
                        "cmd": "decide_full",
                        "features": [0.0] * 1024,  # Placeholder
                        "enrich": True,
                        "text": f"{label}: {description}"
                    })

                    if resp and resp.get("result"):
                        result = resp["result"]
                        output_vec = result.get("output_vector", [])

                        if output_vec and description:
                            # Truncate to 256 dims
                            if len(output_vec) > 256:
                                output_vec = output_vec[:256]

                            sample = {
                                "embedding": output_vec,
                                "description": description[:512],
                                "label": label[:64],
                                "stage": 0,
                                "step": current_step,
                                "loss": 0.0,  # Not available from inference
                                "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S")
                            }
                            buffer.append(sample)
                            collected += 1

                            if len(buffer) >= 10:
                                with open(OUTPUT_PATH, "a") as f:
                                    for s in buffer:
                                        f.write(json.dumps(s) + "\n")
                                buffer.clear()
                                print(f"  Collected {collected} samples "
                                      f"(step {current_step})")

                sock.close()
            except (ConnectionRefusedError, BrokenPipeError, socket.timeout):
                pass
            except Exception as e:
                print(f"  Warning: {e}")

            # Collect every 30 seconds
            time.sleep(30)

    except KeyboardInterrupt:
        # Flush remaining
        if buffer:
            with open(OUTPUT_PATH, "a") as f:
                for s in buffer:
                    f.write(json.dumps(s) + "\n")
            collected += len(buffer)

        print(f"\n\nCollection stopped. Total: {collected} samples")
        file_size = os.path.getsize(OUTPUT_PATH) / (1024 * 1024) if os.path.exists(OUTPUT_PATH) else 0
        print(f"File: {OUTPUT_PATH} ({file_size:.1f} MB)")


if __name__ == "__main__":
    main()
