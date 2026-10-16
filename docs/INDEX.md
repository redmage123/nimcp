# Athena Documentation — Master Index

**Last Updated:** 2026-04-23

## New this release (2.7.0 — 2026-04-23)

| Topic | File |
|-------|------|
| Glial cell infrastructure (astrocyte / oligodendrocyte / microglia, G1–G8 wiring) | [claude/modules/glial.md](claude/modules/glial.md) |
| Neural substrate + thalamic router (F1–F9 campaign, backpressure API) | [claude/modules/substrate.md](claude/modules/substrate.md) |
| Full release notes (G-campaign + F-campaign + SIGPIPE + thalamic tuning + CUDA 12.8/Blackwell) | [../CHANGELOG.md](../CHANGELOG.md#270---2026-04-23) |

## Start Here

- [README.md](../README.md) — top-level project overview
- [CLAUDE.md](../CLAUDE.md) — quick reference (project instructions)
- [architecture/00_overview.md](architecture/00_overview.md) — architecture entry point

## Architecture (new, 2026-04)

| File | Contents |
|------|----------|
| [architecture/00_overview.md](architecture/00_overview.md) | Top-level architecture, recent changes |
| [architecture/10_training_paradigm.md](architecture/10_training_paradigm.md) | Training loop, gates, biological stability |
| [architecture/20_snn.md](architecture/20_snn.md) | SNN internals, populations, CSR, schema v7 |
| [architecture/30_gpu_memory.md](architecture/30_gpu_memory.md) | GPU memory lifecycle, CSR V2 |
| [architecture/40_cognitive_layers.md](architecture/40_cognitive_layers.md) | Memory stores, symbolic vs vector |
| [architecture/50_safety.md](architecture/50_safety.md) | Ethics, LGSS, audit, mental health |
| [architecture/60_test_infrastructure.md](architecture/60_test_infrastructure.md) | Smoke/regression/unit tests |

## API (new, 2026-04)

| File | Contents |
|------|----------|
| [api/python_api.md](api/python_api.md) | 200+ Brain methods grouped by purpose |
| [api/cognitive_battery_api.md](api/cognitive_battery_api.md) | Battery harness + 28 batteries |

## Guides (new, 2026-04)

| File | Contents |
|------|----------|
| [guides/deployment.md](guides/deployment.md) | Safe deployment procedures |

## Plans & Session Records

| File | Contents |
|------|----------|
| [plans/session_roadmap.md](plans/session_roadmap.md) | Full 7-9 week plan, Phase 0-E |
| [plans/session_progress.md](plans/session_progress.md) | Earlier session progress snapshot |
| [plans/session_handoff.md](plans/session_handoff.md) | Current session handoff state |
| [plans/cognitive_safety_battery_plan.md](plans/cognitive_safety_battery_plan.md) | Original battery plan |

## Existing Project Docs (pre-session)

| File | Contents |
|------|----------|
| [EXTERNAL_API_GUIDE.md](EXTERNAL_API_GUIDE.md) | External API guide |
| [claude/00-overview.md](claude/00-overview.md) | Project vision & motivation |
| [claude/01-build-test.md](claude/01-build-test.md) | Build & test commands |
| [claude/02-coding-standards.md](claude/02-coding-standards.md) | Coding standards & protocols |
| [claude/03-api-patterns.md](claude/03-api-patterns.md) | Key API patterns |
| [claude/04-file-organization.md](claude/04-file-organization.md) | File organization |
| [claude/05-resource-optimization.md](claude/05-resource-optimization.md) | Resource optimization |
| [claude/06-error-codes.md](claude/06-error-codes.md) | Error codes |
| [claude/07-common-issues.md](claude/07-common-issues.md) | Common issues |

## Quick Reference Commands

### Build
```bash
cd build && make nimcp nimcp_python -j4
cp build/lib/python/nimcp.so ~/.local/lib/python3.12/site-packages/nimcp.cpython-312-x86_64-linux-gnu.so
```

### Test
```bash
bash tests/regression/run_regression.sh              # full gate
bash tests/smoke/run_all.sh                          # smoke only
python3 tests/unit/test_curiosity_selector.py        # one unit test
bash tests/regression/run_regression.sh --capture    # update baseline
```

### Deploy
```bash
./scripts/deploy_to_pod.sh --full          # brain restart required
./scripts/deploy_to_pod.sh --scripts-only  # no brain restart
./scripts/deploy_to_pod.sh --stimuli-only  # no restart
```

### Battery
```bash
python3 scripts/run_full_battery.py --socket /var/run/athena/brain.sock \
    --output /var/lib/athena/reports --notes "description"
```

## Current Network Apportionment (2026-04)

Brain default neuron counts (from `scripts/brain_daemon.py` constants):

| Network | Count | Constant | Role |
|---------|------:|----------|------|
| ANN | 150,000 | `DEFAULT_ANN_NEURONS` | Teacher / gradient backbone |
| SNN | 1,800,000 | `DEFAULT_SNN_NEURONS` | **Primary learner** (R-STDP, homeostasis) |
| LNN | 512 | `DEFAULT_LNN_NEURONS` | Liquid / temporal ODE |
| CNN | ~1.8M params | (4 per-cortex) | Visual/audio/speech/somato features |
| HNN | — | wrapper on LNN layer 0 | Energy-conserving dynamics |
| FNO | — | wrapper per SNN population | Fourier spectral |

Override via: `python3 brain_daemon.py --snn-neuron-count 2500000 --lnn-neuron-count 1024`.

## Recent Changes (This Session, 2026-04-19)

### Phase 0 — Risk-Reduction Infrastructure ✅
- `tests/smoke/` — 4 files, 10 test functions
- `tests/regression/` — gate + baseline + A/B comparison
- `tests/unit/` — 11 per-module test files

### Phase A — Training Efficiency ✅ (4 of 4)
- A.1 SNN GPU transfer fix (persistent CSR)
- A.2 Deferred stubs closed (audit_log_event, text encoding)
- A.3 `scripts/curiosity_selector.py`
- A.4 `scripts/curriculum.py`

### Phase B — Substrate Improvements ✅ (3 of 3)
- B.1 `scripts/symbolic_writer.py`
- B.2 `scripts/synthesized_sensory.py`
- B.3 `scripts/gradient_accumulator.py`

### Documentation ✅
- 7 architecture docs
- 2 API refs
- 1 deployment guide
- This master index

### Not Done (Phase C-E)
- Phase C — synthetic childhood memory implantation
- Phase D — compressed replay, symbolic consultation, reconstructive recall
- Phase E — innate priors expansion

## Key Known Issues

1. **Save/load inference drift** — pre-existing; smoke tests surface, don't fix
2. **perturb_weights is a logging stub** — needs adaptive_network accessor
3. **Stage 1 slower than a human baby** — see training_paradigm.md
4. **Batch training blocked** — gradient explosion + SNN saturation

## See Also

- Session task list: use `TaskList`
- Live pod status: `ssh ... supervisorctl status`
- Training logs: `/var/log/athena-brain.log`, `/var/log/athena-training.log`
