#!/usr/bin/env bash
#
# run_regression_brain.sh — Hetzner-local fresh-init regression brain.
#
# Spins up a parallel brain instance on Hetzner with --fresh and a
# scaled-down SNN (200K neurons vs production's 1.8M) so it fits in
# 62GB of RAM. Validates the cold-init path that the production pod's
# resumed brain never exercises.
#
# IMPORTANT: runs the daemon from a scratch directory ($REGRESS_ROOT)
# instead of /home/bbrelin/nimcp because snn_create_hierarchical_network
# unconditionally checks `checkpoints/athena/athena_immersive.bin.snn`
# (RELATIVE path, hardcoded at nimcp_snn_hierarchical.c:185) for a cache
# hit. From the project root that path resolves to the production-synced
# checkpoint; our --fresh brain would silently load production state
# into its SNN. Running from a scratch cwd means the relative path
# resolves to nothing, the cache misses, and the SNN actually starts
# fresh as intended.
#
# Use a separate socket + checkpoint dir so this doesn't conflict with
# anything else. Logs go to $REGRESS_ROOT/.
#
# To stop:  pkill -TERM -f 'brain_daemon.py.*regression'

set -euo pipefail

REGRESS_ROOT=/tmp/regression_athena
SOCKET="$REGRESS_ROOT/brain.sock"
CHECKPOINT_DIR="$REGRESS_ROOT/checkpoint"     # inside scratch root
LOG="$REGRESS_ROOT/brain.log"
WORDNET=/home/bbrelin/nimcp/data/lexicon/wordnet_glove_v1.bin
DAEMON_PY=/home/bbrelin/nimcp/scripts/brain_daemon.py

mkdir -p "$REGRESS_ROOT" "$CHECKPOINT_DIR"
rm -f "$SOCKET"
# Wipe last run's log so tail doesn't find old "ready" lines
: > "$LOG"

# Run from a scratch dir so the daemon's relative-path cache lookups
# (nimcp_snn_hierarchical.c SNN_SIDECAR_PATH) miss instead of hitting
# the production-synced checkpoint at $REPO/checkpoints/athena/.
cd "$REGRESS_ROOT"

# LD_LIBRARY_PATH so the local libnimcp.so.* (with today's fixes) wins
# over any system-installed version. Python binding dlopens libnimcp.so.2
# at import time via the search path.
export LD_LIBRARY_PATH="/home/bbrelin/nimcp/build/lib:${LD_LIBRARY_PATH:-}"
export NIMCP_BULK_LEXICON="$WORDNET"
export PYTHONUNBUFFERED=1
export NIMCP_LOG_LEVEL=info
# Skip QuestDB — regression brain shouldn't pollute production metrics.
export NIMCP_QUESTDB_DISABLED=1

echo "=== Regression brain config ==="
echo "  cwd:            $(pwd)"
echo "  socket:         $SOCKET"
echo "  checkpoint dir: $CHECKPOINT_DIR"
echo "  log file:       $LOG"
echo "  bulk lexicon:   $WORDNET"
echo "  libnimcp:       $(stat -c %y /home/bbrelin/nimcp/build/lib/libnimcp.so.0.9.0 2>/dev/null | cut -d. -f1)"
echo "  ANN/SNN/LNN:    150K / feedforward(256) / 512"
echo
# NOTE on SNN sizing: --snn-neuron-count > 0 triggers the hierarchical
# SNN architecture (~1.8M neurons regardless of the value, since
# TIER_DEFS in nimcp_snn_hierarchical.c is hardcoded — the param is
# only used for logging). That eats 60+ GB during wiring. Setting to
# 0 falls through to the small feedforward SNN (~256 neurons/layer,
# ~10MB) which is sufficient for cold-init regression — we're testing
# the language stack and lifecycle, not SNN scale.

nohup python3 -u "$DAEMON_PY" \
    --fresh \
    --init-mode fast \
    --neuron-count 150000 \
    --snn-neuron-count 0 \
    --lnn-neuron-count 512 \
    --num-inputs 1024 \
    --num-outputs 2048 \
    --socket "$SOCKET" \
    --checkpoint-dir "$CHECKPOINT_DIR" \
    --workers 2 \
    --checkpoint-interval 600 \
    > "$LOG" 2>&1 &

PID=$!
echo "spawned regression brain PID=$PID"
echo "  log:    tail -f $LOG"
echo "  stop:   kill -TERM $PID"
echo
echo "=== Waiting for socket (up to 15min) ==="
for i in $(seq 1 90); do
    if [ -S "$SOCKET" ]; then
        echo "socket up after ${i}0s"
        break
    fi
    # Bail early if process died
    if ! kill -0 $PID 2>/dev/null; then
        echo "ERROR: brain process exited (PID $PID gone)"
        echo "Last 30 lines of log:"
        tail -30 "$LOG"
        exit 1
    fi
    sleep 10
done

if [ ! -S "$SOCKET" ]; then
    echo "ERROR: socket never appeared — brain still initializing or stuck"
    echo "Last 30 lines of log:"
    tail -30 "$LOG"
    exit 1
fi

# Wait an extra moment for "ready" line
for i in $(seq 1 30); do
    if grep -q "Brain daemon ready" "$LOG"; then
        echo "brain ready"
        grep -E "ATHENA BRAIN DAEMON|Neurons:|Load time:|recording OFF|FNO pops|inserted.*entries" "$LOG" | tail -10
        break
    fi
    sleep 5
done
