#!/usr/bin/env python3
"""
Monkey-patch hyperparameter setters into the running brain daemon.

Since we can't restart the daemon, this script:
1. Monitors /tmp/athena_* override files
2. Applies them by calling brain methods via the daemon socket
3. For params that can't be set via brain API, writes them to a shared
   state file that the training script reads

Works alongside the hyperparameter_auto_tuner.py.

Usage:
    python3 inject_hp_setters.py
"""

import json
import os
import socket
import struct
import sys
import time

SOCKET_PATH = "/var/run/athena/brain.sock"

HP_FILES = {
    'lr':              '/tmp/athena_lr',
    'sparsity':        '/tmp/athena_sparsity',
    'diversity_weight': '/tmp/athena_diversity_weight',
    'grad_clip':       '/tmp/athena_grad_clip',
    'weight_decay':    '/tmp/athena_weight_decay',
    'output_lr_boost': '/tmp/athena_output_lr_boost',
    'dropout':         '/tmp/athena_dropout',
    'temperature':     '/tmp/athena_temperature',
}

# Shared state file — training script reads this every step
HP_STATE_FILE = "/tmp/athena_hp_state.json"


def send_cmd(cmd):
    """Send command to daemon."""
    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect(SOCKET_PATH)
        data = json.dumps(cmd).encode()
        sock.sendall(struct.pack("!I", len(data)))
        sock.sendall(data)
        hdr = sock.recv(4)
        if len(hdr) < 4:
            sock.close()
            return None
        size = struct.unpack("!I", hdr)[0]
        chunks = []
        while size > 0:
            chunk = sock.recv(min(size, 65536))
            if not chunk:
                break
            chunks.append(chunk)
            size -= len(chunk)
        sock.close()
        return json.loads(b"".join(chunks))
    except Exception:
        return None


def apply_hp(param, value):
    """Apply a hyperparameter change.

    LR: handled by training script's existing /tmp/athena_lr reader
    Sparsity: try set_network_ablation via daemon
    Others: write to shared state file for training script to pick up
    """
    if param == 'lr':
        # LR is already handled by the training script directly
        # Just leave the file for it to read
        return True

    if param == 'sparsity':
        # Try via daemon command
        resp = send_cmd({
            "cmd": "set_network_ablation",
            "sparsity_target": value
        })
        if resp and resp.get("ok"):
            return True
        # Fall through to state file

    # Write all params to shared state file
    state = {}
    if os.path.exists(HP_STATE_FILE):
        try:
            with open(HP_STATE_FILE) as f:
                state = json.load(f)
        except Exception:
            pass

    state[param] = value
    state['_updated_at'] = time.time()

    try:
        with open(HP_STATE_FILE, 'w') as f:
            json.dump(state, f)
        return True
    except Exception:
        return False


def main():
    print("=" * 50)
    print("  HP Setter Injector — Live Patching")
    print("=" * 50)
    print(f"  Monitoring: {', '.join(HP_FILES.keys())}")
    print(f"  State file: {HP_STATE_FILE}")
    print(f"  Interval: 10s")
    print()

    try:
        while True:
            changed = False
            for param, path in HP_FILES.items():
                if param == 'lr':
                    continue  # LR handled by training script directly

                if not os.path.exists(path):
                    continue

                try:
                    with open(path) as f:
                        value = float(f.read().strip())
                    # Don't remove — let the training script also read it
                    # But DO apply immediately
                    if apply_hp(param, value):
                        print(f"  [{time.strftime('%H:%M:%S')}] Applied {param} = {value:.6f}")
                        changed = True
                    # Now remove since we applied it
                    os.remove(path)
                except (ValueError, OSError):
                    try:
                        os.remove(path)
                    except OSError:
                        pass

            if not changed:
                # Check state file age — if stale (>5min), clean up
                if os.path.exists(HP_STATE_FILE):
                    try:
                        with open(HP_STATE_FILE) as f:
                            state = json.load(f)
                        age = time.time() - state.get('_updated_at', 0)
                        if age > 300:
                            pass  # Keep state, it's still valid
                    except Exception:
                        pass

            time.sleep(10)

    except KeyboardInterrupt:
        print("\nHP injector stopped.")


if __name__ == "__main__":
    main()
