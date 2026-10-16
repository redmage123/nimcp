#!/usr/bin/env bash
# Maintenance-window orchestration for the Phase-2 projection soak.
# PAUSE production → run the (isolated, clobber-safe) soak → ALWAYS restart
# production. An EXIT trap guarantees production comes back even if the soak
# crashes/times out or this script is killed. Run detached on the pod:
#   nohup bash scripts/run_projection_soak_window.sh >/var/log/athena/proj_window.log 2>&1 &
set -u
echo "=== $(date -u) PROJECTION SOAK WINDOW START ==="

production_restored=0
restore_production() {
  [ "$production_restored" = 1 ] && return
  production_restored=1
  echo "=== $(date -u) RESTORING PRODUCTION ==="
  supervisorctl start athena-brain 2>&1 | grep -v pkg_resources
  # Wait for the brain socket before starting the trainer (resume can take minutes).
  for i in $(seq 1 90); do
    python3 - <<'PY' 2>/dev/null && { echo "brain socket up after ~$((i*10))s"; break; }
import socket,struct,json
s=socket.socket(socket.AF_UNIX); s.connect('/var/run/athena/brain.sock')
d=json.dumps({'cmd':'lang_status'}).encode(); s.sendall(struct.pack('>I',len(d))+d); s.recv(4)
PY
    sleep 10
  done
  supervisorctl start athena-training 2>&1 | grep -v pkg_resources
  sleep 3
  echo "--- production status after restore ---"
  supervisorctl status athena-brain athena-training 2>&1 | awk '{print $1,$2,$5,$6}'
  echo "=== $(date -u) PRODUCTION RESTORE DONE ==="
}
trap restore_production EXIT INT TERM

echo "--- pre-state ---"
supervisorctl status athena-brain athena-training 2>&1 | awk '{print $1,$2,$5,$6}'
echo "production checkpoint (must stay this after the window):"
ls -la /workspace/nimcp/checkpoints/athena/athena_immersive.bin.snn 2>/dev/null

echo "--- $(date -u) STOPPING PRODUCTION (graceful, brain saves final checkpoint up to 300s) ---"
supervisorctl stop athena-training 2>&1 | grep -v pkg_resources
supervisorctl stop athena-brain    2>&1 | grep -v pkg_resources
sleep 3
supervisorctl status athena-brain athena-training 2>&1 | awk '{print $1,$2,$5,$6}'

echo "--- $(date -u) RUNNING SOAK (production paused → full brain fits the cgroup) ---"
timeout 1500 bash /workspace/nimcp/scripts/soak_lang_projection.sh
echo "--- $(date -u) SOAK finished (exit=$?) ---"

# EXIT trap restores production now.
echo "=== $(date -u) WINDOW END (trap restores production) ==="
