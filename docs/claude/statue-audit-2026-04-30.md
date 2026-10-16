# Cognitive Module Statue Audit — 2026-04-30

Methodology: static analysis via grep/Read across `src/`. A statue is a module
that's created and called in a hot path, but whose output never flows into
loss / decision / persistent state.

Verdict legend:
- **WIRED** — output flows into something that affects loss / decision / weights
- **HALF-WIRED** — created and called, but output is logged-only / discarded /
  consumed only by another statue
- **DEAD** — created but no caller in production (only tests, or no caller at all)
- **MISSING** — no symbol / brain field with that name found

## Audit table

| #  | Module | Verdict | Init site | Hot-path caller | Output consumer | Notes |
|----|--------|---------|-----------|-----------------|-----------------|-------|
| 1  | introspection | WIRED | src/core/brain/factory/init/nimcp_brain_init_monitoring.c:160 | src/core/brain/nimcp_brain_part_core.c:1333 (`wellbeing_assess_distress`) | src/cognitive/immune/nimcp_wellbeing_immune_bridge.c:556-577 (cytokine release) | Distress → IL1/TNF cytokines if score≥threshold; real biological side effect |
| 2  | theory_of_mind / tom | WIRED | src/core/brain/factory/init/nimcp_brain_init_cognitive.c (lazy via accessors) | src/core/brain/nimcp_brain_part_core.c:3522-3549 (`tom_update_self_model`, `tom_infer_emotion`) | persistent ToM model state used in further inference + checkpoint | Updates self-model state across calls |
| 3  | imagination_engine | WIRED | src/core/brain/factory/init/nimcp_brain_init_cognitive_engines.c:163 | src/core/brain/nimcp_brain_part_core.c:2784-2803 | `decision->confidence *= IMAGINATION_FAIL_PENALTY` on sim failure | Modulates confidence on prospective sim failure |
| 4  | reasoning_engine | WIRED | factory init (cognitive_engines) | src/core/brain/nimcp_brain_part_core.c:2699-2715 | `decision->confidence = old*(1-rw) + reasoning_conf*rw` | Direct confidence blend |
| 5  | emotion / emotional_learning | WIRED | src/core/brain/factory/init/nimcp_brain_init_edge.c:322 | src/core/brain/nimcp_brain_part_core.c:1781 (`get_arousal`) | arousal modulates `local_features` recall blend ratio (line 1791-1811) | Arousal feeds into trauma recall dampening |
| 6  | mental_health_monitor | WIRED | factory init (cognitive) | src/core/brain/nimcp_brain_part_core.c:3573-3596 | `decision->confidence *= 0.5F`, appends `[QUARANTINE]` to explanation | Quarantine mode halves confidence |
| 7  | recursive_cognition | MISSING | no `brain->recursive_cognition` field | only mentioned in cognitive_transcript.c:34 as a label | n/a | No standalone module — `max_recursion_depth` is a config field in TOM/security bridges |
| 8  | empathetic_response | HALF-WIRED | factory init | src/core/brain/nimcp_brain_part_core.c:4759-4761 | response struct cast to `(void)` — explicitly discarded | Comment says "Response is discarded — goal is to activate the subsystem" |
| 9  | middleware_controller | WIRED | src/core/brain/factory/init/nimcp_brain_init_monitoring.c:274 | src/core/brain/nimcp_brain_part_core.c:4788 | `pattern_match_callback_t` subscribers fire (W9-finish tracer) | Fires per decision; subscribers can side-effect |
| 10 | inner_speech | HALF-WIRED | src/core/brain/factory/init/nimcp_brain_init_edge.c:335 | src/core/brain/nimcp_brain_part_core.c:4593 (`nimcp_inner_speech_refine`) | writes back into `decision->output_vector` (in-place); but inner_dialogue (a separate module) is the one that affects decision->confidence | Refinement passes output through but native_language_create is gated on `native_language_enabled`; refine is a no-op if internal language model is undertrained |
| 11 | episodic_replay | WIRED | src/core/brain/factory/init/nimcp_brain_init_edge.c:321 | src/core/brain/nimcp_brain_part_core.c:4602; src/core/brain/learning/nimcp_brain_learning.c:3320 | replay buffer consumed during sleep_wake (sleep_wake.c:730) | Sleep consolidation reads buffer |
| 12 | world_model (separate world_model_trainer) | HALF-WIRED w/ BUG | src/core/brain/factory/init/nimcp_brain_init_edge.c:326 | src/core/brain/learning/nimcp_brain_learning.c:3362 (`record_transition`) + line 3367 (`train_predictor`) | `decision->confidence *= 1+0.1*pred_error` | **BUG: `record_transition` is called with `action=NULL, action_dim=0`. `nimcp_world_model_trainer.c:112-117` returns -1 when action is NULL, so history is never written. `train_predictor` then early-returns on `history_count==0`. `pred_error` is therefore always 0, and the confidence boost never fires.** |
| 13 | attention (cognitive, output_attention) | WIRED | src/core/brain/factory/init/nimcp_brain_init_edge.c:327 | src/core/brain/nimcp_brain_part_core.c:4558 (`nimcp_oa_attend`) | writes back into `decision->output_vector` | Trained on labels via `nimcp_oa_train_attention` in learning loop |
| 14 | working_memory (wm_scratchpad) | WIRED | factory init (edge) | src/core/brain/nimcp_brain_part_core.c:4612 (`nimcp_wms_read_all`) | `decision->output_vector[i] = 0.95f*v + 0.05f*wm_context[i]` | Subtle 5% blend into output |
| 15 | analogical_transfer | WIRED | src/core/brain/factory/init/nimcp_brain_init_edge.c:329 | src/core/brain/nimcp_brain_part_core.c:4567 (`apply_transfer`) | overwrites `decision->output_vector` when match found | Pattern stored in learning loop:3414 |
| 16 | multi_timescale_memory (multiscale) | WIRED | src/core/brain/factory/init/nimcp_brain_init_edge.c:330 | src/core/brain/nimcp_brain_part_core.c:1767 (`query_all`) | `recall_result.embedding` blended into `local_features` when similarity>0.8 | Query-and-blend on inference |
| 17 | contrastive_self_learning | WIRED | src/core/brain/factory/init/nimcp_brain_init_edge.c:323 | src/core/brain/learning/nimcp_brain_learning.c:3375 (`record`) | recorded patterns affect future learning batch via accumulator | Recording-only on inference path; consumed in training |
| 18 | self_curriculum | WIRED | src/core/brain/factory/init/nimcp_brain_init_edge.c:324 | src/core/brain/learning/nimcp_brain_learning.c:3333 (`update_uncertainty`) | curriculum drives sample selection in immerse_athena.py via Python binding | Internal uncertainty map persisted |
| 19 | dynamic_arch_search | HALF-WIRED | src/core/brain/factory/init/nimcp_brain_init_edge.c:325 | src/core/brain/nimcp_brain_part_core.c:4648 (`record_activation`) + learning.c:3400 | activation recorded but no architecture mutation in production code path | Statue-shaped: builds activity heatmap that is never consumed for arch changes (only via Python introspection?) |
| 20 | social_interaction (social_bond_system) | WIRED | src/core/brain/factory/nimcp_brain_parallel_init.c:630 | src/core/brain/learning/nimcp_brain_learning.c:4601 (`social_update`) + reasoning_convergent.c:956 (`reasoning_affective_evaluate_social`) | `affect.confidence_delta` sums into evidence accumulator → reasoning chain confidence | Real consumer in convergent reasoning |
| 21 | emergent_language | DEAD | src/cognitive/language/nimcp_emergent_language.c:199 (`_create` defined) | no caller of `nimcp_emergent_language_create` outside its own .c | n/a | No `brain->emergent_language` field; not instantiated anywhere |
| 22 | genius_profiles | DEAD-as-runtime | src/core/brain/genius/nimcp_genius_profiles.c:3018 (`genius_create_brain`) | only at brain creation as config-tweaker (`genius_brain_create`) | n/a — modifies brain_config_t at construct, then unused | Profile is consumed once at init; no runtime instance attached to brain |
| 23 | pink_noise_bridges (sfa + 12 plasticity bridges) | DEAD | src/core/neuron_models/nimcp_sfa_pink_noise_bridge.c:18 (`_create`); src/plasticity/*/nimcp_*_pink_noise_bridge.c | `sfa_pink_noise_create` has no caller in production code; cycle_coordinator stores `pink_noise_bridge` but never calls update | n/a | 13 *_pink_noise_bridge.c files; only self-referential calls. The cycle_coordinator field is set but never stepped. (Aim memory ref: pink_noise_create has Nyquist rule, but the wrappers around it are not used) |
| 24 | fractal_dfa / fractal_cognitive | WIRED (DFA only) | DFA: callable as a function, no instance state. fractal_cognitive: BRIDGE_BOILERPLATE registered but no `_create` called | DFA: src/training/nimcp_unified_training.c:2491 + inference.c:1373 | DFA: `mgr->current_lr *= 0.8/0.5` (auto-LR adjust) — real loss effect. fractal_cognitive: DEAD | DFA = WIRED. fractal_cognitive standalone module = DEAD (no instance) |
| 25 | quantum_annealing | WIRED (constrained) | src/core/brain/factory/nimcp_brain_factory.c:1513 | src/core/brain/learning/nimcp_brain_learning.c:4671 (`quantum_anneal`) | `h->weight = 0.1*opt + 0.9*current` for first ~10 neurons / 100 weights | WIRED but capped at 1000 weights, 10% blend, periodic only |
| 26 | neuromodulator_system | WIRED | src/core/brain/biological/nimcp_brain_biological.c:542 | src/core/brain/nimcp_brain_part_core.c:1531 (`brain_tick_neuromod`); learning.c:1763 (`neural_network_set_neuromodulator_system`) | DA/NE/5HT/ACh/HT/HAB modulate STDP/BCM rules and inhibition | Real plasticity gate |
| 27 | neural_substrate | WIRED | src/core/brain/factory/init/nimcp_brain_init_substrate_thalamic.c:166 | src/snn/nimcp_snn_network.c:1162,2491 (`substrate_apply_tau`); snn_training.c:842 (`substrate_apply_lr`) | tau_eff and learning rate are mutated for SNN/CNN/LNN forward | Direct dynamics modification |
| 28 | dendritic_compartment | HALF-WIRED | src/core/brain/factory/init/nimcp_brain_init_plasticity.c:227 | src/core/brain/learning/nimcp_brain_learning.c:4936 (`dendritic_tree_update`) | dendritic_tree internal state advanced; no read-back into neuron firing in production hot path | Update runs but result not consumed by main net (cortical_dendritic is a separate fully-wired path for cortical columns) |
| 29 | homeostatic_plasticity | HALF-WIRED | src/core/brain/factory/init/nimcp_brain_init_plasticity.c:169 | src/core/brain/learning/nimcp_brain_learning.c:4901,4913 (`homeostatic_controller_update`) | **CPU path passes weights=NULL → only ip_states/meta_states updated, NEVER read elsewhere. GPU path applies scaling to a temp `firing_rates` buffer that is `nimcp_free`'d immediately afterward.** Stats are tracked. | Critical statue: subsystem runs but mutates nothing persistent. SNN homeostatic is a different module and IS wired (see CLAUDE.md tight-bounds). |
| 30 | STDP / BCM / eligibility_traces | WIRED (STDP/BCM); DEAD (eligibility_traces brain attachment) | STDP: factory init plasticity_bridges.c:101 | STDP: src/core/neuralnet/nimcp_neuralnet_learning.c:573,770; BCM: line 437,647,830 | STDP/BCM: `handle->weight = ...` direct mutation. eligibility: `neuromod_plasticity_create` has no caller — DEAD as a brain field | STDP/BCM via neuralnet_learning.c is the real path. neuromod_plasticity_bridge is a separate construct that's DEAD (no `brain->neuromod_plasticity_bridge` field) |
| 31 | brain_immune | WIRED | src/core/brain/biological/nimcp_brain_biological.c (immune init) | src/utils/fault_tolerance/nimcp_health_agent_part_helpers.c:370 (`brain_immune_tick`); cytokine release across many bridges | cytokine release affects metabolic/plasticity bridges + audit logs | Multiple cytokine consumers; tick driven by health agent |
| 32 | dale_principle_enforcement | DEAD | src/training/nimcp_auto_architecture.c:284 (`enforce_dales_law = false`) | only set to false in auto-architecture config; no enforcer in hot path | n/a | The constraint flag exists but nothing reads it during training |
| 33 | gap_junctions | RECLASSIFIED: glial-internal (not a statue) | src/glial/sleep/nimcp_astrocytes_sleep_bridge.c:163 (factor write) + src/gpu/substrate/nimcp_substrate_cpu.c:539 (kernel read) | bridge `gap_junction_coupling_factor` consumed inside calcium IP3 wave compute (the documented biological role of astrocytic gap junctions) | factor IS read by the kernel that owns it; not a hot-path-orphaned statue | Reviewed 2026-04-30 (Fix-3 audit) — wiring into the SNN LIF hot path is intentionally NOT done: (1) `snn_population_t` has no per-neuron neighbor adjacency to weight by, (2) the SNN step has 5 execution branches (GPU/CPU-lightweight/CPU-legacy/sparse/parallel) and the CB-migration just landed (2026-04-26) — adding another conditional branch on this hottest 1.9M-neuron loop is a real perf regression, (3) gap junctions are a glial network-coordination signal here, not a per-neuron coupling term in the SNN. The factor's true consumer is the calcium IP3 wave kernel, which IS its biological match. Removed from the statue list. |
| 34 | snn_attention_bridge | DEAD | src/snn/bridges/nimcp_snn_attention_bridge.c:80 (`_create` defined) | `snn_attention_bridge_create` has zero callers in src/ outside its own file. cycle_coordinator declares the field but no init wires it | n/a | The lightweight CSR pop migration mentioned in CLAUDE.md may have replaced this with a different mechanism; this bridge module is orphaned |
| 35 | snn_mirror_neurons_bridge | MISSING | no file `nimcp_snn_mirror_neurons_bridge.c` in src/snn/bridges/ | n/a | n/a | mirror_neurons (top-level) IS wired via mirror_neurons_execute_action → mirror_substrate_apply_activity_to_myelin (myelin update). The SNN-specific bridge variant doesn't exist as a file |
| 36 | snn_emotion_bridge | DEAD | src/snn/bridges/nimcp_snn_emotion_bridge.c:47 (`_create`) | no caller of `snn_emotion_bridge_create` in production | n/a | Emotion-on-SNN integration goes through nimcp_emotion_snn_bridge.c (a different bridge in src/cognitive/emotion/) |
| 37 | snn_working_memory_bridge | DEAD-or-uncreated | grep finds `nimcp_working_memory_snn_bridge.c` only — name is mirrored | working_memory_snn_bridge has BRIDGE_BOILERPLATE but no `_create` callers found in production | n/a | Module file exists but no instantiation — possibly a statue at the SNN binding side |
| 38 | brain_native_language | HALF-WIRED | src/core/brain/factory/init/nimcp_brain_init_edge.c:285 | src/core/brain/nimcp_brain_part_core.c:4543 (`nimcp_language_generate`) | **`(void)native_text` — explicitly discarded with `Available for caller via future API` comment** | Generated text never returned/stored on decision. inner_speech also calls `language_generate` but that result IS used to refine output_vector |
| 39 | DK-A IDK gate (abstain) | MISSING | no `idk_gate`, `abstain_threshold`, or `i_dont_know` in production code | n/a | n/a | The "DK-A IDK gate (already shipped)" was stated in the prompt but no symbol matches. The closest is OOD detector confidence reduction (which IS wired) and `knowledge_part_core.c:374` "I don't know about '%s' yet." string in explanation buffer. |
| 40 | DK-C temperature calibration / reasoning_calibration | HALF-WIRED | src/cognitive/reasoning/nimcp_reasoning_calibration.c:138 | src/cognitive/reasoning/nimcp_reasoning_convergent.c:1223 (`reasoning_calibration_get_adjustment`) | `contrib->result_confidence = conf*scale + bias` | `_get_adjustment` IS read, but **`reasoning_calibration_record` (the only writer) has zero callers** — calibration table is never updated, so scale=1.0/bias=0.0 always. Read-only consumer with no producer. |

## Summary

- **WIRED**: 17
- **HALF-WIRED**: 11
- **DEAD**: 9
- **MISSING**: 3

(Total = 40 rows; some rows split a verdict — counting the dominant verdict per row.)

Verdict count breakdown by row:
WIRED: #1,2,3,4,5,6,9,11,13,14,15,16,17,18,20,24(DFA),25,26,27,30(STDP/BCM),31
HALF-WIRED: #8,10,12,19,28,29,33,38,40, plus #24 (fractal_cognitive part), #30 (eligibility-as-brain-field)
DEAD: #21,22,23,32,34,36,37, plus #24 (fractal_cognitive instance), #30 (neuromod_plasticity_bridge as brain field)
MISSING: #7, #35, #39

## Top 10 highest-suspicion HALF-WIRED modules

(ranked by likelihood that fixing the wiring moves training loss)

1. **#12 world_model_trainer (BUG)** — `record_transition(action=NULL, action_dim=0)` always returns -1, so history is empty, training never runs, `pred_error` is always 0, and the `decision->confidence *= 1+0.1*pred_error` boost never fires. **Fix: pass a non-null action vector** (decision->output_vector or features).
2. **#29 homeostatic_plasticity** — CPU path passes `weights=NULL` so synaptic scaling is a no-op; GPU path scales a temp buffer that's freed before any persistent state captures it. The whole brain-level homeostatic loop runs but mutates nothing. **Fix: persist scaling factors back into neuron thresholds or pass per-population weight slices.**
3. **#40 reasoning_calibration** — get_adjustment is read in convergent reasoning but `reasoning_calibration_record` has zero callers. Scale=1.0/bias=0.0 forever. **Fix: call `reasoning_calibration_record(cal, module_name, prediction, outcome)` after each session.**
4. **#19 dynamic_arch_search** — records activations every decision but never triggers an architectural mutation. The whole point of the module (search over architectures) is missing. **Fix: gate `nimcp_dynamic_arch_propose_change` on accumulated activation entropy.**
5. **#28 dendritic_compartment** — `dendritic_tree_update` runs each step but result is never read by the main neural network's forward pass. **Fix: read `dendrite_compute_somatic_current` and inject into neuron->state, or document that brain-level dendritic is intentional dead code (cortical columns have their own).**
6. **#38 brain_native_language** — `(void)native_text` discards generated text. **Fix: store on `decision->explanation` (append) or expose via a `decision->native_text` field.**
7. **#10 inner_speech** — refine writes back to output_vector but only fires when `native_language_enabled && decision->output_vector`; if the underlying language model has no learned vocab the refine is identity. **Fix: condition on language model maturity / make idempotent + cheap.**
8. **#8 empathetic_response** — explicitly discards response with comment "goal is to activate the subsystem". **Fix: hook the response back into `decision->explanation` or use it to modulate confidence based on detected distress.**
9. **#33 gap_junctions** — RECLASSIFIED 2026-04-30: not a statue. The factor IS consumed by the calcium IP3 wave kernel (its biological role). SNN-LIF wiring deferred: no neuron neighbor adjacency, 5 SNN step branches post-CB, hot-path perf regression on 1.9M neurons. See row 33 notes.
10. **#30 eligibility_traces (as brain field)** — module exists but `neuromod_plasticity_create` has no caller. **Fix: wire into `nimcp_brain_init_neuromod` and call `neuromod_plasticity_apply_reward_pe` from `brain_apply_reward_learning`.**

## Top 5 cheapest fixes

1. **#12 world_model_trainer**: 1-line change — pass `target` as both state and action (or use `decision->output_vector` as action). Will start populating history immediately.
2. **#38 brain_native_language**: 2-line change — `strncpy(decision->explanation + strlen(decision->explanation), native_text, ...);` to append.
3. **#40 reasoning_calibration**: ~5-line change — after `reasoning_chain_evaluate` completes, call `reasoning_calibration_record(cal, contrib->module_name, contrib->result_confidence, decision->confidence)`.
4. **#8 empathetic_response**: ~3-line change — when arousal > 0.85, apply `decision->confidence *= 0.9` (signal high distress should be cautious) instead of `(void)resp`.
5. **#29 homeostatic_plasticity**: ~10-line change — replace the early-freed `firing_rates` buffer with a persistent `brain->homeostatic_firing_rates` and after the GPU update, copy modulated rates back as scaling factors that get applied during the next plasticity coordinator run. Or just skip the entire block and document it as removed.

## Caveats / Could-not-verify

- **#19 dynamic_arch_search** — there may be a Python-binding callsite in `immerse_athena.py` that reads the activation map and triggers config mutation; I did not audit Python.
- **#37 snn_working_memory_bridge** — the file `nimcp_working_memory_snn_bridge.c` exists with BRIDGE_BOILERPLATE but I did not deeply trace whether the mesh adapter framework auto-registers it via `MESH_ADAPTER_CATEGORY_MEMORY`. If the mesh adapter dispatches to all registered modules, this could be HALF-WIRED via that path.
- **#22 genius_profiles** — `genius_brain_create` is one-shot at brain construction and the `genius_math_orchestrator_t` is created internally inside that, so the orchestrator instance might persist on `brain->orch` somewhere I didn't find. Treat as DEAD-as-runtime with low confidence.
- **#39 DK-A IDK gate** — the prompt asserts this was "already shipped". I could not find any matching symbol. Either it is named differently than I searched (`abstain`, `idk`, `dk_gate`, `confidence_threshold`...) or the OOD detector at `nimcp_brain_part_core.c:1635` is the de-facto IDK gate. The OOD detector IS wired (modulates `decision->confidence`) — see row #unlisted below.

## Bonus finding (not in original 40)

**OOD detector** (`brain->ood_detector`) is fully WIRED: `nimcp_ood_detect` runs at line 1636, then `decision->confidence *= ood_result.confidence_adjustment` at line 2525. If "DK-A IDK gate" was renamed to OOD detector, mark item 39 as WIRED.

**inference_health (s_inference_health, fractal-DFA based)** is HALF-WIRED: `nimcp_inference_health_check` returns a health classification (1=OPTIMAL ... 4=OSCILLATING) but the return value and `s_inference_health.health` field are NEVER READ ANYWHERE. The DFA exponents are computed and discarded — pure observability statue. Same root pattern as the FNO `_last_loss=0` finding.
