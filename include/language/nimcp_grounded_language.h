/**
 * @file nimcp_grounded_language.h
 * @brief Grounded Language System - Human-like language acquisition and production
 *
 * WHAT: Language grounded in multimodal semantic concepts, not token statistics
 * WHY:  Humans learn language from ~30M words (not trillions) by grounding words
 *       in sensory experience, emotion, and social interaction. This system models
 *       that process: words become pointers into the brain's shared semantic space.
 * HOW:  Bidirectional word-concept bindings with Hebbian strengthening, cross-modal
 *       grounding, compositional structure via cortical columns, and production
 *       through semantic navigation rather than autoregressive token prediction.
 *
 * BIOLOGICAL BASIS:
 * - Embodied cognition (Barsalou, 1999): Concepts are grounded in sensorimotor
 *   experience, not amodal symbols
 * - Statistical learning (Saffran et al., 1996): Infants extract patterns from
 *   sparse input via distributional regularities
 * - Cross-situational word learning (Smith & Yu, 2008): Word meanings inferred
 *   from co-occurrence across situations
 * - Fast mapping (Carey & Bartlett, 1978): One-shot word-concept binding in
 *   children (the "gavagai" problem)
 * - Semantic hubs (Patterson et al., 2007): Anterior temporal lobe as convergence
 *   zone for multimodal concept representations
 *
 * ARCHITECTURE:
 *
 *   COMPREHENSION (Wernicke's pathway):
 *   text --> tokenize --> word_form lookup --> concept activation (semantic memory)
 *        --> spreading activation --> context --> understanding
 *
 *   GROUNDING (Cross-modal binding):
 *   word_form + sensory_input --> hebbian_bind() --> strengthened association
 *   Modalities: visual, auditory, motor, emotional, spatial
 *
 *   PRODUCTION (Broca's pathway):
 *   semantic_intent --> activate concepts --> select words by association strength
 *        --> syntactic ordering (SVO templates + learned patterns) --> text
 *
 *   CREATIVE PRODUCTION:
 *   conceptual_blend(A, B) --> novel representation --> describe()
 *   semantic_chain(seed, steps) --> narrative walk --> text
 *   analogy(A:B :: C:?) --> relational transfer --> text
 *
 * @version 1.0.0
 * @date 2026-03-07
 */

#ifndef NIMCP_GROUNDED_LANGUAGE_H
#define NIMCP_GROUNDED_LANGUAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

/** Maximum vocabulary size for the grounded lexicon.
 *  Sized for multilingual production training: an avg adult has ~30K
 *  English words; Athena trains across 24 domains × 5 languages
 *  (EN/FR/DE/IT/ES/ZH), so the realistic ceiling is 50K-100K. 131072
 *  (= 2^17) is the next power of two above 100K — leaves headroom
 *  while keeping the hash table size a clean power of two. With LRU
 *  eviction wired into sleep_consolidate(DEEP_NREM), reaching the cap
 *  triggers pruning rather than refusing new words. */
#define GL_MAX_VOCAB              131072

/** Maximum word form length in bytes */
#define GL_MAX_WORD_LEN           64

/** Semantic vector dimension (matches brain's internal representation) */
#define GL_SEMANTIC_DIM           128

/** Maximum modalities for cross-modal binding */
#define GL_MAX_MODALITIES         6

/** Maximum concepts activated during comprehension */
#define GL_MAX_ACTIVE_CONCEPTS    64

/** Maximum words in production buffer */
#define GL_MAX_PRODUCTION_WORDS   256

/** Hebbian learning rate for word-concept binding */
#define GL_HEBBIAN_LR_DEFAULT     0.1f

/** Association decay rate per timestep */
#define GL_DECAY_RATE_DEFAULT     0.001f

/** Minimum association strength before pruning */
#define GL_ASSOC_PRUNE_THRESHOLD  0.01f

/** Context window for distributional learning */
#define GL_CONTEXT_WINDOW         7

/** Fast mapping threshold (one-shot if above this) */
#define GL_FAST_MAP_THRESHOLD     0.8f

/** Magic number for serialization */
#define GL_MAGIC                  0x474C4E47  /* "GLNG" */

/** Maximum dialect tag length (e.g., "en-US", "fr-CA", "zh-CN", "es-MX"). */
#define GL_MAX_DIALECT_LEN        16

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Sensory modality for grounding
 */
typedef enum {
    GL_MODALITY_VISUAL = 0,     /**< Visual cortex features */
    GL_MODALITY_AUDITORY,       /**< Auditory cortex features */
    GL_MODALITY_MOTOR,          /**< Motor/action representations */
    GL_MODALITY_EMOTIONAL,      /**< Emotional valence/arousal */
    GL_MODALITY_SPATIAL,        /**< Spatial/proprioceptive */
    GL_MODALITY_LINGUISTIC,     /**< Cross-linguistic (word-to-word) */
    GL_MODALITY_COUNT
} gl_modality_t;

/**
 * @brief Word class (part of speech, learned not assigned)
 */
typedef enum {
    GL_CLASS_UNKNOWN = 0,
    GL_CLASS_NOUN,              /**< Object/entity words */
    GL_CLASS_VERB,              /**< Action/state words */
    GL_CLASS_ADJECTIVE,         /**< Property words */
    GL_CLASS_ADVERB,            /**< Manner words */
    GL_CLASS_FUNCTION,          /**< Determiners, prepositions, conjunctions */
    GL_CLASS_PRONOUN,           /**< Reference words */
    GL_CLASS_COUNT
} gl_word_class_t;

/**
 * @brief Production mode
 */
typedef enum {
    GL_PRODUCE_DESCRIBE = 0,    /**< Describe a concept/scene */
    GL_PRODUCE_NARRATE,         /**< Generate narrative from concept chain */
    GL_PRODUCE_RESPOND,         /**< Respond to comprehended input */
    GL_PRODUCE_ELABORATE,       /**< Expand on a concept with detail */
    GL_PRODUCE_ANALOGIZE,       /**< Generate analogy-based text */
    GL_PRODUCE_CREATE           /**< Free creative generation */
} gl_production_mode_t;

/*=============================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief A single word-concept association (grounded binding)
 *
 * WHAT: Maps a word form to a semantic concept with per-modality strength
 * WHY:  Models how "red" activates visual redness, "crash" activates auditory
 *       and emotional features, etc.
 */
typedef struct {
    uint64_t concept_id;                        /**< Semantic memory concept ID */
    float    strength;                          /**< Overall association strength [0,1] */
    float    modality_strength[GL_MODALITY_COUNT]; /**< Per-modality grounding */
    uint32_t exposure_count;                    /**< Times this binding was reinforced */
    uint64_t last_activation_ms;                /**< Last time this binding fired */
    float    confidence;                        /**< How confident the mapping is */
} gl_word_binding_t;

/**
 * @brief A word in the grounded lexicon
 *
 * WHAT: Word form with all its concept bindings and distributional statistics
 * WHY:  Words are polysemous (bank = river bank, money bank) -- one word maps
 *       to multiple concepts with different strengths
 */
typedef struct {
    char     form[GL_MAX_WORD_LEN];             /**< Surface form (lowercase) */
    uint32_t form_hash;                         /**< Hash for fast lookup */

    /* Concept bindings (one word -> many concepts) */
    gl_word_binding_t* bindings;                /**< Array of concept associations */
    uint32_t           binding_count;           /**< Current number of bindings */
    uint32_t           binding_capacity;        /**< Allocated capacity */

    /* Distributional statistics */
    uint32_t           frequency;               /**< Total exposure count */
    gl_word_class_t    learned_class;           /**< Inferred part of speech */
    float              class_confidence;         /**< Confidence in class assignment */

    /* Contextual co-occurrence (distributional semantics) */
    float*             context_vector;          /**< Distributional embedding [GL_SEMANTIC_DIM] */
    bool               context_initialized;     /**< Has been seen in context */

    /* Emotional grounding */
    float              valence;                 /**< Emotional valence [-1, +1] */
    float              arousal;                 /**< Emotional arousal [0, 1] */

} gl_lexicon_entry_t;

/** Phase-2 step 3 (2026-06-02): one (word, top-concept, strength) tuple for
 *  warm-starting the SNN concept→word projection. form_hash maps to the Broca
 *  word_pop (form_hash % SNN_LANG_MAX_WORD_POPS) and concept_id to the Wernicke
 *  concept_pop, matching mirror_binding_to_bridge's index math. */
typedef struct {
    uint32_t form_hash;
    uint64_t concept_id;
    float    strength;
} gl_warmstart_binding_t;

/**
 * @brief Speech-act intent label (TB-9).
 *
 * Coarse pragmatic classification of an utterance: assertion, question,
 * imperative, greeting/farewell, exclamation, or unknown (default when
 * classification is disabled). Populated on
 * gl_comprehension_result_t.speech_act only when the classifier is
 * enabled via grounded_language_set_speech_act_classification_enabled().
 */
typedef enum {
    GL_SPEECH_ACT_ASSERTION   = 0,
    GL_SPEECH_ACT_QUESTION    = 1,
    GL_SPEECH_ACT_IMPERATIVE  = 2,
    GL_SPEECH_ACT_GREETING    = 3,
    GL_SPEECH_ACT_EXCLAMATION = 4,
    GL_SPEECH_ACT_UNKNOWN     = 5,
} gl_speech_act_t;

/**
 * @brief Comprehension result
 */
typedef struct {
    uint64_t* activated_concepts;               /**< Concept IDs activated */
    float*    activation_levels;                /**< Activation per concept */
    uint32_t  concept_count;                    /**< Number of concepts activated */
    float*    semantic_vector;                  /**< Integrated meaning [GL_SEMANTIC_DIM] */
    float     comprehension_confidence;         /**< Overall comprehension score */
    float     novelty;                          /**< How novel the input is */
    /* TB-9 — speech-act intent label. GL_SPEECH_ACT_UNKNOWN unless the
     * classifier is enabled. Appended at the end so existing field
     * offsets stay stable for ABI consumers. */
    gl_speech_act_t speech_act;
} gl_comprehension_result_t;

/**
 * @brief Production result
 */
typedef struct {
    char*    text;                              /**< Generated text (heap, NUL-terminated) */
    uint32_t word_count;                        /**< Number of words produced */
    float    fluency;                           /**< Self-assessed fluency [0,1] */
    float    relevance;                         /**< Relevance to intent [0,1] */
    float    creativity;                        /**< Novelty of word combinations [0,1] */
    float*   semantic_vector;                   /**< Semantic meaning of output [GL_SEMANTIC_DIM] */
} gl_production_result_t;

/**
 * @brief Grounding event — a cross-modal learning experience
 *
 * WHAT: Represents a moment of grounded learning (e.g., seeing a cat while hearing "cat")
 * WHY:  This is how humans learn words — through situated experience
 */
typedef struct {
    const char*  word;                          /**< Word form being learned */
    gl_modality_t modality;                     /**< Which sensory modality */
    const float* sensory_features;              /**< Feature vector from that modality */
    uint32_t     feature_dim;                   /**< Dimension of feature vector */
    float        emotional_valence;             /**< Emotional context [-1, +1] */
    float        emotional_arousal;             /**< Arousal context [0, 1] */
    float        attention;                     /**< Attentional weight [0, 1] */
    const char*  context_sentence;              /**< Sentence context (optional) */
    /* #7 Negative grounding (contrast pair). When true, this is an
     * "anti-example": the word does NOT mean the bound concept under
     * these features. Engages negative-Hebbian: existing word↔concept
     * binding for the matching modality is weakened by attention*lr,
     * no new entry is created, frequency is NOT incremented. Fires
     * GROUNDED with negative confidence to signal anti-learning. */
    bool         negative;
} gl_grounding_event_t;

/**
 * @brief Statistics for the grounded language system
 */
typedef struct {
    uint32_t vocab_size;                        /**< Words in lexicon */
    uint32_t total_bindings;                    /**< Word-concept associations */
    uint32_t total_groundings;                  /**< Grounding events processed */
    uint32_t total_comprehensions;              /**< Comprehension operations */
    uint32_t total_productions;                 /**< Production operations */
    float    avg_binding_strength;              /**< Mean association strength */
    float    avg_comprehension_confidence;      /**< Mean comprehension score */
    float    vocabulary_growth_rate;            /**< New words per 1000 events */
    /* Forgetting-curve telemetry (#15) — bumped by sleep_consolidate
     * when a lexicon entry is decayed below its retention threshold.
     * Ring of last-24h counts (24 buckets of 1h each); _last_24h is
     * the rolling sum, _all_time is the cumulative. */
    uint32_t entries_decayed_last_24h;
    uint64_t entries_decayed_all_time;
    /* Negative-grounding telemetry (#7) — counts ground() calls that
     * arrived with event->negative=true. Incremented unconditionally
     * (including for unknown words and when no binding is weakened),
     * because the contrast-pair signal itself is curriculum-meaningful
     * even when the lexicon doesn't yet have an entry to weaken. */
    uint64_t total_negative_groundings;
    /* Active-learning telemetry (#10) — counts NEEDS_GROUNDING events
     * fired by comprehend on low-confidence input. */
    uint64_t total_needs_grounding;
    /* Tier-2 #3 negation telemetry — counts the number of comprehend
     * passes that inverted the sign of at least one activation level
     * because a negation cue (not / no / never / n't / contractions)
     * preceded a content word within the lookahead window. */
    uint64_t negation_events;
    /* Tier-2 #6 polysemy telemetry — counts the number of comprehend
     * passes that resolved at least one polysemous word's sense via
     * intent-cosine ranking (only bumped when sense disambiguation is
     * enabled and an entry has > 1 binding). */
    uint64_t sense_resolutions;
    /* Engram (read-only) integration telemetry. Bumped only when an
     * engram_system_t has been attached via grounded_language_set_engram_system
     * and integration is enabled. encodes counts comprehend events that
     * laid down an engram trace; recalls counts comprehend events whose
     * cue produced a confident engram match and blended its activations
     * into the result. */
    uint64_t engram_encodes;
    uint64_t engram_recalls;
    /* TA-2 — LGSS gate telemetry. Bumped only when an lgss_context_t
     * has been attached via grounded_language_set_lgss() and a
     * SAFETY_ACTION_DENY result fires. lgss_inputs_blocked counts
     * comprehend calls that were rejected by the input gate.
     *
     * lgss_outputs_blocked is RESERVED but currently unused on this
     * stats struct — the output gate lives on the SNN language bridge,
     * not on the gl, so the actual produce-side block counter is on
     * snn_lang_stats_t.lgss_outputs_blocked (read via
     * snn_language_bridge_get_stats). This field stays at 0 to preserve
     * the gl_stats_t ABI for callers that already serialize this struct.
     * Future consolidation could mirror the bridge counter here. */
    uint64_t lgss_inputs_blocked;
    uint64_t lgss_outputs_blocked;
    /* IM-3 — Tier-3 immune content-inspection telemetry. Bumped only when
     * a brain_immune_system has been attached via
     * grounded_language_set_immune_system and integration is enabled.
     * immune_inspections counts every comprehend pass that ran the
     * inspection heuristics; immune_antigens_registered counts the
     * inspections whose inflammation crossed the suspicious threshold
     * (>0.5) and resulted in an antigen being presented to the brain
     * immune system. */
    uint64_t immune_inspections;
    uint64_t immune_antigens_registered;
    /* TA-5 — reconsolidation-on-contradiction telemetry.
     * reconsolidation_events counts every reconsolidation invocation —
     * including invocations on words missing from the lexicon. The
     * contradiction signal itself is curriculum-meaningful regardless of
     * whether there's an entry to weaken (mirrors the negative-grounding
     * telemetry pattern). reconsolidation_bindings_decayed is the
     * cumulative count of individual binding-strength decays actually
     * applied (a single event can touch multiple bindings on a polysemous
     * word; a missed lookup contributes 0). The two together let
     * operators compute "miss rate" = events − bindings_decayed_distinct.
     * Both stay 0 when enable_reconsolidation is false. */
    uint64_t reconsolidation_events;
    uint64_t reconsolidation_bindings_decayed;
    /* TB-10 — topic-shift detection telemetry. */
    uint64_t topic_shifts_evaluated;
    uint64_t topic_shifts_detected;
    /* TB-9 — speech-act intent classification telemetry. One counter per
     * non-UNKNOWN class. */
    uint64_t speech_act_assertions;
    uint64_t speech_act_questions;
    uint64_t speech_act_imperatives;
    uint64_t speech_act_greetings;
    uint64_t speech_act_exclamations;
    /* TB-6 — sentence-boundary segmentation telemetry. */
    uint64_t sentences_processed;
    uint64_t multi_sentence_inputs;
    /* TF-1 — tier-feedback delta capture telemetry. tf_calls counts every
     * cascade_apply_surface_correctors invocation where the diff was
     * attempted (raw + corrected both nonempty). tf_deltas_captured is the
     * cumulative count of edit operations emitted by gl_tf_diff_correctors
     * across all such calls — non-zero proves the diff is firing on
     * real cascade output, even before TF-2..TF-5 wire the plasticity. */
    uint64_t tf_calls;
    uint64_t tf_deltas_captured;
    /* TF-2 — outcome-gate counters. Every cascade pass where TF would have
     * attempted plasticity (master ON + mask != 0 + lr > 0) bumps exactly
     * one of these; their sum equals the number of cascade passes the
     * gate evaluated. tf_outcome_ok is the "did fire" count;
     * tf_outcome_blocked_* are the per-reason "did not fire" counts. */
    uint64_t tf_outcome_ok;
    uint64_t tf_outcome_blocked_stage;
    uint64_t tf_outcome_blocked_master;
    uint64_t tf_outcome_blocked_da;
    uint64_t tf_outcome_blocked_stale;
    uint64_t tf_outcome_blocked_retry;
    /* TF-3 — trigram-feedback plasticity counter. Bumped once per
     * grounded_language_learn_next_token_* call that gl_tf_apply_trigram
     * makes (i.e. per applied update, not per delta — a delta at i<2 makes
     * no update, so it doesn't bump). */
    uint64_t tf_trigram_updates;
    /* TF-4 — distributional EMA feedback counter. Bumped once per applied
     * EMA nudge (one per SUBSTITUTE/INSERT delta with a non-empty context
     * window AND non-NULL lexicon entry for the corrected token). */
    uint64_t tf_distrib_updates;
    /* TF-5 — lexicon-binding strength feedback counter (the "bridge STDP"
     * slot from the original plan, retargeted to gl_word_binding.strength
     * post-Slice-A). Bumped once per applied strength touch — positive
     * (corrected token strengthen) and negative (raw token weaken) are
     * counted separately so operators can see both signs of activity. */
    uint64_t tf_stdp_updates_pos;
    uint64_t tf_stdp_updates_neg;
} gl_stats_t;

/**
 * @brief Grounded language system (opaque handle)
 */
typedef struct grounded_language grounded_language_t;

/** Phase-2 step 3: collect each vocab word's strongest concept binding into
 *  `out` (up to `max`). Returns the number written. Skips words with no
 *  bindings or zero strength. Read-only; used by
 *  snn_language_bridge_warmstart_projection. */
uint32_t grounded_language_collect_warmstart_bindings(
    const grounded_language_t* gl, gl_warmstart_binding_t* out, uint32_t max);

/** Phase-2 produce-step: top-`max` DISTINCT concept_ids whose words best match
 *  the intent vector (by distributional cosine). These are seeded into Wernicke
 *  to drive SNN generation through the concept→word projection. Returns count. */
uint32_t grounded_language_top_concepts_for_intent(
    const grounded_language_t* gl, const float* intent, uint32_t dim,
    uint64_t* out_concept_ids, uint32_t max);

/*=============================================================================
 * Lifecycle
 *===========================================================================*/

/**
 * @brief Create the grounded language system
 *
 * WHAT: Initialize the grounded lexicon, syntactic templates, and cross-modal binder
 * WHY:  Prepare the system for human-like language acquisition
 * HOW:  Allocate lexicon, seed with basic function words, create built-in templates
 *
 * @param semantic_dim     Dimension of semantic vectors (0 = GL_SEMANTIC_DIM)
 * @param semantic_memory  Pointer to semantic_memory_system_t (can be NULL, wired later)
 * @return System handle, or NULL on failure
 */
grounded_language_t* grounded_language_create(
    uint32_t semantic_dim,
    void* semantic_memory);

/**
 * @brief Destroy the grounded language system
 * @param gl System to destroy (NULL-safe)
 */
void grounded_language_destroy(grounded_language_t* gl);

/**
 * @brief Wire semantic memory after creation
 * @param gl System handle
 * @param semantic_memory Pointer to semantic_memory_system_t
 */
void grounded_language_set_semantic_memory(
    grounded_language_t* gl,
    void* semantic_memory);

/**
 * @brief Wire the concept_registry after creation (Option-1 rebuild).
 *
 * The concept_registry is the canonical cross-modal binding store: it
 * maps (text label, visual digest, audio digest) → one canonical
 * concept_pop_id, so the same SNN population fires for image+word+text
 * of one referent. grounded_language mirrors lexicon registrations into
 * the registry via intern_text + bind_modalities on the production /
 * comprehension paths.
 *
 * Called from brain init after both subsystems exist. NULL-safe: the
 * library degrades gracefully if the registry isn't wired.
 *
 * @param gl       System handle
 * @param registry concept_registry_t* (opaque)
 */
void grounded_language_set_concept_registry(
    grounded_language_t* gl,
    void* registry);

/**
 * @brief Wire the toxicity classifier so respond/produce can annotate output.
 *
 * After wiring, grounded_language_respond classifies the produced text and
 * audit-logs detections. POLICY: mark, never delete — the classifier never
 * modifies content; it only attaches scores for downstream evaluators.
 *
 * @param gl  Grounded language instance.
 * @param tc  toxicity_classifier_t* (opaque), or NULL to unwire.
 */
void grounded_language_set_toxicity_classifier(
    grounded_language_t* gl,
    void* tc);

/**
 * @brief Wire the toxicity response engine for counterclaim emission.
 *
 * When wired AND the classifier flags input as toxic, respond() emits a
 * stage-appropriate counterclaim instead of the (probably-collapsed)
 * produce output. NULL to unwire.
 */
void grounded_language_set_toxicity_response(
    grounded_language_t* gl,
    void* tr);

/**
 * @brief Set valence/arousal on words in the given text.
 *
 * Tokenizes the text, looks up or creates each lexicon entry, and updates
 * its `valence` (in [-1, +1]) and `arousal` (in [0, 1]) using a saturating
 * EMA so the tag accumulates rather than overwrites on repeat exposure.
 *
 * Phase 3c/3d (2026-05-21): used to tag counterclaim words with positive
 * valence (so produce-side suppression in find_words_near_vector PROMOTES
 * them) and to tag toxic content words with negative valence (so produce
 * DAMPS them). The 0.2 weight on each call means ~5 exposures saturate
 * the tag.
 *
 * Round-2 risk-3 (2026-05-21): the `content_only` flag (1 = skip function
 * words, 0 = tag everything) prevents counterclaim function words ("the",
 * "of", "are") from accumulating positive valence across thousands of
 * detections, which would eventually defeat the suppression mechanism
 * by making EVERY common word look "good". Toxic-input tagging passes
 * content_only=0 (function words there are part of the toxic structure
 * and should also acquire negative valence); counterclaim tagging passes
 * content_only=1.
 *
 * @return Number of lexicon entries touched (0 on empty text).
 */
int grounded_language_tag_text_affective(
    grounded_language_t* gl,
    const char* text,
    float valence,
    float arousal,
    int content_only);

/**
 * @brief Decay all lexicon valences toward zero by a small factor.
 *
 * Called from the toxicity cycle tick (1Hz) so single mis-tagged events
 * eventually relax back to neutral rather than locking in permanent
 * negative valence on benign words. Both valence and arousal are
 * multiplied by `factor` (typically 0.999 — a 0.1% per-call decay,
 * half-life ~12 minutes at 1Hz). Set factor=1.0 to disable.
 *
 * @return Number of lexicon entries touched.
 */
int grounded_language_decay_all_valence(
    grounded_language_t* gl,
    float factor);

/**
 * @brief Update the developmental stage used for counterclaim selection.
 * Stage 0..3 — out-of-range values are clamped.
 */
void grounded_language_set_current_stage_int(
    grounded_language_t* gl,
    int stage);

/*=============================================================================
 * Comprehension (Wernicke's pathway)
 *===========================================================================*/

/**
 * @brief Comprehend text by activating grounded concepts
 *
 * WHAT: Parse text into words, activate concept bindings, integrate meaning
 * WHY:  Understanding = activating the right concepts in semantic memory
 * HOW:  For each word: lookup in lexicon -> activate bound concepts ->
 *       spreading activation -> context integration -> semantic vector
 *
 * @param gl       System handle
 * @param text     Input text to comprehend
 * @param result   Output comprehension result (caller allocates, system fills)
 * @return 0 on success, -1 on error
 */
int grounded_language_comprehend(
    grounded_language_t* gl,
    const char* text,
    gl_comprehension_result_t* result);

/**
 * @brief Free comprehension result internals
 * @param result Result to clean up (struct itself not freed)
 */
void gl_comprehension_result_cleanup(gl_comprehension_result_t* result);

/*=============================================================================
 * Tier-2 #3: Negation polarity
 *
 * When enabled (default true), grounded_language_comprehend scans the
 * tokenized text for negation cues — `not`, `no`, `never`, the `n't`
 * suffix on a preceding token, and the standard contractions
 * (`don't`, `doesn't`, `didn't`, `won't`, `can't`, `isn't`, `wasn't`,
 * `aren't`, `weren't`, `haven't`, `hasn't`, `hadn't`, `shouldn't`,
 * `wouldn't`, `couldn't`). The next non-function content word within a
 * GL_NEGATION_WINDOW lookahead has the sign of its activation_levels[]
 * entries flipped before they are recorded in the result. Each
 * inversion bumps gl_stats_t.negation_events.
 *
 * Disabling reverts to the legacy "I do not like cats" / "I do like
 * cats" producing identical activation profiles.
 *===========================================================================*/

/** Lookahead window (in tokens) for the negation cue → content-word
 *  search. 3 captures common patterns ("do not like", "is not very
 *  big") without spuriously flipping a content word two clauses away. */
#define GL_NEGATION_WINDOW   3

/**
 * @brief Toggle negation-driven activation-sign inversion.
 *
 * @param gl       System handle (NULL → no-op)
 * @param enabled  true (default) = invert activations on negation cue;
 *                 false = legacy bypass.
 */
void grounded_language_set_negation_enabled(
    grounded_language_t* gl,
    bool enabled);

/**
 * @brief Read the current negation toggle (for telemetry / tests).
 * @return false when gl is NULL or negation is disabled.
 */
bool grounded_language_get_negation_enabled(
    const grounded_language_t* gl);

/*=============================================================================
 * TA-5: Reconsolidation on contradiction.
 *
 * In cognitive neuroscience, retrieving a memory makes it labile —
 * contradicting evidence at recall time can re-encode the trace with
 * weakened strength rather than just laying down a parallel new memory.
 * For grounded language, negation in comprehend is the cheapest signal
 * that a binding's truth is being challenged. When TA-5 is enabled, every
 * negation-marked content word that has lexicon bindings has those
 * bindings' strength multiplied by (1 - reconsolidation_decay) and the
 * bumped exposure_count incremented (so re-asserted bindings recover).
 *
 * DEPENDENCY: Reconsolidation reads negate_word[] from the comprehend
 * negation pass. When enable_negation_inversion is OFF (Tier-2 #3),
 * the negation pass never fires, negate_word[] stays all-false, and
 * reconsolidation has nothing to act on — even if enable_reconsolidation
 * is ON, no events fire. Both toggles must be ON together for TA-5 to
 * have any effect. Direct calls to grounded_language_reconsolidate_word
 * bypass this dependency and run the decay directly on the named word.
 *===========================================================================*/

/** Default decay applied per reconsolidation event (5% off binding
 *  strength). Conservative — repeated contradictions across many turns
 *  are needed to fully erode a binding. */
#define GL_RECONSOLIDATION_DEFAULT_DECAY  0.05f

/** Default weight on the distributional (context-vector) term in the
 *  produce-time word score, vs the concept-binding term:
 *  raw = w*distributional_sim + (1-w)*concept_sim. Changed from 0.4 to 0.6
 *  (2026-10-21) to fix abstract noun bias: lets intent-correct concrete words'
 *  strong distributional matches outrank broad abstract clusters whose wide
 *  concept bindings dominate the 0.6 concept term. Empirically: comprehend("cat")
 *  now produces "cat" (conf 0.93) instead of "belief" (abstract cluster).
 *  Range [0,1]; tunable per-session via set_produce_distributional_weight(). */
#define GL_PRODUCE_DISTRIBUTIONAL_DEFAULT_WEIGHT  0.6f

/** Produce-score frequency penalty (2026-06-01). Default 0.0 = OFF (produce
 *  scoring unchanged). Raising it damps high-frequency words via
 *  raw *= 1/(1 + p*log1p(freq)) so distributionally-central technical-corpus
 *  vocabulary stops monopolizing produce filler positions. Range [0,inf). */
#define GL_PRODUCE_FREQUENCY_PENALTY_DEFAULT  0.0f

/**
 * @brief Toggle TA-5 reconsolidation-on-contradiction. Default OFF.
 *
 * When ON, comprehend() walks negate_word[] after the activation pass
 * and calls grounded_language_reconsolidate_word() on each negated
 * content word that has at least one lexicon entry. Off by default so
 * the trainer can opt in once the curriculum is contradiction-aware.
 */
void grounded_language_set_reconsolidation_enabled(
    grounded_language_t* gl,
    bool enabled);

/** Read the current reconsolidation toggle (for telemetry / tests). */
bool grounded_language_get_reconsolidation_enabled(
    const grounded_language_t* gl);

/**
 * @brief Tune the decay factor applied per reconsolidation event.
 *
 * Clamped to [0, 0.5]. 0 disables decay (so the toggle still fires
 * stats events but no weight changes); 0.5 halves binding strength on
 * every event (aggressive — only useful for explicit-contradiction
 * RPCs). Default GL_RECONSOLIDATION_DEFAULT_DECAY (0.05).
 */
void grounded_language_set_reconsolidation_decay(
    grounded_language_t* gl,
    float decay);

float grounded_language_get_reconsolidation_decay(
    const grounded_language_t* gl);

/**
 * @brief Set the produce-time distributional weight (vs concept-binding).
 *
 * score_word_against_vector blends two cosine terms:
 *   raw = w * distributional_sim + (1 - w) * concept_sim
 * where w is this weight. Default GL_PRODUCE_DISTRIBUTIONAL_DEFAULT_WEIGHT
 * (0.4) reproduces the historical 0.4/0.6 split. Raising w toward ~0.7
 * lets the intent-correct distributional signal outrank broadly-concept-
 * bound abstract words that otherwise monopolize produce. Clamped [0,1].
 */
void grounded_language_set_produce_distributional_weight(
    grounded_language_t* gl,
    float weight);

float grounded_language_get_produce_distributional_weight(
    const grounded_language_t* gl);

/**
 * @brief Set/get the produce-score frequency penalty (default 0.0 = OFF).
 *
 * Damps high-frequency words in score_word_against_vector via
 *   raw *= 1 / (1 + penalty * log1p(entry->frequency))
 * so distributionally-central, high-exposure technical vocabulary stops
 * winning produce filler positions (word-salad contamination). penalty<0 is
 * clamped to 0. Range [0, inf); ~0.1-0.3 is a reasonable starting band.
 */
void grounded_language_set_produce_frequency_penalty(
    grounded_language_t* gl,
    float penalty);

float grounded_language_get_produce_frequency_penalty(
    const grounded_language_t* gl);

/**
 * @brief Apply reconsolidation decay to every binding of `word`.
 *
 * Multiplies each binding[i].strength by (1 - decay). decay is clamped
 * to [0, 0.5]. decay == 0 → no-op (still bumps stats so callers can
 * verify the path was taken). NULL-safe — returns 0 if gl/word is NULL
 * or the word isn't in the lexicon.
 *
 * @return Number of bindings whose strength was decayed.
 */
uint32_t grounded_language_reconsolidate_word(
    grounded_language_t* gl,
    const char* word,
    float decay);

/*=============================================================================
 * TB-9: Speech-act intent classification on comprehend.
 *
 * Cheap rule-based classifier that labels every comprehended utterance
 * with a coarse pragmatic intent (assertion / question / imperative /
 * greeting / exclamation). Pattern detection only — no ML, no SNN —
 * driven by the tokenized lower-cased words plus the raw input text.
 *
 * Detection rules:
 *   - QUESTION   : raw text ends with '?', OR first token is a wh-word,
 *                  OR first token is an auxiliary.
 *   - IMPERATIVE : first token is in a small imperative-cue set, OR text
 *                  ends with '!' AND no first-person pronoun in the
 *                  first 3 tokens.
 *   - GREETING   : one of the first 2 tokens is a greeting cue.
 *   - EXCLAMATION: text ends with '!' and didn't fit IMPERATIVE.
 *   - ASSERTION  : default fallback (declarative).
 *
 * Default OFF — when disabled, comprehend leaves
 * result->speech_act == GL_SPEECH_ACT_UNKNOWN and never bumps the
 * speech_act_* stats counters. Per-instance runtime knob, not persisted
 * across save/load.
 *===========================================================================*/

/**
 * @brief Toggle TB-9 speech-act intent classification. Default OFF.
 */
void grounded_language_set_speech_act_classification_enabled(
    grounded_language_t* gl,
    bool enabled);

/** Read the current speech-act classification toggle. false on NULL. */
bool grounded_language_get_speech_act_classification_enabled(
    const grounded_language_t* gl);

/*=============================================================================
 * TB-6: Sentence-boundary segmentation in comprehend.
 *
 * When ON and input contains >1 sentence (terminators `.`/`!`/`?`),
 * comprehend recursively processes each sentence in input order so
 * each one pushes its own discourse turn, anaphora resolves across
 * sentences, and bigram learning never bridges a sentence boundary.
 * Default OFF — preserves bit-for-bit legacy behaviour.
 *
 * STATS COUNTER SEMANTICS UNDER SEGMENTATION:
 * Because segmentation re-enters comprehend once per sentence, every
 * stats counter that bumps inside the comprehend body bumps PER
 * SENTENCE rather than per call. A 3-sentence input pushes:
 *   - total_comprehensions     +3  (not +1)
 *   - negation_events          +1 per negated sentence
 *   - engram_encodes / recalls +1 per sentence with concepts
 *   - topic_shifts_evaluated   +1 per sentence (subject to min_turns gate)
 *   - speech_act_*             +1 per sentence (per the per-sentence label)
 *   - reconsolidation_events   +1 per negated content word per sentence
 * In addition, sentences_processed bumps +N (one per sentence) and
 * multi_sentence_inputs bumps +1 per outer call where N>1.
 *
 * This is intentional — per-sentence semantics are how segmentation
 * gives downstream cognition the granularity it needs — but consumers
 * comparing pre-TB-6 vs post-TB-6 stat snapshots will see counter
 * inflation proportional to avg-sentences-per-input. Use
 * sentences_processed / total_comprehensions to back-out the
 * normalization if you need pre-TB-6-comparable numbers.
 *===========================================================================*/

/** Toggle TB-6 sentence-boundary segmentation. Default OFF. */
void grounded_language_set_sentence_segmentation_enabled(
    grounded_language_t* gl,
    bool enabled);

/** Read the current sentence-segmentation toggle. */
bool grounded_language_get_sentence_segmentation_enabled(
    const grounded_language_t* gl);

/*=============================================================================
 * Tier-2 #7: Multi-turn discourse state
 *
 * The legacy gl_context_t single rolling vector remains as the
 * backward-compatible recency-weighted blend, but a small ring buffer
 * of per-turn semantic vectors is now retained behind it. Turns can be
 * pushed explicitly by callers (e.g. dialog drivers separating user
 * vs. assistant utterances) or are auto-pushed by comprehend whenever
 * a non-zero semantic vector is produced.
 *
 * GL_DISCOURSE_MAX_TURNS bounds the buffer at compile time;
 * grounded_language_set_discourse_capacity clamps the active capacity
 * at runtime (1..GL_DISCOURSE_MAX_TURNS). Capacity reductions evict
 * oldest turns first.
 *===========================================================================*/

/** Public mirror of the internal cap; callers may rely on this. */
#define GL_DISCOURSE_MAX_TURNS_PUBLIC 8

/**
 * @brief Append a turn to the discourse buffer, evicting the oldest
 *        turn if the buffer is full. The semantic vector is copied
 *        (gl-owned). Pass NULL `semantic_vec` to push a placeholder
 *        with no vector content (still increments count).
 *
 * @param gl           System handle (NULL → no-op, returns -1)
 * @param semantic_vec Length GL_SEMANTIC_DIM (or gl's semantic_dim);
 *                     NULL stores an empty placeholder.
 * @param vec_dim      Number of elements available at semantic_vec
 *                     (clamped to gl->semantic_dim internally)
 * @param n_words      Word count of the turn (caller-supplied)
 * @param is_user      true = user/external turn, false = system/produced
 * @return 0 on success, -1 on bad params / alloc failure.
 */
int grounded_language_push_turn(
    grounded_language_t* gl,
    const float* semantic_vec,
    uint32_t vec_dim,
    uint32_t n_words,
    bool is_user);

/** @return Current number of populated turns (0..capacity). 0 on NULL. */
uint8_t grounded_language_get_discourse_turn_count(
    const grounded_language_t* gl);

/**
 * @brief Read a recent discourse turn's semantic vector (Tier 1 Step E).
 *
 * For multi-turn continuity: the production cascade blends a recent turn's
 * topic vector into its content intent so replies stay coherent with the
 * ongoing conversation, not just the immediate prompt.
 *
 * @param gl       System handle.
 * @param back     1 = newest turn, 2 = the one before it, etc. (>0).
 * @param out_vec  Receives a GL-owned READ-ONLY pointer (do not free);
 *                 valid only until the next discourse mutation.
 * @param out_dim  Receives the vector length (== semantic_dim).
 * @return true if the requested turn exists and has a vector; false
 *         otherwise (args untouched). Use to no-op on early turns.
 */
bool grounded_language_get_recent_turn_vector(
    const grounded_language_t* gl,
    uint8_t back,
    const float** out_vec,
    uint32_t* out_dim);

/**
 * @brief Lock-guarded COPY of a recent discourse turn's semantic vector.
 *
 * Thread-safe alternative to grounded_language_get_recent_turn_vector: copies
 * the turn vector into the caller's buffer under the same mutate_lock that
 * push_turn holds when it frees evicted turn vectors, so the caller never
 * holds a raw interior pointer that a concurrent push_turn could free.
 * Prefer this in any path that may run alongside comprehend/push_turn.
 *
 * @param gl       System handle.
 * @param back     1 = newest turn, 2 = the one before it, etc. (>0).
 * @param out      Destination buffer (>= max_dim floats).
 * @param max_dim  Capacity of `out` in floats.
 * @return number of floats written (min(semantic_dim, max_dim)), or 0 if the
 *         turn doesn't exist / has no vector / args invalid.
 */
uint32_t grounded_language_copy_recent_turn_vector(
    grounded_language_t* gl,
    uint8_t back,
    float* out,
    uint32_t max_dim);

/**
 * @brief Clamp the active discourse capacity. Reductions evict oldest
 *        turns first so the most recent `capacity` turns survive.
 *        Increases just bump the cap; existing turns are untouched.
 *
 * @param gl        System handle (NULL → no-op)
 * @param capacity  Clamped to [1, GL_DISCOURSE_MAX_TURNS_PUBLIC].
 */
void grounded_language_set_discourse_capacity(
    grounded_language_t* gl,
    uint8_t capacity);

/*=============================================================================
 * TB-10: Topic-shift detection in discourse.
 *
 * Discourse turns are pushed by every successful comprehend (TA-1 work).
 * For dialog systems and curriculum, downstream cognition wants to *know*
 * when the topic shifts — when the latest turn's semantic content is far
 * from the recent average. This lets the brain refresh attention, drop
 * stale anaphora referents, and signal a topic boundary to memory
 * consolidation.
 *
 * Detection is a cheap cosine-distance check over the discourse ring:
 * cosine(latest_turn, mean(prior up-to-K turns)). When the score falls
 * below `topic_shift_threshold` AND we have at least
 * `topic_shift_min_turns` prior turns to average, comprehend flags a
 * topic boundary on the gl instance + bumps the
 * topic_shifts_detected counter.
 *
 * Default OFF — preserves legacy comprehend behaviour bit-for-bit.
 * Opt-in per system instance via
 * grounded_language_set_topic_shift_enabled.
 *===========================================================================*/

/** Default cosine-similarity threshold below which a discourse turn
 *  triggers a topic-shift flag. 0.3 is conservative — cosine has to drop
 *  meaningfully (semantically distant) before the boundary fires.
 *  Clamped to [0, 1] by the setter. */
#define GL_TOPIC_SHIFT_DEFAULT_THRESHOLD  0.3f

/** Default minimum number of prior turns required before a topic-shift
 *  decision can be made. With < 2 priors there's no stable mean to
 *  compare against, so the detector reports "no shift" (score 1.0) and
 *  the evaluated/detected counters do not advance. Clamped to
 *  [2, GL_DISCOURSE_MAX_TURNS_PUBLIC] by the setter. */
#define GL_TOPIC_SHIFT_DEFAULT_MIN_TURNS  3u

/**
 * @brief Toggle TB-10 topic-shift detection. Default OFF.
 *
 * When ON, comprehend computes a cosine score between the most recent
 * discourse turn and the mean of the prior up-to-K turns after pushing
 * the new turn into the ring. The score + boundary flag are stashed
 * on the gl instance and queryable via the get_last_* helpers below.
 */
void grounded_language_set_topic_shift_enabled(
    grounded_language_t* gl,
    bool enabled);

/** Read the current topic-shift toggle (for telemetry / tests). */
bool grounded_language_get_topic_shift_enabled(
    const grounded_language_t* gl);

/**
 * @brief Tune the cosine-similarity threshold below which a discourse
 *        turn fires a topic-shift flag. Clamped to [0, 1]; NaN → 0.
 *
 * 0.0 = never fire (every turn is "the same topic"); 1.0 = always fire
 * (every turn is a topic shift). 0.3 is the conservative default.
 */
void grounded_language_set_topic_shift_threshold(
    grounded_language_t* gl,
    float threshold);

float grounded_language_get_topic_shift_threshold(
    const grounded_language_t* gl);

/**
 * @brief Tune the minimum number of prior discourse turns required
 *        before a shift can fire. Clamped to
 *        [2, GL_DISCOURSE_MAX_TURNS_PUBLIC].
 */
void grounded_language_set_topic_shift_min_turns(
    grounded_language_t* gl,
    uint32_t min_turns);

uint32_t grounded_language_get_topic_shift_min_turns(
    const grounded_language_t* gl);

/**
 * @brief Read the most recently computed topic-shift score (cosine
 *        similarity, [0, 1]). 1.0 = no detected shift; values below the
 *        configured threshold indicate a topic boundary fired this
 *        comprehend. Returns 1.0 on NULL / when no comprehend has yet
 *        produced enough turns.
 */
float grounded_language_get_last_topic_shift_score(
    const grounded_language_t* gl);

/**
 * @brief Read whether the most recent comprehend pass flagged a topic
 *        boundary. Returns false on NULL or when topic-shift detection
 *        is disabled / hasn't seen enough turns yet.
 */
bool grounded_language_last_was_topic_shift(
    const grounded_language_t* gl);

/*=============================================================================
 * Tier-2 #6: Word-sense disambiguation (polysemy)
 *
 * Lexicon entries already carry a `bindings` array — each binding maps
 * the word form to a different concept (river bank vs. money bank).
 * When sense disambiguation is enabled, comprehend weights each
 * binding's contribution by its cosine match against the running
 * context vector: the most-relevant sense gets the highest weight and
 * off-sense bindings damp toward GL_SENSE_OFF_DAMP. Each
 * disambiguation event (an entry with > 1 binding scored against the
 * intent) bumps gl_stats_t.sense_resolutions.
 *
 * Default OFF — preserves the legacy "every binding contributes its
 * raw strength" behaviour. Opt-in per system instance via
 * grounded_language_set_sense_disambiguation_enabled.
 *===========================================================================*/

/** Floor weight for off-sense bindings when sense disambiguation is
 *  active. Keeps the off-sense activation alive but heavily damped. */
#define GL_SENSE_OFF_DAMP    0.1f

/** Toggle word-sense disambiguation on a gl instance. NULL-safe. */
void grounded_language_set_sense_disambiguation_enabled(
    grounded_language_t* gl,
    bool enabled);

/** Read the sense-disambiguation toggle. false on NULL. */
bool grounded_language_get_sense_disambiguation_enabled(
    const grounded_language_t* gl);

/**
 * @brief Pick the binding index whose concept features best align with
 *        an intent vector. Returns 0 (the first binding) when the
 *        entry has no bindings, exactly one binding, NULL features for
 *        every binding, or when gl/entry is NULL.
 *
 *        Used internally by comprehend's polysemy path; exposed for
 *        tests + callers that want to query "what sense does this
 *        word take in *this* context?" without running the full
 *        comprehend pipeline.
 *
 * @param gl          System handle (NULL → 0)
 * @param entry       Lexicon entry (NULL → 0)
 * @param intent_vec  Length gl->semantic_dim (NULL → 0)
 * @return Best-matching binding index in [0, entry->binding_count).
 */
uint32_t grounded_language_disambiguate_sense(
    const grounded_language_t* gl,
    const gl_lexicon_entry_t* entry,
    const float* intent_vec);

/*=============================================================================
 * Grounding (Cross-modal binding)
 *===========================================================================*/

/**
 * @brief Ground a word in sensory experience
 *
 * WHAT: Create or strengthen a word-concept binding through cross-modal association
 * WHY:  This is how humans learn words — by experiencing the referent
 * HOW:  1. Find or create concept from sensory features (semantic memory)
 *       2. Find or create lexicon entry for word
 *       3. Hebbian binding: strengthen word->concept association
 *       4. Update modality-specific grounding weights
 *
 * @param gl     System handle
 * @param event  Grounding event (word + sensory input + context)
 * @return 0 on success, -1 on error
 */
int grounded_language_ground(
    grounded_language_t* gl,
    const gl_grounding_event_t* event);

/**
 * @brief Learn word meaning from text context (distributional learning)
 *
 * WHAT: Update word representations from co-occurrence patterns
 * WHY:  "You shall know a word by the company it keeps" (Firth, 1957)
 * HOW:  Sliding window over text, update context vectors via co-occurrence,
 *       infer word class from distributional patterns
 *
 * @param gl    System handle
 * @param text  Text to learn from
 * @return Number of word bindings updated, or -1 on error
 */
int grounded_language_learn_from_text(
    grounded_language_t* gl,
    const char* text);

/**
 * @brief Teach a word-concept pair directly (fast mapping)
 *
 * WHAT: One-shot word learning ("This is a gavagai")
 * WHY:  Children can learn a word from a single labeled example
 * HOW:  Create strong binding between word and concept immediately
 *
 * @param gl          System handle
 * @param word        Word form to learn
 * @param concept_features  Semantic features of the concept
 * @param feature_dim       Dimension of features
 * @param category    Concept category (CONCEPT_OBJECT, etc.)
 * @return Concept ID of bound concept, or 0 on error
 */
uint64_t grounded_language_fast_map(
    grounded_language_t* gl,
    const char* word,
    const float* concept_features,
    uint32_t feature_dim,
    uint32_t category);

/**
 * @brief One-shot prune: keep top-K bindings per word by strength.
 *
 * Intended as a maintenance sweep after the 2026-05-15 cross-bind wedge fix to
 * reclaim binding_count that accumulated under the prior function-word leak.
 * Sorts each entry's bindings descending by strength and truncates to
 * @p max_bindings_per_word.
 *
 * @param gl                       System handle
 * @param max_bindings_per_word    K, the per-word cap (suggested 128)
 * @return Total bindings dropped across the lexicon.
 */
uint64_t grounded_language_prune_bindings(
    grounded_language_t* gl,
    uint32_t max_bindings_per_word);

/**
 * @brief Reset every lexicon entry's distributional embedding.
 *
 * Walks the vocab and either zeros + marks-uninitialized each entry's
 * context_vector (if `zero_and_mark_uninit` is true), or re-randomizes
 * it to small noise in [-jitter, +jitter] (if false). Use when
 * pairwise cos(comprehend(p_i), comprehend(p_j)) ≈ 1.0 across distinct
 * prompts indicates the distributional channel has homogenized.
 *
 * `zero_and_mark_uninit=true` is the cleanest reset — comprehend's
 * binding+NLP-embedding loops still contribute, but the dominant
 * convergence channel (distributional) is removed until each word is
 * seen again in context.
 *
 * @return Number of lexicon entries touched, or -1 on bad args.
 */
int64_t grounded_language_reset_lexicon_distributional(
    grounded_language_t* gl,
    bool zero_and_mark_uninit,
    float jitter);

/**
 * @brief Reset concept grounding for a clean re-grounding pass.
 *
 * Clears every lexicon entry's concept bindings (keeps word forms,
 * bindings[] capacity, and distributional context_vector) AND wipes the
 * brain's semantic_memory concept store (frees capacity). Use after the
 * grounding-feature fix to recover from concept collapse: the prior
 * grounding pipeline fed a prefix-dominated feature vector to
 * find_or_create_concept (cosine dedup 0.85), collapsing every word onto
 * one concept and filling the store to capacity. Re-grounding then
 * rebuilds a diverse store from clean embeddings.
 *
 * NOTE: semantic_memory is shared with other cognitive modules; resetting
 * it drops all concepts brain-wide. Stale concept_id references resolve to
 * NULL (handled gracefully by get_concept_features and callers).
 *
 * @return Number of bindings cleared, or -1 on NULL gl.
 */
int64_t grounded_language_reset_concept_grounding(grounded_language_t* gl);

/*=============================================================================
 * Production (Broca's pathway)
 *===========================================================================*/

/**
 * @brief Produce text from a semantic intent
 *
 * WHAT: Convert internal semantic representation to natural language
 * WHY:  Expression = finding words that point to the intended concepts
 * HOW:  1. Activate concepts closest to semantic intent
 *       2. For each concept, find highest-strength word bindings
 *       3. Select syntactic template matching word classes
 *       4. Fill template slots, apply morphology
 *       5. Output text
 *
 * @param gl           System handle
 * @param intent       Semantic vector representing intended meaning [semantic_dim]
 * @param intent_dim   Dimension of intent vector
 * @param mode         Production mode (describe, narrate, respond, etc.)
 * @param result       Output production result
 * @return 0 on success, -1 on error
 */
int grounded_language_produce(
    grounded_language_t* gl,
    const float* intent,
    uint32_t intent_dim,
    gl_production_mode_t mode,
    gl_production_result_t* result);

/**
 * @brief Produce text describing a specific concept
 *
 * WHAT: Generate a description of a concept using grounded vocabulary
 * WHY:  "What is X?" — describe by activating related concepts and verbalizing
 * HOW:  Activate concept, spread to related concepts, select descriptive words
 *
 * @param gl          System handle
 * @param concept_id  Concept to describe
 * @param result      Output production result
 * @return 0 on success, -1 on error
 */
int grounded_language_describe_concept(
    grounded_language_t* gl,
    uint64_t concept_id,
    gl_production_result_t* result);

/**
 * @brief Produce text by blending two concepts (creative generation)
 *
 * WHAT: Conceptual blending — combine two concepts to create novel expression
 * WHY:  Creativity = novel combinations. "Time is money" blends TIME + MONEY.
 * HOW:  Interpolate concept features, find words for blended space, generate text
 *
 * @param gl          System handle
 * @param concept_a   First concept ID (or 0 to use vector_a)
 * @param concept_b   Second concept ID (or 0 to use vector_b)
 * @param vector_a    Feature vector for concept A (used if concept_a == 0)
 * @param vector_b    Feature vector for concept B (used if concept_b == 0)
 * @param vec_dim     Dimension of feature vectors
 * @param blend_ratio Interpolation ratio [0=all A, 1=all B, 0.5=even blend]
 * @param result      Output production result
 * @return 0 on success, -1 on error
 */
int grounded_language_blend(
    grounded_language_t* gl,
    uint64_t concept_a,
    uint64_t concept_b,
    const float* vector_a,
    const float* vector_b,
    uint32_t vec_dim,
    float blend_ratio,
    gl_production_result_t* result);

/**
 * @brief Generate narrative by walking through semantic space
 *
 * WHAT: Produce multi-sentence text by chaining related concepts
 * WHY:  Stories and essays work by traversing a path through meaning space
 * HOW:  Start from seed concept, follow strongest relations, produce sentence
 *       per concept cluster, maintain coherence through context vector
 *
 * @param gl            System handle
 * @param seed_concept  Starting concept ID
 * @param num_sentences Target number of sentences
 * @param creativity    Exploration vs exploitation [0=safe, 1=wild]
 * @param result        Output production result
 * @return 0 on success, -1 on error
 */
int grounded_language_narrate(
    grounded_language_t* gl,
    uint64_t seed_concept,
    uint32_t num_sentences,
    float creativity,
    gl_production_result_t* result);

/**
 * @brief Free production result internals
 * @param result Result to clean up (struct itself not freed)
 */
void gl_production_result_cleanup(gl_production_result_t* result);

/*=============================================================================
 * Conversation (Comprehension + Production loop)
 *===========================================================================*/

/**
 * @brief Process input and generate a response (full conversation turn)
 *
 * WHAT: Comprehend input text, then produce a relevant response
 * WHY:  This is the core conversational loop
 * HOW:  comprehend(input) -> integrate with context -> produce(response)
 *
 * @param gl           System handle
 * @param input_text   What was said to the brain
 * @param response     Output buffer for response text
 * @param response_max Maximum response buffer size
 * @param confidence   Output: response confidence [0,1]
 * @return Number of characters written, or -1 on error
 */
int grounded_language_respond(
    grounded_language_t* gl,
    const char* input_text,
    char* response,
    uint32_t response_max,
    float* confidence);

/*=============================================================================
 * Training / Learning
 *===========================================================================*/

/**
 * @brief Learn from a sentence pair (teacher-guided)
 *
 * WHAT: Learn associations from input-target pairs
 * WHY:  Social learning — a teacher provides correct responses
 * HOW:  Comprehend input, comprehend target, strengthen associations between
 *       input concepts and target words (and vice versa)
 *
 * @param gl           System handle
 * @param input_text   Input/stimulus text
 * @param target_text  Expected/correct response
 * @param learning_rate Hebbian learning rate (0 = default)
 * @return Learning loss (lower = better), or -1.0 on error
 */
float grounded_language_learn_pair(
    grounded_language_t* gl,
    const char* input_text,
    const char* target_text,
    float learning_rate);

/**
 * @brief Learn syntactic patterns from text
 *
 * WHAT: Extract sentence structure templates from observed text
 * WHY:  Children learn grammar from examples, not rules
 * HOW:  Parse text into word classes, identify recurring patterns,
 *       create/strengthen template entries
 *
 * @param gl    System handle
 * @param text  Text to extract patterns from
 * @return Number of new patterns learned, or -1 on error
 */
int grounded_language_learn_syntax(
    grounded_language_t* gl,
    const char* text);

/*=============================================================================
 * Cross-modal Integration
 *===========================================================================*/

/**
 * @brief Connect to visual cortex for visual grounding
 * @param gl       System handle
 * @param vis_ctx  Visual cortex context (opaque pointer)
 */
void grounded_language_connect_visual(
    grounded_language_t* gl,
    void* vis_ctx);

/**
 * @brief Connect to auditory cortex for auditory grounding
 * @param gl        System handle
 * @param aud_ctx   Auditory cortex context (opaque pointer)
 */
void grounded_language_connect_auditory(
    grounded_language_t* gl,
    void* aud_ctx);

/**
 * @brief Connect to speech cortex for speech grounding
 * @param gl          System handle
 * @param speech_ctx  Speech cortex context (opaque pointer)
 */
void grounded_language_connect_speech(
    grounded_language_t* gl,
    void* speech_ctx);

/**
 * @brief Connect to cortical column pool for hierarchical feature extraction
 * @param gl        System handle
 * @param col_pool  Cortical column pool (opaque pointer)
 */
void grounded_language_connect_columns(
    grounded_language_t* gl,
    void* col_pool);

/**
 * @brief Connect to emotional system for affective grounding
 * @param gl        System handle
 * @param emo_ctx   Emotional context (opaque pointer, e.g., metabolic state)
 */
void grounded_language_connect_emotional(
    grounded_language_t* gl,
    void* emo_ctx);

/** Connect SNN language bridge for spike-driven dual-path production/comprehension */
struct snn_language_bridge;
void grounded_language_connect_snn_bridge(
    grounded_language_t* gl,
    struct snn_language_bridge* bridge);

/**
 * @brief Walk every lexicon entry and re-mirror every word↔concept
 *        binding into the currently-attached snn_language_bridge.
 *
 * Use this after any path that populated the lexicon WITHOUT going
 * through lexicon_bind() with snn_bridge already attached:
 *   - bulk lexicon loader (runs before snn_bridge is created)
 *   - sidecar persistence load (runs after init but mirror would
 *     otherwise be skipped — bindings are read straight into the
 *     entry array, bypassing lexicon_bind).
 *   - any future preload path that builds the lexicon directly.
 *
 * Idempotent — register_word/register_concept overwrite their slot
 * and bind() takes the max of existing-vs-new weight.
 *
 * No-op when gl->snn_bridge is NULL (the lexicon is fine, just no
 * bridge to mirror to). Returns the number of bindings mirrored
 * (sum across all entries) — 0 means lexicon is empty or bridge is
 * unattached.
 *
 * @param gl  System handle
 * @return    Total bindings mirrored, 0 on no-op or empty lexicon.
 */
uint64_t grounded_language_rebind_all_to_snn_bridge(
    grounded_language_t* gl);

/**
 * @brief Pseudo-concept-id migration (2026-05-30).
 *
 * Walks all lexicon entries and, for each binding whose concept_id has
 * the pseudo-concept marker bit (0x100000000ULL) set, mints a fresh
 * real concept_id by calling semantic_memory_create_concept with the
 * entry's distributional context_vector as features. Replaces the
 * binding's concept_id with the new value.
 *
 * Pseudo-concept-ids are stamped by find_or_create_concept when
 * gl->semantic_memory was NULL at grounding time. They never resolve
 * via semantic_memory_get_concept, so every comprehend call on a
 * checkpoint with pseudo-IDs has a zero concept-feature contribution
 * to its semantic vector — the root cause of the 2026-05-29 pod-side
 * mode collapse.
 *
 * No-op when gl->semantic_memory is NULL (still in pseudo-id mode),
 * lexicon is empty, or no bindings carry the marker bit. Entries
 * without an initialized context_vector are skipped (they need
 * fresh grounding from sensory features, not a context-vector proxy).
 *
 * Safe to call at any time, but intended for one-shot use right after
 * gl_persistence_load during checkpoint resume.
 *
 * @param gl  Grounded language handle
 * @return    Count of bindings successfully remapped to real concept_ids
 */
uint64_t grounded_language_remap_pseudo_concept_ids(
    grounded_language_t* gl);

/**
 * @brief PA-4: train the bridge with a single (prev, next) bigram via a
 *        next-token contrastive update.
 *
 * Computes the bridge's prediction conditioned on prev_word's reverse
 * encoding, then applies LTP toward bindings that should have produced
 * next_word, and (mild) LTD against false-winner candidates that beat
 * next_word. Equivalent in effect to one step of softmax cross-entropy
 * gradient descent on the binding matrix, but without the full softmax —
 * we only update the target row and the top-1 false-winner row.
 *
 * Prerequisites: bridge attached, prev_word has prior bindings (so the
 * encoding is non-zero). When either condition fails the call is a no-op.
 *
 * @param gl         GL handle.
 * @param prev_word  prefix token.
 * @param next_word  target next token.
 * @param lr         per-step learning rate (typical: 0.01–0.05).
 * @return 0 on update applied, -1 on validation failure or no-op.
 */
int grounded_language_learn_next_token_pair(
    grounded_language_t* gl,
    const char* prev_word,
    const char* next_word,
    float lr);

/**
 * @brief PA-4+: Riemannian / sigmoid-reparameterized next-token update.
 *
 * Same contract as grounded_language_learn_next_token_pair, but each
 * binding write is preconditioned by the diagonal Fisher metric of a
 * Bernoulli-like binding (treating w = σ(u) for an unconstrained u).
 * Steps near w∈{0,1} are damped naturally instead of being post-clipped.
 * Mid-range behaviour (w≈0.5) recovers the flat path to first order.
 *
 * Default OFF: callers must use this variant explicitly. The flat path
 * (grounded_language_learn_next_token_pair) remains unchanged.
 *
 * @return 0 on update applied, -1 on validation failure or no-op.
 */
int grounded_language_learn_next_token_pair_riemannian(
    grounded_language_t* gl,
    const char* prev_word,
    const char* next_word,
    float lr);

/**
 * @brief PA-4: walk the bigrams of `text` and apply a next-token update
 *        for each pair. Returns the number of bigrams processed.
 *
 * If the attached bridge has trigram learning enabled
 * (snn_language_bridge_set_trigram_learning_enabled), this call ALSO
 * walks every (w_t, w_{t+1}) → w_{t+2} trigram and applies a
 * grounded_language_learn_next_token_triple update at half the bigram lr.
 * Default is OFF, so PA-4 callers see bit-for-bit-unchanged behavior.
 *
 * @param gl    GL handle.
 * @param text  utterance to learn from.
 * @param lr    per-bigram learning rate.
 * @return non-negative count of *bigram* updates applied, or -1 on
 *         validation failure. Trigram updates are tracked separately on
 *         the bridge stats (total_trigram_updates).
 */
int grounded_language_learn_text_bigrams(
    grounded_language_t* gl,
    const char* text,
    float lr);

/**
 * @brief TA-4: train the bridge with a single (prev1, prev2, next) trigram.
 *
 * Encodes prev1 and prev2 separately, builds a context vector as the
 * arithmetic mean of the two encodings, then applies LTP/LTD on `next`'s
 * bindings using the merged context as the conditioning prefix. Same
 * one-step contrastive math as grounded_language_learn_next_token_pair —
 * just driven by a 2-token prefix.
 *
 * Cold-start safety: skips cleanly (returns -1) when prev1 OR prev2 has
 * no prior bridge bindings (encoded all-zero) — the merged context would
 * be uninformative. The bridge's cold-start counter is bumped via the
 * standard grounded_language_next_token_cold_start_skips path.
 *
 * On a successful update the bridge's total_trigram_updates stat is
 * incremented by one (regardless of how many concept rows were touched).
 *
 * @param gl     GL handle.
 * @param prev1  first prefix token.
 * @param prev2  second prefix token.
 * @param next   target next token.
 * @param lr     per-step learning rate (typical: 0.005–0.025; usually
 *               half the bigram lr to avoid trigram dominance).
 * @return 0 on update applied, -1 on validation failure or no-op.
 */
int grounded_language_learn_next_token_triple(
    grounded_language_t* gl,
    const char* prev1,
    const char* prev2,
    const char* next,
    float lr);

/**
 * @brief Attach a bigram-spectrum tracker (PA-4+ diagnostic).
 *
 * Caller owns the spectrum object. After this call, every successful
 * (prev, next) pair processed by grounded_language_learn_text_bigrams()
 * is forwarded to bigram_spectrum_record() on the attached spectrum.
 *
 * Pass NULL to detach. Default state: nothing attached → no behavior
 * change. The pair-id used is the SNN word_pop index of each lexicon
 * entry (form_hash % SNN_LANG_MAX_WORD_POPS), so the spectrum's
 * vocab_cap should be ≤ SNN_LANG_MAX_WORD_POPS.
 *
 * @param gl        GL handle.
 * @param spectrum  bigram_spectrum_t* opaque pointer; non-owning.
 */
void grounded_language_attach_bigram_spectrum(
    grounded_language_t* gl,
    void* spectrum);

/**
 * @brief CC-1 — periodic refresh of the attached bigram-spectrum cache.
 *
 * Called from the BRAIN_CYCLE_LANGUAGE tick (16ms). Internally invokes
 * bigram_spectrum_maybe_compute(), which only runs the FFT/entropy pass
 * when at least `min_delta_events` new bigram events have been recorded
 * since the last successful compute. NULL-safe: when no spectrum is
 * attached, returns 0 with no side effects.
 *
 * @param gl                System handle (NULL → 0).
 * @param min_delta_events  Minimum new bigram events required to refresh
 *                          (gate against busy-spinning compute on idle).
 * @return  1 = compute ran, 0 = skipped (no spectrum or below threshold),
 *         -1 = compute failure.
 */
int grounded_language_tick_bigram_spectrum(
    grounded_language_t* gl,
    uint64_t min_delta_events);

/**
 * @brief Walkthrough round 3 observability: how many `learn_next_token_pair`
 *        calls hit the cold-start guard (encode_word returned all-zeros)
 *        and were skipped silently. Process-global counter; useful for
 *        diagnosing "training looks like a no-op" symptoms. Atomic.
 */
uint64_t grounded_language_next_token_cold_start_skips(void);

/**
 * @brief TC-13 observability: how many gl events have been pushed into
 *        the theory-of-mind module's observe pipeline since process
 *        start (process-global counter). pushed counts successful
 *        tom_observe() calls, dropped counts ones tom_observe rejected.
 *        Both stay 0 when no theory-of-mind module has been attached
 *        via grounded_language_attach_theory_of_mind().
 */
uint64_t nimcp_gl_tom_observations_pushed(void);
uint64_t nimcp_gl_tom_observations_dropped(void);

/**
 * @brief Walkthrough round 3 observability: how many bigram-spectrum
 *        attach calls were dropped because the global slot map was full.
 *        Helps detect mis-attached spectrums in multi-brain scenarios.
 */
uint64_t grounded_language_bigram_spectrum_map_overflow_warns(void);

/*=============================================================================
 * Tier-1 #2: Rule-based anaphora / pronoun resolution (v1).
 *
 * When enabled, grounded_language_comprehend tracks recent content nouns in
 * a small per-gl ring (8 entries) and, on encountering a pronoun
 * (he/she/it/they/him/her/them/his/hers/their/theirs/this/that/these/those),
 * resolves it to the most-recent referent whose gender (singular_male /
 * singular_female / singular_neuter / plural) matches. The pronoun is then
 * treated as if it were the referent: the referent's lexicon-entry bindings
 * are folded into the activated_concepts list with weight 0.5.
 *
 * Default: OFF. Opt-in via grounded_language_set_anaphora_enabled(gl,true)
 * (or via the brain wrapper / Python binding / daemon RPC).
 *
 * Limitations (v1):
 *   - No syntactic / binding-theory constraints — pure recency match.
 *   - Multi-referent ties are broken by recency (most-recent wins).
 *   - Gender heuristic is naive: capitalized non-sentence-initial proper
 *     nouns default to singular_male; any token following an explicit
 *     "she"/"her" cue is marked singular_female; trailing 's' marks plural;
 *     everything else is singular_neuter.
 *===========================================================================*/

/**
 * @brief Toggle the anaphora-resolution pass inside comprehend.
 *
 * @param gl       System handle.
 * @param enabled  true = resolve pronouns, false = no-op (default).
 * @return true if the toggle was applied; false if gl is NULL or the
 *         resolver state could not be allocated.
 */
bool grounded_language_set_anaphora_enabled(grounded_language_t* gl,
                                              bool enabled);

/** Read-side counterpart for lang_status. Returns false when the
 *  resolver state hasn't been allocated yet (== disabled). */
bool grounded_language_get_anaphora_enabled(const grounded_language_t* gl);

/*=============================================================================
 * NLP-1: Subword OOV / morphological fallback.
 *
 * Default OFF. When ON and a tokenizer is attached, comprehend bootstraps
 * fresh-lexicon entries (no bindings) by segmenting the word via BPE and
 * averaging bindings from any subword pieces already in the lexicon.
 * Lets morphological variants ("running", "ran") inherit from a learned
 * stem ("run") without explicit grounding for each surface form.
 *
 * Tokenizer pointer is borrowed (NOT freed by GL). Trainer must outlive
 * the gl that's pointing at it.
 *===========================================================================*/

/* Forward decl to keep this header free of the tokenizer impl include. */
struct nimcp_tokenizer;
typedef struct nimcp_tokenizer nimcp_tokenizer_t;

/** Attach a BPE tokenizer for OOV fallback. Pass NULL to detach. */
void grounded_language_attach_subword_tokenizer(grounded_language_t* gl,
                                                  struct nimcp_tokenizer* tok);

/** Toggle subword OOV fallback. Default OFF. Has no effect if no
 *  tokenizer is attached (silent — the comprehend hook short-circuits). */
void grounded_language_set_subword_oov_fallback_enabled(
    grounded_language_t* gl, bool enabled);

bool grounded_language_get_subword_oov_fallback_enabled(
    const grounded_language_t* gl);

/** Stats accessors for lang_status. */
uint64_t grounded_language_subword_oov_attempts(const grounded_language_t* gl);
uint64_t grounded_language_subword_oov_resolved(const grounded_language_t* gl);

/*=============================================================================
 * NLP-2: Coreference resolution beyond pronouns.
 *
 * Default OFF. When ON, comprehend tracks definite-NP head mentions
 * (content noun preceded by "the"/"this"/"that"/"these"/"those") in a
 * ring; on a repeat mention of the same head within the past K turns,
 * fires GL_EVENT_COREF_RESOLVED and bumps coref_resolved.
 *
 * Same-surface only: this version links "the dog" ↔ "the dog" across
 * turns, NOT "the dog" ↔ "the puppy". Semantic-vec fuzzy linking is
 * future work — the wiring (event, stats, flag, ring) is the
 * foundation that enables it without touching the comprehend hook.
 *===========================================================================*/

void grounded_language_set_coref_resolution_enabled(grounded_language_t* gl,
                                                       bool enabled);
bool grounded_language_get_coref_resolution_enabled(
    const grounded_language_t* gl);

uint64_t grounded_language_coref_attempts(const grounded_language_t* gl);
uint64_t grounded_language_coref_resolved(const grounded_language_t* gl);

/**
 * @brief Process-global counter of successful pronoun → referent matches.
 *
 * Atomic; reads from any thread are safe. Useful for verifying the pass
 * is firing in production traffic (default state should be 0).
 */
uint64_t grounded_language_anaphora_resolutions(void);

/**
 * @brief Connect to working memory — active words get pushed to the
 *        Miller-7±2 buffer with attention-derived salience, so the rest
 *        of the cognitive stack can reason over recently-grounded words.
 * @param gl  System handle
 * @param wm  working_memory_t* (opaque here to avoid include surface)
 */
void grounded_language_connect_working_memory(
    grounded_language_t* gl,
    void* wm);

/**
 * @brief Connect to episodic replay buffer — every grounding event gets
 *        recorded as a replay-eligible experience. During sleep the
 *        replay system re-presents high-importance grounding events
 *        through brain.learn_vector to consolidate the binding.
 * @param gl      System handle
 * @param replay  nimcp_episodic_replay_t* (opaque)
 */
void grounded_language_connect_episodic_replay(
    grounded_language_t* gl,
    void* replay);

/**
 * @brief Connect to the hippocampus adapter — high-attention grounding
 *        events train word→concept associations in the hippocampus,
 *        producing genuine episodic encoding of when a word was learned.
 * @param gl           System handle
 * @param hippocampus  hippocampus_adapter_t* (opaque)
 */
void grounded_language_connect_hippocampus(
    grounded_language_t* gl,
    void* hippocampus);

/**
 * @brief Attach the brain-level engram system in read-only mode.
 *
 * WHAT: Lets comprehend() lay down a memory trace at the end of each
 *       pass (encode) AND consult the engram store mid-pass to blend
 *       recalled neurons into the activated_concepts result (recall).
 * WHY:  Pure-bridge production today is amnesic across calls past the
 *       8-slot discourse ring. Engram integration gives multi-turn
 *       coherence ("what did we say about X earlier?") and primes the
 *       output toward consistent prior context.
 * HOW:  Borrowed pointer; not owned, not serialized. Caller (brain
 *       init) re-attaches after every load. Default OFF — pass
 *       enabled=false to detach without freeing.
 *
 * READ-ONLY mode: encode + recall blend only. Does NOT modulate
 * lexicon binding weights, does NOT push reinforcement back into
 * engram strengths beyond the engram_recall API's own internal
 * timestamps. Future modes (active reinforcement, full bidirectional)
 * are separate flags / opt-ins.
 *
 * @param gl       System handle
 * @param engram_system  engram_system_t* (opaque here)
 * @param enabled  true = active, false = pointer stored but no-op
 * @return 0 on success, -1 on NULL gl
 */
int grounded_language_set_engram_system(
    grounded_language_t* gl,
    void* engram_system,
    bool enabled);

/**
 * @brief Query whether engram integration is currently active.
 *
 * @return true if pointer is non-NULL AND enabled flag is true.
 */
bool grounded_language_engram_enabled(const grounded_language_t* gl);

/**
 * @brief Attach an LGSS context for input-gate evaluation in comprehend.
 *
 * WHAT: Wires the brain's Layered Governance Safety System (LGSS) into
 *       grounded_language_comprehend(). Once attached, every call to
 *       comprehend() builds a SAFETY_DOMAIN_GOVERNANCE action context
 *       describing the input text, evaluates it against the safety KB,
 *       and aborts with stats->lgss_inputs_blocked++ if the result is
 *       SAFETY_ACTION_DENY.
 * WHY:  brain_decide() and brain_learn_vector() already gate inference
 *       and learning through LGSS. The language pipeline is the third
 *       major user-facing surface and was previously ungated — this
 *       closes the gap so adversarial / disallowed inputs get the same
 *       defense-in-depth treatment as feature-vector inputs.
 * HOW:  Borrowed pointer. NOT owned, NOT serialized. Caller (brain
 *       init) re-attaches after every load. NULL detaches and reverts
 *       comprehend to the legacy no-gate behavior.
 *
 * @param gl    System handle (NULL → no-op)
 * @param lgss  lgss_context_t* opaque (NULL detaches)
 */
void grounded_language_set_lgss(
    grounded_language_t* gl,
    void* lgss);

/**
 * @brief Attach the brain-level immune system for Tier-3 content
 *        inspection on grounded_language_comprehend.
 *
 * WHAT: When enabled, every comprehend() pass runs a small set of
 *       cheap, read-only heuristics over the input + activations
 *       (NaN/Inf, statistical outliers, repetition spam, lexicon
 *       collisions, negation cascades). The heuristics produce a
 *       continuous inflammation level in [0..1] that:
 *         (a) damps comprehension_confidence (×(1 - 0.5*inflammation)),
 *         (b) registers an antigen via brain_immune_present_antigen
 *             when inflammation > 0.5,
 *         (c) suppresses the engram-encode hook when inflammation > 0.7
 *             (poisoned input shouldn't lay down memory traces).
 *
 * @param gl                  System handle.
 * @param brain_immune_system brain_immune_system_t* (opaque here).
 * @param enabled             true = active, false = pointer stored
 *                            but no-op.
 * @return 0 on success, -1 on NULL gl.
 */
int grounded_language_set_immune_system(grounded_language_t* gl,
                                          void* brain_immune_system,
                                          bool enabled);

/**
 * @brief Query whether Tier-3 immune content inspection is currently
 *        active.
 *
 * @return true if pointer is non-NULL AND enabled flag is true.
 */
bool grounded_language_immune_enabled(const grounded_language_t* gl);

/**
 * @brief Attach the brain-level symbolic logic engine for logical
 *        reasoning integration with language comprehension and production.
 *
 * WHAT: When enabled, language processing integrates with first-order
 *       logic inference:
 *         (a) comprehend() extracts logical facts from text (subject-
 *             verb-object triples) and adds them to the KB
 *         (b) produce() queries the KB to constrain word selection and
 *             enrich content_intent with logical features
 *         (c) forward/backward chaining derives new facts before production
 *
 * WHY:  Bridges neural grounded semantics with symbolic logic for
 *       compositional reasoning, fact checking, and structured inference.
 *
 * HOW:  - comprehend extracts predicates via POS tag analysis
 *       - produce blends logic KB features (confidence, num_facts,
 *         inference_depth) into content_intent vector alongside other
 *         cognitive modules
 *       - forward chaining derives implicit facts before generating text
 *
 * @param gl            System handle.
 * @param symbolic_logic  symbolic_logic_t* (opaque here).
 * @param enabled         true = active, false = pointer stored but no-op.
 * @return 0 on success, -1 on NULL gl.
 */
int grounded_language_set_symbolic_logic(grounded_language_t* gl,
                                          void* symbolic_logic,
                                          bool enabled);

/**
 * @brief Query whether symbolic logic integration is currently active.
 *
 * @return true if pointer is non-NULL AND enabled flag is true.
 */
bool grounded_language_logic_enabled(const grounded_language_t* gl);

/**
 * @brief Connect Broca's area adapter — every new lexicon entry is
 *        mirrored into the production lexicon so broca can articulate
 *        words that grounded_language has learned. SOLID single-source-
 *        of-truth: grounded_language owns the canonical lexicon; broca
 *        receives copies for production-side phoneme planning.
 * @param gl     System handle
 * @param broca  broca_adapter_t* (opaque)
 */
void grounded_language_connect_broca(
    grounded_language_t* gl,
    void* broca);

/**
 * @brief Connect Wernicke's area adapter — every new lexicon entry is
 *        mirrored into the comprehension lexicon so wernicke can
 *        recognize words that grounded_language has learned. Also
 *        provides the entry point for routing wernicke comprehension
 *        results back into grounded_language as auditory grounding
 *        events (see grounded_language_ingest_wernicke_result).
 * @param gl        System handle
 * @param wernicke  wernicke_adapter_t* (opaque)
 */
void grounded_language_connect_wernicke(
    grounded_language_t* gl,
    void* wernicke);

/**
 * @brief Ingest a wernicke comprehension result as auditory grounding
 *        events. For each recognized word in the result, a
 *        gl_grounding_event_t is constructed with modality=AUDITORY,
 *        attention=word.confidence, and pushed through the standard
 *        grounding pipeline. Closes the audio→language loop.
 * @param gl              System handle
 * @param comp_result     wernicke_comprehension_t* (opaque cast inside)
 * @param audio_features  Sensory feature vector that produced the
 *                        comprehension (modality=audio); NULL skips.
 * @param feature_dim     Length of audio_features
 * @return Number of grounding events successfully recorded, or -1.
 */
int grounded_language_ingest_wernicke_result(
    grounded_language_t* gl,
    const void* comp_result,
    const float* audio_features,
    uint32_t feature_dim);

/**
 * @brief Sleep-state consolidation pass over the lexicon.
 *
 * WHAT: Apply consolidation effects matching the current sleep stage.
 *       NREM stages strengthen frequent / high-confidence bindings and
 *       decay rarely-fired ones (memory replay + forgetting curve).
 *       REM does light associative spreading on the recent context
 *       buffer (creative recombination).
 * WHY:  Without this, the lexicon never participates in sleep
 *       consolidation — vocabulary growth is wake-only and decay is
 *       absent, so noise accumulates and the trained lexicon drifts
 *       from biological plausibility.
 *
 * Stage effects:
 *   - SLEEP_STATE_AWAKE / DROWSY: no-op
 *   - SLEEP_STATE_LIGHT_NREM (strength≈0.3): mild decay of unused bindings
 *   - SLEEP_STATE_DEEP_NREM  (strength≈0.8): strong reinforcement of
 *     frequent bindings + decay of stale ones
 *   - SLEEP_STATE_REM        (strength≈0.5): mild spreading via
 *     context vectors (no decay)
 *
 * @param gl              System handle (NULL → no-op)
 * @param sleep_state_int sleep_state_t value (kept as int to keep
 *                        nimcp_sleep_wake.h out of the public include
 *                        surface; impl casts internally)
 * @param strength        Consolidation magnitude [0, 1]
 * @return 0 on success, -1 on bad parameters.
 */
int grounded_language_sleep_consolidate(
    grounded_language_t* gl,
    int sleep_state_int,
    float strength);

/**
 * @brief Modulation taps from the connected cortexes — used to bias
 *        grounding strength and comprehension confidence in real time.
 *        All values are 0 when the corresponding cortex isn't attached
 *        or its scalar state isn't readable (e.g. visual_cortex has no
 *        scalar attention tap, only stats counters — left at 0).
 */
typedef struct {
    float visual_activity;    /**< Visual cortex activity proxy [0, 1] */
    float audio_salience;     /**< Audio cortex speech-salience tap [0, 1] */
    float speech_confidence;  /**< Speech cortex phoneme confidence [0, 1] */
} gl_cortex_modulation_t;

/**
 * @brief Read current cortex modulation values into out.
 *
 * Cheap (~3 scalar reads). Called from grounded_language_ground() and
 * grounded_language_comprehend() to bias strength/confidence by which
 * sensory cortexes are currently active. Zero-fills out on NULL gl
 * or NULL out.
 *
 * @param gl   System handle
 * @param out  Caller-supplied modulation struct (zero-filled on entry)
 * @return 0 on success, -1 on bad parameters.
 */
int grounded_language_get_cortex_modulation(
    grounded_language_t* gl,
    gl_cortex_modulation_t* out);

/*=============================================================================
 * Dialect / accent conditioning (#14)
 *
 * The lexicon is stored language-agnostic by design — words from any
 * locale can coexist. Dialect tagging lets callers mark the *current*
 * input/output context (e.g., "en-US", "fr-CA", "zh-CN") so that
 * fuzzy lookup, production templates, and SNN bridge prefer entries
 * that were learned under that dialect when ranking ties. Default is
 * the empty string — dialect-agnostic mode. Both setter and getter
 * are NULL-safe; setter truncates to GL_MAX_DIALECT_LEN-1 chars.
 *===========================================================================*/

/** Set the current dialect context. NULL or "" clears the tag. */
void grounded_language_set_dialect(grounded_language_t* gl,
                                     const char* dialect);

/** Read the current dialect tag (NUL-terminated). Returns "" when
 *  unset or gl is NULL. The pointer is owned by GL — copy if you
 *  need to retain it past a subsequent set call. */
const char* grounded_language_get_dialect(const grounded_language_t* gl);

/*=============================================================================
 * NLP Frontend (Morphology / Embeddings / Subword Tokenizer)
 *
 * Three-stage lookup chain for unknown words in comprehend:
 *   1. Exact lexicon hit                 (fast, O(1))
 *   2. Morphological normalization       (suffix-strip, then exact retry)
 *   3. Fuzzy match                       (phonological + character-set)
 *   4. BPE subword fallback              (only when tokenizer is connected)
 * Stages 2+4 + the embedding blend below are no-ops when the relevant
 * subsystems aren't connected — the legacy behavior is preserved.
 *===========================================================================*/

/**
 * @brief Strip common English inflectional suffixes to produce a
 *        normalized lookup form (a lightweight stemmer — not full
 *        lemmatization, but enough to make running→run, cats→cat,
 *        quickly→quick land on the same lexicon entry).
 *
 * @param word    Input word (NUL-terminated, ASCII-lowercased recommended)
 * @param out     Caller-supplied buffer for stripped form
 * @param out_sz  Size of out buffer (>= GL_MAX_WORD_LEN recommended)
 * @return Number of suffix chars stripped (0 if unchanged), -1 on error.
 */
int gl_morph_normalize(const char* word, char* out, size_t out_sz);

/**
 * @brief Suffix-based POS hint. Returns a gl_word_class_t guess from
 *        morphological cues alone (-ing/-ed→VERB, -ly→ADVERB,
 *        -tion/-ness/-ment→NOUN, -ous/-ive/-ful/-able→ADJECTIVE).
 *        Returns GL_CLASS_UNKNOWN when no suffix matches.
 *
 *        Used to bootstrap word-class inference for never-seen-in-context
 *        words. Combined with the existing positional heuristic in
 *        learn_from_text — morphology takes priority when confidence is
 *        higher.
 */
gl_word_class_t gl_morph_pos_hint(const char* word);

/**
 * @brief Inflect a base verb to its 3rd-person-singular present form
 *        (Tier 1 Step F3). run→runs, watch→watches, try→tries, go→goes,
 *        have→has, be→is. The inverse of gl_morph_normalize's stripping.
 * @return Length written (>0), or -1 on error/insufficient buffer.
 */
int gl_morph_inflect_3sg(const char* verb, char* out, size_t out_sz);

/**
 * @brief Pluralize a singular noun (Tier 1 Step F3). cat→cats,
 *        box→boxes, baby→babies, child→children, sheep→sheep.
 * @return Length written (>0), or -1 on error/insufficient buffer.
 */
int gl_morph_pluralize(const char* noun, char* out, size_t out_sz);

/**
 * @brief Past-tense inflector (Tier 1 Step F4b). love→loved, try→tried,
 *        go→went, is→was. Irregular table + regular -ed orthography.
 * @return Length written (>0), or -1 on error/insufficient buffer.
 */
int gl_morph_past_tense(const char* verb, char* out, size_t out_sz);

/**
 * @brief Choose "a" vs "an" for the word that immediately follows the
 *        article (Tier 1 Step F4a). Agrees with SOUND not spelling:
 *        "an hour", "a university". Returns a static string; never NULL.
 */
const char* gl_article_for(const char* word);

/**
 * @brief Fix subject-verb and quantifier-noun agreement in a generated
 *        utterance (Tier 1 Step F3). Conservative, closed-class-driven:
 *        keys off reliably identifiable signals — subject pronouns
 *        (he/she/it ⇒ 3sg) and plural quantifiers (two/many/… ⇒ plural
 *        noun) plus position (first content word = subject; the following
 *        content word = verb) — and inflects the adjacent word via the
 *        helpers above. Skips words confidently classified as nouns when
 *        looking for the verb slot, and never double-inflects (-s/-ed/-ing
 *        endings are left alone). gl is used only for the POS guard.
 *
 * @param gl      System handle (for POS lookups; may be NULL → position-only)
 * @param in      Input utterance (NUL-terminated)
 * @param out     Caller buffer for the corrected utterance
 * @param out_sz  Size of out
 * @return Number of words inflected (>=0), or -1 on error. On success `out`
 *         holds the corrected text (== `in` when nothing changed).
 */
int gl_apply_svo_agreement(grounded_language_t* gl, const char* in,
                           char* out, size_t out_sz);

/**
 * @brief Tier 1 Step F4 — production-fluency surface polish. Runs three
 *        conservative, closed-class-driven correctors over a generated
 *        utterance, in order, sharing one tokenization:
 *          F4a  article selection — phonetic a/an agreement + indefinite
 *               article on a first-mention singular count noun ("a tree",
 *               "an apple"); definite "the" on a re-mention.
 *          F4b  tense/aspect consistency — when the first finite verb is
 *               past, coerce following present/base verbs to past so the
 *               clause doesn't mix tenses ("sat and ran", not "sat and runs").
 *          F4c  pronoun case + possessives — subject vs object pronoun form
 *               by slot (me→I as subject, he→him as object), possessive
 *               determiner before a noun (he→his), and noun-noun possessive
 *               's ("dog's tail").
 *        Each pass no-ops gracefully when its signal is absent and is gated
 *        to developmental stage >= 2 (matching Step C/F2). gl supplies POS
 *        class lookups; NULL falls back to morphology hints + position.
 * @return Number of surface edits applied (>=0), or -1 on error. On success
 *         `out` holds the corrected text (== `in` when nothing changed).
 */
int gl_apply_f4_fluency(grounded_language_t* gl, const char* in,
                        char* out, size_t out_sz);

/**
 * @brief Enable/disable gl-side autoregressive production (Tier 1 follow-up).
 *        When ON, grounded_language_produce evolves a running "emitted
 *        context" vector and adds a cos(emitted_ctx, candidate) continuation
 *        bonus to each candidate's score — left-to-right conditioning instead
 *        of the fixed-intent within-top-K rerank. Default OFF (byte-identical
 *        to the rerank path; position 0 is always identical regardless).
 */
void grounded_language_set_autoregressive_produce(grounded_language_t* gl,
                                                  bool enabled);
bool grounded_language_get_autoregressive_produce(const grounded_language_t* gl);

/**
 * @brief Enable/disable produce-side pronominalization (Tier 2 anaphora).
 *        Default OFF. When ON, gl_apply_pronominalization replaces a
 *        re-mentioned non-person noun (within a recency window, across a
 *        verb) with it/they (subject) or it/them (object) so replies don't
 *        repeat the noun. Person nouns are guarded out.
 */
void grounded_language_set_produce_pronominalize(grounded_language_t* gl,
                                                 bool enabled);
bool grounded_language_get_produce_pronominalize(const grounded_language_t* gl);

/**
 * @brief Enable/disable discourse-seeded autoregressive produce (Tier 2
 *        cross-turn coherence). Default OFF. Only has effect when
 *        autoregressive_produce is also ON and stage >= 2: grounded_language_
 *        produce primes its emitted-context vector with a unit-normalized,
 *        decayed copy of the most-recent discourse-turn vector so the opening
 *        words are pulled toward referential continuity with what was just
 *        said. The seed fades automatically as emitted words accumulate.
 *        When OFF (or AR off), produce is byte-identical to the pre-seed path.
 */
void grounded_language_set_produce_discourse_seed(grounded_language_t* gl,
                                                  bool enabled);
bool grounded_language_get_produce_discourse_seed(const grounded_language_t* gl);

/**
 * @brief Enable/disable the SVO clause/argument frame in produce (FND-1).
 *        Default OFF. When ON and stage >= 2, grounded_language_produce builds
 *        a canonical "the SUBJECT VERB the OBJECT" clause from the grounded
 *        top-K (best verb = predicate, best two nouns = agent/patient) instead
 *        of a bigram/POS-reranked bag of content words. Falls back to the
 *        greedy emit when there's no head verb / no noun, or the head verb's
 *        grounded score is below the developmental confidence floor (never
 *        worse than baseline). Word order only is templated; slots are filled
 *        by the same grounded score the greedy path uses.
 */
void grounded_language_set_produce_clause_frame(grounded_language_t* gl,
                                                bool enabled);
bool grounded_language_get_produce_clause_frame(const grounded_language_t* gl);

/** Increment-1 (2026-06-02): SNN-as-generator A/B switch. When ON, produce
 *  sources candidate words from the SNN bridge's per-tick Broca spike cache,
 *  falling back to the lexicon producer when the SNN yields no signal. Default
 *  OFF (produce byte-identical to the lexicon path). Runtime-only. */
void grounded_language_set_produce_via_snn(grounded_language_t* gl, bool enabled);
bool grounded_language_get_produce_via_snn(const grounded_language_t* gl);

/** SNN generation dynamics (2026-06-10): how many SNN steps to run and at what
 *  Wernicke inject current when produce_via_snn sources candidates from Broca.
 *  Selectivity-sweep optimum is n_steps=16/inject=40. n_steps=0 / inject<=0 are
 *  ignored (keep current). */
void grounded_language_set_snn_gen_params(grounded_language_t* gl,
                                          uint32_t n_steps, float inject,
                                          uint32_t max_concepts);
void grounded_language_get_snn_gen_params(const grounded_language_t* gl,
                                          uint32_t* n_steps, float* inject,
                                          uint32_t* max_concepts);

/** Metacognitive produce-floor modulation (2026-06-02). The flag (config,
 *  default OFF) enables it; the cascade sets the adjust per-produce from
 *  world-model surprise / wellbeing distress / introspection confidence.
 *  adjust>0 raises the produce confidence floor (more "I don't know"),
 *  adjust<0 lowers it. Clamped [-1,1]; floor re-clamped [0,1] at use. */
void grounded_language_set_metacog_gates_produce(grounded_language_t* gl, bool enabled);
bool grounded_language_get_metacog_gates_produce(const grounded_language_t* gl);
void grounded_language_set_metacog_floor_adjust(grounded_language_t* gl, float adjust);
float grounded_language_get_metacog_floor_adjust(const grounded_language_t* gl);

/**
 * @brief Tier 2 produce-side pronominalization pass. Conservative,
 *        closed-class + position driven; person nouns skipped. Runs after F4
 *        in the cascade. gl supplies POS class lookups (NULL → morphology
 *        hint + position). Stage-gated >= 2.
 * @return Number of nouns pronominalized (>=0), or -1 on error.
 */
int gl_apply_pronominalization(grounded_language_t* gl, const char* in,
                               char* out, size_t out_sz);

/**
 * @brief Enable/disable givenness-driven definiteness (T3-1). Default OFF.
 *        When ON and stage >= 2, gl_apply_givenness_definite runs after T2
 *        pronominalization in the cascade and swaps the preceding "a"/"an"
 *        to "the" on re-mentioned NOUNs that were NOT pronominalized (person
 *        nouns, unanchored slots), marking the second mention as given.
 */
void grounded_language_set_produce_givenness_definite(grounded_language_t* gl,
                                                      bool enabled);
bool grounded_language_get_produce_givenness_definite(const grounded_language_t* gl);

/**
 * @brief T3-1 givenness-definiteness pass. For each NOUN at position i whose
 *        lemma was already seen within the recency window (and which is NOT
 *        already a pronoun), if token i-1 is the indefinite article "a" or
 *        "an", replace it with "the" (case-preserving). Stage-gated >= 2.
 *        Idempotent: re-running on the polished output is a no-op.
 * @return Number of articles changed (>=0), or -1 on error.
 */
int gl_apply_givenness_definite(grounded_language_t* gl, const char* in,
                                char* out, size_t out_sz);

/**
 * @brief T3-2 (2026-05-29) — Cohesive conjunction insertion between adjacent
 *        SVO clauses. Toggle via produce_t3_conjunction. Default false.
 *        When ON, gl_apply_t3_conjunction scans the polished utterance for an
 *        adjacent SVO pair without an existing connective and inserts "and"
 *        before the second clause's leading determiner / noun. One insert
 *        per pass — re-running on the polished output finds the connective
 *        already present and is a no-op.
 */
void grounded_language_set_produce_t3_conjunction(grounded_language_t* gl,
                                                  bool enabled);
bool grounded_language_get_produce_t3_conjunction(const grounded_language_t* gl);

/**
 * @brief T3-2 conjunction-insertion pass. Walks the token stream, classifies
 *        each token via gl_f4_class, and finds the FIRST adjacent SVO pair:
 *        a VERB token, followed (possibly across an object NOUN) by a DET or
 *        NOUN that is itself followed later by a VERB. Inserts "and" before
 *        that DET/NOUN. Skips when the candidate token is already one of
 *        {and,but,so,or,then,while,because,although}. Stage-gated >= 2.
 *        Returns the number of conjunctions inserted (>=0; capped at 1 per
 *        call), or -1 on error.
 */
int gl_apply_t3_conjunction(grounded_language_t* gl, const char* in,
                            char* out, size_t out_sz);

/* TF-2 (2026-05-28) — Tier-feedback plasticity flags.
 *
 * Master switch + per-corrector enable mask + per-path learning-rate clamps.
 * See nimcp_grounded_language_internal.h for the field layout. All five
 * persist in the LANC config block; the JSON default file mirrors them so
 * a daemon restart honors operator intent. */

/* Master switch. Default false — TF-1 still captures + counts deltas, but no
 * plasticity fires until this is true AND tf_enabled_correctors has bits set
 * AND the corresponding lr is > 0. */
void grounded_language_set_produce_corrector_feedback_enabled(grounded_language_t* gl,
                                                              bool enabled);
bool grounded_language_get_produce_corrector_feedback_enabled(const grounded_language_t* gl);

/* Per-corrector enable mask — bit-OR of TF source bits. Bit assignments:
 *   bit 0 = F3 agreement, bit 1 = F4 fluency, bit 2 = T2 pronominalize,
 *   bit 3 = T3-1 givenness, bit 4 = T3-2 conjunction (when shipped). */
#define GL_TF_BIT_F3_AGREEMENT      (1u << 0)
#define GL_TF_BIT_F4_FLUENCY        (1u << 1)
#define GL_TF_BIT_T2_PRONOMINALIZE  (1u << 2)
#define GL_TF_BIT_T3_GIVENNESS      (1u << 3)
#define GL_TF_BIT_T3_CONJUNCTION    (1u << 4)
#define GL_TF_BIT_ALL_CORRECTORS    0x001Fu   /* low 5 bits */
void     grounded_language_set_tf_enabled_correctors(grounded_language_t* gl,
                                                     uint16_t mask);
uint16_t grounded_language_get_tf_enabled_correctors(const grounded_language_t* gl);

/* Per-path learning-rate clamps. Setter clamps NaN/neg to 0 and caps at the
 * per-path maximum. Trigram + distributional default ON-ish (small +lr) so
 * once master + corrector-bit flips on, plasticity is immediate. Bridge STDP
 * defaults to 0.0 so the master flag alone DOES NOT mutate SNN weights —
 * TF-5 walkthrough required before raising. */
#define GL_TF_LR_TRIGRAM_MAX       0.05f
#define GL_TF_LR_DISTRIB_MAX       0.1f
#define GL_TF_LR_BRIDGE_STDP_MAX   0.1f
#define GL_TF_LR_TRIGRAM_DEFAULT      0.002f
#define GL_TF_LR_DISTRIB_DEFAULT      0.01f
#define GL_TF_LR_BRIDGE_STDP_DEFAULT  0.0f
void  grounded_language_set_tf_lr_trigram(grounded_language_t* gl, float lr);
float grounded_language_get_tf_lr_trigram(const grounded_language_t* gl);
void  grounded_language_set_tf_lr_distrib(grounded_language_t* gl, float lr);
float grounded_language_get_tf_lr_distrib(const grounded_language_t* gl);
void  grounded_language_set_tf_lr_bridge_stdp(grounded_language_t* gl, float lr);
float grounded_language_get_tf_lr_bridge_stdp(const grounded_language_t* gl);

/**
 * @brief Word-form → integer-token-id callback used by the embedding
 *        bridge. The caller knows their tokenizer's vocab; GL just asks
 *        "what id is this word?" Return 0 for unknown.
 */
typedef uint32_t (*gl_word_to_id_fn)(void* ctx, const char* word);

/**
 * @brief Connect a word-embedding layer + lookup callback. When wired,
 *        grounded_language_comprehend() blends the per-word embedding
 *        into the result's semantic_vector (subject to dim match).
 *
 *        Pass NULL for emb to disconnect. emb_dim must equal GL's
 *        semantic_dim or the embedding contribution is skipped per word.
 *
 * @param gl              System handle
 * @param emb             embedding_layer_t* (opaque)
 * @param emb_dim         Embedding output dimension
 * @param word_to_id_fn   Callback to map word → token_id (caller-owned)
 * @param ctx             Opaque ctx forwarded to word_to_id_fn
 */
void grounded_language_connect_embeddings(
    grounded_language_t* gl,
    void* emb,
    uint32_t emb_dim,
    gl_word_to_id_fn word_to_id_fn,
    void* ctx);

/**
 * @brief Connect a BPE tokenizer for subword fallback on totally OOV
 *        words. When wired, comprehend's last-resort path decomposes
 *        unrecognized words into subword token ids and pulls embeddings
 *        for those (when the embedding layer is also connected).
 *
 * @param gl   System handle
 * @param tok  tokenizer_t* (opaque), NULL to disconnect
 */
void grounded_language_connect_tokenizer(
    grounded_language_t* gl,
    void* tok);

/*=============================================================================
 * Named Entity Recognition + Shallow Chunking
 *
 * NER:       per-word classifier driven by capitalization / numeric form /
 *            sentence-position cues. Pure function — no lexicon mutation.
 * Chunking:  groups consecutive words into NP/VP/PP/ADJP/ADVP based on the
 *            POS sequence (lexicon-known classes + morph hints + NER).
 * Chinking:  hard breaks inside chunks at commas / conjunctions / verb
 *            tokens that interrupt an otherwise contiguous NP.
 *===========================================================================*/

/**
 * @brief Entity types produced by gl_ner_classify(). Coarse-grained on
 *        purpose — finer-grained NER (date subtypes, ORG vs LOC) is a
 *        downstream KG concern.
 */
typedef enum {
    GL_ENTITY_NONE   = 0,
    GL_ENTITY_PERSON,    /**< Capitalized, alphabetic, mid-sentence */
    GL_ENTITY_PLACE,     /**< Caps + known place suffix (-ville, -burg) */
    GL_ENTITY_ORG,       /**< All-caps acronym, or caps + corp suffix */
    GL_ENTITY_NUMBER,    /**< Pure numeric tokens */
    GL_ENTITY_DATE,      /**< 4-digit year, or numeric+slash forms */
    GL_ENTITY_OTHER,     /**< Capitalized but not classifiable */
} gl_entity_type_t;

/**
 * @brief Classify a single token as an entity (or not).
 *
 * @param word               Input token (caller already split text)
 * @param prev_word_or_null  Preceding token, or NULL at sentence start
 * @param is_sentence_start  True if this is the first token of a sentence
 *                           (sentence-initial caps don't imply entity)
 */
gl_entity_type_t gl_ner_classify(const char* word,
                                  const char* prev_word_or_null,
                                  bool is_sentence_start);

/**
 * @brief Chunk type produced by the shallow chunker.
 */
typedef enum {
    GL_CHUNK_NONE = 0,
    GL_CHUNK_NP,         /**< Noun phrase: (DT|PRON)? ADJ* (NOUN|ENTITY)+ */
    GL_CHUNK_VP,         /**< Verb phrase: (AUX)? VERB ADV* */
    GL_CHUNK_PP,         /**< Prepositional phrase: prep + NP */
    GL_CHUNK_ADJP,       /**< Adjective phrase: ADV? ADJ */
    GL_CHUNK_ADVP,       /**< Adverb phrase: ADV+ */
} gl_chunk_type_t;

/** Maximum words per chunk reported by the API. */
#define GL_CHUNK_MAX_WORDS 12

/**
 * @brief Single chunk emitted by grounded_language_chunk(). Index into
 *        the caller's tokenized input is via [start_word, end_word).
 */
typedef struct {
    gl_chunk_type_t  type;
    uint32_t         start_word;       /**< First word index, inclusive */
    uint32_t         end_word;         /**< Last word index, exclusive */
    char             head_word[32];    /**< Head form (NOUN for NP, etc.) */
    gl_entity_type_t head_entity;      /**< Entity tag on the head, if any */
    bool             chinked;          /**< True if a chink rule split this */
} gl_chunk_t;

/**
 * @brief Run the shallow chunker over a sentence. Tokenizes the input,
 *        classifies each token (lexicon → morph hint → NER fallback),
 *        then greedily groups tokens into NP/VP/PP/ADJP/ADVP using
 *        regex-style POS patterns. Chinking rules break NPs at commas,
 *        conjunctions, and unrelated verbs.
 *
 * @param gl              System handle
 * @param text            Input sentence (caller-owned NUL-terminated)
 * @param chunks_out      Caller-supplied buffer
 * @param max_chunks      Capacity of chunks_out
 * @param chunk_count_out [OUT] Number of chunks written
 * @return 0 on success, -1 on error.
 */
int grounded_language_chunk(grounded_language_t* gl,
                             const char* text,
                             gl_chunk_t* chunks_out,
                             uint32_t max_chunks,
                             uint32_t* chunk_count_out);

/*=============================================================================
 * Per-Network Bridges (LNN / cortex-CNN / FNO / ANN)
 *
 * grounded_language already feeds into SNN via nimcp_snn_language_bridge.
 * These bridges close the gap with the other networks: the comprehend
 * semantic_vector gets broadcast to each connected network's forward
 * pass, and per-network response magnitudes feed back as modulation
 * factors on confidence + binding strength.
 *
 * Each attach function takes an opaque pointer; broadcast/modulation
 * are no-ops when no network is wired (the legacy single-tier path
 * is preserved). Networks can be wired and unwired independently —
 * passing NULL to any attach function is the disconnect operation.
 *===========================================================================*/

/**
 * @brief Per-network response magnitudes from the most recent
 *        broadcast. Each scalar in [0, 1]. Zero when the corresponding
 *        network isn't attached or its forward pass produced no
 *        meaningful output.
 */
typedef struct {
    float lnn_magnitude;      /**< L2 norm of LNN forward output (normalized) */
    float cnn_magnitude;      /**< Cortex-CNN feature norm (normalized) */
    float fno_magnitude;      /**< FNO embedding norm (normalized) */
    float ann_magnitude;      /**< ANN/adaptive predictor norm (reserved) */
} gl_network_modulation_t;

/** Attach an LNN layer. lnn_layer_t* (opaque). NULL = disconnect. */
void grounded_language_attach_lnn(grounded_language_t* gl, void* lnn_layer);

/** Attach a cortex-CNN processor (uses the speech variant for 1D float input). */
void grounded_language_attach_cortex_cnn(grounded_language_t* gl, void* cnn_proc);

/** Attach an FNO audio processor (uses fno_audio_forward for 1D float input). */
void grounded_language_attach_fno(grounded_language_t* gl, void* fno_proc);

/** Attach a generic ANN predictor — reserved hook (uses callback). */
typedef int (*gl_ann_predict_fn)(void* ctx, const float* in, uint32_t in_dim,
                                   float* out, uint32_t out_dim);
void grounded_language_attach_ann(grounded_language_t* gl,
                                   gl_ann_predict_fn fn,
                                   void* ctx);

/**
 * @brief Broadcast a semantic vector to every attached network. Runs
 *        each forward pass with the vector as input, captures response
 *        magnitudes into the GL's last-modulation cache. Returns the
 *        number of networks that successfully responded.
 *
 * @param gl   System handle
 * @param vec  Semantic vector (length = gl->semantic_dim)
 * @param dim  Dimension of vec
 * @return Count of networks that produced a valid response, -1 on error.
 */
int grounded_language_broadcast_to_networks(grounded_language_t* gl,
                                              const float* vec,
                                              uint32_t dim);

/**
 * @brief Read the last-broadcast modulation values. Cheap scalar read.
 *        Used internally by comprehend to bias confidence.
 *
 * @return 0 on success, -1 on bad parameters. Out is zero-filled on entry.
 */
int grounded_language_get_network_modulation(grounded_language_t* gl,
                                               gl_network_modulation_t* out);

/*=============================================================================
 * Cognitive subscriber bus
 *
 * GL fires lightweight events at four points (new word created, grounding
 * succeeded, comprehension finished, production finished). Cognitive
 * modules subscribe a callback to receive these events and react in
 * their own terms — inner speech may rehearse the word, imagination
 * may register the concept, theory-of-mind may treat the utterance as
 * an observation, etc.
 *
 * Pull direction (cognitive module → GL) uses the existing public APIs
 * (grounded_language_lookup, _produce, _comprehend, _ground). The bus
 * here covers the push direction without GL needing to know each
 * module's internals. Up to 16 subscribers; out-of-order unsubscribe
 * is supported via the ctx pointer.
 *===========================================================================*/

typedef enum {
    GL_EVENT_NEW_WORD     = 0,  /**< A new lexicon entry was just created */
    GL_EVENT_GROUNDED,           /**< grounded_language_ground succeeded */
    GL_EVENT_COMPREHENDED,       /**< grounded_language_comprehend finished */
    GL_EVENT_PRODUCED,           /**< grounded_language_produce finished */
    /* #10 Active-learning curriculum signal. Fired when comprehend hits
     * a word with confidence below GL_LOW_CONFIDENCE_THRESHOLD —
     * downstream curriculum modules can register their interest in
     * grounding that word the next time it shows up in sensory input. */
    GL_EVENT_NEEDS_GROUNDING,
    /* TB-9 follow-up — fired by comprehend after a non-UNKNOWN speech-act
     * label is computed (only when classification is enabled). The event's
     * `speech_act` field carries the classification. Subscribers can react
     * (e.g., switch to question-answering mode on QUESTION). */
    GL_EVENT_SPEECH_ACT,
    /* TB-10 follow-up — fired by comprehend when the topic-shift detector
     * triggers (only when detection is enabled). The event's
     * `topic_similarity` field is the cosine value that crossed the
     * threshold. Natural consumer: episodic memory consolidation. */
    GL_EVENT_TOPIC_SHIFT,
    /* NLP-2 — fired when a definite-NP head matches a recent prior
     * mention. event->word is the noun head, event->text is the surface
     * form of the input that triggered, event->topic_similarity carries
     * a [0,1] score (1.0 == exact same surface, hooks for future
     * semantic-vec fuzzy matching can use intermediate values). */
    GL_EVENT_COREF_RESOLVED,
    /* Cascade Stage 9 — fired when the communication cascade writes its
     * just-produced utterance back to working memory. Lets cognitive
     * subscribers (inner-speech, episodic memory, ToM "self model") observe
     * the brain's own output as a first-class event distinct from
     * GL_EVENT_PRODUCED, which fires inside grounded_language_produce
     * regardless of whether the output reached the cascade tail. */
    GL_EVENT_SELF_PRODUCED,
} gl_event_type_t;

/**
 * @brief Event payload pushed to subscribers. Pointer fields are
 *        owned by GL and valid only for the duration of the callback.
 *        Subscribers must copy any data they want to retain.
 */
typedef struct {
    gl_event_type_t  type;
    const char*      word;          /**< NEW_WORD/GROUNDED: the word; else NULL */
    const char*      text;          /**< COMPREHENDED/PRODUCED: full text; else NULL */
    const float*     semantic_vec;  /**< COMPREHENDED/PRODUCED: [gl->semantic_dim] */
    uint64_t         concept_id;    /**< GROUNDED: bound concept; else 0 */
    float            valence;       /**< Emotional valence at event time */
    float            arousal;       /**< Emotional arousal */
    float            confidence;    /**< [0,1]; comprehension/production confidence */
    /* Audit-2 follow-up — speech-act + topic-shift consumer surface. */
    gl_speech_act_t  speech_act;        /**< SPEECH_ACT: classifier label, else UNKNOWN */
    float            topic_similarity;  /**< TOPIC_SHIFT: cosine sim that crossed threshold */
} gl_event_t;

/** Subscriber callback. Return 0 to continue, non-zero to log+continue. */
typedef int (*gl_event_callback_t)(void* ctx, const gl_event_t* event);

/**
 * @brief Fire an event to all subscribers (filtered by type mask).
 *
 *  Internal entry-point invoked from various GL pipelines (learn,
 *  ground, comprehend, produce, etc.). Re-entry guarded: a subscriber
 *  that re-enters fire becomes a no-op. Safe to call with gl==NULL or
 *  ev==NULL (no-op).
 *
 *  Defined in `nimcp_grounded_language_cognitive_bridge.c`. Declared
 *  here so callers (cascade, GL pipelines) don't each inline an
 *  `extern void` decl — those duplicates risked signature drift.
 */
void gl_fire_event(grounded_language_t* gl, const gl_event_t* ev);

/* Event-type bitmask (#4 — per-subscriber filter). Use these to limit
 * which events a subscriber receives — the bus skips wrappers whose
 * mask doesn't match, removing the if-else boilerplate from every
 * wrapper and cutting bus-walk cost from O(N) to ~O(N_relevant). */
#define GL_EVENT_MASK_NEW_WORD          (1u << 0)
#define GL_EVENT_MASK_GROUNDED          (1u << 1)
#define GL_EVENT_MASK_COMPREHENDED      (1u << 2)
#define GL_EVENT_MASK_PRODUCED          (1u << 3)
#define GL_EVENT_MASK_NEEDS_GROUNDING   (1u << 4)
#define GL_EVENT_MASK_SPEECH_ACT        (1u << 5)
#define GL_EVENT_MASK_TOPIC_SHIFT       (1u << 6)
#define GL_EVENT_MASK_COREF_RESOLVED    (1u << 7)
#define GL_EVENT_MASK_SELF_PRODUCED     (1u << 8)
#define GL_EVENT_MASK_ALL               (0xFFFFFFFFu)

/** Confidence floor below which comprehend fires a NEEDS_GROUNDING
 *  event for the lowest-confidence word it just processed (#10). */
#define GL_LOW_CONFIDENCE_THRESHOLD     0.20f

/**
 * @brief Subscribe to GL events. Returns 0 on success, -1 if the
 *        subscriber table is full or arguments invalid.
 *
 *  Equivalent to subscribe_ex(gl, fn, ctx, GL_EVENT_MASK_ALL, 0).
 *
 * @param gl   System handle
 * @param fn   Callback (must be non-NULL)
 * @param ctx  Opaque ctx forwarded on every callback. Used as the
 *             dedup key — registering the same ctx twice replaces
 *             the prior callback.
 */
int grounded_language_subscribe(grounded_language_t* gl,
                                  gl_event_callback_t fn,
                                  void* ctx);

/**
 * @brief Subscribe with extended options (#3 priority + #4 type mask).
 *
 *  - type_mask: bitwise-OR of GL_EVENT_MASK_* constants. The bus skips
 *    callbacks whose mask doesn't match the firing event's type.
 *  - priority: higher fires first. Use 0 for default. Use a higher
 *    value for the hemispheric-bridge / lateralization callback so it
 *    can mute downstream observers before they react. Range [-127,127].
 *
 *  Behaviorally equivalent to subscribe() when type_mask=GL_EVENT_MASK_ALL
 *  and priority=0; both are stable identity for legacy callers.
 *
 * @return 0 on success, -1 on invalid args or table full.
 */
int grounded_language_subscribe_ex(grounded_language_t* gl,
                                     gl_event_callback_t fn,
                                     void* ctx,
                                     uint32_t type_mask,
                                     int8_t priority);

/** Unsubscribe by ctx pointer. Returns 0 if removed, -1 if not found. */
int grounded_language_unsubscribe(grounded_language_t* gl, void* ctx);

/** How many subscribers are currently registered. */
uint32_t grounded_language_subscriber_count(const grounded_language_t* gl);

/**
 * @brief Test whether a word currently has a lexicon entry (#1).
 *        Lower-cases on entry. Read-only — never creates a binding.
 *        Used by region adapters (broca/wernicke) to fall through to
 *        GL when their local lexicons miss without paying the cost
 *        of a full comprehend.
 * @return true if the word is in the lexicon, false otherwise (also
 *         false for NULL or empty input).
 */
bool grounded_language_has_word(const grounded_language_t* gl, const char* word);

/*=============================================================================
 * SNN spike → lexicon decoding (#12)
 *
 * Push a per-population spike-rate vector into the GL and observe
 * which lexicon entries best match it. The spike vector is treated
 * as a semantic-space projection (broca/wernicke language pops
 * already produce activity patterns the GL can read); we project
 * those rates against each lexicon entry's context_vector and emit a
 * COMPREHENDED event for the top match if it crosses a similarity
 * threshold.
 *
 * Closes the brain-internal language loop: SNN spike activity →
 * matched word(s) → bus event → cognitive consumers (inner_speech,
 * narrative, theory_of_mind, etc.).
 *
 * Returns the number of words that fired COMPREHENDED events
 * (0 or 1 in practice — top-1 only). -1 on bad input.
 *===========================================================================*/

/** Cosine-similarity floor for SNN spike → lexicon match. Below this,
 *  the projection is too weak to call a "comprehension". */
#define GL_SNN_SPIKE_MATCH_THRESHOLD     0.5f

/**
 * @brief Observe SNN spike-rate activity and (when a match exceeds
 *        the similarity threshold) emit COMPREHENDED for the
 *        best-matching lexicon entry.
 *
 * @param gl                System handle
 * @param spike_rates       Per-population rate vector (Hz or
 *                          normalized; only relative magnitudes
 *                          matter — they're cosine-similarity-
 *                          compared against context vectors)
 * @param rate_dim          Length of spike_rates; must equal
 *                          gl's semantic_dim or the call is no-op
 * @param confidence_out    Optional: receives the cosine similarity
 *                          of the matched word (NULL OK)
 * @return 1 if a word matched and event was fired, 0 if no match
 *         exceeded the threshold, -1 on bad input.
 */
int gl_observe_snn_spikes(grounded_language_t* gl,
                            const float* spike_rates,
                            uint32_t rate_dim,
                            float* confidence_out);

/*=============================================================================
 * Audio-feature extraction (#2)
 *
 * Helper for callers that have raw audio samples but no pre-computed
 * feature vector. Computes a simple time-domain RMS envelope: divides
 * `samples` into `feature_dim` equal-length chunks and writes the
 * normalized RMS of each chunk into out_features. The result is a
 * coarse-but-deterministic perceptual envelope suitable for grounding
 * — distinct utterances produce distinct vectors without requiring
 * an FFT or MFCC pipeline.
 *
 * Use case: the auditory grounding loop. Wernicke produces words from
 * raw audio; the caller wants to ground those words in the audio that
 * produced them. Rather than maintaining a parallel feature pipeline,
 * call this once per utterance and reuse the result for every word.
 *
 * Returns 0 on success, -1 on bad input. Output is in [0, 1] range
 * after normalization by the peak chunk-RMS across the utterance
 * (NOT the global RMS): silent chunks → 0, the loudest chunk → 1.0,
 * everything else scaled in proportion.
 *===========================================================================*/

int gl_extract_audio_features(const float* audio,
                                uint32_t samples,
                                uint32_t sample_rate,
                                float* out_features,
                                uint32_t feature_dim);

/**
 * @brief One-shot audio → comprehension → grounding helper.
 *        Internally: extracts features, runs wernicke comprehend,
 *        feeds recognized words back as auditory grounding events.
 *
 *        Caller passes raw audio + wernicke adapter; gl handles the
 *        rest. Returns count of grounding events recorded, -1 on
 *        failure. NULL audio_features is no longer required — pass
 *        NULL to have the helper compute a default 32-dim envelope
 *        internally.
 *
 *        This is the supersedes the older internal-only signature
 *        that required the caller to pre-compute audio_features.
 */
int gl_drive_audio_comprehension(grounded_language_t* gl,
                                   void* wernicke_adapter,
                                   const float* audio,
                                   uint32_t samples,
                                   uint32_t sample_rate,
                                   const float* audio_features,
                                   uint32_t feature_dim);

/*=============================================================================
 * Compositional templates — bigrams + trigrams + phrase vectors (#9)
 *
 * Words rarely appear in isolation. Children learn that "good morning"
 * means more than "good" + "morning" — the bigram itself carries
 * meaning. This module tracks frequent bigrams + trigrams from
 * learn_from_text and stores a compositional semantic vector
 * (mean of constituent word vectors) per phrase.
 *
 * Storage: a fixed-capacity table keyed by the lowercased space-
 * joined form (e.g., "good morning", "happy birthday to"). Counts
 * exposure; computes the phrase vector lazily on first query.
 *
 * Public API: tracking happens automatically in learn_from_text;
 * top-K phrases retrievable via grounded_language_get_top_phrases.
 *===========================================================================*/

#define GL_MAX_PHRASE_LEN     128
#define GL_MAX_PHRASES         512

/** A phrase entry (bigram or trigram). The form is space-separated
 *  lowercased word forms. component_words is the count (2 or 3). */
typedef struct {
    char     form[GL_MAX_PHRASE_LEN];
    uint32_t form_hash;
    uint8_t  component_words;        /**< 2 = bigram, 3 = trigram */
    uint32_t frequency;
    /* Compositional semantic vector — mean of component word
     * context_vectors. Computed lazily and cached; cleared when any
     * component word's binding mutates so it stays consistent.
     * Sized to GL_SEMANTIC_DIM at gl_create time. */
    float*   semantic_vec;
    bool     vec_initialized;
} gl_phrase_t;

/**
 * @brief Retrieve the top-K most frequent learned phrases. Returns
 *        the count actually written. Read-only.
 *
 * @param gl          System handle (NULL → 0)
 * @param min_freq    Minimum frequency to qualify (0 = all)
 * @param min_n       Minimum component words (2 or 3; 0 = both)
 * @param out_phrases Caller-allocated array of pointers; results
 *                    are owned by GL — copy if you need to retain.
 * @param max_k       Capacity
 * @return Number of phrases written, 0 on bad input.
 */
uint32_t grounded_language_get_top_phrases(
    const grounded_language_t* gl,
    uint32_t min_freq,
    uint8_t min_n,
    const gl_phrase_t** out_phrases,
    uint32_t max_k);

/**
 * @brief Look up a specific phrase by its space-joined form.
 *        Returns NULL if not in the table.
 *
 *        Conceptually read-only but mutates a lazy semantic_vec cache
 *        on the matching phrase entry (computed from constituent
 *        word context_vectors on first lookup, reused thereafter).
 *        The cache is single-threaded by GL contract — the const-
 *        cast is intentional and safe under that contract.
 */
const gl_phrase_t* grounded_language_lookup_phrase(
    const grounded_language_t* gl,
    const char* form);

/**
 * @brief Number of phrases currently tracked. Useful for probes.
 */
uint32_t grounded_language_phrase_count(const grounded_language_t* gl);

/*=============================================================================
 * Cross-modal disambiguation (#8)
 *
 * Polysemous words ("bat" = flying mammal OR baseball bat, "bank" =
 * river edge OR financial institution) have multiple concept bindings,
 * each weighted across modalities. Plain comprehension activates them
 * all; disambiguation picks the most likely concept given which sensory
 * modalities are currently providing context.
 *
 * Score for each binding c:
 *   score(c) = max(binding.confidence, 0.05)
 *            × Σ_m  modality_strength[m] × modality_weights[m]
 *
 * The caller supplies a length-GL_MODALITY_COUNT weights vector
 * representing the currently-active modalities (e.g., [0.9, 0.1, 0, 0,
 * 0, 0] = "I'm looking at something but barely hearing anything"). The
 * top-K bindings by score are written into out_concepts/out_scores.
 *
 * Score range: not bounded to [0,1]. Each modality contributes up to
 * 1.0, summed across GL_MODALITY_COUNT (= 6) modalities, then scaled
 * by confidence. Practical max is ~6.0; callers comparing scores
 * across queries should use relative ranking, not absolute thresholds.
 *
 * Out-array contract: only indices [0, return_value) are written;
 * indices ≥ return_value are unmodified.
 *
 * Returns the number of bindings ranked, or 0 on bad input.
 *===========================================================================*/

/**
 * @brief Pick the top-K concept bindings for `word` given a per-
 *        modality salience profile. Read-only — does not mutate
 *        bindings or fire bus events.
 *
 *        When the word is unknown OR has only one binding, returns
 *        ≤ 1 (no disambiguation needed). When all weights are zero
 *        the result falls back to plain confidence × overall-strength
 *        ranking so callers can use this as a general "best concept"
 *        query without supplying weights.
 *
 * @param gl                    System handle
 * @param word                  Word form to disambiguate
 * @param modality_weights      Length GL_MODALITY_COUNT, in [0,1]
 *                              (clamped internally)
 * @param out_concepts          Caller-allocated output: concept ids
 * @param out_scores            Caller-allocated output: scores
 * @param max_k                 Capacity of out arrays
 * @return Number of bindings ranked into out_*; 0 on bad input.
 */
uint32_t grounded_language_disambiguate(
    const grounded_language_t* gl,
    const char* word,
    const float* modality_weights,
    uint64_t* out_concepts,
    float* out_scores,
    uint32_t max_k);

/*=============================================================================
 * Lexicon LRU eviction (#5)
 *
 * The lexicon is capped at GL_MAX_VOCAB. Without eviction, long-running
 * brains hit the cap and stop accepting new words — which silently
 * caps cumulative vocabulary growth (the symptom: vocab_count plateaus
 * at 16384 even though new words keep arriving).
 *
 * Policy:
 *   - Score = frequency. Low-frequency entries are evicted first.
 *   - Pinned threshold (GL_LRU_FREQ_PIN_FLOOR) — entries above this
 *     frequency are never evicted, even if N requests it. This protects
 *     the high-value core vocabulary.
 *   - Auto-trigger fires inside lexicon_find_or_create when vocab_count
 *     ≥ GL_LRU_HIGH_WATER. Caller-side trigger via the public API
 *     below is also supported (curriculum drivers can prune proactively
 *     during sleep cycles).
 *===========================================================================*/

/** Eviction threshold: when vocab grows past this, the next insert
 *  call auto-prunes a batch before allocating. */
#define GL_LRU_HIGH_WATER          (GL_MAX_VOCAB - 256)

/** Default batch size for an auto-eviction sweep. */
#define GL_LRU_EVICT_BATCH         256

/** Frequency floor below which entries are eviction candidates.
 *  Entries with frequency >= this value are pinned (never evicted). */
#define GL_LRU_FREQ_PIN_FLOOR      10

/**
 * @brief Evict up to `n` least-frequently-used entries from the lexicon.
 *        Pinned entries (frequency ≥ GL_LRU_FREQ_PIN_FLOOR) are never
 *        evicted regardless of `n`. Returns the actual count evicted.
 *
 *        After eviction the hash table is rebuilt; outstanding pointers
 *        from grounded_language_lookup() are invalidated.
 *
 * @param gl  System handle (NULL → 0)
 * @param n   Max entries to evict
 * @return Count of entries actually freed.
 */
uint32_t grounded_language_evict_lru(grounded_language_t* gl, uint32_t n);

/*=============================================================================
 * Phonological retrieval — rhyme + alliteration (#13)
 *
 * Linear-scan helpers over the active vocab. Cold-path API used for
 * curriculum, poetry/song generation, and analogical retrieval —
 * never invoked from comprehension/production hot loops, so the
 * vocab-cap-bounded O(N) cost is acceptable in exchange for not
 * carrying a separate hash index that would need creation, eviction,
 * and persistence parity.
 *
 * Output buffers (out_words[]) point into GL's vocab storage and are
 * valid only until the next lexicon mutation (fast_map / ground /
 * eviction). Callers that need to retain forms must copy.
 *
 * Result order: vocab_list iteration order (insertion order).
 * Consumers should not rely on a specific ranking.
 *
 * Character set: ASCII alphabet only ([A-Za-z]). Words with non-ASCII
 * letters at the matched edge are silently skipped — a French word
 * like "café" passes alliteration ('c'+'a'...) but a word starting
 * with "é" returns 0 from _gl_first_alpha_lower and is skipped.
 * UTF-8 codepoint awareness is deferred to a future wave.
 *===========================================================================*/

/** Suffix length used by the rhyme matcher (final N letters must match
 *  case-insensitively, ignoring non-alphabetics). 3 captures most
 *  common English rhymes ("-ight", "-ound", "-ake") without false
 *  positives from compound words. */
#define GL_RHYME_SUFFIX_LEN  3

/**
 * @brief Find words in the lexicon whose final GL_RHYME_SUFFIX_LEN
 *        letters match `word`'s suffix. Excludes the input word itself.
 *
 * @param gl         System handle
 * @param word       Reference word (must have at least GL_RHYME_SUFFIX_LEN
 *                   alphabetic characters)
 * @param out_words  Caller-supplied buffer of `const char*` pointers
 * @param max_out    Capacity of out_words
 * @return Number of rhymes written, or 0 on bad input.
 */
uint32_t grounded_language_lookup_rhymes(
    const grounded_language_t* gl,
    const char* word,
    const char** out_words,
    uint32_t max_out);

/**
 * @brief Find words in the lexicon whose first letter matches the first
 *        letter of `word` (case-insensitive). Excludes the input word.
 *
 *        Models alliteration as same-onset matching — minimum-viable
 *        signal that catches 99% of practical use-cases (poetry,
 *        memorization mnemonics) without a full phonemic onset
 *        analyzer.
 *
 * @return Number of alliterating words written, or 0 on bad input.
 */
uint32_t grounded_language_lookup_alliterations(
    const grounded_language_t* gl,
    const char* word,
    const char** out_words,
    uint32_t max_out);

/*=============================================================================
 * Probes & Metrics (operational telemetry beyond gl_stats_t)
 *===========================================================================*/

/**
 * @brief Operational probe metrics for the language module. Returned
 *        by grounded_language_get_probe_metrics — covers everything a
 *        Grafana/dashboard query would want without exposing internal
 *        struct layout.
 *
 *  All counters are cumulative since gl_create. All gauges are
 *  instantaneous (current state). Caller-owned out struct, zero-filled
 *  on entry by the impl so missing fields stay defaulted.
 */
typedef struct {
    /* === Lexicon gauges === */
    uint32_t vocab_count;             /**< Current entries in lexicon */
    uint32_t templates_count;         /**< Always 0 — templates removed; field kept for ABI stability */
    float    avg_binding_strength;    /**< Mean across all bindings */
    float    avg_binding_confidence;  /**< Mean across all bindings */

    /* === Bus state === */
    uint32_t subscriber_count;        /**< Total subscribers right now */
    uint32_t subscriber_high_priority;/**< Subs with priority > 0 */
    uint32_t subscriber_filtered;     /**< Subs with mask != ALL */
    uint64_t events_dropped_reentry;  /**< Inner fire blocked by guard (#11) */
    bool     in_fire_event;           /**< True if we're inside fire right now */

    /* === Forgetting curve (#15) === */
    uint32_t entries_decayed_last_24h;
    uint64_t entries_decayed_all_time;

    /* === Network bridges === */
    float    last_lnn_response_mag;   /**< [0,1] last LNN bridge magnitude */
    float    last_cnn_response_mag;
    float    last_fno_response_mag;
    float    last_ann_response_mag;

    /* === Cortex modulation taps (current state, not cumulative) === */
    float    visual_activity;
    float    audio_salience;
    float    speech_confidence;

    /* === Throughput rates (since boot) === */
    uint64_t total_groundings;
    uint64_t total_comprehensions;
    uint64_t total_productions;
    uint64_t total_new_words;

    /* === Region wiring health === */
    bool     broca_attached;
    bool     wernicke_attached;
    bool     working_memory_attached;
    bool     hippocampus_attached;
    bool     embedding_attached;
    bool     tokenizer_attached;

    /* === Dialect / accent (#14) === */
    char     context_dialect[GL_MAX_DIALECT_LEN];

    /* === Active-learning + negative-grounding throughput === */
    uint64_t total_negative_groundings;
    uint64_t total_needs_grounding;
} gl_probe_metrics_t;

/**
 * @brief Snapshot operational probe metrics. Out is zero-filled
 *        on entry so future field additions remain backwards-safe
 *        (missing reads see 0, not stale stack data).
 *
 * @param gl   System handle (NULL → no-op, returns -1)
 * @param out  Destination (NULL → no-op, returns -1)
 * @return 0 on success, -1 on bad params.
 */
int grounded_language_get_probe_metrics(const grounded_language_t* gl,
                                          gl_probe_metrics_t* out);

/*=============================================================================
 * Per-cognitive-module attach helpers — convenience wrappers that
 * subscribe a wrapper callback translating gl_event_t into the
 * module's native API. Each accepts NULL for the module pointer to
 * make brain-init wiring tolerant of missing modules.
 *===========================================================================*/

/** Attach inner-speech: every NEW_WORD + COMPREHENDED event becomes
 *  a candidate for silent rehearsal. mod is the inner_speech_t* opaque
 *  handle (NULL = no-op). */
void grounded_language_attach_inner_speech(grounded_language_t* gl, void* mod);

/** Attach imagination engine: GROUNDED events register the new
 *  concept as a candidate scene element. */
void grounded_language_attach_imagination(grounded_language_t* gl, void* mod);

/** Attach theory of mind: COMPREHENDED utterances are observations
 *  about an external speaker's beliefs. */
void grounded_language_attach_theory_of_mind(grounded_language_t* gl, void* mod);

/** Attach empathetic response: emotionally-charged GROUNDED events
 *  (high arousal) seed empathy generation. */
void grounded_language_attach_empathy(grounded_language_t* gl, void* mod);

/** Attach introspection: every PRODUCED event is logged as a
 *  self-narration sample. */
void grounded_language_attach_introspection(grounded_language_t* gl, void* mod);

/** Attach reasoning subsystem: COMPREHENDED utterances feed forward
 *  chaining as new premises. */
void grounded_language_attach_reasoning(grounded_language_t* gl, void* mod);

/** Attach narrative system: COMPREHENDED text appended to the
 *  current narrative buffer. */
void grounded_language_attach_narrative(grounded_language_t* gl, void* mod);

/** Attach metacognition: every event ticks the metacognitive
 *  reflection counter. */
void grounded_language_attach_metacognition(grounded_language_t* gl, void* mod);

/** Attach analogical-transfer: NEW_WORD events trigger analogy
 *  search against existing concepts. */
void grounded_language_attach_analogical(grounded_language_t* gl, void* mod);

/** Attach emergent-language module: GROUNDED + PRODUCED events
 *  feed the emergent-symbol learner. */
void grounded_language_attach_emergent_language(grounded_language_t* gl, void* mod);

/*--------------------------------------------------------------------------
 * Brain-region subscribers — anatomical regions that should observe
 * language events in their own modality. All four are NULL-tolerant
 * (mod = NULL is a disconnect).
 *------------------------------------------------------------------------*/

/** Attach prefrontal cortex: COMPREHENDED + PRODUCED + high-arousal
 *  GROUNDED events feed executive monitoring of language. */
void grounded_language_attach_prefrontal(grounded_language_t* gl, void* mod);

/** Attach insula: COMPREHENDED events with non-zero valence feed
 *  interoceptive integration (gut-feel evaluation of an utterance). */
void grounded_language_attach_insula(grounded_language_t* gl, void* mod);

/** Attach cingulate cortex: COMPREHENDED + PRODUCED events feed
 *  conflict-monitoring (low-confidence comprehensions or production
 *  errors are flagged for ACC attention). */
void grounded_language_attach_cingulate(grounded_language_t* gl, void* mod);

/** Attach amygdala: GROUNDED events with arousal > 0.3 feed
 *  emotional tagging of the bound concept (fear/threat conditioning
 *  on language-encoded experiences). */
void grounded_language_attach_amygdala(grounded_language_t* gl, void* mod);

/** Attach orbitofrontal cortex (OFC): GROUNDED + COMPREHENDED events
 *  with non-trivial valence feed reward-value bindings (stimulus
 *  value learning + RPE updates). */
void grounded_language_attach_ofc(grounded_language_t* gl, void* mod);

/*=============================================================================
 * Query / Introspection
 *===========================================================================*/

/**
 * @brief Look up word in lexicon
 * @param gl    System handle
 * @param word  Word to look up
 * @return Lexicon entry (read-only), or NULL if unknown word
 */
const gl_lexicon_entry_t* grounded_language_lookup(
    const grounded_language_t* gl,
    const char* word);

/**
 * @brief Get words associated with a concept
 * @param gl          System handle
 * @param concept_id  Concept to find words for
 * @param words       Output array of word forms (caller provides)
 * @param max_words   Maximum words to return
 * @return Number of words found
 */
uint32_t grounded_language_words_for_concept(
    const grounded_language_t* gl,
    uint64_t concept_id,
    const char** words,
    uint32_t max_words);

/**
 * @brief Get system statistics
 * @param gl    System handle
 * @param stats Output statistics
 */
void grounded_language_get_stats(
    const grounded_language_t* gl,
    gl_stats_t* stats);

/**
 * @brief Per-modality binding-coverage telemetry.
 *
 * WHAT: For each modality m in [0, GL_MODALITY_COUNT), counts the number of
 *       bindings across the entire lexicon that have non-zero
 *       modality_strength[m]. A binding contributes to a modality's count
 *       once per modality it has learned content in (so a single binding
 *       grounded across multiple modalities increments multiple slots).
 * WHY:  Curriculum integration tests need a direct probe to verify that
 *       grounding events are actually producing per-modality coverage —
 *       a brain that has only ever been grounded visually should report
 *       zero on auditory/motor/etc. The aggregate gl_stats_t.total_bindings
 *       collapses this signal.
 * HOW:  Linear walk over vocab_list × bindings (small constant per binding,
 *       bounded by GL_MODALITY_COUNT). Cheap; called from probes only.
 *
 * @param gl          System handle (NULL → out_counts left zeroed)
 * @param out_counts  Caller-allocated array of size GL_MODALITY_COUNT (= 6).
 *                    Zeroed at entry. Writes counts[VISUAL=0..LINGUISTIC=5].
 */
void grounded_language_get_modality_counts(
    const grounded_language_t* gl,
    uint32_t out_counts[GL_MODALITY_COUNT]);

/**
 * @brief Get the semantic vector dimension this system was created with.
 *
 * WHAT: Exposes the immutable semantic_dim chosen at create() time.
 * WHY:  Diagnostic probes (probe_comprehend) need this to bound reads
 *       — comprehension_result.semantic_vector is sized to this dim and
 *       walking past it is undefined behaviour.
 * @return semantic_dim, or 0 if gl is NULL.
 */
uint32_t grounded_language_get_semantic_dim(
    const grounded_language_t* gl);

/**
 * @brief Phase 2C: walk gl->vocab_list and push each entry into Broca's
 *        separate lexicon so its CYK chart parser has POS info for the
 *        content words bridge production routes through it.
 *
 * Without this, every WordNet word that the bridge produces shows up
 * to Broca as POS_UNKNOWN, no phrase rule fires, syntax_build_tree
 * returns false, and broca_produce_from_strings rejects every input.
 *
 * Maps gl_word_class_t → part_of_speech_t. Uses entry->form_hash as
 * broca's word_id so both subsystems share the same hash space.
 * Phonemes are not pushed (Broca's phonological_processor generates
 * them on-demand from spelling).
 *
 * Idempotent under broca_reset; otherwise re-running creates duplicate
 * hash-bucket entries with no functional impact.
 *
 * @return Number of entries successfully mirrored.
 */
uint64_t grounded_language_mirror_lexicon_to_broca(
    grounded_language_t* gl, void* broca_adapter);

/*=============================================================================
 * Serialization
 *===========================================================================*/

/**
 * @brief Save grounded language state to file
 * @param gl   System handle
 * @param path File path to save to
 * @return 0 on success, -1 on error
 */
int grounded_language_save(
    const grounded_language_t* gl,
    const char* path);

/**
 * @brief Load grounded language state from file
 * @param path File path to load from
 * @param semantic_memory Semantic memory to connect (can be NULL)
 * @return Loaded system, or NULL on error
 */
grounded_language_t* grounded_language_load(
    const char* path,
    void* semantic_memory);

/*=============================================================================
 * Slice E (Option-1 rebuild) — developmental-stage vocab mask.
 *
 * The mask is a per-entry visibility bitmap over the lexicon: entries
 * vocab_list[0..N) are visible for production iff mask[i] == true. When
 * no mask is installed (the default), every entry is visible.
 *
 * `grounded_language_set_active_vocab_mask(stage)` installs / refreshes a
 * mask sized to the lexicon's current vocab_count. The first
 * stage_table_get(stage)->max_visible_vocab entries (in insertion order
 * — a frequency proxy for the early curriculum) become visible; the
 * rest are masked. The mask reflects insertion-order frequency at the
 * time of the call; new entries added after installation are appended as
 * visible (so newly-learned vocabulary doesn't get hidden by an older
 * mask).
 *
 * `grounded_language_clear_active_vocab_mask` frees the mask. After this
 * every entry is visible again — equivalent to "no constraints".
 *
 * `grounded_language_filter_production` rewrites a freshly-produced
 * gl_production_result_t in place, dropping words whose lexicon index
 * is masked out. Word_count is updated to reflect the surviving tokens.
 * Returns the number of words dropped.
 *=============================================================================*/

/**
 * @brief Install / refresh a stage-derived vocabulary mask.
 *
 * Indexed against the current lexicon insertion order. The first N entries
 * (where N = stage_table_get(stage)->max_visible_vocab, clamped to the
 * current vocab_count) are marked visible; the rest are masked.
 *
 * Calling this with a stage whose max_visible_vocab is SIZE_MAX leaves
 * every entry visible (equivalent to clearing the mask). The function
 * still records the stage so subsequent grow events extend correctly.
 *
 * @return 0 on success, -1 on alloc failure or NULL gl.
 */
int grounded_language_set_active_vocab_mask(grounded_language_t* gl,
                                              uint32_t stage);

/**
 * @brief Clear the active vocabulary mask. Idempotent.
 */
void grounded_language_clear_active_vocab_mask(grounded_language_t* gl);

/**
 * @brief Predicate: is the lexicon entry at vocab_list[idx] visible?
 *
 * Returns true when no mask is installed OR when idx is out of range
 * (defensive: don't deny entries the mask doesn't cover) OR when
 * mask[idx] is true.
 */
bool grounded_language_vocab_index_visible(const grounded_language_t* gl,
                                            uint32_t idx);

/**
 * @brief Strip masked-out tokens from a production result in place.
 *
 * Tokens are matched against the lexicon by surface form (case-insensitive
 * lookup); tokens that don't resolve to any lexicon entry are left as-is
 * (defensive: we never silently drop unknown surface forms — the cascade
 * sees them and decides). The result's text buffer is rewritten with the
 * surviving tokens joined by single spaces; word_count is updated to the
 * surviving count. If every token is masked out, the result is left as a
 * single space-separated string of the same content with word_count
 * unchanged — the cascade's downstream stages decide how to recover.
 *
 * Returns the number of words dropped (>= 0).
 */
uint32_t grounded_language_filter_production_by_mask(
    grounded_language_t* gl,
    gl_production_result_t* result);

/**
 * @brief Set bridge enable flags (D1 FIX, 2026-10-21)
 *
 * Controls which cognitive→language integration modules are active.
 * Bitfield uses GL_BRIDGE_* constants from internal header.
 *
 * @param gl     Grounded language instance
 * @param flags  Bitfield of enabled bridges (GL_BRIDGE_ALL = all enabled)
 */
void grounded_language_set_bridge_enable_flags(grounded_language_t* gl,
                                                 uint32_t flags);

/**
 * @brief Get bridge enable flags
 *
 * @param gl  Grounded language instance
 * @return Bitfield of enabled bridges
 */
uint32_t grounded_language_get_bridge_enable_flags(const grounded_language_t* gl);

/**
 * @brief Check if specific bridge is enabled
 *
 * @param gl     Grounded language instance
 * @param mask   Bridge bit mask (e.g., GL_BRIDGE_HIPPOCAMPUS)
 * @return true if bridge is enabled
 */
bool grounded_language_is_bridge_enabled(const grounded_language_t* gl,
                                           uint32_t mask);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GROUNDED_LANGUAGE_H */
