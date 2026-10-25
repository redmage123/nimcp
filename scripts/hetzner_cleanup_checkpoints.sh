#!/bin/bash
#
# Hetzner Checkpoint Cleanup
#
# Keeps the latest N checkpoints in each athena/ directory and deletes
# older ones along with all their sidecars. Idempotent and safe:
# - Never deletes the current athena_immersive.bin symlink target.
# - Never deletes files newer than any kept checkpoint.
# - Logs every delete to /home/bbrelin/logs/checkpoint_cleanup.log.
#
# Run via cron:  */30 * * * * /home/bbrelin/scripts/hetzner_cleanup_checkpoints.sh

set -eu

KEEP_LATEST=5
LOG_FILE=/home/bbrelin/logs/checkpoint_cleanup.log
DIRS=(
    "/home/bbrelin/nimcp_checkpoints/athena"
    "/home/bbrelin/nimcp/checkpoints/athena"
)

mkdir -p "$(dirname "$LOG_FILE")"

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >> "$LOG_FILE"
}

cleanup_dir() {
    local dir="$1"
    if [ ! -d "$dir" ]; then
        log "SKIP: $dir does not exist"
        return 0
    fi

    # Find current canonical target (don't delete this one)
    local canonical_target=""
    if [ -L "$dir/athena_immersive.bin" ]; then
        canonical_target=$(readlink "$dir/athena_immersive.bin")
    fi

    # List all step-numbered main .bin files, sorted by step number descending.
    # Keep the top KEEP_LATEST, delete the rest + their sidecars.
    local -a all_steps
    mapfile -t all_steps < <(
        ls "$dir"/athena_s0_step*.bin 2>/dev/null \
          | grep -oE 'step[0-9]+' \
          | grep -oE '[0-9]+' \
          | sort -n -u -r
    )

    if [ "${#all_steps[@]}" -le "$KEEP_LATEST" ]; then
        log "KEEP: $dir has ${#all_steps[@]} snapshots (<= keep=$KEEP_LATEST)"
        return 0
    fi

    local to_delete_count=0
    local freed_bytes=0
    for i in "${!all_steps[@]}"; do
        if [ "$i" -lt "$KEEP_LATEST" ]; then
            continue
        fi
        local step="${all_steps[$i]}"
        local base="athena_s0_step${step}.bin"

        # Never delete the canonical target
        if [ "$base" = "$canonical_target" ]; then
            log "SKIP $dir/$base (canonical target)"
            continue
        fi

        # Delete the main .bin and all its sidecars
        for f in "$dir/$base" "$dir/$base".*; do
            if [ -f "$f" ]; then
                local size=0
                size=$(stat -c%s "$f" 2>/dev/null || echo 0)
                freed_bytes=$((freed_bytes + size))
                rm -f "$f"
                log "DELETE $f ($(numfmt --to=iec $size))"
                to_delete_count=$((to_delete_count + 1))
            fi
        done
    done

    if [ "$to_delete_count" -gt 0 ]; then
        log "SUMMARY: $dir — deleted $to_delete_count files, freed $(numfmt --to=iec $freed_bytes)"
    fi
}

log "=== Checkpoint cleanup run (keep latest $KEEP_LATEST) ==="

for d in "${DIRS[@]}"; do
    cleanup_dir "$d"
done

# Also report disk status
DF_LINE=$(df -h /home 2>/dev/null | tail -1)
log "DISK: $DF_LINE"
log "=== Cleanup run complete ==="
