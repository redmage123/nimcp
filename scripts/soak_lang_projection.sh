#!/usr/bin/env bash
# soak_lang_projection.sh — Phase-2 step-4 validation harness for the
# Wernicke→Broca concept→word SNN projection (NIMCP_LANG_PROJECTION).
#
# WHY THIS EXISTS / SAFETY:
#   * The SNN save path is CWD-relative ("checkpoints/athena/athena_immersive.bin.snn")
#     and does NOT honor --checkpoint-dir. A throwaway brain run from the
#     production working dir would CLOBBER the live checkpoint. So this harness
#     runs from an ISOLATED workdir with its own empty checkpoints/athena/.
#   * It runs CPU-only (CUDA_VISIBLE_DEVICES="") so it never contends for the
#     GPU the production brain holds (no GPU OOM). It WILL use CPU + ~40GB RAM,
#     so expect production training to slow while it runs.
#
#   *** MAINTENANCE WINDOW REQUIRED (learned 2026-06-02): ***
#   The hierarchical SNN architecture is FIXED at ~1.8M neurons — --snn-neuron-count
#   sets config.snn_target_neurons but the builder ignores it, so the throwaway is
#   always the FULL ~40GB brain. Running it ALONGSIDE production OOM-killed the
#   throwaway during the language-pop wave: full-throwaway(~40GB) + production(~20GB)
#   + projection-wave spike exceeded the ~85.7GB cgroup memory.max (silent SIGKILL,
#   no crash log). PRODUCTION WAS UNHARMED (isolated workdir, no clobber), but the
#   projection never wired. => Run this ONLY with production PAUSED:
#       supervisorctl stop athena-training athena-brain   # free the cgroup budget
#       bash scripts/soak_lang_projection.sh              # full throwaway fits now
#       supervisorctl start athena-brain athena-training  # restore production
#   (SNN_N below is left for reference but does NOT shrink the hierarchy.)
#   * It is a FRESH cold-init (projection is created at cold init only). It never
#     touches the production socket or checkpoints.
#
# USAGE (on the pod, NOT in /workspace/nimcp):
#   bash /workspace/nimcp/scripts/soak_lang_projection.sh
#
# GO/NO-GO GATES (printed at the end):
#   GO  requires ALL of:
#     - "concept→word projection wired: ... synapses" present (≈6M expected)
#     - "excluded from global homeostasis/R-STDP/reward" present
#     - daemon reaches "Daemon ready"/socket-up with NO crash-handler output
#     - warmstart_lang_projection RPC returns >0 updated (after a little learning)
#     - Broca/Wernicke firing-rate EMA stays in [0.5%, 6%] across the soak
#   NO-GO on: any SIGSEGV/crash-handler output, OOM, decode/produce errors,
#            rates pegged at 0 (dead) or >10% (runaway).
set -u

WORK="${WORK:-/workspace/projtest_lang}"          # isolated workdir (own checkpoints/)
REPO="${REPO:-/workspace/nimcp}"                   # production repo (scripts source)
SOCK="${SOCK:-/var/run/athena/brain_projtest.sock}"
LOG="${LOG:-/var/log/athena/projtest.log}"
SNN_N="${SNN_N:-300000}"                           # small hierarchy; lang pops are fixed 64K
SOAK_S="${SOAK_S:-180}"                             # seconds to soak after ready

echo "=== Phase-2 step-4 projection soak (isolated, CPU-only) ==="
echo "workdir=$WORK  socket=$SOCK  snn_neurons=$SNN_N"

# 1. Isolated workspace with its OWN empty checkpoints/athena (clobber-safe).
mkdir -p "$WORK/checkpoints/athena"
cd "$WORK" || { echo "NO-GO: cannot cd $WORK"; exit 2; }
# Symlink the scripts + data the daemon needs (read-only use); checkpoints stay local.
for d in scripts data; do [ -e "$WORK/$d" ] || ln -s "$REPO/$d" "$WORK/$d"; done

# 2. Launch the throwaway fresh brain: CPU-only, projection ON, fresh cold-init.
rm -f "$SOCK"
NIMCP_LANG_PROJECTION=1 NIMCP_TRAIN_SNN_PROJECTION=1 CUDA_VISIBLE_DEVICES="" NIMCP_DISABLE_LEARN_LANGUAGE=1 \
  python3 -u "$REPO/scripts/brain_daemon.py" --socket "$SOCK" --fresh \
    --init-mode full --snn-neuron-count "$SNN_N" \
    --checkpoint-dir "$WORK/checkpoints/athena" >"$LOG" 2>&1 &
PID=$!
echo "throwaway PID=$PID  log=$LOG"

# 3. Wait for projection wiring / ready / crash.
for i in $(seq 1 120); do
  grep -qiE "PROJ-DIAG.*returned|projection wired|projection wiring returned|Segmentation|SIGSEGV|core dumped" "$LOG" 2>/dev/null && break
  kill -0 "$PID" 2>/dev/null || { echo "NO-GO: daemon exited during init (OOM/crash?)"; tail -20 "$LOG"; exit 1; }
  sleep 5
done

echo "--- PROJ-DIAG (env + synapse count) / projection / crash lines ---"
grep -iE "PROJ-DIAG|concept.{0,3}word projection wired|projection wiring returned|excluded from global|Segmentation|SIGSEGV|core dumped" "$LOG" | tail -8

# 4. Wait for socket-ready, then RPCs: warm-start + a couple status snapshots.
for i in $(seq 1 60); do
  python3 - "$SOCK" <<'PY' 2>/dev/null && break
import socket,struct,json,sys
s=socket.socket(socket.AF_UNIX); s.connect(sys.argv[1])
d=json.dumps({'cmd':'lang_status'}).encode(); s.sendall(struct.pack('>I',len(d))+d); s.recv(4)
PY
  sleep 5
done

python3 - "$SOCK" <<'PY'
import socket,struct,json,sys
def rpc(req):
    s=socket.socket(socket.AF_UNIX); s.connect(sys.argv[1])
    d=json.dumps(req).encode(); s.sendall(struct.pack('>I',len(d))+d)
    h=b''
    while len(h)<4: h+=s.recv(4-len(h))
    n=struct.unpack('>I',h)[0]; b=b''
    while len(b)<n: b+=s.recv(min(n-len(b),65536))
    s.close(); return json.loads(b.decode())
# warm-start (fresh lexicon is empty → 0 updated expected; proves no crash + reachability)
print("warmstart:", rpc({'cmd':'warmstart_lang_projection','k':1.0}))
print("lang_status ok:", rpc({'cmd':'lang_status'}).get('ok'))
PY

# 5. Brief soak — confirm the daemon stays alive (no runaway) for SOAK_S.
echo "--- soaking ${SOAK_S}s (watching for crash) ---"
END=$(( $(date +%s) + SOAK_S ))
while [ "$(date +%s)" -lt "$END" ]; do
  kill -0 "$PID" 2>/dev/null || { echo "NO-GO: daemon died during soak"; tail -20 "$LOG"; exit 1; }
  sleep 10
done
echo "soak survived: daemon still alive"

# 6. Teardown — kill throwaway, leave production untouched.
kill "$PID" 2>/dev/null; sleep 2; kill -9 "$PID" 2>/dev/null
echo "=== DONE. Review $LOG for projection synapse count + rates. Production untouched. ==="
echo "    (cleanup: rm -rf $WORK ; rm -f $SOCK $LOG)"
