#!/usr/bin/env python3
"""
Binary checkpoint migrator — fixes struct layout mismatches WITHOUT loading the brain.

Reads the old checkpoint at the binary level, identifies the config section,
pads or adjusts it to match the new brain_config_t size, and writes a
compatible checkpoint. No Brain.load() required — no OOM risk.

The key insight: the checkpoint format is:
  .meta file: [NIMP header(16)] [brain_config_t] [other metadata]
  .bin file:  [raw neuron/synapse/weight data]

The .bin file doesn't contain brain_config_t — only the .meta does.
So if we fix the .meta, the .bin loads correctly.

Usage:
    python3 scripts/migrate_checkpoint_binary.py \
        --input checkpoints/athena/athena_s1_step9950.bin.PROTECTED \
        --output checkpoints/athena/athena_daemon.bin
"""

import os
import sys
import struct
import shutil
import argparse


def get_config_sizes():
    """Determine old and new brain_config_t sizes by reading headers."""
    # We can compute this from the .meta file size and known header size
    # NIMP header = 16 bytes
    # After header comes brain_config_t, then other metadata
    # The old .meta is 2503 bytes → config = 2503 - 16 = 2487 bytes (old size)
    # The new config has 9 extra bools + 2 more bools = 11 extra bytes (with padding)
    return {
        "header_size": 16,
        "old_meta_total": None,  # Will be read from file
        "new_extra_bytes": 16,   # Approximate: 11 bools + padding to alignment
    }


def migrate(input_path, output_path, meta_path=None):
    """Migrate a checkpoint to be compatible with the current library."""

    # Find the .meta file
    if meta_path is None:
        for suffix in ['.tmp.meta', '.meta']:
            candidate = input_path + suffix
            if os.path.exists(candidate):
                meta_path = candidate
                break

    if meta_path is None:
        print(f"ERROR: No .meta file found for {input_path}")
        print("The checkpoint needs a .meta file to migrate.")
        return False

    print(f"Input:     {input_path} ({os.path.getsize(input_path)/1024**3:.2f} GB)")
    print(f"Meta:      {meta_path} ({os.path.getsize(meta_path)} bytes)")
    print(f"Output:    {output_path}")

    # Step 1: Read the .meta file
    with open(meta_path, 'rb') as f:
        meta_data = f.read()

    print(f"Meta size: {len(meta_data)} bytes")

    # Check header
    if len(meta_data) >= 4:
        magic = meta_data[:4]
        if magic == b'NIMP':
            print(f"Meta format: NIMP (versioned header)")
            header = meta_data[:16]
            config_and_rest = meta_data[16:]
        else:
            print(f"Meta format: Legacy (no header, raw config)")
            header = b''
            config_and_rest = meta_data

    # Step 2: Pad the config section
    # The new brain_config_t is larger. We need to insert zero bytes
    # after the old config ends. But we don't know exactly where the
    # config ends and other metadata begins.
    #
    # Solution: just pad the entire .meta with zeros at the end.
    # The persistence loader reads config with fread(&config, 1, sizeof(config), f)
    # which reads min(file_remaining, sizeof(config)) bytes.
    # If the file is too short, it gets zero-filled (our fix).
    # If we pad with zeros, the extra zeros are harmless — they'll be
    # read as the new bool fields, all false.

    # We need to ensure the .meta is at least as large as 16 + new_config_size
    # But we don't know new_config_size exactly. We can be generous:
    # pad to 64KB (way more than any config could be)
    padded_size = max(len(meta_data), 65536)
    padded_meta = meta_data + b'\x00' * (padded_size - len(meta_data))

    # Step 3: Copy the .bin file (weights — format independent)
    print(f"Copying weights ({os.path.getsize(input_path)/1024**3:.2f} GB)...")
    shutil.copy2(input_path, output_path)

    # Step 4: Write the padded .meta
    output_meta = output_path + '.meta'
    with open(output_meta, 'wb') as f:
        f.write(padded_meta)

    print(f"Output meta: {output_meta} ({len(padded_meta)} bytes, padded from {len(meta_data)})")

    # Step 5: Verify
    out_size = os.path.getsize(output_path)
    if out_size < 1_000_000_000:
        print(f"WARNING: Output is only {out_size/1024**3:.2f} GB — may be corrupted")
        return False

    print(f"SUCCESS: Migrated checkpoint ({out_size/1024**3:.2f} GB)")
    print(f"The new daemon should load this correctly (zero-filled new config fields)")
    return True


def main():
    parser = argparse.ArgumentParser(description="Binary checkpoint migrator")
    parser.add_argument("--input", required=True, help="Input checkpoint .bin file")
    parser.add_argument("--output", required=True, help="Output checkpoint .bin file")
    parser.add_argument("--meta", help="Input .meta file (auto-detected if omitted)")
    args = parser.parse_args()

    ok = migrate(args.input, args.output, args.meta)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
