/**
 * @file nimcp_grounded_language_internal.h
 * @brief Internal layout of grounded_language_t — shared between the
 *        primary implementation and the persistence sidecar.
 * @date 2026-05-04
 *
 * WHAT: Exposes `struct grounded_language` and `gl_context_t` so that
 *       sibling files in src/language/ can serialize / restore lexicon
 *       state without going through the public opaque-handle API.
 * WHY:  The public API (grounded_language.h) keeps the struct opaque
 *       on purpose — external callers shouldn't depend on the layout.
 *       But the persistence sidecar needs to walk every lexicon entry
 *       field-by-field, which the public API doesn't allow.
 *       Pulling the struct into an *internal* header keeps the opaque
 *       contract for the rest of the codebase while letting persistence
 *       and the primary impl share the layout via the same #include.
 * HOW:  Mirrors the orchestrator-internal pattern already established
 *       in src/language/nimcp_language_orchestrator_internal.h.
 *
 * NOTE: Do NOT include this header from anywhere outside src/language/.
 *       It is not installed; relying on it from another module would
 *       reintroduce the layout coupling we deliberately avoid.
 */

#ifndef NIMCP_GROUNDED_LANGUAGE_INTERNAL_H
#define NIMCP_GROUNDED_LANGUAGE_INTERNAL_H

#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"

#include <stdint.h>
#include <pthread.h>  /* TC-12: per-gl mutex for anaphora + spectrum */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Context buffer for tracking conversation state.
 *
 * Mirrors the (formerly file-local) typedef in nimcp_grounded_language.c.
 */
typedef struct {
    float*   context_vector;          /**< Running context [semantic_dim] */
    float    context_strength;        /**< How strong current context is */
    uint32_t words_in_context;        /**< Words seen in current context */
    uint64_t last_concepts[16];       /**< Recent concept activations */
    uint32_t last_concept_count;      /**< Number of recent concepts */
} gl_context_t;

/**
 * @brief Multi-turn discourse state — small ring buffer of recent
 *        conversational turns. The single rolling `gl_context_t` above
 *        is a recency-weighted blend of these; this buffer keeps the
 *        per-turn breakdown so future coherence / topic-shift checks
 *        can compare individual turns rather than a smeared running
 *        vector.
 *
 * `semantic_vector` is gl-owned (malloc'd at push, freed on destroy /
 * eviction). `head` is the index of the *oldest* turn when the buffer
 * has wrapped (count == capacity); otherwise turns occupy [0, count).
 * Capacity is bounded by GL_DISCOURSE_MAX_TURNS at compile time and
 * runtime-clampable via grounded_language_set_discourse_capacity.
 */
#define GL_DISCOURSE_MAX_TURNS 8

typedef struct {
    float*   semantic_vector;         /**< Owned [semantic_dim], NULL = empty slot */
    uint32_t word_count;
    uint64_t timestamp_ms;
    bool     is_user;
} gl_discourse_turn_t;

typedef struct {
    gl_discourse_turn_t turns[GL_DISCOURSE_MAX_TURNS];
    uint8_t  head;                    /**< Oldest-turn index when full; 0 otherwise */
    uint8_t  count;                   /**< Currently used slots */
    uint8_t  capacity;                /**< 1..GL_DISCOURSE_MAX_TURNS */
} gl_discourse_state_t;

/**
 * D1 FIX: Cognitive bridge enable flag bit positions (2026-10-21).
 * Used with grounded_language.bridge_enable_flags bitfield.
 */
#define GL_BRIDGE_ACC                (1u << 0)
#define GL_BRIDGE_AMYGDALA           (1u << 1)
#define GL_BRIDGE_BASAL_GANGLIA      (1u << 2)
#define GL_BRIDGE_CEREBELLUM         (1u << 3)
#define GL_BRIDGE_CONSCIOUSNESS      (1u << 4)
#define GL_BRIDGE_EPISODIC_REPLAY    (1u << 5)
#define GL_BRIDGE_HAMILTONIAN        (1u << 6)
#define GL_BRIDGE_HIPPOCAMPUS        (1u << 7)
#define GL_BRIDGE_HYPOTHALAMUS       (1u << 8)
#define GL_BRIDGE_IMAGINATION        (1u << 9)
#define GL_BRIDGE_INSULA             (1u << 10)
#define GL_BRIDGE_INTROSPECTION      (1u << 11)
#define GL_BRIDGE_JEPA               (1u << 12)
#define GL_BRIDGE_MIRROR             (1u << 13)
#define GL_BRIDGE_PARIETAL           (1u << 14)
#define GL_BRIDGE_PREFRONTAL         (1u << 15)
#define GL_BRIDGE_PRIME_RESONANCE    (1u << 16)
#define GL_BRIDGE_TEMPORAL           (1u << 17)
#define GL_BRIDGE_TOM                (1u << 18)
#define GL_BRIDGE_WORLD_MODEL        (1u << 19)
#define GL_BRIDGE_ALL                0xFFFFFFFFu  /**< All bridges enabled */

/**
 * @brief Internal state of the grounded language system.
 *
 * Layout previously lived in nimcp_grounded_language.c. Moved here so the
 * persistence sidecar can iterate the lexicon and templates directly.
 */
struct grounded_language {
    /* Lexicon: word forms -> concept bindings */
    gl_lexicon_entry_t** lexicon;       /**< Hash table of lexicon entries */
    uint32_t             lexicon_size;  /**< Hash table size */
    uint32_t             vocab_count;   /**< Number of words */
    gl_lexicon_entry_t** vocab_list;    /**< Linear list for iteration */

    /* Semantic integration */
    uint32_t             semantic_dim;  /**< Dimension of semantic vectors */
    semantic_memory_system_t* semantic_memory; /**< Brain's concept store (not owned, not serialized) */

    /* Context state */
    gl_context_t         context;       /**< Current conversation context */

    /* Cross-modal connections */
    void*                visual_ctx;
    void*                auditory_ctx;
    void*                speech_ctx;
    void*                column_pool;
    void*                emotional_ctx;

    /* Learning parameters */
    float                hebbian_lr;    /**< Hebbian learning rate */
    float                decay_rate;    /**< Association decay rate */

    /* Statistics */
    gl_stats_t           stats;

    /* RNG state for sampling */
    uint64_t             rng_state;

    /* SNN language bridge (optional) */
    struct snn_language_bridge* snn_bridge;

    /* Memory-system attachments (optional, all opaque pointers).
     * Wired at brain init via grounded_language_connect_*; consulted on
     * every successful grounding event so the trained vocabulary
     * participates in working/episodic/long-term memory rather than
     * sitting in an isolated lexicon. None are owned or serialized. */
    void*                working_memory;     /**< working_memory_t* */
    void*                episodic_replay;    /**< nimcp_episodic_replay_t* */
    void*                hippocampus;        /**< hippocampus_adapter_t* */

    /* Region-adapter attachments — when connected, every newly-created
     * lexicon entry is mirrored into the broca production lexicon and
     * the wernicke comprehension lexicon. This keeps the three lexicons
     * in sync with grounded_language as the single source of truth. */
    void*                broca_adapter;      /**< broca_adapter_t* */
    void*                wernicke_adapter;   /**< wernicke_adapter_t* */

    /* NLP frontend attachments — embedding layer + BPE tokenizer used
     * to enrich semantic_vector and recover from totally-OOV words.
     * All optional; comprehend skips the corresponding stages when NULL. */
    void*                embeddings;         /**< embedding_layer_t* */
    uint32_t             emb_dim;            /**< embedding output dim */
    void*                tokenizer;          /**< tokenizer_t* (BPE) */
    gl_word_to_id_fn     word_to_id_fn;      /**< word→id mapper */
    void*                word_to_id_ctx;     /**< opaque ctx for fn */

    /* Per-network bridges — comprehend's semantic_vector gets broadcast
     * to each attached network's forward pass; the response magnitudes
     * feed back as confidence modulation (parallel to cortex modulation). */
    void*                lnn_layer;          /**< lnn_layer_t* */
    void*                cortex_cnn_proc;    /**< cortex_cnn_processor_t* */
    void*                fno_proc;           /**< fno_audio_processor_t* */
    gl_ann_predict_fn    ann_predict_fn;     /**< caller-owned ANN predictor */
    void*                ann_ctx;            /**< opaque ctx for ann_predict_fn */
    /* Last broadcast results — read by comprehend modulation hook. */
    float                last_lnn_mag;
    float                last_cnn_mag;
    float                last_fno_mag;
    float                last_ann_mag;

    /* Cognitive + region subscriber bus — flat array, ctx is the dedup
     * key. Cap covers ~10 cognitive modules + 5 anatomical regions +
     * the hemispheric language bridge, with headroom for future waves.
     * Sorted by descending priority on insert so fire walks once.
     * type_mask gates per-event-type delivery (#3 priority + #4 mask). */
    #define GL_MAX_SUBSCRIBERS 24
    gl_event_callback_t  subscribers[GL_MAX_SUBSCRIBERS];
    void*                subscriber_ctxs[GL_MAX_SUBSCRIBERS];
    uint32_t             subscriber_masks[GL_MAX_SUBSCRIBERS];
    int8_t               subscriber_priorities[GL_MAX_SUBSCRIBERS];
    uint32_t             subscriber_count;

    /* Re-entry guard (#11). Set true while gl_fire_event is iterating;
     * a subscriber that calls back into a function that would re-fire
     * an event hits this and the inner fire becomes a no-op (with a
     * dropped-events counter increment). Prevents infinite recursion
     * if a wrapper accidentally drives ground/comprehend/produce. */
    bool                 in_fire_event;
    uint64_t             events_dropped_reentry;

    /* Forgetting-curve telemetry (#15). Bumped by sleep_consolidate
     * each time an entry is decayed below retention. The ring rotates
     * once per call to gl_forgetting_telemetry_rotate (called by the
     * sleep_consolidate path on hour rollover). */
    uint32_t             decayed_ring[24];
    uint8_t              decayed_ring_head;          /* index of current bucket */
    uint64_t             decayed_all_time;

    /* Dialect/accent context (#14). Empty string = dialect-agnostic
     * mode. Setter truncates to GL_MAX_DIALECT_LEN-1 + NUL. Used by
     * fuzzy lookup as a tie-break preference and surfaced via probe
     * metrics for observability. */
    char                 context_dialect[GL_MAX_DIALECT_LEN];

    /* Compositional phrases (#9). Fixed-capacity table accumulated
     * lazily by learn_from_text. semantic_vec is gl-owned; cleared on
     * destroy. phrases_dropped counts the times capacity overflow
     * forced a new phrase to be skipped (without LRU eviction those
     * would just be lost; with eviction they're swapped in for the
     * least-frequent existing entry). */
    gl_phrase_t*         phrases;
    uint32_t             phrase_count;
    uint64_t             phrases_evicted;

    /* D7 — set true while a persistence rehydrate is in progress. The
     * NEW_WORD event publisher and the SNN-bridge spike synthesizer both
     * gate on this. ~30K rehydrated entries per resume otherwise blast
     * NEW_WORD to every subscriber + drive synthesized spike traces over
     * saved STDP weights. Init/runtime always sees is_loading=false. */
    bool                 is_loading;

    /* D1 — process-monotonic virtual time fed to the SNN language bridge
     * by mirror_binding_to_bridge so each synthesized spike pair lives in
     * its own STDP window. Float ms; advanced by 50ms per binding event.
     * Survives across save/load by design — the bridge keeps running
     * windowed STDP after resume. */
    float                snn_virtual_time_ms;

    /* Tier-2 #3 negation polarity. When enabled (default true), comprehend
     * detects negation cues in the tokenized text and inverts the sign of
     * activation_levels[] for the next non-function content word within a
     * GL_NEGATION_WINDOW lookahead. Counters (negation_events,
     * sense_resolutions) live on gl_stats_t — getter copies them out
     * with the rest of the public stats. */
    bool                 enable_negation_inversion;

    /* TA-5 — reconsolidation on contradiction. When enabled (default
     * false; opt-in trainer knob), comprehend's negation pass also
     * walks negate_word[] and decays the strength of every binding on
     * the negated content word's lexicon entry by reconsolidation_decay.
     * Models the brain's reconsolidation phase, where retrieved memories
     * become labile and contradicting evidence weakens them. Repeated
     * negations across turns erode bindings that no longer fit the
     * evidence; re-asserted bindings recover via normal reinforcement. */
    bool                 enable_reconsolidation;
    float                reconsolidation_decay;

    /* TB-6 — sentence-boundary segmentation. When enabled (default
     * false; opt-in trainer flag), comprehend splits the input on
     * `.`/`!`/`?` and processes each sentence independently — each
     * sentence gets its own discourse-turn push, its own LGSS/immune
     * pass via recursive comprehend, and bigram learning never bridges
     * a sentence boundary. Disabled by default to preserve bit-for-bit
     * legacy behaviour: a single comprehend call still pushes exactly
     * one discourse turn regardless of how many `.`s the input contains.
     * Per-instance runtime knob; not persisted across save/load. */
    bool                 enable_sentence_segmentation;

    /* Tier-2 #6 word-sense disambiguation. When enabled (default false —
     * preserves legacy behaviour), comprehend's per-binding activation is
     * weighted by cosine(intent, binding_concept_features); the most-
     * relevant sense gets the highest weight and off-sense bindings damp.
     * Off-sense weight floor lives at GL_SENSE_OFF_DAMP. */
    bool                 enable_sense_disambiguation;

    /* Tier-2 #7 multi-turn discourse state. Backbone for future coherence
     * / topic-shift checks. The legacy gl->context.context_vector remains
     * the single rolling vector — when discourse turns are pushed it gets
     * recomputed as a recency-weighted blend across the buffer. */
    gl_discourse_state_t discourse;

    /* Engram integration (read-only mode). Borrowed pointer to the
     * brain-level engram system, attached via
     * grounded_language_set_engram_system(). When `engram_enabled` is
     * true and the pointer is non-NULL, comprehend lays down a memory
     * trace at the end of each pass (encode) and consults the engram
     * store mid-pass to blend recalled activations into the result
     * (recall). The pointer is NOT owned, NOT serialized — re-attach
     * after every brain init / load. Default: pointer NULL, flag false. */
    void* engram_system;
    bool  engram_enabled;

    /* TA-2: LGSS gate. Borrowed pointer to the brain-level
     * lgss_context_t, attached via grounded_language_set_lgss(). When
     * the pointer is non-NULL, comprehend evaluates an input-validation
     * action context against the safety KB before lexicon lookup — a
     * SAFETY_ACTION_DENY result early-aborts comprehend, increments the
     * lgss_inputs_blocked stat, and emits an LGSS_INPUT_REJECTED audit
     * event. NOT owned, NOT serialized; re-attach after brain init /
     * load. NULL = no-op (legacy callers see identical behavior). The
     * type is intentionally void* to keep the heavy LGSS headers out of
     * this internal layout — comprehend casts to lgss_context_t* via a
     * forward declaration in the .c file. */
    void* lgss;

    /* IM-3 — Tier-3 immune content inspection. Borrowed pointer to the
     * brain-level brain_immune_system_t, attached via
     * grounded_language_set_immune_system(). When `immune_enabled` is
     * true and the pointer is non-NULL, comprehend runs cheap read-only
     * heuristics over the input + activations to detect adversarial
     * patterns; the resulting inflammation level damps
     * comprehension_confidence and (above thresholds) registers an
     * antigen + skips the engram encode hook. The pointer is NOT owned,
     * NOT serialized — re-attach after every brain init / load. Default:
     * pointer NULL, flag false.
     *
     * `immune_inflammation_level` holds the most-recent inspection's
     * computed level [0..1] for diagnostics; the field is overwritten
     * on every inspected comprehend call. There is no per-tick decay
     * here — the brain_immune_system tracks its own inflammation
     * state. This field is purely a "what did the last inspection
     * see?" probe, not the immune system's authoritative level. */
    void* immune_system;
    bool  immune_enabled;
    float immune_inflammation_level;
    /* Welford-style running mean + sum-of-squared-diffs for
     * total_activation_per_word seen by the inspector. Used by the
     * statistical-outlier heuristic (>5σ trips inflammation). Updated
     * only on inspected comprehend calls, so a disabled inspector
     * does NOT contaminate the baseline with un-checked traffic. */
    double immune_act_mean;
    double immune_act_m2;
    uint64_t immune_act_samples;

    /* TB-10 — topic-shift detection in discourse. When enabled (default
     * OFF — preserves legacy behaviour), comprehend computes the cosine
     * similarity between the most-recent discourse turn and the mean of
     * the prior up-to-K turns, flags a topic boundary when the score
     * falls below `topic_shift_threshold`, and stashes the score +
     * boundary flag for downstream cognition (attention refresh,
     * anaphora reset, memory consolidation). At least
     * `topic_shift_min_turns` prior turns are required before a shift
     * can fire — until then last_was_topic_shift stays false and
     * last_topic_shift_score is held at 1.0 ("no shift"). All three
     * tunables are runtime-mutable via grounded_language_set_topic_shift_*
     * setters; threshold clamps to [0, 1] and min_turns clamps to
     * [2, GL_DISCOURSE_MAX_TURNS]. */
    bool     enable_topic_shift_detection;
    float    topic_shift_threshold;
    uint32_t topic_shift_min_turns;
    float    last_topic_shift_score;
    bool     last_was_topic_shift;

    /* TB-9 — speech-act intent classification. When enabled (default
     * false), comprehend runs a cheap rule-based classifier over the
     * tokenized words + raw text after the negation pass and before
     * engram encoding, populating result->speech_act with one of the
     * gl_speech_act_t labels. When disabled, result->speech_act stays
     * GL_SPEECH_ACT_UNKNOWN and the per-class stats counters never
     * advance. Per-instance runtime knob; not persisted across
     * save/load. */
    bool                 enable_speech_act_classification;

    /* TC-12 — per-gl side-state for anaphora + bigram spectrum, replacing
     * the legacy global side-maps (g_anaphora_map / g_spectrum_map) and
     * their global mutex. Each map slot was capped at 4 brains in-process
     * and serialized every reader through one lock; this layout puts the
     * state directly on the gl with a per-instance mutex so concurrent
     * comprehend calls across brains never contend.
     *
     * `anaphora_state` is owned (allocated lazily on first
     * grounded_language_set_anaphora_enabled, freed in destroy).
     * `bigram_spectrum` is borrowed (lifetime owned by the trainer per
     * the public attach API contract). `tc12_lock` covers both fields. */
    void*                anaphora_state;     /* gl_anaphora_state_t* (opaque to keep this header minimal) */
    void*                bigram_spectrum;    /* bigram_spectrum_t* (borrowed) */
    pthread_mutex_t      tc12_lock;
    bool                 tc12_lock_inited;

    /* Audit walkthrough-2 fix — mutate_lock guards three mutable
     * structures that comprehend touches:
     *   - lexicon[] hash table + vocab_list + vocab_count
     *     (writer: lexicon_find_or_create)
     *   - discourse ring (head/count/turns array)
     *     (writers: grounded_language_push_turn,
     *      discourse_rebuild_context_blend)
     *   - phrases[] table + phrase_count
     *     (writer: gl_internal_phrase_find_or_create)
     *
     * Without this lock, concurrent comprehend calls (from RO socket
     * pool + main pool) race on the hash-chain writes — same shape
     * of bug Broca + Wernicke had before commits b1417e2a9 / a6f965029.
     * Separate from tc12_lock to keep ownership semantics clean:
     * tc12_lock covers anaphora + spectrum side-state; mutate_lock
     * covers core GL state. */
    pthread_mutex_t      mutate_lock;
    bool                 mutate_lock_inited;

    /* NLP-1 — subword OOV/morphological fallback. When the tokenizer is
     * attached AND the flag is on, comprehend tries to bootstrap fresh
     * lexicon entries (no bindings yet) by segmenting the word and
     * averaging bindings from any subword pieces that already have
     * lexicon entries. Borrowed pointer (lifetime owned by the brain
     * tokenizer module); NULL == no fallback. */
    void*                subword_tokenizer;
    bool                 enable_subword_oov_fallback;
    /* Stats counters for the fallback path. */
    uint64_t             subword_oov_attempts;   /* unknown words encountered */
    uint64_t             subword_oov_resolved;   /* unknown words bootstrapped from subwords */

    /* NLP-2 — coreference beyond pronouns.
     *
     * Tracks recent definite-NP head mentions (content noun preceded by
     * "the"/"this"/"that"/"these"/"those") in a small ring. On a repeat
     * mention of the same head within the past K turns, count a coref
     * resolution and fire GL_EVENT_COREF_RESOLVED. Same-surface only —
     * semantic-vec fuzzy matching ("dog" ↔ "puppy") is future work.
     *
     * `coref_state` is owned (gl_coref_state_t*, opaque to keep this
     * header minimal). Lazily allocated on first set_coref_enabled.
     * Protected by tc12_lock alongside anaphora_state. */
    void*                coref_state;
    bool                 enable_coref_resolution;
    uint64_t             coref_attempts;   /* definite-NP heads observed */
    uint64_t             coref_resolved;   /* repeat mentions matched */

    /* Tier 1 follow-up (2026-05-26) — gl-side autoregressive produce.
     * When true, grounded_language_produce maintains a running "emitted
     * context" vector (sum of already-emitted words' context_vectors) and
     * adds a continuation-coherence bonus cos(emitted_ctx, candidate.ctx)
     * to each candidate's score, so production is left-to-right conditioned
     * rather than a fixed-intent rerank. Additive; position 0 and the OFF
     * state are byte-identical to the pre-AR path. Default false. */
    bool                 autoregressive_produce;

    /* Tier 2 (2026-05-26) — produce-side pronominalization (recency
     * anaphora). When true, a post-cascade pass replaces a re-mentioned
     * non-person noun in OBJECT position (after a verb, within a recency
     * window) with "it"/"them" so replies don't robotically repeat the noun
     * ("the cat chased the cat" -> "the cat chased it"). Person nouns are
     * guarded out (gender unknown -> can't safely pronominalize). Default
     * false. */
    bool                 produce_pronominalize;

    /* Tier 2 (2026-05-26) — discourse-seeded autoregressive produce.
     * When true AND autoregressive_produce is also on AND stage >= 2,
     * grounded_language_produce initializes its emitted-context vector with
     * a unit-normalized, decayed copy of the most-recent discourse-turn
     * semantic vector (instead of all-zero). This pulls the very first
     * produced words toward referential continuity with what was just said
     * ("cross-turn coherence"); the seed's influence fades automatically as
     * real emitted words accumulate in the running sum. Reuses the AR
     * continuation bonus (no new scoring term). Default false — when off,
     * or when autoregressive_produce is off, produce is byte-identical to
     * the pre-seed path. */
    bool                 produce_discourse_seed;

    /* FND-1 (2026-05-27) — predicate->argument clause frame. When true AND
     * stage >= 2, grounded_language_produce organizes the grounded top-K into
     * a canonical SVO clause (best VERB as predicate + best two NOUNs as
     * agent/patient, "the SUBJECT VERB the OBJECT") instead of emitting a
     * bigram/POS-reranked bag of content words. Only word ORDER is templated
     * (English SVO — a Broca-level parameter); every slot is filled by the same
     * grounded score the greedy path uses, so WHAT is said stays data-driven.
     * Falls back to the greedy emit when there's no head verb / no noun, or the
     * head verb's grounded score is below the developmental confidence floor
     * (never worse than baseline). Default false. */
    bool                 produce_clause_frame;

    /* Increment-1 (2026-06-02) — SNN-as-generator A/B switch. When true,
     * grounded_language_produce sources its candidate words from the SNN
     * language bridge's per-tick Broca spike cache
     * (snn_language_bridge_decode_spikes_cached) instead of find_words_near_vector,
     * falling back to find_words_near_vector when the SNN yields no signal.
     * Default false → produce is byte-identical to the lexicon path. Runtime-only
     * (applied from JSON at daemon start; not LANC-persisted). */
    bool                 produce_via_snn;

    /* SNN generation dynamics (2026-06-10). When produce_via_snn is on,
     * gl_produce_candidates_via_snn seeds the intent's concepts into Wernicke
     * and steps the SNN snn_gen_n_steps times at snn_gen_inject before decoding
     * Broca. Selectivity sweep (snn_probe_selectivity) showed the expected word
     * lands in the decode top-5 for 94% of bindings at n_steps=16/inject=40
     * (mean rank 1.64, zero misses) vs 27/64 in top-32 at the old 8/100 — the
     * Broca ensembles need ~16 steps for projection drive to dominate ambient
     * noise, and inject=100 over-saturates. Runtime-tunable via
     * grounded_language_set_snn_gen_params. */
    uint32_t             snn_gen_n_steps;   /* default 16 */
    float                snn_gen_inject;    /* default 40.0 */
    /* How many of the intent's top concepts to seed into Wernicke at once.
     * Single-concept selectivity is excellent (probe: 79% top-5), but seeding
     * all 8 floods Broca with 8 concepts' words → salad. Default 2 (focused).
     * Clamped to the 8-slot seed array. */
    uint32_t             snn_gen_max_concepts; /* default 2 */

    /* Metacognitive produce-floor modulation (2026-06-02). When
     * metacog_gates_produce is true, gl_produce_confidence_floor adds
     * metacog_floor_adjust (set per-produce by the cascade from world-model
     * surprise / wellbeing distress / introspection confidence) to the stage
     * floor, so modulatory cognitive state shifts willingness-to-assert. This
     * is the correct integration for the modulatory cognitive modules (emotion,
     * introspection, world-model) which carry no semantic content vector for
     * content_intent. Default OFF / 0.0 → base stage floor unchanged. The
     * adjust is runtime-set (not persisted); the flag is config (runtime-only). */
    bool                 metacog_gates_produce;
    float                metacog_floor_adjust;

    /* T3-1 (2026-05-28) — givenness-driven definiteness. When true AND
     * stage >= 2, gl_apply_givenness_definite runs after T2 pronominalization
     * in cascade_apply_surface_correctors: a NOUN re-mention (recency window)
     * that did NOT get pronominalized (person nouns, unanchored slots) has its
     * preceding indefinite "a"/"an" swapped to "the" so the second mention is
     * marked as given ("a cat sat. a cat purred." -> "a cat sat. the cat
     * purred."). Reuses the same lemma + noun-class helpers as T2-1/T2-3.
     * Default false. */
    bool                 produce_givenness_definite;

    /* T3-2 (2026-05-29) — Cohesive conjunction insertion between adjacent SVO
     * clauses. When true AND stage >= 2, gl_apply_t3_conjunction runs after
     * T3-1 givenness in cascade_apply_surface_correctors: detects an adjacent
     * SVO pair (VERB ... DET-or-NOUN ... VERB) without an existing connective
     * and inserts "and" before the start of the second clause. Skips when the
     * boundary token is already a conjunction (and/but/so/or/then/while/
     * because/although). One insert per pass — idempotent on re-run. */
    bool                 produce_t3_conjunction;

    /* TF-2 (2026-05-28) — Tier-feedback master switch + per-corrector enable
     * mask + per-plasticity-path learning rates. The whole TF stack is gated
     * by `produce_corrector_feedback_enabled` (default OFF): when false,
     * grounded_language_tf_record_diff still bumps observation counters but
     * NO plasticity fires regardless of the other knobs. When true, TF-3/4/5
     * walk the deltas and only apply a path if BOTH (a) the corresponding
     * lr is > 0 AND (b) the delta's source bit is set in
     * tf_enabled_correctors. Two-key safety:
     *
     *   bit 0 = F3 agreement deltas
     *   bit 1 = F4 fluency deltas (a/an, past-tense, pronoun case)
     *   bit 2 = T2 pronominalization deltas
     *   bit 3 = T3-1 givenness deltas
     *   bit 4 = T3-2 conjunction deltas (when shipped)
     *
     * Bridge STDP (TF-5) is additionally gated by tf_lr_bridge_stdp > 0 —
     * separate from the trigram/distrib lrs because SNN weight mutation is
     * the highest-risk path; even with the master flag on, leaving the STDP
     * lr at 0 keeps the bridge inert. Both the master flag and the lrs are
     * persisted in the LANC config block; the JSON default file mirrors
     * them so a daemon restart preserves operator intent. */
    bool                 produce_corrector_feedback_enabled;
    uint16_t             tf_enabled_correctors;
    float                tf_lr_trigram;       /* clamped [0, 0.05] */
    float                tf_lr_distrib;       /* clamped [0, 0.1]  */
    float                tf_lr_bridge_stdp;   /* clamped [0, 0.1]  */

    /* Produce-score rebalance (2026-05-27) — tunable weight on the
     * distributional (context-vector cosine) term in
     * score_word_against_vector, vs the concept-binding cosine term:
     *   raw = w * distributional_sim + (1 - w) * concept_sim
     * Default GL_PRODUCE_DISTRIBUTIONAL_DEFAULT_WEIGHT (0.4) reproduces the
     * historical 0.4/0.6 split bit-for-bit. Raising w toward ~0.7 lets the
     * intent-correct distributional signal (which a concrete word like "cat"
     * matches strongly for a concrete intent) outrank a broad cluster of
     * abstract words whose strong/wide concept bindings otherwise dominate
     * the 0.6 term for ANY intent (diagnosed 2026-05-27: produce collapsed
     * to ~20 abstract nouns regardless of intent — a scoring artifact, not
     * curriculum/comprehend collapse). Clamped to [0,1]. */
    float                produce_distributional_weight;

    /* Produce-score frequency penalty (2026-06-01): IDF-style damping of
     * high-frequency words in produce scoring — raw *= 1/(1 + p*log1p(freq)).
     * Default 0.0 = OFF (no change). Raising p suppresses distributionally-
     * central technical-corpus vocab (gpu/impedance/lidar/...) from
     * monopolizing produce filler positions (the word-salad contamination
     * diagnosed 2026-06-01). Runtime-only: applied from JSON at daemon start,
     * NOT persisted in the LANC block (avoids checkpoint-format churn). */
    float                produce_frequency_penalty;

    /* Slice E (2026-05-19) — developmental-stage vocabulary mask.
     *
     * `vocab_active_mask` is a heap-allocated bool array sized to
     * `vocab_active_mask_capacity`. Index v corresponds to lexicon entry
     * vocab_list[v]; mask[v] == true means the entry is visible for
     * production. NULL pointer (calloc default) means "no mask installed,
     * everything is visible" — preserves the pre-Slice-E byte-identical
     * contract.
     *
     * Installed via grounded_language_set_active_vocab_mask(); resized
     * lazily by vocab_count growth. Capacity grows monotonically; we never
     * shrink so existing index→mask mappings remain stable across vocab
     * growth events.
     *
     * Read by the production-path filter that wraps grounded_language_produce
     * (vocab-mask token replacement). Protected by mutate_lock above —
     * the same lock that guards the lexicon array itself, since mask
     * indexing depends on vocab_list shape. */
    bool*                vocab_active_mask;
    uint32_t             vocab_active_mask_capacity;
    uint32_t             vocab_active_mask_stage;   /* stage that installed it */

    /* Walkthrough-2 (Option-1 rebuild, 2026-05-19) — concept_registry handle.
     * Set via grounded_language_set_concept_registry() during brain init.
     * NULL means "not wired" — concept_registry_* calls are guarded so
     * the legacy bridge-mirror path still compiles. */
    void*                concept_registry;

    /* Toxicity classifier handle (2026-05-21). Wired from brain init via
     * grounded_language_set_toxicity_classifier(). NULL means "not wired"
     * — classify calls are guarded so the produce/respond path stays
     * functional without it. POLICY: mark, never delete — classification
     * only annotates; content is never modified. */
    void*                toxicity_classifier;

    /* Toxicity response engine (Phase 3a, 2026-05-21). Counterclaim
     * generator used by respond() when input is classified toxic. Wired
     * from brain init via grounded_language_set_toxicity_response().
     * NULL means "not wired" — respond falls back to default refusal. */
    void*                toxicity_response;

    /* Current developmental stage for counterclaim selection (0..3).
     * Set externally via grounded_language_set_current_stage(); falls
     * back to gl_internal_get_current_stage() when zero. */
    int                  current_stage;

    /* Symbolic logic engine (Phase 1, 2026-10-21). Borrowed pointer to
     * brain->symbolic_logic, attached via
     * grounded_language_set_symbolic_logic(). When `logic_enabled` is
     * true and the pointer is non-NULL:
     *   - comprehend extracts logical facts (subject-verb-object triples)
     *     and adds them to the KB via symbolic_logic_add_fact()
     *   - produce queries the KB to constrain word selection and
     *     derives new facts via forward chaining
     *   - logical features (confidence, num_facts, inference_depth)
     *     blend into content_intent alongside other cognitive modules
     * The pointer is NOT owned, NOT serialized — re-attach after every
     * brain init / load. Default: pointer NULL, flag false. */
    void*                symbolic_logic;         /* symbolic_logic_t* */
    bool                 logic_enabled;

    /* D1 FIX: Cognitive bridge enable flags (2026-10-21). Bitfield for
     * runtime enable/disable of individual cognitive→language integration
     * modules. Replaces the ineffective `return gl != NULL` pattern with
     * per-module toggles. Default: all enabled (0xFFFFFFFF). Bit positions
     * defined via GL_BRIDGE_* constants below. Runtime-only (not persisted;
     * re-apply from config on daemon restart). */
    uint32_t             bridge_enable_flags;
};

/**
 * @brief Find an existing lexicon entry by form, or create a fresh one.
 *
 * Used by the primary implementation and by the persistence sidecar's
 * load path to rehydrate entries while keeping the hash table in sync
 * with vocab_list. Lower-cased on entry. Returns NULL on alloc failure
 * or vocab cap exhaustion.
 */
gl_lexicon_entry_t* gl_internal_lexicon_find_or_create(
    grounded_language_t* gl,
    const char* word);

/**
 * @brief Find an existing compositional phrase by surface form, or create
 *        a fresh entry (evicting the least-frequent phrase if the table is
 *        at GL_MAX_PHRASES capacity).
 *
 * Used by _gl_track_phrases at learn time and by the persistence sidecar's
 * load path to rehydrate the phrase table. Takes the gl mutate_lock
 * internally — callers must NOT already hold it. New entries start at
 * frequency 0 with semantic_vec = NULL (lazy cache, rebuilt on first
 * lookup from constituent word vectors). Returns NULL on capacity
 * thrash (everything freq=0) or if gl/phrases/form is NULL.
 *
 * @param gl              Owning grounded-language handle.
 * @param form            Whitespace-joined surface form (e.g. "tree green").
 * @param component_words Number of words in the phrase (2 = bigram, 3 = trigram).
 * @return Pointer into gl->phrases[] (valid until next eviction), or NULL.
 */
gl_phrase_t* gl_internal_phrase_find_or_create(
    grounded_language_t* gl,
    const char* form,
    uint8_t component_words);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GROUNDED_LANGUAGE_INTERNAL_H */
