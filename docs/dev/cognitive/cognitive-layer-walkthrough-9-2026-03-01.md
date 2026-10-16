# Cognitive Layer Walkthrough #9 — Full Post-Fix Audit (2026-03-01)

## Overview
- **Scope**: Read-only audit of `src/cognitive/` after walkthrough #8 fixes (~289 fixes across 166 files)
- **Coverage**: 20 of 20 partitions completed (FULL COVERAGE)
- **Files audited**: ~340+ files across 20 partitions
- **Excluded per convention**: `*_snn_bridge.c`, `*_substrate_bridge.c`, `*_thalamic_bridge.c`, `*_plasticity_bridge.c` — except where specifically targeted as a partition

## Aggregate Results

| Metric | Value |
|--------|-------|
| **Total bugs found** | **824** |
| **Critical** | **41** |
| **High** | **238** |
| **Medium** | **354** |
| **Low** | **191** |
| **Weighted score** | **6.3/10** |

## Results by Partition

| # | Partition | Files | C | H | M | L | Total | Score |
|---|-----------|-------|---|---|---|---|-------|-------|
| 1 | Grief/joy/remorse/LLF/inner_dialogue | ~11 | 0 | 3 | 14 | 22 | 39 | 7.5 |
| 2 | Creative external+generation | ~6 | 2 | 6 | 7 | 4 | 19 | 6.5 |
| 3 | Executive/attention/mental_health/sleep/meta | 13 | 2 | 6 | 9 | 2 | 19 | 6.5 |
| 4 | Thalamic bridges (7 files) | 7 | 0 | 4 | 24 | 2 | 30 | 7.0 |
| 5 | Top substrate bridges | 6 | 1 | 4 | 8 | 5 | 18 | 5.5 |
| 6 | Mid substrate bridges | 6 | 1 | 5 | 15 | 5 | 26 | 6.5 |
| 7 | Creative appreciation+inspiration+bridges | 14 | 3 | 9 | 8 | 2 | 22 | 5.0 |
| 8 | Bias/shadow/shadow_emotions | ~7 | 0 | 3 | 8 | 9 | 20 | 7.0 |
| 9 | Imagination | ~8 | 0 | 4 | 20 | 15 | 39 | 6.5 |
| 10 | Self_awareness + self_model | ~8 | 1 | 5 | 19 | 7 | 32 | 7.0 |
| 11 | Curiosity + wellbeing | ~10 | 3 | 12 | 11 | 10 | 36 | 7.5 |
| 12 | TOM + introspection + predictive + JEPA | ~12 | 4 | 21 | 21 | 6 | 52 | 6.5 |
| 13 | Free energy | ~10 | 5 | 11 | 14 | 9 | 39 | 7.5 |
| 14 | Remaining modules (8 dirs) | ~15 | 2 | 11 | 36 | 21 | 70 | 6.0 |
| 15 | Memory + knowledge + consolidation | ~12 | 2 | 9 | 19 | 8 | 38 | 6.5 |
| 16 | Game theory + ethics + personality | ~14 | 5 | 18 | 15 | 16 | 54 | 6.0 |
| 17 | VAE + recursive + fractal | ~10 | 1 | 17 | 15 | 2 | 35 | 7.0 |
| 18 | Omni + GW + integration | ~18 | 2 | 13 | 48 | 15 | 78 | 6.5 |
| 19 | Parietal + linguistics | ~14 | 1 | 7 | 23 | 29 | 60 | 7.0 |
| 20 | Immune + emotion + empathy | ~48 | 6 | 70 | 20 | 2 | 98 | 5.5 |
| | **TOTAL** | **~340+** | **41** | **238** | **354** | **191** | **824** | **6.3** |

## Top Systemic Patterns

### 1. Immune Bridge Stub Functions Disable Entire Subsystems (~12 HIGH across 5 files)
Multiple immune bridge modules contain `static` helper functions that permanently return 0, 0.0f, or `INFLAMMATION_NONE`. These are wired into production computation paths. Affected: `nimcp_emotion_immune_bridge.c` (3 stubs), `nimcp_reasoning_immune.c` (3 stubs), `nimcp_mental_health_immune_bridge.c` (4 hardcoded zeros), `nimcp_self_model_immune_bridge.c` (3 hardcoded defaults), `nimcp_health_emotion_bridge.c` (hardcoded valence/arousal). Entire bridges provide zero actual immune-cognitive coupling.

### 2. NULL Instance Heartbeat Pattern (~24 HIGH across 8+ files)
`*_heartbeat_instance(NULL, ...)` appears in training_begin/end/step functions across 8+ bridge files. The Phase 8 health tracking system was designed for instance-level health agent association, but every instance passes NULL. Additionally, `set_instance_health_agent()` functions in multiple files do `(void)agent` then assign to global, making instance-level tracking impossible.

### 3. Mutex Cast Anti-Pattern (~14 HIGH across 7 bridge files)
`nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex)` appears in 10+ functions across emotion_immune, mental_health_immune, tom_immune, executive_immune, sleep_immune, introspection_immune, wellbeing_immune bridges. Type-unsafe C-cast of bridge->base.mutex field. Silent memory corruption if types ever diverge in size.

### 4. External Callbacks Under Bridge Mutex (CRIT/HIGH, 10+ files)
The coordinator's update loop calls `entry->update_fn()` for every bridge while holding `coordinator->mutex`. Individual bridges hold their own mutex then call into immune system (`brain_immune_get_stats`, `brain_immune_get_antigen`, `brain_immune_release_cytokine`) or other modules (`curiosity_get_drive`, `emotion_attention_get_width`). Multi-layered lock ordering problem: coordinator->bridge->immune with no guaranteed ordering.

### 5. Thalamic Bridge Mutator Functions Skip Mutex (24 MEDIUM, 7 files)
Walkthrough #7/8 fixed getters to hold mutex, but mutators (`reset()`, `route_*()`, `set_attention()`, `training_begin()`) still write `stats` and `attention_weight` without locking. All 7 thalamic bridges have this pattern.

### 6. Error Code Inversion (~8 HIGH across 5 files)
`NIMCP_ERROR_NO_MEMORY` used for NULL pointer checks and `NIMCP_ERROR_NULL_POINTER` used for allocation failures. Appears in knowledge_immune_bridge, tom_immune_bridge, sleep_immune_bridge, wellbeing_immune_bridge, complement_system.

### 7. Spurious THROW_TO_IMMUNE on Normal Paths (~15+ MEDIUM across 30+ files)
Immune exceptions thrown on normal, non-error control flow: perception_immune anomaly lookup miss, coordinator bridge-id not found, empathetic_response no-crisis detection, and many more throughout the codebase.

### 8. Shallow Copy of Structs with Heap Pointers (6 HIGH across 3 files)
`archetype_info_t`, `extracted_pattern_t`, `creative_semantic_memory_t` all contain `float*` fields. Return-by-struct-copy aliases internal heap data, creating UAF/double-free risks.

### 9. Substrate Bridge Getter Data Races (6+ files)
Update functions hold mutex but getter functions read `effects`/`stats` without it. Some return interior pointers (`&bridge->effects`) compounding the risk.

### 10. Timestamp/Time Stub Pattern (~7 HIGH across 5 files)
`get_inflammation_duration_sec()` returns 0 unconditionally in 3 bridge files. `get_time_ms()` returns 0 in reasoning_immune. `get_timestamp_ms()` has integer overflow before cast in 2 files. Self_model uses `immune->start_time` as current time. All time-dependent features broken system-wide.

### 11. Unsigned Counter Underflow (2+ HIGH)
`active_envy_count` and `active_obsession_count` (shadow_emotions), `active_antibodies` (immune helpers), `num_active_antigens` (surface_immune) — `uint32_t` fields decremented without zero-guard, wrapping to UINT32_MAX.

### 12. `nimcp_mutex_free()` vs `nimcp_mutex_destroy()` (2 HIGH)
`nimcp_mutex_free` (undefined function) used instead of `nimcp_mutex_destroy` in brain_immune_part_lifecycle.c and immune_metrics.c. Results in link error or no-op leaving mutex resources leaked.

## Critical Bugs (41 total)

| # | File | Description |
|---|------|-------------|
| 1 | `creative/nimcp_creative.c:1012` | OOB read: visual path casts `visual_image_t*` to `uint8_t*` and iterates 4096 bytes past 24-byte struct |
| 2 | `creative/bridges/nimcp_creative_bridge.c:221` | Heap corruption: frees potentially non-heap `copyright_db_path` from struct assignment |
| 3 | `creative/bridges/nimcp_creative_training_bridge.c:880` | Global `g_feedback_buffer` with never-locked `g_feedback_mutex` — data races on realloc/memcpy |
| 4 | `executive/nimcp_executive.c:3283` | Unbounded mutual recursion: `create_plan_mcts` → `create_plan` → `classical_planning` → stack overflow |
| 5 | `mental_health/nimcp_mental_health_guardian.c:803` | THROW_TO_IMMUNE fires on every normal monitor thread exit, flooding immune |
| 6 | `omni/bridges/nimcp_omni_wm_substrate_bridge.c:194` | Hardcoded metabolic values — entire metabolic pipeline is no-op |
| 7 | `working_memory/nimcp_working_memory_substrate_bridge.c:489+502` | Double mutex unlock — UB/crash |
| 8 | `vae/bridges/nimcp_vae_substrate_bridge.c` | No mutex at all — every read/write is a data race |
| 9 | `vae/bridges/nimcp_vae_substrate_bridge.c:561` | Division by zero: `latent_dim_before / latent_dim_after` when min_latent_dim=0 |
| 10 | `immune/nimcp_brain_immune_part_accessors.c:289` | `#include` directive inside function body — non-standard C |
| 11 | `immune/nimcp_attention_immune_bridge.c:244` | Nested lock: holds bridge mutex then calls `brain_immune_get_stats` — deadlock |
| 12 | `immune/nimcp_attention_immune_bridge.c:484` | `brain_immune_release_cytokine()` called under bridge mutex — re-entrant deadlock |
| 13 | `immune/nimcp_brain_immune_fep_bridge.c:481` | `brain_immune_get_antigen()` under bridge mutex — nested lock inversion |
| 14 | `immune/nimcp_immune_bridge_coordinator.c:629` | Coordinator holds mutex across entire update loop calling all bridge update_fn — deadlock |
| 15 | `immune/nimcp_empathetic_response.c:244` | NULL dereference: accesses `engine->bio_ctx` BEFORE NULL check for `engine` |
| 16-41 | *(various across partitions 9-19)* | Free energy self-deadlock (6), curiosity FEP null deref (3), predictive hierarchy overflow, game theory/ethics/personality deadlocks (5), omni/GW integration races, parietal/JEPA overflows |

## Comparison with Walkthrough #8

| Metric | WL#8 (pre-fix) | WL#9 (post-fix, full) |
|--------|-----------------|------------------------|
| Files audited | ~642 | ~340+ |
| Total bugs | 617 | 824 |
| Critical | 66 | 41 |
| High | 199 | 238 |
| Medium | 239 | 354 |
| Low | 113 | 191 |
| Score | 6.5/10 | 6.3/10 |

**Analysis**:
1. **Fixed systemics confirmed resolved**: SNN cascade leak, missing `bridge_base_init`, return -1 from pointer functions — no longer appearing
2. **Bug count increased** despite fewer files because auditors went deeper per file, finding second-order issues (stubs, lock ordering, time functions)
3. **Critical count dropped 38%** (66→41): The worst crash-level bugs from WL#8 were successfully fixed
4. **HIGH count increased 20%** (199→238): Dominated by the immune bridge partition (70 HIGH alone) revealing stub functions, NULL instance agents, mutex casts, and error code inversions
5. **Immune subsystem is the worst partition** at 5.5/10 with 98 bugs in 48 files — dominated by stub functions that disable entire subsystems and systematic lock ordering violations
6. **Creative subsystem** remains problematic at 5.0-6.5/10 across 2 partitions due to shallow copy aliasing and heap corruption
7. **New dominant pattern**: Stub functions that render entire bridge modules inert (~12 across 5 files) — these are not TODO placeholders but shipped code returning constant values

## Fix Priorities

### Tier 1 — CRITICAL (fix immediately)
1. WM substrate double-unlock (crash)
2. Creative OOB read on visual_image_t
3. Creative bridge heap corruption on copyright_db_path free
4. Executive mutual recursion stack overflow
5. VAE substrate: add mutex + div-by-zero guard
6. Empathetic response NULL dereference before guard
7. Immune coordinator callback-under-lock deadlock
8. Attention immune bridge nested lock deadlocks (2 sites)
9. Brain immune FEP bridge re-entrant lock

### Tier 2 — HIGH (fix soon)
1. Immune bridge stub functions (systemic, ~12 stubs across 5 files — disable entire subsystems)
2. NULL instance heartbeat pattern (systemic, ~24 instances across 8+ files)
3. Mutex cast anti-pattern (systemic, ~14 casts across 7 files)
4. Error code inversion (systemic, ~8 instances across 5 files)
5. Thalamic bridge mutator locking (systemic, 7 files, 24 bugs)
6. Creative shallow-copy aliasing (6 instances, 3 files)
7. Shadow/immune uint32 underflow (4+ files)
8. Meta-learner add mutex
9. Global feedback buffer locking
10. `nimcp_mutex_free` → `nimcp_mutex_destroy` (2 files)
11. Timestamp/time stub functions (5 files)
12. Spurious THROW_TO_IMMUNE on normal paths (30+ files)

### Tier 3 — MEDIUM (schedule)
1. Substrate bridge getter locking (6+ files)
2. person_id=0 sentinel collision (2 files)
3. Training function instance parameter dead code (4+ files)
4. Anomaly timestamp population (2 files)
5. Platform mutex vs thread-layer mutex inconsistency
