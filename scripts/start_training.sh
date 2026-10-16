#!/bin/bash
# Start Athena training WITH Claude teacher enabled
# Usage: ./scripts/start_training.sh [--fresh|--resume]
set -euo pipefail

cd /home/bbrelin/nimcp

# Kill any existing training
pkill -f "immerse_athena" 2>/dev/null || true
sleep 2

MODE="${1:---fresh}"

# Environment — prevent CUDA conflicts, COW signal handler, noisy libs
export NIMCP_NO_COW_SIGNAL=1
export TOKENIZERS_PARALLELISM=false
export TQDM_DISABLE=1

echo "Starting Athena training with Claude teacher..."
echo "  Mode: $MODE"
echo "  Log:  /home/bbrelin/nimcp/immerse_athena.log"

nohup python3 -u scripts/immerse_athena.py $MODE > immerse_athena.log 2>&1 &
TRAIN_PID=$!
echo "  PID:  $TRAIN_PID"

# Start watchdog
sleep 2
nohup bash scripts/watchdog_training.sh $TRAIN_PID > /dev/null 2>&1 &
echo "  Watchdog: $!"

echo ""
echo "Monitor: tail -f immerse_athena.log"
echo "Kill:    kill $TRAIN_PID"
