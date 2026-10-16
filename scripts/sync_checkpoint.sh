#!/bin/bash
# Sync checkpoint from RunPod to Hetzner (compressed)
# Usage: ./sync_checkpoint.sh [checkpoint_path]
#
# Compresses with zstd (typically 2-3× for neural data),
# transfers to Hetzner, verifies integrity.

set -e

HETZNER_USER="bbrelin"
HETZNER_HOST="176.9.99.103"
HETZNER_DIR="/home/bbrelin/nimcp/checkpoints/athena"
CHECKPOINT="${1:-/workspace/nimcp/checkpoints/athena/athena_s1_step20000.bin}"
SIDECAR_EXTS=".snn .lnn .cnn .meta .mirror_neurons .executive .tokenizer .cortex_visual .cortex_audio .cortex_speech .cortex_somato"

if [ ! -f "$CHECKPOINT" ]; then
    echo "ERROR: Checkpoint not found: $CHECKPOINT"
    exit 1
fi

BASENAME=$(basename "$CHECKPOINT")
SIZE_ORIG=$(stat -c%s "$CHECKPOINT" 2>/dev/null || stat -f%z "$CHECKPOINT")
SIZE_MB=$((SIZE_ORIG / 1048576))
echo "=== Checkpoint Sync ==="
echo "  Source: $CHECKPOINT ($SIZE_MB MB)"
echo "  Target: $HETZNER_USER@$HETZNER_HOST:$HETZNER_DIR/"

# Check if zstd is available
if command -v zstd &> /dev/null; then
    COMPRESSED="${CHECKPOINT}.zst"
    echo "  Compressing with zstd..."
    zstd -T0 -3 -f "$CHECKPOINT" -o "$COMPRESSED" 2>/dev/null
    SIZE_COMP=$(stat -c%s "$COMPRESSED" 2>/dev/null || stat -f%z "$COMPRESSED")
    SIZE_COMP_MB=$((SIZE_COMP / 1048576))
    RATIO=$(echo "scale=1; $SIZE_ORIG / $SIZE_COMP" | bc 2>/dev/null || echo "?")
    echo "  Compressed: $SIZE_COMP_MB MB (${RATIO}× ratio)"
    TRANSFER_FILE="$COMPRESSED"
    REMOTE_FILE="${BASENAME}.zst"
else
    echo "  zstd not available — transferring uncompressed"
    TRANSFER_FILE="$CHECKPOINT"
    REMOTE_FILE="$BASENAME"
fi

# Ensure remote directory exists
ssh -o StrictHostKeyChecking=no "$HETZNER_USER@$HETZNER_HOST" \
    "mkdir -p $HETZNER_DIR" 2>/dev/null

# Transfer main checkpoint
echo "  Transferring main checkpoint..."
rsync -avz --progress "$TRANSFER_FILE" \
    "$HETZNER_USER@$HETZNER_HOST:$HETZNER_DIR/$REMOTE_FILE"

# Transfer sidecar files
for ext in $SIDECAR_EXTS; do
    SIDECAR="${CHECKPOINT}${ext}"
    if [ -f "$SIDECAR" ]; then
        echo "  Transferring sidecar: $(basename $SIDECAR)"
        rsync -avz "$SIDECAR" \
            "$HETZNER_USER@$HETZNER_HOST:$HETZNER_DIR/" 2>/dev/null
    fi
done

# Transfer state file
STATE_FILE="$(dirname $CHECKPOINT)/immersive_state.json"
if [ -f "$STATE_FILE" ]; then
    rsync -avz "$STATE_FILE" \
        "$HETZNER_USER@$HETZNER_HOST:$HETZNER_DIR/"
fi

# Verify remote file exists
REMOTE_SIZE=$(ssh "$HETZNER_USER@$HETZNER_HOST" \
    "stat -c%s $HETZNER_DIR/$REMOTE_FILE 2>/dev/null" || echo "0")
LOCAL_SIZE=$(stat -c%s "$TRANSFER_FILE" 2>/dev/null || stat -f%z "$TRANSFER_FILE")

if [ "$REMOTE_SIZE" = "$LOCAL_SIZE" ]; then
    echo "  ✓ Verified: remote size matches ($REMOTE_SIZE bytes)"
else
    echo "  ✗ WARNING: size mismatch! Local=$LOCAL_SIZE Remote=$REMOTE_SIZE"
fi

# Clean up compressed file
if [ -f "$COMPRESSED" ]; then
    rm -f "$COMPRESSED"
fi

echo "=== Sync complete ==="
