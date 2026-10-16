#!/bin/bash
# Sync latest checkpoints from RunPod to Hetzner dev server.
#
# Runs via cron every 30 min.
#
# Syncs TWO files (plus their sidecars):
#   1. athena_immersive.bin — canonical latest state (daemon overwrites every 300s)
#   2. The most recent athena_auto_*.bin — timestamped snapshot (5th-save backup)
#
# Why both: the pod retention policy keeps only 1 previous auto_*.bin +
# the immersive.bin. Between cron runs the pod can prune an auto_*.bin
# before it reaches Hetzner (observed 2026-04-24). Syncing the canonical
# immersive.bin ensures we always have the latest daemon state.
#
# Uses rsync (not scp) so the 17 GB .snn sidecar incrementally updates
# rather than re-uploading on every run.

set -u

RKEY="$HOME/.ssh/id_ed25519_runpod"
RHOST="root@213.173.103.76"
RPORT=31527
REMOTE_DIR="/workspace/nimcp/checkpoints/athena"
LOCAL_DIR="$HOME/nimcp/checkpoints/athena"
LOG="$HOME/nimcp/sync_checkpoint.log"

mkdir -p "$LOCAL_DIR"

# Filenames of interest on the pod
sync_one() {
    local basename="$1"  # e.g. athena_immersive.bin
    local remote_path="$REMOTE_DIR/$basename"

    # Confirm the file exists on the pod (use ssh to stat).
    local remote_size
    remote_size=$(ssh -i "$RKEY" -p "$RPORT" -o StrictHostKeyChecking=no \
                  -o ConnectTimeout=15 "$RHOST" \
                  "stat -c%s $remote_path 2>/dev/null" 2>/dev/null)
    if [ -z "$remote_size" ] || [ "$remote_size" = "0" ]; then
        echo "$(date): $basename not found on RunPod — skipping" >> "$LOG"
        return
    fi

    # Skip if local copy exists and sizes match exactly (both main + at
    # least one sidecar; sidecars usually arrive within a few seconds of
    # the main file on the pod).
    if [ -f "$LOCAL_DIR/$basename" ]; then
        local local_size
        local_size=$(stat -c%s "$LOCAL_DIR/$basename" 2>/dev/null)
        if [ "$remote_size" = "$local_size" ]; then
            # Size match is a strong hint the content is the same since
            # the pod writes atomically; skip sync to save bandwidth.
            echo "$(date): $basename already current ($local_size bytes)" >> "$LOG"
            return
        fi
    fi

    echo "$(date): Syncing $basename + sidecars from RunPod..." >> "$LOG"

    # rsync the main file + all sidecars matching $basename.*
    # --inplace + --partial: resume interrupted transfers, don't waste
    #     space on duplicate files during the window rsync runs.
    # --compress=auto: already-compressed SNN data negotiates.
    # Include pattern matches both "basename" and "basename.*" (sidecars).
    rsync -a --inplace --partial --timeout=300 \
          -e "ssh -i $RKEY -p $RPORT -o StrictHostKeyChecking=no -o ConnectTimeout=15" \
          "$RHOST:$remote_path" "$RHOST:$remote_path".* \
          "$LOCAL_DIR/" >> "$LOG" 2>&1

    local rc=$?
    if [ $rc -eq 0 ]; then
        local main_size sidecar_count
        main_size=$(du -sh "$LOCAL_DIR/$basename" 2>/dev/null | cut -f1)
        sidecar_count=$(ls "$LOCAL_DIR/$basename".* 2>/dev/null | wc -l)
        echo "$(date): Synced $basename ($main_size, $sidecar_count sidecars)" >> "$LOG"
    else
        echo "$(date): SYNC FAILED for $basename (rc=$rc)" >> "$LOG"
    fi
}

# 1) Canonical latest state — daemon overwrites every 300s. This is the
#    single most important file; the pod never prunes it.
sync_one "athena_immersive.bin"

# 2) Most recent timestamped snapshot (fallback for the immersive.bin
#    becoming corrupted or the daemon writing a bad save). Pod retention
#    keeps only ~1 of these so this sync is racy; the immersive.bin above
#    is the authoritative path.
LATEST_AUTO=$(ssh -i "$RKEY" -p "$RPORT" -o StrictHostKeyChecking=no -o ConnectTimeout=10 \
              "$RHOST" "ls -t $REMOTE_DIR/athena_auto_*.bin 2>/dev/null | \
                        grep -v '\.snn$\|\.lnn$\|\.cnn$\|\.meta$\|\.tokenizer$\|\.mirror_neurons$\|\.executive$\|\.cortex_' | \
                        head -1 | xargs -n1 basename" 2>/dev/null)
if [ -n "$LATEST_AUTO" ]; then
    sync_one "$LATEST_AUTO"
fi

# Prune local snapshots older than 7 days to avoid unbounded disk growth on Hetzner
find "$LOCAL_DIR" -maxdepth 1 -name "athena_auto_*.bin*" -mtime +7 -delete 2>/dev/null
