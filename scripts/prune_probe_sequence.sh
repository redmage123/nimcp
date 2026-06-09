#!/usr/bin/env bash
# Pod-side prune-probe-rollback sequence — drops avg-53k-bindings-per-word
# to top-K, snapshots before, re-runs the collapse probe, prints before/after
# numbers. DOES NOT auto-rollback — user decides after seeing post-prune probe.
#
# Why: the comprehend trace (2026-05-29) showed every content word collapses
# onto the same ~64 broad-abstract concept cluster. Pruning bindings to
# top-K should let intent-specific bindings dominate the produce-score.
#
# Usage:
#   bash scripts/prune_probe_sequence.sh         # K=5 (recommended)
#   K=10 bash scripts/prune_probe_sequence.sh
#   DRY_RUN=1 bash scripts/prune_probe_sequence.sh    # print plan, don't execute
#
# Rollback (only if post-prune probe is worse):
#   ssh pod 'cp /workspace/nimcp/checkpoints/athena/predeploy_prune_$STAMP.bin \
#            /workspace/nimcp/checkpoints/athena/athena_immersive.bin && \
#            supervisorctl restart athena-brain'

set -euo pipefail

K="${K:-5}"
DRY_RUN="${DRY_RUN:-0}"
STAMP="$(date +%Y%m%d_%H%M)"

REPO="$(cd "$(dirname "$0")/.." && pwd)"
source "$REPO/scripts/podconfig.sh"

POD_PROBE="python3 /workspace/nimcp/scripts/probe_pod_collapse.py --both --rounds 3 --json"
SNAPSHOT_NAME="predeploy_prune_${STAMP}.bin"

echo "========================================================================"
echo " prune-probe sequence — K=$K stamp=$STAMP dry_run=$DRY_RUN"
echo "========================================================================"

if [[ "$DRY_RUN" == "1" ]]; then
  cat <<EOF
PLAN (no execution):
  1. snapshot brain → /workspace/nimcp/checkpoints/athena/$SNAPSHOT_NAME
  2. probe pre-prune  → probe_pre_prune_${STAMP}.log
  3. prune_lang_bindings(max_bindings_per_word=$K)
  4. probe post-prune → probe_post_prune_${STAMP}.log
  5. summarize: dropped count, before/after div_unique + fidelity + persistent misses

Rollback (manual):
  ssh pod 'cp /workspace/nimcp/checkpoints/athena/$SNAPSHOT_NAME \\
              /workspace/nimcp/checkpoints/athena/athena_immersive.bin && \\
              supervisorctl restart athena-brain'
EOF
  exit 0
fi

echo
echo "=== 1) snapshot brain → $SNAPSHOT_NAME ==="
$POD_SSH "cd /workspace/nimcp && python3 -c '
import sys; sys.path.insert(0, \"/workspace/nimcp/scripts\")
from brain_client import BrainProxy
b = BrainProxy()
r = b.save(\"/workspace/nimcp/checkpoints/athena/$SNAPSHOT_NAME\")
print(\"snapshot:\", r)
'"

echo
echo "=== 2) probe pre-prune (3 rounds, both modes) ==="
$POD_SSH "$POD_PROBE" > "$REPO/probe_pre_prune_${STAMP}.log" 2>&1
PRE_JSON=$(grep PROBE_JSON "$REPO/probe_pre_prune_${STAMP}.log" || echo "PROBE_JSON {}")
echo "pre:  $PRE_JSON"

echo
echo "=== 3) prune_lang_bindings(K=$K) ==="
PRUNE_OUT=$($POD_SSH "cd /workspace/nimcp && python3 -c '
import sys; sys.path.insert(0, \"/workspace/nimcp/scripts\")
from brain_client import BrainProxy
b = BrainProxy()
r = b._send({\"cmd\": \"prune_lang_bindings\", \"max_bindings_per_word\": $K})
import json; print(json.dumps(r))
'")
echo "prune result: $PRUNE_OUT"
DROPPED=$(echo "$PRUNE_OUT" | python3 -c "import sys, json; print(json.load(sys.stdin).get('dropped', '?'))")
echo "dropped: $DROPPED bindings"

echo
echo "=== 4) probe post-prune ==="
$POD_SSH "$POD_PROBE" > "$REPO/probe_post_prune_${STAMP}.log" 2>&1
POST_JSON=$(grep PROBE_JSON "$REPO/probe_post_prune_${STAMP}.log" || echo "PROBE_JSON {}")
echo "post: $POST_JSON"

echo
echo "=== 5) summary ==="
echo "pre  PROBE_JSON: $PRE_JSON"  | sed 's/PROBE_JSON //'
echo "post PROBE_JSON: $POST_JSON" | sed 's/PROBE_JSON //'
echo
echo "Compare:"
echo "  bindings dropped : $DROPPED (target: ~1.7B → ~$(($K * 32000)))"
echo "  pre  log         : probe_pre_prune_${STAMP}.log"
echo "  post log         : probe_post_prune_${STAMP}.log"
echo "  snapshot         : /workspace/nimcp/checkpoints/athena/$SNAPSHOT_NAME"
echo
echo "If post-prune is WORSE than pre, rollback:"
echo "  ssh pod 'cp /workspace/nimcp/checkpoints/athena/$SNAPSHOT_NAME \\"
echo "              /workspace/nimcp/checkpoints/athena/athena_immersive.bin && \\"
echo "              supervisorctl restart athena-brain'"
echo "(brain takes ~3 min to resume from the snapshot)"
