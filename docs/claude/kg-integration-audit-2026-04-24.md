# Internal Knowledge Graph (KG) Integration Audit — 2026-04-24

**Scope**: every cognitive module, brain region, network type, physics/math module, and safety module.
**Excluded (N/A)**: `src/utils/`, `src/security/` primitives, memory allocators, exception infrastructure.
**KG surface**: `brain->internal_kg` (always-on default as of 2026-04-24), admin token in `brain->internal_kg_admin_token`, public API in `include/core/brain/nimcp_brain_kg.h`.
**Helper API**: `kg_module_init(&ctx, brain, "Name")`, `kg_has_node`, `kg_get_outgoing_safe`, `kg_get_neighbors_safe`, `kg_find_path_safe` — in `src/core/brain/nimcp_brain_kg_helpers.c`. Modules that adopt this pattern get write+read in ~50 LOC.

Definitions used throughout:

- **WIRED** — non-trivial write path (structural or event nodes/edges) **and** at least one read/query path in a hot routine.
- **PARTIAL** — either init-only write (most existing `*_kg_wiring.c` files) **or** read-only (e.g. genius_profiles). Missing direction should be added.
- **UNWIRED** — no references to any KG API.
- **N/A** — no symbolic-fact relationship. Documented below, excluded from waves.

Methodology: `grep -l "brain_kg_add_node|brain_kg_add_edge|brain_kg_query|brain_kg_get_node|brain_kg_find|internal_kg_enabled|kg_module_init|kg_has_node|kg_get_(neighbors|outgoing|incoming)_safe"` across `src/`, then call-graph confirmation for any `kg_module_init` user (init-only vs query-in-hot-path). Shallow grep flagged 48 files; only ~22 have actual calls into the KG (the rest are init-log or include-only).

---

## Section 1 — Summary

| Status | Count | % of auditable | Notes |
|---|---|---|---|
| **WIRED** (write + read) | **7** | 3% | `pr_kg_bridge`, `omni_kg_sync`, `imagination_engine`, `pag_kg_wiring`, `rsc_kg_wiring`, `red_nucleus_kg_wiring`, `reticular_kg_wiring` |
| **PARTIAL** (one direction, or init-only self-node) | **25** | 11% | Includes most `*_kg_wiring.c` bridges (write structural nodes at init, never queried back), plus `kg_module_init`-only adopters (executive, working_memory, introspection, reasoning_integration, self_model, attention, fep_orchestrator, bio_async_orchestrator, swarm_module_registry, mental_health_guardian, octopus_bridges, physics_kg_wiring, graph_theory_bridge, dynamical_systems_bridge, info_geometry_bridge, genius_profiles, cochlea init, creative init, wernicke init, surface_geometry init). |
| **UNWIRED** | **~175** | 76% | All 6 network types (LNN, SNN, CNN, FNO, HNN, main ANN), ~20 brain regions including occipital/somatosensory/motor/broca/raphe/olfactory/gustatory/mammillary/parahippocampal/perirhinal/brainstem/sensory_integration and all subcortical basal-ganglia/thalamus/colliculi/VTA/accumbens/substantia-nigra, and ~150 cognitive modules across memory/reasoning/perception/motor/ethics/emotion/social families. |
| **N/A** | **~25** | 10% | Pure math/ODE kernels (LNN/HNN integrators, FNO spectral transform, SNN adaptation biophysics); backup files (`*.bioasync_backup`); thin `*_part_accessors.c` SRP files whose parent part_core is already classified; raw GPU `.cu` kernels. |

**Grand total** auditable modules: ~230. (Excluded: `src/utils`, `src/security` primitives, allocators, exception infra, build artifacts, language bindings, test files, web-demo node_modules.)

Sanity check: `grep -l brain_kg_add_node` returns 48 files; of those only 23 are non-trivial (match the WIRED+PARTIAL-write count of 22 above plus `nimcp_brain_kg_helpers.c` itself and `nimcp_brain_kg.c`).

---

## Section 2 — Detailed classification (one table per family)

Columns: **Module** | **Status** | **KG refs** | **Emits** | **Queries** | **Notes**

Only notable modules shown per family; full-family rollups at bottom of each table. "KG refs" counts distinct `brain_kg_*`/`kg_module_*` call sites.

### 2.1 Brain regions (cortical + subcortical + medulla)

| Module | Status | KG refs | Emits | Queries | Notes |
|---|---|---|---|---|---|
| amygdala (`regions/amygdala/bridges/nimcp_amygdala_kg_wiring.c`) | PARTIAL | 7 add + 5 find | nuclei+fear/reward subgraph at init | find-by-name for cross-edges | No runtime fear-event emission. Template for emotion-valence regions. |
| cerebellum (`cerebellum_kg_wiring.c`) | PARTIAL | 2+2 | Purkinje/granule/deep-nuclei structural | find-by-name only | Canonical template. Add runtime motor-error events. |
| cingulate (`cingulate_kg_wiring.c`) | PARTIAL | 2+2 | ACC/PCC subgraph | find-by-name | No conflict/error emission. |
| claustrum (`claustrum_kg_wiring.c`) | PARTIAL | 5+4 | Binding subgraph | 4 finds | Stronger template — has multi-edge cross-wiring. |
| entorhinal (`entorhinal_kg_wiring.c`) | PARTIAL | 2+2 | Grid/place structural | find-by-name | Spatial events not emitted. |
| habenula (`habenula_kg_wiring.c`) | PARTIAL | 2+2 | Reward/punishment nodes | find-by-name | RPE signal not emitted. |
| hippocampus (`hippocampus_kg_wiring.c`) | PARTIAL | 7+5 | CA1/CA3/DG + pattern-completion | 5 finds | Episode write-path exists in `pr_kg_bridge.c` (covers it). |
| hypothalamus (`hypothalamus_kg_wiring.c`) | PARTIAL | 2+2 | Homeostatic nuclei | find-by-name | Drive-state changes not emitted. |
| insula (`insula_kg_wiring.c`) | PARTIAL | 2+2 | Interoception subgraph | find-by-name | Salience events not emitted. |
| locus_coeruleus (`lc_kg_wiring.c`) | PARTIAL | 2+2 | NE arousal | find-by-name | Arousal transitions not emitted. |
| OFC (`ofc_kg_wiring.c`) | PARTIAL | 7+5 | Value+reward coding | 5 finds | Decision-outcome not emitted. |
| PAG (`pag_kg_wiring.c`) | **WIRED** | 8+8 | Pain/defense + runtime | 8 finds incl. path | Read-path for pain routing. Template for event+query regions. |
| PFC (`pfc_kg_wiring.c`) | PARTIAL | 7+5 | DLPFC/VMPFC/vlPFC | 5 finds | Goals/plans not emitted at runtime. |
| red_nucleus (`red_nucleus_kg_wiring.c`) | **WIRED** | 7+7 | Motor relay | 7 finds | Large template (1002 LOC). |
| reticular (`reticular_kg_wiring.c`) | **WIRED** | 7+7 | RAS/arousal | 7 finds | Biggest template (1670 LOC). |
| retrosplenial (`rsc_kg_wiring.c`) | **WIRED** | 24+8 | Spatial navigation | 8 finds incl. path | Strongest read-path after reticular. |
| temporal (`temporal_kg_wiring.c`) | PARTIAL | 2+2 | Semantic hub | find-by-name | Semantic retrieval not querying. |
| VTA (`vta_kg_wiring.c`) | PARTIAL | 2+2 | Dopamine source | find-by-name | DA RPE not emitted. |
| wernicke (`wernicke_kg_wiring.c` + `wernicke_nlp_bridge.c`) | PARTIAL | 2+2 (+commented-out) | Language nodes | find-by-name | `wernicke_nlp_bridge.c` has `/* Would call brain_kg_add_node() */` stubs — **statue**. |
| subcortical (`subcortical_kg_wiring.c`) | PARTIAL | 7+5 | Striatum/pallidum/STN/SN structural | 5 finds | Covers ~10 regions via one bridge. Action-selection events not emitted. |
| medulla (`medulla_kg_wiring.c`) | PARTIAL | 2+2 | Autonomic | find-by-name | Protective-cutoff events not emitted. |
| occipital | **UNWIRED** | 0 | — | — | 10 source files, no KG. V1/V2/V4/IT hierarchy emits nothing. |
| somatosensory | **UNWIRED** | 0 | — | — | S1/S2, touch events unrecorded. |
| motor (region) | **UNWIRED** | 0 | — | — | `motor_adapter.c` + `motor_quantum_bridge.c` — action-issuance not logged. |
| broca | **UNWIRED** | 0 | — | — | 16 source files — language production, syntax, phonology all silent. |
| gustatory | **UNWIRED** | 0 | — | — | Taste events unrecorded. |
| olfactory | **UNWIRED** | 0 | — | — | Odor events unrecorded. |
| brainstem | **UNWIRED** | 0 | — | — | Vestibular, cardiopulmonary — unrecorded. |
| endocannabinoid | **UNWIRED** | 0 | — | — | eCB retrograde signaling unrecorded. |
| glymphatic | **UNWIRED** | 0 | — | — | Clearance events relevant for sleep KG. |
| mammillary | **UNWIRED** | 0 | — | — | Episodic relay. |
| neuropeptide | **UNWIRED** | 0 | — | — | Peptide releases should emit. |
| parahippocampal / perirhinal | **UNWIRED** | 0 | — | — | MTL — context/familiarity facts. |
| raphe | **UNWIRED** | 0 | — | — | 5-HT source, mood events. |
| parietal (region) | **UNWIRED** | 0 | — | — | Separate from cognitive/parietal. Spatial attention. |
| sensory_integration | **UNWIRED** | 0 | — | — | Cross-modal binding events. |
| claustrum cognitive-side | covered above | | | | |
| basal_ganglia / striatum / globus_pallidus / STN / SN / NAcc / SC / IC / thalamus (subcortical .c files) | **UNWIRED** | 0 | — | — | Covered structurally by `subcortical_kg_wiring.c`, but per-module runtime events (action selection, RPE, thalamic routing decisions) are not emitted. Many of these already have `kg_module_init`-compatible SRP structure. |

### 2.2 Cognitive — memory family

| Module | Status | KG refs | Emits | Queries | Notes |
|---|---|---|---|---|---|
| `memory/core/nimcp_pr_kg_bridge.c` | **WIRED** | 5 add + 5 find + 3 get | Episodes, memory nodes | Find+get by id | Reference template for event-style wiring. |
| `memory/nimcp_engram.c` | UNWIRED | 0 | Should emit engram formation/retrieval | Query related engrams | Core engram store. |
| `memory/nimcp_hopfield_memory.c` | UNWIRED | 0 | Pattern-completion events | Query for cue | |
| `memory/nimcp_semantic_memory.c` | UNWIRED | 0 | Concept nodes/edges | Concept graph walks | Should be a KG primary. |
| `memory/nimcp_episodic_replay.c` | UNWIRED | 0 | Replay events | Query replayed episodes | |
| `memory/nimcp_systems_consolidation*.c` (6 SRP files) | UNWIRED | 0 | Consolidation state transitions | Query fresh-vs-consolidated tags | part_core owns behavior. |
| `memory/nimcp_temporal_replay.c` | UNWIRED | 0 | Temporal sequences | | |
| `memory/nimcp_wm_transfer.c` | UNWIRED | 0 | WM→LTM transfer events | | |
| `memory/core/nimcp_metamemory*.c` | UNWIRED | 0 | Metamemory judgments | Query confidence | |
| `memory/core/nimcp_prospective*.c` | UNWIRED | 0 | Prospective intentions | Query pending intentions | |
| `memory/core/nimcp_source_memory.c` | UNWIRED | 0 | Source-tag facts | Query source for an item | Near-duplicate semantics with `pr_kg_bridge`. |
| `memory/core/nimcp_schemas.c` | UNWIRED | 0 | Schema nodes | Schema activation queries | KG-native candidate. |
| `memory/core/nimcp_social_memory.c` / `nimcp_collective_memory.c` / `nimcp_transactive.c` | UNWIRED | 0 | Who-knows-what edges | Query by agent | |
| `memory/core/nimcp_spaced_repetition.c` / `nimcp_skill_acquisition.c` / `nimcp_procedural.c` | UNWIRED | 0 | Skill acquisition milestones | | |
| `memory/core/nimcp_flashbulb.c` / `nimcp_gist.c` / `nimcp_counterfactual.c` / `nimcp_future_thinking.c` | UNWIRED | 0 | Specialized memory types | | |
| `memory/core/nimcp_reconsolidation.c` | UNWIRED | 0 | Reconsolidation update events | Query prior version | |
| `memory/core/nimcp_kuramoto.c` / `nimcp_theta_gamma.c` / `nimcp_resonance.c` / `nimcp_prime_signature.c` / `nimcp_fractal.c` / `nimcp_quaternion.c` / `nimcp_z_ladder.c` / `nimcp_entanglement.c` | N/A | 0 | Pure math substrates; no symbolic semantics | | Pure math — exclude. |
| `memory/core/nimcp_pr_memory_node.c` | PARTIAL (via pr_kg_bridge) | — | | | Consumer of `pr_kg_bridge`. |
| `memory/core/nimcp_pr_*_bridge.c` (29 bridges) | UNWIRED | 0 | Per-bridge events | | All write to `pr_memory_node` but not `internal_kg`. They are the write fan-in for pr_kg_bridge. |
| `memory/nimcp_memory_*_bridge.c` (consolidation, sleep, thalamic, fep, jepa) | UNWIRED | 0 | Per-bridge events | | Not KG-wired. |

### 2.3 Cognitive — reasoning / knowledge / logic / symbolic

| Module | Status | KG refs | Emits | Queries | Notes |
|---|---|---|---|---|---|
| `reasoning/nimcp_reasoning_integration.c` | PARTIAL | `kg_module_init` + 1 query | Self-node | 1 self-capability query | Needs fact-query path in chain/abduction/causal hot loops. |
| `knowledge/nimcp_kg_reader.c` | N/A for internal KG | 0 | — | — | Reads external `.aim/memory-nimcp.jsonl`, not `internal_kg`. Separate subsystem. |
| `knowledge/nimcp_knowledge.c` + SRP parts (7 files) | UNWIRED | 0 | Knowledge triples | Lookup by subject/predicate | Obvious KG-native candidate; currently parallel store. |
| `knowledge/nimcp_knowledge_*_bridge.c` (5 bridges) | UNWIRED | 0 | | | |
| `logic/nimcp_symbolic_logic.c` | UNWIRED | 0 | Logical facts/rules | Unification queries | Strong candidate — symbolic engine should be KG-backed. |
| `logic/nimcp_*_bridge.c` (audio/omni/somatosensory/visual/sleep/substrate/thalamic) | UNWIRED | 0 | | | |
| `symbolic_logic/*` (7 files incl. `lgss_loader.c`, `hub_bridge.c`, `safety.c`) | UNWIRED | 0 | | | LGSS already logs audit events; wire to KG too. |
| `reasoning/nimcp_knowledge_base_interface.c` | UNWIRED | 0 | | | Should be a KG façade. |
| `reasoning/nimcp_forward_chaining.c` / `backward_chaining.c` / `unification_engine.c` / `symbolic_logic_attachment.c` / `symbolic_logic_brain_integration.c` | UNWIRED | 0 | Inference steps | Antecedent lookup | Clearest read-path wins here. |
| `reasoning/nimcp_reasoning_abduction.c` / `affective.c` / `calibration.c` / `causal.c` / `chain.c` / `convergent.c` / `factory.c` / `hypo_bridge.c` / `kb_persistence.c` / `mesh_bridge.c` / `metacognition.c` / `plasticity_bridge.c` / `portia_bridge.c` / `sleep_bridge.c` / `snn_bridge.c` / `substrate_bridge.c` / `thalamic_bridge.c` / `visuospatial.c` / `reasoning_fep_bridge.c` | UNWIRED | 0 | | | 18 files. |
| `neuro_symbolic/nimcp_hypergraph*.c` (3 SRP), `evolutionary_proof.c`, `genius_math_orchestrator.c`, `energy_consistency.c`, `quantum_math_engine.c`, `quantum_mcts.c` | UNWIRED | 0 | Proof states | Lemma/axiom queries | `hypergraph` is essentially a pre-existing graph store. |

### 2.4 Cognitive — imagination / creative / jepa / world-model / predictive

| Module | Status | KG refs | Emits | Queries | Notes |
|---|---|---|---|---|---|
| `imagination/nimcp_imagination_engine.c` | **WIRED** | 1 add + 7 find + 7 edge | Self-node + 7 cross-edges | Finds JEPA/hipp/pfc/visual/audio/GWS/ToM | Best template for concept-linking. |
| `imagination/nimcp_*_bridge.c` (9 bridges) | UNWIRED | 0 | | | JEPA/GW/hippocampus/sleep/prefrontal/plasticity/snn/fep bridges. |
| `imagination/nimcp_imagination_workspace.c` | UNWIRED | 0 | Simulated scenario nodes | Concept queries | |
| `creative/nimcp_creative.c` + `nimcp_creative_orchestrator.c` | UNWIRED | 0 | Creative products | Prior-product queries | `brain_init_creative.c` references `internal_kg` but only at init. |
| `creative/{appreciation,bridges,external,generation,inspiration}/*` | UNWIRED | 0 | | | |
| `jepa/nimcp_jepa_*` (12 files) | UNWIRED | 0 | Latent state facts | | `imagination_engine` links to `jepa_predictor` node but JEPA doesn't emit. |
| `omni/nimcp_omni_kg_sync.c` | **WIRED** | 1 add + 1 edge + find_edge + find_path | World-model nodes + edges | `find_edge`, `find_path` | Reference template for bidirectional sync. |
| `omni/nimcp_omni_world_model*.c` (7 SRP) | PARTIAL (via `omni_kg_sync`) | — | | | Covered. |
| `omni/nimcp_omni_active_inference.c` / `metacognition.c` / `precision.c` / `jepa_bridge.c` / `wm_state.c` | UNWIRED | 0 | | | |
| `cognitive/physics/*` (41 world-model + sim files: intuitive physics, scene_graph, entity_tracker, biology/chemistry/physics sims) | UNWIRED | 0 | Object tracks + causal events | Entity queries | **Major gap** — world model is the natural KG host. |
| `extrapolation/*` (3 files: `compositional_systematic`, `counterfactual_imagination`, `world_model_multimodal`) | UNWIRED | 0 | | | |
| `predictive/*` (7 files) | UNWIRED | 0 | Prediction errors | Model queries | |
| `free_energy/nimcp_fep_orchestrator_part_core.c` | PARTIAL | `kg_module_init` + admin token read | Self-node | None | FEP has its own KG awareness but doesn't write beliefs. |
| `free_energy/nimcp_free_energy.c` + 20 other free_energy files | UNWIRED | 0 | Surprise/belief events | | |
| `salience/*` (13 files) | UNWIRED | 0 | Salience events | | |

### 2.5 Cognitive — attention / working memory / executive / introspection / global-workspace

| Module | Status | KG refs | Emits | Queries | Notes |
|---|---|---|---|---|---|
| `plasticity/attention/nimcp_attention.c` | PARTIAL | `kg_module_init` + `kg_has_node` + self query | Self-node | 1 `kg_get_outgoing_safe` | Best PARTIAL — one step from WIRED. |
| `cognitive/attention/*` (7 bridges + emotion_attention) | UNWIRED | 0 | Attention shifts | | |
| `working_memory/nimcp_working_memory_part_core.c` | PARTIAL | `kg_module_init` + 1 query | Self-node | 1 neighbors query | Needs item-store events. |
| `working_memory/*` (SRP + fep + gpu) | PARTIAL via part_core | — | | | |
| `executive/nimcp_executive.c` | PARTIAL | `kg_module_init` + `kg_get_outgoing_safe` | Self-node | Capabilities query | Task-switch events not emitted. |
| `executive/*_bridge.c` (6 bridges) | UNWIRED | 0 | | | |
| `introspection/nimcp_introspection_part_core.c` | PARTIAL | `kg_module_init` | Self-node | None (read missing) | Natural KG producer — self-report should query broadly. |
| `introspection/*` (9 other SRP + bridges + `connectivity_health.c`, `consciousness_metrics.c`, `ensemble_uncertainty.c`, `temporal_patterns.c`) | UNWIRED | 0 | Introspective reports | | |
| `global_workspace/*` (10 files) | UNWIRED | 0 | Broadcast events | Coalition membership queries | Core GW competition — KG-native. |
| `self_awareness/*` + `self_awareness_extended/*` (10 files) | UNWIRED | 0 | | | |
| `self_model/nimcp_self_model.c` | PARTIAL | `kg_module_init` + 1 query | Self-node | 1 query | Self-traits should be KG-backed. |
| `self_model/*_bridge.c` (7 bridges) | UNWIRED | 0 | | | |

### 2.6 Cognitive — emotion / social / theory of mind / mirror neurons / empathy

| Module | Status | KG refs | Emits | Queries | Notes |
|---|---|---|---|---|---|
| `emotions/nimcp_emotional_system.c` | UNWIRED | 0 | Emotion transitions | Mood queries | Insula/amygdala emit structurally — this emits runtime. |
| `emotion/*_bridge.c` (6) | UNWIRED | 0 | | | |
| `emotion_recognition/*` (5) | UNWIRED | 0 | Detected-other-emotion facts | | |
| `emotion_tensor/*` (4), `emotional_tagging/*` (4) | UNWIRED | 0 | | | |
| `empathetic_response/*` (6) | UNWIRED | 0 | | | |
| `shadow/*` (5) + `shadow_emotions/*` (3) + `grief/*` (4) + `joy/` + `remorse/*` (3) | UNWIRED | 0 | | | All runtime affect modules. |
| `theory_of_mind/*` (8) | UNWIRED | 0 | Agent-belief nodes | Query other's mental state | ToM → KG is a textbook fit. `imagination_engine` references `theory_of_mind` node but ToM itself doesn't write. |
| `tom/*` (2) | UNWIRED | 0 | | | Duplicate/sibling dir. |
| `mirror_neurons/*` (34 files incl. 7 SRP + `mirror_neurons.c`) | UNWIRED | 0 | Observed-action nodes | Mirror→intention queries | Very large, high-impact. |
| `social/*` (8) | UNWIRED | 0 | Social interaction facts | Relationship queries | |
| `collective_cognition/*` (16) | UNWIRED | 0 | Shared-intention facts | | |
| `personality/*` (5) | UNWIRED | 0 | Trait facts | Trait queries | |
| `inner_dialogue/*` (4) | UNWIRED | 0 | Dialogue turns | | |
| `bias/*` (6) | UNWIRED | 0 | Detected bias events | | |

### 2.7 Cognitive — ethics / mental health / safety / immune / wellbeing

| Module | Status | KG refs | Emits | Queries | Notes |
|---|---|---|---|---|---|
| `ethics/nimcp_ethics*.c` (12 files incl. asimov/warfare/hyperbolic/combinatorial_harm/policies/incidents/evaluation/learning/part_*) | UNWIRED | 0 | Violations, evaluations | Precedent queries | LGSS audit log is the existing sink; wire to KG too so ethics has symbolic context. **HIGH PRIORITY.** |
| `ethics/*_bridge.c` (6) | UNWIRED | 0 | | | |
| `ethics/nimcp_core_directives.c` | UNWIRED | 0 | Directive facts | | Natural KG primary. |
| `mental_health/nimcp_mental_health_guardian.c` | PARTIAL | 1 find + 1 add | Self-node | Find own node | Other `mental_health/*` files are write-statues. |
| `mental_health/*` (12 other) incl. `disorder_detectors.c`, `interventions.c`, `nimcp_mental_health.c` + SRP parts | UNWIRED | 0 | Disorder detections, interventions | | |
| `immune/*` (~45 files) | UNWIRED | 0 | Antigens, exhaustion events | Query past antigens | `brain_immune` already persists via its own system; cross-emit to KG for symbolic queries. |
| `wellbeing/*` (18) | UNWIRED | 0 | Wellbeing state transitions | | |
| `health/*` (4) + `fault_tolerance/*` (16) + `predictive_immune/*` (3) | UNWIRED | 0 | Fault events, recovery steps | Prior-fault queries | |
| `rubric/nimcp_rubric.c` | UNWIRED | 0 | Rubric scores | | |
| `epistemic/*` (5) | UNWIRED | 0 | Epistemic states | | |

### 2.8 Cognitive — language / communication

| Module | Status | KG refs | Emits | Queries | Notes |
|---|---|---|---|---|---|
| `language/nimcp_emergent_language.c` | UNWIRED | 0 | Vocabulary nodes | Token→concept queries | Brain-native vocab is a KG-primary. |
| `language/nimcp_inner_speech.c` / `native_language.c` / `tokenizer.c` | UNWIRED | 0 | | | |
| `parietal/linguistics/nimcp_parietal_linguistics_mesh.c` | UNWIRED (commented) | 0 + 1 statue comment | | | Stub: `/* KG node registration would happen here if brain_kg_add_node was available */`. **STATUE — must finish.** |
| `broca/*` (all 16 files, cross-listed with region) | UNWIRED | 0 | | | |
| `regions/wernicke/nimcp_wernicke_nlp_bridge.c` / `nimcp_omni_wernicke_bridge.c` | UNWIRED (commented) | 0 + 2 statues | | | Same statue pattern. |

### 2.9 Cognitive — curiosity / meta-learning / consolidation / sleep-wake

| Module | Status | KG refs | Emits | Queries | Notes |
|---|---|---|---|---|---|
| `curiosity/*` (10) | UNWIRED | 0 | Novelty events | Prior-novelty queries | |
| `meta_learning/*` (5) + `nimcp_meta_learning.c` + `nimcp_cognitive_meta_controller.c` | UNWIRED | 0 | Task-context facts | Meta queries | |
| `consolidation/*` (7) | UNWIRED | 0 | Consolidation state | | |
| `sleep_wake/*` (7) | UNWIRED | 0 | Sleep stage facts | | |
| `analogical_transfer` (`nimcp_analogical_transfer.c`), `contrastive_self.c`, `multiscale_memory.c`, `world_model_trainer.c`, `dynamic_arch.c`, `self_curriculum.c`, `trauma_resilience.c`, `wm_scratchpad.c`, `ood_detector.c`, `output_attention.c`, `self_awareness_coordinator.c`, `self_awareness_feedback.c`, `fractal_cognitive.c`, `emotional_learning.c`, `emotional_system_fep_bridge.c`, `emotional_tagging.c`, `personality_fep_bridge.c`, `hierarchical_fep_bridge.c`, `meta_learning_fep_bridge.c` | UNWIRED | 0 | | | 19 one-off cognitive files in `src/cognitive/` root. |

### 2.10 Cognitive — math / game theory / physics cognition

| Module | Status | KG refs | Emits | Queries | Notes |
|---|---|---|---|---|---|
| `math/*` (18: abstract_algebra, category_theory, combinatorics, complexity_theory, dsp, game_theory, graph_theory, information_theory, knot_theory, linear_algebra, logic, number_theory, numerical_methods, optimization, probability, real_analysis, topology, zeta_functions) | UNWIRED | 0 | Theorem facts, proof steps | Theorem lookups | Cognitive math engines — symbolic. |
| `game_theory/*` (17) | UNWIRED | 0 | Games, outcomes, equilibria | Opponent-history queries | |
| `parietal/*` (65 files incl. financial analogues, genius profiles, intuition, analogical_reasoning, conceptual_blending, counterfactual, equation_manipulation, hypothesis_generation, insight_discovery, mathematical_genius/intuition, meta_reasoning, number_sense, scientific_reasoning, spatial_reasoning, software/electrical/mechanical/civil engineering, physics_nn, + domain bridges) | UNWIRED | 0 | Domain facts, equations | Equation & concept lookups | **Largest silent cognitive family.** Financial + genius bridges were just activated in round-A but not wired to KG. |
| `parietal/linguistics/nimcp_parietal_linguistics_mesh.c` | UNWIRED (statue) | 0 + 1 comment | | | See 2.8. |
| `physics/*` cognitive (41 world-sim files: intuitive_physics, entity_tracker, scene_graph, world_model_cognitive_integration, world_simulator, + 35 biology/chemistry/physics sims) | UNWIRED | 0 | Object tracks, causal facts | Object queries | Listed in 2.4 too; same set. |

### 2.11 Cognitive — recursive / genius / octopus / VAE / explanations

| Module | Status | KG refs | Emits | Queries | Notes |
|---|---|---|---|---|---|
| `recursive/nimcp_rcog_brain_kg_bridge.c` | UNWIRED (file exists but 0 KG-API calls) | 0 | | | Bridge file exists but its body calls `kg_reader` (external `.aim` KG), NOT `brain->internal_kg`. **Mis-named statue.** |
| `recursive/*` (26 other files incl. orchestrator + engine SRP + 10 bridges + tool_router + delegation_pool) | UNWIRED | 0 | Recursive thought steps | Sub-question queries | |
| `octopus/nimcp_octopus.c` + `nimcp_octopus_bridges.c` | PARTIAL | 1 add | Tentacle rate-limited nodes | None | **Emerging template** — write-only swarm emission. |
| `core/brain/genius/nimcp_genius_profiles.c` | PARTIAL | 2 add + 3 find + 3 edge | Genius-profile subgraph at init | 3 finds | Write-only structural. |
| `explanations/*` (4) | UNWIRED | 0 | Explanation chains | Prior-explanation queries | |
| `vae/*` (7) | N/A | 0 | Latent space — pure numerical | | Exclude. |
| `collective_cognition/*` (16) | UNWIRED | 0 | Shared beliefs | | Covered in 2.6. |

### 2.12 Networks (LNN / SNN / CNN / FNO / HNN / main)

All six are **UNWIRED** — zero KG references.

| Network | Status | Emits | Queries | Notes |
|---|---|---|---|---|
| **LNN** (`src/lnn/*.c`, 22 files incl. `nimcp_lnn.c`, `nimcp_lnn_hamiltonian.c` for HNN, `nimcp_lnn_network.c`, `nimcp_lnn_training.c`) | UNWIRED | Network activity snapshots, phase transitions | Region connectivity queries | The `nimcp_lnn_hamiltonian.c` is where HNN lives — same UNWIRED state. |
| **SNN** (`src/snn/*.c`, 14 files + `batch_safe/` + `bridges/`) | UNWIRED | Spike-rate anomalies, population activity | | Many bridges but none to KG. |
| **CNN** (`src/training/nimcp_cortex_cnn.c` + `nimcp_cnn_training.c` + `nimcp_cnn_cortex_bridge.c` + `adapters/nimcp_cnn_trainable.c`) | UNWIRED | Per-cortex detected features | Prior-feature queries | |
| **FNO** (`src/training/nimcp_fno_layer.c` + `snn/nimcp_snn_fno.c`) | UNWIRED | Spectral mode facts | | |
| **HNN** (inside `src/lnn/nimcp_lnn_hamiltonian.c` + `cognitive/free_energy/nimcp_fep_hnn_fno_bridges.c`) | UNWIRED | Energy-conserving dynamics summaries | | |
| Main ANN (under `src/core/brain/` generally) | Covered elsewhere | | | |

Deep-math kernels (`lnn_ode.c`, `lnn_parallel.c`, `snn_adaptation.c`, `snn_encoding.c`, `fno_layer.c` kernel itself) are **N/A** — no symbolic content. Wire at the *network level* (`nimcp_lnn.c`, `nimcp_snn_network.c`, `nimcp_cortex_cnn.c`), not the kernel level.

### 2.13 Physics / geometry / graph / thermodynamics

| Module | Status | KG refs | Emits | Queries | Notes |
|---|---|---|---|---|---|
| `physics/bridges/nimcp_physics_kg_wiring.c` | PARTIAL | 3 add | Physics subgraph root | None | Init-only. Template for physics discipline nodes. |
| `physics/graphs/nimcp_graph_theory_bridge.c` | PARTIAL | 6 add + 5 edge | Graph-theory discipline nodes at init | None | Write-only. |
| `physics/dynamics/nimcp_dynamical_systems.c` | PARTIAL | 4 add + 3 edge | Dynamical-systems subgraph | None | Write-only. |
| `physics/geometry/nimcp_information_geometry_bridge.c` | PARTIAL | 6 add + 5 edge | InfoGeom subgraph | None | Write-only. |
| `physics/biophysics/nimcp_hodgkin_huxley.c` | UNWIRED | 0 | Per-neuron events (probably too hot) | | Wire via aggregator, not per-neuron. |
| `physics/ephaptic/nimcp_ephaptic.c` | UNWIRED | 0 | Field-coupling events | | |
| `physics/thermodynamics/nimcp_thermodynamics.c` | UNWIRED | 0 | Dissipation/entropy summaries | | |
| `physics/geometry/nimcp_information_geometry.c` | UNWIRED | 0 | Manifold-tracking summaries | | The `_bridge.c` sibling is PARTIAL; `_core.c` itself isn't. |
| `physics/bridges/nimcp_physics_*_bridge.c` (14 non-kg bridges) | UNWIRED | 0 | | | Per-subsystem coupling. |

### 2.14 Integration / swarm / edge / async / plasticity (support modules)

| Module | Status | KG refs | Notes |
|---|---|---|---|
| `integration/knowledge/nimcp_sensory_kg_wiring.c` | PARTIAL (via `sensor_hub`) | stubs | Exists but 0 direct calls in file (uses include-chain). Verify via deep-audit. |
| `async/nimcp_bio_async_orchestrator.c` | PARTIAL | `kg_module_init` | Self-node only. |
| `swarm/nimcp_swarm_module_registry.c` | PARTIAL | 1 add + 1 edge + `kg_module_init` | Registers swarm modules; no read-back. |
| `plasticity/attention/nimcp_attention.c` | PARTIAL | See 2.5 | |
| Everything else in `src/async`, `src/swarm`, `src/edge`, `src/plasticity` | UNWIRED | 0 | Most are support infrastructure — some genuinely N/A (e.g. `nimcp_dragonfly_cnn_bridge.c`, GPU kernels). |

### 2.15 Safety modules (LGSS / security / audit)

| Module | Status | KG refs | Notes |
|---|---|---|---|
| `security/*` (non-primitive: `lgss.c`, `nimcp_audit_log.c`, `lgss_safety_kb.c`) | UNWIRED | 0 | LGSS evaluations + audit events are promising KG emissions (precedent, rule-lookup). **HIGH PRIORITY.** |
| `cognitive/ethics/*` | UNWIRED | 0 | Covered in 2.7. |

### 2.16 N/A list (not auditable)

- All `*.bioasync_backup` (backup files — ignore).
- `src/utils/`, `src/security/nimcp_constant_time.c`, memory allocators, exception infra (explicitly excluded).
- `src/gpu/` raw `.cu` kernels.
- Pure-math substrates that have no symbolic semantics: `memory/core/nimcp_kuramoto.c`, `theta_gamma.c`, `resonance.c`, `prime_signature.c`, `fractal.c`, `quaternion.c`, `z_ladder.c`, `entanglement.c`; `cognitive/vae/*`; LNN ODE/parallel kernels; SNN adaptation biophysics; FNO spectral kernel; HNN integrator.
- `language/`, `chemistry/`, `biology/` top-level `src/` dirs (integration shims; audit if they grow symbolic).
- All `*_part_accessors.c`, `*_part_helpers.c`, `*_part_io.c`, `*_part_lifecycle.c` SRP files are classified under the same status as their sibling `*_part_core.c`.

---

## Section 3 — Wave retrofit plan

Sized to 6–10 modules per wave so one agent can finish + test in one pass. Each wave names a template file to copy from, to constrain code style and ensure the `BRIDGE_BOILERPLATE_MESH_ONLY` + `NIMCP_LOGGING_INFO` + `brain_kg_add_node` idioms stay consistent.

| # | Wave | Modules | Template | Effort |
|---|---|---|---|---|
| **W1** | Statue-finish (commented-out stubs) | `regions/wernicke/nimcp_wernicke_nlp_bridge.c`, `regions/wernicke/nimcp_omni_wernicke_bridge.c`, `parietal/linguistics/nimcp_parietal_linguistics_mesh.c`, `brain/factory/init/nimcp_brain_init_surface_geometry.c`, `cognitive/recursive/nimcp_rcog_brain_kg_bridge.c` (re-point at `brain->internal_kg`, not `kg_reader`) | `insula_kg_wiring.c` | S — 1 pass |
| **W2** | Region write-emissions round (add runtime events to already-structural regions) | amygdala, cingulate, entorhinal, habenula, hypothalamus, insula, LC, OFC, PFC, temporal, VTA, subcortical | `pag_kg_wiring.c` (it has runtime pain events) | M — runtime events only |
| **W3** | Unwired brain regions | occipital, somatosensory, motor, broca, gustatory, olfactory, brainstem, raphe, parahippocampal, perirhinal | `cerebellum_kg_wiring.c` | M — 10 new `*_kg_wiring.c` files |
| **W4** | Remaining brain regions + peptide subsystems | endocannabinoid, glymphatic, mammillary, neuropeptide, sensory_integration, basal_ganglia per-module (striatum, GP, STN, SN, NAcc, SC, IC, thalamus individual events) | `subcortical_kg_wiring.c` | M |
| **W5** | Network types (6 networks) | `lnn/nimcp_lnn.c` + `lnn_hamiltonian.c`, `snn/nimcp_snn_network.c`, `training/nimcp_cortex_cnn.c`, `training/nimcp_fno_layer.c`, `snn/nimcp_snn_fno.c`, main ANN hooks in `core/brain/` | `physics_kg_wiring.c` (discipline + aggregator pattern) | L — network-level aggregators, not per-neuron |
| **W6** | Cognitive–memory family (KG-native) | `memory/nimcp_engram.c`, `nimcp_semantic_memory.c`, `nimcp_hopfield_memory.c`, `nimcp_episodic_replay.c`, `nimcp_systems_consolidation_part_core.c`, `memory/core/nimcp_schemas.c`, `nimcp_source_memory.c`, `nimcp_reconsolidation.c` | `pr_kg_bridge.c` (WIRED) | L — bidirectional, core product feature |
| **W7** | Cognitive–reasoning / knowledge / symbolic | `knowledge/nimcp_knowledge_part_core.c`, `reasoning/nimcp_knowledge_base_interface.c`, `reasoning/nimcp_forward_chaining.c`, `backward_chaining.c`, `unification_engine.c`, `symbolic_logic_attachment.c`, `logic/nimcp_symbolic_logic.c`, `neuro_symbolic/nimcp_hypergraph_part_core.c`, `symbolic_logic/nimcp_symbolic_logic_lgss_loader.c` | `imagination_engine` (WIRED cross-linking) | L — highest read-path value |
| **W8** | Cognitive–world-model / imagination / FEP / predictive / salience | `omni/nimcp_omni_world_model_part_core.c`, `cognitive/physics/nimcp_intuitive_physics.c`, `nimcp_scene_graph.c`, `nimcp_entity_tracker.c`, `nimcp_world_simulator.c`, `free_energy/nimcp_free_energy.c`, `predictive/nimcp_predictive.c`, `salience/nimcp_salience_part_core.c`, `imagination/nimcp_imagination_workspace.c`, `jepa/nimcp_jepa_brain_bridges.c` | `omni_kg_sync.c` (WIRED) | L |
| **W9** | Cognitive–attention/WM/executive/introspection/GW (finish PARTIAL set) | Add read-paths to: `plasticity/attention/nimcp_attention.c`, `working_memory/nimcp_working_memory_part_core.c`, `executive/nimcp_executive.c`, `introspection/nimcp_introspection_part_core.c`, `self_model/nimcp_self_model.c`; full wire: `global_workspace/nimcp_global_workspace_part_core.c`, `self_awareness/nimcp_self_awareness_extended.c` | `executive.c` (PARTIAL→WIRED delta) | M — mostly read-path additions |
| **W10** | Cognitive–emotion / affect / ToM / mirror / social (runtime events) | `emotions/nimcp_emotional_system.c`, `theory_of_mind/nimcp_theory_of_mind.c`, `mirror_neurons/nimcp_mirror_neurons_part_core.c`, `social/nimcp_social_interaction.c`, `collective_cognition/nimcp_collective_cognition_part_core.c`, `personality/nimcp_personality.c`, `empathetic_response/nimcp_empathetic_response.c`, `emotion_recognition/nimcp_emotion_recognition_simple.c`, `grief/nimcp_grief_and_loss.c`, `shadow/nimcp_shadow_emotions.c` | `imagination_engine` | L |
| **W11** | Safety: ethics + LGSS + mental health + immune | `ethics/nimcp_ethics_part_core.c`, `nimcp_ethics_evaluation.c`, `nimcp_ethics_incidents.c`, `nimcp_ethics_policies.c`, `nimcp_core_directives.c`, `security/lgss.c` (+ safety_kb), `symbolic_logic/nimcp_symbolic_logic_safety.c`, `mental_health/nimcp_mental_health.c` + `disorder_detectors.c` + `interventions.c`, `cognitive/immune/nimcp_brain_immune_part_core.c` | `pag_kg_wiring.c` + `pr_kg_bridge.c` | M — HIGH-PRIORITY audit cross-emit |
| **W12** | Cognitive–language / communication | `language/nimcp_emergent_language.c`, `nimcp_inner_speech.c`, `nimcp_native_language.c`, `nimcp_tokenizer.c`, `broca/nimcp_discourse_manager.c`, `nimcp_syntax_processor.c`, `nimcp_pragmatics_processor.c`, `nimcp_multimodal_language.c` | `pr_kg_bridge` | M |
| **W13** | Cognitive–curiosity / meta-learning / consolidation / sleep-wake / curriculum | `curiosity/nimcp_curiosity.c` + `nimcp_information_forager.c`, `meta_learning/nimcp_meta_learning.c`, `consolidation/nimcp_consolidation.c`, `sleep_wake/nimcp_sleep_wake.c`, `nimcp_self_curriculum.c`, `nimcp_analogical_transfer.c`, `nimcp_multiscale_memory.c`, `nimcp_contrastive_self.c` | `executive.c` | M |
| **W14** | Cognitive–math / game-theory / parietal genius / financial | `math/*` (18 files — one bridge per discipline via a single new `math_kg_wiring.c`), `game_theory/nimcp_game_theory.c`, `parietal/nimcp_genius_erdos/gauss/newton.c`, `parietal/nimcp_analogical_reasoning.c`, `nimcp_hypothesis_generation.c`, `nimcp_insight_discovery.c`, financial orchestrator | `graph_theory_bridge.c` (discipline sub-graph) | M |
| **W15** | Physics / geometry / thermodynamics / biophysics (runtime events for already-PARTIAL) | `physics/biophysics/nimcp_hodgkin_huxley.c` (aggregated), `physics/ephaptic/nimcp_ephaptic.c`, `physics/thermodynamics/nimcp_thermodynamics.c`, `physics/geometry/nimcp_information_geometry.c`; add read/runtime-emit to graph_theory/dynamical_systems/info_geometry bridges | `dynamical_systems.c` | M |

**Waves map cleanly to parallel agents**: W1 blocks nothing, W2+W3+W4 are region-parallel, W5 is its own agent (needs network expertise), W6+W7+W8 are the high-value KG-primary waves, W9–W15 can run concurrently.

Approximate total new LOC: W1 ~500; W2–W4 ~5000; W5 ~3000; W6–W8 ~6000; W9–W15 ~8000. Budget: roughly 22–25k LOC across ~200 files, matching the 16k LOC already in the 23 existing wiring files.

---

## Section 4 — Risks

1. **Statues misreported as WIRED by shallow grep.** `recursive/nimcp_rcog_brain_kg_bridge.c` includes `cognitive/knowledge/nimcp_kg_reader.h` and is named like a `brain_kg` bridge, but calls 0 `brain_kg_*` functions — it wires to the external `.aim/memory-nimcp.jsonl` KG (kg_reader), not `brain->internal_kg`. Classified UNWIRED. Always follow the call graph two levels deep.

2. **`wernicke_nlp_bridge.c`, `omni_wernicke_bridge.c`, `parietal_linguistics_mesh.c`, `brain_init_surface_geometry.c`** all contain `/* Would call brain_kg_add_node() */` comments — these are explicit **statues**, not UNWIRED (someone started the wiring and left a TODO). W1 promotes them before agents spend time re-analyzing.

3. **Cross-module query collisions.** WIRED modules use node-name strings (`"jepa_predictor"`, `"hippocampus"`, `"global_workspace"`) to find peers via `brain_kg_find_node`. If W3 (unwired regions) picks inconsistent names (e.g. `"V1"` vs `"occipital_v1"` vs `"primary_visual_cortex"`), read-path queries in W8 (world-model) will silently miss edges. **Mitigation**: before W3 starts, produce a node-naming registry (draft from existing bridges + `genius_profiles.c`'s `region_edges[]` list as canonical).

4. **Network-level wiring granularity (W5).** `nimcp_lnn_hamiltonian.c` is the HNN — there's no separate `src/hnn/` dir. Wiring must choose one emit point (network-tick aggregator) rather than per-neuron emission, else the KG becomes a log firehose. Same for `hodgkin_huxley.c` and `ephaptic.c`. **Mitigation**: emit state-transition nodes on trigger events (mode changes, anomaly spikes), not every tick.

5. **Admin token propagation.** `brain->internal_kg_admin_token` is required for protected writes. `kg_module_init` hides this via context, but direct callers (most `*_kg_wiring.c` bridges currently pass `0` as `admin_token`) won't survive if security tightens. **Mitigation**: every new wiring in W2+ should read the token via `brain->internal_kg_admin_token` up-front, not pass 0.

6. **Typedef / include circularity.** `internal_kg_enabled` lives on `brain_t` (800+ fields). Several bridge files in `cognitive/immune/` and `cognitive/ethics/` use forward declarations (`struct brain_immune_system;`) specifically to avoid circular includes. Pulling in `core/brain/nimcp_brain_kg.h` may force include-chain reshuffling — plan for at least one refactor iteration per cognitive subdir. **Mitigation**: W11 (ethics/immune) is isolated from W6–W8 to absorb the churn.

7. **"Knowledge" is overloaded.** `cognitive/knowledge/nimcp_knowledge.c` is a separate concept-store, `cognitive/knowledge/nimcp_kg_reader.c` reads external JSONL, and `brain->internal_kg` is the target. A consumer might confuse the three. **Mitigation**: in W7, the first commit should merge `cognitive/knowledge/` into `internal_kg` as its primary backing store, or document the three-store model explicitly. This is arguably a design call, not an audit-call, but flagging.

8. **Modules where "wiring" requires inventing semantics.** Several are classified UNWIRED but have no obvious symbolic content and may belong in N/A after discussion: `nimcp_pr_pink_noise_bridge.c`, `nimcp_self_repair.c`, `nimcp_mirror_habituation.c`, `nimcp_thalamic_bridge.c` (per-module), `health/nimcp_rcog_health.c`. Treat as provisional UNWIRED pending first attempt; escalate to N/A if the agent cannot articulate what fact they'd emit.

9. **`kg_module_init` modules double-count.** Seven modules use the helper (executive, working_memory, introspection, reasoning_integration, self_model, attention, fep_orchestrator) and register a self-node; the helper does the `brain_kg_add_node` for them. Don't also add a manual `brain_kg_add_node` for the same module — check `kg_has_node(ctx)` first.

10. **Build and link impact.** Each new `*_kg_wiring.c` needs a `CMakeLists.txt` entry in its region/module dir; several region dirs don't have a `bridges/` subdir yet (occipital, broca, somatosensory). W3 will need to create the subdir + CMake glue before writing code. Template: `regions/cerebellum/bridges/CMakeLists.txt`.

---

**Next actions (not executed here):**

1. Create the node-naming registry (risk 3) as a new `docs/claude/kg-node-naming-registry.md` before starting W3.
2. Pick W1 (statue finish) as the first parallel task — low risk, bounded scope.
3. Proceed with W6/W7 in parallel as the highest-value waves for user-visible symbolic reasoning.
