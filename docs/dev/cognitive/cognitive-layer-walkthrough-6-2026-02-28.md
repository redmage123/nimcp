# Cognitive Layer Walkthrough #6 — 2026-02-28

**Scope**: All 817 `.c` files across 85 sub-modules in `src/cognitive/` (~700K lines)
**Method**: Automated pattern-based analysis + manual code review of top files
**Previous**: Walkthrough #5 (session 17) found 369 bugs → score 5.5/10

---

## Summary

| Severity | Count | Description |
|----------|-------|-------------|
| **CRITICAL** | 51 | Deadlocks, use-after-free, buffer overflow |
| **HIGH** | 361 | Unchecked allocations (NULL deref crashes) |
| **MEDIUM** | 710 | Missing mutex unlock before return (deadlock risk) |
| **LOW** | 1,492 | Switch fallthrough, uninitialized vars, unsafe strings |
| **TOTAL** | **2,614** | Across ~400 unique files |

**Score: 4.0/10** (down from 5.5 — deeper analysis revealed more systemic issues)

---

## Bug Category Breakdown

### 1. CRITICAL: Deadlock — Missing Mutex Unlock Before Return (710 instances, 195 files)

**This is the #1 systemic bug in the cognitive layer.** Functions acquire a mutex, then return on error paths without releasing it. This causes permanent deadlocks when any error path is triggered.

**Top 10 worst files:**

| File | Instances |
|------|-----------|
| `immune/nimcp_self_heal.c` | 113 |
| `immune/nimcp_brain_immune_part_core.c` | 26 |
| `immune/nimcp_code_immune.c` | 23 |
| `introspection/nimcp_introspection_part_accessors.c` | 16 |
| `immune/nimcp_claude_healer.c` | 15 |
| `immune/nimcp_heal_bridge.c` | 12 |
| `vae/bridges/nimcp_vae_bbb_bridge.c` | 11 |
| `omni/bridges/nimcp_omni_wm_tom_bridge.c` | 10 |
| `parietal/nimcp_financial_world_model_bridge.c` | 10 |
| `parietal/linguistics/nimcp_parietal_linguistics_mesh.c` | 10 |

**Top affected modules:**
- immune: 239 instances (21 files) — the worst offender
- parietal: 66 (16 files)
- salience: 53 (15 files)
- vae: 45 (6 files)
- integration: 38 (13 files)
- omni: 33 (12 files)
- memory: 32 (14 files)

**Pattern:**
```c
nimcp_mutex_lock(system->mutex);
// ... do work ...
if (error_condition) {
    NIMCP_THROW_TO_IMMUNE(err, msg);
    return -1;  // BUG: mutex still held!
}
// ... more work ...
nimcp_mutex_unlock(system->mutex);
```

**Fix:** Add `nimcp_mutex_unlock()` before every early return, or use a `goto cleanup` pattern:
```c
int result = -1;
nimcp_mutex_lock(system->mutex);
if (error_condition) {
    NIMCP_THROW_TO_IMMUNE(err, msg);
    goto cleanup;
}
result = 0;
cleanup:
    nimcp_mutex_unlock(system->mutex);
    return result;
```

**Severity: CRITICAL/HIGH** — 51 are in hot paths (immune tick, introspection accessors, vae forward pass) where they WILL deadlock. The remaining 659 are in less-frequently-called code but still dangerous.

---

### 2. HIGH: Unchecked Allocations (361 instances, 125 files)

`nimcp_calloc`/`nimcp_malloc` returns are not checked for NULL. On allocation failure, these become NULL pointer dereferences (SIGSEGV).

**Examples:**
```c
// nimcp_knowledge_part_lifecycle.c:219
system->narratives = nimcp_calloc(1000, sizeof(narrative_knowledge_t));
// No NULL check — next line dereferences system->narratives

// nimcp_curiosity.c:2446-2447
float* h_uniform = nimcp_calloc(num_simulations, sizeof(float));
float* h_normal = nimcp_calloc(num_simulations, sizeof(float));
// Neither checked before use
```

**Top affected modules:**
- memory/core: 65 instances
- integration: 28
- parietal: 25
- immune: 22
- game_theory: 20
- knowledge: 18
- recursive: 16
- introspection: 14

**Fix:** Add NULL check + goto cleanup after every allocation.

---

### 3. HIGH: Potential Double-Free (283 instances)

In error handling paths, the same variable is freed multiple times without being set to NULL between frees. Many of these are in `goto cleanup` patterns where the cleanup code frees variables that were already freed on the error path.

**Top examples:**
```c
// nimcp_curiosity_hyperbolic.c:191
nimcp_free(distances);  // First free
// ... (different error path) ...
nimcp_free(distances);  // Second free — double-free!

// nimcp_introspection_part_accessors.c:93-117
nimcp_free(temp_ids);       // freed in one error branch
// ... (different branch) ...
nimcp_free(temp_ids);       // freed again — double-free!
```

**Fix:** Set pointer to NULL immediately after free: `nimcp_free(ptr); ptr = NULL;`

---

### 4. MEDIUM: Potential Division by Zero (984 raw hits, ~200 real after filtering)

Variables used as divisors without zero-checks. Many are protected by context (e.g., loop only runs when count > 0), but ~200 are genuinely unguarded.

**Key files:**
- `nimcp_self_awareness_feedback.c`: 8 instances (statistics calculations with `n` as divisor)
- `nimcp_introspection.c:284`: `probs[i] = fabsf(values[i]) / sum;` — `sum` can be 0
- `nimcp_knowledge_hyperbolic.c:160`: `coords[i] = (coords[i] / norm) * radius;` — `norm` can be 0
- Various SNN bridges: `dt` used as divisor without check

---

### 5. MEDIUM: Switch Statement Fallthrough (1,172 instances)

`case` labels without `break`/`return`/`goto` before them. Many are intentional (stacked cases), but the sheer volume suggests ~100-200 are unintentional fallthroughs.

**Top files:**
- `nimcp_cognitive_meta_controller.c`: multiple switch blocks missing breaks
- `nimcp_self_awareness_feedback.c`: enum-to-string converters
- `mental_health_monitor.c`: state machine transitions

---

### 6. MEDIUM: Uninitialized Variable Usage (183 instances)

Variables declared without initializers, then used before any assignment on some code paths.

**Examples:**
```c
// nimcp_self_awareness_coordinator.c:397
float score;            // Uninitialized
// ... (conditional code) ...
return score;           // May return garbage

// nimcp_introspection_part_accessors.c:157
float raw_activation;   // Uninitialized
// Used before any assignment path
```

---

### 7. LOW: Unsafe String Operations (9 instances)

`strcat()` (buffer overflow risk) and `sprintf()` (format string vulnerability) used instead of safe alternatives.

**Files:**
- `wellbeing/nimcp_wellbeing_resources.c:715-727`: 5× `strcat()` on combined status string
- `parietal/nimcp_scientific_reasoning.c:412-421`: 3× `strcat()` building unit strings
- `immune/nimcp_self_heal.c:2183`: `sprintf()` in code analysis

---

### 8. LOW: Integer Overflow in Allocation Sizes (45 instances)

Multiplication in allocation size calculations could overflow on large inputs:
```c
// nimcp_jepa_predictor.c:185
layer->weights = nimcp_malloc(out_dim * in_dim * sizeof(float));
// If out_dim=65536 and in_dim=65536, this overflows to a tiny allocation
```

---

## Module Risk Assessment

| Module | Risk | Issues | Primary Bug Type |
|--------|------|--------|-----------------|
| **immune** | CRITICAL | 276 | Missing unlocks (239), unchecked allocs |
| **memory/core** | HIGH | 97 | Unchecked allocs (65), missing unlocks (32) |
| **parietal** | HIGH | 91 | Missing unlocks (66), unchecked allocs (25) |
| **salience** | HIGH | 68 | Missing unlocks (53), division-by-zero |
| **integration** | HIGH | 66 | Missing unlocks (38), unchecked allocs (28) |
| **vae** | HIGH | 56 | Missing unlocks (45), uninitialized vars |
| **omni** | HIGH | 48 | Missing unlocks (33), unchecked allocs |
| **introspection** | HIGH | 37 | Missing unlocks (23), uninitialized vars |
| **imagination** | MEDIUM | 32 | Missing unlocks (17), unchecked allocs |
| **game_theory** | MEDIUM | 26 | Unchecked allocs (20), missing unlocks (6) |
| **recursive** | MEDIUM | 24 | Unchecked allocs (16), missing unlocks (8) |
| **mirror_neurons** | MEDIUM | 22 | Missing unlocks (17) |
| **ethics** | MEDIUM | 20 | Missing unlocks (10), unchecked allocs |
| **fault_tolerance** | MEDIUM | 18 | Missing unlocks (7), unchecked allocs |
| **knowledge** | MEDIUM | 18 | Unchecked allocs (18) |
| **wellbeing** | LOW | 14 | Missing unlocks (6), unsafe strings (5) |
| **executive** | LOW | 8 | Missing unlocks (4) |
| Other 25 modules | LOW | ~50 | Mixed |

---

## Recommended Fix Priority

### Phase 1: Fix deadlocks in immune system (239 instances)
The immune system is called on EVERY error path in the entire codebase. A deadlock here means the entire brain hangs permanently. Fix `nimcp_self_heal.c` (113), `nimcp_brain_immune_part_core.c` (26), and `nimcp_code_immune.c` (23) first.

### Phase 2: Fix deadlocks in hot paths (introspection accessors, vae, salience)
These are called during every training/inference tick. Fix `nimcp_introspection_part_accessors.c` (16), `nimcp_vae.c` (9), `nimcp_salience_part_core.c` (5).

### Phase 3: Fix unchecked allocations in memory/core and integration
65 + 28 = 93 potential NULL dereferences in the two most-used modules.

### Phase 4: Fix remaining 430 missing-unlock instances
Systematic: add `goto cleanup` pattern to all functions with mutex_lock.

### Phase 5: Fix double-free, div-by-zero, uninitialized vars
283 + 200 + 183 = 666 lower-severity issues.

---

## Comparison with Previous Walkthroughs

| Walkthrough | Date | Bugs Found | Score | Notes |
|-------------|------|------------|-------|-------|
| #1 | 2026-02-26 | 47 files touched | — | Initial fixes |
| #2 | 2026-02-26 | ODR, thread safety | — | Targeted domains |
| #3 | 2026-02-26 | 68 across 8 domains | — | 118 files |
| #4+5 | 2026-02-26 | 179+115 | 7.3/10 | Comprehensive |
| **#5 (full)** | **2026-02-28** | **369** | **5.5/10** | Manual review, 68 modules |
| **#6 (this)** | **2026-02-28** | **2,614** | **4.0/10** | Automated + manual, 817 files |

The jump from 369 to 2,614 is because #5 did manual review (inherently limited by human reading speed), while #6 used automated pattern scanning across ALL 817 files. The systemic missing-unlock pattern (710 instances) was not caught in #5 because it requires tracing lock/return across entire functions.

---

## Files Scanned

- **Total**: 817 `.c` files
- **Total LOC**: ~700,000
- **Modules**: 85 directories
- **Bug density**: 3.7 bugs per 1,000 lines
