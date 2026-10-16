# Wave F — Cortico-Cortical Long-Range Projections — Design Note

**Status**: Design draft 2026-04-27. Not yet implemented.
**Trigger**: Wave F (task #263), originally scoped as "add direct V1→V2,
A1→Wernicke skip-paths bypassing thalamus."
**Owner**: SNN biological-fidelity initiative.

## Why this is harder than the original spec implied

The original Wave F prompt assumed the SNN substrate has explicit cortical
regions (V1, V2, A1, Wernicke, Broca) that could be wired pairwise. After
inspecting the actual code, that's not how the substrate is organized.

### Where the regions actually live

The brain has TWO disjoint abstractions for "cortex":

1. **High-level cortex modules** — `brain->visual_cortex`, `brain->wernicke`,
   `brain->broca`, `brain->cortex_cnn[]` etc. These are perception
   subsystems with their own internal state, not SNN populations.
2. **SNN tier hierarchy** — 8 tiers (input, L1_feature, L2_pattern,
   L3_concept, L4_integr, L5_exec, L6_project, output) with no concept of
   "region." Every SNN pop is in ONE tier; the only inter-region
   structure today is the within-hierarchy `SKIP_DEFS` table at
   `nimcp_snn_hierarchical.c:74-77`:

   ```c
   { 1, 5, 0.0005f },  /* L1 → L5: fast sensory-to-executive shortcut */
   { 2, 6, 0.0005f },  /* L2 → L6: pattern-to-output projection */
   ```

These are **already** "cortico-cortical" projections in the loose sense —
they bypass intermediate tiers. What they're not is region-specific:
they fan out from one tier to all pops of another tier.

### The actual gap

Real cortex has functionally-specialised regions wired in topographic
patterns (V1 → V2 retinotopically, V1 → MT motion-selective, etc.).
The SNN tier hierarchy has none of that — every L2_pattern pop is
treated equivalently to every other L2_pattern pop. Adding "V1→V2
projections" requires either:

- **Option A — add region tags to populations.** Each tier's pops would
  get a `region_t` field (V1 / V2 / A1 / Wernicke / Broca / motor / ...).
  Then SKIP_DEFS could be region-aware: "from any V1 pop in tier N, fan
  out only to V2 pops in tier N+1." Adds significant data plumbing
  (region tag in struct, hierarchical wiring restructure, save/load
  format change, KG / monitoring side-effects).
- **Option B — bypass the SNN entirely and wire cortex modules
  directly.** Already partially exists: `omni_wernicke_bridge` is one
  example of inter-cortex routing. Wave F could be re-scoped as "wire
  more cortex-to-cortex bridges" — V1_to_V2_bridge, V1_to_MT_bridge,
  Wernicke_to_Broca_bridge — each with its own activation tap and
  routing logic. Doesn't touch SNN; faster to implement; less faithful
  to substrate-level biology.

### Why neither is a single-session implementation

Option A touches:
- `snn_population_s` struct (new field)
- Hierarchical wiring (region-aware SKIP_DEFS expansion)
- Per-region apportionment (currently 1 tier-pyr pop ≈ 1 region; with
  6+ regions per tier we'd need either more pops/region or shared
  pools — a real architectural decision)
- SNN_WIRING_SCHEMA_VERSION bump + cache invalidation
- Persistence (region tag must survive save/load)
- KG integration (kg_audit must register regions)
- Tests across all 4 tiers PLUS behavioural tests that verify
  region-specific routing actually fires (not just structurally
  exists)
- Walkthrough across the substrate-correctness audit dimensions
  (Dale's principle still holds per region; FFI timing per region;
  etc.)

Option B touches:
- `include/core/brain/cortex/` bridges (new modules)
- `nimcp_brain_init_cortex_*` factory functions
- Tick-chain inclusion in `brain_decide` / `brain_learn_vector`
- Activation taps + KG consumer registration (W17 pattern)
- Tests at the cortex-bridge level (not SNN)
- Walkthrough across the cortex-module audit dimensions

Both options need ~4-8 focused hours, and neither slots into the current
substrate-fidelity campaign cleanly.

## Recommendation

**Do not attempt Wave F in this session.** Instead:

1. **Acknowledge** that the existing `SKIP_DEFS` already provides
   cross-tier "cortico-cortical-like" routing at the SNN substrate
   level — the substrate is *not* missing cross-cortex structure
   per se; it's missing *region tags* and *region-specific topography*.
2. **Defer** to a separate planning session. Pick Option A or Option B
   based on which abstraction layer you want to enrich (substrate vs
   cortex-bridge).
3. **In the meantime**, the substrate is fully consistent with the
   "single generic cortex" mental model, which is what the surrounding
   training pipeline currently assumes.

If a single intermediate step is wanted, **add 2-4 more entries to
SKIP_DEFS** to extend the existing skip-path topology — that's a
2-line change and gives more diverse routing without committing to
either option above. Suggested:

```c
{ 0, 4, 0.0003f },  /* input → L4: thalamic bypass to integration */
{ 3, 6, 0.0008f },  /* L3 → L6: concept → output projection */
{ 5, 2, 0.0002f },  /* L5 → L2: long-range top-down (executive bias) */
```

These are still tier-level skips — not true region-to-region — but they
add useful topological diversity while staying within the existing
infra.

## Status going into the rest of this campaign

Marking Wave F as **DEFERRED PENDING DESIGN** in the task list. Closes
out the "do all of them" mandate insofar as Wave F cannot be done
without a real architectural decision (Option A vs Option B), which
needs human input.

## References

- Existing skip topology: `src/snn/nimcp_snn_hierarchical.c:74-77`
- Cortex modules: `include/perception/`, `include/core/brain/cortex/`
- Existing inter-cortex bridge example: `omni_wernicke_bridge` in
  `src/core/brain/factory/init/nimcp_brain_init_language.c`
