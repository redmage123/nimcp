#!/bin/bash
#
# GPU Diagnostic and Fix Script
#
# Called by watchdog when training fails with GPU error.
# Performs comprehensive diagnosis and attempts fixes.
#
# Exit codes:
#   0 = fixed, safe to restart
#   1 = fix attempted but may fail again
#   2 = unfixable, needs manual intervention

TRAINING_LOG="${1:-/tmp/training_watchdog.log}"
DIAG_LOG="/tmp/gpu_diagnosis.log"
WATCHDOG_LOG="/tmp/watchdog.log"

log_diag() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" | tee -a "$DIAG_LOG"
}

log_diag "=========================================="
log_diag "GPU DIAGNOSIS - analyzing failure"
log_diag "=========================================="

# ============================================================
# PHASE 1: Collect diagnostic data
# ============================================================

log_diag "PHASE 1: Collecting diagnostic data..."

# Get last 100 lines of training log for error context
LAST_LINES=$(tail -100 "$TRAINING_LOG" 2>/dev/null)
if [ -z "$LAST_LINES" ]; then
    log_diag "WARNING: No training log found"
fi

# Identify the specific error pattern
ERROR_TYPE="unknown"
if echo "$LAST_LINES" | grep -q "busy or unavailable"; then
    ERROR_TYPE="device_busy"
    log_diag "ERROR TYPE: GPU device busy/unavailable"
elif echo "$LAST_LINES" | grep -q "out of memory"; then
    ERROR_TYPE="oom"
    log_diag "ERROR TYPE: GPU out of memory"
elif echo "$LAST_LINES" | grep -q "illegal memory access"; then
    ERROR_TYPE="illegal_access"
    log_diag "ERROR TYPE: Illegal memory access"
elif echo "$LAST_LINES" | grep -q "cudaDeviceSynchronize FAILED"; then
    ERROR_TYPE="sync_failed"
    log_diag "ERROR TYPE: Synchronization failure"
elif echo "$LAST_LINES" | grep -q "GPU 1 spike vector ready" && ! echo "$LAST_LINES" | grep -q "post-step breakdown"; then
    ERROR_TYPE="hang_in_pop_loop"
    log_diag "ERROR TYPE: Hang in population loop (silent kernel failure)"
fi

# Get GPU step where failure occurred
LAST_STEP=$(echo "$LAST_LINES" | grep "SNN step" | tail -1 | grep -oP "step \d+" | grep -oP "\d+")
log_diag "LAST SUCCESSFUL SNN STEP: ${LAST_STEP:-unknown}"

# Get spike count at failure (high spikes = more GPU pressure)
LAST_SPIKES=$(echo "$LAST_LINES" | grep "SNN step" | tail -1 | grep -oP "spikes=\d+" | grep -oP "\d+")
log_diag "SPIKES AT FAILURE: ${LAST_SPIKES:-unknown}"

# ============================================================
# PHASE 2: GPU state analysis
# ============================================================

log_diag "PHASE 2: GPU state analysis..."

# Check GPU accessibility
if ! nvidia-smi > /dev/null 2>&1; then
    log_diag "CRITICAL: nvidia-smi fails - driver may be dead"
    log_diag "Attempting nvidia-smi reset..."
    nvidia-smi -r 2>&1 | tee -a "$DIAG_LOG"
    sleep 5
fi

# Capture current GPU state
log_diag "Current GPU state:"
nvidia-smi --query-gpu=index,memory.used,memory.total,utilization.gpu,temperature.gpu,power.draw --format=csv 2>&1 | tee -a "$DIAG_LOG"

# Check for orphaned CUDA contexts (processes holding GPU memory)
log_diag "Processes using GPUs:"
nvidia-smi --query-compute-apps=pid,process_name,used_memory --format=csv 2>&1 | tee -a "$DIAG_LOG"

# Check GPU memory fragmentation estimate
GPU0_FREE=$(nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits -i 0 2>/dev/null || echo 0)
GPU1_FREE=$(nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits -i 1 2>/dev/null || echo 0)
log_diag "GPU 0 free: ${GPU0_FREE} MB, GPU 1 free: ${GPU1_FREE} MB"

# ============================================================
# PHASE 3: Apply targeted fixes based on error type
# ============================================================

log_diag "PHASE 3: Applying targeted fixes for error type: $ERROR_TYPE"

FIX_RESULT=1  # default: fix attempted, may fail again

case "$ERROR_TYPE" in
    "device_busy"|"hang_in_pop_loop"|"sync_failed")
        log_diag "FIX: GPU device busy - clearing CUDA state"

        # 1. Kill ALL python processes to free CUDA contexts
        log_diag "  Step 1: Killing all python processes"
        pkill -9 -f python3 2>/dev/null
        sleep 2

        # 2. Clear CUDA cache files that might be stale
        log_diag "  Step 2: Clearing CUDA cache"
        rm -rf ~/.nv/ComputeCache 2>/dev/null
        rm -rf /tmp/cuda-* 2>/dev/null

        # 3. Reset NVIDIA driver state via proc
        log_diag "  Step 3: Resetting NVIDIA driver state"
        for i in 0 1; do
            if [ -w /proc/driver/nvidia/gpus/0000:*:00.0/power ]; then
                echo "auto" > /proc/driver/nvidia/gpus/0000:*:00.0/power 2>/dev/null
            fi
        done

        # 4. Attempt device reset
        log_diag "  Step 4: Attempting GPU device reset"
        nvidia-smi --gpu-reset 2>&1 | tee -a "$DIAG_LOG"

        # 5. Verify GPU accessibility
        sleep 3
        if nvidia-smi > /dev/null 2>&1; then
            # Try to do a basic CUDA test
            log_diag "  Step 5: Running CUDA sanity test"
            python3 -c "
import subprocess
import sys
try:
    # Try to initialize CUDA
    import ctypes
    cuda = ctypes.CDLL('libcudart.so')
    device_count = ctypes.c_int()
    result = cuda.cudaGetDeviceCount(ctypes.byref(device_count))
    if result == 0 and device_count.value > 0:
        print(f'CUDA OK: {device_count.value} devices')
        sys.exit(0)
    else:
        print(f'CUDA FAIL: result={result}, devices={device_count.value}')
        sys.exit(1)
except Exception as e:
    print(f'CUDA ERROR: {e}')
    sys.exit(1)
" 2>&1 | tee -a "$DIAG_LOG"

            if [ ${PIPESTATUS[0]} -eq 0 ]; then
                log_diag "FIX SUCCESS: GPUs responsive after reset"
                FIX_RESULT=0
            else
                log_diag "FIX PARTIAL: GPU reset done but CUDA test failed"
                FIX_RESULT=1
            fi
        else
            log_diag "FIX FAILED: GPUs still inaccessible"
            FIX_RESULT=2
        fi
        ;;

    "oom")
        log_diag "FIX: GPU OOM - clearing memory"
        pkill -9 -f python3 2>/dev/null
        sleep 2
        rm -rf ~/.nv/ComputeCache 2>/dev/null

        # Check how much memory is freed
        sleep 2
        GPU0_FREE_AFTER=$(nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits -i 0 2>/dev/null || echo 0)
        log_diag "  Memory after cleanup: GPU 0 = ${GPU0_FREE_AFTER} MB"
        if [ "$GPU0_FREE_AFTER" -gt 20000 ]; then
            log_diag "FIX SUCCESS: Memory freed"
            FIX_RESULT=0
        else
            log_diag "FIX PARTIAL: Limited memory recovered"
            FIX_RESULT=1
        fi
        ;;

    "illegal_access")
        log_diag "FIX: Illegal memory access - full GPU reset required"
        pkill -9 -f python3 2>/dev/null
        sleep 2
        nvidia-smi --gpu-reset 2>&1 | tee -a "$DIAG_LOG"
        sleep 3
        if nvidia-smi > /dev/null 2>&1; then
            FIX_RESULT=1  # Reset done but memory may be corrupted
        else
            FIX_RESULT=2
        fi
        ;;

    *)
        log_diag "FIX: Unknown error type - generic recovery"
        pkill -9 -f python3 2>/dev/null
        sleep 3
        if nvidia-smi > /dev/null 2>&1; then
            FIX_RESULT=1
        else
            FIX_RESULT=2
        fi
        ;;
esac

# ============================================================
# PHASE 4: Pattern analysis for persistent issues
# ============================================================

log_diag "PHASE 4: Pattern analysis..."

# Count recent restarts in watchdog log
RESTART_COUNT=$(grep -c "Training started with PID" "$WATCHDOG_LOG" 2>/dev/null || echo 0)
log_diag "Total restart count: $RESTART_COUNT"

# If we're restarting repeatedly at same step, log pattern
if [ "$RESTART_COUNT" -gt 3 ]; then
    log_diag "WARNING: $RESTART_COUNT restarts detected - checking for persistent failure pattern"
    FAILURE_STEPS=$(grep "LAST SUCCESSFUL SNN STEP:" "$DIAG_LOG" | tail -5)
    log_diag "Recent failure steps: $FAILURE_STEPS"
fi

log_diag "=========================================="
log_diag "DIAGNOSIS COMPLETE - exit code $FIX_RESULT"
log_diag "=========================================="

exit $FIX_RESULT
