# Deployment Guide

**Last Updated:** 2026-04-19

## Critical Rule: Never Overwrite `.so` While Brain Is Running

Overwriting an `.so` file that is `mmap`-ed into a running process causes
**SIGSEGV** when the process next calls a relocated function. The crash
manifests at 07:15:50 on 2026-04-19 illustrated this: a direct `cp` of a
new `.so` over a live brain crashed it mid-training.

The deployment script `scripts/deploy_to_pod.sh` enforces the safe sequence:

1. Stop `athena-brain` via supervisord
2. Wait until process has exited
3. Swap `.so` files
4. Swap scripts
5. Swap stimuli
6. Start `athena-brain`

**Never bypass this script for `.so` deployments.**

## Three Deployment Modes

```bash
./scripts/deploy_to_pod.sh --full         # stops brain, swaps .so + scripts + stimuli, restarts
./scripts/deploy_to_pod.sh --scripts-only # Python scripts only (no brain restart)
./scripts/deploy_to_pod.sh --stimuli-only # just data files (no brain restart)
```

### `--full` — Brain Restart Required

Use when C code has changed:
- Python bindings (`src/bindings/python/nimcp_python.c`)
- Any `.c` or `.cu` file
- `include/*/*.h` changes
- Schema version bumps

Brain re-init takes ~15 min (load 10 GB `.snn` cache + subsystem init).

### `--scripts-only` — Daemon Restart Optional

Use when only Python has changed (training scripts, harness, stage gate).
Daemon handler changes take effect on next restart, not immediately. If
new daemon handlers are needed, restart via `supervisorctl restart
athena-brain`.

### `--stimuli-only` — No Restart

Use when only JSON stimuli changed. Harness reads from disk on each battery
run; no process restart needed.

## Pre-Deployment Checklist

1. **Local build passes**:
   ```bash
   make -C build nimcp nimcp_python -j4
   ```

2. **Regression gate passes**:
   ```bash
   bash tests/regression/run_regression.sh
   ```
   - Smoke tests must all pass
   - Baseline comparison must show no regressions >10%

3. **A/B comparison passes** (for C changes):
   ```bash
   cp build/lib/python/nimcp.so /tmp/nimcp_before.so
   # ... make changes, rebuild ...
   python3 tests/regression/ab_compare.py \
       /tmp/nimcp_before.so build/lib/python/nimcp.so \
       --accept-known-drift
   ```

4. **Notes prepared** describing the change (for deploy logs).

## Post-Deployment Verification

1. **Brain restarts successfully**:
   ```bash
   ssh -i ~/.ssh/id_ed25519_runpod -p 17653 root@74.2.96.55 \
       'supervisorctl status athena-brain'
   ```
   Should report `RUNNING` after ~15 min.

2. **New methods available** (if bindings changed):
   ```bash
   ssh ... 'python3 -c "import nimcp; b = nimcp.Brain(\"test\", 128, 10); print(hasattr(b, \"new_method\"))"'
   ```

3. **Training resumes cleanly**:
   ```bash
   ssh ... 'supervisorctl start athena-training'
   # After 5 min:
   ssh ... 'tail -n 20 /var/log/athena-training.log'
   ```

4. **Health indicators normal**:
   - SNN firing rate 1-10% (not 99% saturation, not <0.1%)
   - learn_vector step time 2-4s
   - Loss trending or stable (not diverging)

## Rollback Procedure

If deployment causes regression:

1. **Stop everything**:
   ```bash
   ssh ... 'supervisorctl stop athena-training athena-brain'
   ```

2. **Restore previous `.so`**:
   ```bash
   ssh ... 'cp /usr/local/lib/python3.12/dist-packages/nimcp.cpython-312-x86_64-linux-gnu.so.bak \
                /usr/local/lib/python3.12/dist-packages/nimcp.cpython-312-x86_64-linux-gnu.so'
   ```

   (deploy_to_pod.sh backs up the previous version before swap)

3. **Restore scripts** (backed up at `scripts/<name>.py.bak`):
   ```bash
   ssh ... 'cd /workspace/nimcp/scripts && \
            for f in *.bak; do cp "$f" "${f%.bak}"; done'
   ```

4. **Restart brain**:
   ```bash
   ssh ... 'supervisorctl start athena-brain'
   ```

5. **Document the rollback** — what broke, what was rolled back, next steps.

## Environment Variables

| Variable | Purpose | Default |
|----------|---------|---------|
| `SSH_KEY` | SSH private key path | `~/.ssh/id_ed25519_runpod` |
| `SSH_PORT` | Pod SSH port | `17653` |
| `SSH_HOST` | Pod SSH target | `root@74.2.96.55` |
| `ATHENA_STIMULI_DIR` | Override stimulus bank location | auto-detect |
| `ATHENA_TEST_DB` | Override test result DB path | `/var/lib/athena/test_results.db` |

## Common Pitfalls

1. **Forgetting to copy `.so` to site-packages** after `make nimcp_python`:
   ```bash
   cp build/lib/python/nimcp.so ~/.local/lib/python3.12/site-packages/nimcp.cpython-312-x86_64-linux-gnu.so
   ```
   Without this, `import nimcp` uses the old version.

2. **Training auto-restart on supervisord failure** — check `autorestart`
   in `/etc/supervisor/supervisord.conf`. Currently `autorestart=false` for
   training (intentional — avoids cascading restarts).

3. **Schema version mismatches** — v7 `.snn` files cannot be loaded by
   v6 binaries. Coordinate deploys of binary + checkpoint together.

4. **PCIe memory fragmentation** — after many brain restarts without
   draining GPU, memory can fragment. Reboot the pod after ~10 consecutive
   restarts.

## See Also

- [../plans/session_handoff.md](../plans/session_handoff.md) — session state
- [../architecture/30_gpu_memory.md](../architecture/30_gpu_memory.md) — GPU lifecycle
