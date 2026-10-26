#!/bin/bash
#
# vast.ai Disk Monitor / Auto-Cleaner
#
# Runs every 5 min via cron. When disk usage exceeds the threshold, it
# cleans up safe-to-delete things (ollama, pip cache, old training logs,
# old /dev/shm checkpoints beyond the current canonical). Logs every
# action.
#
# INSTALLED BY: vast_pod_recovery.sh (persistent across pod reboots
# requires the recovery script to re-add the cron entry).
#
# INSTALL (handled by vast_pod_recovery.sh step 10):
#   cp /root/nimcp/scripts/vast_disk_monitor.sh /root/
#   chmod +x /root/vast_disk_monitor.sh
#   (crontab -l 2>/dev/null | grep -v vast_disk_monitor; \
#    echo "*/5 * * * * /root/vast_disk_monitor.sh") | crontab -

set -u

THRESHOLD_PCT=85  # Trigger cleanup when usage exceeds this
LOG_FILE=/tmp/vast_disk_monitor.log

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >> "$LOG_FILE"
}

# Rotate log if > 10 MB (don't eat disk ourselves)
if [ -f "$LOG_FILE" ]; then
    LOG_SIZE=$(stat -c%s "$LOG_FILE" 2>/dev/null || echo 0)
    if [ "$LOG_SIZE" -gt 10485760 ]; then
        tail -c 1048576 "$LOG_FILE" > "${LOG_FILE}.tmp" && mv "${LOG_FILE}.tmp" "$LOG_FILE"
    fi
fi

USAGE_PCT=$(df / | awk 'NR==2 {gsub("%","",$5); print $5}')

if [ "$USAGE_PCT" -lt "$THRESHOLD_PCT" ]; then
    # Silent on healthy disk — no log spam
    exit 0
fi

log "=== Disk usage ${USAGE_PCT}% exceeds threshold ${THRESHOLD_PCT}%, cleaning ==="

FREED_TOTAL=0

cleanup_target() {
    local path="$1"
    local desc="$2"
    if [ ! -e "$path" ]; then return 0; fi
    local size_before=$(du -sb "$path" 2>/dev/null | awk '{print $1}')
    rm -rf "$path" 2>/dev/null
    FREED_TOTAL=$((FREED_TOTAL + ${size_before:-0}))
    log "  Deleted $desc ($(numfmt --to=iec ${size_before:-0}))"
}

# 1. Ollama models (returns on every reboot, never needed by training)
if [ -d /workspace/ollama ]; then
    for subpath in /workspace/ollama/*; do
        [ -e "$subpath" ] && cleanup_target "$subpath" "$subpath"
    done
fi

# 2. Pip cache (rebuilt as needed)
if [ -d /root/.cache/pip ]; then
    local size_before=$(du -sb /root/.cache/pip 2>/dev/null | awk '{print $1}')
    pip cache purge >/dev/null 2>&1
    FREED_TOTAL=$((FREED_TOTAL + ${size_before:-0}))
    log "  Purged pip cache ($(numfmt --to=iec ${size_before:-0}))"
fi

# 3. Old training logs (keep current session)
# Oldest first — ONLY delete if usage still > threshold
POST_CLEANUP_PCT=$(df / | awk 'NR==2 {gsub("%","",$5); print $5}')
if [ "$POST_CLEANUP_PCT" -ge "$THRESHOLD_PCT" ]; then
    log "  Still > threshold after ollama + pip; cleaning old training logs"
    for logf in $(ls -t /tmp/training_*.log 2>/dev/null | tail -n +2); do
        cleanup_target "$logf" "$(basename $logf)"
    done
fi

# 4. Old training_attempt_*.log archives (keep last 3)
POST_LOGS_PCT=$(df / | awk 'NR==2 {gsub("%","",$5); print $5}')
if [ "$POST_LOGS_PCT" -ge "$THRESHOLD_PCT" ]; then
    log "  Still > threshold; cleaning old attempt archives (keep last 3)"
    for arch in $(ls -t /tmp/training_attempt_*.log 2>/dev/null | tail -n +4); do
        cleanup_target "$arch" "$(basename $arch)"
    done
fi

# 5. Old /dev/shm checkpoints beyond current (preserve what's live)
if [ -d /dev/shm/athena_ckpt ]; then
    local canonical=""
    if [ -L /dev/shm/athena_ckpt/athena_immersive.bin ]; then
        canonical=$(readlink /dev/shm/athena_ckpt/athena_immersive.bin)
    fi
    POST_SHM_PCT=$(df / | awk 'NR==2 {gsub("%","",$5); print $5}')
    if [ "$POST_SHM_PCT" -ge "$THRESHOLD_PCT" ] && [ -n "$canonical" ]; then
        log "  Still > threshold; pruning /dev/shm checkpoints != $canonical"
        # Delete any athena_s0_step*.bin* that isn't the canonical
        for f in /dev/shm/athena_ckpt/athena_s0_step*.bin*; do
            local base=$(basename "$f")
            if [[ "$base" != "$canonical"* ]]; then
                cleanup_target "$f" "/dev/shm/$base"
            fi
        done
    fi
fi

FINAL_PCT=$(df / | awk 'NR==2 {gsub("%","",$5); print $5}')
log "=== Cleanup done — freed $(numfmt --to=iec $FREED_TOTAL), disk now ${FINAL_PCT}% ==="

# Alert if STILL over threshold (we did everything we could, need manual help)
if [ "$FINAL_PCT" -ge "$THRESHOLD_PCT" ]; then
    log "WARNING: Still > threshold after exhausting safe cleanup. Manual intervention needed."
fi
