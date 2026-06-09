//=============================================================================
// nimcp_snn_language_bridge.h - SNN ↔ Language Transport Bridge (Option-1)
//=============================================================================
/**
 * @file nimcp_snn_language_bridge.h
 * @brief Transport-only bridge between SNN concept/word populations and
 *        the language system.
 *
 * Option-1 rebuild (Slice A, 2026-05-19): the bridge no longer owns a
 * concept_pop × word_pop STDP weight matrix. Concept patterns are owned
 * by the SNN; the lexicon stores word_id ↔ concept_pop_id indexes
 * (Slice B's concept_registry). This bridge is a pure transport — it
 * routes spikes between concept and word populations and applies K-WTA
 * lateral inhibition (a local-dynamics primitive, not learning).
 *
 * All STDP / weight / binding-learning APIs from the pre-rebuild bridge
 * are now NO-OP stubs that return success. They remain in the header for
 * ABI compatibility with callers (notably grounded_language.c, Slice B's
 * territory) until those call sites are migrated. New code should use the
 * `snn_language_bridge_route_*` family instead.
 *
 * Integrates with:
 * - Grounded Language System (production/comprehension fallback)
 * - Imagination SNN (creative spike patterns → word selection)
 * - Curiosity SNN (novelty-driven lexical exploration)
 * - Neuromodulator system (DA broadcast; bridge passes it through to SNN —
 *   bridge has no weights to modulate of its own)
 */

#ifndef NIMCP_SNN_LANGUAGE_BRIDGE_H
#define NIMCP_SNN_LANGUAGE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/* Raised 4096→8000 (2026-06-09) to fill Wernicke (64000/8) with DISJOINT concept
 * ensembles and cut concept-level (concept_id % CONCEPT_POPS) collisions. */
#define SNN_LANG_MAX_CONCEPT_POPS     8000
/* Vocab cap for the SNN language bridge. Bumped 16384 → 32768 once the
 * canonical literary corpus + active-learning grounding pushed produced
 * vocab past the 8K-12K band. Each bridge instance allocates O(cap)
 * arrays (word_pops, word_norm_sq, word_emb_cache, word_emb_norm), so
 * every doubling adds ~12-16MB per bridge — trivial at single-bridge
 * scale. Keep aligned with grounded_language's form_hash modulus —
 * mismatches reproduce the silent-capacity-drop bug logged 2026-04-27. */
/* Capped 32768→8000 (2026-06-09 collision fix) = Broca(64000)/NEURONS_PER_POP(8)
 * so every word pop gets a DISJOINT contiguous 8-neuron ensemble with NO overlap.
 * At 32768, 32768×8=262144 ensemble-neuron claims overlapped ~4× in 64000 Broca
 * neurons — distinct words shared neurons and were indistinguishable at decode
 * (the generation scramble). 8000 clean slots; ~4 lexicon words may share a slot
 * (resolved by projection weight competition) until Broca grows via checkpoint
 * migration. */
#define SNN_LANG_MAX_WORD_POPS        8000
#define SNN_LANG_MAX_ATTACHED_POPS    8
#define SNN_LANG_NEURONS_PER_POP      8
/* Max per-synapse weight for the concept→word projection. Higher than the 2.0
 * generic-AMPA cap because only SNN_LANG_NEURONS_PER_POP synapses converge on a
 * word neuron: at 2.0 the ensemble delivers 16 < the 20mV LIF gap and never
 * fires. The projection is excluded from global plasticity, so a higher cap here
 * doesn't affect the rest of the brain. */
#define SNN_LANG_PROJ_W_MAX           50.0f
#define SNN_LANG_DEFAULT_STDP_TAU     50.0f   // ms (wider than standard 20ms)
#define SNN_LANG_DEFAULT_STDP_A_PLUS  0.008f
#define SNN_LANG_DEFAULT_STDP_A_MINUS 0.0084f // Slight LTD bias for sparsity
#define SNN_LANG_BINDING_W_MAX        1.0f
#define SNN_LANG_BINDING_W_MIN        0.0f
#define SNN_LANG_DECODE_WINDOW_MS     100.0f
#define SNN_LANG_DECAY_RATE           0.95f
#define SNN_LANG_SPIKE_BLEND_DEFAULT  0.1f    // 10% spike, 90% vector initially
#define SNN_LANG_MAGIC                0x534C4247  // "SLBG"

/* On-disk file format version markers.
 *
 * V2 (legacy, no version field): [magic:u32][snn_lang_config_t blob][...].
 *   Loader detects V2 because the u32 immediately after magic is
 *   `max_concept_pops` (a small bounded number, ≤ SNN_LANG_MAX_CONCEPT_POPS),
 *   never the V3 sentinel.
 *
 * V3 (PA + MQ knobs persisted explicitly):
 *   [magic:u32]
 *   [SNN_LANG_BRIDGE_FILE_V3_SENTINEL:u32]
 *   [SNN_LANG_BRIDGE_FILE_VERSION_V3:u32 = 3]
 *   [snn_lang_config_t blob]
 *   [ext_block_size:u32]   // size of fields below, in bytes
 *   [temperature:f32]
 *   [top_p:f32]
 *   [produce_topk:u32]
 *   [glove_blend:f32]
 *   [intent_persistence:f32]
 *   [word_feedback:f32]
 *   [enable_snn_spike_routing:u8]
 *   [activation_tau_ms:f32]
 *   [use_hyperbolic_embeddings:u8]
 *   [sampling_mode:i32]
 *   [...remainder: populations, bindings...]
 *
 * V4 (V3 + EOS-stopping knobs persisted explicitly):
 *   Same prefix as V3 with SNN_LANG_BRIDGE_FILE_VERSION_V4 instead of V3,
 *   plus 3 trailing fields appended to the ext block:
 *
 *   [enable_eos_stopping:u8]
 *   [eos_min_activation:f32]
 *   [eos_min_confidence:f32]
 *
 *   V3 readers ignore the trailing bytes (forward-compat via ext_block_size).
 *   V4 readers handle both V3 and V4 files — V3 files leave EOS knobs at
 *   library defaults; V4 files decode them authoritatively.
 *
 * V5 (V4 + beam re-rank knobs in ext block + cumulative stats trailer):
 *   Same prefix as V4 with SNN_LANG_BRIDGE_FILE_VERSION_V5 instead of V4,
 *   plus 3 trailing fields appended to the ext block:
 *
 *   [enable_beam_hnn_rerank:u8]
 *   [beam_hnn_weight:f32]
 *   [beam_length_norm_alpha:f32]
 *
 *   AND after the bindings array, a self-describing stats trailer block:
 *
 *   [stats_block_size:u32]
 *   [29 × u64 cumulative counters]
 *
 *   The stats trailer fixes the gap where every cumulative counter
 *   (total_produce_calls, total_eos_terminations, etc.) was lost on every
 *   save — V4 and earlier wrote no stats at all.
 *
 *   V3/V4 readers on a V5 file: ignore the trailing config bytes via
 *   ext_block_size; the stats block sits past the bindings array which
 *   they stop reading at. V5 readers on a V3/V4 file: stats stay zero
 *   (EOF after bindings is non-fatal).
 *
 * The ext_block_size lets future readers seek past unknown trailing bytes
 * if/when more knobs are added. */
#define SNN_LANG_BRIDGE_FILE_V3_SENTINEL  0xFFFFFFFEu
#define SNN_LANG_BRIDGE_FILE_VERSION_V2   2u  /* implicit: no version on disk */
#define SNN_LANG_BRIDGE_FILE_VERSION_V3   3u
#define SNN_LANG_BRIDGE_FILE_VERSION_V4   4u
#define SNN_LANG_BRIDGE_FILE_VERSION_V5   5u

//=============================================================================
// Forward declarations
//=============================================================================

struct snn_network_s;
struct grounded_language;
struct imagination_snn_bridge;
struct curiosity_snn_bridge;
struct neuromodulator_system_struct;

//=============================================================================
// Types
//=============================================================================

/** Sparse binding entry (concept_id → word_id with STDP weight) */
typedef struct {
    uint32_t concept_pop;       // Concept population index
    uint32_t word_pop;          // Word population index
    float    weight;            // STDP-modifiable binding strength [0, 1]
    float    eligibility;       // Eligibility trace for three-factor learning
    float    pre_trace;         // Presynaptic (concept) trace
    float    post_trace;        // Postsynaptic (word) trace
    float    last_pre_spike_ms; // Last concept spike time
    float    last_post_spike_ms;// Last word spike time
    uint32_t ltp_count;         // Potentiation events
    uint32_t ltd_count;         // Depression events
} snn_lang_binding_t;

/** Configuration for SNN language bridge */
typedef struct {
    uint32_t max_concept_pops;     // Max concept populations
    uint32_t max_word_pops;        // Max word populations
    uint32_t neurons_per_pop;      // Neurons per population
    float    stdp_tau_plus;        // LTP time constant (ms)
    float    stdp_tau_minus;       // LTD time constant (ms)
    float    stdp_a_plus;          // LTP amplitude
    float    stdp_a_minus;         // LTD amplitude
    float    stdp_learning_rate;   // Base learning rate
    float    binding_w_max;        // Max binding weight
    float    decode_window_ms;     // Spike integration window
    float    decay_rate;           // Per-step activation decay
    float    spike_blend;          // Spike vs vector blend [0=all vector, 1=all spike]
    bool     enable_da_modulation; // Dopamine-gated three-factor learning
    float    da_modulation_gain;   // DA → LR scaling
    bool     enable_imagination;   // Wire imagination SNN output to word selection
    bool     enable_curiosity;     // Wire curiosity SNN to lexical exploration
    bool     enable_sleep_consolidation; // Enable binding replay during sleep
    float    prune_threshold;      // Binding weights below this are pruned
    /* PA-6: sampling knobs for bridge_produce. temperature == 0 keeps the
     * legacy hard-argmax behavior (every produce_word picks the top non-
     * refractory candidate). temperature > 0 enables softmax sampling
     * over the top-K cosine-scored candidates. top_p applies nucleus
     * truncation (keep candidates whose cumulative probability ≥ top_p,
     * default 1.0 = no truncation). produce_topk is the candidate pool
     * size pulled from decode_spikes per word (default 5; max 32). */
    float    temperature;
    float    top_p;
    uint32_t produce_topk;
    /* PA-5: GloVe-aware blend.  decode_spikes ranks each word by
     *   (1 − glove_blend) · cosine(concept_rates, binding_col[w])
     * + glove_blend       · cosine(concept_rates[:emb_dim], glove_emb[w])
     * when an embedding lookup callback is attached. glove_blend = 0
     * (default) preserves Patch-A binding-only behavior. */
    float    glove_blend;
    /* PA-2: autoregressive recurrent decoder controls. Per-step:
     *   state_t   = (1 − word_feedback) · state_{t-1} + word_feedback · w_{t-1}
     *   concept_t = intent_persistence · intent + (1 − intent_persistence) · state_t
     * with state_0 = intent. word_feedback = 0.3 (legacy hard-coded value)
     * and intent_persistence = 0 (default) preserve the prior behavior
     * exactly — concept_acts evolves entirely through state, which decays
     * the original intent away in favor of recently-picked word reverse-
     * encodings. Set intent_persistence > 0 to keep the prompt's intent
     * present at every step (real autoregressive context, not just bag-
     * of-words ranked by drifted activation). */
    float    intent_persistence;
    float    word_feedback;
    /* PA-3: SNN-spike → bridge STDP wiring. enable_snn_spike_routing
     * (default false) gates the entire path. When enabled, drain
     * spike_output from attached Broca/Wernicke pops per global tick
     * and route through concept_spike / word_spike. activation_tau_ms
     * is the decay time constant for concept_pops[].activation and
     * word_pops[].activation — required to be > 0 to prevent the
     * accumulator runaway that previously drove SNN sparsity to 0.00. */
    bool     enable_snn_spike_routing;
    float    activation_tau_ms;
    /* PA-5+: hyperbolic GloVe distance mode. When true *and* the GloVe
     * blend is active (glove_blend > 0 + emb_lookup attached), the
     * GloVe term in decode_spikes is replaced with
     *   1 / (1 + d_H(query, word_emb))
     * where d_H is the Poincaré-ball hyperbolic distance. Larger ⇒ better,
     * matching the cosine sign convention. Vectors are projected into the
     * Poincaré ball via tanh(‖v‖)·v/‖v‖ at query-time. Default false
     * reproduces Euclidean cosine PA-5 behavior bit-for-bit. */
    bool     use_hyperbolic_embeddings;
    /* PA-6+: produce-time sampling mode dispatch.
     *   0 = legacy / argmax (temperature == 0 path)
     *   1 = softmax + nucleus top-p (PA-6, default when temperature > 0)
     *   2 = quantum-Monte-Carlo MCMC sampling over candidate scores.
     * Mode 1 is auto-selected when temperature > 0 and sampling_mode == 0
     * (preserves PA-6 callers). Mode 2 must be set explicitly. */
    int      sampling_mode;
    /* TIER1-A: optional beam-K decoding in snn_language_bridge_produce.
     * produce_beam_width = 1 (default) preserves greedy / current behavior
     * bit-for-bit. > 1 maintains K parallel beams (each with its own state
     * buffer + cumulative log-prob), expands their top-V candidates per
     * step, prunes to K best by length-normalized score
     *   logprob / token_count^0.6
     * and returns the highest-scoring beam's token sequence. Capped at 16. */
    uint32_t produce_beam_width;
    /* TIER1-B: explicit end-of-utterance token. eos_word_pop == UINT32_MAX
     * (default) disables the mechanism. When set to a registered word_pop,
     * sampling that pop in the produce loop halts production cleanly
     * (without appending the EOS token to the output text). */
    uint32_t eos_word_pop;
    /* TIER1-C: n-gram repetition penalty. Per produce step, before
     * sampling, multiplicatively penalize the score of any candidate
     * whose word_pop appears in the last repetition_window picks:
     *   score *= (1 - repetition_penalty)   per match.
     * repetition_penalty == 0 (default) disables; window == 0 falls back
     * to the default (3) when penalty > 0. Penalty clamped to [0, 1]. */
    float    repetition_penalty;
    uint32_t repetition_window;
    /* TB-7: hard length-control on snn_language_bridge_produce.
     *
     * min_produce_words > 0 suppresses an EOS pick (eos_word_pop) until at
     * least that many real words have been emitted — when EOS would
     * otherwise win, the produce loop falls back to the next-best non-EOS
     * candidate and bumps stats.length_min_suppressions. 0 (default) keeps
     * EOS firing immediately as before.
     *
     * max_produce_words > 0 forces the produce loop to terminate cleanly
     * once that many words have been emitted, regardless of EOS state or
     * the legacy hard cap (32). 0 (default) keeps the legacy 32-word cap.
     *
     * Both clamped at runtime to [0, 1024]. Values must satisfy
     * min ≤ max when both are nonzero (rejected by the setter). */
    uint32_t min_produce_words;
    uint32_t max_produce_words;
    /* TC-11 — GPU-decode scaffold flag (NOT YET IMPLEMENTED).
     *
     * Reserved for a future CUDA port of snn_language_bridge_decode_spikes.
     * The decode pass currently runs on CPU; SIMD muladds + cosine
     * normalize + GloVe blend cost ~50µs at vocab=2K, so the PCIe
     * round-trip would dominate any GPU win until vocab grows past
     * ~16K and the binding map past ~100K entries. Flag stays for ABI
     * forward-compat — the decode hot path checks it, logs a one-shot
     * warning if true, and falls through to the CPU path. When the
     * actual kernel ships, this same flag will route to the GPU path.
     *
     * Default false. Setting it currently has no effect except the
     * one-shot informational log. */
    bool     enable_gpu_decode;
    /* CSTDP — comprehend-driven bridge STDP. When false (default),
     * snn_language_bridge_comprehend is the legacy read-only forward
     * pass: tokenize → walk bindings → return concept activations. The
     * bridge's STDP loop never fires during training-time comprehend,
     * so bridge weights stay near initialization regardless of how
     * many texts the trainer feeds it.
     *
     * When true, comprehend additionally fires concept_pop+word_pop
     * spike pairs in production-direction order (concept first, then
     * word δ ms later) for bindings whose pre-existing weight crosses
     * comprehend_stdp_min_weight AND whose accumulated activation for
     * this comprehend call crosses comprehend_stdp_min_activation,
     * then calls a SCOPED apply_stdp variant that walks only the
     * touched concept/word pop pairs (not the full 1.6M-binding
     * hash). Reinforces existing strong bindings on every comprehend
     * — turns lexicon-side training signal into bridge-weight
     * reinforcement without a separate supervised loop.
     *
     * Default OFF — turn on with
     * snn_language_bridge_set_comprehend_stdp_enabled after verifying
     * the bridge isn't dominated by noise bindings. Stage-1 brains
     * with mostly-noise weights would entrench noise. */
    bool     enable_comprehend_stdp;
    float    comprehend_stdp_min_weight;     /* default 0.05 */
    float    comprehend_stdp_min_activation; /* default 0.10 */
    float    comprehend_stdp_lr_scale;       /* multiplied with stdp_learning_rate, default 0.5 */
    /* EOS stopping criterion — opt-in early termination for the produce
     * loop based on intrinsic "thought-complete" signals rather than a
     * trained EOS token.
     *
     * When enable_eos_stopping is true, after each emitted word the
     * produce loop additionally checks two thresholds and stops early
     * (only after at least min_produce_words have been emitted, so the
     * caller's hard length-control floor still wins):
     *
     *   1. Activation drop: ||concept_acts||_2 (post decay+feedback for
     *      the next step) < eos_min_activation. Rationale: when the
     *      original intent has been "spent" by the recurrent state
     *      drift, the brain has nothing more to say.
     *
     *   2. Coherence drop: the just-emitted word's confidence
     *      (word_result.confidence) < eos_min_confidence. Rationale:
     *      below this, the system is just emitting noise — no clearly
     *      preferred word for the current activation.
     *
     * Either threshold being undershot triggers the stop. Default OFF
     * for backward compatibility — bit-for-bit identical output to
     * pre-EOS behavior.
     *
     * APPEND-ONLY: these fields MUST stay at the end of snn_lang_config_t
     * — older TUs compiled against the pre-EOS layout still read/write
     * the leading fields safely. On-disk persistence uses the explicit V4
     * ext block (not the raw struct blob) so a stale TU + new library
     * cannot stack-smash even if the struct grows further. */
    bool     enable_eos_stopping;
    float    eos_min_activation;             /* default 0.05 */
    float    eos_min_confidence;             /* default 0.01 */
    /* Beam-search re-ranking knobs.
     *
     * enable_beam_hnn_rerank gates an HNN energy-deviation penalty on the
     * final beam selection step. When true and the bridge has an attached
     * lnn_hamiltonian_net_t (via snn_language_bridge_set_hnn), each beam's
     * length-normalized score is multiplied by
     *   1.0 / (1.0 + beam_hnn_weight * |energy_deviation|)
     * before picking the winner. Filters out beams whose recurrent state
     * drift implies thermodynamic implausibility — closes the audit-item-4
     * gap where energy_deviation was computed but never read by language.
     *
     * beam_length_norm_alpha is the exponent in Wu et al. length-norm:
     *   normalized = cum_logprob / max(1, n)^alpha
     * Default 0.6 preserves prior beam ranking bit-for-bit. Clamped at
     * runtime to [0.1, 1.5]; out-of-range values fall back to 0.6.
     *
     * APPEND-ONLY: these fields stay at the end of snn_lang_config_t.
     * On-disk persistence uses the V5 ext block tail. */
    bool     enable_beam_hnn_rerank;
    float    beam_hnn_weight;                /* default 1.0 */
    float    beam_length_norm_alpha;         /* default 0.6 (Wu et al.) */
    /* Margin-gated LTD for learn_next_token_pair / _triple. The legacy
     * rule applied LTD on (context_concepts, top-1) whenever the target
     * word_pop wasn't already top-1. At high pump rates (~85 updates/min
     * via learn_text_bigrams) this indiscriminate suppression erodes
     * every globally-dominant binding regardless of whether the
     * false_winner was actually competing in *this* context, collapsing
     * the bridge onto a tiny set of un-pruned word_pops (see step-3900
     * regression: 1/50 unique answers, mean_conf 0.0022).
     *
     * With this margin gate, LTD fires only when BOTH:
     *   (a) target_rank is in [0, num_out)  — target appears in topK
     *   (b) topK[0].confidence >= ltd_margin * topK[target_rank].confidence
     *       — false_winner is decisively beating target in this context
     *
     * ltd_margin = 1.0 reproduces the legacy "any winner > target" rule;
     * 1.5 (default) requires false_winner to lead by 50%; ≥10.0 effectively
     * disables LTD. Clamped to [1.0, 100.0] at runtime — values below 1.0
     * would flip the direction. Runtime-only; not persisted in the V5 ext
     * block (set via snn_language_bridge_set_ltd_margin on resume). */
    float    ltd_margin;                     /* default 1.5 */
    /* Slice 4 — Lateral inhibition in lexical selection.
     *
     * Real cortex doesn't pick a word by argmax; it runs a recurrent
     * competition where candidate word representations excite themselves
     * AND inhibit each other. Winner emerges from settling dynamics over
     * ~50-200ms simulated time. Cohort model (Marslen-Wilson 1987),
     * interactive activation (McClelland 1981), drift-diffusion models —
     * all converge on this competitive structure as the mechanism behind
     * lexical selection.
     *
     * When enable_lateral_inhibition is true, the new
     * snn_language_bridge_decode_with_lateral_inhibition() function wraps
     * the standard decode_spikes pass: take its top-K candidates (the
     * cosine winners), initialize a[k]=score[k], then run T micro-steps of
     *
     *   new_a[k] = sigmoid(a[k]*gain_self - sum_{j!=k} a[j]*gain_inhibit)
     *
     * and re-rank by the post-settling activations. cascade_stage_lexical
     * routes through this path automatically when the flag is on; standard
     * decode_spikes callers are unaffected.
     *
     * Defaults preserve bit-for-bit legacy behavior (flag OFF). When the
     * caller opts in, lateral_gain_self=1.5 and lateral_gain_inhibit=0.026
     * (~ 0.8/(32-1)) bracket the competition in the regime where one
     * candidate consistently wins without runaway. lateral_micro_steps=20
     * matches the ~50ms/step biological cadence at our per-step cost.
     *
     * APPEND-ONLY: these fields MUST stay at the end of snn_lang_config_t.
     * Not persisted in the V5 ext block (runtime-only); caller re-applies
     * via snn_language_bridge_set_lateral_inhibition_* setters after load. */
    bool     enable_lateral_inhibition;      /* default false */
    float    lateral_gain_self;              /* default 1.5  */
    float    lateral_gain_inhibit;           /* default 0.026f ~= 0.8/(32-1) */
    uint32_t lateral_micro_steps;            /* default 20   */
} snn_lang_config_t;

/** Word decode result */
typedef struct {
    uint32_t    word_pop;      // Word population index
    const char* word_form;     // Word string (borrowed pointer)
    float       activation;    // Accumulated activation level
    float       confidence;    // Decode confidence [0, 1]
} snn_lang_word_result_t;

/** Production result from spike-driven generation */
typedef struct {
    char*    text;             // Generated text (heap-allocated)
    uint32_t word_count;       // Number of words produced
    float    fluency;          // Fluency score [0, 1]
    float    spike_confidence; // Average spike-based word confidence
    float    creativity;       // Creativity contribution from imagination SNN
    /* DK-A+: quantum-Shannon entropy-derived confidence.
     * 1 − (H(p) / log2(K)) over the post-softmax candidate distribution at
     * each produced step, averaged across produced words. Peaked posteriors
     * → near 1.0 (high confidence). Flat posteriors → near 0.0 (low). 0 if
     * no posterior was computed (argmax / mode 0 path). spike_confidence
     * stays for backcompat. */
    float    entropy_confidence;
} snn_lang_production_result_t;

/** Bridge statistics */
typedef struct {
    uint64_t total_decode_calls;
    uint64_t total_encode_calls;
    uint64_t total_produce_calls;
    uint64_t total_comprehend_calls;
    uint64_t total_stdp_updates;
    uint64_t total_ltp_events;
    uint64_t total_ltd_events;
    uint32_t active_bindings;
    float    avg_binding_weight;
    float    avg_word_confidence;
    float    spike_blend_current;
    uint64_t imagination_contributions;
    uint64_t curiosity_contributions;
    uint64_t sleep_consolidation_cycles;
    uint64_t bindings_pruned;
    /* PA-3 walkthrough fix: counter for the collision warning fired by
     * snn_language_bridge_attach_snn_pop when n_neurons > bridge cap.
     * Tested by test_attach_overlarge_pop_warns_but_succeeds. */
    uint64_t attach_collision_warnings;
    /* Tier-4 #16: produce-loop latency telemetry. produce_total_us
     * accumulates wall-clock microseconds spent in bridge_produce across
     * the bridge's lifetime; produce_call_count counts every entry to
     * bridge_produce regardless of return code. Both are measured with
     * CLOCK_MONOTONIC and zeroed by snn_language_bridge_reset_stats. */
    uint64_t produce_total_us;
    uint64_t produce_call_count;
    /* TA-2 — LGSS output-gate telemetry. Bumped only when an
     * lgss_context_t has been attached via
     * snn_language_bridge_set_lgss() and a SAFETY_ACTION_DENY fires
     * against the produced text. Stays 0 when no lgss is attached. */
    uint64_t lgss_outputs_blocked;
    /* TA-4: trigram next-token training counter. Incremented once per
     * successful (a, b) → c update (only when grounded_language_learn_next_token_triple
     * applies its LTP/LTD pass; cold-start no-ops do not count). Zeroed by
     * snn_language_bridge_reset_stats. Reads return 0 when trigram learning
     * was never used. */
    uint64_t total_trigram_updates;
    /* TA-3 — reward-modulated STDP telemetry. Bumped once per
     * snn_language_bridge_apply_stdp pass that found a non-trivial DA
     * gate (config.enable_da_modulation == true AND a neuromodulator
     * system is connected). last_da_modulation captures the multiplier
     * applied this pass — 1.0 when modulation is off / DA is at zero,
     * (1 + da*gain) otherwise. Useful for verifying that dopamine is
     * actually reaching the binding-learning loop. Zeroed by
     * snn_language_bridge_reset_stats. */
    uint64_t da_gated_stdp_passes;
    float    last_da_modulation;
    /* TB-7 — produce-time length-control telemetry. */
    uint64_t length_min_suppressions;
    uint64_t length_max_truncations;
    /* TB-8 — streaming-produce telemetry. stream_callbacks_invoked counts
     * every per-token callback fired; stream_aborts counts produce calls
     * that exited early because the callback returned non-zero. */
    uint64_t stream_callbacks_invoked;
    uint64_t stream_aborts;
    /* TC-11 — cumulative wallclock spent in snn_language_bridge_decode_spikes
     * across all calls in nanoseconds. Combined with total_decode_calls,
     * lets consumers compute average decode latency:
     *   avg_us = (decode_total_ns / total_decode_calls) / 1000
     * Surfaces the actual cost of the CPU decode path so operators can
     * make a cost-benefit call on the GPU port (currently deferred). */
    uint64_t decode_total_ns;
    /* CSTDP — count comprehend calls that fired comprehend-STDP and the
     * number of (concept_pop, word_pop) spike pairs injected. Both stay
     * 0 when enable_comprehend_stdp is false. Useful for verifying the
     * comprehend-driven plasticity path is actually firing during a
     * training run. */
    uint64_t comprehend_stdp_passes;
    uint64_t comprehend_stdp_pairs_fired;
    /* Echo-correct: supervised production training. echo_correct_calls
     * bumps on every snn_language_bridge_echo_correct() invocation;
     * echo_correct_pairs counts the total (concept_pop, target_word_pop)
     * bindings strengthened across all calls; echo_correct_target_misses
     * counts calls where the target word wasn't registered in the bridge
     * (caller fed an unfamiliar word). */
    uint64_t echo_correct_calls;
    uint64_t echo_correct_pairs;
    uint64_t echo_correct_target_misses;
    /* EOS stopping telemetry. total_eos_terminations bumps once per
     * produce call where enable_eos_stopping fired (activation or
     * confidence dropped under threshold after min_produce_words).
     * total_max_truncations bumps once per produce call where the loop
     * exited via the implicit/explicit max-words cap (mirrors the
     * existing length_max_truncations counter for consumers that want
     * a single name). Both stay 0 when EOS stopping is disabled.
     *
     * APPEND-ONLY: these fields MUST stay at the end of snn_lang_stats_t
     * to preserve the offsets of every prior counter for stale-TU callers. The _Static_assert at the bottom of this header makes
     * struct-size drift a compile-time error. */
    uint64_t total_eos_terminations;
    uint64_t total_max_truncations;
    /* Beam re-rank telemetry. beam_hnn_rerank_passes bumps once per
     * produce_beam_search call where enable_beam_hnn_rerank was on AND
     * a HNN net was attached AND the re-rank scaling was actually
     * applied (i.e. fabsf(energy_deviation) > 0). Stays 0 in greedy
     * mode and when the rerank is disabled.
     *
     * APPEND-ONLY: ABI sentinel below pins the new size. */
    uint64_t beam_hnn_rerank_passes;

    /* Wave-3 (2026-05-19) S4 lateral-inhibition telemetry. All zero when
     * enable_lateral_inhibition is false.
     *
     *   lateral_inhibition_decode_calls       — invocations of the K-WTA
     *                                            decode entry point.
     *   lateral_inhibition_winner_margin_sum  — running sum of
     *                                            (a[winner] - a[2nd])
     *                                            across all decode calls;
     *                                            divide by decode_calls for
     *                                            mean margin.
     *   lateral_inhibition_settled_steps_sum  — running sum of micro-steps
     *                                            actually executed before
     *                                            convergence; divide by
     *                                            decode_calls for mean
     *                                            settling speed.
     *   lateral_inhibition_nan_fallbacks      — count of calls that hit a
     *                                            NaN in the activation
     *                                            field mid-settle and fell
     *                                            back to plain argmax. Non-
     *                                            zero indicates numeric
     *                                            instability (gain too high
     *                                            or input out of range).
     *
     * APPEND-ONLY: ABI sentinel at end of header pins new size. */
    uint64_t lateral_inhibition_decode_calls;
    uint64_t lateral_inhibition_winner_margin_sum;   /* fixed-point: margin * 1e6 */
    uint64_t lateral_inhibition_settled_steps_sum;
    uint64_t lateral_inhibition_nan_fallbacks;

    /* Option-1 transport telemetry (Slice A, 2026-05-19).
     *
     * Bridge now counts message volume, not weight updates. Every entry to
     * snn_language_bridge_route_concept_to_word / route_word_to_concept
     * bumps the corresponding counter once. The legacy total_stdp_updates /
     * total_ltp_events / total_ltd_events / comprehend_stdp_* /
     * echo_correct_* / da_gated_stdp_passes counters above remain in the
     * struct for ABI but stay flat at zero — the bridge no longer owns
     * weights to update.
     *
     * APPEND-ONLY: ABI sentinel at end of header pins new size. */
    uint64_t total_spike_routes_concept_to_word;
    uint64_t total_spike_routes_word_to_concept;
} snn_lang_stats_t;

/** Opaque bridge type */
typedef struct snn_language_bridge snn_language_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

/** Get default configuration */
snn_lang_config_t snn_lang_config_default(void);

//=============================================================================
// Lifecycle
//=============================================================================

/** Create SNN language bridge */
snn_language_bridge_t* snn_language_bridge_create(
    const snn_lang_config_t* config);

/** Destroy and free */
void snn_language_bridge_destroy(snn_language_bridge_t* bridge);

/** Reset state (clear activations, keep bindings) */
int snn_language_bridge_reset(snn_language_bridge_t* bridge);

//=============================================================================
// Connection (wire to existing subsystems)
//=============================================================================

/** Connect to grounded language system (required for word lookup) */
int snn_language_bridge_connect_grounded(
    snn_language_bridge_t* bridge,
    struct grounded_language* gl);

/** Connect to imagination SNN for creative word selection */
int snn_language_bridge_connect_imagination(
    snn_language_bridge_t* bridge,
    struct imagination_snn_bridge* imagination);

/** Connect to curiosity SNN for novelty-driven lexical exploration */
int snn_language_bridge_connect_curiosity(
    snn_language_bridge_t* bridge,
    struct curiosity_snn_bridge* curiosity);

/** Connect neuromodulator system for dopamine-gated STDP */
int snn_language_bridge_connect_neuromod(
    snn_language_bridge_t* bridge,
    struct neuromodulator_system_struct* neuromod);

/**
 * @brief Attach an LGSS context for output-gate evaluation in produce.
 *
 * WHAT: Wires the brain's Layered Governance Safety System (LGSS) into
 *       snn_language_bridge_produce(). After the spike cascade has
 *       constructed result->text but before that pointer is returned
 *       to the caller, the bridge evaluates a SAFETY_DOMAIN_GOVERNANCE
 *       action context against the safety KB. A SAFETY_ACTION_DENY
 *       result blocks emission: result->text is freed + zeroed,
 *       result->word_count is reset to 0, stats.lgss_outputs_blocked
 *       is bumped, and an LGSS_ACTION_BLOCKED audit event is emitted.
 * WHY:  Symmetric to the comprehend input gate on grounded_language.
 *       Without an output gate, an attacker who poisons the lexicon or
 *       the imagination drive could exfiltrate disallowed content via
 *       produce even when the input gate is intact.
 * HOW:  Borrowed pointer. NOT owned, NOT serialized. NULL detaches.
 *       Typed void* to avoid pulling the LGSS umbrella header into
 *       language consumers (enum collision risk).
 *
 * @param bridge Bridge handle (NULL → -1)
 * @param lgss   lgss_context_t* opaque (NULL detaches)
 * @return 0 on success, -1 on invalid handle
 */
int snn_language_bridge_set_lgss(
    snn_language_bridge_t* bridge,
    void* lgss);

/**
 * @brief Attach a Hamiltonian (HNN) network for beam re-ranking.
 *
 * WHAT: Borrowed pointer to an lnn_hamiltonian_net_t. When the bridge's
 *       enable_beam_hnn_rerank config flag is true and produce_beam_search
 *       runs, the final beam pick is scaled by
 *         1.0 / (1.0 + beam_hnn_weight * |energy_deviation|)
 *       penalizing beams whose recurrent drift trips the conservation
 *       check.
 * WHY:  Closes audit-item-4 — energy_deviation was computed every step
 *       but never consumed by language. Treats HNN as a re-ranking filter
 *       on already-decoded candidates, not a generation driver.
 * HOW:  NULL detaches. Type-erased to keep the LNN header out of language
 *       consumers' include set.
 *
 * @param bridge Bridge handle (NULL → -1)
 * @param hnn    lnn_hamiltonian_net_t* (NULL detaches)
 * @return 0 on success, -1 on invalid handle
 */
int snn_language_bridge_set_hnn(
    snn_language_bridge_t* bridge,
    void* hnn);

/** Setter for beam HNN re-rank enable + weight. weight clamped [0,100];
 * NaN/inf rejected. Returns 0 on success, -1 on invalid handle. */
int snn_language_bridge_set_beam_hnn_rerank(
    snn_language_bridge_t* bridge,
    bool enabled,
    float weight);

/** Setter for beam length-norm alpha (Wu et al. exponent). alpha clamped
 * [0.1, 1.5]; NaN/inf rejected. Returns 0 on success, -1 on invalid handle. */
int snn_language_bridge_set_beam_length_norm_alpha(
    snn_language_bridge_t* bridge,
    float alpha);

//=============================================================================
// Phase 1: Spike-to-Word Decoding
//=============================================================================

/** Register a concept population (maps concept_id to neuron population) */
int snn_language_bridge_register_concept(
    snn_language_bridge_t* bridge,
    uint32_t concept_pop,
    uint64_t concept_id);

/** Register a word population (maps word string to neuron population) */
int snn_language_bridge_register_word(
    snn_language_bridge_t* bridge,
    uint32_t word_pop,
    const char* word_form);

/** Decode spike patterns to word activations (population vector decoding).
 *  NOTE: transport-only stub since Slice A (returns 0) — kept stubbed so the
 *  on-training-path bigram-learning callers are unchanged. For the opt-in
 *  SNN produce readout use snn_language_bridge_decode_spikes_cached below. */
int snn_language_bridge_decode_spikes(
    snn_language_bridge_t* bridge,
    const float* concept_rates,    // Firing rates per concept pop [num_concept_pops]
    uint32_t num_concept_pops,
    snn_lang_word_result_t* results, // Output: top-k words
    uint32_t max_results,
    uint32_t* num_results);

/** Increment-1 (2026-06-02): opt-in SNN-derived produce readout. Ranks words by
 *  summed Broca spike activity over each word's deterministic neuron ensemble,
 *  read from the per-tick spike cache. Returns 0 results when there is no SNN
 *  signal (caller falls back to the lexicon producer). Used only by the
 *  produce_via_snn path — does NOT affect default training/produce. */
int snn_language_bridge_decode_spikes_cached(
    snn_language_bridge_t* bridge,
    const float* concept_rates,
    uint32_t num_concept_pops,
    snn_lang_word_result_t* results,
    uint32_t max_results,
    uint32_t* num_results);

/** Phase-2 step 3 (2026-06-02): warm-start the concept→word projection from the
 *  grounded-language lexicon. Sets the weight of each (concept ensemble →
 *  word ensemble) projection synapse to k*binding_strength (AMPA-clamped),
 *  updating CSR host weights + GPU. Runtime-safe (weights only, no rewire).
 *  Requires a finalized Broca incoming CSR and a connected grounded_lang.
 *  Returns #synapses updated, or -1 on error. */
int snn_language_bridge_warmstart_projection(
    snn_language_bridge_t* bridge,
    struct snn_network_s* net,
    int wernicke_pop_id,
    int broca_pop_id,
    float k, float lr);   /* lr>=1: overwrite (warm-start); 0<lr<1: EMA (online train) */

/** Train ONE concept↔word association into the SNN projection (2026-06-07): the
 *  unit of "the SNN learns language". Strengthens the projection synapses from
 *  `concept_id`'s Wernicke ensemble to `word_pop`'s Broca ensemble toward
 *  `w_target` (AMPA-clamped) via EMA blend `lr` (lr>=1 = overwrite). Operates on
 *  EXISTING incoming-CSR synapses of the word ensemble — a targeted or dense
 *  projection must already provide them (warm-start/STDP can only set weights,
 *  not create topology). Updates CSR host weights + GPU mirror; runtime-safe
 *  (weights only, no rewire). This is the per-pair primitive the training loop
 *  calls for each taught concept↔word; warmstart_projection is the bulk variant.
 *  Returns #synapses updated, or -1 on error. */
int snn_language_bridge_learn_pair(
    snn_language_bridge_t* bridge,
    struct snn_network_s* net,
    int wernicke_pop_id,
    int broca_pop_id,
    uint64_t concept_id, uint32_t word_pop,
    float w_target, float lr);

/** Wire the TARGETED concept→word projection for ONE pair (2026-06-07): add the
 *  SNN_LANG_NEURONS_PER_POP × SNN_LANG_NEURONS_PER_POP ensemble cross-product
 *  synapses linking concept_id's Wernicke ensemble to word_pop's Broca ensemble,
 *  at initial weight w_init. This is the SCALABLE topology — only registered
 *  pairs get synapses, so coverage is exact at vocab scale, vs the ~0.1-synapse-
 *  per-pair of a 0.15% random sweep (which left ~90% of pairs unwired and made
 *  warmstart/learn_pair no-ops). MUST run BEFORE the Broca incoming CSR is
 *  finalized (snn_csr_add_entry requirement) — i.e. at init / pre-finalize.
 *  Returns #synapses added, or -1 on error. */
int snn_language_bridge_wire_concept_word(
    snn_language_bridge_t* bridge,
    struct snn_network_s* net,
    int wernicke_pop_id,
    int broca_pop_id,
    uint64_t concept_id, uint32_t word_pop,
    float w_init);

/** Build the concept→word projection TOPOLOGY for ALL registered lexicon
 *  bindings (2026-06-09) — the missing "coverage half". Reopens the finalized
 *  Broca incoming CSR (snn_csr_reopen frees the stale GPU mirror), wires every
 *  (concept_id → word_pop) lexicon binding via wire_concept_word, then
 *  re-finalizes. learn_pair/warmstart then set weights supra-threshold. This is
 *  what gives Broca its concept input so the SNN can GENERATE; without it decode
 *  returns 0 and produce falls to the lexicon. CONCURRENCY: mutates live CSR
 *  topology — MUST run with SNN stepping paused (daemon RPC path holds the global
 *  lock). Returns #pairs wired (>0), 0 if no bindings, or -1 on error. */
int snn_language_bridge_build_projection(
    snn_language_bridge_t* bridge,
    struct snn_network_s* net,
    float w_init);

/** Phase-2 produce-time SNN generation step (2026-06-03): seed `concept_ids` into
 *  Wernicke, step the whole SNN n_steps, copy Broca spikes into the decode cache
 *  (then decode_spikes_cached reads intent-driven activity). Clears injection on
 *  exit. COST: snn_network_step advances the FULL network — keep n_steps small.
 *  Default-OFF / opt-in. Returns 0 on success, -1 on error. */
int snn_language_bridge_generate_step(
    snn_language_bridge_t* bridge,
    struct snn_network_s* net,
    int wernicke_pop_id, int broca_pop_id,
    const uint64_t* concept_ids, uint32_t n_concepts,
    uint32_t n_steps, float inject_current);

/** Store the borrowed SNN network + Wernicke/Broca pop ids (2026-06-07) so the
 *  produce path can drive generation via generate_and_decode without
 *  re-discovering them. Call once at init. Returns 0 / -1. */
int snn_language_bridge_connect_network(
    snn_language_bridge_t* bridge,
    struct snn_network_s* net,
    int wernicke_pop_id, int broca_pop_id);

/** Produce-time: seed concept_ids into Wernicke, step the SNN, decode fresh
 *  Broca activity into ranked words — using the handles from connect_network.
 *  This is how the SNN GENERATES from the cascade's content_intent concepts.
 *  -1 if not connected/error; 0 with *num_results (0 = cold/uncovered → caller
 *  falls back to the lexicon producer). */
int snn_language_bridge_generate_and_decode(
    snn_language_bridge_t* bridge,
    const uint64_t* concept_ids, uint32_t n_concepts,
    uint32_t n_steps, float inject_current,
    snn_lang_word_result_t* results,
    uint32_t max_results, uint32_t* num_results);

/** Slice 4: Decode + competitive lateral inhibition over top-K candidates.
 *
 * Pulls the standard top-K from snn_language_bridge_decode_spikes (cosine
 * winners), then iterates a recurrent competition for `lateral_micro_steps`
 * cycles:
 *
 *   for k in [0..K):
 *     new_a[k] = sigmoid(a[k]*gain_self - sum_{j != k} a[j]*gain_inhibit)
 *   a := new_a
 *
 * After settling, the results array is re-sorted by post-competition
 * activation (which now reflects the competitive winner, not just the
 * one-shot cosine pick). `activation` is overwritten with the settled
 * value; `confidence` is recomputed as a[k] / sum(a) so callers reading
 * confidence still see a probability-like quantity.
 *
 * This is the path that cascade_stage_lexical routes through when the
 * config flag `enable_lateral_inhibition` is on. Direct callers of
 * decode_spikes are unaffected.
 *
 * Hyperparameters are read from `bridge->config`:
 *   lateral_gain_self     — self-excitation gain  (default 1.5)
 *   lateral_gain_inhibit  — per-other inhibition  (default ~0.026)
 *   lateral_micro_steps   — settling iterations   (default 20)
 *
 * Stability: activations are bounded to [0, 1] via sigmoid each step.
 * If a NaN/Inf appears (shouldn't, given the bounded transfer) the
 * function falls back to returning the pre-competition top-K unchanged
 * and logs a one-shot warning. */
int snn_language_bridge_decode_with_lateral_inhibition(
    snn_language_bridge_t* bridge,
    const float* concept_rates,
    uint32_t num_concept_pops,
    snn_lang_word_result_t* results,
    uint32_t max_results,
    uint32_t* num_results);

/** Encode a word as concept neuron activation pattern */
int snn_language_bridge_encode_word(
    snn_language_bridge_t* bridge,
    uint32_t word_pop,
    float* concept_activations,    // Output: concept activations [num_concept_pops]
    uint32_t num_concept_pops);

/*=============================================================================
 * Option-1 transport API (Slice A)
 *
 * Pure transport: forward an active list of concept pop ids into the
 * corresponding word pops (and vice-versa) via the SNN's own projection
 * synapses. The bridge does NOT own weights — projection synapses live in
 * the SNN (Slice B wires them through the concept_registry).
 *
 * Until Slice B's concept_registry is merged, these functions are stubs
 * that fire the same pop_id on the other side (identity mapping). Tests
 * verify the function exists, accepts the inputs, returns successfully —
 * not that it produces the right mapping (that's a joint Slice B + A
 * test run during walkthroughs).
 *===========================================================================*/

/** Route a list of active concept pop ids to the corresponding word pop ids.
 *
 * @param bridge            Bridge handle.
 * @param concept_pop_ids   Active concept population ids (input).
 * @param n_concepts        Length of concept_pop_ids.
 * @param word_pop_ids_out  Output buffer for routed word pop ids.
 * @param n_words_out       On success: number of word pops written.
 * @param max_out           Capacity of word_pop_ids_out (0 → query only,
 *                          *n_words_out gets the would-be size).
 * @return 0 on success, -1 on invalid arguments.
 */
int snn_language_bridge_route_concept_to_word(
    snn_language_bridge_t* bridge,
    const uint32_t* concept_pop_ids, size_t n_concepts,
    uint32_t* word_pop_ids_out, size_t* n_words_out, size_t max_out);

/** Route a list of active word pop ids to the corresponding concept pop ids.
 *
 * @param bridge              Bridge handle.
 * @param word_pop_ids        Active word population ids (input).
 * @param n_words             Length of word_pop_ids.
 * @param concept_pop_ids_out Output buffer for routed concept pop ids.
 * @param n_concepts_out      On success: number of concept pops written.
 * @param max_out             Capacity of concept_pop_ids_out (0 → query only).
 * @return 0 on success, -1 on invalid arguments.
 */
int snn_language_bridge_route_word_to_concept(
    snn_language_bridge_t* bridge,
    const uint32_t* word_pop_ids, size_t n_words,
    uint32_t* concept_pop_ids_out, size_t* n_concepts_out, size_t max_out);

//=============================================================================
// DEPRECATED — Phase 2 STDP / binding-learning APIs
//=============================================================================
/* Option-1 (Slice A, 2026-05-19): the bridge no longer owns a
 * concept_pop × word_pop weight matrix. The functions below are kept as
 * NO-OP stubs that return success so existing callers continue to link;
 * they do nothing and have no observable side effect. New code MUST use
 * snn_language_bridge_route_concept_to_word / route_word_to_concept.
 *
 * Slice B (concept_registry) will migrate the remaining callers in
 * grounded_language.c off these APIs entirely; at that point this whole
 * section can be deleted. Until then, the symbols stay for ABI stability.
 */

/** DEPRECATED (Slice A): no-op stub. Returns 0. */
int snn_language_bridge_concept_spike(
    snn_language_bridge_t* bridge,
    uint32_t concept_pop,
    float spike_time_ms);

/** DEPRECATED (Slice A): no-op stub. Returns 0. */
int snn_language_bridge_word_spike(
    snn_language_bridge_t* bridge,
    uint32_t word_pop,
    float spike_time_ms);

/** DEPRECATED (Slice A): no-op stub. Returns 0. Bridge owns no weights. */
int snn_language_bridge_apply_stdp(
    snn_language_bridge_t* bridge,
    float current_time_ms);

/** DEPRECATED (Slice A): no-op stub. Returns 0. */
int snn_language_bridge_bind(
    snn_language_bridge_t* bridge,
    uint32_t concept_pop,
    uint32_t word_pop,
    float initial_weight);

/** DEPRECATED (Slice A): no-op stub. Returns 0. Bridge owns no weights. */
int snn_language_bridge_strengthen_binding(
    snn_language_bridge_t* bridge,
    uint32_t concept_pop,
    uint32_t word_pop,
    float delta);

/** DEPRECATED (Slice A): no-op stub. Returns 0 (zero bindings strengthened). */
int snn_language_bridge_echo_correct(
    snn_language_bridge_t* bridge,
    const float* intent,
    uint32_t intent_dim,
    const char* target_word_form,
    float lr_scale);

/** DEPRECATED (Slice A): no-op stub. Returns 0. */
int snn_language_bridge_strengthen_binding_riemannian(
    snn_language_bridge_t* bridge,
    uint32_t concept_pop,
    uint32_t word_pop,
    float grad);

/** DEPRECATED (Slice A): no-op stub. Returns 0. Bridge has no weights to prune. */
int snn_language_bridge_prune(
    snn_language_bridge_t* bridge,
    float threshold);

//=============================================================================
// Phase 3: Spike-Driven Language Production (Broca pathway)
//=============================================================================

/** Produce text from semantic intent via spike cascade */
int snn_language_bridge_produce(
    snn_language_bridge_t* bridge,
    const float* semantic_intent,
    uint32_t intent_dim,
    snn_lang_production_result_t* result);

/** Produce a single word from current concept activations */
int snn_language_bridge_produce_word(
    snn_language_bridge_t* bridge,
    const float* concept_activations,
    uint32_t num_concepts,
    snn_lang_word_result_t* result);

/** Cleanup production result */
void snn_lang_production_result_cleanup(snn_lang_production_result_t* result);

//=============================================================================
// Phase 4: Spike-Driven Comprehension (Wernicke pathway)
//=============================================================================

/** Comprehend text by cascading word spikes to concept activations */
int snn_language_bridge_comprehend(
    snn_language_bridge_t* bridge,
    const char* text,
    float* concept_activations,    // Output: activated concepts
    uint32_t max_concepts,
    uint32_t* num_activated,
    float* comprehension_confidence);

//=============================================================================
// Phase 5: Creative/Imagination Integration
//=============================================================================

/** Feed imagination SNN output through binding matrix for creative words */
int snn_language_bridge_creative_produce(
    snn_language_bridge_t* bridge,
    const float* imagination_activations,
    uint32_t num_dims,
    float creativity_level,
    snn_lang_production_result_t* result);

/** Feed curiosity drive into lexical exploration (bias toward novel words) */
int snn_language_bridge_curiosity_modulate(
    snn_language_bridge_t* bridge,
    float novelty_level,
    float exploration_drive);

//=============================================================================
// Phase 6: Sleep Consolidation
//=============================================================================

/** Consolidate bindings during sleep (replay + prune) */
int snn_language_bridge_sleep_consolidate(
    snn_language_bridge_t* bridge,
    float consolidation_strength);

//=============================================================================
// Statistics & Introspection
//=============================================================================

/** Get bridge statistics */
int snn_language_bridge_get_stats(
    const snn_language_bridge_t* bridge,
    snn_lang_stats_t* stats);

/** Reset statistics counters */
int snn_language_bridge_reset_stats(snn_language_bridge_t* bridge);

/** Tier-4 #15: copy the entire bridge configuration into *out.
 *
 * Operators today have setters for every PA/MQ knob (blend, sampling,
 * glove_blend, autoregressive, spike_routing, hyperbolic_embeddings,
 * sampling_mode) but no consolidated getter — they must read source to
 * answer questions like "is autoregressive enabled? what's the
 * temperature?". This getter copies the live `snn_lang_config_t` out so
 * RPC consumers can introspect any/all fields uniformly.
 *
 * @return 0 on success; -1 if bridge or out is NULL or magic mismatched. */
int snn_language_bridge_get_config(
    const snn_language_bridge_t* bridge,
    snn_lang_config_t* out);

/** Tier-4 #16: average wall-clock microseconds per bridge_produce call.
 *
 * Computed from stats.produce_total_us / stats.produce_call_count. Returns
 * 0.0 when no produce calls have been made (or bridge is invalid) so the
 * caller can divide blindly without a guard. The raw counters live in
 * snn_lang_stats_t (already exposed by snn_language_bridge_get_stats). */
float snn_language_bridge_get_avg_produce_us(
    const snn_language_bridge_t* bridge);

/** Tier-4 #17: explicit RNG seed for deterministic sampling tests.
 *
 * The bridge's xorshift64* RNG state is seeded at create() from
 * (time XOR pointer-mix), which is fine for production but makes tests
 * that exercise softmax/top-p/q-MC sampling flaky on slow runners. This
 * setter writes `seed` directly into the RNG state. xorshift64 requires a
 * non-zero state, so seed=0 is remapped to 1 (caller does not need to
 * special-case it).
 *
 * @return 0 on success; -1 if bridge is NULL or magic mismatched. */
int snn_language_bridge_set_rng_seed(
    snn_language_bridge_t* bridge,
    uint64_t seed);

/** DEPRECATED (Slice A): no-op stub. Returns 0. Trigram learning, if still
 *  wanted, lives in grounded_language now. */
int snn_language_bridge_set_trigram_learning_enabled(
    snn_language_bridge_t* bridge,
    bool enabled);

/** DEPRECATED (Slice A): always returns false. */
bool snn_language_bridge_get_trigram_learning_enabled(
    const snn_language_bridge_t* bridge);

/** DEPRECATED (Slice A): no-op stub. Returns 0. */
int snn_language_bridge_set_ltd_margin(
    snn_language_bridge_t* bridge,
    float margin);

/** DEPRECATED (Slice A): always returns 0.0f. */
float snn_language_bridge_get_ltd_margin(
    const snn_language_bridge_t* bridge);

/** Slice 4: toggle lateral-inhibition decode path.
 *
 * Default OFF — preserves bit-for-bit legacy decode_spikes behavior. When
 * enabled, callers that route through
 * snn_language_bridge_decode_with_lateral_inhibition (notably
 * cascade_stage_lexical when the cascade routes through the bridge) get
 * the recurrent-competition winner instead of the one-shot cosine
 * argmax. Direct decode_spikes callers are unaffected by this flag.
 *
 * Runtime-only — not persisted in the V5 sidecar. Caller must re-apply
 * after each load.
 *
 * @return 0 on success; -1 if bridge is NULL/invalid. */
int snn_language_bridge_set_lateral_inhibition_enabled(
    snn_language_bridge_t* bridge,
    bool enabled);

/** Slice 4: read the lateral-inhibition runtime flag. Returns false if
 *  bridge is NULL/invalid. */
bool snn_language_bridge_get_lateral_inhibition_enabled(
    const snn_language_bridge_t* bridge);

/** Slice 4: tune the recurrent-competition dynamics.
 *
 * gain_self ~ self-excitation per micro-step. > 0 (default 1.5). High
 *   values can saturate the sigmoid early.
 * gain_inhibit ~ per-other inhibition coefficient. > 0 (default
 *   ~0.026 ~= 0.8/(K-1) for K=32). Multiplied by the sum of every
 *   other candidate's activation at each step.
 * micro_steps ~ settling iterations (default 20). Capped at 200 to
 *   prevent runaway compute.
 *
 * All three values are validated; any non-finite or out-of-range input
 * is rejected without mutating state.
 *
 * Runtime-only — not persisted in the V5 sidecar.
 *
 * @return 0 on success; -1 if bridge invalid or any value out of range. */
int snn_language_bridge_set_lateral_inhibition_params(
    snn_language_bridge_t* bridge,
    float gain_self,
    float gain_inhibit,
    uint32_t micro_steps);

/** Slice 4: read the lateral-inhibition tunables. NULL out-pointers are
 *  skipped. Returns -1 on invalid bridge. */
int snn_language_bridge_get_lateral_inhibition_params(
    const snn_language_bridge_t* bridge,
    float* out_gain_self,
    float* out_gain_inhibit,
    uint32_t* out_micro_steps);

/** DEPRECATED (Slice A): no-op stub. Returns 0. Bridge owns no weights. */
int64_t snn_language_bridge_reset_weights(
    snn_language_bridge_t* bridge,
    float w_min,
    float w_max);

/** TB-8: per-token streaming callback. Invoked once per emitted word
 *  during snn_language_bridge_produce. Returning non-zero aborts the
 *  produce loop early (text accumulated so far is preserved in
 *  result->text on return; word_count reflects what was emitted before
 *  the abort). user_data is passed through unchanged.
 *
 *  IMPORTANT: the callback is a strict observer. It MUST NOT hold the
 *  bridge mutex (the caller already does), MUST NOT call any other
 *  snn_language_bridge_* API, and MUST NOT free word_form (borrowed,
 *  valid only for the duration of the call). Treat it as fire-and-
 *  forget — pipe to TTS, log, update a UI, then return. */
typedef int (*snn_lang_stream_callback_t)(
    uint32_t      word_index,        /* 0-based emission index */
    const char*   word_form,         /* borrowed; valid only during callback */
    uint32_t      word_pop,          /* SNN word population id */
    float         confidence,        /* per-token confidence [0,1] */
    void*         user_data);

/** TB-8: attach (or detach) a per-token streaming callback for produce.
 *
 * The callback + user_data are stored on the bridge as per-call runtime
 * state — they are NOT persisted in the V3 sidecar and NOT in the
 * config struct. Pass cb=NULL to detach. Detaching from inside a
 * callback is safe (the next produce call sees the new state).
 *
 * @param bridge    Bridge handle (NULL → -1).
 * @param cb        Streaming callback, or NULL to detach.
 * @param user_data Opaque pointer passed through to every callback
 *                  invocation. Bridge does not free it.
 * @return 0 on success, -1 on invalid handle.
 */
int snn_language_bridge_set_stream_callback(
    snn_language_bridge_t* bridge,
    snn_lang_stream_callback_t cb,
    void* user_data);

/** DEPRECATED (Slice A): no-op stub. Bridge has no trigram counter to bump. */
void snn_language_bridge_inc_trigram_updates(snn_language_bridge_t* bridge);

/** DEPRECATED (Slice A): no-op stub. Returns 0. DA modulation on bridge
 *  weights is meaningless because the bridge has no weights. DA broadcast
 *  is still received via connect_neuromod and forwarded to the SNN. */
int snn_language_bridge_set_da_modulation_enabled(
    snn_language_bridge_t* bridge,
    bool enabled);

/** DEPRECATED (Slice A): always returns false. */
bool snn_language_bridge_get_da_modulation_enabled(
    const snn_language_bridge_t* bridge);

/** DEPRECATED (Slice A): no-op stub. Returns 0. Comprehend is read-only
 *  transport now; no bridge-side weight updates fire. */
int snn_language_bridge_set_comprehend_stdp_enabled(
    snn_language_bridge_t* bridge,
    bool enabled);

/** DEPRECATED (Slice A): always returns false. */
bool snn_language_bridge_get_comprehend_stdp_enabled(
    const snn_language_bridge_t* bridge);

/** DEPRECATED (Slice A): no-op stub. Returns 0. */
int snn_language_bridge_set_da_modulation_gain(
    snn_language_bridge_t* bridge,
    float gain);

/** Get current spike blend factor */
float snn_language_bridge_get_blend(const snn_language_bridge_t* bridge);

/** Get word form string for a given word population index (NULL if invalid/unregistered) */
const char* snn_language_bridge_get_word_form(
    const snn_language_bridge_t* bridge,
    uint32_t word_pop_index);

/** Set spike blend factor [0=all vector, 1=all spike] */
void snn_language_bridge_set_blend(snn_language_bridge_t* bridge, float blend);

/** PA-6: Configure produce-time sampling.
 *
 * @param temperature  0 = argmax (legacy / default). >0 = softmax sampling
 *                     over top-K cosine-scored candidates with this T.
 * @param top_p        Nucleus truncation in [0,1]. 1.0 = no truncation.
 *                     Smaller values keep only the highest-probability mass.
 * @return 0 on success; -1 if bridge invalid or args out of range.
 */
int snn_language_bridge_set_sampling(snn_language_bridge_t* bridge,
                                      float temperature, float top_p);

/** PA-5: word → embedding lookup callback. Caller fills `out_vec` with
 * the embedding (length `out_dim`) for `word_form` and returns 0; returns
 * -1 if the word is not in the embedding table. Called by decode_spikes
 * (lazily, with caching) when glove_blend > 0. Must be thread-safe with
 * respect to the embedding table. */
typedef int (*snn_lang_word_emb_fn)(void* ctx,
                                     const char* word_form,
                                     float* out_vec,
                                     uint32_t out_dim);

/** PA-5: attach embedding lookup. Until this is called, glove_blend has
 * no effect. emb_dim must equal the prefix length of concept_rates that
 * carries the embedding signal (in NIMCP, semantic_dim == emb_dim, so
 * pass gl->semantic_dim). The first call also allocates the bridge's
 * word_emb cache (sized word_pops_capacity × emb_dim). Pass NULL fn to
 * detach, which frees the cache. */
int snn_language_bridge_set_embedding_lookup(snn_language_bridge_t* bridge,
                                              snn_lang_word_emb_fn fn,
                                              void* ctx,
                                              uint32_t emb_dim);

/** PA-5: set the GloVe blend coefficient at runtime. blend in [0, 1];
 * 0 = binding-only (PA-1 default), 1 = embedding-only ranking.
 * Returns -1 if bridge invalid or blend out of range. */
int snn_language_bridge_set_glove_blend(snn_language_bridge_t* bridge,
                                         float blend);

/** PA-5: invalidate the per-word embedding cache. Call after the
 * embedding table changes (rare — only on retraining or model swap). */
int snn_language_bridge_invalidate_emb_cache(snn_language_bridge_t* bridge);

/** PA-5+: select the GloVe distance metric.
 *
 * @param enabled  false (default) → Euclidean cosine (PA-5 legacy).
 *                 true → Poincaré-ball hyperbolic distance, mapped to
 *                        1 / (1 + d_H(query, word_emb)) so the score
 *                        keeps the "larger is better" convention.
 * Only takes effect when an embedding lookup is attached and
 * glove_blend > 0. Switching the mode invalidates the per-word emb cache
 * (the projected hyperbolic representation is cached alongside).
 *
 * @return 0 on success, -1 if bridge invalid.
 */
int snn_language_bridge_set_hyperbolic_embeddings(snn_language_bridge_t* bridge,
                                                   bool enabled);

/** PA-6+: select produce-time sampling mode.
 *
 * @param mode  0 = legacy (argmax when temperature == 0; softmax+top-p
 *              otherwise — equivalent to PA-6 dispatch).
 *              1 = force softmax + nucleus top-p (PA-6).
 *              2 = quantum-Monte-Carlo MCMC sampling (q-MC).
 * Modes 1 and 2 require a pre-set temperature > 0 to seed the
 * candidate distribution.
 *
 * @return 0 on success, -1 if bridge invalid or mode out of range.
 */
int snn_language_bridge_set_sampling_mode(snn_language_bridge_t* bridge,
                                            int mode);

/** TIER1-A: configure optional beam-K decoding in snn_language_bridge_produce.
 *
 * @param k  Beam width. 1 (default) = greedy / legacy bit-for-bit. > 1 keeps
 *           K parallel beams with cumulative log-prob and length-normalized
 *           ranking (logprob / token_count^0.6). Capped at 16. 0 is treated
 *           as 1.
 *
 * @return 0 on success, -1 if bridge invalid.
 */
int snn_language_bridge_set_beam_width(snn_language_bridge_t* bridge,
                                        uint32_t k);

/** TIER1-B: register the end-of-utterance word_pop. When sampled inside
 * the produce loop, generation halts cleanly (the EOS pop itself is NOT
 * appended to the output text).
 *
 * @param pop  Word_pop index, or UINT32_MAX (default) to disable EOS.
 * @return 0 on success, -1 if bridge invalid.
 */
int snn_language_bridge_set_eos_word_pop(snn_language_bridge_t* bridge,
                                          uint32_t pop);

/** TB-7: hard length control on snn_language_bridge_produce.
 *
 * @param min_words  When > 0, EOS picks (eos_word_pop) are suppressed and
 *                   replaced with the next-best non-EOS candidate until at
 *                   least this many words have been emitted. Each
 *                   suppression bumps stats.length_min_suppressions.
 *                   0 (default) preserves legacy EOS-first behavior.
 * @param max_words  When > 0, the produce loop terminates cleanly after
 *                   this many words have been emitted, overriding the
 *                   legacy implicit 32-word cap. Hitting the cap bumps
 *                   stats.length_max_truncations once. 0 (default) keeps
 *                   the legacy 32-word cap.
 *
 * Validation: NULL bridge → -1. Each value clamped to [0, 1024]. When BOTH
 * are nonzero, min_words > max_words is rejected (-1, no state change).
 * Either value may be 0 (disabled sentinel) regardless of the other.
 *
 * @return 0 on success, -1 on validation failure.
 */
int snn_language_bridge_set_length_control(snn_language_bridge_t* bridge,
                                            uint32_t min_words,
                                            uint32_t max_words);

/** TB-7: read the current length-control settings.
 *
 * Copies config.min_produce_words / config.max_produce_words into the
 * out-pointers (NULL out-pointers are skipped). Returns -1 on invalid
 * bridge.
 */
int snn_language_bridge_get_length_control(const snn_language_bridge_t* bridge,
                                            uint32_t* min_words,
                                            uint32_t* max_words);

/** TIER1-C: configure the n-gram repetition penalty applied per produce step.
 *
 * Before scoring/sampling candidates, every candidate whose word_pop appears
 * in the last `window` picks has its score multiplied by `(1 - penalty)` for
 * each match. With penalty = 0 (default) the path is a no-op.
 *
 * @param penalty  In [0, 1]. 0 disables. Clamped if out of range.
 * @param window   Look-back length. 0 falls back to 3 when penalty > 0.
 * @return 0 on success, -1 if bridge invalid.
 */
int snn_language_bridge_set_repetition_penalty(snn_language_bridge_t* bridge,
                                                 float penalty,
                                                 uint32_t window);

/** PA-2: configure the autoregressive recurrent decoder.
 *
 * @param intent_persistence  In [0, 1]. 0 (default) = legacy behavior — the
 *                            original intent decays exponentially across
 *                            the produce loop as state evolves toward the
 *                            most recent words. 1 = pure non-recurrent
 *                            (intent stays full-strength every step,
 *                            ignoring state). Values in between blend.
 * @param word_feedback       In [0, 1]. How aggressively each just-picked
 *                            word reshapes the recurrent state. Default
 *                            0.3 (matches the legacy hard-coded blend).
 *                            Higher = stronger context dependence.
 * @return 0 on success; -1 if bridge invalid or args out of range.
 */
int snn_language_bridge_set_autoregressive(snn_language_bridge_t* bridge,
                                            float intent_persistence,
                                            float word_feedback);

/** PA-3: role of an attached SNN population. CONCEPT routes spikes through
 * snn_language_bridge_concept_spike (Wernicke / arcuate / comprehension
 * tier). WORD routes through snn_language_bridge_word_spike (Broca /
 * production tier). */
typedef enum {
    SNN_LANG_POP_ROLE_CONCEPT = 0,
    SNN_LANG_POP_ROLE_WORD    = 1
} snn_lang_pop_role_t;

/** PA-3: configure SNN-spike routing.
 *
 * @param enabled    Master gate. False (default) disables the entire path
 *                   to prevent the previously-observed sparsity collapse.
 * @param tau_ms     Per-tick decay time constant for activation accumulators.
 *                   Must be > 0 when enabled is true. Suggested 200 ms.
 * @return 0 on success; -1 if bridge invalid or tau invalid given enabled.
 */
int snn_language_bridge_set_snn_spike_routing(snn_language_bridge_t* bridge,
                                               bool enabled, float tau_ms);

/** PA-3: register an SNN population for spike routing through this bridge.
 * Up to SNN_LANG_MAX_ATTACHED_POPS attached at once. Re-attaching with a
 * known pop_id updates the role.
 * @return 0 on success; -1 if bridge invalid or attach table full. */
int snn_language_bridge_attach_snn_pop(snn_language_bridge_t* bridge,
                                        int snn_pop_id,
                                        uint32_t n_neurons,
                                        snn_lang_pop_role_t role);

/** PA-3: drain spike_output for one attached SNN pop and route each fired
 * neuron through concept_spike or word_spike according to its registered
 * role. Bridge-side neuron→pop mapping is `neuron_idx % MAX_*_POPS`.
 * @param spike_output   spike_output[] from the SNN pop, length n_neurons.
 * @param current_time_ms wallclock timestamp of the current tick (for STDP).
 * @return 0 on success; -1 if pop_id not registered or bridge invalid. */
int snn_language_bridge_drain_pop_spikes(snn_language_bridge_t* bridge,
                                          int snn_pop_id,
                                          const float* spike_output,
                                          uint32_t n_neurons,
                                          float current_time_ms);

/** PA-3: per-tick decay on activation accumulators. Must be called once
 * per global tick (cadence ~10 ms) when spike routing is enabled, or the
 * activations diverge.
 * @return 0 on success; -1 if bridge invalid. */
int snn_language_bridge_tick(snn_language_bridge_t* bridge, float dt_ms);

/** PA-3: iterate attached SNN pops. Returns -1 on invalid bridge or out-of-
 * range index; 0 with `*pop_id < 0` indicates an empty slot to skip. */
int snn_language_bridge_get_attached_pop(const snn_language_bridge_t* bridge,
                                          uint32_t index,
                                          int* out_pop_id,
                                          uint32_t* out_n_neurons,
                                          snn_lang_pop_role_t* out_role);

//=============================================================================
// Serialization
//=============================================================================

/** Save binding weights and configuration */
int snn_language_bridge_save(const snn_language_bridge_t* bridge, const char* path);

/** Load binding weights and configuration */
snn_language_bridge_t* snn_language_bridge_load(const char* path);

/** Recompute per-word_pop binding-weight L2 norm cache (Σ weight² per word_pop)
 * from the current binding state. Used after bulk binding changes and on
 * bridge load. Cosine-normalized decode_spikes consults this cache to remove
 * binding-density rank-1 bias. Cheap O(num_bindings); idempotent. */
int snn_language_bridge_recompute_norms(snn_language_bridge_t* bridge);

//=============================================================================
// Phase 8.5: Top-Down Binding -> Perception Attention Feedback
//=============================================================================

/**
 * @brief Generate top-down attention signal from concept bindings
 * WHAT: Convert active concept bindings into perception attention weights
 * WHY:  Enable language understanding to guide visual/auditory attention
 * HOW:  Strong bindings -> high attention for associated sensory populations
 */
int snn_language_bridge_generate_attention_feedback(
    snn_language_bridge_t* bridge,
    float* attention_weights,
    uint32_t num_weights
);

/**
 * @brief Apply top-down prediction to sensory input
 * WHAT: Use concept predictions to modulate expected sensory patterns
 * WHY:  Predictive coding - reduce prediction error at sensory level
 * HOW:  Active concepts generate expected spike patterns, compared to actual
 */
int snn_language_bridge_predict_sensory(
    snn_language_bridge_t* bridge,
    const float* concept_activations,
    uint32_t num_concepts,
    float* predicted_sensory,
    uint32_t sensory_dim
);

/* ABI size sentinels.
 *
 * Why: the previous EOS-stopping commit (revert 65337cdbc) crashed with
 * "stack smashing detected" in tests that had been compiled against the
 * pre-EOS struct layout while the library was rebuilt with the larger
 * layout. The library wrote a larger config/stats blob into a stack-
 * allocated caller buffer sized for the older layout (via
 *   *out = bridge->config; *stats = bridge->stats;
 * in snn_language_bridge_get_config / get_stats).
 *
 * These _Static_assert sentinels turn that runtime stack-smash into a
 * clear compile error: any TU compiled with a stale layout will fail
 * the static-assert at -E preprocessing and refuse to produce a .o.
 * Bump the literal here whenever a field is appended to either struct.
 *
 * The numbers below are computed on x86_64 Linux gcc with -fno-pic and
 * default struct packing (verified via build/sizecheck). Bumping a
 * field adds the field's size (rounded up to alignment) — the matching
 * change to the literal MUST land in the same commit so reviewers can
 * see the ABI delta. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#include <stddef.h>
_Static_assert(sizeof(snn_lang_config_t) == 188,
    "snn_lang_config_t ABI: size drifted from expected 188 bytes. "
    "Append-only on the struct; bump this literal in lockstep.");
_Static_assert(sizeof(snn_lang_stats_t) == 312,
    "snn_lang_stats_t ABI: size drifted from expected 312 bytes. "
    "Append-only on the struct; bump this literal in lockstep. "
    "Slice A (2026-05-19) appended 2 spike-route counters (16 bytes) -> 312.");
#endif

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_LANGUAGE_BRIDGE_H */
