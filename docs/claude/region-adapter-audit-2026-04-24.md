# Brain-Region Adapter Audit — 2026-04-24

**Purpose**: Scope Wave 8B. Enumerates all anatomical-region adapters in
NIMCP and classifies each as LIVE or STATUE based on hot-path reachability.

## Summary

29 region adapters. **7 LIVE** (ticked by Round A's `brain_tick_bio_async`
at `nimcp_brain_learning.c:1082`). **18 STATUEs** — created with working
`*_update` functions but never called from any hot path. **4 have no update
API** (thalamus-as-such, mammillary, spinal_cord, cortical_interneurons,
claustrum — static config objects, not statues).

## LIVE (7 — Round A)

hippocampus, prefrontal, occipital, parietal_cortex, temporal, insula,
brainstem. All ticked via `brain_tick_bio_async` which processes each
region's bio-router inbox + advances brainstem.

## STATUEs (18) — the Wave 8B backlog

| Region | Update API | Input required | Complexity |
|---|---|---|---|
| **medulla** | `medulla_update(dt)` | dt only | Simple |
| **locus_coeruleus** | `lc_step(dt)` | dt + novelty signal | Medium |
| **raphe** | `raphe_update(dt)` | dt + arousal | Medium |
| **vta** | `vta_step(dt)` | dt + reward signal | Medium |
| **habenula** | `habenula_step(dt)` | dt + prediction error | Medium |
| **amygdala** | `amygdala_step(dt)` | dt + threat/safety | High |
| **cingulate** | `cingulate_step(dt)` | dt + conflict | High |
| **motor** | `motor_adapter_update(dt)` | dt only | Simple |
| **olfactory** | `olfactory_update(dt)` | odor signal | Medium |
| **gustatory** | `gustatory_step(dt)` | taste signal | Medium |
| **somatosensory** | `somatosensory_step(dt)` | touch signal | Medium |
| **basal_ganglia** | `bg_enhanced_step(dt)` | cortical input + reward | Very High |
| **cerebellum** | `cerebellum_update_forward_model()` | motor err + state | Very High |
| **hypothalamus** | `hypothalamus_update(dt)` | drive state, neuromod | Very High |
| **entorhinal** | `entorhinal_step(dt)` | grid codes | High |
| **broca** | — (no update API yet) | bio-router messages | Needs API |
| **wernicke** | — (no update API yet) | bio-router messages | Needs API |

## Proposed Wave 8B sub-scopes

| Sub-wave | Regions | Drives | Risk |
|---|---|---|---|
| **8B-a** (recommended first) | medulla, locus_coeruleus, raphe, vta, habenula | All dt-only or dt+simple-scalar — unlocks arousal/salience/reward tone | Very low |
| **8B-b** | motor, olfactory, gustatory, somatosensory, amygdala, cingulate | Closes sensorimotor feedback loops + emotional learning | Medium |
| **8B-c** | hypothalamus, entorhinal, cerebellum, basal_ganglia | Drive-based motivation, spatial generalization, action selection. Needs reward/motor-error routing first | High |
| **8B-d** (optional) | broca, wernicke | Adapter exists but no update function; would need `*_process_bio_messages()` added first | Low structural, zero current value |

## Five modules flagged earlier

The user specifically asked about basal_ganglia, medulla, broca, wernicke,
cerebellum. All confirmed statues. They span three different sub-waves
(8B-a, 8B-c, 8B-d) based on complexity. No single wave naturally groups
them — they'd need to be picked up across 2-3 sub-waves.
