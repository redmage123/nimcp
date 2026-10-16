# Option 1 Architectural Rebuild

**Status**: ACTIVE — 6 parallel slices, in-flight 2026-05-19
**Goal**: Move learning out of the SNN↔language bridge. Bridge becomes a transport. SNN owns concept patterns. Reward shaping replaces self-confirmation. Stage scaffolding constrains output to developmental level.

## Why this exists

Pre-rebuild, the SNN↔language bridge held a `concept_pop × word_pop` STDP weight matrix that tried to learn cross-modal associations between SNN concept activations and lexical entries. This was wrong on three counts:

1. **Wrong location for the learning**: in biology, sensory pattern→word grounding lives in association cortex (a single distributed code), not in a transport layer between subsystems. Quian Quiroga concept cells fire for image, written word, and spoken word of the same referent — same population.
2. **Two failure surfaces**: the bridge could collapse independently of either endpoint, with no corrective pressure from the SNN or the language system.
3. **No cross-modal binding enforcement**: visual encode allocated its own concept pop, text comprehend allocated a different one; the bridge mapping between concept_pop and word_pop never tied them together.

Result: outputs bore no relationship to inputs (modalities ran on parallel uncrossed tracks); mode collapse onto broadcast words ("grand gel telephotography"); supervised image+label training had near-zero effect on text production.

## Architecture (after rebuild)

```
SENSORY → SNN cortex layers → concept population code (sparse, stable, cross-modal)
                                       ↓
                              bridge (transport only)
                                       ↓
                  language system: lexicon indexes concept_pop_id ↔ word_id
                                       ↓
                              production / comprehension
```

**Key invariant**: a referent (e.g. "leaves") corresponds to **one** SNN concept population. That population fires for the image, the spoken word, and the written word. The lexicon stores `word_id → concept_pop_id` and `concept_pop_id → word_id` indexes. Bridge ferries spikes; it does not learn.

## Slices (parallel)

### Slice A — Bridge as transport
**Owns**: `src/snn/bridges/nimcp_snn_language_bridge.[ch]`
**Goal**: strip all STDP / weight-matrix / learning from the bridge. Keep only spike routing.

**Remove**:
- `apply_stdp`, `learn_text_bigrams`, `comprehend_stdp`, `strengthen_binding`, `prune_weak_bindings`
- The `concept_pop × word_pop` weight matrix on the bridge
- All STDP tau/A/decay tunables on bridge config
- Bridge-side trigram state (move trigram into grounded_language if still needed; this is a separate-PR concern)

**Keep**:
- Spike routing: `bridge_route_concept_spikes_to_words()`, `bridge_route_word_spikes_to_concepts()` — pure pop→pop spike forwarding
- Lateral inhibition for K-WTA at output (this is local dynamics, not learning)
- Telemetry counters (count messages, not weight updates)
- All connection / lifecycle / neuromod-connect machinery

**New public API**:
```c
/* Pure transport: forward spikes from a list of active concept pops
 * into the corresponding word pops via SNN's own projection synapses.
 * The bridge does NOT own weights — projection synapses live in the SNN. */
int snn_language_bridge_route_concept_to_word(
    snn_language_bridge_t* bridge,
    const uint32_t* concept_pop_ids, size_t n_concepts,
    uint32_t* word_pop_ids_out, size_t* n_words_out, size_t max_out);

int snn_language_bridge_route_word_to_concept(
    snn_language_bridge_t* bridge,
    const uint32_t* word_pop_ids, size_t n_words,
    uint32_t* concept_pop_ids_out, size_t* n_concepts_out, size_t max_out);
```

**Migration**: callers in `nimcp_communication_cascade.c` and `nimcp_brain_learning.c` use the new API. The old `snn_language_bridge_decode_with_lateral_inhibition` becomes a thin wrapper that routes spikes + applies inhibition only.

**Tests**: add `tests/snn_bridge_plumbing_only.c` — verify spike routing works, verify no weight state survives a brain_save+load round-trip (because there are no weights).

---

### Slice B — Cross-modal population binding
**Owns**: NEW `include/cognitive/grounded_language/nimcp_concept_registry.h` + `src/cognitive/grounded_language/nimcp_concept_registry.c`

**Goal**: a referent (text label, visual feature hash, audio feature hash) maps to **one** `concept_pop_id`. First-write wins; subsequent registrations of the same referent return the existing id.

**API**:
```c
typedef struct concept_registry concept_registry_t;
typedef uint32_t concept_pop_id_t;

concept_registry_t* concept_registry_create(size_t initial_capacity);
void concept_registry_destroy(concept_registry_t* reg);

/* Find or allocate. Returns the canonical concept_pop_id for this referent.
 * First successful lookup binds; subsequent calls return the same id. */
concept_pop_id_t concept_registry_intern_text(concept_registry_t* reg, const char* text);
concept_pop_id_t concept_registry_intern_visual(concept_registry_t* reg, const float* features, size_t n);
concept_pop_id_t concept_registry_intern_audio(concept_registry_t* reg, const float* features, size_t n);

/* Cross-modal binding: tell the registry that two modalities refer to the same thing.
 * Called from training when (image, label) or (audio, label) co-occurs. */
int concept_registry_bind_modalities(concept_registry_t* reg, concept_pop_id_t a, concept_pop_id_t b);

/* Lookup: get all modality fingerprints associated with a pop. */
size_t concept_registry_get_bindings(concept_registry_t* reg, concept_pop_id_t pop, ...);
```

**Wiring**:
- `brain->concept_registry` field, created in `nimcp_brain_init_cognitive_engines.c`
- `grounded_language_register_concept(text)` delegates to `concept_registry_intern_text`
- During sensory submit with label (visual/audio paths in `brain_submit_sensory_*`), the path now: (1) intern modality features, (2) intern text label, (3) bind those two ids
- The concept_pop_id corresponds to an actual SNN population — allocated lazily from a pool of free pops in `snn_concept_pool` (extension to existing concept pool)

**Migration**: existing visual cortex / audio cortex paths gain a `label` param (already passed in some places). Where label is NULL, allocate a "perceptual-only" pop that can later be bound when a label arrives.

**Tests**: `tests/concept_registry_binding.c` — bind image→pop, bind text→pop, verify they coalesce; submit image then text, verify same pop_id returned.

---

### Slice C — Caregiver critic + RL pipeline
**Owns**: NEW `scripts/caregiver_critic.py` + integration in `scripts/immerse_athena.py`

**Goal**: external reward signal on every brain production. Recasts on errors.

**`caregiver_critic.py` API**:
```python
class CaregiverCritic:
    def __init__(self, stage: int, vocab: dict[str, dict]): ...

    def evaluate(self, prompt: str, response: str) -> CriticVerdict:
        """Returns reward ∈ [-1, +1] and optional recast (correct form)."""

@dataclass
class CriticVerdict:
    reward: float        # in [-1, +1]
    recast: str | None   # corrected form, if applicable
    reason: str          # human-readable diagnostic
```

**Rules per stage**:
- Stage 0: reward if response is exactly one word from the stage-0 vocabulary; punish gibberish/non-word.
- Stage 1: reward two-word "agent + action" or "modifier + noun" combinations from stage-1 vocab; recast on word-order errors.
- Stage 2: reward 3–4 content-word productions, basic SVO grammar; recast on agreement / structure errors.
- Stage 3+: layered LLM judge (use Claude CLI via the existing parent narration pipeline; fix the parent pipeline first if broken).

**Integration in `immerse_athena.py`**:
```python
# After every brain.respond(prompt):
verdict = critic.evaluate(prompt, response)
brain.apply_reward_learning(verdict.reward, last_decision_state)
if verdict.recast:
    # Treat recast as supervised pair with high DA
    brain.learn_vector(features=prompt_features, target=recast_target,
                       label=verdict.recast, lr_mult=2.0)
```

**Tests**: `scripts/test_caregiver_critic.py` — verify stage 0 rejects gibberish, stage 1 detects word-order errors, recasts are grammatical.

---

### Slice D — Self-train external-reward gating
**Owns**: `cascade_stage_self_train` in `src/language/nimcp_communication_cascade.c`

**Goal**: `cascade_self_train` only fires when an external reward is present and positive. Removes the autoconfirm loop.

**Changes**:
1. Add field `float last_external_reward;` `uint64_t last_external_reward_us;` to `brain_struct` (in `include/core/brain/nimcp_brain_internal.h`, appended at end with `_Static_assert` sentinel).
2. `cascade_stage_self_train` reads `brain->last_external_reward`; if missing or stale (>5 s) or below threshold (default 0.5) → skip self-train.
3. New RPC `_cmd_set_last_external_reward` in `brain_daemon.py` + public setter `nimcp_brain_set_last_external_reward(brain, reward)` in `src/api/nimcp_part_core.c` + Python binding.
4. New tunable `cascade_self_train_reward_threshold` (default 0.5) — `set_cascade_self_train_tunables` extended.

**Tests**: `tests/cascade_self_train_gated.c` — verify self-train skipped when reward absent / stale / below threshold; verify it fires when reward fresh + high.

---

### Slice E — Stage-anchored developmental scaffolding
**Owns**: NEW `include/cognitive/grounded_language/nimcp_stage_table.h` + integration in `cascade_stage_motor` + `grounded_language` vocab mask.

**Goal**: per-stage constraints on what the brain *can* produce.

**Stage table**:
```c
typedef struct stage_constraints {
    uint32_t stage_idx;
    size_t max_visible_vocab;      // lexicon mask: only first N tokens active
    uint16_t min_produce_words;
    uint16_t max_produce_words;
    uint32_t allowed_grammar_mask; // bitmask of grammar template ids
} stage_constraints_t;

const stage_constraints_t* stage_table_get(uint32_t stage_idx);
```

**Stage 0**: max_vocab=50, min=1, max=1, grammar=NOUN_ONLY.
**Stage 1**: max_vocab=200, min=2, max=2, grammar=NOUN_ONLY|VERB_ONLY|AGENT_ACTION|MODIFIER_NOUN.
**Stage 2**: max_vocab=800, min=3, max=4, grammar adds SVO.
**Stage 3+**: progressive expansion.

**Wiring**:
- `grounded_language_set_active_vocab_mask(stage)` — installs a mask the lexicon honors during production word selection.
- `cascade_stage_motor` reads `brain->current_stage`, applies length cap from stage table, runs Broca CYK against allowed grammar templates, rejects (returns to cascade settle loop) if violating.
- Stage advancement: existing immerse_athena.py logic continues to set the stage; add a production-diversity gate as a recommendation but not blocking (yet).

**Tests**: `tests/stage_scaffolding.c` — set stage=0, verify cascade rejects 2-word productions; set stage=1, verify vocab mask restricts retrieval.

---

### Slice F — Negative-DA support
**Owns**: DA clamp + verification in `src/snn/bridges/nimcp_snn_language_bridge.c` (sole touch) **plus** any SNN STDP code that gates on DA.

**Goal**: punishment as well as reward. Allow DA ∈ [-200, +200]. Verify the SNN's three-factor STDP behaves correctly when DA < 0 (synaptic depression, not weight inversion or NaN).

**Changes**:
1. `set_da_modulation_gain` clamp `[0, 200]` → `[-200, +200]`.
2. Audit SNN STDP code paths (`src/snn/nimcp_snn_stdp.c` and any plasticity code that multiplies by neuromod): ensure the weight update is `dw = lr * pre * post * DA`, and synaptic floor `w_min` (typically 0 or epsilon) prevents weights going negative.
3. New tunable `da_min` (default -200.0) on bridge config.

**Tests**: `tests/snn_negative_da.c` — fire pre+post pair, deliver DA=+1 → weight increases; fire same pair, deliver DA=-1 → weight decreases; verify weight clamps at w_min, doesn't go negative.

---

## Inter-slice contracts

| Producer | Consumer | What |
|---|---|---|
| Slice B | Slice A | `concept_pop_id` allocation — bridge transports spikes between pop_ids the registry owns |
| Slice B | Slice E | vocab mask is per-stage but pop_ids stay stable across stages |
| Slice C | Slice D | reward signal flows into `brain->last_external_reward` which D gates self-train on |
| Slice C | Slice F | C calls `apply_reward_learning` with possibly-negative reward; F enables it to propagate |
| Slice E | Slice C | stage→constraint table is the contract C's critic reads from |

## Build invariants (all slices)

1. `make nimcp -j4` must succeed cleanly in your worktree before declaring done.
2. New tests added for the slice's behavioral surface — at least 1 unit test, 1 regression test.
3. `_Static_assert` sentinels on any struct you append to (especially `brain_struct`).
4. No `git stash`. Worktrees are isolated; commit your work.
5. If you discover a missing API (phantom) in a header, check `src/utils/` for an existing primitive before rolling your own.
6. ABI append-only: new fields go at the **end** of structs, never inserted.

## Out of scope (do not touch)

- Brain init wave ordering (other than registering the new concept_registry in cognitive_engines wave)
- LGSS / ethics modules
- GPU paths (CB conductance migration is unrelated)
- Anything in `src/swarm/`, `src/edge/`, `src/io/`
- The `data/` curriculum corpus content (Slice E only adds masks, doesn't change corpus)

## Merge order (post-completion)

B → A → F → D → E → C

Walkthroughs run after merge.
