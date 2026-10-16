# KG Node Naming Registry

**Purpose**: canonical node-name convention for all nodes written to `brain->internal_kg`. Waves W1–W15 must pick names from these rules so cross-module `brain_kg_find_node` queries resolve.
**Authority**: extracted from the already-WIRED + PARTIAL bridges as of 2026-04-24 (amygdala, cerebellum, insula, pag, pr_kg_bridge, imagination_engine, genius_profiles `region_edges[]`). Where existing files disagree, the majority pattern below wins; exceptions are flagged.
**Last updated**: 2026-04-24

## Rules

### 1. Brain regions (structural nodes)

- Lowercase, words separated by `_`. Latin/anatomical term when it exists.
- Sub-parts use `_` as the only separator: `hippocampus_ca1`, `hippocampus_ca3`, `hippocampus_dg`, `amygdala_bla`, `pag_dorsolateral`, `cea_medial`. Never `.`, never camelCase.
- Full cortical names are spelled out, not abbreviated: prefer `"orbitofrontal_cortex"` over `"ofc"`, `"retrosplenial_cortex"` over `"rsc"`, `"cingulate_cortex"` over `"acc"`. (`"vta"`, `"insula"`, `"amygdala"` keep their common names.)
- Bare layer labels like `"V1"` alone are **forbidden**. Use `"occipital_v1"`, `"occipital_v2"`, `"occipital_v4"`, `"occipital_it"`. Same for `somatosensory_s1`, `somatosensory_s2`, `motor_m1`, `auditory_a1`.
- The canonical region-name list (from `genius_profiles.c` `region_edges[]`) is load-bearing for cross-wave queries: `parietal_cortex`, `occipital_cortex`, `temporal_cortex`, `prefrontal_cortex`, `hippocampus`, `cerebellum`, `amygdala`, `basal_ganglia`. Do not rename these.

### 2. Cognitive modules

- `cog_<family>_<name>` — the family comes from the `src/cognitive/<family>/` directory.
- Examples: `cog_memory_episodic`, `cog_memory_semantic`, `cog_memory_hopfield`, `cog_reasoning_forward_chain`, `cog_reasoning_abduction`, `cog_imagination_workspace`, `cog_attention_salience`.
- Exception already in the tree: the canonical names `working_memory`, `semantic_memory`, `theory_of_mind`, `global_workspace`, `jepa_predictor`, `brain_immune`, `mesh_coordinator` are used by WIRED queries in `genius_profiles.c` and `imagination_engine.c`. Keep them as-is; do not rename to the `cog_` prefix.

### 3. Network types

- `net_<type>`: `net_lnn`, `net_snn`, `net_cnn`, `net_fno`, `net_hnn`, `net_main_ann`.
- Network state nodes (W5): `net_<type>_state_<event>`. Examples: `net_snn_state_mode_collapse`, `net_lnn_state_gradient_explosion`, `net_cnn_state_feature_saturation`.

### 4. Runtime event nodes

- `<owner>_event_<kind>_<timestamp_us>`. The timestamp suffix guarantees uniqueness — **never reuse an event-node name**.
- `owner` matches an existing structural node (region, module, or `net_<type>`).
- Examples: `pag_event_defense_fight_1714089600000000`, `amygdala_event_fear_spike_1714089601200000`, `hippocampus_event_replay_1714089602000000`, `net_snn_event_pop_dead_1714089603000000`.
- If you need event aggregation (counts, rates) as a single node, use `<owner>_event_<kind>_aggregate` with no timestamp, and store counters via `brain_kg_add_metadata`.

### 5. Predicate / edge labels (descriptions)

- `brain_kg_add_edge`'s `description` field is free text (human-readable). The *type* is the enum `BRAIN_KG_EDGE_*`. Match the enum to the predicate:
  - generic connection → `BRAIN_KG_EDGE_CONNECTS_TO` (predicate intent: `connects_to`)
  - one-way flow → `BRAIN_KG_EDGE_SENDS_TO` (`sends_to`) / `BRAIN_KG_EDGE_RECEIVES_FROM` (`receives_from`)
  - cross-module integration → `BRAIN_KG_EDGE_INTEGRATES_WITH` (`integrates_with`)
  - neuromodulation → `BRAIN_KG_EDGE_MODULATES` (`modulates`)
  - excite/inhibit → `BRAIN_KG_EDGE_EXCITES` / `BRAIN_KG_EDGE_INHIBITS`
  - coordination / sync → `BRAIN_KG_EDGE_COORDINATES_WITH` (`coordinates_with`)
  - causation (event → event) → `BRAIN_KG_EDGE_DEPENDS_ON` reused as `caused_by`; add explicit description text `"caused_by"` if clarity needed.
- Use lowercase snake_case verbs in any free-text description that downstream code may string-match on: `observes`, `produces`, `consumed_by`, `triggered_by`.

### 6. Idempotent init

- Every module must guard its structural adds with `brain_kg_find_node(kg, name) == BRAIN_KG_INVALID_NODE` (or `kg_has_node(ctx)` for `kg_module_init` users) before calling `brain_kg_add_node`. Re-init of the same brain (e.g. reload from checkpoint) must not collide.
- Event nodes (rule 4) are inherently unique via timestamp, so no guard needed — but you still must check `brain->internal_kg && brain->internal_kg_enabled` first.

### 7. Admin token (CRITICAL — gates all writes)

- `brain_kg_init` sets access level to ADMIN, populates, then **downgrades to READ** (`src/core/brain/factory/init/nimcp_brain_init_internal_kg.c` line 290). **Any code that writes to the KG after that point MUST elevate first, or every `brain_kg_add_node` / `brain_kg_add_edge` call silently fails** with `Insufficient access level for write`.
- Pattern for writers that have a `brain_t` handle:
  ```c
  brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN,
                            brain->internal_kg_admin_token);
  /* ... brain_kg_add_node / brain_kg_add_edge ... */
  brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
  ```
- Pattern for writers that only have a `brain_kg_t*` (the mesh/bridge pattern): the outer **caller** must elevate before invoking the bridge's wiring function, or the bridge must be extended to take the admin token. W1 bridges follow the outer-caller-elevates rule; W2+ should propagate the token explicitly.
- The existing WIRED region wiring bridges (amygdala/pag/insula/etc) appear to work only because they run during `brain_kg_populate_from_brain` while access is still ADMIN. Do NOT assume this in new code.
- Do not hardcode the admin token to `0`.

## Known exceptions (flag for future cleanup)

- `basal_ganglia` is a single node per `genius_profiles.c`, but `subcortical_kg_wiring.c` emits sub-parts (striatum, GP, STN, SN, NAcc). Both styles coexist. Preferred forward: emit sub-parts with `_` separator (`basal_ganglia_striatum`, `basal_ganglia_gpe`, `basal_ganglia_stn`, etc.) **and** keep the `basal_ganglia` umbrella node for backward-compat queries.
- `rcog_brain_kg_bridge.c` is named like a `brain_kg` bridge but historically wired to `kg_reader` (external `.aim` KG). W1 re-points it; any caller that still expects external-KG semantics must switch to `kg_reader` directly.
- `wernicke_kg_wiring.c` already emits `"wernicke"`; the W1 statue files (`wernicke_nlp_bridge.c`, `omni_wernicke_bridge.c`) must **not** re-add `"wernicke"` — they only add child/sibling nodes and link to the existing one via `brain_kg_find_node`.
