#!/bin/bash
#
# vast.ai Pod Recovery Script
#
# Restores a vast.ai pod to a working training state after a reboot (or any
# event that resets the container). Handles every failure mode we hit during
# 2026-10-24/25 setup:
#
#   1. Disk-full in overlay FS (deletes ollama + pip cache + unused dirs)
#   2. PyTorch not supporting RTX 5090 (installs 2.11+cu128)
#   3. huggingface-hub version conflict (pins to 0.34.0)
#   4. libpython3.10 missing (rebuilds nimcp.so for Python 3.12)
#   5. Checkpoint lost from local disk (rsyncs from Hetzner → /dev/shm)
#   6. State file out of sync (copies from /dev/shm)
#   7. Hetzner cleanup cron missing (ensures installed)
#
# Finally starts the watchdog so training resumes from the latest Hetzner
# snapshot.
#
# Usage (run on the vast pod as root):
#   bash /root/nimcp/scripts/vast_pod_recovery.sh
#
# Idempotent — safe to re-run. Each step checks for prior success before
# re-doing work.

set -u  # catch undefined vars
# NOT set -e — we handle failures per step so partial progress is kept.

NIMCP_DIR=/root/nimcp
LOG_FILE=/tmp/pod_recovery.log
HETZNER_HOST=bbrelin@176.9.99.103
HETZNER_SSH_KEY=/root/.ssh/id_ed25519_hetzner
# The trainer's _ship_to_hetzner target. NOT ~/nimcp_checkpoints/athena — that
# is the old backup_to_hetzner.sh mirror whose athena_immersive.bin symlink is
# never advanced (resumed from step2950 instead of step6250 on 2026-10-26).
HETZNER_CKPT_DIR=/home/bbrelin/nimcp/checkpoints/athena
LOCAL_CKPT_DIR=/dev/shm/athena_ckpt
# Sidecars that must all be present (non-empty) for a checkpoint to count as
# complete. A ship cut off mid-transfer leaves a truncated .snn behind.
# Comma-separated: ssh flattens quoted argument lists into separate words.
REQUIRED_SIDECARS=".snn,.cnn,.lnn,.meta,.tokenizer"

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" | tee -a "$LOG_FILE"
}

step() {
    log "=================================================="
    log "STEP: $*"
    log "=================================================="
}

# ============================================================
# STEP 0: Disable ollama autostart (reclaims ~5-11 GB permanently)
# ============================================================
step "0. Disable ollama autostart (prevents disk-fill on reboot)"

OLLAMA_CONF=/etc/supervisor/conf.d/ollama.conf
if [ -f "$OLLAMA_CONF" ]; then
    if grep -q '^autostart=true' "$OLLAMA_CONF"; then
        log "Disabling ollama autostart in $OLLAMA_CONF"
        sed -i 's/^autostart=true/autostart=false/' "$OLLAMA_CONF"
    else
        log "Ollama autostart already disabled"
    fi
    # Kill running ollama processes (they re-materialize models in /workspace)
    pkill -9 -f /opt/supervisor-scripts/ollama.sh 2>/dev/null
    pkill -9 -f /usr/bin/ollama 2>/dev/null
    # Delete any already-materialized ollama models (user approved on 2026-10-26)
    if [ -d /workspace/ollama ]; then
        OLLAMA_SIZE=$(du -sh /workspace/ollama 2>/dev/null | awk '{print $1}')
        rm -rf /workspace/ollama/* 2>/dev/null
        log "Deleted /workspace/ollama/* (was: ${OLLAMA_SIZE:-unknown})"
    fi
else
    log "No ollama config found, skipping"
fi

# ============================================================
# STEP 1: Disk cleanup (if overlay is tight)
# ============================================================
step "1. Disk cleanup"

DISK_USED_PCT=$(df / | awk 'NR==2 {gsub("%","",$5); print $5}')
log "Current disk usage: ${DISK_USED_PCT}%"

if [ "$DISK_USED_PCT" -gt 70 ]; then
    log "Disk > 70%, cleaning up..."

    # Delete ollama if present (11 GB on vast templates)
    if [ -d /workspace/ollama ]; then
        log "  Removing /workspace/ollama/* (not used by training)"
        rm -rf /workspace/ollama/* 2>/dev/null
    fi

    # Purge pip cache
    log "  Purging pip cache"
    pip cache purge 2>&1 | tail -1 | tee -a "$LOG_FILE"

    # Delete old training logs
    log "  Deleting old /tmp/training_*.log (preserves current session)"
    find /tmp -name 'training_*.log' -mtime +1 -delete 2>/dev/null

    # Delete unused nimcp subdirs if present (we rebuild from local)
    for d in test examples docs frontend data; do
        if [ -d "$NIMCP_DIR/$d" ] && [ "$d" != "src" ] && [ "$d" != "scripts" ]; then
            # Only delete if we have minimal CMakeLists stubs
            log "  Checking $NIMCP_DIR/$d (size: $(du -sh $NIMCP_DIR/$d 2>/dev/null | cut -f1))"
        fi
    done

    DISK_USED_PCT_AFTER=$(df / | awk 'NR==2 {gsub("%","",$5); print $5}')
    log "Disk after cleanup: ${DISK_USED_PCT_AFTER}%"
else
    log "Disk OK (<= 70%), no cleanup needed"
fi

# ============================================================
# STEP 2: PyTorch with Blackwell support
# ============================================================
step "2. Verify/install PyTorch 2.11+cu128 (RTX 5090 sm_120 support)"

PYTORCH_HAS_BLACKWELL=$(python3 -c "import torch; print('sm_120' in torch.cuda.get_arch_list())" 2>/dev/null || echo "False")

if [ "$PYTORCH_HAS_BLACKWELL" = "True" ]; then
    log "PyTorch already supports sm_120, skipping install"
else
    log "PyTorch missing sm_120 support, installing 2.11+cu128..."
    pip install --upgrade torch torchvision \
        --index-url https://download.pytorch.org/whl/cu128 2>&1 | tail -3 | tee -a "$LOG_FILE"

    # Verify
    python3 -c "import torch; print(f'PyTorch: {torch.__version__}'); print(f'Archs: {torch.cuda.get_arch_list()}')" 2>&1 | tee -a "$LOG_FILE"
fi

# ============================================================
# STEP 3: Pin huggingface-hub (compat with transformers)
# ============================================================
step "3. Verify huggingface-hub version (must be <1.0 for sentence_transformers)"

HF_HUB_VERSION=$(pip show huggingface-hub 2>/dev/null | grep '^Version:' | awk '{print $2}')
log "Current huggingface-hub: ${HF_HUB_VERSION:-not installed}"

if [[ "$HF_HUB_VERSION" > "1" ]] 2>/dev/null; then
    log "huggingface-hub >= 1.0 breaks sentence_transformers, downgrading to 0.34.0"
    pip install 'huggingface-hub==0.34.0' --force-reinstall --no-deps 2>&1 | tail -2 | tee -a "$LOG_FILE"
fi

# Verify sentence_transformers can import
python3 -c "from sentence_transformers import SentenceTransformer; print('sentence_transformers OK')" 2>&1 | tail -1 | tee -a "$LOG_FILE"

# ============================================================
# STEP 4: Rebuild nimcp.so for Python 3.12 if needed
# ============================================================
step "4. Source freshness + nimcp.so rebuild if stale"

# /root/nimcp lost its .git to disk pressure on 2026-10-26 and was rebuilt
# from Oct-16/21 sources — without this session's leak + sync fixes. Keep the
# tree current from Hetzner's master checkout (GitHub TLS is flaky from vast).
# Checksum compare, no mtime copy: changed files land with a fresh mtime so
# the stale-source check below (and make) see them.
if [ -d "$NIMCP_DIR/.git" ]; then
    log "git checkout present — pulling master"
    git -C "$NIMCP_DIR" pull --ff-only 2>&1 | tail -2 | tee -a "$LOG_FILE"
else
    log "No .git — syncing tracked source from Hetzner ~/nimcp (master)"
    ssh -i "$HETZNER_SSH_KEY" -o StrictHostKeyChecking=no "$HETZNER_HOST" \
        'git -C ~/nimcp pull --ff-only >/dev/null 2>&1; git -C ~/nimcp ls-files' \
        | grep -vE '^(test|tests|docs|examples|data|frontend|\.claude|\.github)/' \
        | rsync -rlpgoDzc --files-from=- \
            -e "ssh -i $HETZNER_SSH_KEY -o StrictHostKeyChecking=no" \
            "$HETZNER_HOST:nimcp/" "$NIMCP_DIR/" 2>&1 | tail -2 | tee -a "$LOG_FILE"
fi

NIMCP_WORKS=$(python3 -c "import nimcp; print('OK')" 2>&1 | tail -1)
NIMCP_LIB="$NIMCP_DIR/build/lib/libnimcp.so.2"
STALE_SRC=""
if [ -f "$NIMCP_LIB" ]; then
    STALE_SRC=$(find "$NIMCP_DIR/src" "$NIMCP_DIR/include" -type f -newer "$NIMCP_LIB" 2>/dev/null | head -1)
fi
if [ "$NIMCP_WORKS" = "OK" ] && [ -z "$STALE_SRC" ]; then
    log "nimcp imports and no source newer than $NIMCP_LIB, skipping rebuild"
else
    [ "$NIMCP_WORKS" != "OK" ] && log "nimcp import FAILED: $NIMCP_WORKS"
    [ -n "$STALE_SRC" ] && log "Source newer than built library (e.g. $STALE_SRC)"
    log "Rebuilding nimcp from source..."

    # Need test/ examples/ dir stubs if deleted
    [ ! -d "$NIMCP_DIR/test" ] && mkdir -p "$NIMCP_DIR/test" && echo '# Stub' > "$NIMCP_DIR/test/CMakeLists.txt"
    [ ! -d "$NIMCP_DIR/examples" ] && mkdir -p "$NIMCP_DIR/examples" && echo '# Stub' > "$NIMCP_DIR/examples/CMakeLists.txt"

    # Check we have source + headers
    if [ ! -d "$NIMCP_DIR/src" ] || [ ! -d "$NIMCP_DIR/include" ]; then
        log "FATAL: Missing $NIMCP_DIR/src or $NIMCP_DIR/include — cannot rebuild"
        log "       scp from local repo: scp -r include/ install/ tools/ root@pod:$NIMCP_DIR/"
        exit 1
    fi

    mkdir -p "$NIMCP_DIR/build"
    cd "$NIMCP_DIR/build" || exit 1

    # Incremental when a configured tree exists; full configure otherwise.
    if [ ! -f CMakeCache.txt ]; then
        log "  Running cmake..."
        cmake .. -DCMAKE_BUILD_TYPE=Release > /tmp/build_cmake.log 2>&1
        if [ $? -ne 0 ]; then
            log "FATAL: cmake failed, see /tmp/build_cmake.log"
            exit 1
        fi
    fi

    log "  Running make nimcp (10-15 min full, less incremental)..."
    make nimcp -j"$(nproc)" > /tmp/build_make.log 2>&1
    if [ $? -ne 0 ]; then
        log "FATAL: make nimcp failed, see /tmp/build_make.log"
        exit 1
    fi

    log "  Running make nimcp_python..."
    make nimcp_python -j"$(nproc)" >> /tmp/build_make.log 2>&1
    if [ $? -ne 0 ]; then
        log "FATAL: make nimcp_python failed"
        exit 1
    fi

    log "  Installing built artifacts..."
    cp lib/libnimcp.so.0.9.0 /usr/local/lib/libnimcp.so.2
    cp lib/python/nimcp.so /usr/local/lib/python3.12/dist-packages/nimcp.so
    cp lib/python/nimcp.so /usr/local/lib/python3.12/dist-packages/nimcp.cpython-312-x86_64-linux-gnu.so
    ldconfig

    log "  Verifying..."
    python3 -c "import nimcp; print('nimcp rebuild OK')" 2>&1 | tail -1 | tee -a "$LOG_FILE"
fi

# ============================================================
# STEP 5: Restore checkpoint from Hetzner to /dev/shm
# ============================================================
step "5. Restore newest complete checkpoint from Hetzner to /dev/shm"

mkdir -p "$LOCAL_CKPT_DIR"

# A local canonical checkpoint with its SNN sidecar means the pod was not
# wiped — keep it (it is newer than anything on Hetzner).
if [ -s "$LOCAL_CKPT_DIR/athena_immersive.bin" ] && [ -s "$LOCAL_CKPT_DIR/athena_immersive.bin.snn" ]; then
    log "Local canonical checkpoint present ($(readlink "$LOCAL_CKPT_DIR/athena_immersive.bin")), skipping restore"
else
    # Highest step number on Hetzner whose required sidecars are all non-empty.
    PICK=$(ssh -i "$HETZNER_SSH_KEY" -o StrictHostKeyChecking=no "$HETZNER_HOST" \
           bash -s "$HETZNER_CKPT_DIR" "$REQUIRED_SIDECARS" <<'EOF'
cd "$1" 2>/dev/null || exit 0
# Every checkpoint of one brain has the same .snn size. A ship interrupted
# mid-transfer (bare rsync --partial renamed a 1.8 GB fragment over a 21 GB
# sidecar on 2026-10-26) leaves a NON-EMPTY but truncated .snn, so "exists
# and non-empty" is not enough: require >= 90% of the largest .snn present.
maxsnn=$(stat -c%s athena_s*_step*.bin.snn 2>/dev/null | sort -n | tail -1)
minsnn=$(( ${maxsnn:-0} / 10 * 9 ))
for b in $(ls athena_s*_step*.bin 2>/dev/null | sed -E 's/^(.*_step([0-9]+)\.bin)$/\2 \1/' | sort -rn | cut -d' ' -f2); do
    ok=1
    for e in ${2//,/ }; do [ -s "$b$e" ] || ok=0; done
    snnsz=$(stat -c%s "$b.snn" 2>/dev/null || echo 0)
    [ "$snnsz" -ge "$minsnn" ] || { echo "skip $b: .snn $snnsz < $minsnn (truncated)" >&2; ok=0; }
    [ "$ok" = 1 ] && { echo "$b"; break; }
done
EOF
)
    if [ -z "$PICK" ]; then
        log "WARNING: no complete checkpoint in $HETZNER_HOST:$HETZNER_CKPT_DIR — training will start fresh"
    else
        log "Newest complete checkpoint on Hetzner: $PICK — restoring"
        # --partial-dir, not --partial: an interrupted pull must not leave a
        # fragment under the final name (the local-canonical check above
        # would then skip the restore on the next run).
        rsync -a --partial-dir=.rsync-partial \
            -e "ssh -i $HETZNER_SSH_KEY -o StrictHostKeyChecking=no" \
            "$HETZNER_HOST:$HETZNER_CKPT_DIR/$PICK*" \
            "$LOCAL_CKPT_DIR/" 2>&1 | tail -3 | tee -a "$LOG_FILE"

        # Canonical symlinks for the main file and every sidecar it shipped with.
        (
            cd "$LOCAL_CKPT_DIR" || exit 1
            ln -sfn "$PICK" athena_immersive.bin
            for f in "$PICK".*; do
                [ -e "$f" ] && ln -sfn "$f" "athena_immersive.bin${f#"$PICK"}"
            done
        )

        PICK_STAGE=$(echo "$PICK" | sed -E 's/athena_s([0-9]+)_step([0-9]+)\.bin/\1/')
        PICK_STEP=$(echo "$PICK" | sed -E 's/athena_s([0-9]+)_step([0-9]+)\.bin/\2/')
        # Merge into any existing state file so curriculum flags survive; the
        # bookmark must match the restored snapshot or the trainer resumes at
        # the wrong step.
        python3 - "$LOCAL_CKPT_DIR/immersive_state.json" "$PICK_STAGE" "$PICK_STEP" "$PICK" <<'EOF'
import json, os, sys
path, stage, step, snap = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
state = {}
if os.path.exists(path):
    try:
        state = json.load(open(path))
    except Exception:
        state = {}
state.update({"stage": stage, "step": step, "snapshot": snap})
json.dump(state, open(path, "w"))
print(f"state: stage={stage} step={step} snapshot={snap}")
EOF
        log "Restored $(du -sh "$LOCAL_CKPT_DIR" 2>/dev/null | awk '{print $1}') to $LOCAL_CKPT_DIR"
    fi
fi

# ============================================================
# STEP 6: Sync state file (immersive_state.json)
# ============================================================
step "6. Sync state file"

mkdir -p "$NIMCP_DIR/checkpoints/athena"
if [ -f "$LOCAL_CKPT_DIR/immersive_state.json" ]; then
    cp "$LOCAL_CKPT_DIR/immersive_state.json" "$NIMCP_DIR/checkpoints/athena/immersive_state.json"
    log "State file: $(cat $NIMCP_DIR/checkpoints/athena/immersive_state.json | head -c 200)"
fi

# ============================================================
# STEP 7: Verify Hetzner cleanup cron (install if missing)
# ============================================================
step "7. Verify Hetzner cleanup cron"

HAS_CLEANUP=$(ssh -i "$HETZNER_SSH_KEY" -o StrictHostKeyChecking=no "$HETZNER_HOST" \
    "crontab -l 2>/dev/null | grep -c hetzner_cleanup_checkpoints" 2>/dev/null || echo 0)

if [ "$HAS_CLEANUP" -eq 0 ]; then
    log "Cleanup cron not installed on Hetzner — installing..."

    # Deploy script
    scp -i "$HETZNER_SSH_KEY" -o StrictHostKeyChecking=no \
        "$NIMCP_DIR/scripts/hetzner_cleanup_checkpoints.sh" \
        "$HETZNER_HOST:/tmp/" 2>&1 | tail -2 | tee -a "$LOG_FILE"

    ssh -i "$HETZNER_SSH_KEY" -o StrictHostKeyChecking=no "$HETZNER_HOST" '
        mkdir -p ~/scripts ~/logs
        mv /tmp/hetzner_cleanup_checkpoints.sh ~/scripts/
        chmod +x ~/scripts/hetzner_cleanup_checkpoints.sh
        (crontab -l 2>/dev/null; echo "*/30 * * * * /home/bbrelin/scripts/hetzner_cleanup_checkpoints.sh") | crontab -
    ' 2>&1 | tee -a "$LOG_FILE"
    log "Hetzner cleanup cron installed"
else
    log "Hetzner cleanup cron already installed"
fi

# ============================================================
# STEP 8: Verify SSH key for Hetzner (fail hard if missing)
# ============================================================
step "8. Verify Hetzner SSH key"

if [ ! -f "$HETZNER_SSH_KEY" ]; then
    log "FATAL: Hetzner SSH key missing at $HETZNER_SSH_KEY"
    log "       You must restore it before training can checkpoint to Hetzner"
    log "       The key should be the one authorized in Hetzner's ~/.ssh/authorized_keys"
    exit 1
fi

# Test connectivity
ssh -i "$HETZNER_SSH_KEY" -o StrictHostKeyChecking=no -o ConnectTimeout=10 "$HETZNER_HOST" 'echo CONNECTED' 2>&1 | tail -1 | tee -a "$LOG_FILE"

# ============================================================
# STEP 9: Install disk monitor cron (vast crontab is wiped on reboot)
# ============================================================
step "9. Install disk monitor cron"

if [ -f "$NIMCP_DIR/scripts/vast_disk_monitor.sh" ]; then
    cp "$NIMCP_DIR/scripts/vast_disk_monitor.sh" /root/vast_disk_monitor.sh
    chmod +x /root/vast_disk_monitor.sh
    # Remove any existing entry, then add fresh
    (crontab -l 2>/dev/null | grep -v vast_disk_monitor; \
     echo "*/5 * * * * /root/vast_disk_monitor.sh") | crontab -
    log "Disk monitor cron installed (runs every 5 min, cleans at >85%)"
    log "  Logs: /tmp/vast_disk_monitor.log"
else
    log "WARNING: $NIMCP_DIR/scripts/vast_disk_monitor.sh not found — skipping"
fi

# ============================================================
# STEP 10: Kill any stale training/watchdog, start watchdog
# ============================================================
step "10. Start watchdog"

# Kill stale processes (idempotent — pkill returns 1 if none found)
pkill -9 -f train_watchdog 2>/dev/null
pkill -9 -f immerse_athena 2>/dev/null
sleep 3

# Start the watchdog
cd "$NIMCP_DIR" || exit 1
nohup bash scripts/train_watchdog.sh 0 > /tmp/watchdog_main.log 2>&1 </dev/null &
disown
sleep 3

if pgrep -f train_watchdog > /dev/null; then
    log "Watchdog started (PID $(pgrep -f train_watchdog | head -1))"
else
    log "FATAL: Watchdog failed to start, see /tmp/watchdog_main.log"
    exit 1
fi

# ============================================================
# Final status
# ============================================================
step "RECOVERY COMPLETE"
log "Training should be loading checkpoint from $LOCAL_CKPT_DIR"
log "Monitor: tail -f /tmp/training_watchdog.log"
log "Watchdog events: tail -f /tmp/watchdog.log"
log "Hetzner cleanup: ssh $HETZNER_HOST 'tail -20 ~/logs/checkpoint_cleanup.log'"
