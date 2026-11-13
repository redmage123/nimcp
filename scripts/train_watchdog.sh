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
# Daemon mode (2026-11-13): when the trainer flags include --daemon, the brain
# lives in `brain_daemon.py --trainer-mode` and survives trainer restarts. The
# watchdog starts it, health-checks it before every trainer start, and treats
# its log as part of the training log (C-side logging moves to the daemon).
DAEMON_LOG="${LOG_DIR}/brain_daemon.log"
DAEMON_LOAD_TIMEOUT_SEC=2400     # checkpoint load on Vast: ~2-4 min; generous
DAEMON_BARRIER_TIMEOUT_SEC=900   # a save (~21 GB SNN to shm) takes ~1 min
DAEMON_MODE=0
DAEMON_MAX_START_FAILS=3         # in a row; a load that keeps failing is not transient
daemon_start_fails=0
ONCE_ENV_FILE=/root/train_once_env

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

# One-shot environment: the first line of $ONCE_ENV_FILE, consumed (deleted)
# by whichever process loads the brain next — the trainer, or in daemon mode
# the daemon.
take_once_env() {
    if [ -f "$ONCE_ENV_FILE" ]; then
        head -n1 "$ONCE_ENV_FILE"
        rm -f "$ONCE_ENV_FILE"
    fi
}

daemon_pids() {
    pgrep -f "^python3 scripts/brain_daemon.py"
}

# daemon_rpc CMD TIMEOUT_SEC — exit 0 when the daemon answers CMD in time.
# "barrier" runs under the brain lock, so it also proves the lock is not stuck.
daemon_rpc() {
    python3 - "$1" "$2" <<'PY'
import sys
sys.path.insert(0, "scripts")
from brain_client import BrainProxy
try:
    BrainProxy(timeout=float(sys.argv[2]))._send_once({"cmd": sys.argv[1]})
except Exception as e:
    print(f"daemon {sys.argv[1]}: {type(e).__name__}: {e}")
    sys.exit(1)
PY
}

stop_daemon() {
    local pids p i
    pids=$(daemon_pids)
    [ -z "$pids" ] && return 0
    log_wd "Stopping brain daemon (PIDs $pids) - trainer mode keeps nothing unsaved of its own"
    kill -TERM $pids 2>/dev/null
    for i in $(seq 1 30); do
        [ -z "$(daemon_pids)" ] && return 0
        sleep 2
    done
    for p in $(daemon_pids); do kill -9 "$p" 2>/dev/null; done
    sleep 3
}

start_daemon() {
    local once ckpt waited=0 pid
    if [ ! -f "$CKPT_DIR/athena_immersive.bin" ]; then
        log_wd "FATAL: no checkpoint at $CKPT_DIR/athena_immersive.bin for the brain daemon"
        return 1
    fi
    once=$(take_once_env)
    ckpt=$(readlink -f "$CKPT_DIR/athena_immersive.bin")
    log_wd "Starting brain daemon (trainer mode) from $(basename "$ckpt")${once:+ with one-shot env: $once}"
    [ -f "$DAEMON_LOG" ] && mv -f "$DAEMON_LOG" "$DAEMON_LOG.prev"
    # shellcheck disable=SC2086  # word-split the one-shot env on purpose
    env $once python3 scripts/brain_daemon.py --trainer-mode \
        --checkpoint "$CKPT_DIR/athena_immersive.bin" > "$DAEMON_LOG" 2>&1 < /dev/null &
    pid=$!
    while [ "$waited" -lt "$DAEMON_LOAD_TIMEOUT_SEC" ]; do
        sleep 15
        waited=$((waited + 15))
        if ! kill -0 "$pid" 2>/dev/null; then
            log_wd "Brain daemon exited while loading - see $DAEMON_LOG"
            return 1
        fi
        if daemon_rpc ping 10 > /dev/null; then
            log_wd "Brain daemon ready (PID $pid) after ${waited}s"
            return 0
        fi
    done
    log_wd "Brain daemon not ready after ${DAEMON_LOAD_TIMEOUT_SEC}s - stopping it"
    stop_daemon
    return 1
}

# Make sure a healthy daemon is serving before a trainer start: reuse a live
# one (wait if it is still loading), restart one whose brain lock is stuck.
ensure_daemon() {
    local waited=0 msg
    if [ -n "$(daemon_pids)" ]; then
        while ! daemon_rpc ping 10 > /dev/null; do
            if [ -z "$(daemon_pids)" ] || [ "$waited" -ge "$DAEMON_LOAD_TIMEOUT_SEC" ]; then
                break
            fi
            [ "$waited" -eq 0 ] && log_wd "Brain daemon running but not answering yet - waiting (still loading?)"
            sleep 15
            waited=$((waited + 15))
        done
        if [ -n "$(daemon_pids)" ] && daemon_rpc ping 10 > /dev/null; then
            if msg=$(daemon_rpc barrier "$DAEMON_BARRIER_TIMEOUT_SEC"); then
                log_wd "Brain daemon healthy (PID $(daemon_pids | tr '\n' ' ')) - reusing it"
                return 0
            fi
            log_wd "Brain daemon answers ping but its brain lock is stuck ($msg) - restarting it"
        else
            log_wd "Brain daemon not answering after ${waited}s - restarting it"
        fi
        stop_daemon
    fi
    start_daemon
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
            if [ "$DAEMON_MODE" = 1 ]; then
                current_size=$((current_size + $(stat -c%s "$DAEMON_LOG" 2>/dev/null || echo 0)))
            fi
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

            local dead_count gpu_log="$TRAINING_LOG"
            # In daemon mode the CUDA work, and its errors, are in the daemon.
            [ "$DAEMON_MODE" = 1 ] && gpu_log="$DAEMON_LOG"
            dead_count=$(tail -c "$GPU_DEAD_TAIL_BYTES" "$gpu_log" 2>/dev/null \
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
# BBR congestion control for the long, lossy pod<->Hetzner path (checkpoint
# ships). The container setting does not survive restarts, so set it here.
sysctl -q -w net.ipv4.tcp_congestion_control=bbr 2>/dev/null \
    && log_wd "TCP congestion control: $(cat /proc/sys/net/ipv4/tcp_congestion_control)"
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

    # Extra trainer flags (2026-11-05, e.g. "--video-phase --stage0-video-rate 0.7"):
    # $TRAIN_EXTRA_ARGS, else the first line of /root/train_extra_args, which
    # survives watchdog restarts that do not carry the environment.
    extra_args="${TRAIN_EXTRA_ARGS:-$(head -n1 /root/train_extra_args 2>/dev/null)}"
    # One-shot environment (2026-11-12): the first line of /root/train_once_env
    # (e.g. "NIMCP_ANN_OUTPUT_LOAD_SCALE=0.05") applies to THIS start only and
    # the file is deleted, so a later crash-restart cannot repeat a one-off
    # such as an output-layer rescale of the loaded checkpoint.
    once_env=""
    case " $extra_args " in
        *" --daemon "*) DAEMON_MODE=1 ;;
        *) DAEMON_MODE=0 ;;
    esac
    if [ "$DAEMON_MODE" = 1 ]; then
        # The daemon loads the brain, so it takes the one-shot environment
        # (only when it is (re)started; a reused daemon leaves the file).
        if ! ensure_daemon; then
            daemon_start_fails=$((daemon_start_fails + 1))
            if [ "$daemon_start_fails" -ge "$DAEMON_MAX_START_FAILS" ]; then
                log_wd "FATAL: brain daemon failed to start $daemon_start_fails times in a row - manual intervention needed (see $DAEMON_LOG)"
                exit 2
            fi
            log_wd "Brain daemon failed to start ($daemon_start_fails/$DAEMON_MAX_START_FAILS) - retrying in 60s"
            sleep 60
            continue
        fi
        daemon_start_fails=0
    else
        once_env="$(take_once_env)"
        [ -n "$once_env" ] && log_wd "One-shot environment for this start: $once_env"
    fi
    # shellcheck disable=SC2086  # word-split the flag list on purpose
    env $once_env python3 scripts/immerse_athena.py --stage "$STAGE" --resume $extra_args > "$TRAINING_LOG" 2>&1 &
    train_pid=$!
    log_wd "Training started with PID $train_pid${extra_args:+ (extra args: $extra_args)}"
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
            # The daemon owns the dead CUDA context: stop it before the fix.
            [ "$DAEMON_MODE" = 1 ] && stop_daemon
            if ! diagnose_and_fix; then
                log_wd "FATAL: Diagnosis says unfixable - exiting"
                exit 2
            fi
            ;;
        99)
            if [ "$DAEMON_MODE" = 1 ]; then
                # The trainer holds no GPU state; ensure_daemon checks the
                # brain (barrier) before the next start.
                log_wd "Hang detected - restarting the trainer"
            else
                log_wd "Hang detected - diagnosing and fixing"
                if ! diagnose_and_fix; then
                    log_wd "FATAL: Diagnosis says unfixable - exiting"
                    exit 2
                fi
            fi
            ;;
        *)
            if [ "$DAEMON_MODE" = 1 ]; then
                log_wd "Unexpected exit code $exit_code - restarting the trainer (daemon is checked first)"
            else
                log_wd "Unexpected exit code $exit_code - diagnosing and fixing"
                if ! diagnose_and_fix; then
                    log_wd "FATAL: Diagnosis says unfixable - exiting"
                    exit 2
                fi
            fi
            ;;
    esac

    log_wd "Waiting ${RESTART_DELAY_SEC}s before restart..."
    sleep "$RESTART_DELAY_SEC"
done

log_wd "FATAL: Maximum restart limit ($MAX_RESTARTS) reached, giving up"
exit 1
