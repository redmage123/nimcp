#!/bin/bash
#
# Hetzner Checkpoint Cleanup
#
# Keeps the latest N main .bin checkpoints (by MTIME, not filename) in each
# athena/ directory and deletes older ones along with all their sidecars.
#
# CRITICAL: sorts by mtime, NOT by step number parsed from filename. The
# training script's --resume path resets its session step counter to 0 on
# each run, so a save written NOW can have a lower step number than a save
# from yesterday. Sort-by-step-number destroys the recent saves (which
# is what happened on 2026-10-26, losing 5+ hours of training).
#
# Safety:
# - Never deletes the current athena_immersive.bin symlink target
# - Only touches athena_s0_step*.bin and its sidecars (nothing else)
# - Idempotent — no-op when count <= keep threshold
# - Logs every delete to ~/logs/checkpoint_cleanup.log
#
# Run via cron:  */30 * * * * /home/bbrelin/scripts/hetzner_cleanup_checkpoints.sh

set -eu

KEEP_LATEST=5
# Safety floor: never delete anything newer than this many minutes, regardless
# of what keep-N math says. If sort logic ever goes wrong again, recent saves
# survive long enough for a human to notice before they're destroyed.
SAFETY_FLOOR_MIN=120
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

    # Current canonical target — never delete this
    local canonical_base=""
    if [ -L "$dir/athena_immersive.bin" ]; then
        canonical_base=$(readlink "$dir/athena_immersive.bin")
    fi

    # List main .bin files SORTED BY MTIME (newest first), then take ones to delete.
    # Using NUL-separated output + null-delimited mapfile keeps whitespace-safe.
    local -a all_bins
    mapfile -d '' -t all_bins < <(
        find "$dir" -maxdepth 1 -type f -name 'athena_s0_step*.bin' \
          -printf '%T@ %p\0' 2>/dev/null \
          | sort -z -k1,1 -r -n \
          | cut -z -d' ' -f2-
    )

    if [ "${#all_bins[@]}" -le "$KEEP_LATEST" ]; then
        log "KEEP: $dir has ${#all_bins[@]} snapshots (<= keep=$KEEP_LATEST)"
        return 0
    fi

    # Report which are kept
    local kept_list=""
    for i in "${!all_bins[@]}"; do
        if [ "$i" -ge "$KEEP_LATEST" ]; then break; fi
        kept_list+="$(basename "${all_bins[$i]}") "
    done
    log "KEEP (newest $KEEP_LATEST by mtime): $kept_list"

    # Delete anything beyond the keep window
    local to_delete_count=0
    local freed_bytes=0
    local now_epoch
    now_epoch=$(date +%s)
    local safety_floor_epoch=$((now_epoch - SAFETY_FLOOR_MIN * 60))
    for i in "${!all_bins[@]}"; do
        if [ "$i" -lt "$KEEP_LATEST" ]; then continue; fi

        local main_bin="${all_bins[$i]}"
        local base_name
        base_name=$(basename "$main_bin")

        # Never delete the canonical symlink target
        if [ "$base_name" = "$canonical_base" ]; then
            log "SKIP $dir/$base_name (canonical target)"
            continue
        fi

        # Safety floor: never delete files modified in the last SAFETY_FLOOR_MIN minutes
        local main_mtime
        main_mtime=$(stat -c%Y "$main_bin" 2>/dev/null || echo 0)
        if [ "$main_mtime" -gt "$safety_floor_epoch" ]; then
            local age_min=$(( (now_epoch - main_mtime) / 60 ))
            log "SKIP $dir/$base_name (age ${age_min}min < safety floor ${SAFETY_FLOOR_MIN}min)"
            continue
        fi

        # Delete the main .bin and all its sidecars (.cnn, .snn, .lnn,
        # .cortex_*, .meta, .tokenizer, .mirror_neurons, .executive,
        # .gl_lang, .gl_multiturn, .kg, .immune, .concept_registry,
        # .knowledge, .pink_noise, .temperature).
        for f in "$main_bin" "$main_bin".*; do
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

log "=== Checkpoint cleanup run (keep latest $KEEP_LATEST by mtime) ==="

for d in "${DIRS[@]}"; do
    cleanup_dir "$d"
done

# Also report disk status
DF_LINE=$(df -h /home 2>/dev/null | tail -1)
log "DISK: $DF_LINE"
log "=== Cleanup run complete ==="
