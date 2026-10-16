#!/usr/bin/env bash
# Safe deployment of a new nimcp.so + scripts to the RunPod.
#
# NEVER overwrite nimcp.so while the brain daemon is running — the .so is mmap'd
# into the process, and overwriting it causes SIGSEGV. This script enforces:
#   0. Detect the pod's process manager (supervisord vs none)
#   1. Bundle artifacts + upload to /tmp on pod
#   2. Stop daemon + training — supervisorctl stop when supervised (honors
#      stopwaitsecs for the graceful checkpoint save), else SIGTERM by PID
#   3. Swap libnimcp (/usr/local/lib + repo tree) + python wrapper + scripts
#   4. Restart daemon + training — supervisorctl start when supervised,
#      else nohup with the captured argv
#   5. Wait for the socket to appear before returning
#
# RunPod templates run athena-brain / athena-training under supervisord with
# autorestart=true. The script detects this and drives stop/start through
# supervisorctl — killing the daemon directly would race supervisord's own
# autorestart (two daemons, one dies, false "daemon died" exit 6). On a pod
# with no supervisord it falls back to direct kill + nohup. If the script
# fails mid-way in the non-supervised path the daemon stays down until you
# intervene; under supervisord, autorestart is the safety net.
#
# Usage:
#   ./deploy_to_pod.sh                  # full deploy: .so + scripts + stimuli
#   ./deploy_to_pod.sh --scripts-only   # just scripts (no daemon restart)
#   ./deploy_to_pod.sh --stimuli-only   # just stimuli (no daemon restart)
#
# Override pod connection with env vars — see scripts/podconfig.sh.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=podconfig.sh
. "$HERE/podconfig.sh"

REPO="$(cd "$HERE/.." && pwd)"
cd "$REPO"

mode="full"
case "${1:-}" in
  --scripts-only) mode="scripts" ;;
  --stimuli-only) mode="stimuli" ;;
  "") ;;
  *) echo "unknown arg: $1" >&2; exit 2 ;;
esac

echo "=== Mode: $mode  Pod: $POD_HOST:$POD_PORT ==="

# Preflight: the brain .so must exist and be readable before we stop anything.
if [[ "$mode" == "full" ]]; then
  if [[ ! -f build/lib/python/nimcp.so ]]; then
    echo "ERROR: build/lib/python/nimcp.so missing. Run: make nimcp_python -j4" >&2
    exit 3
  fi
fi

# ----------------------------------------------------------------------------
# Pack bundle
# ----------------------------------------------------------------------------
BUNDLE=/tmp/athena_bundle.tgz
# Resolve the versioned libnimcp (e.g. libnimcp.so.0.9.0) — the symlinks on
# the pod already point at it by name, so we only need to ship the real file.
LIBNIMCP_REAL="$(readlink -f build/lib/libnimcp.so 2>/dev/null || true)"
if [[ "$mode" == "full" && ! -f "$LIBNIMCP_REAL" ]]; then
    echo "ERROR: build/lib/libnimcp.so → $LIBNIMCP_REAL missing. Run: make nimcp -j4" >&2
    exit 3
fi
# Curriculum-stack files that landed in the CE-1..19 campaign. These are
# pure-Python (no .so dependency) but the trainer needs them on the pod.
# Skip any path that doesn't exist locally so older branches still bundle.
CE_SCRIPTS=()
for f in \
    scripts/storytelling.py scripts/socratic_qa.py scripts/sibling_dialog.py \
    scripts/tom_probes.py scripts/counterfactual.py scripts/failure_replay.py \
    scripts/music_rhythm.py scripts/visual_art.py \
    scripts/canonical_corpus.py scripts/code_corpus.py scripts/streaming_ingest.py \
    scripts/claude_teacher.py; do
  [[ -f "$f" ]] && CE_SCRIPTS+=("$f")
done
[[ -d scripts/sources ]] && CE_SCRIPTS+=(scripts/sources)
CE_DATA=()
for d in \
    data/canonical_corpus data/math_corpus \
    data/physics_corpus data/chemistry_corpus data/biology_corpus \
    data/finance_corpus data/code_corpus; do
  [[ -d "$d" ]] && CE_DATA+=("$d")
done
# Loose data/*.json files: runtime lang config, per-stage vocab masks
# (Option-1 rebuild). The historic deploy bundle only synced curriculum
# subdirs + data/stimuli/ — root-level JSONs were silently dropped, which
# is why lang_runtime_default.json kept needing manual scp on 2026-05-19.
DATA_JSON=()
for f in \
    data/lang_runtime_default.json \
    data/stage_vocab_0.json data/stage_vocab_1.json data/stage_vocab_2.json; do
  [[ -f "$f" ]] && DATA_JSON+=("$f")
done

case "$mode" in
  full)
    tar czf "$BUNDLE" \
        "${LIBNIMCP_REAL#"$REPO/"}" \
        build/lib/python/nimcp.so \
        scripts/test_harness scripts/tests \
        scripts/run_full_battery.py \
        scripts/brain_daemon.py scripts/brain_client.py \
        scripts/cb_rescaled_marker.py \
        scripts/immerse_athena.py \
        scripts/caregiver_critic.py \
        "${CE_SCRIPTS[@]}" \
        data/stimuli/ \
        "${CE_DATA[@]}" \
        "${DATA_JSON[@]}"
    ;;
  scripts)
    tar czf "$BUNDLE" \
        scripts/test_harness scripts/tests \
        scripts/run_full_battery.py \
        scripts/brain_daemon.py scripts/brain_client.py \
        scripts/cb_rescaled_marker.py \
        scripts/immerse_athena.py \
        scripts/caregiver_critic.py \
        "${CE_SCRIPTS[@]}" \
        "${CE_DATA[@]}" \
        "${DATA_JSON[@]}"
    ;;
  stimuli)
    tar czf "$BUNDLE" data/stimuli/
    ;;
esac

echo "=== Uploading bundle ($(du -h "$BUNDLE" | cut -f1)) ==="
$POD_SCP "$BUNDLE" "$POD_HOST:/tmp/"

# ----------------------------------------------------------------------------
# Remote execution
# ----------------------------------------------------------------------------
if [[ "$mode" == "full" ]]; then
  echo "=== Full deploy: stop daemon, swap, restart ==="
  # Export pod paths so the heredoc can interpolate.
  export POD_DIR POD_PY_SITE POD_SOCKET POD_LOGDIR POD_CKPT
  export POD_MIN_PRODUCE_WORDS POD_MAX_PRODUCE_WORDS
  $POD_SSH "POD_DIR=$POD_DIR POD_PY_SITE=$POD_PY_SITE POD_SOCKET=$POD_SOCKET POD_LOGDIR=$POD_LOGDIR POD_CKPT=$POD_CKPT POD_MIN_PRODUCE_WORDS=$POD_MIN_PRODUCE_WORDS POD_MAX_PRODUCE_WORDS=$POD_MAX_PRODUCE_WORDS bash -s" <<'REMOTE_FULL'
set -euo pipefail

# 0) Detect whether athena-brain / athena-training run under supervisord.
# RunPod templates manage them with supervisord + autorestart=true. If we
# kill the daemon directly, supervisord races us with its own restart — the
# deploy's nohup copy and supervisord's copy both try to bind the socket,
# one dies, and the script's PID tracking trips a false "daemon died" (exit
# 6). When supervised we drive stop/start through supervisorctl: that sets
# the program's expected state (no autorestart race) and honors
# stopwaitsecs=300 for the graceful final-checkpoint save.
SUPERVISED=0
if command -v supervisorctl >/dev/null 2>&1 \
   && supervisorctl status athena-brain >/dev/null 2>&1; then
  SUPERVISED=1
  echo "Process manager: supervisord (athena-brain / athena-training)"
else
  echo "Process manager: none detected — using direct kill + nohup"
fi

if [[ "$SUPERVISED" == "1" ]]; then
  # supervisorctl stop is synchronous: it sends stopsignal (TERM) and blocks
  # until the process exits or stopwaitsecs elapses. athena-brain has
  # stopwaitsecs=300 — ample for the ~35s final-checkpoint save — and
  # athena-training has 60s. Stopping also clears each program's expected
  # RUNNING state so autorestart stays quiet while we swap files. Training
  # first (it holds socket connections), then the brain.
  echo "Stopping athena-training via supervisorctl..."
  supervisorctl stop athena-training 2>&1 || true
  echo "Stopping athena-brain via supervisorctl (graceful — final checkpoint save, up to 300s)..."
  supervisorctl stop athena-brain 2>&1 || true
  for svc in athena-brain athena-training; do
    if supervisorctl status "$svc" 2>/dev/null | grep -qE 'RUNNING|STARTING|STOPPING'; then
      echo "ERROR: $svc did not stop cleanly — aborting before file swap." >&2
      supervisorctl status "$svc" >&2
      exit 5
    fi
  done
  echo "athena-brain + athena-training stopped."
else

# 1) Find running daemon + training PIDs. Match only actual python3 processes
# (comm=python3) — bash wrappers that launched them via `bash -c "python3 ..."`
# also appear in pgrep -f output, and their argv is shell metacharacters, not
# something we can feed back to python.
find_python_pid() {
  # $1 = script basename (e.g. brain_daemon.py)
  # Prints PID of the first python3 process whose argv contains the script, or nothing.
  for pid in $(pgrep -x python3 2>/dev/null); do
    if tr '\0' ' ' < /proc/$pid/cmdline 2>/dev/null | grep -q "$1"; then
      echo "$pid"
      return
    fi
  done
}
DAEMON_PID="$(find_python_pid brain_daemon.py)"
TRAIN_PID="$(find_python_pid immerse_athena.py)"

DAEMON_ARGS=""
if [[ -n "$DAEMON_PID" ]]; then
  # /proc cmdline is NUL-separated. argv[0]=python3, argv[1]=-u, argv[2]=scripts/brain_daemon.py,
  # argv[3..]=the daemon's real args (--socket, --resume, --checkpoint, ...).
  DAEMON_ARGS="$(tr '\0' ' ' < /proc/$DAEMON_PID/cmdline | cut -d' ' -f4- | sed 's/ *$//')"
  echo "Daemon PID=$DAEMON_PID args: $DAEMON_ARGS"
else
  # No daemon running — use sensible defaults for the first deploy.
  DAEMON_ARGS="--socket $POD_SOCKET --resume --checkpoint $POD_CKPT --init-mode full"
  echo "No running daemon. Will start fresh with: $DAEMON_ARGS"
fi

TRAIN_ARGS=""
if [[ -n "$TRAIN_PID" ]]; then
  TRAIN_ARGS="$(tr '\0' ' ' < /proc/$TRAIN_PID/cmdline | cut -d' ' -f4- | sed 's/ *$//')"
  echo "Training PID=$TRAIN_PID args: $TRAIN_ARGS"
fi

# 2) Stop training first (it holds socket connections to the daemon). Then daemon.
if [[ -n "$TRAIN_PID" ]]; then
  echo "Stopping training (PID=$TRAIN_PID)..."
  kill -TERM "$TRAIN_PID" 2>/dev/null || true
  for _ in $(seq 1 20); do
    kill -0 "$TRAIN_PID" 2>/dev/null || break
    sleep 1
  done
  if kill -0 "$TRAIN_PID" 2>/dev/null; then
    echo "Training didn't exit in 20s — SIGKILL"
    kill -KILL "$TRAIN_PID" 2>/dev/null || true
  fi
fi

if [[ -n "$DAEMON_PID" ]]; then
  echo "Stopping daemon (PID=$DAEMON_PID) — this triggers final checkpoint save..."
  kill -TERM "$DAEMON_PID" 2>/dev/null || true
  # Final save can take ~15s for a 2M brain. Wait up to 120s.
  for i in $(seq 1 120); do
    kill -0 "$DAEMON_PID" 2>/dev/null || { echo "Daemon exited after ${i}s"; break; }
    sleep 1
  done
  if kill -0 "$DAEMON_PID" 2>/dev/null; then
    echo "ERROR: Daemon didn't exit in 120s — refusing to SIGKILL (would lose checkpoint)" >&2
    echo "       Inspect /proc/$DAEMON_PID and decide manually." >&2
    exit 5
  fi
fi

fi  # end: SUPERVISED stop branch

# Remove the socket if left behind (shouldn't happen with clean exit).
rm -f "$POD_SOCKET"

# 2b) Defense in depth: refuse to overwrite .so files if ANY python3 process
# still has libnimcp mmap'd. We already killed the daemon above, but a
# zombie/escaped python3 elsewhere would silently corrupt if we plowed ahead.
#
# NOTE on bash footguns: `grep ... && echo` can return 1 (when grep misses).
# Under `set -e` + `pipefail`, that exit-1 propagates through the pipeline
# and aborts the whole heredoc WITHOUT EMITTING ANYTHING — you'd see a clean
# early exit and think the install succeeded. Use an explicit `if` instead
# so each loop iteration returns 0 regardless of grep's match status.
STILL_MAPPED=""
for p in $(pgrep -x python3 2>/dev/null || true); do
    if grep -q 'libnimcp\.so' "/proc/$p/maps" 2>/dev/null; then
        STILL_MAPPED+="PID=$p "
    fi
done
if [[ -n "$STILL_MAPPED" ]]; then
    echo "ERROR: python3 process(es) still hold libnimcp.so mmap'd — aborting swap:" >&2
    echo "$STILL_MAPPED" >&2
    exit 7
fi

# 3) Extract bundle + swap files via atomic rename. Writing to a temp name
# then `mv` into place means any process that still has the OLD .so mmap'd
# keeps seeing the old inode (unlinked but kept alive by the mapping) — no
# page-level corruption. `cp` over the destination TRUNCATES the existing
# inode, which corrupts any live mmap; never use it for mmap'd files.
cd /tmp
tar xzf athena_bundle.tgz

atomic_install() {
    # $1 = source, $2 = destination.
    local src="$1" dst="$2" tmp
    tmp="$dst.deploy.$$"
    cp "$src" "$tmp"
    chmod --reference="$src" "$tmp" 2>/dev/null || chmod 0755 "$tmp"
    mv -f "$tmp" "$dst"
    echo "Installed $dst"
}

# The versioned C library — the daemon mmaps this at runtime. It is
# resolved via the ld.so cache from /usr/local/lib (the python wrapper's
# RUNPATH points at a dead build-box path), so /usr/local/lib is the
# location that ACTUALLY matters — installing only into $POD_DIR/build/lib
# is a silent no-op that leaves the daemon on the stale library after the
# restart. We install into both: /usr/local/lib (load path) and
# $POD_DIR/build/lib (kept fresh for repo-tree tooling). Both the daemon
# and any other python3 holder were confirmed stopped + unmapped in
# steps 2 / 2b above, so overwriting the live versioned file is safe.
# Symlinks (libnimcp.so, libnimcp.so.2) are left untouched; they already
# point at the versioned name (e.g. libnimcp.so.0.9.0). If the version
# tag changes between builds you'll need to update them manually.
for f in /tmp/build/lib/libnimcp.so.*; do
    [[ -f "$f" && ! -L "$f" ]] || continue
    base="$(basename "$f")"
    atomic_install "$f" "$POD_DIR/build/lib/$base"
    # Sweep stale .OLD-* siblings out of /usr/local/lib first — multiple
    # OLD copies sharing the SONAME confuse ldconfig's SONAME picker
    # (libnimcp.so.2 → wrong file). See memory: old-symlink-trap.
    for old in /usr/local/lib/libnimcp.so.*.OLD-* /usr/local/lib/libnimcp.*.OLD-*; do
        if [[ -e "$old" ]]; then
            echo "Sweeping stale $old"
            rm -f "$old"
        fi
    done
    atomic_install "$f" "/usr/local/lib/$base"
done
# Refresh the ld.so cache so the next daemon start resolves
# libnimcp.so.2 → the just-installed /usr/local/lib versioned file.
ldconfig
echo "ldconfig refreshed: $(ldconfig -p | grep 'libnimcp.so.2' || echo 'libnimcp.so.2 NOT IN CACHE')"

# The Python binding wrapper — thin .so that dlopens libnimcp.so.2.
atomic_install /tmp/build/lib/python/nimcp.so \
    "$POD_PY_SITE/nimcp.cpython-312-x86_64-linux-gnu.so"

# Scripts (bundle paths are relative to repo root — /tmp/scripts/...).
cp -r /tmp/scripts/test_harness "$POD_DIR/scripts/"
cp -r /tmp/scripts/tests        "$POD_DIR/scripts/"
cp    /tmp/scripts/run_full_battery.py "$POD_DIR/scripts/"
cp    /tmp/scripts/brain_daemon.py     "$POD_DIR/scripts/"
cp    /tmp/scripts/brain_client.py     "$POD_DIR/scripts/"
cp    /tmp/scripts/cb_rescaled_marker.py "$POD_DIR/scripts/"
cp    /tmp/scripts/immerse_athena.py   "$POD_DIR/scripts/"
[[ -f /tmp/scripts/caregiver_critic.py ]] && cp /tmp/scripts/caregiver_critic.py "$POD_DIR/scripts/"

# Curriculum-stack files (CE-1..19).
for f in /tmp/scripts/storytelling.py /tmp/scripts/socratic_qa.py \
         /tmp/scripts/sibling_dialog.py /tmp/scripts/tom_probes.py \
         /tmp/scripts/counterfactual.py /tmp/scripts/failure_replay.py \
         /tmp/scripts/music_rhythm.py /tmp/scripts/visual_art.py \
         /tmp/scripts/canonical_corpus.py /tmp/scripts/code_corpus.py \
         /tmp/scripts/streaming_ingest.py /tmp/scripts/claude_teacher.py; do
  [[ -f "$f" ]] && cp -f "$f" "$POD_DIR/scripts/"
done
if [[ -d /tmp/scripts/sources ]]; then
  rm -rf "$POD_DIR/scripts/sources"
  cp -r /tmp/scripts/sources "$POD_DIR/scripts/"
fi

# Stimuli + curriculum data corpora.
rm -rf "$POD_DIR/data/stimuli"
mkdir -p "$POD_DIR/data"
cp -r /tmp/data/stimuli "$POD_DIR/data/"
for d in canonical_corpus math_corpus physics_corpus chemistry_corpus \
         biology_corpus finance_corpus code_corpus; do
  if [[ -d "/tmp/data/$d" ]]; then
    rm -rf "$POD_DIR/data/$d"
    cp -r "/tmp/data/$d" "$POD_DIR/data/"
  fi
done

# Loose data/*.json files (Option-1 rebuild: lang_runtime_default + stage_vocab_*).
for f in /tmp/data/lang_runtime_default.json \
         /tmp/data/stage_vocab_0.json /tmp/data/stage_vocab_1.json \
         /tmp/data/stage_vocab_2.json; do
  [[ -f "$f" ]] && cp -f "$f" "$POD_DIR/data/"
done

# 4) Relaunch the daemon, then 5) wait for it to come ready.
mkdir -p "$POD_LOGDIR" "$(dirname "$POD_SOCKET")"
cd "$POD_DIR"

if [[ "$SUPERVISED" == "1" ]]; then
  # supervisord owns the process + its logs (50MB×10 rotation, brain.stderr).
  # `supervisorctl start` blocks up to startsecs (30s) — the 2M-brain init
  # runs far longer, so a returned "started" just means it launched OK; we
  # still poll for the socket. A crash surfaces as FATAL/BACKOFF/EXITED.
  echo "Starting athena-brain via supervisorctl..."
  supervisorctl start athena-brain 2>&1 || true
  echo "Waiting for daemon ready (socket)..."
  for i in $(seq 1 180); do
    if [[ -S "$POD_SOCKET" ]]; then
      echo "Daemon socket up after ~$((i * 10))s"
      break
    fi
    if supervisorctl status athena-brain 2>/dev/null | grep -qE 'FATAL|BACKOFF|EXITED'; then
      echo "ERROR: athena-brain crashed during init:" >&2
      supervisorctl status athena-brain >&2
      tail -40 "$POD_LOGDIR/brain.stderr" >&2 2>/dev/null || true
      exit 6
    fi
    sleep 10
  done
  if [[ ! -S "$POD_SOCKET" ]]; then
    echo "WARNING: Daemon still initializing after 30 min — supervisord keeps it running."
    echo "         Watch: supervisorctl tail -f athena-brain stderr"
  fi
else
  # Rotate previous logs so the new run starts clean.
  ts="$(date +%m-%d-%H%M-deploy)"
  for f in daemon.stdout daemon.stderr training.stdout training.stderr; do
    [[ -f "$POD_LOGDIR/$f" ]] && mv "$POD_LOGDIR/$f" "$POD_LOGDIR/$f.$ts"
  done

  echo "Starting daemon: python3 -u scripts/brain_daemon.py $DAEMON_ARGS"
  # shellcheck disable=SC2086
  nohup python3 -u scripts/brain_daemon.py $DAEMON_ARGS \
      > "$POD_LOGDIR/daemon.stdout" 2> "$POD_LOGDIR/daemon.stderr" &
  NEW_DAEMON_PID=$!
  disown || true
  echo "Daemon started PID=$NEW_DAEMON_PID"

  # 5) Wait for socket + "Brain daemon ready" in stderr. 2M brain init can take 15min.
  echo "Waiting for daemon ready (socket + log marker)..."
  for i in $(seq 1 120); do
    if [[ -S "$POD_SOCKET" ]] && grep -q "Brain daemon ready" "$POD_LOGDIR/daemon.stderr" 2>/dev/null; then
      echo "Daemon READY after ${i}*10s"
      break
    fi
    # Detect early crash.
    if ! kill -0 "$NEW_DAEMON_PID" 2>/dev/null; then
      echo "ERROR: Daemon died during init. Tail of stderr:" >&2
      tail -40 "$POD_LOGDIR/daemon.stderr" >&2 || true
      exit 6
    fi
    sleep 10
  done
  if [[ ! -S "$POD_SOCKET" ]]; then
    echo "WARNING: Daemon still initializing after 20 min — letting it continue in background"
    echo "         Watch: tail -f $POD_LOGDIR/daemon.stderr"
  fi
fi

# 5b) Activate the SNN language-bridge produce length floor (min_produce_words)
# so the cascade self-train bigram/trigram path isn't starved by the greedy
# decoder's 1-word confidence-floor collapse (commit 0d993d656). The 2M brain
# can take ~30min to finish init — longer than the ready-wait above — so this
# runs as a detached poller that applies the set_length_control RPC whenever
# the daemon socket comes live (polling up to 60min), then exits. Non-fatal:
# a failure here only means the cascade keeps its legacy behavior until the
# RPC is sent manually. Set POD_MIN_PRODUCE_WORDS=0 to skip entirely.
if [[ "${POD_MIN_PRODUCE_WORDS:-0}" != "0" ]]; then
  echo "Scheduling set_length_control(min=$POD_MIN_PRODUCE_WORDS, max=$POD_MAX_PRODUCE_WORDS) poller..."
  cat > /tmp/apply_length_control.py <<'PYEOF'
import socket, struct, json, sys, time, os
sock_path, minw, maxw = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
deadline = time.time() + 3600
while time.time() < deadline:
    if os.path.exists(sock_path):
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.settimeout(180)
            s.connect(sock_path)
            req = json.dumps({"cmd": "set_length_control",
                              "min_words": minw, "max_words": maxw}).encode()
            s.sendall(struct.pack(">I", len(req)) + req)
            hdr = b""
            while len(hdr) < 4:
                hdr += s.recv(4 - len(hdr))
            n = struct.unpack(">I", hdr)[0]
            buf = b""
            while len(buf) < n:
                buf += s.recv(min(65536, n - len(buf)))
            s.close()
            resp = json.loads(buf.decode())
            print(f"[apply_length_control] set_length_control -> {resp}", flush=True)
            sys.exit(0 if resp.get("ok") else 1)
        except Exception as e:
            print(f"[apply_length_control] socket up but RPC retry: {e}", flush=True)
    time.sleep(15)
print("[apply_length_control] timed out waiting for daemon socket", flush=True)
sys.exit(1)
PYEOF
  nohup python3 /tmp/apply_length_control.py "$POD_SOCKET" \
      "$POD_MIN_PRODUCE_WORDS" "$POD_MAX_PRODUCE_WORDS" \
      > "$POD_LOGDIR/apply_length_control.log" 2>&1 &
  disown || true
  echo "  poller PID=$! — log: $POD_LOGDIR/apply_length_control.log"
else
  echo "set_length_control: skipped (POD_MIN_PRODUCE_WORDS=0)"
fi

# 6) Relaunch training. immerse_athena.py connects to the brain socket with
# retry, so it can start before the daemon finishes init.
if [[ "$SUPERVISED" == "1" ]]; then
  echo "Starting athena-training via supervisorctl..."
  supervisorctl start athena-training 2>&1 || true
elif [[ -n "$TRAIN_ARGS" ]]; then
  echo "Restarting training: python3 -u scripts/immerse_athena.py $TRAIN_ARGS"
  # shellcheck disable=SC2086
  nohup python3 -u scripts/immerse_athena.py $TRAIN_ARGS \
      > "$POD_LOGDIR/training.stdout" 2> "$POD_LOGDIR/training.stderr" &
  disown || true
  echo "Training PID=$!"
else
  echo "Training was not running before deploy — not restarting."
fi

echo "=== Full deploy complete ==="
REMOTE_FULL

elif [[ "$mode" == "scripts" ]]; then
  echo "=== Scripts-only deploy (no daemon restart) ==="
  export POD_DIR
  $POD_SSH "POD_DIR=$POD_DIR bash -s" <<'REMOTE_SCRIPTS'
set -euo pipefail
cd /tmp && tar xzf athena_bundle.tgz
cp -r /tmp/scripts/test_harness "$POD_DIR/scripts/"
cp -r /tmp/scripts/tests        "$POD_DIR/scripts/"
cp    /tmp/scripts/run_full_battery.py "$POD_DIR/scripts/"
cp    /tmp/scripts/brain_daemon.py     "$POD_DIR/scripts/"
cp    /tmp/scripts/brain_client.py     "$POD_DIR/scripts/"
cp    /tmp/scripts/cb_rescaled_marker.py "$POD_DIR/scripts/"
cp    /tmp/scripts/immerse_athena.py   "$POD_DIR/scripts/"
[[ -f /tmp/scripts/caregiver_critic.py ]] && cp /tmp/scripts/caregiver_critic.py "$POD_DIR/scripts/"
# Curriculum-stack files (CE-1..19). Ship whatever's in the bundle —
# old branches without these files won't have them packed, and `cp -f`
# is a no-op on missing sources thanks to the for-loop existence guard.
for f in /tmp/scripts/storytelling.py /tmp/scripts/socratic_qa.py \
         /tmp/scripts/sibling_dialog.py /tmp/scripts/tom_probes.py \
         /tmp/scripts/counterfactual.py /tmp/scripts/failure_replay.py \
         /tmp/scripts/music_rhythm.py /tmp/scripts/visual_art.py \
         /tmp/scripts/canonical_corpus.py /tmp/scripts/code_corpus.py \
         /tmp/scripts/streaming_ingest.py /tmp/scripts/claude_teacher.py; do
  [[ -f "$f" ]] && cp -f "$f" "$POD_DIR/scripts/"
done
if [[ -d /tmp/scripts/sources ]]; then
  rm -rf "$POD_DIR/scripts/sources"
  cp -r /tmp/scripts/sources "$POD_DIR/scripts/"
fi
# Curriculum data corpora — replace each whole tree atomically.
mkdir -p "$POD_DIR/data"
for d in canonical_corpus math_corpus physics_corpus chemistry_corpus \
         biology_corpus finance_corpus code_corpus; do
  if [[ -d "/tmp/data/$d" ]]; then
    rm -rf "$POD_DIR/data/$d"
    cp -r "/tmp/data/$d" "$POD_DIR/data/"
  fi
done
echo "=== Scripts updated. Trainer needs a restart to pick up wiring changes. ==="
REMOTE_SCRIPTS

elif [[ "$mode" == "stimuli" ]]; then
  echo "=== Stimuli-only deploy ==="
  export POD_DIR
  $POD_SSH "POD_DIR=$POD_DIR bash -s" <<'REMOTE_STIM'
set -euo pipefail
cd /tmp && tar xzf athena_bundle.tgz
rm -rf "$POD_DIR/data/stimuli"
mkdir -p "$POD_DIR/data"
cp -r /tmp/data/stimuli "$POD_DIR/data/"
echo "=== Stimuli updated (no restart needed) ==="
REMOTE_STIM
fi

echo "=== Deployment done ==="
