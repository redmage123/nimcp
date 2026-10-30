#!/bin/bash
# Finish a manual checkpoint restore on the vast pod after the files for one
# snapshot have been copied into /dev/shm/athena_ckpt (e.g. relayed through a
# laptop when the Hetzner->pod link is stalled; see vast_pod_recovery.sh):
# verify the SNN sidecar, write canonical links and the state file, start
# the watchdog.
#
# Usage: bash scripts/finalize_restore.sh athena_s0_step27373.bin [CKPT_DIR] [NIMCP_DIR]
set -u
S="${1:?snapshot name, e.g. athena_s0_step27373.bin}"
CKPT_DIR="${2:-/dev/shm/athena_ckpt}"
NIMCP_DIR="${3:-/root/nimcp}"
cd "$CKPT_DIR" || exit 1
[ -f "$S" ] || { echo "missing $CKPT_DIR/$S"; exit 1; }
snn=$(stat -c%s "$S.snn" 2>/dev/null || echo 0)
# A complete .snn is >= 90% of the largest .snn present (checkpoints of one
# brain have identical .snn sizes; a fragment is far smaller).
ref=$(stat -c%s athena_s*_step*.bin.snn 2>/dev/null | sort -n | tail -1)
[ "$snn" -gt 0 ] && [ "$snn" -ge $(( ${ref:-0} / 10 * 9 )) ] || { echo "SNN sidecar missing/incomplete ($snn bytes)"; exit 1; }
echo "files: $(ls "$S"* | wc -l), snn=$snn"
rm -f athena_immersive.bin* .local_checkpoint_shipped
for f in "$S"*; do ln -sfn "$f" "athena_immersive.bin${f#"$S"}"; done
stage=$(echo "$S" | sed -E 's/athena_s([0-9]+)_step([0-9]+)\.bin/\1/')
step=$(echo "$S" | sed -E 's/athena_s([0-9]+)_step([0-9]+)\.bin/\2/')
python3 - "$NIMCP_DIR/checkpoints/athena/immersive_state.json" "$stage" "$step" "$S" <<'EOF'
import json, os, sys, time
path, stage, step, snap = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
state = {}
if os.path.exists(path):
    try:
        state = json.load(open(path))
    except Exception:
        state = {}
state.update({"stage": stage, "step": step, "snapshot": snap,
              "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")})
os.makedirs(os.path.dirname(path), exist_ok=True)
json.dump(state, open(path, "w"), indent=2)
print("state:", json.dumps(state)[:120])
EOF
cp "$NIMCP_DIR/checkpoints/athena/immersive_state.json" "$CKPT_DIR/immersive_state.json"
echo "canonical -> $(readlink athena_immersive.bin), snn -> $(readlink athena_immersive.bin.snn)"
cd "$NIMCP_DIR" || exit 1
export CUDA_LAUNCH_BLOCKING="${CUDA_LAUNCH_BLOCKING:-0}"
nohup bash scripts/train_watchdog.sh "$stage" > /tmp/watchdog_main.log 2>&1 < /dev/null &
sleep 15
echo "watchdog: $(pgrep -fc '^bash scripts/train_watchdog.sh') trainer: $(pgrep -fc '^python3 scripts/immerse_athena.py')"
tail -2 /tmp/watchdog.log
