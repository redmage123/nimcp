#!/bin/bash
#
# NIMCP Training Monitor — cron-friendly, persistent across sessions
# Runs all 3 monitors: differentiation, health, checkpoint integrity
#
# Install: crontab -e, then add:
#   */5 * * * * /home/bbrelin/nimcp/scripts/monitor_training_cron.sh
#
# Alerts go to:
#   1. /home/bbrelin/nimcp/monitoring.log (always)
#   2. Desktop notification via notify-send (if DISPLAY available)
#   3. /home/bbrelin/nimcp/TRAINING_ALERT.txt (touch file on critical alerts)
#

set -uo pipefail

NIMCP_DIR="/home/bbrelin/nimcp"
TRAINING_LOG="/tmp/athena_pod_training.log"   # populated below from pod via SSH
CHECKPOINT_DIR="${NIMCP_DIR}/checkpoints/athena"
MONITOR_LOG="${NIMCP_DIR}/monitoring.log"
ALERT_FILE="${NIMCP_DIR}/TRAINING_ALERT.txt"
STATE_FILE="${CHECKPOINT_DIR}/immersive_state.json"

# Pod connection — sourced for $POD_SSH, $POD_HOST, etc.
# shellcheck source=podconfig.sh
. "${NIMCP_DIR}/scripts/podconfig.sh" 2>/dev/null || true
POD_AVAILABLE=0
[ -n "${POD_SSH:-}" ] && $POD_SSH -o BatchMode=yes 'true' 2>/dev/null && POD_AVAILABLE=1

# Thresholds
OUTPUT_SIM_THRESHOLD=0.95
EFF_RANK_THRESHOLD=10
GRAD_NORM_THRESHOLD=100
CHECKPOINT_MIN_SIZE=104857600  # 100 MB
RSS_WARN_GB=72  # warn if RSS > 72 GB (pod cgroup cap is 85.68 GB)
DISK_WARN_PCT=80       # warn if filesystem use% >= this
DISK_CRIT_PCT=92       # critical if filesystem use% >= this
SNAPSHOT_RETENTION=5   # keep N newest athena_auto_* snapshot families (incl. shards)
PIP_CACHE_CAP_GB=5     # truncate ~/.cache/pip if > this (GB)
UV_CACHE_CAP_GB=2      # truncate ~/.cache/uv if > this (GB)
HF_CACHE_WARN_GB=120   # alert (no delete) if huggingface cache > this (GB)
JOURNAL_CAP_MB=500     # vacuum /var/log/journal to this size (MB)
SIBLING_WARN_GB=50     # alert if any /home/$USER/<dir> outside nimcp > this

NOW=$(date '+%Y-%m-%d %H:%M:%S')

# Truncate monitor log if > 10MB
if [ -f "$MONITOR_LOG" ] && [ "$(stat -c%s "$MONITOR_LOG" 2>/dev/null || echo 0)" -gt 10485760 ]; then
    tail -1000 "$MONITOR_LOG" > "${MONITOR_LOG}.tmp" && mv "${MONITOR_LOG}.tmp" "$MONITOR_LOG"
fi

log() {
    echo "[$NOW] $1" >> "$MONITOR_LOG"
}

alert() {
    local level="$1"  # WARN or CRITICAL
    local msg="$2"
    log "[$level] $msg"
    echo "[$NOW] [$level] $msg" >> "$ALERT_FILE"

    # Desktop notification if possible
    if [ -n "${DISPLAY:-}" ] || [ -n "${WAYLAND_DISPLAY:-}" ]; then
        urgency="normal"
        [ "$level" = "CRITICAL" ] && urgency="critical"
        notify-send -u "$urgency" "NIMCP Training $level" "$msg" 2>/dev/null || true
    fi
}

#=============================================================================
# 0. FETCH METRICS FROM POD (training runs on RunPod, not laptop)
#=============================================================================
fetch_pod_metrics() {
    if [ "$POD_AVAILABLE" != "1" ]; then
        log "[FETCH] Pod unreachable — skipping pod metric fetch"
        return 1
    fi

    # Query brain daemon for live metrics, write in the format the bash parsers expect.
    local fetched
    fetched=$($POD_SSH "cd ${POD_DIR} && python3 -c '
from scripts.brain_client import BrainProxy
import subprocess, re
b = BrainProxy(\"${POD_SOCKET}\")
try:
    n = b.get_network_metrics()
except Exception as e:
    print(f\"FETCH_ERROR: get_network_metrics: {e}\"); raise SystemExit(0)
try:
    s = b.snn_get_stats()
except Exception:
    s = {}
try:
    sleep = b.sleep_get_state()
except Exception:
    sleep = 0
# Find brain daemon PID and RSS via supervisorctl
out = subprocess.run([\"supervisorctl\",\"status\",\"athena-brain\"], capture_output=True, text=True).stdout
m = re.search(r\"pid (\d+)\", out)
pid = m.group(1) if m else \"\"
status_line = out.strip().split(\"\n\")[0] if out else \"\"
rss = \"\"
if pid:
    try:
        with open(f\"/proc/{pid}/status\") as f:
            for line in f:
                if line.startswith(\"VmRSS:\"):
                    rss = line.split()[1]; break
    except Exception:
        pass
print(\"#POD_BRAIN_STATUS:\", status_line)
print(f\"#POD_BRAIN_PID: {pid} VmRSS_kB: {rss}\")
print(f\"  Arousal=0.50 | SleepPressure=0.00 (sleep_state={sleep})\")
print(f\"  LNN: steps={n.get(\"lnn_steps\",0)} loss={n.get(\"lnn_loss\",0):.4f}\")
print(f\"  SNN: steps={n.get(\"snn_steps\",0)} spikes={s.get(\"total_spikes\",0)} rate={s.get(\"mean_firing_rate\",0):.1f}Hz sparsity={s.get(\"sparsity\",0):.2f}\")
print(f\"  ANN: steps={n.get(\"ann_steps\",0)} loss={n.get(\"ann_loss\",0):.4f}\")
print(f\"  CNN: steps={n.get(\"cnn_steps\",0)} loss={n.get(\"cnn_loss\",0):.6f}\")
if s:
    print(f\"  SNN-detail: silent={s.get(\"silent_neurons\",0)} hyperactive={s.get(\"hyperactive_neurons\",0)} synchrony={s.get(\"synchrony\",0):.3f}\")
' 2>/dev/null" 2>/dev/null)

    if [ -z "$fetched" ]; then
        log "[FETCH] Empty response from pod brain daemon"
        return 1
    fi

    # Append, don't overwrite — keep history so trend grep works
    {
        echo "===== ${NOW} ====="
        echo "$fetched"
    } >> "$TRAINING_LOG"

    # Truncate if > 5MB
    if [ -f "$TRAINING_LOG" ] && [ "$(stat -c%s "$TRAINING_LOG" 2>/dev/null || echo 0)" -gt 5242880 ]; then
        tail -2000 "$TRAINING_LOG" > "${TRAINING_LOG}.tmp" && mv "${TRAINING_LOG}.tmp" "$TRAINING_LOG"
    fi
    return 0
}

#=============================================================================
# 1. PROCESS HEALTH
#=============================================================================
check_health() {
    log "--- HEALTH CHECK ---"

    if [ "$POD_AVAILABLE" != "1" ]; then
        alert "CRITICAL" "Pod ${POD_HOST:-?} unreachable on port ${POD_PORT:-?} — training health unknown"
        return
    fi

    # Pull supervisor status from pod
    local sup_status
    sup_status=$($POD_SSH "supervisorctl status 2>/dev/null" 2>/dev/null)
    if ! echo "$sup_status" | grep -q "athena-training.*RUNNING"; then
        alert "CRITICAL" "athena-training NOT RUNNING on pod"
        log "$sup_status"
    fi
    if ! echo "$sup_status" | grep -q "athena-brain.*RUNNING"; then
        alert "CRITICAL" "athena-brain NOT RUNNING on pod"
        log "$sup_status"
    fi

    # Brain daemon RSS
    local pid rss_kb rss_gb
    pid=$(echo "$sup_status" | awk '/athena-brain.*RUNNING/ {for(i=1;i<=NF;i++) if($i=="pid") print $(i+1)}' | tr -d ',')
    if [ -n "$pid" ]; then
        rss_kb=$($POD_SSH "awk '/^VmRSS:/ {print \$2}' /proc/$pid/status 2>/dev/null" 2>/dev/null)
        rss_kb=${rss_kb:-0}
        rss_gb=$(awk "BEGIN {printf \"%.1f\", $rss_kb / 1048576}")
        log "Pod brain PID=$pid | RSS=${rss_gb}GB"
        if [ "$(awk "BEGIN {print ($rss_gb > $RSS_WARN_GB)}")" = "1" ]; then
            alert "WARN" "Pod brain RSS high: ${rss_gb}GB (threshold: ${RSS_WARN_GB}GB) — OOM risk"
        fi
    fi
    # Skip the local pid block — pod-side metrics are now the source of truth.
    return
}

# Original local-process body kept intact below as _check_health_local (unused).
_check_health_local() {
    local pid rss_kb rss_gb cpu
    pid=$(pgrep -x -f "python3 scripts/immerse_athena.py" 2>/dev/null | head -1 || true)
    if [ -z "$pid" ]; then
        pid=$(ps aux | grep "[p]ython3.*immerse_athena" | awk '{print $2}' | head -1 || true)
    fi
    if [ -z "$pid" ]; then
        alert "CRITICAL" "Local training process NOT running! immerse_athena.py is dead."
        return
    fi
    rss_kb=$(cat /proc/"$pid"/status 2>/dev/null | grep VmRSS | awk '{print $2}' || echo 0)
    rss_kb=${rss_kb:-0}
    rss_gb=$(awk "BEGIN {printf \"%.1f\", $rss_kb / 1048576}")
    cpu=$(ps -p "$pid" -o %cpu= 2>/dev/null | tr -d ' ' || echo "?")
    log "Process alive PID=$pid | RSS=${rss_gb}GB | CPU=${cpu}%"

    if [ "$(awk "BEGIN {print ($rss_gb > $RSS_WARN_GB)}")" = "1" ]; then
        alert "WARN" "RSS memory high: ${rss_gb}GB (threshold: ${RSS_WARN_GB}GB)"
    fi

    # Check for NaN/Inf in recent log (last 500 lines)
    if [ -f "$TRAINING_LOG" ]; then
        local nan_count
        nan_count=$(tail -500 "$TRAINING_LOG" | grep -c -i "nan" 2>/dev/null || true)
        nan_count=${nan_count:-0}
        nan_count=$(echo "$nan_count" | tr -d '[:space:]')
        local inf_count
        inf_count=$(tail -500 "$TRAINING_LOG" | grep -c -iE "\binf\b|INFINITY" 2>/dev/null || true)
        inf_count=${inf_count:-0}
        inf_count=$(echo "$inf_count" | tr -d '[:space:]')

        if [ "$nan_count" -gt 5 ]; then
            alert "CRITICAL" "NaN detected in training: $nan_count occurrences in last 500 log lines"
        fi
        if [ "$inf_count" -gt 5 ]; then
            alert "WARN" "Inf detected in training: $inf_count occurrences in last 500 log lines"
        fi

        # Latest gradient norm from optimizer log
        local latest_grad
        latest_grad=$(grep -oP 'grad_norm=\K[0-9.e+-]+' "$TRAINING_LOG" | tail -1 || true)
        if [ -n "$latest_grad" ]; then
            log "Latest optimizer grad_norm=$latest_grad"
            # Check if > threshold (handle scientific notation)
            if [ "$(awk "BEGIN {print ($latest_grad > $GRAD_NORM_THRESHOLD)}" 2>/dev/null)" = "1" ]; then
                alert "WARN" "Gradient norm high: $latest_grad (threshold: $GRAD_NORM_THRESHOLD)"
            fi
        fi

        # Latest LNN adjoint norm
        local latest_adjoint
        latest_adjoint=$(grep -oP 'Adjoint computation complete: norm=\K[0-9.]+' "$TRAINING_LOG" | tail -1 || true)
        if [ -n "$latest_adjoint" ]; then
            log "Latest LNN adjoint norm=$latest_adjoint"
        fi

        # Latest loss value
        local latest_loss
        latest_loss=$(grep -oP 'loss[=:]\s*\K[0-9.e+-]+' "$TRAINING_LOG" | tail -1 || true)
        if [ -n "$latest_loss" ]; then
            log "Latest loss=$latest_loss"
        fi

        # K-WTA sparsity
        local latest_kwta
        latest_kwta=$(grep -oP 'sparsity[=:]\s*\K[0-9.]+' "$TRAINING_LOG" | tail -1 || true)
        if [ -n "$latest_kwta" ]; then
            log "Latest K-WTA sparsity=$latest_kwta"
        fi

        # Optimizer step count
        local latest_step
        latest_step=$(grep -oP 'Optimizer step \K[0-9]+' "$TRAINING_LOG" | tail -1 || true)
        if [ -n "$latest_step" ]; then
            log "Latest optimizer step=$latest_step"
        fi
    fi
}

#=============================================================================
# 2. PLASTICITY / SNN / NEUROMODULATION
#=============================================================================
check_bio_metrics() {
    log "--- BIO METRICS CHECK ---"

    if [ ! -f "$TRAINING_LOG" ]; then
        log "No training.log found"
        return
    fi

    # Plasticity state + neuromodulators (from _print_bio_stats output)
    local bio_line
    bio_line=$(grep "Bio:" "$TRAINING_LOG" | tail -1 || true)
    if [ -n "$bio_line" ]; then
        log "$bio_line"
        # Extract individual values
        local da ach rpe stdp ltp state
        state=$(echo "$bio_line" | grep -oP 'state=\K\w+' || true)
        da=$(echo "$bio_line" | grep -oP 'DA=\K[0-9.+-]+' || true)
        ach=$(echo "$bio_line" | grep -oP 'ACh=\K[0-9.+-]+' || true)
        rpe=$(echo "$bio_line" | grep -oP 'RPE=\K[0-9.e+-]+' || true)
        stdp=$(echo "$bio_line" | grep -oP 'STDP=\K[0-9]+' || true)
        ltp=$(echo "$bio_line" | grep -oP 'LTP=\K[0-9]+' || true)
        [ -n "$state" ] && log "  Plasticity state: $state"
        [ -n "$da" ] && log "  Dopamine: $da"
        [ -n "$ach" ] && log "  Acetylcholine: $ach"
        [ -n "$rpe" ] && log "  RPE: $rpe"
        [ -n "$stdp" ] && log "  STDP updates: $stdp"
        [ -n "$ltp" ] && log "  LTP events: $ltp"

        # Alert on stuck plasticity state (should progress from ACQUISITION)
        if [ -n "$state" ] && [ "$state" = "ACQUISITION" ]; then
            local opt_step
            opt_step=$(grep -oP 'Optimizer step \K[0-9]+' "$TRAINING_LOG" | tail -1 || echo "0")
            if [ "$opt_step" -gt 5000 ]; then
                alert "WARN" "Plasticity still in ACQUISITION at step $opt_step (expected CONSOLIDATION)"
            fi
        fi
    else
        log "No Bio stats output yet"
    fi

    # SNN metrics
    local snn_line
    snn_line=$(grep "SNN:" "$TRAINING_LOG" | tail -1 || true)
    if [ -n "$snn_line" ]; then
        log "$snn_line"
        local firing_rate spikes snn_sparsity silent
        firing_rate=$(echo "$snn_line" | grep -oP 'rate=\K[0-9.]+' || true)
        spikes=$(echo "$snn_line" | grep -oP 'spikes=\K[0-9]+' || true)
        snn_sparsity=$(echo "$snn_line" | grep -oP 'sparsity=\K[0-9.]+' || true)
        [ -n "$firing_rate" ] && log "  SNN firing rate: ${firing_rate}Hz"
        [ -n "$spikes" ] && log "  SNN total spikes: $spikes"
        [ -n "$snn_sparsity" ] && log "  SNN sparsity: $snn_sparsity"

        # Alert on dead SNN (0 Hz firing after significant training)
        if [ -n "$firing_rate" ] && [ "$(awk "BEGIN {print ($firing_rate == 0)}")" = "1" ]; then
            local opt_step
            opt_step=$(grep -oP 'Optimizer step \K[0-9]+' "$TRAINING_LOG" | tail -1 || echo "0")
            if [ "$opt_step" -gt 1000 ]; then
                alert "WARN" "SNN firing rate is 0 Hz at step $opt_step — network may be dead"
            fi
        fi
        # Alert on hyperactive SNN (>100 Hz mean is suspicious)
        if [ -n "$firing_rate" ] && [ "$(awk "BEGIN {print ($firing_rate > 100)}" 2>/dev/null)" = "1" ]; then
            alert "WARN" "SNN firing rate very high: ${firing_rate}Hz (possible runaway excitation)"
        fi
    else
        log "No SNN stats output yet"
    fi

    # LNN metrics
    local lnn_line
    lnn_line=$(grep "LNN:" "$TRAINING_LOG" | tail -1 || true)
    if [ -n "$lnn_line" ]; then
        log "$lnn_line"
        local tau lnn_steps
        tau=$(echo "$lnn_line" | grep -oP 'tau=\K[0-9.]+' || true)
        lnn_steps=$(echo "$lnn_line" | grep -oP 'steps=\K[0-9]+' || true)
        [ -n "$tau" ] && log "  LNN avg tau: $tau"
        [ -n "$lnn_steps" ] && log "  LNN forward steps: $lnn_steps"

        # Alert on tau collapse (should be > 0.1 for meaningful temporal dynamics)
        if [ -n "$tau" ] && [ "$(awk "BEGIN {print ($tau < 0.1)}" 2>/dev/null)" = "1" ]; then
            alert "WARN" "LNN tau collapsed to $tau (temporal dynamics lost)"
        fi
    else
        log "No LNN stats output yet"
    fi

    # Arousal / Sleep pressure
    local arousal_line
    arousal_line=$(grep "Arousal=" "$TRAINING_LOG" | grep "SleepPressure" | tail -1 || true)
    if [ -n "$arousal_line" ]; then
        log "$arousal_line"
    fi
}

#=============================================================================
# 3. UTM HEALTH (DFA / Fractal Analysis)
#=============================================================================
check_utm_health() {
    log "--- UTM HEALTH CHECK ---"

    if [ ! -f "$TRAINING_LOG" ]; then
        log "No training.log found"
        return
    fi

    # Parse [UTM] health output from training script
    local utm_line
    utm_line=$(grep "\[UTM\]" "$TRAINING_LOG" | tail -1 || true)
    if [ -n "$utm_line" ]; then
        log "$utm_line"

        local health_name dfa_alpha
        health_name=$(echo "$utm_line" | grep -oP 'health=\K\w+' || true)
        dfa_alpha=$(echo "$utm_line" | grep -oP 'DFA_α=\K[0-9.+-]+' || true)

        [ -n "$health_name" ] && log "  Training health: $health_name"
        [ -n "$dfa_alpha" ] && log "  DFA exponent: $dfa_alpha"

        case "$health_name" in
            oscillating)
                alert "CRITICAL" "UTM health: OSCILLATING (DFA α=${dfa_alpha}) — chaotic gradient dynamics"
                ;;
            drifting)
                alert "WARN" "UTM health: DRIFTING (DFA α=${dfa_alpha}) — loss diverging"
                ;;
            noisy)
                alert "WARN" "UTM health: NOISY (DFA α=${dfa_alpha}) — excessive randomness"
                ;;
            plateau)
                alert "WARN" "UTM health: PLATEAU (DFA α=${dfa_alpha}) — learning stalled"
                ;;
            optimal)
                log "  Training health OPTIMAL — pink noise dynamics"
                ;;
        esac

        if echo "$utm_line" | grep -q "GRAD_UNHEALTHY"; then
            alert "CRITICAL" "UTM reports UNHEALTHY gradients"
        fi
        if echo "$utm_line" | grep -q "EARLY_STOPPED"; then
            alert "WARN" "UTM triggered early stopping"
        fi
    else
        log "No UTM health output yet"
    fi

    # DFA-based LR adjustments
    local dfa_lr_line
    dfa_lr_line=$(grep "\[LR/DFA\]" "$TRAINING_LOG" | tail -1 || true)
    if [ -n "$dfa_lr_line" ]; then
        log "  Last DFA LR adjustment: $dfa_lr_line"
    fi

    # Pink noise DFA feedback
    local pink_line
    pink_line=$(grep "DFA→PinkNoise" "$TRAINING_LOG" | tail -1 || true)
    if [ -n "$pink_line" ]; then
        log "  Pink noise feedback: $pink_line"
    fi

    # EMA evaluation count
    local ema_count
    ema_count=$(grep -c "\[EMA\]" "$TRAINING_LOG" 2>/dev/null || echo "0")
    ema_count=$(echo "$ema_count" | tr -d '[:space:]')
    if [ "$ema_count" -gt 0 ]; then
        log "  EMA evaluations: $ema_count"
    fi
}

#=============================================================================
# 4. OUTPUT DIFFERENTIATION
#=============================================================================
check_differentiation() {
    log "--- DIFFERENTIATION CHECK ---"

    if [ ! -f "$TRAINING_LOG" ]; then
        log "No training.log found"
        return
    fi

    # output_sim (cosine similarity between outputs)
    local latest_sim
    latest_sim=$(grep -oP 'output_sim[=:]\s*\K[0-9.]+' "$TRAINING_LOG" | tail -1 || true)
    if [ -n "$latest_sim" ]; then
        log "output_sim=$latest_sim"
        if [ "$(awk "BEGIN {print ($latest_sim > $OUTPUT_SIM_THRESHOLD)}")" = "1" ]; then
            alert "CRITICAL" "Mode collapse risk! output_sim=$latest_sim (threshold: $OUTPUT_SIM_THRESHOLD)"
        fi
    else
        log "output_sim: not yet reported"
    fi

    # eff_rank (effective rank of output matrix)
    local latest_rank
    latest_rank=$(grep -oP 'eff_rank[=:]\s*\K[0-9.]+' "$TRAINING_LOG" | tail -1 || true)
    if [ -n "$latest_rank" ]; then
        log "eff_rank=$latest_rank"
        if [ "$(awk "BEGIN {print ($latest_rank < $EFF_RANK_THRESHOLD)}")" = "1" ]; then
            alert "WARN" "Poor output differentiation! eff_rank=$latest_rank (threshold: $EFF_RANK_THRESHOLD)"
        fi
    else
        log "eff_rank: not yet reported"
    fi

    # contrastive loss
    local latest_contrastive
    latest_contrastive=$(grep -oP 'contrastive[=:]\s*\K[0-9.e+-]+' "$TRAINING_LOG" | tail -1 || true)
    if [ -n "$latest_contrastive" ]; then
        log "contrastive=$latest_contrastive"
    else
        log "contrastive: not yet reported"
    fi

    # diversity
    local latest_diversity
    latest_diversity=$(grep -oP 'diversity[=:]\s*\K[0-9.e+-]+' "$TRAINING_LOG" | tail -1 || true)
    if [ -n "$latest_diversity" ]; then
        log "diversity=$latest_diversity"
    else
        log "diversity: not yet reported"
    fi
}

#=============================================================================
# 5. CHECKPOINT INTEGRITY & STAGE PROGRESSION
#=============================================================================
check_checkpoints() {
    log "--- CHECKPOINT CHECK ---"

    # Pod-side check — training/checkpointing happens on the pod
    if [ "$POD_AVAILABLE" = "1" ]; then
        local pod_ckpt_info
        pod_ckpt_info=$($POD_SSH "stat -c '%n %s %Y' ${POD_CKPT}* 2>/dev/null | sort -k3 -nr | head -3" 2>/dev/null)
        if [ -n "$pod_ckpt_info" ]; then
            log "Pod checkpoint(s):"
            local now_s newest_age=99999
            now_s=$(date +%s)
            while IFS= read -r line; do
                log "  $line"
                # Track age of newest .bin
                local ts
                ts=$(echo "$line" | awk '{print $NF}')
                local age=$(( now_s - ts ))
                if [ "$age" -lt "$newest_age" ]; then newest_age=$age; fi
            done <<< "$pod_ckpt_info"
            if [ "$newest_age" -gt 7200 ]; then
                alert "WARN" "Pod checkpoint stale: newest is $(( newest_age / 60 ))m old"
            fi
        else
            log "No pod checkpoints found at ${POD_CKPT}*"
        fi

        # Trainer-aliveness tripwire (2026-04-26 walkthrough revision):
        # athena_auto_*.bin is only written when the daemon's
        # notify_training_step counter has *increased*
        # since the last athena_auto write (gated in brain_daemon.py:save_now
        # by _save_count > _save_count_at_last_athena_auto). The daemon's
        # own 5-min auto-save loop runs even when the trainer is dead, so
        # we cannot key on athena_immersive.bin / athena_s*_step*.bin
        # mtime — those tick regardless. Combined with the wall-clock
        # 25-min cadence gate, this means a missing athena_auto for >60 min
        # reliably indicates a dead trainer. The *.bin.{snn,lnn,...}
        # sidecars are excluded because they trail the canonical save.
        local pod_auto_age
        pod_auto_age=$($POD_SSH "ls -t ${POD_CKPT%/*}/athena_auto_*.bin 2>/dev/null \
                                 | grep -v '\.snn$\|\.lnn$\|\.cnn$\|\.meta$\|\.tokenizer$\|\.mirror_neurons$\|\.executive$\|\.cortex_' \
                                 | head -1 | xargs -r stat -c '%Y' 2>/dev/null" 2>/dev/null)
        if [ -n "$pod_auto_age" ]; then
            local now_s2 auto_age
            now_s2=$(date +%s)
            auto_age=$(( now_s2 - pod_auto_age ))
            log "Latest athena_auto snapshot age: $(( auto_age / 60 ))m"
            if [ "$auto_age" -gt 3600 ]; then
                alert "CRITICAL" "Trainer appears dead: no athena_auto snapshot in $(( auto_age / 60 ))m (brain shards may still refresh — check supervisorctl status athena-training)"
            fi
        else
            alert "WARN" "No athena_auto_*.bin snapshots found on pod"
        fi
    fi

    if [ ! -d "$CHECKPOINT_DIR" ]; then
        log "Local checkpoint dir not found: $CHECKPOINT_DIR (this is expected — training is on pod)"
        return
    fi

    # Find latest .bin checkpoint
    local latest_ckpt
    latest_ckpt=$(find "$CHECKPOINT_DIR" -maxdepth 1 -name "athena_s*.bin" -not -name "*.tmp" \
                  -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -1 | awk '{print $2}' || true)

    if [ -n "$latest_ckpt" ] && [ -f "$latest_ckpt" ]; then
        local ckpt_size ckpt_size_mb ckpt_age_sec ckpt_name
        ckpt_name=$(basename "$latest_ckpt")
        ckpt_size=$(stat -c%s "$latest_ckpt" 2>/dev/null || echo 0)
        ckpt_size_mb=$(awk "BEGIN {printf \"%.0f\", $ckpt_size / 1048576}")
        ckpt_age_sec=$(( $(date +%s) - $(stat -c%Y "$latest_ckpt") ))

        log "Latest checkpoint: $ckpt_name (${ckpt_size_mb}MB, ${ckpt_age_sec}s ago)"

        # Size check
        if [ "$ckpt_size" -lt "$CHECKPOINT_MIN_SIZE" ]; then
            # Only alert if training has been running a while (check optimizer step)
            local step
            step=$(grep -oP 'Optimizer step \K[0-9]+' "$TRAINING_LOG" 2>/dev/null | tail -1 || echo "0")
            if [ "$step" -gt 1000 ]; then
                alert "WARN" "Checkpoint possibly truncated: $ckpt_name is only ${ckpt_size_mb}MB (expected >100MB)"
            fi
        fi

        # Staleness check — alert if no checkpoint in 2 hours
        if [ "$ckpt_age_sec" -gt 7200 ]; then
            alert "WARN" "Checkpoint stale: $ckpt_name is $(( ckpt_age_sec / 60 ))m old"
        fi
    else
        log "No snapshot checkpoints found yet"
    fi

    # Count total snapshots
    local snap_count
    snap_count=$(find "$CHECKPOINT_DIR" -maxdepth 1 -name "athena_s*.bin" -not -name "*.tmp" 2>/dev/null | wc -l)
    log "Total snapshots: $snap_count"

    # Canonical symlink
    local canonical="${CHECKPOINT_DIR}/athena_checkpoint.bin"
    if [ -L "$canonical" ]; then
        local target
        target=$(readlink "$canonical" 2>/dev/null || true)
        log "Canonical symlink -> $target"
    elif [ -f "$canonical" ]; then
        local can_size_mb
        can_size_mb=$(awk "BEGIN {printf \"%.0f\", $(stat -c%s "$canonical" 2>/dev/null || echo 0) / 1048576}")
        log "Canonical checkpoint: ${can_size_mb}MB (regular file, not symlink)"
    else
        log "No canonical checkpoint yet"
    fi

    # Stage progression from immersive_state.json
    if [ -f "$STATE_FILE" ]; then
        local stage step
        stage=$(python3 -c "import json; d=json.load(open('$STATE_FILE')); print(d.get('current_stage','?'))" 2>/dev/null || echo "?")
        step=$(python3 -c "import json; d=json.load(open('$STATE_FILE')); print(d.get('current_step', d.get('step','?')))" 2>/dev/null || echo "?")
        log "Stage=$stage Step=$step"
    else
        log "No immersive_state.json found"
    fi

    # List all checkpoint files with sizes
    local listing
    listing=$(ls -lhS "$CHECKPOINT_DIR"/*.bin 2>/dev/null || true)
    if [ -n "$listing" ]; then
        log "Checkpoint listing:"
        echo "$listing" | while read -r line; do
            log "  $line"
        done
    fi
}

#=============================================================================
# 6. DISK USAGE (local + pod) — disk-full silently kills the trainer
#=============================================================================
check_disk_usage() {
    log "--- DISK USAGE CHECK ---"

    # Local: filesystem holding $CHECKPOINT_DIR (or $NIMCP_DIR if not present)
    local probe_path="$CHECKPOINT_DIR"
    [ -d "$probe_path" ] || probe_path="$NIMCP_DIR"
    local local_line local_pct local_avail local_mount
    local_line=$(df -P "$probe_path" 2>/dev/null | awk 'NR==2')
    if [ -n "$local_line" ]; then
        local_pct=$(echo "$local_line" | awk '{print $5}' | tr -d '%')
        local_avail=$(echo "$local_line" | awk '{print $4}')
        local_mount=$(echo "$local_line" | awk '{print $6}')
        local_pct=${local_pct:-0}
        log "Local disk ${local_mount}: ${local_pct}% used, $(awk "BEGIN {printf \"%.1f\", $local_avail / 1048576}")GB free"
        if [ "$local_pct" -ge "$DISK_CRIT_PCT" ]; then
            alert "CRITICAL" "Local disk ${local_mount} at ${local_pct}% — purge old snapshots NOW"
        elif [ "$local_pct" -ge "$DISK_WARN_PCT" ]; then
            alert "WARN" "Local disk ${local_mount} at ${local_pct}% (warn=${DISK_WARN_PCT}%, crit=${DISK_CRIT_PCT}%)"
        fi
    fi

    # Pod: filesystem holding the checkpoint directory
    if [ "$POD_AVAILABLE" = "1" ] && [ -n "${POD_CKPT:-}" ]; then
        local pod_dir pod_line pod_pct pod_avail pod_mount
        pod_dir="${POD_CKPT%/*}"
        pod_line=$($POD_SSH "df -P '$pod_dir' 2>/dev/null | awk 'NR==2'" 2>/dev/null)
        if [ -n "$pod_line" ]; then
            pod_pct=$(echo "$pod_line" | awk '{print $5}' | tr -d '%')
            pod_avail=$(echo "$pod_line" | awk '{print $4}')
            pod_mount=$(echo "$pod_line" | awk '{print $6}')
            pod_pct=${pod_pct:-0}
            log "Pod disk ${pod_mount}: ${pod_pct}% used, $(awk "BEGIN {printf \"%.1f\", $pod_avail / 1048576}")GB free"
            if [ "$pod_pct" -ge "$DISK_CRIT_PCT" ]; then
                alert "CRITICAL" "Pod disk ${pod_mount} at ${pod_pct}% — trainer will hang on next snapshot"
            elif [ "$pod_pct" -ge "$DISK_WARN_PCT" ]; then
                alert "WARN" "Pod disk ${pod_mount} at ${pod_pct}% (warn=${DISK_WARN_PCT}%, crit=${DISK_CRIT_PCT}%)"
            fi
        fi
    fi
}

#=============================================================================
# 7. AUTO-PRUNE old snapshot families (keep N newest per family-glob)
#    Each "family" is a glob-prefixed snapshot with sibling shards
#    (.bin, .bin.snn, .bin.cnn, .bin.lnn, .bin.tokenizer, .bin.cortex_*, etc.)
#
#    Two glob patterns are pruned with the SAME N-family retention:
#      - athena_auto_*.bin  (daemon auto-saves, timestamped)
#      - athena_s1_step*.bin (trainer per-step snapshots)
#    Anything pointed at by an athena_immersive.bin* symlink is preserved
#    so the canonical resume target is never pruned out from under us.
#=============================================================================
_prune_glob() {
    # $1 = canonical glob (e.g. 'athena_auto_*.bin'), $2 = retention count.
    # Mutates the outer-scope `freed` and `deleted` counters via `eval` —
    # bash doesn't have proper return values for ints, and printing+capturing
    # would lose the exit code. Phase 2 (orphan shards) walks the matching
    # `${glob}.*` shard space and deletes any whose .bin canonical is gone.
    local glob="$1" keep="$2" canonicals total victims canon prefix sz f canonical
    canonicals=$(find "$CHECKPOINT_DIR" -maxdepth 1 -type f -name "$glob" \
                 -printf '%T@ %p\n' 2>/dev/null | sort -rn | awk '{print $2}')

    # Build the protected-set (resume targets) — files referenced by any
    # athena_immersive.bin* symlink. These are the canonical resume points
    # for both the brain daemon and the trainer; pruning them would break
    # --resume on next start.
    local immune_targets=""
    if [ -L "$CHECKPOINT_DIR/athena_immersive.bin" ]; then
        immune_targets="$(readlink -f "$CHECKPOINT_DIR/athena_immersive.bin" 2>/dev/null)"
    fi
    for sl in "$CHECKPOINT_DIR"/athena_immersive.bin.*; do
        [ -L "$sl" ] || continue
        immune_targets="$immune_targets $(readlink -f "$sl" 2>/dev/null)"
    done

    if [ -n "$canonicals" ]; then
        total=$(echo "$canonicals" | wc -l)
        if [ "$total" -gt "$keep" ]; then
            victims=$(echo "$canonicals" | tail -n +$((keep + 1)))
            while IFS= read -r canon; do
                [ -z "$canon" ] && continue
                # Skip if canonical OR any of its shards is the resume target.
                local skip=0
                for t in $immune_targets; do
                    [ -n "$t" ] || continue
                    case "$canon" in
                        "$t") skip=1; break ;;
                    esac
                done
                if [ "$skip" = "1" ]; then continue; fi
                prefix="${canon%.bin}"
                for f in "$canon" "$prefix".bin.*; do
                    [ -e "$f" ] || continue
                    sz=$(stat -c%s "$f" 2>/dev/null || echo 0)
                    rm -f -- "$f" && { freed=$((freed + sz)); deleted=$((deleted + 1)); }
                done
            done <<< "$victims"
        fi
    fi

    # Orphan shards for this glob (any "${glob}.*" whose .bin canonical
    # was already deleted manually). The .bin shards are fat — SNN
    # sidecars run ~16-20 GB each.
    while IFS= read -r f; do
        [ -e "$f" ] || continue
        canonical="${f%.*}"
        if [ ! -e "$canonical" ]; then
            sz=$(stat -c%s "$f" 2>/dev/null || echo 0)
            rm -f -- "$f" && { freed=$((freed + sz)); deleted=$((deleted + 1)); }
        fi
    done < <(find "$CHECKPOINT_DIR" -maxdepth 1 -type f -name "${glob}.*" 2>/dev/null)
}

prune_local_snapshots() {
    [ -d "$CHECKPOINT_DIR" ] || return 0
    local freed=0 deleted=0

    _prune_glob 'athena_auto_*.bin'    "$SNAPSHOT_RETENTION"
    _prune_glob 'athena_s1_step*.bin'  "$SNAPSHOT_RETENTION"

    if [ "$deleted" -gt 0 ]; then
        log "Local prune: deleted $deleted files ($(awk "BEGIN {printf \"%.1f\", $freed / 1073741824}")GB), kept newest $SNAPSHOT_RETENTION per family + orphan shards (athena_auto_, athena_s1_step)"
    fi
}

prune_pod_snapshots() {
    [ "$POD_AVAILABLE" = "1" ] || return 0
    [ -n "${POD_CKPT:-}" ] || return 0
    local pod_dir
    pod_dir="${POD_CKPT%/*}"
    local result
    result=$($POD_SSH "
        cd '$pod_dir' 2>/dev/null || exit 0
        deleted=0
        freed=0
        # Phase 1: trim canonicals to N newest, delete each victim's shards
        canonicals=\$(ls -t athena_auto_*.bin 2>/dev/null | grep -E '^athena_auto_[0-9_]+\.bin$' || true)
        if [ -n \"\$canonicals\" ]; then
            total=\$(echo \"\$canonicals\" | wc -l)
            if [ \"\$total\" -gt $SNAPSHOT_RETENTION ]; then
                victims=\$(echo \"\$canonicals\" | tail -n +$((SNAPSHOT_RETENTION + 1)))
                while IFS= read -r canon; do
                    [ -z \"\$canon\" ] && continue
                    prefix=\${canon%.bin}
                    for f in \"\$canon\" \"\$prefix\".bin.*; do
                        [ -e \"\$f\" ] || continue
                        sz=\$(stat -c%s \"\$f\" 2>/dev/null || echo 0)
                        if rm -f -- \"\$f\"; then
                            deleted=\$((deleted + 1))
                            freed=\$((freed + sz))
                        fi
                    done
                done <<< \"\$victims\"
            fi
        fi
        # Phase 2: orphaned shards (canonical .bin missing)
        for f in athena_auto_*.bin.*; do
            [ -e \"\$f\" ] || continue
            canonical=\${f%.*}
            if [ ! -e \"\$canonical\" ]; then
                sz=\$(stat -c%s \"\$f\" 2>/dev/null || echo 0)
                if rm -f -- \"\$f\"; then
                    deleted=\$((deleted + 1))
                    freed=\$((freed + sz))
                fi
            fi
        done
        echo \"\$deleted \$freed\"
    " 2>/dev/null)
    if [ -n "$result" ]; then
        local d f
        d=$(echo "$result" | awk '{print $1}')
        f=$(echo "$result" | awk '{print $2}')
        if [ -n "${d:-}" ] && [ "${d:-0}" -gt 0 ]; then
            log "Pod prune: deleted $d files ($(awk "BEGIN {printf \"%.1f\", ${f:-0} / 1073741824}")GB), kept newest $SNAPSHOT_RETENTION families"
        fi
    fi
}

#=============================================================================
# 8. AUTO-PRUNE local caches that grow unbounded (pip, uv, journal)
#=============================================================================
prune_local_caches() {
    local freed=0 sz_kb cap_kb

    # pip cache — pip never gc's; cap by total size
    if [ -d "$HOME/.cache/pip" ]; then
        sz_kb=$(du -sk "$HOME/.cache/pip" 2>/dev/null | awk '{print $1}')
        cap_kb=$((PIP_CACHE_CAP_GB * 1024 * 1024))
        if [ "${sz_kb:-0}" -gt "$cap_kb" ]; then
            rm -rf "$HOME/.cache/pip"
            freed=$((freed + sz_kb))
            log "Pruned ~/.cache/pip ($(awk "BEGIN {printf \"%.1f\", $sz_kb / 1048576}")GB > ${PIP_CACHE_CAP_GB}GB cap)"
        fi
    fi

    # uv cache — same pattern
    if [ -d "$HOME/.cache/uv" ]; then
        sz_kb=$(du -sk "$HOME/.cache/uv" 2>/dev/null | awk '{print $1}')
        cap_kb=$((UV_CACHE_CAP_GB * 1024 * 1024))
        if [ "${sz_kb:-0}" -gt "$cap_kb" ]; then
            rm -rf "$HOME/.cache/uv"
            freed=$((freed + sz_kb))
            log "Pruned ~/.cache/uv ($(awk "BEGIN {printf \"%.1f\", $sz_kb / 1048576}")GB > ${UV_CACHE_CAP_GB}GB cap)"
        fi
    fi

    # systemd journal — runs once a day at minute 0-4 of hour 4 (idempotent if cron fires multiple times)
    local hh mm
    hh=$(date +%H); mm=$(date +%M)
    if [ "$hh" = "04" ] && [ "$mm" -lt 5 ]; then
        if command -v journalctl >/dev/null 2>&1; then
            sudo -n journalctl --vacuum-size=${JOURNAL_CAP_MB}M >/dev/null 2>&1 \
                && log "Vacuumed /var/log/journal to ${JOURNAL_CAP_MB}M"
        fi
    fi
}

#=============================================================================
# 9. AUTO-PRUNE stale stage-N checkpoints (s0/s2 zombies + 0-byte corruption)
#    Live training is on a single stage at a time; once moved on, prior
#    stage's per-step .bin files are not used. The active stage is the one
#    pointed to by athena_immersive.bin's symlink target.
#=============================================================================
prune_local_stage_checkpoints() {
    [ -d "$CHECKPOINT_DIR" ] || return 0
    local active_stage=""
    if [ -L "$CHECKPOINT_DIR/athena_immersive.bin" ]; then
        local target
        target=$(readlink "$CHECKPOINT_DIR/athena_immersive.bin")
        # extract sN from athena_sN_stepXXXX.bin
        active_stage=$(echo "$target" | grep -oE 'athena_s[0-9]+' | head -1)
    fi

    # Always nuke 0-byte zombie shards (corrupt aborted writes)
    local zombies
    zombies=$(find "$CHECKPOINT_DIR" -maxdepth 1 -size 0 -name 'athena_s*' 2>/dev/null | wc -l)
    [ "$zombies" -gt 0 ] && find "$CHECKPOINT_DIR" -maxdepth 1 -size 0 -name 'athena_s*' -delete 2>/dev/null \
        && log "Deleted $zombies zero-byte zombie checkpoint shards"

    # If we know the active stage, nuke all OTHER stage families (s0/s1/s2 not in use)
    if [ -n "$active_stage" ]; then
        local removed=0 freed=0 sz
        for stage_pattern in athena_s0 athena_s1 athena_s2 athena_s3; do
            [ "$stage_pattern" = "$active_stage" ] && continue
            while IFS= read -r f; do
                [ -e "$f" ] || continue
                sz=$(stat -c%s "$f" 2>/dev/null || echo 0)
                rm -f -- "$f" && { removed=$((removed + 1)); freed=$((freed + sz)); }
            done < <(find "$CHECKPOINT_DIR" -maxdepth 1 -name "${stage_pattern}_*" 2>/dev/null)
        done
        if [ "$removed" -gt 0 ]; then
            log "Stage-prune: deleted $removed files ($(awk "BEGIN {printf \"%.1f\", $freed / 1073741824}")GB) from inactive stages (active=$active_stage)"
        fi
    fi
}

#=============================================================================
# 10. AUDIT (no-delete) — huggingface cache + sibling project dirs
#=============================================================================
check_external_dirs() {
    if [ -d "$HOME/.cache/huggingface" ]; then
        local hf_kb hf_cap_kb
        hf_kb=$(du -sk "$HOME/.cache/huggingface" 2>/dev/null | awk '{print $1}')
        hf_cap_kb=$((HF_CACHE_WARN_GB * 1024 * 1024))
        if [ "${hf_kb:-0}" -gt "$hf_cap_kb" ]; then
            alert "WARN" "~/.cache/huggingface = $(awk "BEGIN {printf \"%.0f\", $hf_kb / 1048576}")GB (> ${HF_CACHE_WARN_GB}GB) — review for unused/duplicate models"
        fi
    fi

    # Sibling project dirs (anything in $HOME that isn't nimcp/.cache/.claude/etc)
    local cap_kb=$((SIBLING_WARN_GB * 1024 * 1024))
    while IFS= read -r d; do
        [ -d "$d" ] || continue
        local sz_kb
        sz_kb=$(du -sk "$d" 2>/dev/null | awk '{print $1}')
        if [ "${sz_kb:-0}" -gt "$cap_kb" ]; then
            alert "WARN" "$d = $(awk "BEGIN {printf \"%.0f\", $sz_kb / 1048576}")GB (> ${SIBLING_WARN_GB}GB) — sibling dir bloating /"
        fi
    done < <(find "$HOME" -maxdepth 1 -mindepth 1 -type d \
              ! -name 'nimcp' ! -name '.cache' ! -name '.claude' ! -name '.local' \
              ! -name '.venv' ! -name '.config' ! -name '.ssh' ! -name '.*' 2>/dev/null)
}

#=============================================================================
# MAIN
#=============================================================================
log "========== TRAINING MONITOR RUN =========="

fetch_pod_metrics
check_health
check_bio_metrics
check_utm_health
check_differentiation
check_checkpoints
prune_local_snapshots
prune_local_stage_checkpoints
prune_local_caches
prune_pod_snapshots
check_disk_usage
check_external_dirs

log "========== END MONITOR RUN =========="
log ""
