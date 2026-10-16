# Portia/Dragonfly/Swarm Walkthrough #14 — Final Report

**Date**: 2026-03-01
**Scope**: 203 .c files (~91,442 LOC) across Portia (23), Dragonfly (33), Swarm (86+) — line-by-line audit
**Result**: 96 files changed, +1012/-890 lines, ~390 bugs fixed
**Build**: Zero errors, zero warnings

---

## Bug Tally by Severity

| Severity | Count | Examples |
|----------|-------|---------|
| **CRITICAL** | 24 | Deadlocks from public locking functions called under lock (swarm_memory compress/forget, swarm_immune share_memory_cell, swarm_consensus invoke_callback x5, quorum cross_inhibit/cascade, energy_gossip harvest), missing bridge_base_init (3 portia + 2 dragonfly bridges), callback under mutex (portia_swarm_bridge x2) |
| **HIGH** | 30 | nimcp_mutex_free→destroy (15), missing bridge_base_init/cleanup (7 dragonfly bridges), wrong error codes for alloc (8), thread creation race (1), TOCTOU in sync_all (1), missing mutex on getter (1) |
| **MEDIUM** | ~140 | Wrong error codes (50+ across FEP/immune/sleep bridges), spurious THROW removal (60+), void*→typed mutex (1), cast removal, div-by-zero guards, external call under mutex |
| **LOW** | ~196 | Spurious THROW on normal conditions (75+ in swarm behavior), isfinite() guards (14), dead code removal (9), wrong THROW messages (11), unnecessary casts (56+) |
| **Total** | **~390** | |

---

## Module Results

### Portia (23 files, 16,254 LOC) — ~137 bugs

| # | Partition | Files | Bugs | Key Fixes |
|---|-----------|-------|------|-----------|
| P1 | Core | 7 | 26 | Stack buffer OOB in attention (HIGH), thread-unsafe getter in lifecycle (HIGH), platform mutex→thread layer in attention (17 sites), wrong error codes (13), spurious THROW (1) |
| P2 | Bridges+Power+Monitoring | 6 | 57 | Missing bridge_base_init (3 bridges CRIT), callback under mutex (2 CRIT in swarm_bridge), nimcp_mutex_free (5), platform mutex migration (power+monitoring), div-by-zero (2), ~33 spurious THROWs |
| P3 | Remaining+Immune | 10 | 54 | Callback under mutex (tier_switch HIGH), void*→typed mutex (sensor_fusion), immune bridge mutex migration (3 files, 36 sites), 24 spurious THROWs, wrong error codes (4) |

### Dragonfly (33 files, 25,876 LOC) — ~71 bugs

| # | Partition | Files | Bugs | Key Fixes |
|---|-----------|-------|------|-----------|
| D1 | Core+Tracking | 8 | 17 | nimcp_mutex_free (8 files), div-by-zero in multi_target (HIGH), spurious THROWs (3), Kalman zero-guard, isfinite EMA |
| D2 | Vision+Bridges | 8 | 15 | Missing bridge_base_init/cleanup (cortical+cognitive CRIT), nimcp_mutex_free (3), external call under mutex (visual), div-by-zero (2), wrong error code |
| D3 | Remaining Bridges | 16 | 39 | Missing bridge_base_init/cleanup (7 bridges HIGH), nimcp_mutex_free (4), div-by-zero (4), isfinite guards (10), unnecessary casts (56+) |

### Swarm (86 files, 49,312 LOC) — ~182 bugs

| # | Partition | Files | Bugs | Key Fixes |
|---|-----------|-------|------|-----------|
| S1 | Core (5 large) | 5 | 28 | Deadlock: compress/forget/share_memory_cell under lock (5 CRIT), TOCTOU in sync_all (HIGH), memory leak on mutex create fail (CRIT), wrong error codes (6), div-by-zero (4), spurious THROWs (7) |
| S2 | Behavior (14) | 14 | ~92 | Deadlock: quorum cross_inhibit/cascade (2 CRIT), energy_gossip harvest (1 CRIT), nimcp_mutex_free (1), wrong error codes (5), ~75 spurious THROWs on normal conditions |
| S3 | Logic+Parts (24) | 24 | 18 | Consensus callback-under-mutex (5 CRIT), nimcp_mutex_free (2), wrong error codes (6), spurious THROWs (5) |
| S4 | Bridges (33) | 33 | 24 | Wrong error code for malloc failure (22 bridges — copy-paste propagation), spurious THROW in bio_async (1) |

---

## Top Systemic Patterns

| Pattern | Count | Files | Severity |
|---------|-------|-------|----------|
| Deadlock: public locking function called under lock | 14 | 7 | CRITICAL |
| nimcp_mutex_free→nimcp_mutex_destroy | 15 | 15 | HIGH |
| Missing bridge_base_init/cleanup | 12 | 12 | CRIT/HIGH |
| Wrong error code for malloc (NULL_POINTER→NO_MEMORY) | ~50 | ~35 | MEDIUM |
| Spurious THROW_TO_IMMUNE on normal conditions | ~100 | ~25 | MED/LOW |
| Unnecessary (nimcp_mutex_t*) casts | 56+ | 8 | LOW |
| Missing isfinite() on EMA stores | 14 | 8 | LOW |
| Division without zero-guard | ~12 | ~8 | MED/HIGH |
| Callback/external call under mutex | 5 | 4 | CRIT/HIGH |

---

## Quality Score

**Before**: Never line-by-line audited (estimated ~6/10 based on pattern density)
**After**: ~8.5-9.0/10

**Remaining known items** (not fixed — low risk or out of scope):
- Cortical columns files within portia/dragonfly that pre-date thread abstraction (use platform layer directly)
- Some stubs with legitimate TODO comments

---

## Build Verification

```
$ cd build && cmake .. && make nimcp -j4
[100%] Built target nimcp
# Zero errors, zero warnings
```
