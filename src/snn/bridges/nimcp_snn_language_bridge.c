//=============================================================================
// nimcp_snn_language_bridge.c - SNN ↔ Language Transport Bridge (Option-1)
//=============================================================================
/**
 * @file nimcp_snn_language_bridge.c
 * @brief Implementation of the Option-1 transport-only SNN↔language bridge.
 *
 * Slice A (2026-05-19): the bridge no longer holds a concept_pop × word_pop
 * weight matrix or applies STDP. All learning has moved into the SNN's
 * own projection synapses and the lexicon's concept_registry (Slice B).
 * This TU keeps the public API stable: every former learning function is
 * still here as a no-op stub that returns success, so existing callers
 * in grounded_language.c / cascade.c / etc. continue to link cleanly
 * until Slice B migrates them off the deprecated names.
 *
 * Pure transport stays:
 *   - register_concept / register_word (population bookkeeping)
 *   - route_concept_to_word / route_word_to_concept (NEW transport API,
 *     currently identity-mapping stubs; Slice B will swap in the registry)
 *   - decode_with_lateral_inhibition (K-WTA over routed pops, local dynamics)
 *   - produce / comprehend (consume the transport)
 *   - connection lifecycle + LGSS attachment
 */

#include "snn/bridges/nimcp_snn_language_bridge.h"
#include "language/nimcp_grounded_language.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/geometry/nimcp_hyperbolic.h"
#include "utils/quantum/nimcp_quantum_monte_carlo.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "snn/nimcp_snn_network.h"   /* Phase-2 warm-start: pop + CSR access */
#include "snn/nimcp_snn_synapse.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <float.h>

#define LOG_MODULE "SNN_LANG_BRIDGE"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_language_bridge)

//=============================================================================
// Internal: Sparse binding hash map
//=============================================================================

#define BINDING_HASH_BUCKETS 8192
#define BINDING_HASH_MASK    (BINDING_HASH_BUCKETS - 1)

typedef struct binding_node {
    snn_lang_binding_t binding;
    struct binding_node* next;
} binding_node_t;

//=============================================================================
// Internal: Population registration
//=============================================================================

typedef struct {
    uint64_t concept_id;
    bool     registered;
    float    last_spike_ms;
    float    activation;       // Accumulated activation
} concept_pop_info_t;

typedef struct {
    char     word_form[64];
    bool     registered;
    float    last_spike_ms;
    float    activation;
} word_pop_info_t;

//=============================================================================
// Bridge structure
//=============================================================================

struct snn_language_bridge {
    uint32_t magic;
    snn_lang_config_t config;

    // Population registrations
    concept_pop_info_t* concept_pops;
    uint32_t num_concept_pops;
    uint32_t concept_pops_capacity;

    word_pop_info_t* word_pops;
    uint32_t num_word_pops;
    uint32_t word_pops_capacity;

    // Sparse binding hash map
    binding_node_t* binding_buckets[BINDING_HASH_BUCKETS];
    uint32_t num_bindings;

    /* Patch A: per-word_pop sum of squared binding weights, for cosine
     * normalization in decode_spikes. Sized to word_pops_capacity. Maintained
     * incrementally on every binding mutation (binding_insert new + max-merge,
     * STDP weight update). decode_spikes divides word_acts[w] by sqrt(this+ε)
     * to remove rank-1 binding-density bias. Without this, words that have
     * accumulated more bindings (curriculum-frequent tokens) win every rank
     * regardless of input semantic vector. */
    float* word_norm_sq;

    /* PA-6: xorshift64* RNG state for produce-time softmax sampling. Seeded
     * once at create() with time XOR pointer mix. Only consulted when
     * config.temperature > 0; deterministic argmax path bypasses it. */
    uint64_t rng_state;

    /* PA-5: GloVe lookup callback + lazy per-word cache. emb_dim and the
     * cache buffers are NULL/zero until set_embedding_lookup() runs.
     *   word_emb_cache:   word_pops_capacity * emb_dim floats, row-major.
     *   word_emb_cached:  bool flag per word_pop — true once filled.
     *   word_emb_norm:    cached ‖emb[w]‖ per word_pop for cosine.
     * Cache is invalidated by snn_language_bridge_invalidate_emb_cache()
     * (e.g. after embedding-table retrain) and by detaching the lookup. */
    snn_lang_word_emb_fn emb_lookup_fn;
    void*               emb_lookup_ctx;
    uint32_t            emb_dim;
    float*              word_emb_cache;
    uint8_t*            word_emb_cached;
    float*              word_emb_norm;

    /* PA-3: SNN-spike routing — table of attached SNN pops + their roles.
     * Drain loop iterates this table once per tick. snn_pop_id < 0 marks
     * an empty slot. */
    struct {
        int                 snn_pop_id;
        uint32_t            n_neurons;
        snn_lang_pop_role_t role;
    } attached_pops[SNN_LANG_MAX_ATTACHED_POPS];
    uint32_t                n_attached_pops;

    /* Borrowed SNN handles for produce-time generation (set by
     * connect_network). generate_step needs the network + the Wernicke/Broca
     * pop ids, and attached_pops[] can't distinguish Wernicke from Arcuate
     * (both CONCEPT-role), so the produce path can't recover them itself.
     * Borrowed pointer — not owned, not freed. -1 ids = not connected. */
    struct snn_network_s*   gen_net;
    int                     gen_wernicke_pop_id;
    int                     gen_broca_pop_id;

    /* Walkthrough round 2: PA-2 × PA-5 interaction fix. The autoregressive
     * decoder evolves concept_acts into binding space (state drifts toward
     * encode_word(picked) bindings). The GloVe cosine in decode_spikes uses
     * concept_rates[0:emb_dim] as the embedding query — which is corrupt
     * once state diverges from intent. produce sets emb_query_override to
     * the immutable original intent before each decode_spikes call, and
     * resets it on exit. NULL = use concept_rates (legacy / no autoreg). */
    const float*            emb_query_override;
    uint32_t                emb_query_override_dim;

    // Connected subsystems
    struct grounded_language* grounded_lang;
    struct imagination_snn_bridge* imagination;
    struct curiosity_snn_bridge* curiosity;
    struct neuromodulator_system_struct* neuromod;

    /* TA-2: LGSS output gate. Borrowed pointer; cast to lgss_context_t*
     * at the call site via forward decl. NULL = no-op (legacy default,
     * preserves behavior for callers that haven't attached one). */
    void* lgss;

    /* Beam-HNN re-rank: borrowed pointer to an lnn_hamiltonian_net_t.
     * Used only by produce_beam_search when config.enable_beam_hnn_rerank
     * is true. NULL = no-op, behaves identically to a disabled flag.
     * Type-erased to keep the LNN header out of this TU. */
    void* hnn;

    // Current time
    float current_time_ms;

    /* Increment-1 (2026-06-02): cached latest Broca (WORD-role) spike_output,
     * copied each tick by drain_pop_spikes. decode_spikes reads this to rank
     * words from the running SNN's activity WITHOUT a synchronous full-SNN step
     * or a network handle (the main SNN is only stepped during training). Lazily
     * sized to the WORD pop's n_neurons. Borrowed-data-free: owns its buffer. */
    float*   broca_spike_cache;
    uint32_t broca_spike_cache_cap;

    /* TA-4: runtime-only flag (NOT persisted) gating trigram next-token
     * training in grounded_language_learn_text_bigrams. Default false →
     * PA-4 behavior is preserved bit-for-bit. Callers opt in via
     * snn_language_bridge_set_trigram_learning_enabled. */
    bool enable_trigram_learning;

    /* TB-8: per-token streaming callback for snn_language_bridge_produce.
     * Borrowed pointers — NOT persisted, NOT in config. NULL fn = no
     * streaming (legacy behavior). Caller installs via
     * snn_language_bridge_set_stream_callback. */
    snn_lang_stream_callback_t stream_cb;
    void*                       stream_user_data;

    /* TC-11 — one-shot warning sticky for enable_gpu_decode-but-no-kernel
     * scaffold. Set on first decode call where the flag is true. */
    bool _tc11_warned;

    /* S4-C1 (H1): one-shot warning sticky for NaN/Inf in lateral-inhibition
     * settling. Without this the warning fires on every cascade tick once
     * the regime goes bad, flooding the log. */
    bool _li_warned;

    // Statistics
    snn_lang_stats_t stats;
};

//=============================================================================
// Hash function for binding lookup
//=============================================================================

/* Option-1 (Slice A): the binding-hashmap helpers below have no live
 * callers — the bridge no longer owns a concept_pop × word_pop weight
 * matrix. They are kept here behind #if 0 for reference (the surrounding
 * struct still has binding_buckets / num_bindings members for ABI / save-
 * load file-format compatibility, but those arrays are never populated).
 * Slice B's concept_registry replaces this layer entirely.
 */
#if 0
static inline uint32_t binding_hash(uint32_t concept_pop, uint32_t word_pop)
{
    uint32_t h = concept_pop * 2654435761u + word_pop * 40503u;
    return h & BINDING_HASH_MASK;
}

static binding_node_t* binding_find(snn_language_bridge_t* bridge,
                                     uint32_t concept_pop, uint32_t word_pop)
{
    uint32_t bucket = binding_hash(concept_pop, word_pop);
    binding_node_t* node = bridge->binding_buckets[bucket];
    while (node) {
        if (node->binding.concept_pop == concept_pop &&
            node->binding.word_pop == word_pop) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

static inline void norm_update(snn_language_bridge_t* bridge,
                                uint32_t word_pop,
                                float old_w, float new_w)
{
    if (!bridge->word_norm_sq || word_pop >= bridge->word_pops_capacity) return;
    float delta = (new_w * new_w) - (old_w * old_w);
    bridge->word_norm_sq[word_pop] += delta;
    if (bridge->word_norm_sq[word_pop] < 0.0f) {
        bridge->word_norm_sq[word_pop] = 0.0f;
    }
}

static binding_node_t* binding_insert(snn_language_bridge_t* bridge,
                                       uint32_t concept_pop, uint32_t word_pop,
                                       float initial_weight)
{
    binding_node_t* existing = binding_find(bridge, concept_pop, word_pop);
    if (existing) {
        float old_w = existing->binding.weight;
        float new_w = fmaxf(old_w, initial_weight);
        if (new_w != old_w) {
            existing->binding.weight = new_w;
            norm_update(bridge, word_pop, old_w, new_w);
        }
        return existing;
    }

    binding_node_t* node = nimcp_calloc(1, sizeof(binding_node_t));
    if (!node) {
        return NULL;
    }

    node->binding.concept_pop = concept_pop;
    node->binding.word_pop = word_pop;
    node->binding.weight = initial_weight;
    node->binding.last_pre_spike_ms = -1000.0f;
    node->binding.last_post_spike_ms = -1000.0f;

    uint32_t bucket = binding_hash(concept_pop, word_pop);
    node->next = bridge->binding_buckets[bucket];
    bridge->binding_buckets[bucket] = node;
    bridge->num_bindings++;

    norm_update(bridge, word_pop, 0.0f, initial_weight);

    return node;
}
#endif  /* legacy binding hashmap helpers — Slice A removed all live callers */

/* PA-5 forward decl: lazy embedding cache filler. Defined alongside the
 * other PA-5 helpers below; declared up here because decode_spikes uses
 * it before its definition. */
static inline int emb_cache_ensure(snn_language_bridge_t* bridge, uint32_t w);

//=============================================================================
// Configuration
//=============================================================================

snn_lang_config_t snn_lang_config_default(void)
{
    /* These two MUST match the SNN_LANG_MAX_*_POPS macros in the
     * public header — grounded_language's mirror_binding_to_bridge
     * hashes form_hash modulo SNN_LANG_MAX_WORD_POPS (16384) and
     * concept_id modulo SNN_LANG_MAX_CONCEPT_POPS (4096). If the
     * runtime capacity is smaller, register_word/register_concept
     * silently reject any pop beyond the smaller cap (line 330+346
     * of this file: `if (pop >= bridge->*_pops_capacity) return -1`)
     * and ~75% of bindings are dropped on the floor. Bug verified
     * 2026-05-06 — bridge_active_bindings was effectively zero for
     * months because of this mismatch. */
    snn_lang_config_t config = {
        .max_concept_pops = SNN_LANG_MAX_CONCEPT_POPS,
        .max_word_pops    = SNN_LANG_MAX_WORD_POPS,
        .neurons_per_pop = SNN_LANG_NEURONS_PER_POP,
        .stdp_tau_plus = SNN_LANG_DEFAULT_STDP_TAU,
        .stdp_tau_minus = SNN_LANG_DEFAULT_STDP_TAU,
        .stdp_a_plus = SNN_LANG_DEFAULT_STDP_A_PLUS,
        .stdp_a_minus = SNN_LANG_DEFAULT_STDP_A_MINUS,
        .stdp_learning_rate = 0.01f,
        .binding_w_max = SNN_LANG_BINDING_W_MAX,
        .decode_window_ms = SNN_LANG_DECODE_WINDOW_MS,
        .decay_rate = SNN_LANG_DECAY_RATE,
        .spike_blend = SNN_LANG_SPIKE_BLEND_DEFAULT,
        .enable_da_modulation = true,
        .da_modulation_gain = 50.0f,
        .enable_imagination = true,
        .enable_curiosity = true,
        .enable_sleep_consolidation = true,
        .prune_threshold = 0.005f,
        /* PA-6: defaults preserve legacy argmax behavior. Callers explicitly
         * set temperature > 0 to opt into sampling. produce_topk = 8 gives
         * sampling some headroom over the legacy 5 without much cost. */
        .temperature = 0.0f,
        .top_p = 1.0f,
        .produce_topk = 8,
        /* PA-5: GloVe blend off by default. Caller sets >0 after attaching
         * an embedding lookup with set_embedding_lookup(). */
        .glove_blend = 0.0f,
        /* PA-2: autoregressive defaults match legacy 0.7/0.3 in-place blend
         * (intent_persistence = 0 → state-driven; word_feedback = 0.3). */
        .intent_persistence = 0.0f,
        .word_feedback      = 0.3f,
        /* PA-3: spike routing OFF by default — explicit opt-in required to
         * avoid recreating the prior sparsity-collapse failure mode.
         * activation_tau_ms = 200 is a safe non-zero value; ignored when
         * the master flag is off. */
        .enable_snn_spike_routing = false,
        .activation_tau_ms        = 200.0f,
        /* PA-5+: hyperbolic distance OFF by default. Switching to true
         * makes decode_spikes use 1/(1+d_H(.,.)) instead of cosine. */
        .use_hyperbolic_embeddings = false,
        /* PA-6+: 0 = legacy auto-dispatch (argmax / softmax+top-p driven
         * by temperature). Modes 1 (softmax-only) and 2 (q-MC) are
         * explicit opt-ins. */
        .sampling_mode = 0,
        /* TIER1-A: beam_width = 1 = greedy / legacy bit-for-bit. */
        .produce_beam_width = 1,
        /* TIER1-B: EOS disabled by default. */
        .eos_word_pop = UINT32_MAX,
        /* TIER1-C: repetition penalty disabled by default. */
        .repetition_penalty = 0.0f,
        .repetition_window  = 3,
        /* TB-7: length-control disabled by default — sentinel 0 on both
         * preserves the legacy 32-word implicit cap and immediate-EOS
         * behavior bit-for-bit. */
        .min_produce_words = 0,
        .max_produce_words = 0,
        /* CSTDP — comprehend-driven STDP. Default ON. Min weight +
         * activation thresholds keep the path scoped to existing strong
         * bindings so we don't entrench noise; lr_scale dampens the LR vs
         * the produce-side STDP since comprehend fires far more often than
         * produce. Default ON because comprehend is the highest-frequency
         * language signal during training — left OFF, comprehend_stdp_passes
         * stays flat at zero for the whole run. The scoping thresholds make
         * it reinforce-what-comprehension-agrees-with, not inject-structure,
         * so it's safe as a default. Toggle off via
         * snn_language_bridge_set_comprehend_stdp_enabled if needed. */
        .enable_comprehend_stdp     = true,
        .comprehend_stdp_min_weight = 0.05f,
        .comprehend_stdp_min_activation = 0.10f,
        .comprehend_stdp_lr_scale   = 0.5f,
        /* EOS stopping criterion — default OFF preserves bit-for-bit
         * legacy produce-loop behavior. Caller opts in by setting
         * enable_eos_stopping = true; thresholds are tuneable but the
         * defaults are calibrated for unit-norm intent vectors and
         * cosine-scored confidences (see header for rationale). */
        .enable_eos_stopping        = false,
        .eos_min_activation         = 0.05f,
        .eos_min_confidence         = 0.01f,
        /* Beam-HNN re-rank — default OFF preserves bit-for-bit identical
         * beam ranking. weight = 1.0 is the natural penalty scale once
         * caller opts in (1/(1+|dev|) maps dev=1.0 → 0.5x score). alpha
         * = 0.6 reproduces the prior hard-coded length-norm exponent. */
        .enable_beam_hnn_rerank     = false,
        .beam_hnn_weight            = 1.0f,
        .beam_length_norm_alpha     = 0.6f,
        /* Margin gate for learn_next_token_pair / _triple LTD. 1.5 means
         * the false_winner must beat target by 50% in this context before
         * LTD fires — empirically tuned to stop the step-3900 regression
         * where indiscriminate LTD-on-top-1 eroded every globally-dominant
         * binding regardless of in-context competition. */
        .ltd_margin                 = 1.5f,
        /* Slice 4 — Lateral inhibition. Default OFF preserves bit-for-bit
         * decode_spikes behavior. Defaults bracket the competition in the
         * stable regime: gain_self > sum of inhibition for top candidate
         * (1.5 vs 31 * 0.026 ≈ 0.81), so a clear winner consistently
         * settles within ~20 micro-steps. */
        .enable_lateral_inhibition  = false,
        .lateral_gain_self          = 1.5f,
        .lateral_gain_inhibit       = 0.026f,  /* ~ 0.8 / (32 - 1) */
        .lateral_micro_steps        = 20
    };
    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

snn_language_bridge_t* snn_language_bridge_create(const snn_lang_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_create: config is NULL");
        return NULL;
    }

    snn_language_bridge_t* bridge = nimcp_calloc(1, sizeof(snn_language_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "snn_language_bridge_create: failed to allocate bridge");
        return NULL;
    }

    bridge->magic = SNN_LANG_MAGIC;
    bridge->config = *config;

    // Allocate concept populations
    bridge->concept_pops_capacity = config->max_concept_pops;
    bridge->concept_pops = nimcp_calloc(bridge->concept_pops_capacity,
                                        sizeof(concept_pop_info_t));
    if (!bridge->concept_pops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "snn_language_bridge_create: failed to allocate concept_pops");
        nimcp_free(bridge);
        return NULL;
    }

    // Allocate word populations
    bridge->word_pops_capacity = config->max_word_pops;
    bridge->word_pops = nimcp_calloc(bridge->word_pops_capacity,
                                     sizeof(word_pop_info_t));
    if (!bridge->word_pops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "snn_language_bridge_create: failed to allocate word_pops");
        nimcp_free(bridge->concept_pops);
        nimcp_free(bridge);
        return NULL;
    }

    /* Patch A: per-word_pop binding-weight L2 norm cache. Calloc → all zeros,
     * which is the correct initial state (no bindings yet). */
    bridge->word_norm_sq = nimcp_calloc(bridge->word_pops_capacity,
                                         sizeof(float));
    if (!bridge->word_norm_sq) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "snn_language_bridge_create: failed to allocate word_norm_sq");
        nimcp_free(bridge->word_pops);
        nimcp_free(bridge->concept_pops);
        nimcp_free(bridge);
        return NULL;
    }

    /* PA-6: seed sampling RNG. xorshift64* requires nonzero seed. */
    bridge->rng_state = (uint64_t)time(NULL) ^ ((uintptr_t)bridge * 0x9E3779B97F4A7C15ULL);
    if (bridge->rng_state == 0) bridge->rng_state = 0xDEADBEEF;

    /* PA-3: empty attached-pops table (snn_pop_id < 0 = unused slot). */
    for (uint32_t i = 0; i < SNN_LANG_MAX_ATTACHED_POPS; i++) {
        bridge->attached_pops[i].snn_pop_id = -1;
    }
    bridge->n_attached_pops = 0;

    /* Not connected to a network for produce-time generation until
     * snn_language_bridge_connect_network is called (at init). */
    bridge->gen_net = NULL;
    bridge->gen_wernicke_pop_id = -1;
    bridge->gen_broca_pop_id = -1;

    bbb_register_module("snn_language_bridge", BBB_MODULE_TYPE_COGNITIVE);

    LOG_INFO(LOG_MODULE, "SNN language bridge created (concepts=%u, words=%u, blend=%.2f)",
             config->max_concept_pops, config->max_word_pops, config->spike_blend);

    return bridge;
}

void snn_language_bridge_destroy(snn_language_bridge_t* bridge)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return;

    // Free binding hash map
    for (uint32_t i = 0; i < BINDING_HASH_BUCKETS; i++) {
        binding_node_t* node = bridge->binding_buckets[i];
        while (node) {
            binding_node_t* next = node->next;
            nimcp_free(node);
            node = next;
        }
    }

    nimcp_free(bridge->concept_pops);
    nimcp_free(bridge->word_pops);
    nimcp_free(bridge->word_norm_sq);
    nimcp_free(bridge->word_emb_cache);
    nimcp_free(bridge->word_emb_cached);
    nimcp_free(bridge->word_emb_norm);
    nimcp_free(bridge->broca_spike_cache);

    bridge->magic = 0;
    nimcp_free(bridge);
}

int snn_language_bridge_reset(snn_language_bridge_t* bridge)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;

    // Reset activations, keep bindings and registrations
    for (uint32_t i = 0; i < bridge->num_concept_pops; i++) {
        bridge->concept_pops[i].activation = 0.0f;
        bridge->concept_pops[i].last_spike_ms = -1000.0f;
    }
    for (uint32_t i = 0; i < bridge->num_word_pops; i++) {
        bridge->word_pops[i].activation = 0.0f;
        bridge->word_pops[i].last_spike_ms = -1000.0f;
    }

    bridge->current_time_ms = 0.0f;
    return 0;
}

//=============================================================================
// Connection
//=============================================================================

int snn_language_bridge_connect_grounded(snn_language_bridge_t* bridge,
                                          struct grounded_language* gl)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_connect_grounded: bridge is NULL or invalid");
        return -1;
    }
    bridge->grounded_lang = gl;
    return 0;
}

int snn_language_bridge_connect_imagination(snn_language_bridge_t* bridge,
                                             struct imagination_snn_bridge* imagination)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_connect_imagination: bridge is NULL or invalid");
        return -1;
    }
    bridge->imagination = imagination;
    return 0;
}

int snn_language_bridge_connect_curiosity(snn_language_bridge_t* bridge,
                                           struct curiosity_snn_bridge* curiosity)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_connect_curiosity: bridge is NULL or invalid");
        return -1;
    }
    bridge->curiosity = curiosity;
    return 0;
}

int snn_language_bridge_connect_neuromod(snn_language_bridge_t* bridge,
                                          struct neuromodulator_system_struct* neuromod)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_connect_neuromod: bridge is NULL or invalid");
        return -1;
    }
    bridge->neuromod = neuromod;
    return 0;
}

/* TA-2 LGSS output-gate attach. Stored as void* to keep the LGSS
 * umbrella header (which drags in cognitive/symbolic_logic enums) out
 * of the SNN bridge translation unit's exports. The produce wrapper
 * forward-declares lgss_evaluate + safety_action_context_t locally and
 * casts back. NULL = detach (gate becomes a no-op). */
int snn_language_bridge_set_lgss(snn_language_bridge_t* bridge, void* lgss)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_set_lgss: bridge is NULL or invalid");
        return -1;
    }
    bridge->lgss = lgss;
    return 0;
}

//=============================================================================
// Phase 1: Population Registration + Spike-to-Word Decoding
//=============================================================================

int snn_language_bridge_register_concept(snn_language_bridge_t* bridge,
                                          uint32_t concept_pop,
                                          uint64_t concept_id)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (concept_pop >= bridge->concept_pops_capacity) return -1;

    bridge->concept_pops[concept_pop].concept_id = concept_id;
    bridge->concept_pops[concept_pop].registered = true;
    bridge->concept_pops[concept_pop].last_spike_ms = -1000.0f;
    if (concept_pop >= bridge->num_concept_pops) {
        bridge->num_concept_pops = concept_pop + 1;
    }
    return 0;
}

int snn_language_bridge_register_word(snn_language_bridge_t* bridge,
                                       uint32_t word_pop,
                                       const char* word_form)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC || !word_form) return -1;
    if (word_pop >= bridge->word_pops_capacity) return -1;

    strncpy(bridge->word_pops[word_pop].word_form, word_form, 63);
    bridge->word_pops[word_pop].word_form[63] = '\0';
    bridge->word_pops[word_pop].registered = true;
    bridge->word_pops[word_pop].last_spike_ms = -1000.0f;
    if (word_pop >= bridge->num_word_pops) {
        bridge->num_word_pops = word_pop + 1;
    }
    return 0;
}

/* Increment-1 (2026-06-02): deterministic word↔neuron ensemble map. Each word_pop
 * index owns E = SNN_LANG_NEURONS_PER_POP neurons in a pop of `n_neurons`, chosen
 * by a splitmix64 hash of the index (stable across runs, no storage). Replaces the
 * lossy `neuron_idx % cap` aliasing for the produce-side readout: a word's E
 * neurons are spread by an odd stride so they stay disjoint within the pop, and
 * distinct words map to distinct ensembles until the pop saturates (n_neurons/E
 * fully-disjoint ensembles, graceful overlap beyond). */
static uint32_t lang_ensemble_neuron(uint32_t pop_idx, uint32_t j, uint32_t n_neurons)
{
    if (n_neurons == 0) return 0;
    /* DISJOINT contiguous allocation (2026-06-09 collision fix). The previous
     * splitmix64-hashed base+stride placement scattered each pop's 8 neurons
     * pseudo-randomly across n_neurons, so with MAX_WORD_POPS(32768)×8 = 262144
     * neuron-claims landing in a 64000-neuron Broca, every neuron was claimed by
     * ~4 different word pops — distinct words shared neurons and were
     * indistinguishable at decode (the generation scramble). Contiguous blocks
     * [pop_idx*8 .. pop_idx*8+8) are provably disjoint as long as the population
     * is sized n_neurons >= MAX_POPS * NEURONS_PER_POP (Broca = 32768*8 = 262144;
     * Wernicke 64000 >= 4096*8). The modulo is a safety wrap only — under correct
     * sizing it never triggers. */
    return (uint32_t)(((uint64_t)pop_idx * SNN_LANG_NEURONS_PER_POP + j) % n_neurons);
}

int snn_language_bridge_decode_spikes(snn_language_bridge_t* bridge,
                                       const float* concept_rates,
                                       uint32_t num_concept_pops,
                                       snn_lang_word_result_t* results,
                                       uint32_t max_results,
                                       uint32_t* num_results)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC || !concept_rates ||
        !results || !num_results) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_decode_spikes: bridge, concept_rates, results, or num_results is NULL");
        return -1;
    }
    bridge->stats.total_decode_calls++;
    *num_results = 0;
    (void)num_concept_pops; (void)max_results;
    /* Option-1 (Slice A): transport-only stub — returns zero results. KEPT a
     * stub on purpose: existing on-training-path callers (next-token bigram
     * learning, learn_text_bigrams) rely on this returning 0 so their target-
     * rank lookup stays -1; changing it would alter default-ON learning. The
     * Increment-1 SNN readout lives in the separate
     * snn_language_bridge_decode_spikes_cached() below, used only by the opt-in
     * produce_via_snn path. */
    (void)concept_rates;
    return 0;
}

/* Increment-1 (2026-06-02): opt-in SNN-derived produce readout. Ranks words by
 * summed Broca spike activity over each word's deterministic neuron ensemble,
 * read from the cache populated by drain_pop_spikes each tick. Returns 0 results
 * (caller falls back to the lexicon producer) when there is no SNN signal. This
 * is SEPARATE from the stubbed decode_spikes so it cannot perturb the default
 * training/produce paths — only grounded_language_produce_via_snn calls it. */
int snn_language_bridge_decode_spikes_cached(snn_language_bridge_t* bridge,
                                       const float* concept_rates,
                                       uint32_t num_concept_pops,
                                       snn_lang_word_result_t* results,
                                       uint32_t max_results,
                                       uint32_t* num_results)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC || !concept_rates ||
        !results || !num_results) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_decode_spikes_cached: bridge, concept_rates, results, or num_results is NULL");
        return -1;
    }

    bridge->stats.total_decode_calls++;
    *num_results = 0;
    (void)concept_rates; (void)num_concept_pops;

    /* Increment-1 readout: rank words by summed Broca spike activity over each
     * word's deterministic neuron ensemble, read from the cache populated by
     * drain_pop_spikes each tick. This is the SNN-derived candidate signal; the
     * caller (grounded_language_produce_via_snn) blends it with lexicon scoring
     * and falls through to find_words_near_vector when this returns 0 (no SNN
     * signal yet — e.g. spike routing off, or a cold Broca pop). No concept→word
     * projection exists yet (Phase 2), so concept_rates is not consulted here. */
    const float*   cache = bridge->broca_spike_cache;
    const uint32_t ncap  = bridge->broca_spike_cache_cap;
    if (!cache || ncap == 0 || max_results == 0) return 0;

    const uint32_t nwords = bridge->num_word_pops;
    uint32_t filled = 0;
    for (uint32_t w = 0; w < nwords; w++) {
        if (!bridge->word_pops[w].registered) continue;
        float act = 0.0f;
        for (uint32_t j = 0; j < SNN_LANG_NEURONS_PER_POP; j++) {
            act += cache[lang_ensemble_neuron(w, j, ncap)];
        }
        if (!(act > 0.0f)) continue;

        /* Insertion sort into the top-`max_results` by activation (desc). */
        if (filled < max_results) {
            uint32_t p = filled;
            while (p > 0 && results[p-1].activation < act) {
                results[p] = results[p-1]; p--;
            }
            results[p].word_pop    = w;
            results[p].word_form   = bridge->word_pops[w].word_form;
            results[p].activation  = act;
            results[p].confidence  = act / (float)SNN_LANG_NEURONS_PER_POP;
            filled++;
        } else if (act > results[max_results-1].activation) {
            uint32_t p = max_results - 1;
            while (p > 0 && results[p-1].activation < act) {
                results[p] = results[p-1]; p--;
            }
            results[p].word_pop    = w;
            results[p].word_form   = bridge->word_pops[w].word_form;
            results[p].activation  = act;
            results[p].confidence  = act / (float)SNN_LANG_NEURONS_PER_POP;
        }
    }

    *num_results = filled;
    return 0;
}

/* Phase-2 step 3 (2026-06-02): warm-start the concept→word projection from the
 * lexicon. For each word's strongest concept binding, set the weight of the
 * projection synapses linking that concept's Wernicke ensemble to the word's
 * Broca ensemble to k*strength (clamped to the AMPA range). Updates both the
 * CSR entries[] and the parallel flat weights[] (line-195 invariant), then
 * syncs H→D so the running SNN sees it. Runtime-safe: only mutates existing
 * synapse weights (no topology/receptor-table change → no CB-GPU-7 hazard).
 * Returns #synapses updated, or -1 on invalid args / unfinalized CSR. */
int snn_language_bridge_warmstart_projection(snn_language_bridge_t* bridge,
                                             struct snn_network_s* net,
                                             int wernicke_pop_id,
                                             int broca_pop_id,
                                             float k, float lr)
{
    /* lr is the blend toward the lexicon target k*strength:
     *   lr >= 1.0 → overwrite (one-shot warm-start, Phase 2 step 3),
     *   0 < lr < 1 → incremental EMA toward the target (Phase 3 online
     *                training — the projection TRACKS the lexicon as it
     *                learns, so distributional/trigram/TF/cognitive feedback
     *                that shapes the lexicon flows into the SNN synapses).
     * Clamp lr to (0,1]. */
    if (!(lr > 0.0f)) lr = 1.0f;
    if (lr > 1.0f) lr = 1.0f;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC || !net ||
        wernicke_pop_id < 0 || broca_pop_id < 0 || !bridge->grounded_lang) {
        return -1;
    }
    snn_population_t* broca = snn_network_get_population(net, (uint32_t)broca_pop_id);
    snn_population_t* wern  = snn_network_get_population(net, (uint32_t)wernicke_pop_id);
    if (!broca || !wern || !broca->incoming_csr || !broca->incoming_csr->finalized) {
        return -1;
    }
    snn_csr_storage_t* csr = broca->incoming_csr;
    const uint32_t broca_n = broca->n_neurons;
    const uint32_t wern_n  = wern->n_neurons;
    if (broca_n == 0 || wern_n == 0) return -1;

    const uint32_t cap = SNN_LANG_MAX_WORD_POPS;
    gl_warmstart_binding_t* binds =
        (gl_warmstart_binding_t*)nimcp_calloc(cap, sizeof(*binds));
    if (!binds) return -1;
    const uint32_t nb =
        grounded_language_collect_warmstart_bindings(bridge->grounded_lang, binds, cap);

    int updated = 0;
    for (uint32_t i = 0; i < nb; i++) {
        const uint32_t word_pop    = binds[i].form_hash % SNN_LANG_MAX_WORD_POPS;
        const uint32_t concept_pop =
            (uint32_t)(binds[i].concept_id % SNN_LANG_MAX_CONCEPT_POPS);
        float wval = k * binds[i].strength;   /* w_base = 0 (silent until set) */
        if (!(wval > 0.0f)) continue;
        if (wval > SNN_LANG_PROJ_W_MAX) wval = SNN_LANG_PROJ_W_MAX;  /* projection cap (supra-threshold) */

        uint32_t cens[SNN_LANG_NEURONS_PER_POP];
        for (uint32_t j = 0; j < SNN_LANG_NEURONS_PER_POP; j++) {
            cens[j] = lang_ensemble_neuron(concept_pop, j, wern_n);
        }
        for (uint32_t j = 0; j < SNN_LANG_NEURONS_PER_POP; j++) {
            const uint32_t dst = lang_ensemble_neuron(word_pop, j, broca_n);
            uint32_t cnt = 0;
            snn_csr_synapse_t* inc = snn_csr_get_incoming(csr, dst, &cnt);
            if (!inc || cnt == 0) continue;
            const uint32_t bdx = (uint32_t)(inc - csr->entries);
            for (uint32_t e = 0; e < cnt; e++) {
                if (inc[e].src_pop != (uint32_t)wernicke_pop_id) continue;
                for (uint32_t j2 = 0; j2 < SNN_LANG_NEURONS_PER_POP; j2++) {
                    if (inc[e].src_neuron == cens[j2]) {
                        /* Blend toward the lexicon target: overwrite when lr=1
                         * (warm-start), EMA when lr<1 (online training). */
                        float nw = inc[e].weight + lr * (wval - inc[e].weight);
                        if (nw < 0.0f) nw = 0.0f;
                        if (nw > SNN_LANG_PROJ_W_MAX) nw = SNN_LANG_PROJ_W_MAX;
                        inc[e].weight = nw;
                        if (csr->weights && (bdx + e) < csr->n_synapses) {
                            csr->weights[bdx + e] = nw;
                        }
                        updated++;
                        break;
                    }
                }
            }
        }
    }
    nimcp_free(binds);
    (void)snn_csr_sync_weights_to_gpu(csr);  /* H->D so the running SNN sees it */
    LOG_INFO("[snn_lang_bridge] warmstart_projection: %u lexicon bindings -> %d "
             "projection synapses set (wernicke=%d->broca=%d, k=%.3f)",
             nb, updated, wernicke_pop_id, broca_pop_id, (double)k);
    return updated;
}

/* Train ONE concept<->word association — the per-pair primitive the training
 * loop calls for each taught pair (the bulk lexicon variant is
 * warmstart_projection above). Strengthens the projection synapses linking
 * concept_id's Wernicke ensemble to word_pop's Broca ensemble toward w_target
 * via EMA blend lr. Like warm-start it can only SET weights on synapses that
 * already exist (targeted or dense projection topology), so a concept with no
 * wired pathway to its word ensemble updates nothing (returns 0). */
int snn_language_bridge_learn_pair(snn_language_bridge_t* bridge,
                                   struct snn_network_s* net,
                                   int wernicke_pop_id, int broca_pop_id,
                                   uint64_t concept_id, uint32_t word_pop,
                                   float w_target, float lr)
{
    if (!(lr > 0.0f)) lr = 1.0f;
    if (lr > 1.0f) lr = 1.0f;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC || !net ||
        wernicke_pop_id < 0 || broca_pop_id < 0) {
        return -1;
    }
    /* Clamp to SNN_LANG_PROJ_W_MAX, NOT the 2.0 generic-AMPA cap: only
     * SNN_LANG_NEURONS_PER_POP (8) synapses converge on each word neuron, so at
     * 2.0 the ensemble delivers 8*2.0=16 — below the 20mV LIF gap — and the word
     * never fires (validated 2026-06-07: Broca silent at clamp 2.0). A small
     * dedicated ensemble must carry a higher per-synapse weight to be
     * supra-threshold; the projection is excluded from global plasticity so this
     * doesn't perturb the rest of the brain. */
    if (w_target < 0.0f) w_target = 0.0f;
    if (w_target > SNN_LANG_PROJ_W_MAX) w_target = SNN_LANG_PROJ_W_MAX;

    snn_population_t* broca = snn_network_get_population(net, (uint32_t)broca_pop_id);
    snn_population_t* wern  = snn_network_get_population(net, (uint32_t)wernicke_pop_id);
    if (!broca || !wern || !broca->incoming_csr || !broca->incoming_csr->finalized) {
        return -1;
    }
    snn_csr_storage_t* csr = broca->incoming_csr;
    const uint32_t broca_n = broca->n_neurons;
    const uint32_t wern_n  = wern->n_neurons;
    if (broca_n == 0 || wern_n == 0) return -1;

    /* Same concept-pop mapping generate_step uses to SEED, so the synapses we
     * strengthen are exactly the ones a later generate_step(concept_id) drives. */
    const uint32_t concept_pop = (uint32_t)(concept_id % SNN_LANG_MAX_CONCEPT_POPS);
    uint32_t cens[SNN_LANG_NEURONS_PER_POP];
    for (uint32_t j = 0; j < SNN_LANG_NEURONS_PER_POP; j++) {
        cens[j] = lang_ensemble_neuron(concept_pop, j, wern_n);
    }

    int updated = 0;
    for (uint32_t j = 0; j < SNN_LANG_NEURONS_PER_POP; j++) {
        const uint32_t dst = lang_ensemble_neuron(word_pop, j, broca_n);
        uint32_t cnt = 0;
        snn_csr_synapse_t* inc = snn_csr_get_incoming(csr, dst, &cnt);
        if (!inc || cnt == 0) continue;
        const uint32_t bdx = (uint32_t)(inc - csr->entries);
        for (uint32_t e = 0; e < cnt; e++) {
            if (inc[e].src_pop != (uint32_t)wernicke_pop_id) continue;
            for (uint32_t j2 = 0; j2 < SNN_LANG_NEURONS_PER_POP; j2++) {
                if (inc[e].src_neuron == cens[j2]) {
                    float nw = inc[e].weight + lr * (w_target - inc[e].weight);
                    if (nw < 0.0f) nw = 0.0f;
                    if (nw > SNN_LANG_PROJ_W_MAX) nw = SNN_LANG_PROJ_W_MAX;
                    inc[e].weight = nw;
                    if (csr->weights && (bdx + e) < csr->n_synapses) {
                        csr->weights[bdx + e] = nw;
                    }
                    updated++;
                    break;
                }
            }
        }
    }
    (void)snn_csr_sync_weights_to_gpu(csr);  /* H->D so the running SNN sees it */
    return updated;
}

/* Wire the TARGETED concept→word projection for ONE pair — the scalable
 * topology builder. Adds the 8×8 ensemble cross-product (concept Wernicke
 * ensemble → word Broca ensemble) via snn_csr_add_entry, which REQUIRES the
 * Broca incoming CSR to be pre-finalize. Only registered pairs get synapses,
 * so a 32K-word vocab costs ~vocab×64 synapses (bounded) with EXACT coverage,
 * vs a random sweep that connects a given concept ensemble to its word ensemble
 * with ~0.1 expected synapses. learn_pair / warmstart then set the weights. */
int snn_language_bridge_wire_concept_word(snn_language_bridge_t* bridge,
                                          struct snn_network_s* net,
                                          int wernicke_pop_id, int broca_pop_id,
                                          uint64_t concept_id, uint32_t word_pop,
                                          float w_init)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC || !net ||
        wernicke_pop_id < 0 || broca_pop_id < 0) {
        return -1;
    }
    snn_population_t* broca = snn_network_get_population(net, (uint32_t)broca_pop_id);
    snn_population_t* wern  = snn_network_get_population(net, (uint32_t)wernicke_pop_id);
    if (!broca || !wern || !broca->incoming_csr) return -1;
    if (broca->incoming_csr->finalized) return -1;   /* must add pre-finalize */
    const uint32_t broca_n = broca->n_neurons;
    const uint32_t wern_n  = wern->n_neurons;
    if (broca_n == 0 || wern_n == 0) return -1;
    if (w_init < 0.0f) w_init = 0.0f;
    if (w_init > SNN_LANG_PROJ_W_MAX) w_init = SNN_LANG_PROJ_W_MAX;

    const uint32_t concept_pop = (uint32_t)(concept_id % SNN_LANG_MAX_CONCEPT_POPS);
    int added = 0;
    for (uint32_t j = 0; j < SNN_LANG_NEURONS_PER_POP; j++) {
        uint32_t src = lang_ensemble_neuron(concept_pop, j, wern_n);
        for (uint32_t k = 0; k < SNN_LANG_NEURONS_PER_POP; k++) {
            uint32_t dst = lang_ensemble_neuron(word_pop, k, broca_n);
            if (snn_csr_add_entry(broca->incoming_csr, dst,
                                  (uint32_t)wernicke_pop_id, src, w_init) == 0)
                added++;
        }
    }
    return added;
}

/* Build the concept→word projection TOPOLOGY for ALL registered lexicon
 * bindings — the missing "coverage half" (2026-06-09). wire_concept_word and
 * learn_pair/warmstart can only operate on synapses that already exist, and
 * nothing previously created them: the Broca incoming CSR is finalized at init,
 * and the lexicon bindings only exist post-load, so there was never a window to
 * wire them. Result: Broca had zero concept input, decode returned 0, the SNN
 * never generated. This orchestrates the fix:
 *   1. snn_csr_reopen(Broca CSR)  — un-finalize so add_entry is allowed; the
 *      reopen frees the GPU device mirror so the next step re-uploads topology.
 *   2. wire_concept_word for every (concept_id → word_pop) lexicon binding —
 *      the 8×8 Wernicke-ensemble → Broca-ensemble cross-product per pair.
 *   3. snn_csr_finalize — rebuild row_ptr so the running SNN can use it.
 * learn_pair/warmstart then set the weights supra-threshold.
 *
 * CONCURRENCY: mutates live CSR topology. MUST run with SNN stepping paused —
 * the daemon RPC path holds the global lock (serialized with learn_vector), so
 * no snn_network_step runs concurrently. Do NOT call from the step hot path.
 * Returns #pairs wired (>0), 0 if no bindings, or -1 on error. */
int snn_language_bridge_build_projection(snn_language_bridge_t* bridge,
                                         struct snn_network_s* net,
                                         float w_init)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC || !net ||
        bridge->gen_wernicke_pop_id < 0 || bridge->gen_broca_pop_id < 0 ||
        !bridge->grounded_lang) {
        return -1;
    }
    const int wid = bridge->gen_wernicke_pop_id;
    const int bid = bridge->gen_broca_pop_id;
    snn_population_t* broca = snn_network_get_population(net, (uint32_t)bid);
    if (!broca || !broca->incoming_csr) return -1;
    snn_csr_storage_t* csr = broca->incoming_csr;

    /* Reopen if finalized so wire_concept_word's snn_csr_add_entry is allowed. */
    const bool was_finalized = csr->finalized;
    if (was_finalized && snn_csr_reopen(csr) != 0) {
        LOG_ERROR("[snn_lang_bridge] build_projection: snn_csr_reopen failed");
        return -1;
    }

    const uint32_t cap = SNN_LANG_MAX_WORD_POPS;
    gl_warmstart_binding_t* binds =
        (gl_warmstart_binding_t*)nimcp_calloc(cap, sizeof(*binds));
    if (!binds) {
        if (was_finalized) (void)snn_csr_finalize(csr);
        return -1;
    }
    const uint32_t nb =
        grounded_language_collect_warmstart_bindings(bridge->grounded_lang, binds, cap);

    int wired = 0;
    for (uint32_t i = 0; i < nb; i++) {
        const uint32_t word_pop = binds[i].form_hash % SNN_LANG_MAX_WORD_POPS;
        if (snn_language_bridge_wire_concept_word(bridge, net, wid, bid,
                                                  binds[i].concept_id, word_pop,
                                                  w_init) > 0) {
            wired++;
        }
    }
    nimcp_free(binds);

    /* Re-finalize so the running SNN sees the new topology (next step re-uploads
     * the freed GPU mirror). */
    if (snn_csr_finalize(csr) != 0) {
        LOG_ERROR("[snn_lang_bridge] build_projection: snn_csr_finalize failed");
        return -1;
    }
    LOG_INFO("[snn_lang_bridge] build_projection: wired %d/%u concept→word pairs "
             "(wernicke=%d->broca=%d, w_init=%.2f)",
             wired, nb, wid, bid, (double)w_init);
    return wired;
}

/* Phase-2 produce-time SNN generation step (2026-06-03). THE generator: seed the
 * intent's concepts into the Wernicke concept ensembles (external_current), step
 * the whole SNN n_steps times so the concept->word projection drives Broca, then
 * copy Broca's spike_output into broca_spike_cache for decode_spikes_cached to
 * read. This is what makes the SNN GENERATE from intent (vs reading stale cached
 * training activity). Clears the injected current on exit so it doesn't bleed
 * into subsequent ticks.
 *
 * COST: there is no scoped stepper — snn_network_step advances the ENTIRE ~2M-
 * neuron network, so this adds n_steps * (~full-step latency) to produce. Use a
 * small n_steps (4-8). Default-OFF, opt-in only.
 *
 * UNVALIDATED: written against the documented APIs but never executed end-to-end
 * (no runnable full-brain env this session). Returns 0 on success, -1 on error. */
int snn_language_bridge_generate_step(snn_language_bridge_t* bridge,
                                      struct snn_network_s* net,
                                      int wernicke_pop_id, int broca_pop_id,
                                      const uint64_t* concept_ids, uint32_t n_concepts,
                                      uint32_t n_steps, float inject_current)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC || !net ||
        wernicke_pop_id < 0 || broca_pop_id < 0 || !concept_ids || n_concepts == 0) {
        return -1;
    }
    snn_population_t* wern  = snn_network_get_population(net, (uint32_t)wernicke_pop_id);
    snn_population_t* broca = snn_network_get_population(net, (uint32_t)broca_pop_id);
    if (!wern || !broca || !wern->external_current || !broca->spike_output) return -1;
    const uint32_t wern_n  = wern->n_neurons;
    const uint32_t broca_n = broca->n_neurons;
    if (wern_n == 0 || broca_n == 0) return -1;
    if (n_steps == 0) n_steps = 4;
    if (!(inject_current > 0.0f)) inject_current = 1.0f;

    /* Ensure the Broca decode cache exists and is zeroed before the window —
     * we ACCUMULATE spike counts across all n_steps into it (see below). */
    if (!bridge->broca_spike_cache || bridge->broca_spike_cache_cap < broca_n) {
        if (bridge->broca_spike_cache) nimcp_free(bridge->broca_spike_cache);
        bridge->broca_spike_cache = (float*)nimcp_calloc(broca_n, sizeof(float));
        bridge->broca_spike_cache_cap = bridge->broca_spike_cache ? broca_n : 0u;
    }
    float* bcache = (bridge->broca_spike_cache &&
                     bridge->broca_spike_cache_cap >= broca_n)
                  ? bridge->broca_spike_cache : NULL;
    if (bcache) memset(bcache, 0, (size_t)broca_n * sizeof(float));

    /* 1+2+3. Drive + step + accumulate. Two physics facts shape this loop:
     *   - snn_network_step ZEROES external_current at the end of each step
     *     (it's consumed within the step), so the concept seed must be
     *     RE-INJECTED before every step to keep Wernicke firing across the
     *     window — inject-once drives only step 1 and Broca never sustains.
     *   - Broca's spike_output is a single-step snapshot: on any given step a
     *     just-fired neuron is in reset/refractory and reads 0. Snapshotting
     *     only the LAST step therefore loses most of the produced activity
     *     (validated: last-step snapshot yields decode=0). So accumulate the
     *     per-neuron spike COUNT over the whole window — that's the Broca
     *     firing-rate signal decode_spikes_cached actually wants.
     *
     * BASELINE-SUBTRACTED (DIFFERENTIAL) DECODE (2026-06-11): gen_net is the
     * full live 2M-neuron SNN, so Broca carries heavy AMBIENT firing (~15k
     * spikes/step brain-wide) unrelated to the seed. Raw spike-count decode is
     * dominated by whichever Broca neurons happen to be ambient-hot, yielding
     * the SAME prompt-independent exotic-word salad regardless of which concept
     * is seeded (the seed's ~8-neuron projection is a drop in that ocean).
     * Fix: run an UNSEEDED baseline window first, then the SEEDED signal window,
     * and keep only the positive per-neuron delta (signal − baseline). That
     * cancels the always-on ambient pops and leaves the projection-driven
     * response. Falls back to raw signal counts if the baseline alloc fails. */
    enum { MAXC = 64 };
    if (n_concepts > MAXC) n_concepts = MAXC;

    float* baseline = (float*)nimcp_calloc(broca_n, sizeof(float));

    /* Baseline window: step the net with NO seed injection, accumulate ambient
     * Broca spike counts. Skipped if alloc failed (raw-count fallback). */
    if (baseline) {
        for (uint32_t s = 0; s < n_steps; s++) {
            (void)snn_network_step(net, 1.0f /* dt_ms */);
            const float* spikes = (const float*)nimcp_tensor_data(broca->spike_output);
            if (spikes) {
                for (uint32_t n = 0; n < broca_n; n++) {
                    if (spikes[n] > 0.5f) baseline[n] += 1.0f;
                }
            }
        }
    }

    /* Signal window: re-inject the seed every step, accumulate Broca counts. */
    int any_spikes = 0;
    for (uint32_t s = 0; s < n_steps; s++) {
        for (uint32_t c = 0; c < n_concepts; c++) {
            uint32_t concept_pop = (uint32_t)(concept_ids[c] % SNN_LANG_MAX_CONCEPT_POPS);
            for (uint32_t j = 0; j < SNN_LANG_NEURONS_PER_POP; j++) {
                uint32_t nrn = lang_ensemble_neuron(concept_pop, j, wern_n);
                wern->external_current[nrn] = inject_current;
            }
        }
        (void)snn_network_step(net, 1.0f /* dt_ms */);
        const float* spikes = (const float*)nimcp_tensor_data(broca->spike_output);
        if (spikes && bcache) {
            for (uint32_t n = 0; n < broca_n; n++) {
                if (spikes[n] > 0.5f) { bcache[n] += 1.0f; any_spikes = 1; }
            }
        }
    }

    /* 4. Clear the injected current so it doesn't drive subsequent ticks. */
    for (uint32_t c = 0; c < n_concepts; c++) {
        uint32_t concept_pop = (uint32_t)(concept_ids[c] % SNN_LANG_MAX_CONCEPT_POPS);
        for (uint32_t j = 0; j < SNN_LANG_NEURONS_PER_POP; j++) {
            uint32_t nrn = lang_ensemble_neuron(concept_pop, j, wern_n);
            wern->external_current[nrn] = 0.0f;
        }
    }

    /* Differential: keep only the seed-driven excess over ambient. */
    if (baseline && bcache) {
        for (uint32_t n = 0; n < broca_n; n++) {
            float d = bcache[n] - baseline[n];
            bcache[n] = (d > 0.0f) ? d : 0.0f;
        }
    }
    if (baseline) nimcp_free(baseline);

    (void)any_spikes;  /* 0 spikes is a valid (cold) result, not an error */
    return (bcache != NULL) ? 0 : -1;
}

/* Store the borrowed network + Wernicke/Broca pop ids so the produce path can
 * drive generation without re-discovering them. Called once at init from
 * attach_lang_adapters_to_substrate. */
int snn_language_bridge_connect_network(snn_language_bridge_t* bridge,
                                        struct snn_network_s* net,
                                        int wernicke_pop_id, int broca_pop_id)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    bridge->gen_net = net;
    bridge->gen_wernicke_pop_id = wernicke_pop_id;
    bridge->gen_broca_pop_id = broca_pop_id;
    return 0;
}

/* Produce-time convenience: seed `concept_ids` into Wernicke, step the SNN, and
 * decode the fresh Broca activity into ranked words — using the network + pop
 * ids stored by connect_network. This is the call the produce path makes to let
 * the SNN GENERATE from the cascade's content_intent concepts (vs reading stale
 * ambient cache). Returns -1 if not connected or on error; 0 with *num_results
 * (possibly 0 — a cold/uncovered projection yields nothing and the caller falls
 * back to the lexicon producer). */
int snn_language_bridge_generate_and_decode(snn_language_bridge_t* bridge,
                                            const uint64_t* concept_ids,
                                            uint32_t n_concepts,
                                            uint32_t n_steps, float inject_current,
                                            snn_lang_word_result_t* results,
                                            uint32_t max_results,
                                            uint32_t* num_results)
{
    if (num_results) *num_results = 0;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC || !bridge->gen_net ||
        bridge->gen_wernicke_pop_id < 0 || bridge->gen_broca_pop_id < 0 ||
        !concept_ids || n_concepts == 0 || !results || !num_results) {
        return -1;
    }
    if (snn_language_bridge_generate_step(bridge, bridge->gen_net,
            bridge->gen_wernicke_pop_id, bridge->gen_broca_pop_id,
            concept_ids, n_concepts, n_steps, inject_current) != 0) {
        return -1;
    }
    /* decode_spikes_cached ignores its concept_rates arg; pass a dummy. */
    float dummy[1] = { 0.0f };
    return snn_language_bridge_decode_spikes_cached(bridge, dummy, 1,
                                                    results, max_results, num_results);
}

/* Diagnostic: concept→word projection round-trip selectivity. See header. */
int snn_language_bridge_probe_selectivity(snn_language_bridge_t* bridge,
                                          uint32_t n_probe, uint32_t n_steps,
                                          float inject, int* out_ranks,
                                          uint32_t max_out, uint32_t* out_n)
{
    if (out_n) *out_n = 0;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC || !bridge->gen_net ||
        bridge->gen_wernicke_pop_id < 0 || bridge->gen_broca_pop_id < 0 ||
        !bridge->grounded_lang || !out_ranks || !out_n || max_out == 0) {
        return -1;
    }
    if (n_steps == 0) n_steps = 8;
    if (!(inject > 0.0f)) inject = 100.0f;

    const uint32_t cap = SNN_LANG_MAX_WORD_POPS;
    gl_warmstart_binding_t* binds =
        (gl_warmstart_binding_t*)nimcp_calloc(cap, sizeof(*binds));
    if (!binds) return -1;
    const uint32_t nb =
        grounded_language_collect_warmstart_bindings(bridge->grounded_lang, binds, cap);
    if (n_probe > nb) n_probe = nb;
    if (n_probe > max_out) n_probe = max_out;

    enum { PROBE_TOPK = 32 };
    snn_lang_word_result_t res[PROBE_TOPK];
    uint32_t out = 0;
    for (uint32_t i = 0; i < n_probe; i++) {
        const uint32_t expect_pop = binds[i].form_hash % SNN_LANG_MAX_WORD_POPS;
        uint64_t cid = binds[i].concept_id;
        uint32_t nres = 0;
        int rank = -1;
        if (snn_language_bridge_generate_and_decode(bridge, &cid, 1, n_steps, inject,
                                                    res, PROBE_TOPK, &nres) == 0) {
            for (uint32_t r = 0; r < nres; r++) {
                if (res[r].word_pop == expect_pop) { rank = (int)r; break; }
            }
        }
        out_ranks[out++] = rank;
    }
    nimcp_free(binds);
    *out_n = out;
    return 0;
}

#if 0  /* Old decode_spikes body — retained only for reference; Slice B will
        * rewrite this on top of concept_registry. */
int snn_language_bridge_decode_spikes_LEGACY_OFF(snn_language_bridge_t* bridge,
                                       const float* concept_rates,
                                       uint32_t num_concept_pops,
                                       snn_lang_word_result_t* results,
                                       uint32_t max_results,
                                       uint32_t* num_results)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC || !concept_rates ||
        !results || !num_results) {
        return -1;
    }

    bridge->stats.total_decode_calls++;
    *num_results = 0;

    /* TC-11 — bench-first scaffold. The CUDA port of decode is deferred
     * pending vocab growth past ~16K (PCIe round-trip dominates the math
     * win at current scale). The flag exists for ABI forward-compat;
     * setting it logs once and falls through to the CPU path. */
    if (bridge->config.enable_gpu_decode && !bridge->_tc11_warned) {
        LOG_WARN("snn_lang_bridge",
                 "enable_gpu_decode=true but GPU decode kernel is not yet "
                 "implemented (TC-11 scaffold); falling back to CPU path. "
                 "This warning is one-shot per bridge.");
        bridge->_tc11_warned = true;
    }

    /* TC-11 — wallclock timing of the CPU decode pass. Lets operators
     * see whether decode is actually a bottleneck before anyone ports it
     * to GPU. Resolution = clock_gettime(CLOCK_MONOTONIC), accumulated
     * into stats.decode_total_ns. */
    struct timespec _tc11_t0;
    clock_gettime(CLOCK_MONOTONIC, &_tc11_t0);

    // Compute word activations through binding matrix
    // word_activation[w] = sum_c(concept_rates[c] * binding_weight[c,w])
    uint32_t n_words = bridge->num_word_pops;
    float* word_acts = nimcp_calloc(n_words, sizeof(float));
    if (!word_acts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "snn_language_bridge_decode_spikes: failed to allocate word_acts");
        return -1;
    }

    // Iterate all bindings (sparse traversal)
    for (uint32_t bucket = 0; bucket < BINDING_HASH_BUCKETS; bucket++) {
        binding_node_t* node = bridge->binding_buckets[bucket];
        while (node) {
            uint32_t c = node->binding.concept_pop;
            uint32_t w = node->binding.word_pop;
            if (c < num_concept_pops && w < n_words) {
                word_acts[w] += concept_rates[c] * node->binding.weight;
            }
            node = node->next;
        }
    }

    /* Patch A: cosine-normalize. score[w] = (concept_rates · weight[*, w]) /
     *                                       ||weight[*, w]||₂
     * Without this, words with more bindings (or larger total binding mass)
     * dominate top-K regardless of input direction — diagnosed live as a
     * rank-1 collapse where the same 4 words win every produce call.
     * concept_rates is treated as already-comparable across calls (caller
     * passes l2-normalized intent); we only normalize the binding column. */
    const float eps = 1e-6f;
    float* word_scores = nimcp_calloc(n_words, sizeof(float));
    if (!word_scores) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "snn_language_bridge_decode_spikes: failed to allocate word_scores");
        nimcp_free(word_acts);
        return -1;
    }

    /* PA-5: precompute the intent-side embedding norm once if the GloVe
     * blend is active. The embedding query is the leading emb_dim coords
     * of concept_rates. Skip lookups entirely when blend is 0 or when no
     * embedding callback has been attached.
     *
     * Walkthrough round 2 fix (PA-2 × PA-5): when the autoregressive
     * decoder evolves concept_acts into binding space, the GloVe cosine
     * sees a corrupted embedding query. produce sets emb_query_override
     * to the immutable original intent so the GloVe term remains
     * embedding-space-coherent across the loop. NULL = no override
     * (legacy callers + non-autoregressive produce). */
    const float glove_blend = bridge->config.glove_blend;
    const bool emb_active = (glove_blend > 0.0f && bridge->emb_lookup_fn &&
                              bridge->word_emb_cache && bridge->emb_dim > 0);
    const bool hyper_mode = bridge->config.use_hyperbolic_embeddings;
    const float* emb_query = bridge->emb_query_override
                               ? bridge->emb_query_override
                               : concept_rates;
    const uint32_t emb_query_dim = bridge->emb_query_override
                                     ? bridge->emb_query_override_dim
                                     : num_concept_pops;
    float intent_emb_norm = 0.0f;
    /* PA-5+: hyperbolic mode also needs the projected query inside the
     * Poincaré ball. Project via tanh(‖v‖)·v/‖v‖ — this maps any finite
     * Euclidean vector into B^d while preserving direction, and clips
     * to ‖.‖ < 1 even for unit-norm inputs. d_used is capped at the
     * smaller of emb_dim and emb_query_dim, max 64 (more than enough
     * for any GloVe slice we ever pass through here). */
    float intent_hyper[64] = {0};
    uint32_t d_used = 0;
    if (emb_active) {
        d_used = (bridge->emb_dim < emb_query_dim)
                   ? bridge->emb_dim : emb_query_dim;
        if (d_used > 64) d_used = 64;
        for (uint32_t d = 0; d < d_used; d++) {
            intent_emb_norm += emb_query[d] * emb_query[d];
        }
        intent_emb_norm = sqrtf(intent_emb_norm + eps);
        if (hyper_mode) {
            /* Project: q' = tanh(‖q‖) · q / ‖q‖. */
            float scale = tanhf(intent_emb_norm) / intent_emb_norm;
            for (uint32_t d = 0; d < d_used; d++) {
                intent_hyper[d] = emb_query[d] * scale;
            }
        }
    }

    for (uint32_t w = 0; w < n_words; w++) {
        if (!bridge->word_pops[w].registered) {
            word_scores[w] = -FLT_MAX;
            continue;
        }
        float ns = bridge->word_norm_sq ? bridge->word_norm_sq[w] : 1.0f;
        float norm = sqrtf(ns + eps);
        float binding_score = word_acts[w] / norm;

        /* PA-5: blend in cosine(intent_emb, word_emb[w]) when active.
         * Walkthrough round 2 fix: read embedding query from emb_query
         * (override-aware) so PA-2 autoregressive blending doesn't
         * corrupt the GloVe term.
         * PA-5+: hyper_mode swaps the cosine for 1/(1+d_H(.,.)). */
        if (emb_active) {
            int got = emb_cache_ensure(bridge, w);
            if (got == 1) {
                const float* emb = bridge->word_emb_cache + (size_t)w * bridge->emb_dim;
                float glove_score = 0.0f;
                if (hyper_mode) {
                    /* Project word emb the same way: e' = tanh(‖e‖)·e/‖e‖.
                     * word_emb_norm[w] already holds sqrt(Σe² + eps) from
                     * emb_cache_ensure, so reuse it. */
                    float wnorm = bridge->word_emb_norm[w];
                    if (wnorm < eps) wnorm = eps;
                    float wscale = tanhf(wnorm) / wnorm;
                    /* Inline Poincaré distance:
                     *   d = acosh(1 + 2 ‖x-y‖² / ((1-‖x‖²)(1-‖y‖²)))
                     */
                    float diff_sq = 0.0f;
                    float xn_sq = 0.0f;
                    float yn_sq = 0.0f;
                    for (uint32_t d = 0; d < d_used; d++) {
                        float xv = intent_hyper[d];
                        float yv = emb[d] * wscale;
                        float dv = xv - yv;
                        diff_sq += dv * dv;
                        xn_sq += xv * xv;
                        yn_sq += yv * yv;
                    }
                    /* Clip to ball interior. */
                    float one_minus_x = 1.0f - xn_sq;
                    float one_minus_y = 1.0f - yn_sq;
                    if (one_minus_x < POINCARE_EPSILON) one_minus_x = POINCARE_EPSILON;
                    if (one_minus_y < POINCARE_EPSILON) one_minus_y = POINCARE_EPSILON;
                    float arg = 1.0f + 2.0f * diff_sq / (one_minus_x * one_minus_y);
                    /* Numerically-safe acosh. arg ≥ 1 by construction; floor
                     * at 1.0 in case of fp drift. */
                    if (arg < 1.0f) arg = 1.0f;
                    float d_h = acoshf(arg);
                    if (!isfinite(d_h)) d_h = 0.0f;
                    glove_score = 1.0f / (1.0f + d_h);
                } else {
                    uint32_t d_cos = (bridge->emb_dim < emb_query_dim)
                                       ? bridge->emb_dim : emb_query_dim;
                    float dot = 0.0f;
                    for (uint32_t d = 0; d < d_cos; d++) {
                        dot += emb_query[d] * emb[d];
                    }
                    glove_score = dot / (intent_emb_norm * bridge->word_emb_norm[w]);
                }
                word_scores[w] = (1.0f - glove_blend) * binding_score
                                  + glove_blend * glove_score;
            } else {
                /* No embedding for this word — fall back to binding-only,
                 * scaled by (1−blend) so words with embeddings still get a
                 * fair comparison if their glove_score is small. */
                word_scores[w] = (1.0f - glove_blend) * binding_score;
            }
        } else {
            word_scores[w] = binding_score;
        }
    }

    /* Sum of positive cosine scores for confidence normalization (denominator
     * of softmax-style attribution). Computed once across the registered set. */
    float sum_pos = 0.0f;
    for (uint32_t w = 0; w < n_words; w++) {
        if (word_scores[w] > 0.0f) sum_pos += word_scores[w];
    }

    // Find top-k words by cosine score
    for (uint32_t k = 0; k < max_results && k < n_words; k++) {
        float best_score = -FLT_MAX;
        uint32_t best_w = 0;
        bool found = false;

        for (uint32_t w = 0; w < n_words; w++) {
            if (!bridge->word_pops[w].registered) continue;
            if (word_scores[w] > best_score) {
                // Check not already in results
                bool duplicate = false;
                for (uint32_t j = 0; j < *num_results; j++) {
                    if (results[j].word_pop == w) { duplicate = true; break; }
                }
                if (!duplicate) {
                    best_score = word_scores[w];
                    best_w = w;
                    found = true;
                }
            }
        }

        if (!found || best_score <= 0.0f) break;

        results[*num_results].word_pop = best_w;
        results[*num_results].word_form = bridge->word_pops[best_w].word_form;
        results[*num_results].activation = best_score;
        results[*num_results].confidence = (sum_pos > 0.0f) ? best_score / sum_pos : 0.0f;
        (*num_results)++;
    }

    nimcp_free(word_scores);
    nimcp_free(word_acts);

    /* TC-11 — close the timing window + accumulate into stats. */
    struct timespec _tc11_t1;
    clock_gettime(CLOCK_MONOTONIC, &_tc11_t1);
    uint64_t elapsed_ns = (uint64_t)(_tc11_t1.tv_sec - _tc11_t0.tv_sec) * 1000000000ull
                         + (uint64_t)(_tc11_t1.tv_nsec - _tc11_t0.tv_nsec);
    bridge->stats.decode_total_ns += elapsed_ns;

    return 0;
}
#endif  /* end legacy decode_spikes off-block */

/*===========================================================================
 * Slice 4 — Decode with lateral inhibition (competitive lexical selection)
 *
 * Real cortex selects words via competitive recurrent dynamics rather than
 * one-shot argmax. Cohort model (Marslen-Wilson 1987), interactive activa-
 * tion (McClelland 1981), drift-diffusion (Ratcliff). All converge on:
 * candidate representations excite themselves AND inhibit each other; the
 * winner emerges from settling over ~50-200ms of simulated time.
 *
 * Implementation: take the standard top-K from decode_spikes (the cosine
 * winners — these are the candidates "in the cohort"), initialize their
 * activations to the cosine scores, then iterate
 *
 *   new_a[k] = sigmoid(a[k] * gain_self - sum_{j != k} a[j] * gain_inhibit)
 *
 * for `micro_steps` cycles. Sigmoid bounds a[k] to (0, 1) — no NaN/Inf
 * blow-up regardless of pathological gains. Final re-rank by post-
 * competition activation.
 *
 * Stability: at K=32 and the default gains (1.5 self, ~0.026 per-other),
 * self-excitation (1.5) just outweighs total inhibition for the top
 * candidate (~31 * 0.026 ≈ 0.81). Subordinate candidates see net
 * inhibition and decay to near zero; the leader saturates near
 * sigmoid(~0.5) ≈ 0.62. Empirically clean separation within ~10-15
 * micro-steps.
 *
 * Cost: K * T multiplies + K sigmoid per call. At K=32, T=20 that's
 * ~640 ops + 20 sigmoids — under 5us at -O2 on x86_64.
 *===========================================================================*/

#ifndef LATERAL_INHIBITION_MAX_K
#define LATERAL_INHIBITION_MAX_K 32u
#endif
#ifndef LATERAL_INHIBITION_MAX_STEPS
#define LATERAL_INHIBITION_MAX_STEPS 200u
#endif

static inline float _li_sigmoid(float x) {
    /* Numerically stable sigmoid: split positive / negative to avoid
     * expf overflow at large |x| (defensive, the gain regime shouldn't
     * push us past |x| > 20). */
    if (x >= 0.0f) {
        float ez = expf(-x);
        return 1.0f / (1.0f + ez);
    } else {
        float ez = expf(x);
        return ez / (1.0f + ez);
    }
}

int snn_language_bridge_decode_with_lateral_inhibition(
    snn_language_bridge_t* bridge,
    const float* concept_rates,
    uint32_t num_concept_pops,
    snn_lang_word_result_t* results,
    uint32_t max_results,
    uint32_t* num_results)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC ||
        !concept_rates || !results || !num_results) {
        return -1;
    }

    /* Pull the cosine top-K from the standard decode path. This is the
     * "cohort" that gets to compete. Cap at LATERAL_INHIBITION_MAX_K so
     * the on-stack activation buffer stays bounded. */
    uint32_t k_cap = max_results;
    if (k_cap > LATERAL_INHIBITION_MAX_K) k_cap = LATERAL_INHIBITION_MAX_K;

    int rc = snn_language_bridge_decode_spikes(bridge, concept_rates,
                                                num_concept_pops, results,
                                                k_cap, num_results);
    if (rc != 0) return rc;

    uint32_t K = *num_results;
    if (K == 0) return 0;
    /* Wave-3 (2026-05-19) telemetry: bump decode_calls for every entry
     * that reached actual K-WTA logic (K >= 1), so consumers can divide
     * the sum-counters by it for averages. */
    bridge->stats.lateral_inhibition_decode_calls++;
    /* Degenerate: a single candidate has nothing to compete with. Leave
     * the result as-is (activation = cosine score, confidence intact). */
    if (K == 1) return 0;

    /* Read tunables from config. Clamp here too, defense-in-depth — the
     * setter validates but a future raw-config-injection path might
     * not. */
    float gain_self    = bridge->config.lateral_gain_self;
    float gain_inhibit = bridge->config.lateral_gain_inhibit;
    uint32_t T         = bridge->config.lateral_micro_steps;
    if (!isfinite(gain_self)    || gain_self    <= 0.0f) gain_self    = 1.5f;
    if (!isfinite(gain_inhibit) || gain_inhibit <= 0.0f) gain_inhibit = 0.026f;
    if (T == 0) T = 20;
    if (T > LATERAL_INHIBITION_MAX_STEPS) T = LATERAL_INHIBITION_MAX_STEPS;

    /* S4-C1 fix: replaced the sigmoid update with Grossberg-style divisive
     * normalization (new_a[k] = a[k]^p / (eps + sum_j a[j]^p)).
     *
     * The previous formula
     *     new_a[k] = sigmoid(a[k]*gain_self - gain_inhibit*(sum_a - a[k]))
     * had a STABLE symmetric fixed point: at the default
     * gain_self=1.5, gain_inhibit=0.026, K=32 the per-step linearisation
     * sigmoid'(drive) * (gain_self + gain_inhibit) yielded an amplification
     * less than 1, so any small asymmetry decayed back into the symmetric
     * fixed point a* ≈ 0.62 instead of growing into a winner. Net effect:
     * every candidate converged to ≈ equal activation and the re-rank
     * became noise.
     *
     * Divisive normalization at exponent p ≈ 2 is the standard
     * neuroscience-style WTA primitive: it is mathematically equivalent
     * to a softmax over log(a) with temperature 1/p, and a Liapunov
     * argument (sum a^p is conserved up to scaling) guarantees the
     * leader → 1, subordinates → 0 attractor at p > 1. With p = 2 the
     * leader hits >0.9 within ~10-15 micro-steps for the K we care
     * about (2..32).
     *
     * The legacy tunables (gain_self / gain_inhibit / micro_steps) are
     * REPURPOSED in this regime:
     *   - gain_self: divisive exponent p (clamped to [1.0, 8.0]; old
     *     1.5 default was too low — bump implicit default to 2.0).
     *   - gain_inhibit: epsilon floor on the denominator (clamped to
     *     [1e-8, 1e-3]; old 0.026 was an inhibition gain, not an epsilon
     *     — coerce small values into the new range).
     *   - lateral_micro_steps: unchanged (settling iterations).
     */
    float p = gain_self;
    if (p < 1.0f) p = 1.0f;
    if (p > 8.0f) p = 8.0f;
    /* Old gain_inhibit defaults were O(0.01..0.1) — too big for an
     * eps floor. Clamp into a sane range; values > 1e-3 are forced
     * down. */
    float eps = gain_inhibit;
    if (eps < 1e-8f) eps = 1e-8f;
    if (eps > 1e-3f) eps = 1e-3f;

    /* On-stack activation buffers — K bounded by LATERAL_INHIBITION_MAX_K.
     * Two buffers: read from `a`, write to `new_a`, swap each step. */
    float a[LATERAL_INHIBITION_MAX_K];
    float new_a[LATERAL_INHIBITION_MAX_K];

    /* Initial activations from the cosine scores. decode_spikes returns
     * scores in [0, 1+] typically (cosine + GloVe blend); normalize the
     * starting point by the max so the competition begins with the
     * leader at 1.0 and others scaled below. Avoids cases where every
     * a[k] starts tiny and the sigmoid never escapes 0.5. */
    float a_max = 0.0f;
    for (uint32_t k = 0; k < K; k++) {
        if (results[k].activation > a_max) a_max = results[k].activation;
    }
    if (a_max <= 0.0f) a_max = 1.0f;  /* defensive — keep ratios */

    for (uint32_t k = 0; k < K; k++) {
        a[k] = results[k].activation / a_max;
        if (!isfinite(a[k]) || a[k] < 0.0f) a[k] = 0.0f;
        if (a[k] > 1.0f) a[k] = 1.0f;
    }

    /* Divisive-normalization settling loop. Each step is O(K).
     *   pwr[k]   = a[k]^p  (one powf per element)
     *   sum_pwr  = sum_j pwr[j] + eps
     *   new_a[k] = pwr[k] / sum_pwr
     * Result: the leader's share grows as the p-th power each step;
     * subordinates → 0 in O(log K) micro-steps. */
    bool nan_seen = false;
    uint32_t steps_done = 0;
    for (uint32_t t = 0; t < T; t++) {
        steps_done = t + 1;
        float sum_pwr = 0.0f;
        float pwr[LATERAL_INHIBITION_MAX_K];
        for (uint32_t k = 0; k < K; k++) {
            float v = a[k];
            if (!isfinite(v) || v < 0.0f) v = 0.0f;
            /* powf(0, p) is well-defined as 0 for p > 0; no domain issue. */
            float pk = powf(v, p);
            if (!isfinite(pk)) {
                nan_seen = true;
                pk = 0.0f;
            }
            pwr[k] = pk;
            sum_pwr += pk;
        }
        sum_pwr += eps;

        for (uint32_t k = 0; k < K; k++) {
            float next = (sum_pwr > 0.0f) ? (pwr[k] / sum_pwr) : 0.0f;
            if (!isfinite(next)) {
                nan_seen = true;
                next = 0.0f;
            }
            new_a[k] = next;
        }

        /* Swap buffers. */
        for (uint32_t k = 0; k < K; k++) a[k] = new_a[k];

        if (nan_seen) break;  /* abandon settling, fall back below */
    }

    if (nan_seen) {
        /* Wave-3 telemetry: bump NaN-fallback counter. */
        bridge->stats.lateral_inhibition_nan_fallbacks++;
        /* Sticky one-shot warning — without this, the warning fires every
         * tick once the regime is bad. Fall back to cosine top-K. */
        if (!bridge->_li_warned) {
            bridge->_li_warned = true;
            LOG_WARN(LOG_MODULE,
                     "decode_with_lateral_inhibition: NaN/Inf in divisive-"
                     "normalization settling — falling back to cosine top-K "
                     "(p=%.3f, eps=%.2e, T=%u, K=%u). Further occurrences "
                     "suppressed.", p, (double)eps, T, K);
        }
        return 0;
    }

    /* Wave-3 telemetry: settled_steps + winner-margin. settled_steps is
     * always T here (no early-exit on convergence yet — slot for future
     * delta-stop), so this is "steps done before normal exit". Winner
     * margin uses the post-sort top two activations after we do the
     * sort below; record before tmp[] is destroyed. */
    bridge->stats.lateral_inhibition_settled_steps_sum += steps_done;

    /* Sum for probability-like confidence. */
    float sum_settled = 0.0f;
    for (uint32_t k = 0; k < K; k++) sum_settled += a[k];

    /* Write post-competition activations + confidences back into results.
     * We re-rank by sorting a simple index array — K <= 32, insertion
     * sort is fine. */
    uint8_t order[LATERAL_INHIBITION_MAX_K];
    for (uint32_t k = 0; k < K; k++) order[k] = (uint8_t)k;
    for (uint32_t i = 1; i < K; i++) {
        uint8_t key = order[i];
        float key_a = a[key];
        int j = (int)i - 1;
        while (j >= 0 && a[order[j]] < key_a) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }

    /* Write back in the new ranked order. Use a temp buffer so we don't
     * stomp on results[i] before we read its old contents. */
    snn_lang_word_result_t tmp[LATERAL_INHIBITION_MAX_K];
    for (uint32_t k = 0; k < K; k++) {
        uint8_t src = order[k];
        tmp[k] = results[src];
        tmp[k].activation = a[src];
        tmp[k].confidence = (sum_settled > 0.0f) ? (a[src] / sum_settled)
                                                 : 0.0f;
    }
    for (uint32_t k = 0; k < K; k++) results[k] = tmp[k];

    /* Wave-3 telemetry: winner margin = a[order[0]] - a[order[1]] (post-
     * settle). K guaranteed >= 2 here. Stored as fixed-point margin * 1e6
     * to keep the field uint64_t (consumers divide by 1e6 * decode_calls
     * for mean margin). */
    if (K >= 2) {
        float margin = a[order[0]] - a[order[1]];
        if (margin < 0.0f) margin = 0.0f;
        if (margin > 1.0f) margin = 1.0f;
        bridge->stats.lateral_inhibition_winner_margin_sum +=
            (uint64_t)(margin * 1000000.0f);
    }

    return 0;
}

/*===========================================================================
 * Option-1 transport API (Slice A)
 *
 * Pure spike routing between concept and word populations. The bridge does
 * not own weights — the projection synapses that actually drive the
 * concept→word mapping live in the SNN, indexed by the concept_registry
 * Slice B is building. Until that registry is wired in, the routing here
 * is an identity stub: each input pop_id is echoed to the same pop_id on
 * the other side. Tests verify the function exists, accepts the inputs,
 * returns successfully — joint Slice B+A walkthroughs will verify the
 * mapping itself.
 *===========================================================================*/

int snn_language_bridge_route_concept_to_word(
    snn_language_bridge_t* bridge,
    const uint32_t* concept_pop_ids, size_t n_concepts,
    uint32_t* word_pop_ids_out, size_t* n_words_out, size_t max_out)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (n_concepts > 0 && !concept_pop_ids) return -1;
    if (!n_words_out) return -1;

    bridge->stats.total_spike_routes_concept_to_word++;

    /* TODO(slice-B): use concept_registry to translate concept_pop_id ->
     * canonical word_pop_id. Until then, identity-map and clamp to max_out. */
    size_t out_count = (n_concepts < max_out) ? n_concepts : max_out;
    if (word_pop_ids_out && out_count > 0) {
        for (size_t i = 0; i < out_count; i++) {
            word_pop_ids_out[i] = concept_pop_ids[i];
        }
    }
    *n_words_out = out_count;
    return 0;
}

int snn_language_bridge_route_word_to_concept(
    snn_language_bridge_t* bridge,
    const uint32_t* word_pop_ids, size_t n_words,
    uint32_t* concept_pop_ids_out, size_t* n_concepts_out, size_t max_out)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (n_words > 0 && !word_pop_ids) return -1;
    if (!n_concepts_out) return -1;

    bridge->stats.total_spike_routes_word_to_concept++;

    /* TODO(slice-B): use concept_registry to translate word_pop_id ->
     * canonical concept_pop_id. Until then, identity-map and clamp. */
    size_t out_count = (n_words < max_out) ? n_words : max_out;
    if (concept_pop_ids_out && out_count > 0) {
        for (size_t i = 0; i < out_count; i++) {
            concept_pop_ids_out[i] = word_pop_ids[i];
        }
    }
    *n_concepts_out = out_count;
    return 0;
}

int snn_language_bridge_encode_word(snn_language_bridge_t* bridge,
                                     uint32_t word_pop,
                                     float* concept_activations,
                                     uint32_t num_concept_pops)
{
    /* Option-1 (Slice A): transport-only stub. Bridge has no binding
     * weights to encode a word as a concept activation pattern. Returns
     * zeroed activations and 0 (success). Slice B's concept_registry
     * will reattach this to the canonical word_id → concept_pop_id
     * mapping. */
    (void)word_pop;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC || !concept_activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_encode_word: bridge or concept_activations is NULL");
        return -1;
    }
    bridge->stats.total_encode_calls++;
    memset(concept_activations, 0, num_concept_pops * sizeof(float));
    return 0;
}

//=============================================================================
// Phase 2: STDP-Driven Word-Concept Binding
//=============================================================================

int snn_language_bridge_concept_spike(snn_language_bridge_t* bridge,
                                       uint32_t concept_pop,
                                       float spike_time_ms)
{
    /* Option-1 (Slice A): no-op stub. The bridge no longer drives STDP
     * off spike timing — the SNN owns its plasticity directly. Concept-
     * pop activation was only ever consumed by apply_stdp / decode_spikes,
     * both of which are now transport-only. */
    (void)concept_pop; (void)spike_time_ms;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    return 0;
}

int snn_language_bridge_word_spike(snn_language_bridge_t* bridge,
                                    uint32_t word_pop,
                                    float spike_time_ms)
{
    /* Option-1 (Slice A): no-op stub. See concept_spike. */
    (void)word_pop; (void)spike_time_ms;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    return 0;
}

int snn_language_bridge_apply_stdp(snn_language_bridge_t* bridge,
                                    float current_time_ms)
{
    /* Option-1 (Slice A): no-op stub. The bridge no longer owns a
     * concept_pop × word_pop weight matrix; STDP is the SNN's job now,
     * acting on the SNN's own projection synapses. Kept for ABI so
     * existing callers (grounded_language.c, cascade.c, brain_tick_*.c)
     * continue to link until Slice B migrates them. */
    (void)current_time_ms;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    return 0;
}

int snn_language_bridge_bind(snn_language_bridge_t* bridge,
                              uint32_t concept_pop, uint32_t word_pop,
                              float initial_weight)
{
    /* Option-1 (Slice A): no-op stub. Bridge has no weights. */
    (void)concept_pop; (void)word_pop; (void)initial_weight;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    return 0;
}

int snn_language_bridge_strengthen_binding(snn_language_bridge_t* bridge,
                                            uint32_t concept_pop,
                                            uint32_t word_pop,
                                            float delta)
{
    /* Option-1 (Slice A): no-op stub. Bridge has no weights. */
    (void)concept_pop; (void)word_pop; (void)delta;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    return 0;
}

int snn_language_bridge_strengthen_binding_riemannian(snn_language_bridge_t* bridge,
                                                       uint32_t concept_pop,
                                                       uint32_t word_pop,
                                                       float grad)
{
    /* Option-1 (Slice A): no-op stub. Bridge has no weights. */
    (void)concept_pop; (void)word_pop; (void)grad;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    return 0;
}

/* PA-6: xorshift64* — small period (~2^64) but more than enough for
 * per-word sampling. Returns a uniform float in [0, 1). */
static inline uint64_t bridge_rng_u64(snn_language_bridge_t* bridge)
{
    uint64_t x = bridge->rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    bridge->rng_state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

static inline float bridge_rng_unit(snn_language_bridge_t* bridge)
{
    /* 24 mantissa bits → uniform [0, 1). */
    uint32_t bits = (uint32_t)(bridge_rng_u64(bridge) >> 40);
    return (float)bits * (1.0f / 16777216.0f);
}

/* Tier-4 #17: explicit RNG seed — overrides the create()-time
 * (time XOR pointer-mix). Sampling tests (PA-6, MQ-A) seed with a known
 * constant to make picks reproducible across runs. xorshift64 collapses
 * to a permanent zero on a zero state, so we remap seed=0 → 1 silently
 * (caller need not special-case it). */
int snn_language_bridge_set_rng_seed(snn_language_bridge_t* bridge,
                                       uint64_t seed)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    bridge->rng_state = (seed == 0ULL) ? 1ULL : seed;
    return 0;
}

int snn_language_bridge_set_sampling(snn_language_bridge_t* bridge,
                                      float temperature, float top_p)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (!isfinite(temperature) || temperature < 0.0f) return -1;
    if (!isfinite(top_p) || top_p <= 0.0f || top_p > 1.0f) return -1;
    bridge->config.temperature = temperature;
    bridge->config.top_p = top_p;
    return 0;
}

/* PA-6+: select sampling mode dispatch.
 *   0 = legacy (argmax / softmax+top-p auto-dispatch by temperature).
 *   1 = force softmax+top-p (PA-6).
 *   2 = quantum-Monte-Carlo MCMC sampling. */
int snn_language_bridge_set_sampling_mode(snn_language_bridge_t* bridge,
                                            int mode)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (mode < 0 || mode > 2) return -1;
    bridge->config.sampling_mode = mode;
    return 0;
}

/* TIER1-A: configure beam-K decoding. k=1 (or 0) means greedy; capped at 16. */
int snn_language_bridge_set_beam_width(snn_language_bridge_t* bridge, uint32_t k)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (k == 0) k = 1;
    if (k > 16) k = 16;
    bridge->config.produce_beam_width = k;
    return 0;
}

/* Beam-HNN re-rank attach/detach. NULL detaches. Type-erased to keep
 * the LNN header out of this TU. */
int snn_language_bridge_set_hnn(snn_language_bridge_t* bridge, void* hnn)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    bridge->hnn = hnn;
    return 0;
}

int snn_language_bridge_set_beam_hnn_rerank(snn_language_bridge_t* bridge,
                                              bool enabled,
                                              float weight)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (!isfinite(weight)) return -1;
    if (weight < 0.0f) weight = 0.0f;
    if (weight > 100.0f) weight = 100.0f;
    bridge->config.enable_beam_hnn_rerank = enabled;
    bridge->config.beam_hnn_weight = weight;
    return 0;
}

int snn_language_bridge_set_beam_length_norm_alpha(snn_language_bridge_t* bridge,
                                                     float alpha)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (!isfinite(alpha)) return -1;
    if (alpha < 0.1f) alpha = 0.1f;
    if (alpha > 1.5f) alpha = 1.5f;
    bridge->config.beam_length_norm_alpha = alpha;
    return 0;
}

/* TIER1-B: register EOS word_pop. UINT32_MAX disables. */
int snn_language_bridge_set_eos_word_pop(snn_language_bridge_t* bridge,
                                          uint32_t pop)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    bridge->config.eos_word_pop = pop;
    return 0;
}

/* TIER1-C: configure n-gram repetition penalty. */
int snn_language_bridge_set_repetition_penalty(snn_language_bridge_t* bridge,
                                                 float penalty,
                                                 uint32_t window)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (!isfinite(penalty)) return -1;
    if (penalty < 0.0f) penalty = 0.0f;
    if (penalty > 1.0f) penalty = 1.0f;
    bridge->config.repetition_penalty = penalty;
    /* window == 0 with penalty > 0 falls back to 3 (default) inside produce. */
    bridge->config.repetition_window = window;
    return 0;
}

/* TB-7: hard length-control on bridge_produce.
 *
 * Both arguments are clamped to [0, 1024] (anything beyond is almost
 * certainly a config error — even a 1024-word "sentence" is far past any
 * reasonable training example or dialog turn). Sentinel 0 keeps the
 * corresponding side disabled.
 *
 * Cross-validation: when both arguments are nonzero, min must be ≤ max,
 * otherwise the call is rejected with -1 and config is unchanged. Disabled
 * sentinels (0 on either side) skip the cross-check so callers can flip
 * one side at a time without first reading the other. */
#define SNN_LANG_LENGTH_CONTROL_MAX 1024u

int snn_language_bridge_set_length_control(snn_language_bridge_t* bridge,
                                            uint32_t min_words,
                                            uint32_t max_words)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (min_words > SNN_LANG_LENGTH_CONTROL_MAX) {
        min_words = SNN_LANG_LENGTH_CONTROL_MAX;
    }
    if (max_words > SNN_LANG_LENGTH_CONTROL_MAX) {
        max_words = SNN_LANG_LENGTH_CONTROL_MAX;
    }
    /* Reject min > max only when both are active. Either disabled (0) is
     * fine — the caller may legitimately set just a min or just a max. */
    if (min_words > 0 && max_words > 0 && min_words > max_words) {
        return -1;
    }
    bridge->config.min_produce_words = min_words;
    bridge->config.max_produce_words = max_words;
    return 0;
}

int snn_language_bridge_get_length_control(const snn_language_bridge_t* bridge,
                                            uint32_t* min_words,
                                            uint32_t* max_words)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (min_words) *min_words = bridge->config.min_produce_words;
    if (max_words) *max_words = bridge->config.max_produce_words;
    return 0;
}

/* PA-5 helpers — embedding cache lifecycle. */
static void emb_cache_free(snn_language_bridge_t* bridge)
{
    nimcp_free(bridge->word_emb_cache);
    nimcp_free(bridge->word_emb_cached);
    nimcp_free(bridge->word_emb_norm);
    bridge->word_emb_cache  = NULL;
    bridge->word_emb_cached = NULL;
    bridge->word_emb_norm   = NULL;
}

static int emb_cache_alloc(snn_language_bridge_t* bridge, uint32_t emb_dim)
{
    size_t cap = (size_t)bridge->word_pops_capacity;
    bridge->word_emb_cache  = nimcp_calloc(cap * emb_dim, sizeof(float));
    bridge->word_emb_cached = nimcp_calloc(cap, sizeof(uint8_t));
    bridge->word_emb_norm   = nimcp_calloc(cap, sizeof(float));
    if (!bridge->word_emb_cache || !bridge->word_emb_cached ||
        !bridge->word_emb_norm) {
        emb_cache_free(bridge);
        return -1;
    }
    return 0;
}

int snn_language_bridge_set_embedding_lookup(snn_language_bridge_t* bridge,
                                              snn_lang_word_emb_fn fn,
                                              void* ctx,
                                              uint32_t emb_dim)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;

    /* Detach: NULL fn frees the cache and zeros the lookup. */
    if (!fn) {
        emb_cache_free(bridge);
        bridge->emb_lookup_fn = NULL;
        bridge->emb_lookup_ctx = NULL;
        bridge->emb_dim = 0;
        return 0;
    }

    if (emb_dim == 0) return -1;

    /* Reattach with different dim → realloc cache. */
    if (bridge->word_emb_cache && bridge->emb_dim != emb_dim) {
        emb_cache_free(bridge);
    }
    if (!bridge->word_emb_cache) {
        if (emb_cache_alloc(bridge, emb_dim) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "set_embedding_lookup: failed to allocate emb cache");
            return -1;
        }
    }
    bridge->emb_lookup_fn  = fn;
    bridge->emb_lookup_ctx = ctx;
    bridge->emb_dim        = emb_dim;
    return 0;
}

int snn_language_bridge_set_glove_blend(snn_language_bridge_t* bridge,
                                         float blend)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (!isfinite(blend) || blend < 0.0f || blend > 1.0f) return -1;
    bridge->config.glove_blend = blend;
    return 0;
}

int snn_language_bridge_invalidate_emb_cache(snn_language_bridge_t* bridge)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (bridge->word_emb_cached) {
        memset(bridge->word_emb_cached, 0,
               bridge->word_pops_capacity * sizeof(uint8_t));
    }
    if (bridge->word_emb_norm) {
        memset(bridge->word_emb_norm, 0,
               bridge->word_pops_capacity * sizeof(float));
    }
    return 0;
}

/* PA-5+: toggle Poincaré-ball hyperbolic distance for the GloVe term. The
 * raw Euclidean emb cache is shared between cosine and hyperbolic paths
 * (we project on the fly in decode_spikes) so no cache invalidation is
 * strictly required, but we do it anyway to be safe — callers may have
 * their own cache state predicated on which metric was active. */
int snn_language_bridge_set_hyperbolic_embeddings(snn_language_bridge_t* bridge,
                                                    bool enabled)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    bool was = bridge->config.use_hyperbolic_embeddings;
    bridge->config.use_hyperbolic_embeddings = enabled;
    if (was != enabled) {
        snn_language_bridge_invalidate_emb_cache(bridge);
    }
    return 0;
}

int snn_language_bridge_set_autoregressive(snn_language_bridge_t* bridge,
                                            float intent_persistence,
                                            float word_feedback)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (!isfinite(intent_persistence) ||
        intent_persistence < 0.0f || intent_persistence > 1.0f) return -1;
    if (!isfinite(word_feedback) ||
        word_feedback < 0.0f || word_feedback > 1.0f) return -1;
    bridge->config.intent_persistence = intent_persistence;
    bridge->config.word_feedback      = word_feedback;
    return 0;
}

/* ============================================================================
 * PA-3: SNN-spike routing.
 *
 * The decode/comprehend paths previously relied on synthesized spike events
 * issued at lexicon-bind time, which caused SNN sparsity collapse — without
 * decay on concept_pops[].activation, every accumulated spike summed forever
 * and the attention-feedback path broadcast "attend everything" to the
 * sensory bridges. This API re-introduces real spike routing with three
 * mandatory safeguards:
 *
 *   1. Master flag (config.enable_snn_spike_routing) defaults to false.
 *   2. activation_tau_ms must be > 0 when the flag is true (rejected here).
 *   3. snn_language_bridge_tick() applies exponential decay every global
 *      tick, so accumulators cannot drift unbounded even at high spike
 *      rates.
 * ============================================================================ */
int snn_language_bridge_set_snn_spike_routing(snn_language_bridge_t* bridge,
                                                bool enabled, float tau_ms)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (enabled) {
        if (!isfinite(tau_ms) || tau_ms <= 0.0f) return -1;
    }
    bridge->config.enable_snn_spike_routing = enabled;
    if (isfinite(tau_ms) && tau_ms > 0.0f) {
        bridge->config.activation_tau_ms = tau_ms;
    }
    return 0;
}

int snn_language_bridge_attach_snn_pop(snn_language_bridge_t* bridge,
                                        int snn_pop_id, uint32_t n_neurons,
                                        snn_lang_pop_role_t role)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (snn_pop_id < 0 || n_neurons == 0) return -1;

    /* PA-3 walkthrough fix: warn loudly if SNN pop is wider than the
     * bridge's matching cap. neuron_idx % cap aliases distinct neurons
     * onto the same bridge slot — at high collision counts each bridge
     * slot accumulates spikes from many sources, which together with
     * activation += 1.0 per spike approaches the runaway regime that
     * already destroyed sparsity once (commit 5d47666ae). The decay in
     * snn_language_bridge_tick keeps the activation bounded but only if
     * tau_ms stays sane; logging the collision factor up front lets
     * operators see the budget. */
    {
        uint32_t cap = (role == SNN_LANG_POP_ROLE_WORD)
                         ? bridge->word_pops_capacity
                         : bridge->concept_pops_capacity;
        if (cap > 0 && n_neurons > cap) {
            uint32_t collision_factor = (n_neurons + cap - 1u) / cap;
            LOG_WARN(LOG_MODULE,
                     "attach_snn_pop: pop_id=%d n_neurons=%u > bridge cap=%u "
                     "(role=%s, collision_factor=%u). decay_tau must stay "
                     "low enough that activation stays bounded.",
                     snn_pop_id, n_neurons, cap,
                     role == SNN_LANG_POP_ROLE_WORD ? "WORD" : "CONCEPT",
                     collision_factor);
            bridge->stats.attach_collision_warnings++;
        }
    }

    /* Update existing slot if pop_id is already attached. */
    for (uint32_t i = 0; i < SNN_LANG_MAX_ATTACHED_POPS; i++) {
        if (bridge->attached_pops[i].snn_pop_id == snn_pop_id) {
            bridge->attached_pops[i].n_neurons = n_neurons;
            bridge->attached_pops[i].role      = role;
            return 0;
        }
    }
    /* Otherwise grab the first free slot. */
    for (uint32_t i = 0; i < SNN_LANG_MAX_ATTACHED_POPS; i++) {
        if (bridge->attached_pops[i].snn_pop_id < 0) {
            bridge->attached_pops[i].snn_pop_id = snn_pop_id;
            bridge->attached_pops[i].n_neurons  = n_neurons;
            bridge->attached_pops[i].role       = role;
            bridge->n_attached_pops++;
            return 0;
        }
    }
    return -1;  /* table full */
}

int snn_language_bridge_drain_pop_spikes(snn_language_bridge_t* bridge,
                                           int snn_pop_id,
                                           const float* spike_output,
                                           uint32_t n_neurons,
                                           float current_time_ms)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (!spike_output) return -1;
    if (!bridge->config.enable_snn_spike_routing) return 0;  /* gated off */

    /* Look up role for this pop_id. */
    snn_lang_pop_role_t role = SNN_LANG_POP_ROLE_CONCEPT;
    bool found = false;
    for (uint32_t i = 0; i < SNN_LANG_MAX_ATTACHED_POPS; i++) {
        if (bridge->attached_pops[i].snn_pop_id == snn_pop_id) {
            role = bridge->attached_pops[i].role;
            found = true;
            break;
        }
    }
    if (!found) return -1;

    bridge->current_time_ms = current_time_ms;

    /* Walk spikes. neuron_idx → bridge pop index via modulo of bridge cap.
     * Multiple SNN neurons can map to the same bridge pop (synonyms-by-
     * collision; same pattern as the existing form_hash mirror). STDP
     * trace updates merge near-time spikes naturally. */
    if (role == SNN_LANG_POP_ROLE_WORD) {
        /* Increment-1 (2026-06-02): cache the raw Broca spike_output so
         * decode_spikes can rank words by per-ensemble activity at produce time
         * (the main SNN is not stepped on the produce path). Lazily (re)allocate
         * to the pop width; Broca width is stable so this allocates once. */
        if (!bridge->broca_spike_cache || bridge->broca_spike_cache_cap < n_neurons) {
            if (bridge->broca_spike_cache) nimcp_free(bridge->broca_spike_cache);
            bridge->broca_spike_cache = (float*)nimcp_calloc(n_neurons, sizeof(float));
            bridge->broca_spike_cache_cap =
                bridge->broca_spike_cache ? n_neurons : 0u;
        }
        if (bridge->broca_spike_cache && bridge->broca_spike_cache_cap >= n_neurons) {
            memcpy(bridge->broca_spike_cache, spike_output,
                   (size_t)n_neurons * sizeof(float));
        }
        const uint32_t cap = bridge->word_pops_capacity > 0
                              ? bridge->word_pops_capacity
                              : SNN_LANG_MAX_WORD_POPS;
        for (uint32_t n = 0; n < n_neurons; n++) {
            if (spike_output[n] > 0.5f) {
                snn_language_bridge_word_spike(bridge, n % cap, current_time_ms);
            }
        }
    } else {
        const uint32_t cap = bridge->concept_pops_capacity > 0
                              ? bridge->concept_pops_capacity
                              : SNN_LANG_MAX_CONCEPT_POPS;
        for (uint32_t n = 0; n < n_neurons; n++) {
            if (spike_output[n] > 0.5f) {
                snn_language_bridge_concept_spike(bridge, n % cap, current_time_ms);
            }
        }
    }
    return 0;
}

int snn_language_bridge_tick(snn_language_bridge_t* bridge, float dt_ms)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (!isfinite(dt_ms) || dt_ms < 0.0f) return -1;

    /* Always-on decay (independent of spike-routing flag) — cheap and
     * idempotent; keeps activations bounded even when callers synthesize
     * spikes through other paths. */
    float tau = bridge->config.activation_tau_ms > 0.0f
                  ? bridge->config.activation_tau_ms : 200.0f;
    float decay = expf(-dt_ms / tau);

    for (uint32_t c = 0; c < bridge->concept_pops_capacity; c++) {
        bridge->concept_pops[c].activation *= decay;
    }
    for (uint32_t w = 0; w < bridge->word_pops_capacity; w++) {
        bridge->word_pops[w].activation *= decay;
    }
    return 0;
}

int snn_language_bridge_get_attached_pop(const snn_language_bridge_t* bridge,
                                          uint32_t index,
                                          int* out_pop_id,
                                          uint32_t* out_n_neurons,
                                          snn_lang_pop_role_t* out_role)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (index >= SNN_LANG_MAX_ATTACHED_POPS) return -1;
    if (out_pop_id)     *out_pop_id     = bridge->attached_pops[index].snn_pop_id;
    if (out_n_neurons)  *out_n_neurons  = bridge->attached_pops[index].n_neurons;
    if (out_role)       *out_role       = bridge->attached_pops[index].role;
    return 0;
}

/* Lazy fill: ensure word w's embedding is cached. Returns 1 if cached
 * (success or already filled), 0 if no embedding for this word, -1 on
 * setup failure. After return 1, word_emb_cache[w] and word_emb_norm[w]
 * are populated. Inline to keep the decode hot path tight. */
static inline int emb_cache_ensure(snn_language_bridge_t* bridge, uint32_t w)
{
    if (!bridge->emb_lookup_fn || !bridge->word_emb_cache) return -1;
    if (w >= bridge->word_pops_capacity) return -1;
    if (bridge->word_emb_cached[w]) {
        /* 1 = embedding present and cached, 2 = looked up but missing. */
        return (bridge->word_emb_cached[w] == 1) ? 1 : 0;
    }
    const char* word = bridge->word_pops[w].word_form;
    if (!word || !word[0]) {
        bridge->word_emb_cached[w] = 2;  /* mark as missing, don't retry */
        return 0;
    }
    float* row = bridge->word_emb_cache + (size_t)w * bridge->emb_dim;
    int rc = bridge->emb_lookup_fn(bridge->emb_lookup_ctx,
                                    word, row, bridge->emb_dim);
    if (rc != 0) {
        bridge->word_emb_cached[w] = 2;
        return 0;
    }
    /* Compute and cache the norm so cosine in decode is one division. */
    float n2 = 0.0f;
    for (uint32_t d = 0; d < bridge->emb_dim; d++) n2 += row[d] * row[d];
    bridge->word_emb_norm[w] = sqrtf(n2 + 1e-6f);
    bridge->word_emb_cached[w] = 1;
    return 1;
}

/* Patch A: rebuild word_norm_sq[] from current binding state. Called after
 * binding load (where node->binding = b overwrites the weight that
 * binding_insert just norm-accounted), and exposed via the public API for
 * mid-flight live salvage of brains that pre-date Patch A. */
int snn_language_bridge_recompute_norms(snn_language_bridge_t* bridge)
{
    /* Option-1 (Slice A): the bridge no longer holds binding weights, so
     * word_norm_sq has no meaningful content to recompute. Zero the cache
     * (defensive — defaults to zero on calloc and stays there) and return
     * success. Existing callers (Python binding +
     * nimcp_brain_recompute_snn_language_bridge_norms) keep their contract. */
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (bridge->word_norm_sq) {
        memset(bridge->word_norm_sq, 0,
               bridge->word_pops_capacity * sizeof(float));
    }
    return 0;
}

int snn_language_bridge_prune(snn_language_bridge_t* bridge, float threshold)
{
    /* Option-1 (Slice A): no-op stub. Bridge has no weights to prune. */
    (void)threshold;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    return 0;
}

//=============================================================================
// Phase 3: Spike-Driven Language Production (Broca pathway)
//=============================================================================

int snn_language_bridge_produce_word(snn_language_bridge_t* bridge,
                                      const float* concept_activations,
                                      uint32_t num_concepts,
                                      snn_lang_word_result_t* result)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC ||
        !concept_activations || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_produce_word: bridge, concept_activations, or result is NULL");
        return -1;
    }

    snn_lang_word_result_t top;
    uint32_t num_out = 0;
    int rc = snn_language_bridge_decode_spikes(bridge, concept_activations,
                                               num_concepts, &top, 1, &num_out);
    if (rc != 0 || num_out == 0) return -1;

    *result = top;
    return 0;
}

/* TIER1-A forward declaration. Beam-K decoding is a separate code path that
 * runs only when produce_beam_width > 1; greedy / beam_width <= 1 stays on
 * the legacy single-beam loop below. Defined further down in this file. */
static int produce_beam_search(snn_language_bridge_t* bridge,
                                const float* semantic_intent,
                                uint32_t intent_dim,
                                uint32_t beam_width,
                                snn_lang_production_result_t* result);

/* Tier-4 #16: produce-loop latency telemetry — work delegated to this static
 * impl from the public wrapper, which times the call. NULL/magic guards run
 * in the wrapper BEFORE clock_gettime so timing only counts work that
 * actually happened. Beam dispatch (TIER1-A) lives inside this impl so beam
 * runs are timed too. */
static int bridge_produce_impl(snn_language_bridge_t* bridge,
                                const float* semantic_intent,
                                uint32_t intent_dim,
                                snn_lang_production_result_t* result)
{
    /* TIER1-A: dispatch to beam-K decoder when configured. The beam path
     * itself increments total_produce_calls + writes the EMA, so we return
     * directly. beam_width = 0 or 1 falls through to the legacy greedy loop
     * which preserves bit-for-bit prior behavior. */
    {
        uint32_t bw = bridge->config.produce_beam_width;
        if (bw > 1) {
            if (bw > 16) bw = 16;
            return produce_beam_search(bridge, semantic_intent, intent_dim,
                                       bw, result);
        }
    }

    bridge->stats.total_produce_calls++;
    memset(result, 0, sizeof(*result));

    // Map semantic intent to concept activations
    // Use first num_concept_pops dimensions of intent (or zero-pad)
    uint32_t n_concepts = bridge->num_concept_pops;

    /* PA-2: separate intent / state / concept_acts buffers. Intent stays
     * constant across the produce loop. State evolves recurrently from
     * picked-word reverse-encodings. concept_acts (the actual decode input)
     * is recomputed each step as a blend of the two. The legacy in-place
     * 70/30 update was equivalent to the special case
     * intent_persistence = 0, word_feedback = 0.3 — which is the default. */
    float* intent       = nimcp_calloc(n_concepts, sizeof(float));
    float* state        = nimcp_calloc(n_concepts, sizeof(float));
    float* concept_acts = nimcp_calloc(n_concepts, sizeof(float));
    if (!intent || !state || !concept_acts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "snn_language_bridge_produce: failed to allocate decode buffers");
        nimcp_free(intent); nimcp_free(state); nimcp_free(concept_acts);
        return -1;
    }

    uint32_t copy_dim = (intent_dim < n_concepts) ? intent_dim : n_concepts;
    for (uint32_t i = 0; i < copy_dim; i++) {
        intent[i] = fmaxf(0.0f, semantic_intent[i]); // ReLU activation
    }
    /* state_0 = intent (so the first decode sees the same input as legacy
     * regardless of intent_persistence). */
    memcpy(state, intent, n_concepts * sizeof(float));
    memcpy(concept_acts, intent, n_concepts * sizeof(float));

    // Iterative word production with refractory inhibition
    char text_buf[2048] = {0};
    uint32_t text_pos = 0;
    /* TB-7: max_produce_words > 0 overrides the legacy 32-word cap; 0
     * (default) preserves it. Also clamped to a sane upper bound 1024 to
     * keep used_words[] allocation bounded. */
    const uint32_t max_cfg = bridge->config.max_produce_words;
    uint32_t max_words = (max_cfg > 0) ? max_cfg : 32u;
    if (max_words > 1024u) max_words = 1024u;
    const uint32_t min_words_cfg = bridge->config.min_produce_words;
    bool max_truncated = false;  /* set if the loop terminates via the cap */
    float total_confidence = 0.0f;
    uint32_t word_count = 0;

    // Track used words for refractory
    uint32_t* used_words = nimcp_calloc(max_words, sizeof(uint32_t));
    if (!used_words) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "snn_language_bridge_produce: failed to allocate used_words");
        nimcp_free(intent); nimcp_free(state); nimcp_free(concept_acts);
        return -1;
    }

    /* PA-2: read the recurrent knobs once. Clamp defensively. */
    float ip = bridge->config.intent_persistence;
    float wf = bridge->config.word_feedback;
    if (!isfinite(ip) || ip < 0.0f) ip = 0.0f;
    if (ip > 1.0f) ip = 1.0f;
    if (!isfinite(wf) || wf < 0.0f) wf = 0.0f;
    if (wf > 1.0f) wf = 1.0f;

    /* Walkthrough round 2 (PA-2 × PA-5): pin the embedding query to the
     * immutable original intent for every decode_spikes call inside this
     * produce loop. Without this, PA-5's GloVe cosine reads concept_acts
     * which PA-2 evolves into binding space — and the GloVe term would
     * see a corrupted query. Reset on every exit path below. */
    bridge->emb_query_override     = intent;
    bridge->emb_query_override_dim = n_concepts;

    /* PA-6: pull config sampling knobs once per produce call.
     * PA-6+: sampling_mode dispatch — 0 = auto, 1 = force softmax+top-p,
     * 2 = quantum-Monte-Carlo MCMC. */
    const float temperature = bridge->config.temperature;
    const float top_p = (bridge->config.top_p > 0.0f) ? bridge->config.top_p : 1.0f;
    uint32_t topk = bridge->config.produce_topk;
    if (topk == 0)  topk = 5;
    if (topk > 32)  topk = 32;
    const int sampling_mode = bridge->config.sampling_mode;

    /* DK-A+: accumulator for the average per-step entropy_confidence. */
    float total_entropy_conf = 0.0f;
    uint32_t entropy_steps = 0;

    /* TIER1-B/C: pull EOS + repetition penalty knobs once. Default-disabled
     * values keep the loop bit-for-bit identical to the prior path. */
    const uint32_t eos_pop      = bridge->config.eos_word_pop;  /* UINT32_MAX = off */
    float          rep_penalty  = bridge->config.repetition_penalty;
    uint32_t       rep_window   = bridge->config.repetition_window;
    if (!isfinite(rep_penalty) || rep_penalty < 0.0f) rep_penalty = 0.0f;
    if (rep_penalty > 1.0f) rep_penalty = 1.0f;
    if (rep_penalty > 0.0f && rep_window == 0) rep_window = 3;

    /* EOS stopping criterion (default-OFF). Pull thresholds once. The
     * activation threshold is compared against L2 magnitude of the
     * post-recurrent-update concept_acts; the confidence threshold is
     * compared against the just-emitted word's per-step confidence.
     * Defaults: enable=false, min_act=0.05, min_conf=0.01. Defensive
     * clamping — non-finite or negative thresholds disable the
     * respective sub-check by setting it to -1, which can never trip. */
    const bool  eos_stop_enabled = bridge->config.enable_eos_stopping;
    float       eos_min_act      = bridge->config.eos_min_activation;
    float       eos_min_conf     = bridge->config.eos_min_confidence;
    if (!isfinite(eos_min_act)  || eos_min_act  < 0.0f) eos_min_act  = -1.0f;
    if (!isfinite(eos_min_conf) || eos_min_conf < 0.0f) eos_min_conf = -1.0f;
    bool eos_terminated = false;

    /* Slice 4 — lateral inhibition opt-in. When the bridge-level flag is
     * on, route the per-word decode through the competitive-settling path
     * instead of one-shot argmax. Default-off preserves bit-for-bit
     * legacy behavior. cascade_stage_lexical -> grounded_language_produce
     * -> here, so flipping this flag from cascade is the wiring point. */
    const bool lat_inh_on = bridge->config.enable_lateral_inhibition;

    for (uint32_t w = 0; w < max_words; w++) {
        // Get top word
        snn_lang_word_result_t word_result;
        uint32_t num_out = 0;
        snn_lang_word_result_t topK[32];
        int rc = lat_inh_on
            ? snn_language_bridge_decode_with_lateral_inhibition(
                bridge, concept_acts, n_concepts, topK, topk, &num_out)
            : snn_language_bridge_decode_spikes(
                bridge, concept_acts, n_concepts, topK, topk, &num_out);
        if (rc != 0 || num_out == 0) break;

        /* Filter out refractory (already-used) candidates. */
        uint32_t valid_idx[32];
        uint32_t n_valid = 0;
        for (uint32_t k = 0; k < num_out; k++) {
            bool refractory = false;
            for (uint32_t u = 0; u < word_count; u++) {
                if (used_words[u] == topK[k].word_pop) {
                    refractory = true;
                    break;
                }
            }
            if (!refractory) valid_idx[n_valid++] = k;
        }
        if (n_valid == 0) break;

        /* TIER1-C: apply the n-gram repetition penalty in-place to topK
         * activations (which decode_spikes set to the cosine score). Each
         * candidate's activation is multiplied by (1 - rep_penalty) once
         * per occurrence in the last `rep_window` picks. With the default
         * rep_penalty == 0 this loop is skipped entirely (legacy path). */
        if (rep_penalty > 0.0f && rep_window > 0 && word_count > 0) {
            uint32_t lookback_start = (word_count > rep_window)
                                         ? (word_count - rep_window) : 0;
            float scale_per_match = 1.0f - rep_penalty;
            for (uint32_t i = 0; i < n_valid; i++) {
                uint32_t cand_pop = topK[valid_idx[i]].word_pop;
                /* Although refractory filtering already drops exact
                 * duplicates, the window may extend over a longer history
                 * than the refractory list when callers shorten it later.
                 * Count matches strictly inside the window. */
                uint32_t matches = 0;
                for (uint32_t u = lookback_start; u < word_count; u++) {
                    if (used_words[u] == cand_pop) matches++;
                }
                for (uint32_t m = 0; m < matches; m++) {
                    topK[valid_idx[i]].activation *= scale_per_match;
                }
            }
        }

        /* PA-6+: when sampling_mode == 0 (auto) and temperature == 0, stay
         * on argmax. Otherwise compute the softmax posterior (used by both
         * mode 1 / 2). Mode 1 & 2 require T > 0 — if caller set the mode
         * with T = 0, fall back to argmax to avoid div-by-zero. */
        const bool need_posterior =
            (sampling_mode == 1 || sampling_mode == 2) ||
            (sampling_mode == 0 && temperature > 0.0f);

        if (!need_posterior || n_valid == 1) {
            /* Legacy hard-argmax path: pick the first valid (highest cosine).
             * TIER1-C: when rep_penalty has scaled the activations, the
             * highest-scoring candidate may no longer be valid_idx[0]; scan
             * for the actual max. With rep_penalty == 0 this scan still
             * picks valid_idx[0] because decode_spikes returns topK in
             * descending cosine order. */
            uint32_t best_local = 0;
            if (rep_penalty > 0.0f && n_valid > 1) {
                float best_act = topK[valid_idx[0]].activation;
                for (uint32_t i = 1; i < n_valid; i++) {
                    if (topK[valid_idx[i]].activation > best_act) {
                        best_act = topK[valid_idx[i]].activation;
                        best_local = i;
                    }
                }
            }
            word_result = topK[valid_idx[best_local]];
        } else {
            /* PA-6: softmax sample over valid candidates. Subtract max before
             * exp() for numerical stability. Then top-p (nucleus) truncation:
             * sort probs descending, keep until cumulative ≥ top_p, renormalize.
             * Finally inverse-CDF sample. */
            float scores[32];
            float max_score = -FLT_MAX;
            for (uint32_t i = 0; i < n_valid; i++) {
                scores[i] = topK[valid_idx[i]].activation;  /* cosine score */
                if (scores[i] > max_score) max_score = scores[i];
            }
            float probs[32];
            float sum = 0.0f;
            float T = (temperature > 0.0f) ? temperature : 1.0f;
            for (uint32_t i = 0; i < n_valid; i++) {
                probs[i] = expf((scores[i] - max_score) / T);
                sum += probs[i];
            }
            if (sum <= 0.0f) {
                word_result = topK[valid_idx[0]];
            } else {
                for (uint32_t i = 0; i < n_valid; i++) probs[i] /= sum;

                /* DK-A+: quantum-Shannon entropy confidence over the
                 * candidate posterior. 1 − H(p)/log2(K) — peaked → 1,
                 * flat → 0. Computed in nats then normalized via Hmax in
                 * nats too, so the ratio is unit-free in [0, 1]. */
                if (n_valid > 1) {
                    float H = 0.0f;
                    for (uint32_t i = 0; i < n_valid; i++) {
                        float p = probs[i];
                        if (p > 1e-12f) H -= p * logf(p);
                    }
                    float Hmax = logf((float)n_valid);  /* nats */
                    float ec = (Hmax > 0.0f) ? (1.0f - H / Hmax) : 1.0f;
                    if (ec < 0.0f) ec = 0.0f;
                    if (ec > 1.0f) ec = 1.0f;
                    total_entropy_conf += ec;
                    entropy_steps++;
                }

                /* PA-6+: q-MC route. Feed sqrt(probs) as quantum
                 * amplitudes — the quantum-MC importance sampler then
                 * draws |amp|² == probs. Single quantum measurement. */
                if (sampling_mode == 2) {
                    float amps[32];
                    for (uint32_t i = 0; i < n_valid; i++) {
                        amps[i] = sqrtf(probs[i]);
                    }
                    uint32_t qseed = (uint32_t)(bridge_rng_u64(bridge) & 0xFFFFFFFFu);
                    if (qseed == 0) qseed = 1;
                    uint32_t chosen = qmc_measure_importance(amps, n_valid,
                                                              /*proposal=*/NULL,
                                                              &qseed);
                    if (chosen >= n_valid) chosen = 0;
                    word_result = topK[valid_idx[chosen]];
                    goto sample_done;
                }

                if (top_p < 1.0f) {
                    /* Sort indices by descending prob (insertion sort, n≤32). */
                    uint32_t order[32];
                    for (uint32_t i = 0; i < n_valid; i++) order[i] = i;
                    for (uint32_t i = 1; i < n_valid; i++) {
                        uint32_t key = order[i];
                        int32_t j = (int32_t)i - 1;
                        while (j >= 0 && probs[order[j]] < probs[key]) {
                            order[j + 1] = order[j];
                            j--;
                        }
                        order[j + 1] = key;
                    }
                    /* Truncate tail past cumulative top_p. */
                    float cum = 0.0f;
                    uint32_t keep = n_valid;
                    for (uint32_t i = 0; i < n_valid; i++) {
                        cum += probs[order[i]];
                        if (cum >= top_p) { keep = i + 1; break; }
                    }
                    /* Zero-out tail and renormalize the kept head. */
                    float new_sum = 0.0f;
                    for (uint32_t i = 0; i < n_valid; i++) {
                        if (i >= keep) probs[order[i]] = 0.0f;
                        else new_sum += probs[order[i]];
                    }
                    if (new_sum > 0.0f) {
                        for (uint32_t i = 0; i < n_valid; i++) probs[i] /= new_sum;
                    } else {
                        /* PA-6 fix (walkthrough round 1): pathological top_p
                         * (e.g. < smallest prob mass) zeroes everything. The
                         * inverse-CDF below would deterministically pick
                         * `n_valid - 1` (silent argmin). Fall back to argmax
                         * — the most-likely candidate still respects the
                         * caller's intent under "keep only the highest mass". */
                        word_result = topK[valid_idx[order[0]]];
                        goto sample_done;
                    }
                }

                /* Inverse-CDF sample. */
                float u = bridge_rng_unit(bridge);
                float cum = 0.0f;
                uint32_t chosen = n_valid - 1;  /* fallback to last */
                for (uint32_t i = 0; i < n_valid; i++) {
                    cum += probs[i];
                    if (u < cum) { chosen = i; break; }
                }
                word_result = topK[valid_idx[chosen]];
            }
        }
sample_done:;

        /* TIER1-B: EOS halts the loop cleanly. The EOS word is NOT appended
         * to text_buf nor counted in word_count — production simply stops
         * at the prior word, which is the desired behavior for trainers
         * that bind EOS to "end of sentence" concept activations. Default
         * eos_pop == UINT32_MAX never matches a valid word_pop.
         *
         * TB-7: when min_words_cfg > 0 and the current emission count is
         * below that minimum, suppress EOS — fall back to the highest-
         * scoring non-EOS valid candidate this step instead. Each
         * suppression bumps stats.length_min_suppressions. If every
         * remaining valid candidate IS the EOS pop, the suppression
         * cannot fire (no replacement available) and the loop terminates
         * via the legacy break. */
        if (eos_pop != UINT32_MAX && word_result.word_pop == eos_pop) {
            if (min_words_cfg > 0 && word_count < min_words_cfg) {
                /* Walk valid_idx[] for the best non-EOS candidate. After
                 * TIER1-C repetition penalty + sampling, topK[].activation
                 * is the score we should rank by. */
                int32_t replacement = -1;
                float   replacement_act = -FLT_MAX;
                for (uint32_t i = 0; i < n_valid; i++) {
                    uint32_t cand_pop = topK[valid_idx[i]].word_pop;
                    if (cand_pop == eos_pop) continue;
                    float a = topK[valid_idx[i]].activation;
                    if (a > replacement_act) {
                        replacement_act = a;
                        replacement = (int32_t)i;
                    }
                }
                if (replacement >= 0) {
                    word_result = topK[valid_idx[replacement]];
                    bridge->stats.length_min_suppressions++;
                } else {
                    /* No non-EOS alternative — accept EOS and halt. */
                    break;
                }
            } else {
                break;
            }
        }

        /* Stop if confidence too low — but honor the operator's
         * min_produce_words floor, exactly as the EOS pick (line ~2042)
         * and the beam decoder's EOS path (line ~2610) already do. TB-7
         * taught those two stop conditions to respect the floor; this
         * older confidence-floor break predated TB-7 and was never
         * updated. On an undertrained bridge every post-first word sits
         * below 0.01, so the greedy path collapsed to a 1-word utterance
         * even when the caller explicitly asked for more — which in turn
         * starved the cascade self-train bigram/trigram path (it needs
         * >=2 tokens). With the default min_produce_words == 0 the guard
         * is a no-op: the `else break` fires bit-for-bit as before. Each
         * suppression bumps length_min_suppressions to mirror the EOS
         * telemetry. */
        if (word_result.confidence < 0.01f && word_count > 0) {
            if (min_words_cfg > 0 && word_count < min_words_cfg) {
                bridge->stats.length_min_suppressions++;
            } else {
                break;
            }
        }

        // Append word to text
        const char* word = word_result.word_form;
        size_t wlen = strlen(word);
        if (text_pos + wlen + 2 >= sizeof(text_buf)) break;

        if (text_pos > 0) {
            text_buf[text_pos++] = ' ';
        }
        memcpy(text_buf + text_pos, word, wlen);
        text_pos += wlen;

        used_words[word_count] = word_result.word_pop;
        total_confidence += word_result.confidence;
        word_count++;

        /* TB-8: per-token streaming callback. Fire BEFORE TB-7's max-cap
         * break so the consumer always sees one callback per emitted word —
         * including the very last word when max_produce_words is what
         * terminates the loop. Pre-fix the cap-break short-circuited the
         * callback for the cap-final word, so a consumer streaming N words
         * to a UI saw only N-1 callbacks. The callback's own abort path
         * still wins (returning non-zero stops emission), but a non-aborting
         * callback always sees every emitted word.
         * Returning non-zero aborts; accumulated text stays in result->text. */
        if (bridge->stream_cb) {
            bridge->stats.stream_callbacks_invoked++;
            int abort_rc = bridge->stream_cb(
                /*word_index=*/word_count - 1,
                /*word_form=*/ word_result.word_form,
                /*word_pop=*/  word_result.word_pop,
                /*confidence=*/word_result.confidence,
                /*user_data=*/ bridge->stream_user_data);
            if (abort_rc != 0) {
                bridge->stats.stream_aborts++;
                break;
            }
        }

        /* TB-7: terminate cleanly once max_produce_words has been reached. */
        if (max_cfg > 0 && word_count >= max_cfg) {
            max_truncated = true;
            break;
        }

        /* PA-2: recurrent update. Evolve state from the just-picked word's
         * reverse-encoding, then rebuild concept_acts as the per-step blend
         * of constant intent + evolving state. With intent_persistence = 0
         * and word_feedback = 0.3, this reproduces the legacy single-buffer
         * 70/30 update bit-for-bit. */
        float* word_concepts = nimcp_calloc(n_concepts, sizeof(float));
        if (word_concepts) {
            snn_language_bridge_encode_word(bridge, word_result.word_pop,
                                            word_concepts, n_concepts);
            float keep = 1.0f - wf;
            for (uint32_t c = 0; c < n_concepts; c++) {
                state[c] = keep * state[c] + wf * word_concepts[c];
            }
            nimcp_free(word_concepts);
        }
        for (uint32_t c = 0; c < n_concepts; c++) {
            concept_acts[c] = ip * intent[c] + (1.0f - ip) * state[c];
        }

        /* EOS stopping criterion (opt-in via enable_eos_stopping).
         * Evaluated after the recurrent state update so the activation
         * sample reflects what the NEXT decode pass would see. We
         * respect the caller's min_words_cfg floor — no early stop
         * before the configured minimum has been emitted. With the
         * default flag OFF, this entire block is skipped (preserves
         * legacy behavior bit-for-bit). */
        if (eos_stop_enabled && word_count >= min_words_cfg) {
            /* L2 magnitude of the just-rebuilt concept_acts. */
            float act_sq = 0.0f;
            for (uint32_t c = 0; c < n_concepts; c++) {
                act_sq += concept_acts[c] * concept_acts[c];
            }
            float act_mag = sqrtf(act_sq);
            bool  act_undershot  = (eos_min_act  >= 0.0f) && (act_mag < eos_min_act);
            bool  conf_undershot = (eos_min_conf >= 0.0f) &&
                                   (word_result.confidence < eos_min_conf);
            if (act_undershot || conf_undershot) {
                eos_terminated = true;
                break;
            }
        }
    }

    nimcp_free(used_words);
    nimcp_free(intent);
    nimcp_free(state);
    nimcp_free(concept_acts);

    /* TB-7: bump the max-truncation counter once per produce call where the
     * caller-set cap fired. The legacy implicit 32-word cap (when max_cfg
     * is 0) does NOT bump the counter — that path is invisible to callers
     * who never opted into length control. */
    if (max_truncated) {
        bridge->stats.length_max_truncations++;
    }

    /* EOS stopping telemetry: bump symmetrical counters so callers can
     * tell from a single stats read how each produce call terminated.
     * total_eos_terminations bumps when the activation/confidence
     * thresholds tripped; total_max_truncations bumps for any
     * cap-driven exit (mirrors length_max_truncations but also covers
     * the legacy implicit 32-word cap so consumers can compute an
     * EOS-vs-cap ratio without a second counter). Both stay 0 when the
     * EOS feature is unused, matching backward-compat semantics. */
    if (eos_terminated) {
        bridge->stats.total_eos_terminations++;
    }
    if (word_count >= max_words) {
        bridge->stats.total_max_truncations++;
    }

    /* Walkthrough round 2: clear the embedding-query override so any
     * subsequent direct caller of decode_spikes (outside this produce
     * call) reverts to using concept_rates as the GloVe query. */
    bridge->emb_query_override     = NULL;
    bridge->emb_query_override_dim = 0;

    if (word_count == 0) return -1;

    text_buf[text_pos] = '\0';
    result->text = nimcp_malloc(text_pos + 1);
    if (!result->text) return -1;
    memcpy(result->text, text_buf, text_pos + 1);

    result->word_count = word_count;
    result->spike_confidence = total_confidence / word_count;
    result->fluency = fminf(1.0f, (float)word_count / 8.0f);
    result->creativity = 0.0f;  // Set by creative_produce
    /* DK-A+: average per-step entropy_confidence over the steps where we
     * actually had a posterior. 0 when only argmax fired (no posterior to
     * measure). */
    result->entropy_confidence = (entropy_steps > 0)
                                   ? (total_entropy_conf / (float)entropy_steps)
                                   : 0.0f;

    /* Running average of per-call mean word confidence. EMA over the
     * total_produce_calls counter so the diagnostic stays bounded and
     * tracks recent behavior without unbounded growth. alpha=1/N gives
     * a true running mean; we use 0.05 floor so the metric stays
     * responsive once N is large. */
    {
        float alpha = (bridge->stats.total_produce_calls > 0)
            ? 1.0f / (float)bridge->stats.total_produce_calls : 1.0f;
        if (alpha < 0.05f) alpha = 0.05f;
        bridge->stats.avg_word_confidence =
            (1.0f - alpha) * bridge->stats.avg_word_confidence
            + alpha * result->spike_confidence;
    }

    return 0;
}

/* TIER1-A: beam-K decoding. Each beam tracks its own state buffer, used-
 * word refractory list (which doubles as the produced sequence), cumulative
 * log-prob, total confidence, finished flag, and entropy stats. At each
 * step, every active beam expands its top-V candidates; the union pool is
 * pruned to K best by length-normalized cumulative score
 *   logprob / token_count^0.6
 * (Wu et al. length-norm). Finished beams (EOS, confidence floor, or
 * refractory exhaustion) are preserved across steps and ranked together
 * with the active beams at the end. The highest-scoring beam wins.
 *
 * Memory layout: state buffers and used_word lists are heap-allocated per
 * beam (one block each, sized n_concepts * floats and max_words * uint32).
 * 16 beams × 32 words × ~thousands of concepts is bounded by O(K·N) which
 * is fine for any sane brain configuration. */

#define BEAM_MAX_K        16
#define BEAM_MAX_TOPV     8     /* per-beam expansion fanout */
#define BEAM_MAX_WORDS    32

typedef struct {
    float*   state;             /* n_concepts */
    float*   concept_acts;      /* n_concepts */
    uint32_t used_words[BEAM_MAX_WORDS];
    char     text_buf[2048];
    uint32_t text_pos;
    uint32_t n_used;
    float    cum_logprob;       /* sum log(prob) along this beam */
    float    total_confidence;
    float    entropy_conf_sum;
    uint32_t entropy_steps;
    bool     finished;          /* EOS / refractory / confidence floor */
    bool     active;            /* slot in use */
} beam_t;

/* Length-normalized score: cum_logprob / max(1, token_count)^alpha.
 * alpha defaults to 0.6 (Wu et al.) but is configurable via
 * config.beam_length_norm_alpha (clamped [0.1, 1.5]).
 * Empty beams (token_count == 0) get -inf so they can't beat any real one. */
static inline float beam_score_alpha(const beam_t* b, float alpha)
{
    if (!b->active) return -FLT_MAX;
    if (b->n_used == 0) return -FLT_MAX;
    float n = (float)b->n_used;
    float denom = powf(n, alpha);
    if (denom < 1e-6f) denom = 1e-6f;
    return b->cum_logprob / denom;
}

static void beam_free(beam_t* b)
{
    if (b->state)        nimcp_free(b->state);
    if (b->concept_acts) nimcp_free(b->concept_acts);
    b->state        = NULL;
    b->concept_acts = NULL;
    b->active       = false;
}

/* Forward decl — HNN energy accessor. Borrowed pointer; the bridge's
 * `void* hnn` slot is cast to this type at call sites only. Keeps the
 * LNN header out of this TU. */
typedef struct lnn_hamiltonian_net lnn_hamiltonian_net_t;
extern float lnn_hamiltonian_get_energy_deviation(const lnn_hamiltonian_net_t* net);

static int beam_init(beam_t* b, uint32_t n_concepts, const float* intent_buf)
{
    memset(b, 0, sizeof(*b));
    b->state        = nimcp_calloc(n_concepts, sizeof(float));
    b->concept_acts = nimcp_calloc(n_concepts, sizeof(float));
    if (!b->state || !b->concept_acts) {
        beam_free(b);
        return -1;
    }
    memcpy(b->state,        intent_buf, n_concepts * sizeof(float));
    memcpy(b->concept_acts, intent_buf, n_concepts * sizeof(float));
    b->active = true;
    return 0;
}

/* Deep copy: text_buf, used_words, scalars + reallocate state/concept_acts. */
static int beam_clone(beam_t* dst, const beam_t* src, uint32_t n_concepts)
{
    memset(dst, 0, sizeof(*dst));
    dst->state        = nimcp_calloc(n_concepts, sizeof(float));
    dst->concept_acts = nimcp_calloc(n_concepts, sizeof(float));
    if (!dst->state || !dst->concept_acts) {
        beam_free(dst);
        return -1;
    }
    memcpy(dst->state,        src->state,        n_concepts * sizeof(float));
    memcpy(dst->concept_acts, src->concept_acts, n_concepts * sizeof(float));
    memcpy(dst->used_words,   src->used_words,   sizeof(dst->used_words));
    memcpy(dst->text_buf,     src->text_buf,     sizeof(dst->text_buf));
    dst->text_pos         = src->text_pos;
    dst->n_used           = src->n_used;
    dst->cum_logprob      = src->cum_logprob;
    dst->total_confidence = src->total_confidence;
    dst->entropy_conf_sum = src->entropy_conf_sum;
    dst->entropy_steps    = src->entropy_steps;
    dst->finished         = src->finished;
    dst->active           = true;
    return 0;
}

static int produce_beam_search(snn_language_bridge_t* bridge,
                                const float* semantic_intent,
                                uint32_t intent_dim,
                                uint32_t beam_width,
                                snn_lang_production_result_t* result)
{
    bridge->stats.total_produce_calls++;
    memset(result, 0, sizeof(*result));

    if (beam_width < 1) beam_width = 1;
    if (beam_width > BEAM_MAX_K) beam_width = BEAM_MAX_K;

    /* Pull length-norm alpha + EOS-stop knobs once. Out-of-range / NaN
     * length-norm alpha falls back to 0.6 (matches the pre-V5 hard-coded
     * exponent so existing callers keep their ranking). */
    float beam_alpha = bridge->config.beam_length_norm_alpha;
    if (!isfinite(beam_alpha) || beam_alpha < 0.1f || beam_alpha > 1.5f) {
        beam_alpha = 0.6f;
    }
    const bool     eos_stop_enabled = bridge->config.enable_eos_stopping;
    const float    eos_min_act      = bridge->config.eos_min_activation;
    const float    eos_min_conf     = bridge->config.eos_min_confidence;
    const uint32_t min_words_cfg    = bridge->config.min_produce_words;
    const uint32_t max_cfg          = bridge->config.max_produce_words;

    uint32_t n_concepts = bridge->num_concept_pops;
    if (n_concepts == 0) return -1;

    /* Immutable original intent (shared across all beams). */
    float* intent = nimcp_calloc(n_concepts, sizeof(float));
    if (!intent) return -1;
    uint32_t copy_dim = (intent_dim < n_concepts) ? intent_dim : n_concepts;
    for (uint32_t i = 0; i < copy_dim; i++) {
        intent[i] = fmaxf(0.0f, semantic_intent[i]);
    }

    /* Pin the GloVe embedding query to the immutable intent for the duration
     * of this call, just like the greedy path. Reset on every exit. */
    bridge->emb_query_override     = intent;
    bridge->emb_query_override_dim = n_concepts;

    /* Read recurrent + sampling knobs once. */
    float ip = bridge->config.intent_persistence;
    float wf = bridge->config.word_feedback;
    if (!isfinite(ip) || ip < 0.0f) ip = 0.0f;
    if (ip > 1.0f) ip = 1.0f;
    if (!isfinite(wf) || wf < 0.0f) wf = 0.0f;
    if (wf > 1.0f) wf = 1.0f;

    const float temperature = bridge->config.temperature;
    uint32_t topk = bridge->config.produce_topk;
    if (topk == 0)  topk = 5;
    if (topk > BEAM_MAX_TOPV) topk = BEAM_MAX_TOPV;

    const uint32_t eos_pop = bridge->config.eos_word_pop;
    float    rep_penalty = bridge->config.repetition_penalty;
    uint32_t rep_window  = bridge->config.repetition_window;
    if (!isfinite(rep_penalty) || rep_penalty < 0.0f) rep_penalty = 0.0f;
    if (rep_penalty > 1.0f) rep_penalty = 1.0f;
    if (rep_penalty > 0.0f && rep_window == 0) rep_window = 3;

    /* Allocate beams. Start with a single seed beam; subsequent steps grow
     * to beam_width via expansion. */
    beam_t beams[BEAM_MAX_K];
    memset(beams, 0, sizeof(beams));
    if (beam_init(&beams[0], n_concepts, intent) != 0) {
        nimcp_free(intent);
        bridge->emb_query_override = NULL;
        bridge->emb_query_override_dim = 0;
        return -1;
    }
    uint32_t n_beams = 1;

    /* Per-step expansion buffers. Each candidate carries: (beam_idx,
     * word_pop, word_form, confidence, log_prob_step, entropy_step). */
    typedef struct {
        uint32_t    beam_idx;
        uint32_t    word_pop;
        const char* word_form;   /* borrowed from topK[..].word_form */
        float       confidence;
        float       log_prob;    /* of this candidate given its parent beam */
        float       entropy_step;/* per-step entropy contribution */
        bool        is_eos;
    } cand_t;

    /* Up to BEAM_MAX_K beams × BEAM_MAX_TOPV candidates each. */
    cand_t cands[BEAM_MAX_K * BEAM_MAX_TOPV];

    for (uint32_t step = 0; step < BEAM_MAX_WORDS; step++) {
        /* If every beam is finished, stop. */
        bool any_active = false;
        for (uint32_t i = 0; i < n_beams; i++) {
            if (beams[i].active && !beams[i].finished) { any_active = true; break; }
        }
        if (!any_active) break;

        uint32_t n_cands = 0;

        for (uint32_t bi = 0; bi < n_beams; bi++) {
            beam_t* B = &beams[bi];
            if (!B->active || B->finished) continue;

            /* Decode top-V from this beam's concept_acts. Slice 4 — opt
             * into lateral inhibition when the bridge flag is on. */
            snn_lang_word_result_t topV[BEAM_MAX_TOPV];
            uint32_t num_out = 0;
            int rc = bridge->config.enable_lateral_inhibition
                ? snn_language_bridge_decode_with_lateral_inhibition(
                    bridge, B->concept_acts, n_concepts, topV, topk, &num_out)
                : snn_language_bridge_decode_spikes(
                    bridge, B->concept_acts, n_concepts, topV, topk, &num_out);
            if (rc != 0 || num_out == 0) {
                B->finished = true;
                continue;
            }

            /* Filter refractory + apply repetition penalty (in-place on the
             * topV.activation cosine score). */
            uint32_t valid_idx[BEAM_MAX_TOPV];
            uint32_t n_valid = 0;
            for (uint32_t k = 0; k < num_out; k++) {
                bool refractory = false;
                for (uint32_t u = 0; u < B->n_used; u++) {
                    if (B->used_words[u] == topV[k].word_pop) {
                        refractory = true; break;
                    }
                }
                if (!refractory) valid_idx[n_valid++] = k;
            }
            if (n_valid == 0) { B->finished = true; continue; }

            if (rep_penalty > 0.0f && rep_window > 0 && B->n_used > 0) {
                uint32_t lookback_start = (B->n_used > rep_window)
                                             ? (B->n_used - rep_window) : 0;
                float scale_per_match = 1.0f - rep_penalty;
                for (uint32_t i = 0; i < n_valid; i++) {
                    uint32_t cand_pop = topV[valid_idx[i]].word_pop;
                    uint32_t matches = 0;
                    for (uint32_t u = lookback_start; u < B->n_used; u++) {
                        if (B->used_words[u] == cand_pop) matches++;
                    }
                    for (uint32_t m = 0; m < matches; m++) {
                        topV[valid_idx[i]].activation *= scale_per_match;
                    }
                }
            }

            /* Compute softmax over valid candidates → log-probs. */
            float scores[BEAM_MAX_TOPV];
            float max_score = -FLT_MAX;
            for (uint32_t i = 0; i < n_valid; i++) {
                scores[i] = topV[valid_idx[i]].activation;
                if (scores[i] > max_score) max_score = scores[i];
            }
            float T = (temperature > 0.0f) ? temperature : 1.0f;
            float sum = 0.0f;
            float probs[BEAM_MAX_TOPV];
            for (uint32_t i = 0; i < n_valid; i++) {
                probs[i] = expf((scores[i] - max_score) / T);
                sum += probs[i];
            }
            if (sum <= 0.0f) {
                /* Degenerate — assign uniform. */
                for (uint32_t i = 0; i < n_valid; i++) {
                    probs[i] = 1.0f / (float)n_valid;
                }
                sum = 1.0f;
            } else {
                for (uint32_t i = 0; i < n_valid; i++) probs[i] /= sum;
            }

            /* Per-step entropy confidence (1 − H(p)/log K). */
            float entropy_step = 0.0f;
            if (n_valid > 1) {
                float H = 0.0f;
                for (uint32_t i = 0; i < n_valid; i++) {
                    float p = probs[i];
                    if (p > 1e-12f) H -= p * logf(p);
                }
                float Hmax = logf((float)n_valid);
                entropy_step = (Hmax > 0.0f) ? (1.0f - H / Hmax) : 1.0f;
                if (entropy_step < 0.0f) entropy_step = 0.0f;
                if (entropy_step > 1.0f) entropy_step = 1.0f;
            }

            for (uint32_t i = 0; i < n_valid && n_cands < (BEAM_MAX_K * BEAM_MAX_TOPV); i++) {
                float p = probs[i];
                if (p <= 1e-30f) continue;
                cand_t* C = &cands[n_cands++];
                C->beam_idx     = bi;
                C->word_pop     = topV[valid_idx[i]].word_pop;
                C->word_form    = topV[valid_idx[i]].word_form;
                C->confidence   = topV[valid_idx[i]].confidence;
                C->log_prob     = logf(p);
                C->entropy_step = entropy_step;
                C->is_eos       = (eos_pop != UINT32_MAX &&
                                    topV[valid_idx[i]].word_pop == eos_pop);
            }
        }

        if (n_cands == 0) {
            /* No beam produced any candidate — all finished. */
            break;
        }

        /* Score each candidate: parent_beam.cum_logprob + cand.log_prob,
         * then length-normalize by (parent.n_used + 1)^0.6 so candidates
         * starting from a longer beam don't unfairly outscore short ones.
         * EOS candidates use the parent's current length (no new token
         * added). */
        float cand_scores[BEAM_MAX_K * BEAM_MAX_TOPV];
        for (uint32_t i = 0; i < n_cands; i++) {
            const beam_t* parent = &beams[cands[i].beam_idx];
            uint32_t newlen = parent->n_used + (cands[i].is_eos ? 0 : 1);
            if (newlen == 0) newlen = 1;
            float denom = powf((float)newlen, beam_alpha);
            if (denom < 1e-6f) denom = 1e-6f;
            cand_scores[i] = (parent->cum_logprob + cands[i].log_prob) / denom;
        }

        /* Rank-K selection by cand_scores (insertion sort on indices, K
         * small — n_cands ≤ 128). */
        uint32_t order[BEAM_MAX_K * BEAM_MAX_TOPV];
        for (uint32_t i = 0; i < n_cands; i++) order[i] = i;
        for (uint32_t i = 1; i < n_cands; i++) {
            uint32_t key = order[i];
            int32_t j = (int32_t)i - 1;
            while (j >= 0 && cand_scores[order[j]] < cand_scores[key]) {
                order[j + 1] = order[j];
                j--;
            }
            order[j + 1] = key;
        }

        uint32_t keep = (n_cands < beam_width) ? n_cands : beam_width;

        /* Build the next-step beam roster. We need ALL previously-finished
         * beams to be carried forward unchanged + up to keep new beams from
         * the candidate ranking. Move existing beams into a temp buffer
         * first so we don't clobber sources during clone. */
        beam_t old_beams[BEAM_MAX_K];
        memcpy(old_beams, beams, sizeof(beams));
        memset(beams, 0, sizeof(beams));
        uint32_t new_n_beams = 0;

        /* 1) Carry already-finished beams forward (they keep their score). */
        for (uint32_t i = 0; i < n_beams && new_n_beams < BEAM_MAX_K; i++) {
            if (old_beams[i].active && old_beams[i].finished) {
                /* Move ownership rather than clone — avoids extra alloc. */
                beams[new_n_beams++] = old_beams[i];
                memset(&old_beams[i], 0, sizeof(old_beams[i]));
            }
        }

        /* 2) Spawn new beams from the top-K candidates. */
        for (uint32_t r = 0; r < keep && new_n_beams < BEAM_MAX_K; r++) {
            const cand_t* C = &cands[order[r]];
            const beam_t* parent = &old_beams[C->beam_idx];
            beam_t* dst = &beams[new_n_beams];
            if (beam_clone(dst, parent, n_concepts) != 0) {
                continue;
            }

            /* Update accumulated stats for this new beam. */
            dst->cum_logprob      += C->log_prob;
            dst->total_confidence += C->confidence;
            if (C->entropy_step > 0.0f || C->entropy_step == 0.0f) {
                /* Always include the entropy step (even 0 contributes a
                 * sample, matching the greedy path's behavior of recording
                 * a posterior every step where we computed one). */
                dst->entropy_conf_sum += C->entropy_step;
                dst->entropy_steps    += 1;
            }

            if (C->is_eos) {
                /* TB-7 parity: respect min_produce_words floor — when the
                 * beam hasn't emitted enough real words yet, don't accept
                 * EOS. Mark this candidate slot rejected; the beam_clone
                 * we just made is freed below to avoid a leak. Caller
                 * sees a length_min_suppressions++ to mirror greedy
                 * telemetry. */
                if (min_words_cfg > 0 && dst->n_used < min_words_cfg) {
                    bridge->stats.length_min_suppressions++;
                    beam_free(dst);
                    continue;
                }
                /* EOS halts this beam cleanly: do NOT append the EOS form
                 * to text_buf, do NOT add to used_words / n_used. */
                dst->finished = true;
            } else {
                /* Append word to text. */
                size_t wlen = strlen(C->word_form);
                if (dst->text_pos + wlen + 2 < sizeof(dst->text_buf)) {
                    if (dst->text_pos > 0) dst->text_buf[dst->text_pos++] = ' ';
                    memcpy(dst->text_buf + dst->text_pos, C->word_form, wlen);
                    dst->text_pos += wlen;
                } else {
                    /* Walkthrough-3 fix — text_buf is sizeof(dst->text_buf)
                     * bytes (currently 2048). Pre-fix, this branch silently
                     * dropped the word from the text but still incremented
                     * n_used + emitted it through the beam. Result: caller
                     * saw word_count=N but the returned text was a
                     * truncated <N-word string. Mark the beam finished so
                     * the next pick at most expands a different beam. */
                    dst->finished = true;
                }
                if (dst->n_used < BEAM_MAX_WORDS) {
                    dst->used_words[dst->n_used++] = C->word_pop;
                }

                /* Confidence floor — match greedy semantics: stop if
                 * confidence < 0.01 *and* there was at least one prior
                 * emitted word.
                 *
                 * Greedy (line 2071): `confidence < 0.01 && word_count > 0`
                 * is checked BEFORE word_count++ (line 2086). So at greedy's
                 * check time, word_count = number of PRIOR words. word_count
                 * > 0 means >= 1 prior word.
                 *
                 * Beam: dst->n_used has ALREADY been incremented to include
                 * the current word (line 2627). To match greedy's "at least
                 * one prior word", we need n_used > 1 (i.e., >= 2 total =
                 * >= 1 prior + this one).
                 *
                 * Walkthrough-3 audit found this used to be `> 0` after a
                 * previous "fix" — that incorrectly stopped beam on the
                 * FIRST low-confidence word while greedy accepted it. */
                if (C->confidence < 0.01f && dst->n_used > 1) {
                    dst->finished = true;
                }

                /* Recurrent state update: state = (1 - wf)*state + wf*encode(word).
                 * Then concept_acts = ip*intent + (1-ip)*state. */
                float* word_concepts = nimcp_calloc(n_concepts, sizeof(float));
                if (word_concepts) {
                    snn_language_bridge_encode_word(bridge, C->word_pop,
                                                    word_concepts, n_concepts);
                    float keep_w = 1.0f - wf;
                    for (uint32_t c = 0; c < n_concepts; c++) {
                        dst->state[c] = keep_w * dst->state[c]
                                          + wf * word_concepts[c];
                    }
                    nimcp_free(word_concepts);
                }
                for (uint32_t c = 0; c < n_concepts; c++) {
                    dst->concept_acts[c] = ip * intent[c]
                                             + (1.0f - ip) * dst->state[c];
                }

                /* EOS stopping criterion parity with greedy: when the
                 * caller has opted in AND we're past min_produce_words,
                 * stop this beam if the just-rebuilt activation magnitude
                 * dropped under threshold OR the picked word's confidence
                 * is under eos_min_confidence. Bridges audit-G item 2. */
                if (eos_stop_enabled && dst->n_used >= min_words_cfg) {
                    float act_sq = 0.0f;
                    for (uint32_t c = 0; c < n_concepts; c++) {
                        act_sq += dst->concept_acts[c] * dst->concept_acts[c];
                    }
                    float act_mag = sqrtf(act_sq);
                    bool act_undershot  = (eos_min_act  >= 0.0f) && (act_mag < eos_min_act);
                    bool conf_undershot = (eos_min_conf >= 0.0f) &&
                                          (C->confidence < eos_min_conf);
                    if (act_undershot || conf_undershot) {
                        dst->finished = true;
                    }
                }

                /* TB-7 parity: hard max-words cap. */
                if (max_cfg > 0 && dst->n_used >= max_cfg) {
                    dst->finished = true;
                }
            }

            new_n_beams++;
        }

        /* 3) Free any old_beams we didn't move forward. */
        for (uint32_t i = 0; i < n_beams; i++) {
            if (old_beams[i].active) beam_free(&old_beams[i]);
        }

        n_beams = new_n_beams;

        /* If every surviving beam is finished, stop. */
        bool any_unfinished = false;
        for (uint32_t i = 0; i < n_beams; i++) {
            if (beams[i].active && !beams[i].finished) {
                any_unfinished = true; break;
            }
        }
        if (!any_unfinished) break;
    }

    /* Pick the best beam by length-normalized cum_logprob, optionally
     * scaled by 1/(1+w*|energy_deviation|) when HNN re-rank is enabled.
     * The HNN net is type-erased on the bridge struct; we cast at the
     * single call site. Re-rank only applies when both the flag is on
     * AND a net is attached AND |dev| > 0 — otherwise we degrade to
     * plain length-norm scoring.
     *
     * Audit fix — snapshot bridge->hnn ONCE at entry. A concurrent
     * snn_language_bridge_set_hnn(bridge, NULL) call from another
     * thread could nullify the pointer between the check and the
     * dereference; the cached local copy is immune to that race. */
    const lnn_hamiltonian_net_t* hnn_cached =
        (const lnn_hamiltonian_net_t*)bridge->hnn;
    bool hnn_rerank_active = bridge->config.enable_beam_hnn_rerank &&
                              hnn_cached != NULL;
    float energy_dev = 0.0f;
    if (hnn_rerank_active) {
        energy_dev = lnn_hamiltonian_get_energy_deviation(hnn_cached);
        if (!isfinite(energy_dev)) energy_dev = 0.0f;
        if (energy_dev < 0.0f) energy_dev = -energy_dev;
    }
    int best = -1;
    float best_score = -FLT_MAX;
    for (uint32_t i = 0; i < n_beams; i++) {
        if (!beams[i].active || beams[i].n_used == 0) continue;
        float s = beam_score_alpha(&beams[i], beam_alpha);
        if (hnn_rerank_active && energy_dev > 0.0f) {
            float w = bridge->config.beam_hnn_weight;
            if (!isfinite(w) || w < 0.0f) w = 0.0f;
            float scale = 1.0f / (1.0f + w * energy_dev);
            /* s is typically negative (log-prob / length-norm); the
             * scaling penalizes high-deviation beams by SHRINKING the
             * magnitude, i.e. pulling negative scores toward zero. To
             * make penalty consistent (worse → lower), invert when s<0
             * by dividing instead of multiplying. */
            s = (s >= 0.0f) ? (s * scale) : (s / scale);
        }
        if (s > best_score) {
            best_score = s;
            best = (int)i;
        }
    }
    if (hnn_rerank_active && energy_dev > 0.0f) {
        bridge->stats.beam_hnn_rerank_passes++;
    }

    int rc_out = 0;
    if (best < 0) {
        rc_out = -1;
    } else {
        beam_t* B = &beams[best];
        /* BEAM_MAX_WORDS truncation telemetry — parity with greedy's
         * length_max_truncations bump. Treat the picked beam as cap-
         * truncated if it ran into the BEAM_MAX_WORDS internal cap
         * (32) without becoming finished, OR if the caller-set max_cfg
         * was reached. */
        if (B->n_used >= BEAM_MAX_WORDS ||
            (max_cfg > 0 && B->n_used >= max_cfg)) {
            bridge->stats.length_max_truncations++;
            bridge->stats.total_max_truncations++;
        }
        B->text_buf[B->text_pos] = '\0';
        result->text = nimcp_malloc(B->text_pos + 1);
        if (!result->text) {
            rc_out = -1;
        } else {
            memcpy(result->text, B->text_buf, B->text_pos + 1);
            result->word_count = B->n_used;
            result->spike_confidence = (B->n_used > 0)
                                          ? B->total_confidence / (float)B->n_used
                                          : 0.0f;
            result->fluency = fminf(1.0f, (float)B->n_used / 8.0f);
            result->creativity = 0.0f;
            result->entropy_confidence = (B->entropy_steps > 0)
                                           ? B->entropy_conf_sum / (float)B->entropy_steps
                                           : 0.0f;

            /* Update running EMA on avg_word_confidence (matches greedy path). */
            float alpha = (bridge->stats.total_produce_calls > 0)
                ? 1.0f / (float)bridge->stats.total_produce_calls : 1.0f;
            if (alpha < 0.05f) alpha = 0.05f;
            bridge->stats.avg_word_confidence =
                (1.0f - alpha) * bridge->stats.avg_word_confidence
                + alpha * result->spike_confidence;
        }
    }

    /* Clean up. */
    for (uint32_t i = 0; i < n_beams; i++) {
        if (beams[i].active) beam_free(&beams[i]);
    }
    nimcp_free(intent);

    bridge->emb_query_override     = NULL;
    bridge->emb_query_override_dim = 0;

    return rc_out;
}

/* TA-2 — LGSS evaluator forward decl + small safety types header.
 *
 * Same rationale as the grounded_language side: the LGSS umbrella
 * cascades into cognitive/symbolic_logic enum collisions, so the SNN
 * bridge translation unit only pulls the lightweight safety_types
 * header (POD context + enums) and forward-declares the evaluator. */
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety_types.h"
#include "security/nimcp_audit_log.h"
typedef struct lgss_context lgss_context_t;
extern int lgss_evaluate(
    lgss_context_t* lgss,
    const safety_action_context_t* context,
    safety_evaluation_t* result);

/* Tier-4 #16: public wrapper — timed entry/exit. NULL guards run before
 * clock_gettime so timing only counts work that actually happened. */
int snn_language_bridge_produce(snn_language_bridge_t* bridge,
                                 const float* semantic_intent,
                                 uint32_t intent_dim,
                                 snn_lang_production_result_t* result)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC ||
        !semantic_intent || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_produce: bridge, semantic_intent, or result is NULL");
        return -1;
    }

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    int rc = bridge_produce_impl(bridge, semantic_intent, intent_dim, result);

    /* TA-2 — LGSS OUTPUT GATE.
     *
     * After produce_impl has constructed result->text but before the
     * caller sees it, evaluate the produced text against the safety
     * KB. SAFETY_ACTION_DENY blocks emission: free + zero result->text,
     * reset word_count to 0, bump stats.lgss_outputs_blocked, and emit
     * an LGSS_ACTION_BLOCKED audit event.
     *
     * Placement: only when produce_impl reported success (rc == 0) AND
     * actually emitted text — a NULL result->text path skips the gate
     * (nothing to block). When no LGSS is attached, the gate is a
     * no-op and lgss_outputs_blocked stays 0. */
    if (rc == 0 && bridge->lgss && result->text && result->text[0] != '\0') {
        safety_action_context_t lgss_ctx;
        memset(&lgss_ctx, 0, sizeof(lgss_ctx));

        strncpy(lgss_ctx.string_fields[0].key, "operation", 63);
        lgss_ctx.string_fields[0].key[63] = '\0';
        strncpy(lgss_ctx.string_fields[0].value, "language_produce",
                SAFETY_MAX_VALUE_LEN - 1);
        lgss_ctx.string_fields[0].value[SAFETY_MAX_VALUE_LEN - 1] = '\0';

        strncpy(lgss_ctx.string_fields[1].key, "text", 63);
        lgss_ctx.string_fields[1].key[63] = '\0';
        strncpy(lgss_ctx.string_fields[1].value, result->text,
                SAFETY_MAX_VALUE_LEN - 1);
        lgss_ctx.string_fields[1].value[SAFETY_MAX_VALUE_LEN - 1] = '\0';
        lgss_ctx.num_string_fields = 2;

        strncpy(lgss_ctx.numeric_fields[0].key, "word_count", 63);
        lgss_ctx.numeric_fields[0].key[63] = '\0';
        lgss_ctx.numeric_fields[0].value = (float)result->word_count;
        lgss_ctx.num_numeric_fields = 1;

        lgss_ctx.domain_hint = SAFETY_DOMAIN_GOVERNANCE;
        lgss_ctx.has_domain_hint = true;
        snprintf(lgss_ctx.action_description,
                 sizeof(lgss_ctx.action_description),
                 "language_produce output: %u words", result->word_count);
        lgss_ctx.action_description[sizeof(lgss_ctx.action_description) - 1]
            = '\0';
        strncpy(lgss_ctx.source, "LANGUAGE_PRODUCE", 63);
        lgss_ctx.source[63] = '\0';

        safety_evaluation_t lgss_eval;
        memset(&lgss_eval, 0, sizeof(lgss_eval));
        int lgss_rc = lgss_evaluate((lgss_context_t*)bridge->lgss,
                                     &lgss_ctx, &lgss_eval);
        if (lgss_rc == 0 && lgss_eval.action == SAFETY_ACTION_DENY) {
            bridge->stats.lgss_outputs_blocked++;
            nimcp_safety_audit_log_event(
                NIMCP_SAFETY_AUDIT_LGSS_ACTION_BLOCKED, 2,
                "LGSS blocked language_produce output "
                "(words=%u, severity=%d): %s",
                result->word_count, (int)lgss_eval.max_severity,
                lgss_eval.explanation);
            /* Free + zero produced text so the caller never sees the
             * blocked content. Keep the result struct itself valid. */
            nimcp_free(result->text);
            result->text = NULL;
            result->word_count = 0;
            rc = -1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    /* Saturating-conservative: if the wall clock somehow steps backward
     * (CLOCK_MONOTONIC shouldn't but defensively guard) skip the bump
     * rather than wrap into a huge unsigned. */
    int64_t elapsed_us =
        (int64_t)(t_end.tv_sec  - t_start.tv_sec ) * 1000000LL +
        (int64_t)(t_end.tv_nsec - t_start.tv_nsec) / 1000LL;
    if (elapsed_us < 0) elapsed_us = 0;
    bridge->stats.produce_total_us  += (uint64_t)elapsed_us;
    bridge->stats.produce_call_count++;

    return rc;
}

/* Tier-4 #16: derived getter for ops dashboards. */
float snn_language_bridge_get_avg_produce_us(const snn_language_bridge_t* bridge)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return 0.0f;
    if (bridge->stats.produce_call_count == 0) return 0.0f;
    return (float)bridge->stats.produce_total_us /
           (float)bridge->stats.produce_call_count;
}

/* TA-4: trigram-learning runtime flag accessors. */
int snn_language_bridge_set_trigram_learning_enabled(
    snn_language_bridge_t* bridge,
    bool enabled)
{
    /* Option-1 (Slice A): no-op stub. Trigram learning is no longer a
     * bridge concern; if grounded_language wants to learn trigrams, it
     * does so against its own lexicon, not against bridge weights. */
    (void)enabled;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    return 0;
}

bool snn_language_bridge_get_trigram_learning_enabled(
    const snn_language_bridge_t* bridge)
{
    (void)bridge;
    return false;
}

int snn_language_bridge_set_ltd_margin(
    snn_language_bridge_t* bridge,
    float margin)
{
    /* Option-1 (Slice A): no-op stub. */
    (void)margin;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    return 0;
}

float snn_language_bridge_get_ltd_margin(
    const snn_language_bridge_t* bridge)
{
    (void)bridge;
    return 0.0f;
}

/*===========================================================================
 * Slice 4 — Lateral-inhibition runtime setters / getters.
 *
 * Toggle + hyperparameter tuning surface for the recurrent-competition
 * decode path. All runtime-only (NOT persisted in the V5 sidecar) —
 * caller re-applies after each load. Matches the contract used by
 * trigram / DA-modulation / comprehend-STDP toggles upstream.
 *===========================================================================*/

int snn_language_bridge_set_lateral_inhibition_enabled(
    snn_language_bridge_t* bridge,
    bool enabled)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    bridge->config.enable_lateral_inhibition = enabled;
    return 0;
}

bool snn_language_bridge_get_lateral_inhibition_enabled(
    const snn_language_bridge_t* bridge)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return false;
    return bridge->config.enable_lateral_inhibition;
}

int snn_language_bridge_set_lateral_inhibition_params(
    snn_language_bridge_t* bridge,
    float gain_self,
    float gain_inhibit,
    uint32_t micro_steps)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    /* Validate before mutating: any bad input rejects the whole call. */
    if (!isfinite(gain_self)    || gain_self    <= 0.0f || gain_self    > 100.0f)
        return -1;
    if (!isfinite(gain_inhibit) || gain_inhibit <= 0.0f || gain_inhibit > 100.0f)
        return -1;
    if (micro_steps == 0 || micro_steps > LATERAL_INHIBITION_MAX_STEPS)
        return -1;
    bridge->config.lateral_gain_self    = gain_self;
    bridge->config.lateral_gain_inhibit = gain_inhibit;
    bridge->config.lateral_micro_steps  = micro_steps;
    return 0;
}

int snn_language_bridge_get_lateral_inhibition_params(
    const snn_language_bridge_t* bridge,
    float* out_gain_self,
    float* out_gain_inhibit,
    uint32_t* out_micro_steps)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (out_gain_self)    *out_gain_self    = bridge->config.lateral_gain_self;
    if (out_gain_inhibit) *out_gain_inhibit = bridge->config.lateral_gain_inhibit;
    if (out_micro_steps)  *out_micro_steps  = bridge->config.lateral_micro_steps;
    return 0;
}

int64_t snn_language_bridge_reset_weights(snn_language_bridge_t* bridge,
                                           float w_min,
                                           float w_max)
{
    /* Option-1 (Slice A): no-op stub. Bridge has no weights to reset. */
    (void)w_min; (void)w_max;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    return 0;
}

/* TB-8: streaming-produce callback attach/detach. cb=NULL detaches.
 * Both cb and user_data are borrowed pointers — bridge never frees them
 * and never persists them across save/load. Stays consistent with the
 * other runtime-only setters (LGSS, trigram, DA modulation). */
int snn_language_bridge_set_stream_callback(
    snn_language_bridge_t* bridge,
    snn_lang_stream_callback_t cb,
    void* user_data)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    bridge->stream_cb        = cb;
    bridge->stream_user_data = user_data;
    return 0;
}

void snn_language_bridge_inc_trigram_updates(snn_language_bridge_t* bridge)
{
    /* Option-1 (Slice A): no-op stub. */
    (void)bridge;
}

int snn_language_bridge_set_da_modulation_enabled(
    snn_language_bridge_t* bridge,
    bool enabled)
{
    /* Option-1 (Slice A): no-op stub. Bridge has no weights to modulate. */
    (void)enabled;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    return 0;
}

bool snn_language_bridge_get_da_modulation_enabled(
    const snn_language_bridge_t* bridge)
{
    (void)bridge;
    return false;
}

int snn_language_bridge_set_da_modulation_gain(
    snn_language_bridge_t* bridge,
    float gain)
{
    /* Option-1 (Slice A): no-op stub. DA modulation as a concept persists
     * but moves to SNN neuromod (Slice F). The bridge-side setter is now
     * inert on the weight matrix (there are no weights) but we still
     * validate the gain shape — NaN is always a caller bug regardless of
     * whether the bridge consumes the value. */
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    if (isnan(gain)) return -1;
    return 0;
}

void snn_lang_production_result_cleanup(snn_lang_production_result_t* result)
{
    if (!result) return;
    if (result->text) {
        nimcp_free(result->text);
        result->text = NULL;
    }
}

//=============================================================================
// Phase 4: Spike-Driven Comprehension (Wernicke pathway)
//=============================================================================

int snn_language_bridge_comprehend(snn_language_bridge_t* bridge,
                                    const char* text,
                                    float* concept_activations,
                                    uint32_t max_concepts,
                                    uint32_t* num_activated,
                                    float* comprehension_confidence)
{
    /* Option-1 (Slice A): the bridge no longer owns a word_pop→concept_pop
     * weight matrix, so it cannot map text to concept activations on its
     * own. Comprehend becomes a transport-only stub — zero activations,
     * zero confidence. The real text→concept mapping moves to the lexicon
     * (Slice B concept_registry); callers (cascade Wernicke stage) should
     * already fall through to grounded_language's own comprehend path
     * when this returns 0 activations.
     *
     * TODO(slice-B): rewrite on top of concept_registry — tokenize, look
     * up each word_id, fetch concept_pop_id from the registry, write a
     * Kronecker activation at that pop index.
     */
    if (!bridge || bridge->magic != SNN_LANG_MAGIC ||
        !text || !concept_activations || !num_activated) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_comprehend: bridge, text, concept_activations, or num_activated is NULL");
        return -1;
    }

    bridge->stats.total_comprehend_calls++;
    memset(concept_activations, 0, max_concepts * sizeof(float));
    *num_activated = 0;
    if (comprehension_confidence) *comprehension_confidence = 0.0f;
    return 0;
}

int snn_language_bridge_set_comprehend_stdp_enabled(
    snn_language_bridge_t* bridge, bool enabled)
{
    /* Option-1 (Slice A): no-op stub. Comprehend is read-only transport now. */
    (void)enabled;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    return 0;
}

bool snn_language_bridge_get_comprehend_stdp_enabled(
    const snn_language_bridge_t* bridge)
{
    (void)bridge;
    return false;
}

int snn_language_bridge_echo_correct(
    snn_language_bridge_t* bridge,
    const float* intent,
    uint32_t intent_dim,
    const char* target_word_form,
    float lr_scale)
{
    /* Option-1 (Slice A): no-op stub. Echo-correct supervised production
     * learning is gone from the bridge — bridge owns no concept→word
     * weights to strengthen. The supervised production-side learning
     * loop is being moved into the SNN's projection synapses + the
     * lexicon's concept_registry. Until Slice B wires that, callers
     * (notably cascade_apply_self_train_reward) just get a 0-pairs
     * return — equivalent to running the supervised step on an
     * unfamiliar word, which they already handle as a no-op. */
    (void)intent; (void)intent_dim; (void)target_word_form; (void)lr_scale;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    return 0;
}

//=============================================================================
// Phase 5: Creative/Imagination Integration
//=============================================================================

int snn_language_bridge_creative_produce(snn_language_bridge_t* bridge,
                                          const float* imagination_activations,
                                          uint32_t num_dims,
                                          float creativity_level,
                                          snn_lang_production_result_t* result)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC ||
        !imagination_activations || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_creative_produce: bridge, imagination_activations, or result is NULL");
        return -1;
    }

    bridge->stats.imagination_contributions++;

    // Map imagination activations to concept space
    // Imagination dims are creativity/vividness/coherence signals
    // Scale them by creativity_level to modulate word selection
    uint32_t n_concepts = bridge->num_concept_pops;
    float* concept_acts = nimcp_calloc(n_concepts, sizeof(float));
    if (!concept_acts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "snn_language_bridge_creative_produce: failed to allocate concept_acts");
        return -1;
    }

    // Distribute imagination activations across concept populations
    // with creativity-scaled noise for exploration
    uint32_t copy_dim = (num_dims < n_concepts) ? num_dims : n_concepts;
    for (uint32_t i = 0; i < copy_dim; i++) {
        concept_acts[i] = imagination_activations[i] * (1.0f + creativity_level);
    }

    // Add stochastic exploration proportional to creativity
    if (creativity_level > 0.1f) {
        for (uint32_t i = 0; i < n_concepts; i++) {
            // Simple hash-based pseudo-random noise
            uint32_t h = (i * 2654435761u + bridge->stats.imagination_contributions * 40503u);
            float noise = ((float)(h & 0xFFFF) / 65535.0f - 0.5f) * 2.0f;
            concept_acts[i] += noise * creativity_level * 0.1f;
            if (concept_acts[i] < 0.0f) concept_acts[i] = 0.0f;
        }
    }

    // Produce via standard spike cascade
    int rc = snn_language_bridge_produce(bridge, concept_acts, n_concepts, result);
    if (rc == 0) {
        result->creativity = creativity_level;
    }

    nimcp_free(concept_acts);
    return rc;
}

int snn_language_bridge_curiosity_modulate(snn_language_bridge_t* bridge,
                                            float novelty_level,
                                            float exploration_drive)
{
    /* Option-1 (Slice A): no-op stub. Novelty / exploration as concepts
     * persist but their effect on learning rates moves to the SNN's own
     * STDP knobs, not the bridge's (now-deleted) STDP. */
    (void)novelty_level; (void)exploration_drive;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    bridge->stats.curiosity_contributions++;
    return 0;
}

//=============================================================================
// Phase 6: Sleep Consolidation
//=============================================================================

int snn_language_bridge_sleep_consolidate(snn_language_bridge_t* bridge,
                                           float consolidation_strength)
{
    /* Option-1 (Slice A): no-op stub. Bridge has no weights to consolidate.
     * If/when sleep consolidation lives on top of concept_registry +
     * SNN projection synapses, Slice B will wire it through there. */
    (void)consolidation_strength;
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    bridge->stats.sleep_consolidation_cycles++;
    return 0;
}

//=============================================================================
// Statistics & Introspection
//=============================================================================

int snn_language_bridge_get_stats(const snn_language_bridge_t* bridge,
                                   snn_lang_stats_t* stats)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_get_stats: bridge or stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    /* Option-1 (Slice A): bridge owns no bindings now. active_bindings and
     * avg_binding_weight stay at the legacy 0 they get from the memcpy +
     * default-init. spike_blend_current is still a config knob, so report it. */
    stats->active_bindings = 0;
    stats->avg_binding_weight = 0.0f;
    stats->spike_blend_current = bridge->config.spike_blend;

    return 0;
}

int snn_language_bridge_reset_stats(snn_language_bridge_t* bridge)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return -1;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

/* Tier-4 #15: copy entire bridge config out for introspection. */
int snn_language_bridge_get_config(const snn_language_bridge_t* bridge,
                                    snn_lang_config_t* out)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC || !out) return -1;
    *out = bridge->config;
    return 0;
}

float snn_language_bridge_get_blend(const snn_language_bridge_t* bridge)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return 0.0f;
    return bridge->config.spike_blend;
}

const char* snn_language_bridge_get_word_form(
    const snn_language_bridge_t* bridge,
    uint32_t word_pop_index)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return NULL;
    if (word_pop_index >= bridge->num_word_pops) return NULL;
    if (!bridge->word_pops[word_pop_index].registered) return NULL;
    return bridge->word_pops[word_pop_index].word_form;
}

void snn_language_bridge_set_blend(snn_language_bridge_t* bridge, float blend)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC) return;
    bridge->config.spike_blend = fmaxf(0.0f, fminf(1.0f, blend));
}

//=============================================================================
// Serialization
//=============================================================================

/* Tier 2 #8 + EOS migration: extended config block written immediately after
 * the snn_lang_config_t blob in V3/V4 files. Holds the PA/MQ + EOS runtime-
 * tunable knobs in a fixed, deterministic, packed layout. The block is
 * preceded by a u32 size field so readers can both
 *   (a) seek past trailing unknown bytes (forward-compat from V3→V4+), and
 *   (b) leave fields at library defaults when the on-disk block is shorter
 *       than this build expects (backward-compat from V4 reader to V3 file).
 *
 * Field order is APPEND-ONLY — every byte written by an older writer keeps
 * the same meaning. New fields go at the tail.
 *
 * NOTE: this is intentionally redundant with the same fields in
 * snn_lang_config_t — the explicit block is what we treat as authoritative
 * on load. The legacy struct blob still carries them for backward
 * compatibility with consumers that memcpy the whole struct, but the EOS
 * stopping knobs are NEVER read back from the raw blob — only from this
 * ext block. That is the belt+suspenders defense against the previous
 * "struct grew, stale-TU stack-smash" failure mode. */

/* Wire-format sizes (bytes), so callers don't multiply sizeof() at three
 * different sites and accidentally drift. */
#define EXT_KNOWN_SIZE_V3 \
    ( sizeof(float)    /* temperature */                \
    + sizeof(float)    /* top_p */                      \
    + sizeof(uint32_t) /* produce_topk */               \
    + sizeof(float)    /* glove_blend */                \
    + sizeof(float)    /* intent_persistence */         \
    + sizeof(float)    /* word_feedback */              \
    + sizeof(uint8_t)  /* enable_snn_spike_routing */   \
    + sizeof(float)    /* activation_tau_ms */          \
    + sizeof(uint8_t)  /* use_hyperbolic_embeddings */  \
    + sizeof(int32_t)  /* sampling_mode */              \
    )

#define EXT_KNOWN_SIZE_V4 \
    ( EXT_KNOWN_SIZE_V3                                 \
    + sizeof(uint8_t)  /* enable_eos_stopping */        \
    + sizeof(float)    /* eos_min_activation */         \
    + sizeof(float)    /* eos_min_confidence */         \
    )

#define EXT_KNOWN_SIZE_V5 \
    ( EXT_KNOWN_SIZE_V4                                 \
    + sizeof(uint8_t)  /* enable_beam_hnn_rerank */     \
    + sizeof(float)    /* beam_hnn_weight */            \
    + sizeof(float)    /* beam_length_norm_alpha */     \
    )

/* Walkthrough-4 — defense-in-depth: invariant that newer versions must
 * strictly extend older ones. If a future commit accidentally shrinks
 * the V5 size (e.g., by removing a field instead of appending) the
 * V3/V4 reader's forward-skip would underflow. _Static_assert catches
 * it at compile time. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(EXT_KNOWN_SIZE_V5 > EXT_KNOWN_SIZE_V4,
    "ext block versions must strictly extend — V5 size must exceed V4");
_Static_assert(EXT_KNOWN_SIZE_V4 > EXT_KNOWN_SIZE_V3,
    "ext block versions must strictly extend — V4 size must exceed V3");
#endif

/* Pack/unpack helpers (write fields one-at-a-time to avoid struct padding
 * surprises across compilers). Always writes the V5 layout. V3/V4 readers
 * forward-compat skip the trailing bytes via the block_size header. */
static int write_ext_config_v5(FILE* f, const snn_lang_config_t* cfg)
{
    const uint32_t ext_block_size = (uint32_t)EXT_KNOWN_SIZE_V5;

    uint8_t spike_routing  = cfg->enable_snn_spike_routing ? 1 : 0;
    uint8_t hyperbolic     = cfg->use_hyperbolic_embeddings ? 1 : 0;
    int32_t sampling_mode  = cfg->sampling_mode;
    uint8_t eos_enabled    = cfg->enable_eos_stopping ? 1 : 0;
    uint8_t beam_rerank_en = cfg->enable_beam_hnn_rerank ? 1 : 0;

    if (fwrite(&ext_block_size,           sizeof(uint32_t), 1, f) != 1) return -1;
    if (fwrite(&cfg->temperature,         sizeof(float),    1, f) != 1) return -1;
    if (fwrite(&cfg->top_p,               sizeof(float),    1, f) != 1) return -1;
    if (fwrite(&cfg->produce_topk,        sizeof(uint32_t), 1, f) != 1) return -1;
    if (fwrite(&cfg->glove_blend,         sizeof(float),    1, f) != 1) return -1;
    if (fwrite(&cfg->intent_persistence,  sizeof(float),    1, f) != 1) return -1;
    if (fwrite(&cfg->word_feedback,       sizeof(float),    1, f) != 1) return -1;
    if (fwrite(&spike_routing,            sizeof(uint8_t),  1, f) != 1) return -1;
    if (fwrite(&cfg->activation_tau_ms,   sizeof(float),    1, f) != 1) return -1;
    if (fwrite(&hyperbolic,               sizeof(uint8_t),  1, f) != 1) return -1;
    if (fwrite(&sampling_mode,            sizeof(int32_t),  1, f) != 1) return -1;
    /* V4 tail — EOS stopping knobs. */
    if (fwrite(&eos_enabled,              sizeof(uint8_t),  1, f) != 1) return -1;
    if (fwrite(&cfg->eos_min_activation,  sizeof(float),    1, f) != 1) return -1;
    if (fwrite(&cfg->eos_min_confidence,  sizeof(float),    1, f) != 1) return -1;
    /* V5 tail — beam re-rank knobs. */
    if (fwrite(&beam_rerank_en,           sizeof(uint8_t),  1, f) != 1) return -1;
    if (fwrite(&cfg->beam_hnn_weight,        sizeof(float), 1, f) != 1) return -1;
    if (fwrite(&cfg->beam_length_norm_alpha, sizeof(float), 1, f) != 1) return -1;
    return 0;
}

/* Read the V3/V4 ext block. Reads `block_size` bytes total — known fields
 * are decoded based on the size hint, and any trailing bytes (future-
 * extension fields) are skipped via fseek. Caller is expected to have
 * pre-populated cfg_out with library defaults (or with the struct-blob
 * contents) before calling — fields outside the on-disk block keep
 * whatever the caller put there. */
static int read_ext_config_v3_or_v4_or_v5(FILE* f, uint32_t block_size,
                                    snn_lang_config_t* cfg_out)
{
    /* Hard upper bound: refuse pathologically large blocks (corruption guard). */
    if (block_size > 64u * 1024u) return -1;

    long start_pos = ftell(f);
    if (start_pos < 0) return -1;

    if (block_size < (uint32_t)EXT_KNOWN_SIZE_V3) return -1; /* truncated */

    float    temperature, top_p, glove_blend, intent_persistence;
    float    word_feedback, activation_tau_ms;
    uint32_t produce_topk;
    uint8_t  spike_routing, hyperbolic;
    int32_t  sampling_mode;

    if (fread(&temperature,        sizeof(float),    1, f) != 1) return -1;
    if (fread(&top_p,              sizeof(float),    1, f) != 1) return -1;
    if (fread(&produce_topk,       sizeof(uint32_t), 1, f) != 1) return -1;
    if (fread(&glove_blend,        sizeof(float),    1, f) != 1) return -1;
    if (fread(&intent_persistence, sizeof(float),    1, f) != 1) return -1;
    if (fread(&word_feedback,      sizeof(float),    1, f) != 1) return -1;
    if (fread(&spike_routing,      sizeof(uint8_t),  1, f) != 1) return -1;
    if (fread(&activation_tau_ms,  sizeof(float),    1, f) != 1) return -1;
    if (fread(&hyperbolic,         sizeof(uint8_t),  1, f) != 1) return -1;
    if (fread(&sampling_mode,      sizeof(int32_t),  1, f) != 1) return -1;

    cfg_out->temperature              = temperature;
    cfg_out->top_p                    = top_p;
    cfg_out->produce_topk             = produce_topk;
    cfg_out->glove_blend              = glove_blend;
    cfg_out->intent_persistence       = intent_persistence;
    cfg_out->word_feedback            = word_feedback;
    cfg_out->enable_snn_spike_routing = (spike_routing != 0);
    cfg_out->activation_tau_ms        = activation_tau_ms;
    cfg_out->use_hyperbolic_embeddings = (hyperbolic != 0);
    cfg_out->sampling_mode            = sampling_mode;

    /* V4 tail — present when the writer emitted EOS knobs. Older V3-format
     * writers stopped at EXT_KNOWN_SIZE_V3 — leave EOS at the library
     * defaults the caller pre-populated. */
    if (block_size >= (uint32_t)EXT_KNOWN_SIZE_V4) {
        uint8_t eos_enabled;
        float   eos_min_act, eos_min_conf;
        if (fread(&eos_enabled,  sizeof(uint8_t), 1, f) != 1) return -1;
        if (fread(&eos_min_act,  sizeof(float),   1, f) != 1) return -1;
        if (fread(&eos_min_conf, sizeof(float),   1, f) != 1) return -1;
        cfg_out->enable_eos_stopping = (eos_enabled != 0);
        cfg_out->eos_min_activation  = eos_min_act;
        cfg_out->eos_min_confidence  = eos_min_conf;
    }

    /* V5 tail — beam re-rank knobs. V3/V4 writers stop before this point. */
    if (block_size >= (uint32_t)EXT_KNOWN_SIZE_V5) {
        uint8_t beam_rerank;
        float   beam_w, beam_alpha;
        if (fread(&beam_rerank, sizeof(uint8_t), 1, f) != 1) return -1;
        if (fread(&beam_w,      sizeof(float),   1, f) != 1) return -1;
        if (fread(&beam_alpha,  sizeof(float),   1, f) != 1) return -1;
        cfg_out->enable_beam_hnn_rerank = (beam_rerank != 0);
        cfg_out->beam_hnn_weight        = beam_w;
        cfg_out->beam_length_norm_alpha = beam_alpha;
    }

    /* Forward-compat: skip any trailing bytes belonging to a newer writer. */
    long want = start_pos + (long)block_size;
    long here = ftell(f);
    if (here < 0) return -1;
    if (here < want) {
        if (fseek(f, want, SEEK_SET) != 0) return -1;
    }
    return 0;
}

/* Reset the persisted PA/MQ knobs to their library defaults — used on V2
 * load so we don't trust whatever the legacy struct blob had in those
 * positions (older builds may have written zeros, garbage padding, or
 * different field offsets). */
static void reset_persisted_knobs_to_defaults(snn_lang_config_t* cfg)
{
    snn_lang_config_t defaults = snn_lang_config_default();
    cfg->temperature              = defaults.temperature;
    cfg->top_p                    = defaults.top_p;
    cfg->produce_topk             = defaults.produce_topk;
    cfg->glove_blend              = defaults.glove_blend;
    cfg->intent_persistence       = defaults.intent_persistence;
    cfg->word_feedback            = defaults.word_feedback;
    cfg->enable_snn_spike_routing = defaults.enable_snn_spike_routing;
    cfg->activation_tau_ms        = defaults.activation_tau_ms;
    cfg->use_hyperbolic_embeddings = defaults.use_hyperbolic_embeddings;
    cfg->sampling_mode            = defaults.sampling_mode;
    /* EOS stopping fields are new — older V2/V3 on-disk blobs do not
     * contain them in the ext block. Reset to library defaults so a
     * stale sidecar can't accidentally enable EOS stopping with garbage
     * thresholds. V4 readers overwrite these from the ext-block tail
     * after this reset runs. */
    cfg->enable_eos_stopping      = defaults.enable_eos_stopping;
    cfg->eos_min_activation       = defaults.eos_min_activation;
    cfg->eos_min_confidence       = defaults.eos_min_confidence;
    /* Beam re-rank fields — same rationale: V2/V3/V4 sidecars do not carry
     * them. Reset to library defaults; V5 readers overwrite from the
     * ext-block tail. */
    cfg->enable_beam_hnn_rerank   = defaults.enable_beam_hnn_rerank;
    cfg->beam_hnn_weight          = defaults.beam_hnn_weight;
    cfg->beam_length_norm_alpha   = defaults.beam_length_norm_alpha;
    /* ltd_margin is not in the V5 ext block — runtime-only knob. Reset
     * to library default on every load; trainer/RPC re-applies any
     * non-default value after the brain comes up. */
    cfg->ltd_margin               = defaults.ltd_margin;
}

/* V5 stats trailer — 30 cumulative counters written after the bindings
 * array. Self-describing via a leading block_size so older readers can
 * skip and newer readers can detect missing fields. Excludes gauges
 * (active_bindings, avg_*, spike_blend_current, last_da_modulation) —
 * those re-derive cleanly from runtime state. */
#define STATS_BLOCK_V5_COUNT 30u  /* number of u64 fields below */
#define STATS_BLOCK_V5_SIZE  (STATS_BLOCK_V5_COUNT * sizeof(uint64_t))

static int write_stats_block_v5(FILE* f, const snn_lang_stats_t* s)
{
    const uint32_t block_size = (uint32_t)STATS_BLOCK_V5_SIZE;
    if (fwrite(&block_size, sizeof(uint32_t), 1, f) != 1) return -1;
    /* MUST stay in append-only order matching read_stats_block_v5. */
    const uint64_t fields[STATS_BLOCK_V5_COUNT] = {
        s->total_decode_calls,
        s->total_encode_calls,
        s->total_produce_calls,
        s->total_comprehend_calls,
        s->total_stdp_updates,
        s->total_ltp_events,
        s->total_ltd_events,
        s->imagination_contributions,
        s->curiosity_contributions,
        s->sleep_consolidation_cycles,
        s->bindings_pruned,
        s->attach_collision_warnings,
        s->produce_total_us,
        s->produce_call_count,
        s->lgss_outputs_blocked,
        s->total_trigram_updates,
        s->da_gated_stdp_passes,
        s->length_min_suppressions,
        s->length_max_truncations,
        s->stream_callbacks_invoked,
        s->stream_aborts,
        s->decode_total_ns,
        s->comprehend_stdp_passes,
        s->comprehend_stdp_pairs_fired,
        s->echo_correct_calls,
        s->echo_correct_pairs,
        s->echo_correct_target_misses,
        s->total_eos_terminations,
        s->total_max_truncations,
        s->beam_hnn_rerank_passes
    };
    if (fwrite(fields, sizeof(uint64_t), STATS_BLOCK_V5_COUNT, f)
        != STATS_BLOCK_V5_COUNT) return -1;
    return 0;
}

/* Read a V5 stats trailer. If EOF (V3/V4 file), returns 0 with stats
 * left zero — non-fatal. block_size > expected: skip trailing bytes
 * (newer writer forward-compat). */
static int read_stats_block_v5(FILE* f, snn_lang_stats_t* s_out)
{
    uint32_t block_size = 0;
    size_t   got = fread(&block_size, sizeof(uint32_t), 1, f);
    if (got != 1) {
        /* EOF — V3/V4 file. Stats stay zero. */
        return 0;
    }
    if (block_size > 64u * 1024u) return -1;  /* corruption guard */

    long start_pos = ftell(f);
    if (start_pos < 0) return -1;

    /* Read up to STATS_BLOCK_V5_COUNT fields. Truncated blocks (older
     * partial writers) leave the missing tail at zero. */
    uint32_t to_read = (block_size < (uint32_t)STATS_BLOCK_V5_SIZE)
        ? (block_size / (uint32_t)sizeof(uint64_t))
        : STATS_BLOCK_V5_COUNT;

    uint64_t fields[STATS_BLOCK_V5_COUNT] = {0};
    if (to_read > 0) {
        if (fread(fields, sizeof(uint64_t), to_read, f) != to_read) return -1;
    }
    uint32_t i = 0;
    s_out->total_decode_calls         = fields[i++];
    s_out->total_encode_calls         = fields[i++];
    s_out->total_produce_calls        = fields[i++];
    s_out->total_comprehend_calls     = fields[i++];
    s_out->total_stdp_updates         = fields[i++];
    s_out->total_ltp_events           = fields[i++];
    s_out->total_ltd_events           = fields[i++];
    s_out->imagination_contributions  = fields[i++];
    s_out->curiosity_contributions    = fields[i++];
    s_out->sleep_consolidation_cycles = fields[i++];
    s_out->bindings_pruned            = fields[i++];
    s_out->attach_collision_warnings  = fields[i++];
    s_out->produce_total_us           = fields[i++];
    s_out->produce_call_count         = fields[i++];
    s_out->lgss_outputs_blocked       = fields[i++];
    s_out->total_trigram_updates      = fields[i++];
    s_out->da_gated_stdp_passes       = fields[i++];
    s_out->length_min_suppressions    = fields[i++];
    s_out->length_max_truncations     = fields[i++];
    s_out->stream_callbacks_invoked   = fields[i++];
    s_out->stream_aborts              = fields[i++];
    s_out->decode_total_ns            = fields[i++];
    s_out->comprehend_stdp_passes     = fields[i++];
    s_out->comprehend_stdp_pairs_fired = fields[i++];
    s_out->echo_correct_calls         = fields[i++];
    s_out->echo_correct_pairs         = fields[i++];
    s_out->echo_correct_target_misses = fields[i++];
    s_out->total_eos_terminations     = fields[i++];
    s_out->total_max_truncations      = fields[i++];
    s_out->beam_hnn_rerank_passes     = fields[i++];
    (void)i;

    /* B4 (walkthrough fix, 2026-05-27): the SNN-language bridge became
     * transport-only in the Option-1 rebuild — apply_stdp / trigram learning /
     * comprehend-STDP / echo-correct are all no-op stubs, so these plasticity
     * counters have NO incrementer anymore. A pre-rebuild checkpoint would
     * reload nonzero values that then sit frozen forever, making a dead bridge
     * look like it once learned (and lang_status.plasticity surfacing stale
     * numbers). Zero them on load so the telemetry reflects the live (no-op)
     * reality. The live counters above (decode/produce/comprehend calls,
     * timings, LGSS blocks, length controls, EOS) are preserved. */
    s_out->total_stdp_updates          = 0;
    s_out->total_ltp_events            = 0;
    s_out->total_ltd_events            = 0;
    s_out->total_trigram_updates       = 0;
    s_out->da_gated_stdp_passes        = 0;
    s_out->comprehend_stdp_passes      = 0;
    s_out->comprehend_stdp_pairs_fired = 0;
    s_out->echo_correct_calls          = 0;
    s_out->echo_correct_pairs          = 0;
    s_out->echo_correct_target_misses  = 0;

    /* Skip any trailing bytes belonging to a newer writer. */
    long want = start_pos + (long)block_size;
    long here = ftell(f);
    if (here < 0) return -1;
    if (here < want) {
        if (fseek(f, want, SEEK_SET) != 0) return -1;
    }
    return 0;
}

int snn_language_bridge_save(const snn_language_bridge_t* bridge, const char* path)
{
    if (!bridge || bridge->magic != SNN_LANG_MAGIC || !path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_save: bridge or path is NULL");
        return -1;
    }

    FILE* f = fopen(path, "wb");
    if (!f) return -1;

    /* V3+ header: magic, V3 sentinel (reused), version. The sentinel
     * disambiguates V3+ from V2 (where the next u32 after magic was
     * max_concept_pops, always ≤ SNN_LANG_MAX_CONCEPT_POPS = 4096, never
     * the 0xFFFFFFFE sentinel). V5 adds beam-rerank knobs to the ext
     * block AND a cumulative stats trailer past the bindings array;
     * V3/V4 readers forward-compat via ext_block_size + EOF tolerance. */
    const uint32_t v3_sentinel = SNN_LANG_BRIDGE_FILE_V3_SENTINEL;
    const uint32_t version     = SNN_LANG_BRIDGE_FILE_VERSION_V5;
    fwrite(&bridge->magic, sizeof(uint32_t), 1, f);
    fwrite(&v3_sentinel,   sizeof(uint32_t), 1, f);
    fwrite(&version,       sizeof(uint32_t), 1, f);

    /* Full snn_lang_config_t blob (preserves all existing struct fields
     * for consumers that memcpy the whole struct). The explicit ext block
     * below is what the loader treats as authoritative for the PA/MQ +
     * EOS + beam-rerank knobs — those are NEVER trusted from the raw blob,
     * so a stale reader+writer pair cannot stack-smash even if the struct
     * grows. */
    fwrite(&bridge->config, sizeof(snn_lang_config_t), 1, f);

    /* Tier 2 #8 + EOS + beam-rerank: extended config block in a fixed
     * wire layout. Always writes V5 (size = EXT_KNOWN_SIZE_V5);
     * V3/V4 readers seek past the trailing bytes. */
    if (write_ext_config_v5(f, &bridge->config) != 0) {
        fclose(f);
        return -1;
    }

    fwrite(&bridge->num_concept_pops, sizeof(uint32_t), 1, f);
    fwrite(&bridge->num_word_pops, sizeof(uint32_t), 1, f);

    // Concept populations
    fwrite(bridge->concept_pops, sizeof(concept_pop_info_t),
           bridge->num_concept_pops, f);

    // Word populations
    fwrite(bridge->word_pops, sizeof(word_pop_info_t),
           bridge->num_word_pops, f);

    // Bindings (count first, then each binding)
    fwrite(&bridge->num_bindings, sizeof(uint32_t), 1, f);
    for (uint32_t bucket = 0; bucket < BINDING_HASH_BUCKETS; bucket++) {
        binding_node_t* node = bridge->binding_buckets[bucket];
        while (node) {
            fwrite(&node->binding, sizeof(snn_lang_binding_t), 1, f);
            node = node->next;
        }
    }

    /* V5 stats trailer — round-trip 30 cumulative counters. Fixes the
     * gap where V4 and earlier wrote no stats, so every produce/decode/
     * stdp counter reset to zero on every load. */
    if (write_stats_block_v5(f, &bridge->stats) != 0) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

snn_language_bridge_t* snn_language_bridge_load(const char* path)
{
    if (!path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_load: path is NULL");
        return NULL;
    }

    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    uint32_t magic;
    if (fread(&magic, sizeof(uint32_t), 1, f) != 1 || magic != SNN_LANG_MAGIC) {
        fclose(f);
        return NULL;
    }

    /* Tier 2 #8: detect V3 vs V2 by sniffing the next u32. V3 has the
     * SNN_LANG_BRIDGE_FILE_V3_SENTINEL there; V2 has max_concept_pops
     * (always ≤ SNN_LANG_MAX_CONCEPT_POPS, never the sentinel). */
    uint32_t version_or_first_field;
    if (fread(&version_or_first_field, sizeof(uint32_t), 1, f) != 1) {
        fclose(f);
        return NULL;
    }

    bool is_v3 = (version_or_first_field == SNN_LANG_BRIDGE_FILE_V3_SENTINEL);

    snn_lang_config_t config;
    uint32_t file_version = SNN_LANG_BRIDGE_FILE_VERSION_V2;

    if (is_v3) {
        /* V3 or V4: read explicit version, full config blob, and ext block. */
        if (fread(&file_version, sizeof(uint32_t), 1, f) != 1) {
            fclose(f);
            return NULL;
        }
        if (file_version < SNN_LANG_BRIDGE_FILE_VERSION_V3) {
            /* Sentinel said "V3+" but version u32 disagrees — corruption. */
            fclose(f);
            return NULL;
        }
        if (fread(&config, sizeof(snn_lang_config_t), 1, f) != 1) {
            fclose(f);
            return NULL;
        }
        /* EOS + beam-rerank knobs are NOT trusted from the raw struct
         * blob — older V3/V4 files don't have all of them, and even V5
         * files leave the struct-blob copy redundant with the ext block.
         * Pre-reset to defaults so a V3 file (whose ext block stops at
         * EXT_KNOWN_SIZE_V3) leaves all new knobs at defaults; V4
         * overwrites EOS only; V5 overwrites everything authoritatively. */
        snn_lang_config_t cfg_defaults = snn_lang_config_default();
        config.enable_eos_stopping      = cfg_defaults.enable_eos_stopping;
        config.eos_min_activation       = cfg_defaults.eos_min_activation;
        config.eos_min_confidence       = cfg_defaults.eos_min_confidence;
        config.enable_beam_hnn_rerank   = cfg_defaults.enable_beam_hnn_rerank;
        config.beam_hnn_weight          = cfg_defaults.beam_hnn_weight;
        config.beam_length_norm_alpha   = cfg_defaults.beam_length_norm_alpha;
        /* Runtime-only knob, not in the ext block — reset to default. */
        config.ltd_margin               = cfg_defaults.ltd_margin;
        uint32_t ext_block_size = 0;
        if (fread(&ext_block_size, sizeof(uint32_t), 1, f) != 1) {
            fclose(f);
            return NULL;
        }
        /* Authoritative: explicit ext block overrides the struct-blob copy. */
        if (read_ext_config_v3_or_v4_or_v5(f, ext_block_size, &config) != 0) {
            fclose(f);
            return NULL;
        }
    } else {
        /* V2: rewind 4 bytes (we consumed what is actually max_concept_pops),
         * then read the legacy config blob in-place. The PA/MQ knob slots in
         * that legacy blob may be uninitialized/garbage on truly old builds —
         * reset them to library defaults rather than trusting whatever the
         * struct memcpy gave us. */
        if (fseek(f, -((long)sizeof(uint32_t)), SEEK_CUR) != 0) {
            fclose(f);
            return NULL;
        }
        if (fread(&config, sizeof(snn_lang_config_t), 1, f) != 1) {
            fclose(f);
            return NULL;
        }
        reset_persisted_knobs_to_defaults(&config);
    }

    snn_language_bridge_t* bridge = snn_language_bridge_create(&config);
    if (!bridge) {
        fclose(f);
        return NULL;
    }

    uint32_t num_concepts, num_words;
    if (fread(&num_concepts, sizeof(uint32_t), 1, f) != 1 ||
        fread(&num_words, sizeof(uint32_t), 1, f) != 1) {
        snn_language_bridge_destroy(bridge);
        fclose(f);
        return NULL;
    }

    // Load populations
    if (num_concepts <= bridge->concept_pops_capacity) {
        if (fread(bridge->concept_pops, sizeof(concept_pop_info_t), num_concepts, f)
            != num_concepts) {
            snn_language_bridge_destroy(bridge);
            fclose(f);
            return NULL;
        }
        bridge->num_concept_pops = num_concepts;
    }

    if (num_words <= bridge->word_pops_capacity) {
        if (fread(bridge->word_pops, sizeof(word_pop_info_t), num_words, f)
            != num_words) {
            snn_language_bridge_destroy(bridge);
            fclose(f);
            return NULL;
        }
        bridge->num_word_pops = num_words;
    }

    /* Option-1 (Slice A): the bridge no longer owns binding weights.
     * Older checkpoints still have a bindings array on disk — we read
     * past it to keep file-format compatibility but discard every
     * entry (bridge has no place to store them, nor any code path
     * that would use them). num_bindings on the new save is always 0. */
    uint32_t num_bindings;
    if (fread(&num_bindings, sizeof(uint32_t), 1, f) != 1) {
        snn_language_bridge_destroy(bridge);
        fclose(f);
        return NULL;
    }
    for (uint32_t i = 0; i < num_bindings; i++) {
        snn_lang_binding_t b;
        if (fread(&b, sizeof(snn_lang_binding_t), 1, f) != 1) break;
        /* discard */
    }
    /* word_norm_sq stays zeroed (calloc'd in create) — no bindings to
     * compute norms over. */

    /* V5 stats trailer — best-effort read. V3/V4 files have nothing here
     * (EOF), in which case stats stay zero. V5 files round-trip 30 fields. */
    if (file_version >= SNN_LANG_BRIDGE_FILE_VERSION_V5) {
        if (read_stats_block_v5(f, &bridge->stats) != 0) {
            /* Truncated/corrupt stats trailer is non-fatal — bridge data
             * still loaded cleanly. Stats reset to zero. */
            memset(&bridge->stats, 0, sizeof(bridge->stats));
        }
    }

    fclose(f);
    return bridge;
}

//=============================================================================
// Phase 8.5: Top-Down Binding -> Perception Attention Feedback
//=============================================================================

/**
 * WHAT: Generate attention weights from active concept bindings
 * WHY:  Language understanding guides sensory attention (top-down)
 * HOW:  Iterate all bindings; sum weights per concept population;
 *        normalize to [0, 1] attention weights
 */
int snn_language_bridge_generate_attention_feedback(
    snn_language_bridge_t* bridge,
    float* attention_weights,
    uint32_t num_weights)
{
    /* Option-1 (Slice A): the bridge no longer holds binding weights, so
     * there is no concept→word attention signal to derive from this side.
     * Return zeroed attention; callers that wanted top-down attention
     * should consult grounded_language / concept_registry once Slice B
     * lands. */
    if (!bridge || !attention_weights || num_weights == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_language_bridge_generate_attention_feedback: bridge or attention_weights is NULL");
        return -1;
    }
    memset(attention_weights, 0, num_weights * sizeof(float));
    return 0;
}

int snn_language_bridge_predict_sensory(
    snn_language_bridge_t* bridge,
    const float* concept_activations,
    uint32_t num_concepts,
    float* predicted_sensory,
    uint32_t sensory_dim)
{
    /* Option-1 (Slice A): no bridge-side bindings → no predictive sensory
     * pattern. Zero output and succeed. */
    (void)concept_activations; (void)num_concepts;
    if (!bridge || !predicted_sensory || sensory_dim == 0) return -1;
    memset(predicted_sensory, 0, sensory_dim * sizeof(float));
    return 0;
}
