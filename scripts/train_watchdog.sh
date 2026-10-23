#!/bin/bash
#
# Training Watchdog - automatically restarts training from last checkpoint on crash
#
# Usage: ./train_watchdog.sh [--stage N]
#
# Behavior:
#   - Starts training with --resume (from last checkpoint)
#   - Monitors for crashes, hangs, or GPU failures
#   - Exit code 42 = GPU error, restart from checkpoint
#   - Exit code 0 = normal completion
#   - Other codes = unexpected crash, restart from checkpoint
#   - Detects hangs (no log activity for 5+ minutes) and kills+restarts
#   - Logs all restarts with timestamps

STAGE="${1:-0}"
NIMCP_DIR="${NIMCP_DIR:-/root/nimcp}"
LOG_DIR="/tmp"
WATCHDOG_LOG="${LOG_DIR}/watchdog.log"
TRAINING_LOG="${LOG_DIR}/training_watchdog.log"
HANG_TIMEOUT_SEC=300   # 5 minutes without log activity = hang
MAX_RESTARTS=100       # Safety limit
RESTART_DELAY_SEC=10   # Wait between restarts

log_wd() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" | tee -a "$WATCHDOG_LOG"
}

cleanup_gpu() {
    log_wd "Attempting GPU cleanup..."
    # Kill any lingering python processes
    pkill -9 -f immerse_athena 2>/dev/null
    sleep 2
    # Reset GPU (if possible without root restriction)
    nvidia-smi --gpu-reset 2>/dev/null || log_wd "Note: nvidia-smi --gpu-reset not available"
    sleep 2
    # Check GPU accessibility
    if nvidia-smi > /dev/null 2>&1; then
        log_wd "GPUs accessible after cleanup"
        return 0
    else
        log_wd "WARNING: GPUs not accessible"
        return 1
    fi
}

monitor_training() {
    local pid=$1
    local last_size=0
    local stale_count=0
    local check_interval=30  # Check every 30 seconds

    while kill -0 "$pid" 2>/dev/null; do
        sleep "$check_interval"

        if [ -f "$TRAINING_LOG" ]; then
            local current_size=$(stat -c%s "$TRAINING_LOG" 2>/dev/null || echo 0)
            if [ "$current_size" -eq "$last_size" ]; then
                stale_count=$((stale_count + check_interval))
                if [ "$stale_count" -ge "$HANG_TIMEOUT_SEC" ]; then
                    log_wd "HANG DETECTED: No log activity for ${stale_count}s - killing PID $pid"
                    kill -9 "$pid" 2>/dev/null
                    return 99  # Hang exit code
                fi
            else
                stale_count=0
                last_size=$current_size
            fi
        fi
    done

    # Process exited naturally, get its exit code
    wait "$pid" 2>/dev/null
    return $?
}

# Main watchdog loop
log_wd "========================================"
log_wd "Training watchdog starting (stage=$STAGE)"
log_wd "========================================"

restart_count=0
while [ "$restart_count" -lt "$MAX_RESTARTS" ]; do
    restart_count=$((restart_count + 1))
    log_wd "Starting training attempt #$restart_count"

    # Check checkpoint exists
    if [ -f "$NIMCP_DIR/checkpoints/athena/athena_immersive.bin" ]; then
        local ckpt=$(readlink -f "$NIMCP_DIR/checkpoints/athena/athena_immersive.bin")
        log_wd "Resuming from checkpoint: $(basename $ckpt)"
    else
        log_wd "No checkpoint found, starting fresh"
    fi

    # Start training in background
    cd "$NIMCP_DIR" || { log_wd "FATAL: Cannot cd to $NIMCP_DIR"; exit 1; }

    python3 scripts/immerse_athena.py --stage "$STAGE" --resume > "$TRAINING_LOG" 2>&1 &
    train_pid=$!
    log_wd "Training started with PID $train_pid"

    # Monitor training
    monitor_training "$train_pid"
    exit_code=$?

    log_wd "Training exited with code: $exit_code"

    # Determine action based on exit code
    case "$exit_code" in
        0)
            log_wd "Training completed successfully!"
            exit 0
            ;;
        42)
            log_wd "GPU error detected (code 42) - will restart from checkpoint"
            cleanup_gpu
            ;;
        99)
            log_wd "Hang detected - will restart from checkpoint"
            cleanup_gpu
            ;;
        *)
            log_wd "Unexpected exit code $exit_code - will restart from checkpoint"
            cleanup_gpu
            ;;
    esac

    log_wd "Waiting ${RESTART_DELAY_SEC}s before restart..."
    sleep "$RESTART_DELAY_SEC"
done

log_wd "FATAL: Maximum restart limit ($MAX_RESTARTS) reached, giving up"
exit 1
