# Post-Campaign Stage 1 + Stage 2 Regression Plan

**Date drafted**: 2026-05-05
**Trigger**: deferred-enhancement campaign W1-W8 + walkthrough fixes + canonical corpus + #309 multimodal grounding

## Why this regression matters

Roughly six weeks of changes have landed since the last clean Stage 2 baseline. The campaign touched:

- GL save format: v1/v2/v3 -> v4 (int8 quantized context vectors)
- GL public API: `get_top_phrases`, `get_modality_counts`, `ground_word_with_emotion`, `set_grounding_emotion`, `learn_language_pair`, `gl_observe_snn_spikes`, `gl_extract_audio_features`, `gl_drive_audio_comprehension`
- 10 deferred enhancements: negative grounding, dialect, active-learning, rhyme, alliteration, LRU eviction, disambiguation, compositional phrases, audio ingest, SNN spike->lexicon, checkpoint compression
- 4-layer RPC chains: `get_top_phrases`, `get_modality_counts`, `ground_word`, `set_grounding_emotion`, `learn_language_pair`
- Canonical corpus: 33-work manifest, fetcher, importer, ingest seam at Stage 1/2/3 entry + every-200-stimulus drips
- Curriculum-side valence/arousal propagation in `_train_cognitive` and `_ingest_canonical_corpus`
- MOTOR + SPATIAL grounding-event emission in `world_model_curriculum` and `submit_multimodal`

Unit tests cover individual surfaces (~250 GL tests passing). What unit tests cannot cover: the live interactions across all of these in a real multi-hour Stage run.

## Pre-flight checklist

Before pressing go:

1. **#309 agents have all landed and committed.** Specifically:
   - `_cmd_ground_word` + `_cmd_set_grounding_emotion` + `_cmd_learn_language_pair` are in `scripts/brain_daemon.py`
   - `_cmd_get_modality_counts` is in `scripts/brain_daemon.py`
   - `BrainProxy.ground_word`, `set_grounding_emotion`, `learn_language_pair`, `get_modality_counts` are in `scripts/brain_client.py`
   - Motor/Spatial emission is in `world_model_curriculum.py` and `submit_multimodal`
   - Valence/arousal propagation is in `_train_cognitive` and `_ingest_canonical_corpus`
2. **Build clean**: `cd build && make nimcp -j4 && make nimcp_python -j4` both succeed.
3. **`.so` installed**: `cp build/lib/python/nimcp.so ~/.local/lib/python3.12/site-packages/nimcp.cpython-312-x86_64-linux-gnu.so`.
4. **Daemon restarted**: `sudo systemctl restart athena-brain` AFTER the .so swap, BEFORE the run. Confirm with `systemctl status athena-brain`.
5. **Disk space**: at least 200 GB free on the working volume. Stage 1 + Stage 2 with checkpoint snapshots can pile up fast. The cron disk-watch in `monitor_training_cron.sh` will alarm but auto-prune is best-effort.
6. **Canonical corpus fetched**: `python3 tools/fetch_canonical_corpus.py` to pull all 28 PD works from Project Gutenberg. ~120 MB. Runs once.
7. **Pod quota cleared**: prune `athena_auto_*` and `predeploy_*` snapshots OLDER than 24h. Pod has a known ~80GB workspace cap.
8. **Old checkpoints archived**: any pre-campaign Stage 1/2 checkpoint goes to `/archive/pre_campaign_2026-05-05/`. They're on the old GL format and must NOT be loaded by the new daemon.

## Run configuration

### Run A: Stage 1 fresh, canonical corpus ON

```bash
cd /home/bbrelin/nimcp
sudo systemctl stop athena-brain
sudo rm -f /var/lib/athena/checkpoints/athena/.canonical_corpus_state.json
sudo systemctl start athena-brain

# Wait for daemon ready
until python3 -c "from scripts.brain_client import BrainProxy; BrainProxy().ping()" 2>/dev/null; do sleep 2; done

python3 scripts/immerse_athena.py \
    --daemon \
    --fresh \
    --stage 1 \
    --canonical-corpus \
    --canonical-restart \
    2>&1 | tee logs/regression_2026-05-05_stage1_canonical_on.log
```

Run to the existing Stage 1 success criterion (90%+ accuracy across 24 domains, or ~10K steps, whichever the script flags as "Stage 1 complete").

### Run B: Stage 1 fresh, canonical corpus OFF (control)

Same as Run A except `--no-canonical-corpus`. Run to the same step count or success criterion. Log to `logs/regression_2026-05-05_stage1_canonical_off.log`.

Purpose: isolates "did the canonical drips perturb Stage 1" from "is Stage 1 itself broken." If A passes and B fails, the corpus seam is suspect. If both fail in similar ways, look at the rest of the campaign.

Run B only needs to go far enough to compare the metric profile to Run A — half the steps is fine if comparison is the only goal. If A diverges from B significantly, run B to completion.

### Run C: Stage 2 from Run A's checkpoint, canonical corpus ON

```bash
python3 scripts/immerse_athena.py \
    --daemon \
    --resume \
    --start-stage 2 \
    --canonical-corpus \
    2>&1 | tee logs/regression_2026-05-05_stage2_canonical_on.log
```

Run to Stage 2 success criterion.

### Run D: Stage 2 from Run B's checkpoint, canonical corpus OFF (control)

Same as C except `--no-canonical-corpus`, resuming from Run B's Stage 1 output. `logs/regression_2026-05-05_stage2_canonical_off.log`.

### Optional Run E: Stage 1 + 2 from Run A's checkpoint, daemon-restart mid-run

After Run C reaches its midpoint, `sudo systemctl restart athena-brain` and `--resume`. This exercises the v4 GL save/load round-trip across a real daemon restart in a way unit tests can't. If the load fails or the lexicon comes back diminished, that's a v4 regression we need to catch before deploying.

## Acceptance criteria

### Stage 1

| Metric | Pass | Investigate | Fail |
|---|---|---|---|
| Final accuracy across 24 domains | >=85% | 70-85% | <70% |
| ANN loss trajectory | monotone-ish decline | flat for >500 steps | exploding |
| SNN homeostasis envelope | rate in [0.98, 1.02] of target | drifts slowly out and back | locked outside band |
| LNN gradient norm | <=1e3 sustained | spikes 1e3-1e5 | sustained >1e5 |
| Mode-collapse detector | clean | warns but recovers | latched |
| GL `total_groundings` | grows monotone, >5000 by end | grows but plateaus early | stuck near 0 |
| GL `total_phrases` | grows, >20 by end | flat | 0 |
| Per-modality counts (post-#309) | all 6 modalities non-zero | 4-5 of 6 non-zero | <=3 non-zero |
| Checkpoint round-trip | every save loads back identically | warning during load | load fails or corrupt |
| Wall-clock per step | within 1.5x baseline | 1.5-3x | >3x |

Compare against the historical Stage 1 baseline at step ~10050 (referenced in MEMORY.md, commit `d981f7e33`-era).

### Stage 2

Stage 2 inherits Stage 1's criteria plus:

| Stage-2-specific metric | Pass | Investigate | Fail |
|---|---|---|---|
| `phrases_evicted` counter | >0 (LRU is exercised) and <50% of total inserts | 50-80% | >80% (thrash) |
| Cognitive injection register-loss spikes | <=2x normal during canonical drip windows, recovers within 50 steps | spikes 2-5x | spikes >5x or doesn't recover |
| World-model curriculum MOTOR groundings (post-#309) | >100 by Stage 2 end | <100 but >0 | 0 |
| Spatial groundings from somato pairing (post-#309) | >100 | <100 but >0 | 0 |
| Bigram/trigram diversity in `get_top_phrases` | mix of canonical-derived + cognitive-data-derived phrases | only one source represented | empty or single-phrase domination |

## Monitoring during the run

Watch in three windows (the `monitor_training_cron.sh` covers most of it; this is the eyeball list):

1. **`tail -f logs/regression_*.log`** — primary trainer log. Watch for `WARN`, `FAIL`, `mode_collapse`, `homeostasis`.
2. **`watch -n 60 'python3 -c "from scripts.brain_client import BrainProxy; b=BrainProxy(); import json; print(json.dumps({**b.get_grounded_language_diagnostics(), \"phrases\": len(b.get_top_phrases(5)), \"modalities\": b.get_modality_counts()}, indent=2))"'`** — GL state every minute.
3. **`tail -f monitoring.log`** — the existing cron's training-status digest. Verify `working` not just `running`.

## Abort triggers

Abort the run and investigate immediately if:
- Daemon crashes (look at `journalctl -u athena-brain --since '5 min ago'`).
- SNN homeostasis stays outside [0.98, 1.02] for 200+ consecutive steps.
- ANN loss explodes >1e6.
- LNN gradient norm explodes >1e8.
- Disk pressure >90%.
- Mode collapse latches and `CollapseDetector` reports `latched=true` for 100+ steps.
- GL save fails (any "truncated" warning — that documented as MUST-INVESTIGATE in CLAUDE.md).

## Comparison & report

After Runs A-D complete:

```bash
python3 tests/regression/compare_baseline.py \
    --baseline /archive/pre_campaign_2026-05-05/stage1_baseline.json \
    --candidate logs/regression_2026-05-05_stage1_canonical_on.log \
    --output reports/post_campaign_stage1.html

python3 tests/regression/ab_compare.py \
    --a logs/regression_2026-05-05_stage1_canonical_on.log \
    --b logs/regression_2026-05-05_stage1_canonical_off.log \
    --metrics ann_loss snn_rate lnn_grad_norm gl_total_groundings phrases_evicted modality_counts \
    --output reports/post_campaign_stage1_ab.html
```

(Same for Stage 2 with the C/D logs.)

Final report: pass/fail per criterion table + the two HTML diffs + a written narrative for any criterion that landed in the Investigate column. Commit the report to `reports/post_campaign_2026-05-05/`.

## Time budget

Rough estimate at current Stage 1 pace (~13s/step from MEMORY.md, single-A100 pod):
- Run A (Stage 1, 10K steps): ~36 hours
- Run B (Stage 1 control, 5K steps): ~18 hours
- Run C (Stage 2): ~48 hours (Stage 2 typically ~1.5x Stage 1 pace)
- Run D (Stage 2 control): ~24 hours

Total: ~5 days wall clock if serial, ~2.5 days if A&B run in parallel pods then C&D in parallel.

## What to do if a regression is found

If Stage 1 fails:
1. Check Run B (control). If B passes, the failure is in the canonical corpus seam or related additions.
2. Try `--no-canonical-corpus` for the next attempt. If that passes, the seam is the suspect — review `_ingest_canonical_corpus` for obvious bugs (state file not flushing, drip count off, etc.).
3. If B also fails, bisect with `git bisect` against the 5 corpus-campaign commits + the 4 #309 multimodal commits.

If Stage 2 fails but Stage 1 passes:
1. Check the Stage-2-specific criteria first (phrase eviction thrash, register-loss spike).
2. Try Stage 2 from a Stage 1 checkpoint trained WITHOUT the canonical corpus. If THAT Stage 2 passes, the canonical material is destabilizing the Stage 2 register and we adjust weights.
