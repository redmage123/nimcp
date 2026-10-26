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
CKPT_DIR=/dev/shm/athena_ckpt
# A dead CUDA context (after cudaDeviceReset / OOM cascade) keeps the trainer
# alive and logging while every GPU call fails. Observed 2026-10-26: 2.3M such
# lines over 5h, trainer still saving checkpoints. Count them in the log tail.
GPU_DEAD_PATTERN='busy or unavailable|recovery action: RESET_DEVICE|CUDA UNRECOVERABLE'
GPU_DEAD_THRESHOLD=20
GPU_DEAD_TAIL_BYTES=2000000
GPU_MEM_SAMPLES=/tmp/gpu_mem_samples.csv

log_wd() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" | tee -a "$WATCHDOG_LOG"
}

diagnose_and_fix() {
    log_wd "Running GPU diagnostic and fix script..."
    local diag_script="$(dirname $0)/gpu_diagnose_fix.sh"
    if [ ! -x "$diag_script" ]; then
        log_wd "ERROR: diagnostic script not found at $diag_script"
        return 1
    fi

    bash "$diag_script" "$TRAINING_LOG"
    local diag_result=$?

    case "$diag_result" in
        0)
            log_wd "DIAGNOSIS: Fixed, safe to restart"
            return 0
            ;;
        1)
            log_wd "DIAGNOSIS: Fix attempted, may fail again"
            return 0  # Still try to restart
            ;;
        2)
            log_wd "DIAGNOSIS: Unfixable - manual intervention needed"
            return 1
            ;;
        *)
            log_wd "DIAGNOSIS: Unknown result $diag_result"
            return 1
            ;;
    esac
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

            local dead_count
            dead_count=$(tail -c "$GPU_DEAD_TAIL_BYTES" "$TRAINING_LOG" 2>/dev/null \
                         | grep -cE "$GPU_DEAD_PATTERN")
            if [ "${dead_count:-0}" -ge "$GPU_DEAD_THRESHOLD" ]; then
                log_wd "GPU CONTEXT DEAD: $dead_count CUDA failures in log tail - killing PID $pid"
                kill -9 "$pid" 2>/dev/null
                return 42
            fi
        fi
    done

    # Process exited naturally, get its exit code
    wait "$pid" 2>/dev/null
    return $?
}

start_gpu_sampler() {
    # 60s VRAM/utilization samples — the only way to see a slow leak coming.
    if [ -f "$GPU_MEM_SAMPLES" ] && [ "$(stat -c%s "$GPU_MEM_SAMPLES")" -gt 5000000 ]; then
        tail -c 1000000 "$GPU_MEM_SAMPLES" > "$GPU_MEM_SAMPLES.tmp" && mv "$GPU_MEM_SAMPLES.tmp" "$GPU_MEM_SAMPLES"
    fi
    nvidia-smi --query-gpu=timestamp,index,memory.used,utilization.gpu \
        --format=csv,noheader -l 60 >> "$GPU_MEM_SAMPLES" 2>/dev/null &
    echo $!
}

# Main watchdog loop
log_wd "========================================"
log_wd "Training watchdog starting (stage=$STAGE)"
log_wd "========================================"

restart_count=0
while [ "$restart_count" -lt "$MAX_RESTARTS" ]; do
    restart_count=$((restart_count + 1))
    log_wd "Starting training attempt #$restart_count"

    # Check checkpoint exists. The trainer resumes from $CKPT_DIR only; the
    # old fallback to $NIMCP_DIR/checkpoints/athena was a stale symlink trap
    # (it reported "step2950" on 2026-10-26 while the trainer fresh-inited).
    # With a lineage on record (state file names a snapshot) and no local
    # canonical set — e.g. the canonical was just evicted to Hetzner — refuse
    # to start rather than let --resume become a fresh brain.
    if [ -f "$CKPT_DIR/athena_immersive.bin" ] && [ -f "$CKPT_DIR/athena_immersive.bin.snn" ]; then
        ckpt=$(readlink -f "$CKPT_DIR/athena_immersive.bin")
        log_wd "Resuming from checkpoint: $(basename $ckpt)"
    elif grep -q '"snapshot"' "$NIMCP_DIR/checkpoints/athena/immersive_state.json" 2>/dev/null; then
        log_wd "FATAL: no local canonical checkpoint in $CKPT_DIR but the state file records a lineage ($(tr -d '\n' < "$NIMCP_DIR/checkpoints/athena/immersive_state.json" | cut -c1-120)) — restore it from Hetzner (scripts/vast_pod_recovery.sh) before starting; refusing to train a fresh brain"
        exit 3
    else
        log_wd "No checkpoint found, starting fresh"
    fi

    # Preserve previous training log before overwriting
    if [ -f "$TRAINING_LOG" ] && [ "$restart_count" -gt 1 ]; then
        prev_num=$((restart_count - 1))
        archive_log="${LOG_DIR}/training_attempt_${prev_num}.log"
        # Only keep last 10 attempts worth of logs (preserve disk space)
        cp "$TRAINING_LOG" "$archive_log" 2>/dev/null
        log_wd "Archived previous log to $(basename $archive_log)"
        # Clean up old archives (keep last 10)
        ls -t ${LOG_DIR}/training_attempt_*.log 2>/dev/null | tail -n +11 | xargs -r rm -f
    fi

    # Start training in background
    cd "$NIMCP_DIR" || { log_wd "FATAL: Cannot cd to $NIMCP_DIR"; exit 1; }

    # Prevent huggingface/network hangs - we don't need external data
    export HF_HUB_OFFLINE=1
    export TRANSFORMERS_OFFLINE=1
    export HF_DATASETS_OFFLINE=1
    export NIMCP_SKIP_AUDIO_DOWNLOAD=1
    # Save checkpoints to /dev/shm (RAM-backed tmpfs, 31GB avail)
    # instead of disk (32GB vast.ai overlay FS is too tight for 6+GB saves).
    # _ship_to_hetzner rsyncs to durable storage after each save.
    export NIMCP_USE_SHM_CHECKPOINT=1
    mkdir -p /dev/shm/athena_ckpt

    # Prefer PyTorch's bundled CUDA 12.8 libs over system CUDA 12.0 libs
    # (system libcudart.so.12 is 12.0.146 - missing cudaGetDriverEntryPointByVersion)
    PYTORCH_CUDA_LIBS="/usr/local/lib/python3.12/dist-packages/nvidia/cuda_runtime/lib"
    export LD_LIBRARY_PATH="${PYTORCH_CUDA_LIBS}:${LD_LIBRARY_PATH}"

    python3 scripts/immerse_athena.py --stage "$STAGE" --resume > "$TRAINING_LOG" 2>&1 &
    train_pid=$!
    log_wd "Training started with PID $train_pid"
    sampler_pid=$(start_gpu_sampler)

    # Monitor training
    monitor_training "$train_pid"
    exit_code=$?
    kill "$sampler_pid" 2>/dev/null

    log_wd "Training exited with code: $exit_code"

    # Determine action based on exit code
    case "$exit_code" in
        0)
            log_wd "Training completed successfully!"
            exit 0
            ;;
        3)
            log_wd "FATAL: trainer refused to resume (exit 3: lineage on record but no local checkpoint) - not restarting"
            exit 3
            ;;
        42)
            log_wd "GPU error detected (code 42) - diagnosing and fixing"
            if ! diagnose_and_fix; then
                log_wd "FATAL: Diagnosis says unfixable - exiting"
                exit 2
            fi
            ;;
        99)
            log_wd "Hang detected - diagnosing and fixing"
            if ! diagnose_and_fix; then
                log_wd "FATAL: Diagnosis says unfixable - exiting"
                exit 2
            fi
            ;;
        *)
            log_wd "Unexpected exit code $exit_code - diagnosing and fixing"
            if ! diagnose_and_fix; then
                log_wd "FATAL: Diagnosis says unfixable - exiting"
                exit 2
            fi
            ;;
    esac

    log_wd "Waiting ${RESTART_DELAY_SEC}s before restart..."
    sleep "$RESTART_DELAY_SEC"
done

log_wd "FATAL: Maximum restart limit ($MAX_RESTARTS) reached, giving up"
exit 1
