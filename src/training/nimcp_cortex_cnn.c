/**
 * @file nimcp_cortex_cnn.c
 * @brief Per-cortex CNN processors — modality-specific feature extraction
 *
 * WHAT: Independent CNN processors for visual, audio, speech, somatosensory modalities
 * WHY:  Replace single classifier head with per-modality convolutional architectures
 * HOW:  Each processor wraps a cnn_trainer_t with modality-specific layers,
 *       provides forward/backward wrappers, attention fusion, and UTM adapter
 */

#include "training/nimcp_cortex_cnn.h"
#include "training/nimcp_cnn_training.h"
#include "training/nimcp_fno_layer.h"
#include "training/nimcp_unified_training.h"
/* W5 KG-integration: brain_internal MUST come before thalamic_channel.h /
 * thalamic_router.h so parietal.h's NIMCP_THALAMIC_ROUTER_T_DEFINED guard
 * wins over the non-guarded typedef in middleware/routing/. */
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "core/thalamic/nimcp_thalamic_channel.h"
#include "middleware/routing/nimcp_thalamic_router.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"

/* Phase 3 substrate integration: pulls in substrate_compute_effects,
 * substrate_debit_activity, substrate_apply_lr + the axon/dendrite
 * effect struct typedefs. */
#include "core/substrate/nimcp_substrate_effects.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "middleware/training/nimcp_optimizers.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* Forward-decl: avoids pulling the full thalamic_channel header into the
 * struct's public forward declarations. The full type is available via the
 * include above for internal use. */
struct thalamic_channel_s;

/* W5 KG-integration — CNN network-level aggregator event emitter.
 * Emits "feature_collapse" when loss becomes non-finite or exceeds 1e6
 * (gradient blow-up). Admin-token elevation per kg-registry §7.
 * (brain_internal.h + brain_kg.h included above.) */

static brain_t s_net_cnn_kg_brain = NULL;

void net_cnn_kg_register_brain(brain_t brain) {
    s_net_cnn_kg_brain = brain;
}

static void net_cnn_kg_emit_event(brain_t brain, const char* kind,
                                  float magnitude, uint64_t ts_us) {
    if (!brain || !kind) return;
    if (!brain->internal_kg_enabled || !brain->internal_kg) return;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    char ev_name[160];
    snprintf(ev_name, sizeof(ev_name),
             "net_cnn_event_%s_%llu", kind, (unsigned long long)ts_us);
    char desc[240];
    snprintf(desc, sizeof(desc),
             "CNN training event: kind=%s magnitude=%.4f", kind, magnitude);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, ev_name,
        BRAIN_KG_NODE_CORE, desc);
    brain_kg_node_id_t owner = brain_kg_find_node(kg, "net_cnn");
    if (owner != BRAIN_KG_INVALID_NODE && ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, owner, ev, BRAIN_KG_EDGE_SENDS_TO,
            "produced_by", magnitude);
    }
    if (ev != BRAIN_KG_INVALID_NODE) {
        char buf[40];
        snprintf(buf, sizeof(buf), "%.6f", magnitude);
        brain_kg_add_metadata(kg, ev, "magnitude", buf);
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)ts_us);
        brain_kg_add_metadata(kg, ev, "ts_us", buf);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/* Public test-facing trigger — forwards to static emit. */
void net_cnn_kg_trigger_event(brain_t brain, const char* kind,
                              float magnitude, uint64_t ts_us) {
    net_cnn_kg_emit_event(brain, kind, magnitude, ts_us);
}

/* ========================================================================= */
/* Default embedding dimensions per modality                                  */
/* ========================================================================= */

#define VISUAL_DEFAULT_EMBED_DIM   256
#define AUDIO_DEFAULT_EMBED_DIM    256
#define SPEECH_DEFAULT_EMBED_DIM   128
#define SOMATO_DEFAULT_EMBED_DIM   128

#define CORTEX_CNN_MAX_LABELS     4096
#define CORTEX_CNN_ATTENTION_TEMP  0.5f

/* ========================================================================= */
/* Thalamic adapter tunables (process-wide)                                   */
/*                                                                            */
/* These globals control how a cortex CNN attached via                        */
/* cortex_cnn_attach_thalamic_router() interacts with its channel during      */
/* forward. They are deliberately process-wide (not per-processor) so a       */
/* single safety toggle applies uniformly across all four modalities —        */
/* matches the SNN / LNN tunable convention.                                  */
/*                                                                            */
/* Default: all three enabled (1.0) so that attaching a router has a          */
/* visible effect out of the box.                                             */
/* ========================================================================= */
static float g_cnn_thalamic_enabled                 = 1.0f;
static float g_cnn_thalamic_featuremap_gain_on      = 1.0f;
static float g_cnn_thalamic_burst_dropout_reduce_on = 1.0f;

/* ========================================================================= */
/* Internal processor structure                                               */
/* ========================================================================= */

struct cortex_cnn_processor {
    cortex_cnn_type_t type;
    cnn_trainer_t* trainer;           /* Owned CNN trainer */
    uint32_t embedding_dim;           /* Output embedding size */

    /* Last forward result (cached for backward) */
    cnn_forward_result_t last_fwd;
    bool has_fwd_result;

    /* Embedding buffer (extracted from last forward output) */
    float* embedding;                 /* [embedding_dim] */

    /* Label management (simple index for one-hot) */
    char** labels;
    uint32_t num_labels;
    uint32_t max_labels;

    /* Metrics */
    float last_loss;
    float ema_loss;
    uint64_t forward_steps;
    uint64_t backward_steps;
    float confidence;                 /* Softmax max from last forward */
    uint32_t num_params;              /* Approximate param count */

    /* FNO spectral processors (per-modality). Each is fno_audio_processor_t*. */
    void* fno_audio;
    void* fno_visual;
    void* fno_speech;
    void* fno_somato;

    /* Phase 3 self-distillation training: per-FNO target (pre-FNO embedding[:64])
     * and last fno output. has_*_target=true means cortex_cnn_backward should
     * train the FNO on this step. Audio's FNO is the embedding (no pre-FNO
     * target available) — its has_target stays false. */
    float fno_visual_target[64];
    float fno_visual_emb[64];
    bool  fno_visual_has_target;
    float fno_speech_target[64];
    float fno_speech_emb[64];
    bool  fno_speech_has_target;
    float fno_somato_target[64];
    float fno_somato_emb[64];
    bool  fno_somato_has_target;

    /* FNO mix-in ramp: fno contribution scales as min(1.0, fno_distill_calls/RAMP_STEPS).
     * Counter is per-cortex, zero-initialized at create — NOT persisted in checkpoints
     * — so each daemon restart re-ramps. Gives the main net time to adapt as FNOs
     * transition from random-noise output to CNN-distilled output. */
    uint64_t fno_distill_calls;

    /* Substrate integration (Phase 3). Pointer borrowed. region_id is
     * per-modality so each cortex can have different local chemistry. */
    struct neural_substrate*      substrate;                    /* borrowed */
    uint32_t                      region_id;                    /* 0..3 per cortex_cnn_type_t */
    axon_substrate_effects_t      cached_axon_effects;
    dendrite_substrate_effects_t  cached_dend_effects;
    uint32_t                      substrate_steps_since_update; /* period counter */

    /* Thalamic adapter — owned channel, borrowed router. */
    struct thalamic_channel_s*    thalamic_channel;              /* owned */
};

/* ========================================================================= */
/* Substrate tunables (process-wide, same pattern as SNN/LNN adapters)        */
/* ========================================================================= */

/* Rationale: these four knobs are process-wide tuning rather than per-
 * instance config. Every live cortex CNN reads the same values from the
 * hot forward path, and a single-variable read is cheaper than chasing
 * a config pointer through an opaque handle. Matches snn_tune_* /
 * lnn_tune_* convention. Defaults enable all modulation. */
static float g_cnn_substrate_enabled           = 1.0f;
static float g_cnn_substrate_update_period     = 10.0f;  /* forward steps */
static float g_cnn_substrate_activation_mod_on = 1.0f;   /* scale post-act by integration_efficiency */
static float g_cnn_substrate_plasticity_mod_on = 1.0f;   /* scale lr by plasticity_mod */

/* Setters — booleans accept any nonzero as on; period clamped to [1, 10000]
 * forward steps, out-of-range silently ignored. */
void cortex_cnn_tune_set_substrate_enabled(float v) {
    g_cnn_substrate_enabled = (v != 0.0f) ? 1.0f : 0.0f;
}
void cortex_cnn_tune_set_substrate_update_period(float v) {
    if (v >= 1.0f && v <= 10000.0f) g_cnn_substrate_update_period = v;
}
void cortex_cnn_tune_set_substrate_activation_mod_on(float v) {
    g_cnn_substrate_activation_mod_on = (v != 0.0f) ? 1.0f : 0.0f;
}
void cortex_cnn_tune_set_substrate_plasticity_mod_on(float v) {
    g_cnn_substrate_plasticity_mod_on = (v != 0.0f) ? 1.0f : 0.0f;
}

/* Getters — mirror the setters. */
float cortex_cnn_tune_get_substrate_enabled(void)           { return g_cnn_substrate_enabled; }
float cortex_cnn_tune_get_substrate_update_period(void)     { return g_cnn_substrate_update_period; }
float cortex_cnn_tune_get_substrate_activation_mod_on(void) { return g_cnn_substrate_activation_mod_on; }
float cortex_cnn_tune_get_substrate_plasticity_mod_on(void) { return g_cnn_substrate_plasticity_mod_on; }

/* ========================================================================= */
/* Internal helpers                                                           */
/* ========================================================================= */

static const char* cortex_type_names[CORTEX_CNN_COUNT] = {
    "Visual", "Audio", "Speech", "Somato"
};

/**
 * @brief Refresh cached substrate effects if the update-period tick rolled over.
 *
 * WHAT: On the first call since attach (counter==0) or after N=period steps,
 *       reads the substrate and recomputes axon/dendrite effect structs into
 *       the per-processor cache. Advances the counter and resets it at period.
 * WHY:  Substrate effects change slowly relative to forward steps; recomputing
 *       every step wastes cycles. Rate-limited refresh matches LNN/SNN adapters.
 * HOW:  If the master knob is off or no substrate attached, returns NULLs.
 *       Otherwise returns pointers into proc->cached_*_effects on success.
 *
 *       W4 audit (Bug #4): the counter is only advanced here when the caller
 *       opts in via advance_counter=true. Callers that invoke this helper
 *       BEFORE a forward may fail (e.g. out-of-memory on input tensor create)
 *       should pass false so the refresh cadence does not drift on failed
 *       forwards. The canonical pattern is: refresh AFTER the forward has
 *       succeeded so the counter only ticks on completed steps.
 *
 * @param proc             Processor (must be non-NULL; checked by callers)
 * @param out_axon         [out] axon effects pointer or NULL
 * @param out_dend         [out] dendrite effects pointer or NULL
 * @param advance_counter  Whether to bump the period counter on this call.
 */
static void cortex_cnn_refresh_substrate(cortex_cnn_processor_t* proc,
                                          const axon_substrate_effects_t** out_axon,
                                          const dendrite_substrate_effects_t** out_dend,
                                          bool advance_counter) {
    if (out_axon) *out_axon = NULL;
    if (out_dend) *out_dend = NULL;
    if (!proc || !proc->substrate || g_cnn_substrate_enabled == 0.0f) return;

    uint32_t period = (uint32_t)g_cnn_substrate_update_period;
    if (proc->substrate_steps_since_update == 0 || period == 0) {
        (void)substrate_compute_effects(proc->substrate,
                                        &proc->cached_axon_effects,
                                        &proc->cached_dend_effects);
    }
    if (advance_counter) {
        proc->substrate_steps_since_update++;
        if (period > 0 && proc->substrate_steps_since_update >= period) {
            proc->substrate_steps_since_update = 0;
        }
    }

    if (out_axon) *out_axon = &proc->cached_axon_effects;
    if (out_dend) *out_dend = &proc->cached_dend_effects;
}

/**
 * @brief Apply substrate activation modulation to an embedding buffer.
 *
 * WHAT: Scales each element of embedding[0..dim-1] by dend->integration_efficiency
 *       when the activation_mod knob is on and the multiplier differs from 1.0.
 * WHY:  Dendritic substrate damage reduces post-synaptic integration efficiency;
 *       mirror that in the CNN embedding amplitude so downstream consumers see
 *       a weaker signal when chemistry is compromised.
 * HOW:  Null-tolerant. Called by each modality forward once the embedding is
 *       populated.
 */
static void cortex_cnn_apply_activation_mod(float* embedding,
                                             uint32_t dim,
                                             const dendrite_substrate_effects_t* dend) {
    if (!embedding || dim == 0 || !dend) return;
    if (g_cnn_substrate_activation_mod_on == 0.0f) return;
    float m = dend->integration_efficiency;
    if (m == 1.0f) return;
    for (uint32_t i = 0; i < dim; i++) {
        embedding[i] *= m;
    }
}

/**
 * @brief Report network activity back to the substrate (end-of-forward debit).
 *
 * WHAT: Uses L1 norm of the embedding × 100 as the activity proxy, same
 *       pattern as the LNN adapter (continuous-valued output, no discrete
 *       spikes to count).
 * WHY:  Sustained activity consumes ATP; debiting closes the loop between
 *       cortex output and metabolic state so a heavily-used cortex gradually
 *       degrades its own substrate and therefore its own effects.
 * HOW:  Only debits when substrate is attached AND master knob is on. Plasticity
 *       updates are 0 in the forward phase (backward applies its own debit).
 */
static void cortex_cnn_debit_activity(cortex_cnn_processor_t* proc) {
    if (!proc || !proc->substrate || g_cnn_substrate_enabled == 0.0f) return;
    if (!proc->embedding || proc->embedding_dim == 0) return;

    float sum_abs = 0.0f;
    for (uint32_t i = 0; i < proc->embedding_dim; i++) {
        float v = proc->embedding[i];
        if (isfinite(v)) sum_abs += fabsf(v);
    }
    uint32_t activity = (uint32_t)(sum_abs * 100.0f);
    substrate_debit_activity(proc->substrate,
                             proc->region_id,
                             activity,
                             0 /* plasticity counted separately */);
}

/**
 * @brief Get or create label index for a string label
 */
static uint32_t cortex_get_or_create_label(cortex_cnn_processor_t* proc, const char* label) {
    if (!proc || !label) return 0;

    /* Search existing labels */
    for (uint32_t i = 0; i < proc->num_labels; i++) {
        if (proc->labels[i] && strcmp(proc->labels[i], label) == 0) {
            return i;
        }
    }

    /* Create new label */
    if (proc->num_labels >= proc->max_labels) return 0;

    size_t len = strlen(label);
    proc->labels[proc->num_labels] = (char*)nimcp_malloc(len + 1);
    if (!proc->labels[proc->num_labels]) return 0;
    memcpy(proc->labels[proc->num_labels], label, len + 1);

    return proc->num_labels++;
}

/**
 * @brief Extract embedding from CNN forward output
 *
 * The last dense layer's output IS the embedding. Copy it to proc->embedding.
 * Also compute confidence as softmax max.
 */
static void extract_embedding(cortex_cnn_processor_t* proc) {
    if (!proc->has_fwd_result || !proc->last_fwd.output) return;

    const float* out = (const float*)nimcp_tensor_data_const(proc->last_fwd.output);
    size_t numel = nimcp_tensor_numel(proc->last_fwd.output);
    if (!out || numel == 0) return;
    if (!proc->embedding) return;  /* Guard against NULL embedding buffer */

    uint32_t copy_dim = (proc->embedding_dim < (uint32_t)numel) ?
                         proc->embedding_dim : (uint32_t)numel;
    memcpy(proc->embedding, out, copy_dim * sizeof(float));

    /* Zero-pad if output smaller than embedding_dim */
    if (copy_dim < proc->embedding_dim) {
        memset(proc->embedding + copy_dim, 0,
               (proc->embedding_dim - copy_dim) * sizeof(float));
    }

    /* Compute confidence = max of softmax-like values.
     * Use exp(x)/sum(exp(x)) but for efficiency just find max output. */
    float max_val = -1e30f;
    float sum_exp = 0.0f;
    for (uint32_t i = 0; i < copy_dim; i++) {
        if (out[i] > max_val) max_val = out[i];
    }
    for (uint32_t i = 0; i < copy_dim; i++) {
        sum_exp += expf(out[i] - max_val);
    }
    proc->confidence = (sum_exp > 0.0f) ? (1.0f / sum_exp) : 0.0f;
}

/**
 * @brief Apply thalamic attention gating and submit-for-routing after forward
 *
 * WHAT: (1) If a thalamic channel is attached, scale the final embedding by
 *       the channel's attention gate for destination 0. (2) Submit the
 *       (possibly gated) embedding to the router. (3) Tick the channel.
 * WHY:  Per-cortex attention gating lets the router dynamically suppress
 *       a modality whose signal is stale/irrelevant. The submit-tick pair
 *       lets the router's Hebbian learning update routes based on which
 *       modalities actually fire together.
 * HOW:  Zero-cost no-op when either the channel is not attached or the
 *       master enable tunable is 0. The gate-!=-1 check avoids the embed
 *       scaling loop when the router has not diverged from its default
 *       uniform attention.
 *
 * Must be called AFTER the embedding has been finalized (post-FNO mix-in)
 * but BEFORE the forward function returns.
 */
static void cortex_apply_thalamic_gate(cortex_cnn_processor_t* proc) {
    if (!proc) return;
    if (!proc->thalamic_channel) return;
    if (g_cnn_thalamic_enabled == 0.0f) return;
    if (!proc->embedding || proc->embedding_dim == 0) return;

    thalamic_channel_t* ch = (thalamic_channel_t*)proc->thalamic_channel;

    /* (1) Feature-map attention gain. */
    if (g_cnn_thalamic_featuremap_gain_on != 0.0f) {
        float gate = thalamic_channel_get_gate(ch, 0 /* dest */);
        if (gate != 1.0f) {
            for (uint32_t i = 0; i < proc->embedding_dim; ++i) {
                proc->embedding[i] *= gate;
            }
        }
    }

    /* (2) Burst-mode dropout reduction: the actual dropout adjustment
     * requires plumbing into cnn_trainer's dropout-layer state, which is
     * out of scope for this adapter. For this MVP we log the mode at
     * low frequency so callers can observe burst transitions. */
    if (g_cnn_thalamic_burst_dropout_reduce_on != 0.0f &&
        ch->mode == THALAMIC_CHAN_BURST) {
        /* Log once every ~1024 forward steps to avoid flooding. */
        if ((proc->forward_steps & 1023u) == 0u) {
            NIMCP_LOGGING_DEBUG(
                "cortex_cnn[%d]: thalamic channel in BURST mode "
                "(dropout-reduce hint active, actual clamp is a future wiring)",
                (int)proc->type);
        }
    }

    /* (3) Submit embedding to router + tick channel. */
    thalamic_channel_submit(ch, 0 /* dest */,
                            proc->embedding,
                            (size_t)proc->embedding_dim,
                            THALAMIC_PRIORITY_NORMAL);
    thalamic_channel_tick(ch);
}

/**
 * @brief Run CNN forward on a 1D float tensor — raw CNN only, no substrate / thalamic hooks.
 *
 * W3/W4 audit: previously this helper also applied the thalamic gate, which
 * broke the "substrate → thalamic → debit" ordering invariant: callers that
 * layered substrate activation_mod on top of cortex_forward_1d applied
 * activation_mod AFTER the gate (wrong order). It also bypassed substrate
 * entirely on the UTM adapter path. The fix moves ALL post-forward hooks
 * out of this helper; callers must invoke cortex_cnn_apply_post_forward()
 * explicitly after any modality-specific mix-in step to restore the
 * canonical order.
 */
static const float* cortex_forward_1d(cortex_cnn_processor_t* proc,
                                       const float* data, uint32_t size) {
    if (!proc || !proc->trainer || !data || size == 0) return NULL;

    /* Conv2d expects (batch, channels, height, width) — reshape 1D data
     * to [1, 1, 1, size] so the conv kernel slides along the width axis */
    uint32_t dims[4] = {1, 1, 1, size};
    nimcp_tensor_t* input = nimcp_tensor_create(dims, 4, NIMCP_DTYPE_F32);
    if (!input) return NULL;

    memcpy(nimcp_tensor_data(input), data, size * sizeof(float));

    memset(&proc->last_fwd, 0, sizeof(proc->last_fwd));
    nimcp_error_t err = cnn_trainer_forward(proc->trainer, input, &proc->last_fwd);
    nimcp_tensor_destroy(input);

    if (err != NIMCP_SUCCESS) {
        proc->has_fwd_result = false;
        return NULL;
    }

    proc->has_fwd_result = true;
    proc->forward_steps++;
    extract_embedding(proc);
    /* Intentionally no substrate / thalamic / debit hooks here — caller owns
     * the post-forward sequence via cortex_cnn_apply_post_forward(). */
    return proc->embedding;
}

/**
 * @brief Apply the canonical post-forward sequence to proc->embedding.
 *
 * The design invariant is: substrate activation_mod → thalamic gate →
 * substrate activity debit. Every modality forward and the UTM adapter must
 * run this sequence AFTER the embedding (including any FNO mix-in) has been
 * finalized so substrate damage and attention gating compound correctly.
 *
 * Also refreshes the substrate cache here (post-forward) so the period
 * counter only advances when the forward actually completed, fixing the
 * Bug #4 cadence drift on early-return.
 */
static void cortex_cnn_apply_post_forward(cortex_cnn_processor_t* proc) {
    if (!proc || !proc->embedding || proc->embedding_dim == 0) return;

    const axon_substrate_effects_t*     axon_eff = NULL;
    const dendrite_substrate_effects_t* dend_eff = NULL;
    cortex_cnn_refresh_substrate(proc, &axon_eff, &dend_eff, /*advance_counter=*/true);
    (void)axon_eff;  /* axon effects are consumed by backward (plasticity_mod), not here */

    cortex_cnn_apply_activation_mod(proc->embedding, proc->embedding_dim, dend_eff);
    cortex_apply_thalamic_gate(proc);
    cortex_cnn_debit_activity(proc);
}

/* ========================================================================= */
/* Factory — build modality-specific architectures                            */
/* ========================================================================= */

/**
 * @brief Build visual cortex CNN architecture
 *
 * Conv2d(3->16,3x3)->BN->ReLU->Pool(2x2)
 * ->Conv2d(16->32,3x3)->BN->ReLU->Pool(2x2)
 * ->Conv2d(32->64,3x3)->BN->ReLU->GlobalAvgPool
 * ->Dense(64->embed_dim)
 */
static int build_visual_architecture(cnn_trainer_t* trainer, uint32_t embed_dim) {
    /* Conv1: 3->16, 3x3, padding=1 */
    cnn_conv_config_t conv1 = {
        .kernel_h = 3, .kernel_w = 3,
        .stride_h = 1, .stride_w = 1,
        .padding_h = 1, .padding_w = 1,
        .dilation_h = 1, .dilation_w = 1,
        .in_channels = 3, .out_channels = 16,
        .groups = 1,
        .activation = CNN_ACTIVATION_NONE,
        .use_bias = true,
        .weight_init_std = 0.01f
    };
    if (!cnn_trainer_add_conv_layer(trainer, &conv1)) return -1;

    cnn_batch_norm_config_t bn1 = {
        .num_features = 16, .epsilon = 1e-5f,
        .momentum = 0.1f, .affine = true, .track_running_stats = true
    };
    if (!cnn_trainer_add_batch_norm_layer(trainer, &bn1)) return -1;
    if (!cnn_trainer_add_activation_layer(trainer, CNN_ACTIVATION_RELU)) return -1;

    cnn_pool_config_t pool1 = {
        .type = CNN_POOL_MAX,
        .pool_h = 2, .pool_w = 2,
        .stride_h = 2, .stride_w = 2
    };
    if (!cnn_trainer_add_pool_layer(trainer, &pool1)) return -1;

    /* Conv2: 16->32, 3x3, padding=1 */
    cnn_conv_config_t conv2 = {
        .kernel_h = 3, .kernel_w = 3,
        .stride_h = 1, .stride_w = 1,
        .padding_h = 1, .padding_w = 1,
        .dilation_h = 1, .dilation_w = 1,
        .in_channels = 16, .out_channels = 32,
        .groups = 1,
        .activation = CNN_ACTIVATION_NONE,
        .use_bias = true,
        .weight_init_std = 0.01f
    };
    if (!cnn_trainer_add_conv_layer(trainer, &conv2)) return -1;

    cnn_batch_norm_config_t bn2 = {
        .num_features = 32, .epsilon = 1e-5f,
        .momentum = 0.1f, .affine = true, .track_running_stats = true
    };
    if (!cnn_trainer_add_batch_norm_layer(trainer, &bn2)) return -1;
    if (!cnn_trainer_add_activation_layer(trainer, CNN_ACTIVATION_RELU)) return -1;

    cnn_pool_config_t pool2 = {
        .type = CNN_POOL_MAX,
        .pool_h = 2, .pool_w = 2,
        .stride_h = 2, .stride_w = 2
    };
    if (!cnn_trainer_add_pool_layer(trainer, &pool2)) return -1;

    /* Conv3: 32->64, 3x3, padding=1 */
    cnn_conv_config_t conv3 = {
        .kernel_h = 3, .kernel_w = 3,
        .stride_h = 1, .stride_w = 1,
        .padding_h = 1, .padding_w = 1,
        .dilation_h = 1, .dilation_w = 1,
        .in_channels = 32, .out_channels = 64,
        .groups = 1,
        .activation = CNN_ACTIVATION_NONE,
        .use_bias = true,
        .weight_init_std = 0.01f
    };
    if (!cnn_trainer_add_conv_layer(trainer, &conv3)) return -1;

    cnn_batch_norm_config_t bn3 = {
        .num_features = 64, .epsilon = 1e-5f,
        .momentum = 0.1f, .affine = true, .track_running_stats = true
    };
    if (!cnn_trainer_add_batch_norm_layer(trainer, &bn3)) return -1;
    if (!cnn_trainer_add_activation_layer(trainer, CNN_ACTIVATION_RELU)) return -1;

    /* Global Average Pool */
    cnn_pool_config_t gap = {
        .type = CNN_POOL_GLOBAL_AVERAGE,
        .pool_h = 0, .pool_w = 0,  /* Global = entire spatial extent */
        .stride_h = 1, .stride_w = 1
    };
    if (!cnn_trainer_add_pool_layer(trainer, &gap)) return -1;

    /* Flatten + Dense(64->embed_dim) */
    if (!cnn_trainer_add_flatten_layer(trainer)) return -1;

    cnn_dense_config_t dense = {
        .in_features = 64,
        .out_features = embed_dim,
        .activation = CNN_ACTIVATION_NONE,
        .use_bias = true,
        .weight_init_std = 0.01f
    };
    if (!cnn_trainer_add_dense_layer(trainer, &dense)) return -1;

    return 0;
}

/**
 * @brief Build audio cortex CNN architecture
 *
 * Conv2d(1->16,1x5)->BN->ReLU->Pool(1x2)
 * ->Conv2d(16->32,1x3)->BN->ReLU->GlobalAvgPool
 * ->Dense(32->embed_dim)
 */
static int build_audio_architecture(cnn_trainer_t* trainer, uint32_t embed_dim) {
    cnn_conv_config_t conv1 = {
        .kernel_h = 1, .kernel_w = 5,
        .stride_h = 1, .stride_w = 1,
        .padding_h = 0, .padding_w = 2,
        .dilation_h = 1, .dilation_w = 1,
        .in_channels = 1, .out_channels = 16,
        .groups = 1,
        .activation = CNN_ACTIVATION_NONE,
        .use_bias = true,
        .weight_init_std = 0.01f
    };
    if (!cnn_trainer_add_conv_layer(trainer, &conv1)) return -1;

    cnn_batch_norm_config_t bn1 = {
        .num_features = 16, .epsilon = 1e-5f,
        .momentum = 0.1f, .affine = true, .track_running_stats = true
    };
    if (!cnn_trainer_add_batch_norm_layer(trainer, &bn1)) return -1;
    if (!cnn_trainer_add_activation_layer(trainer, CNN_ACTIVATION_RELU)) return -1;

    cnn_pool_config_t pool1 = {
        .type = CNN_POOL_MAX,
        .pool_h = 1, .pool_w = 2,
        .stride_h = 1, .stride_w = 2
    };
    if (!cnn_trainer_add_pool_layer(trainer, &pool1)) return -1;

    cnn_conv_config_t conv2 = {
        .kernel_h = 1, .kernel_w = 3,
        .stride_h = 1, .stride_w = 1,
        .padding_h = 0, .padding_w = 1,
        .dilation_h = 1, .dilation_w = 1,
        .in_channels = 16, .out_channels = 32,
        .groups = 1,
        .activation = CNN_ACTIVATION_NONE,
        .use_bias = true,
        .weight_init_std = 0.01f
    };
    if (!cnn_trainer_add_conv_layer(trainer, &conv2)) return -1;

    cnn_batch_norm_config_t bn2 = {
        .num_features = 32, .epsilon = 1e-5f,
        .momentum = 0.1f, .affine = true, .track_running_stats = true
    };
    if (!cnn_trainer_add_batch_norm_layer(trainer, &bn2)) return -1;
    if (!cnn_trainer_add_activation_layer(trainer, CNN_ACTIVATION_RELU)) return -1;

    cnn_pool_config_t gap = {
        .type = CNN_POOL_GLOBAL_AVERAGE,
        .pool_h = 0, .pool_w = 0,
        .stride_h = 1, .stride_w = 1
    };
    if (!cnn_trainer_add_pool_layer(trainer, &gap)) return -1;

    if (!cnn_trainer_add_flatten_layer(trainer)) return -1;

    cnn_dense_config_t dense = {
        .in_features = 32,
        .out_features = embed_dim,
        .activation = CNN_ACTIVATION_NONE,
        .use_bias = true,
        .weight_init_std = 0.01f
    };
    if (!cnn_trainer_add_dense_layer(trainer, &dense)) return -1;

    return 0;
}

/**
 * @brief Build speech cortex CNN architecture
 *
 * Conv2d(1->16,1x3)->BN->ReLU
 * ->Conv2d(16->32,1x3)->BN->ReLU->GlobalAvgPool
 * ->Dense(32->embed_dim)
 */
static int build_speech_architecture(cnn_trainer_t* trainer, uint32_t embed_dim) {
    cnn_conv_config_t conv1 = {
        .kernel_h = 1, .kernel_w = 3,
        .stride_h = 1, .stride_w = 1,
        .padding_h = 0, .padding_w = 1,
        .dilation_h = 1, .dilation_w = 1,
        .in_channels = 1, .out_channels = 16,
        .groups = 1,
        .activation = CNN_ACTIVATION_NONE,
        .use_bias = true,
        .weight_init_std = 0.01f
    };
    if (!cnn_trainer_add_conv_layer(trainer, &conv1)) return -1;

    cnn_batch_norm_config_t bn1 = {
        .num_features = 16, .epsilon = 1e-5f,
        .momentum = 0.1f, .affine = true, .track_running_stats = true
    };
    if (!cnn_trainer_add_batch_norm_layer(trainer, &bn1)) return -1;
    if (!cnn_trainer_add_activation_layer(trainer, CNN_ACTIVATION_RELU)) return -1;

    cnn_conv_config_t conv2 = {
        .kernel_h = 1, .kernel_w = 3,
        .stride_h = 1, .stride_w = 1,
        .padding_h = 0, .padding_w = 1,
        .dilation_h = 1, .dilation_w = 1,
        .in_channels = 16, .out_channels = 32,
        .groups = 1,
        .activation = CNN_ACTIVATION_NONE,
        .use_bias = true,
        .weight_init_std = 0.01f
    };
    if (!cnn_trainer_add_conv_layer(trainer, &conv2)) return -1;

    cnn_batch_norm_config_t bn2 = {
        .num_features = 32, .epsilon = 1e-5f,
        .momentum = 0.1f, .affine = true, .track_running_stats = true
    };
    if (!cnn_trainer_add_batch_norm_layer(trainer, &bn2)) return -1;
    if (!cnn_trainer_add_activation_layer(trainer, CNN_ACTIVATION_RELU)) return -1;

    cnn_pool_config_t gap = {
        .type = CNN_POOL_GLOBAL_AVERAGE,
        .pool_h = 0, .pool_w = 0,
        .stride_h = 1, .stride_w = 1
    };
    if (!cnn_trainer_add_pool_layer(trainer, &gap)) return -1;

    if (!cnn_trainer_add_flatten_layer(trainer)) return -1;

    cnn_dense_config_t dense = {
        .in_features = 32,
        .out_features = embed_dim,
        .activation = CNN_ACTIVATION_NONE,
        .use_bias = true,
        .weight_init_std = 0.01f
    };
    if (!cnn_trainer_add_dense_layer(trainer, &dense)) return -1;

    return 0;
}

/**
 * @brief Build somatosensory cortex architecture (dense-only, no conv)
 *
 * Dense(45->64)->ReLU->Dense(64->embed_dim)
 */
static int build_somato_architecture(cnn_trainer_t* trainer, uint32_t embed_dim) {
    /* Input size: 3-level Haar wavelet of 45 segments = 45 + 23 + 12 = 80 dims.
     * Weber-Fechner scaling + wavelet decomposition happens in cortex_cnn_forward_somato. */
    cnn_dense_config_t dense1 = {
        .in_features = 80,
        .out_features = 64,
        .activation = CNN_ACTIVATION_NONE,
        .use_bias = true,
        .weight_init_std = 0.01f
    };
    if (!cnn_trainer_add_dense_layer(trainer, &dense1)) return -1;
    if (!cnn_trainer_add_activation_layer(trainer, CNN_ACTIVATION_RELU)) return -1;

    cnn_dense_config_t dense2 = {
        .in_features = 64,
        .out_features = embed_dim,
        .activation = CNN_ACTIVATION_NONE,
        .use_bias = true,
        .weight_init_std = 0.01f
    };
    if (!cnn_trainer_add_dense_layer(trainer, &dense2)) return -1;

    return 0;
}

static uint32_t default_embed_dim(cortex_cnn_type_t type) {
    switch (type) {
        case CORTEX_CNN_VISUAL:  return VISUAL_DEFAULT_EMBED_DIM;
        case CORTEX_CNN_AUDIO:   return AUDIO_DEFAULT_EMBED_DIM;
        case CORTEX_CNN_SPEECH:  return SPEECH_DEFAULT_EMBED_DIM;
        case CORTEX_CNN_SOMATO:  return SOMATO_DEFAULT_EMBED_DIM;
        default: return 32;
    }
}

static uint32_t estimate_params(cortex_cnn_type_t type) {
    switch (type) {
        case CORTEX_CNN_VISUAL:  return 30000;
        case CORTEX_CNN_AUDIO:   return 8000;
        case CORTEX_CNN_SPEECH:  return 4000;
        case CORTEX_CNN_SOMATO:  return 4000;
        default: return 0;
    }
}

/* ========================================================================= */
/* Public API: Lifecycle                                                      */
/* ========================================================================= */

cortex_cnn_processor_t* cortex_cnn_create(cortex_cnn_type_t type, uint32_t embedding_dim) {
    if (type < 0 || type >= CORTEX_CNN_COUNT) {
        NIMCP_LOGGING_WARN("cortex_cnn_create: invalid type %d", (int)type);
        return NULL;
    }

    if (embedding_dim == 0) {
        embedding_dim = default_embed_dim(type);
    }
    NIMCP_LOGGING_INFO("cortex_cnn_create: type=%d embed_dim=%u", (int)type, embedding_dim);

    cortex_cnn_processor_t* proc = (cortex_cnn_processor_t*)nimcp_calloc(
        1, sizeof(cortex_cnn_processor_t));
    if (!proc) return NULL;

    proc->type = type;
    proc->embedding_dim = embedding_dim;
    proc->ema_loss = -1.0f;  /* Sentinel: not yet computed */

    /* Allocate embedding buffer */
    proc->embedding = (float*)nimcp_calloc(embedding_dim, sizeof(float));
    if (!proc->embedding) {
        nimcp_free(proc);
        return NULL;
    }

    /* Allocate label storage */
    proc->max_labels = CORTEX_CNN_MAX_LABELS;
    proc->labels = (char**)nimcp_calloc(proc->max_labels, sizeof(char*));
    if (!proc->labels) {
        nimcp_free(proc->embedding);
        nimcp_free(proc);
        return NULL;
    }

    /* Create CNN trainer */
    cnn_trainer_config_t cfg;
    cnn_trainer_default_config(&cfg);
    cfg.learning_rate = 0.001f;
    cfg.gradient_clip_value = 5.0f;
    cfg.diversity_loss_weight = 0.1f;
    cfg.use_gradient_normalization = true;
    cfg.gradient_target_norm = 1.0f;

    proc->trainer = cnn_trainer_create(&cfg);
    if (!proc->trainer) {
        nimcp_free(proc->labels);
        nimcp_free(proc->embedding);
        nimcp_free(proc);
        return NULL;
    }

    /* Build modality-specific architecture */
    int rc = -1;
    switch (type) {
        case CORTEX_CNN_VISUAL:  rc = build_visual_architecture(proc->trainer, embedding_dim); break;
        case CORTEX_CNN_AUDIO:   rc = build_audio_architecture(proc->trainer, embedding_dim); break;
        case CORTEX_CNN_SPEECH:  rc = build_speech_architecture(proc->trainer, embedding_dim); break;
        case CORTEX_CNN_SOMATO:  rc = build_somato_architecture(proc->trainer, embedding_dim); break;
        default: break;
    }

    if (rc != 0) {
        NIMCP_LOGGING_WARN("cortex_cnn_create: failed to build %s architecture",
                          cortex_type_names[type]);
        cnn_trainer_destroy(proc->trainer);
        nimcp_free(proc->labels);
        nimcp_free(proc->embedding);
        nimcp_free(proc);
        return NULL;
    }

    proc->num_params = estimate_params(type);

    /* Substrate integration: init fields. region_id mirrors the cortex type
     * (visual=0, audio=1, speech=2, somato=3) so each modality gets its own
     * local metabolic compartment (caller can extend to separate substrates
     * via cortex_cnn_attach_substrate). */
    proc->substrate = NULL;
    proc->region_id = (uint32_t)type;
    memset(&proc->cached_axon_effects, 0, sizeof(proc->cached_axon_effects));
    memset(&proc->cached_dend_effects, 0, sizeof(proc->cached_dend_effects));
    proc->substrate_steps_since_update = 0;

    NIMCP_LOGGING_INFO("Created %s cortex CNN processor: embed_dim=%u, ~%u params",
                      cortex_type_names[type], embedding_dim, proc->num_params);
    return proc;
}

/**
 * @brief Attach a neural substrate to the cortex CNN (borrowed pointer).
 *
 * WHAT: Stores a borrowed pointer to the substrate and resets the effect
 *       cache + update counter so the next forward step recomputes fresh.
 * WHY:  The cortex needs a substrate handle to read ATP/temperature/ion/
 *       membrane state on each refresh tick. Ownership remains with the
 *       caller — the processor does not free it.
 * HOW:  Null-tolerant on both arguments.
 */
void cortex_cnn_attach_substrate(cortex_cnn_processor_t* proc,
                                 struct neural_substrate* substrate) {
    if (!proc) return;
    proc->substrate = substrate;
    memset(&proc->cached_axon_effects, 0, sizeof(proc->cached_axon_effects));
    memset(&proc->cached_dend_effects, 0, sizeof(proc->cached_dend_effects));
    proc->substrate_steps_since_update = 0;
    /* F1 CRITICAL mirror: populate cache IMMEDIATELY so downstream consumers
     * that read cached_dend_effects.plasticity_mod before the first forward
     * step don't multiply LR by 0. Without this, learning silently dies on
     * any pre-step inference or training hop that skips the refresh block. */
    if (substrate) {
        substrate_compute_effects(substrate,
                                  &proc->cached_axon_effects,
                                  &proc->cached_dend_effects);
        NIMCP_LOGGING_DEBUG("cortex_cnn_attach_substrate: eager-populated substrate cache after attach");
    }
    NIMCP_LOGGING_DEBUG("cortex_cnn_attach_substrate: type=%s region=%u substrate=%p",
                        cortex_type_names[proc->type], proc->region_id,
                        (void*)substrate);
}

void cortex_cnn_destroy(cortex_cnn_processor_t* proc) {
    if (!proc) return;

    /* Free the thalamic channel (router is borrowed — not freed here). */
    if (proc->thalamic_channel) {
        thalamic_channel_destroy((thalamic_channel_t*)proc->thalamic_channel);
        proc->thalamic_channel = NULL;
    }

    if (proc->trainer) {
        cnn_trainer_destroy(proc->trainer);
    }
    if (proc->labels) {
        for (uint32_t i = 0; i < proc->num_labels; i++) {
            nimcp_free(proc->labels[i]);
        }
        nimcp_free(proc->labels);
    }
    nimcp_free(proc->embedding);
    nimcp_free(proc);
}

/* ========================================================================= */
/* Public API: Forward Pass                                                   */
/* ========================================================================= */

const float* cortex_cnn_forward_visual(cortex_cnn_processor_t* proc,
                                        const uint8_t* pixels,
                                        uint32_t w, uint32_t h, uint32_t ch) {
    if (!proc || proc->type != CORTEX_CNN_VISUAL || !pixels) return NULL;
    if (!proc->trainer) return NULL;  /* defensive: partial load / racy destroy */
    if (w == 0 || h == 0 || ch == 0) return NULL;

    /* Convert uint8 pixels to float [0,1] and pack as 1D tensor (H*W*C) */
    uint32_t total = w * h * ch;
    float* float_data = (float*)nimcp_malloc(total * sizeof(float));
    if (!float_data) return NULL;

    for (uint32_t i = 0; i < total; i++) {
        float_data[i] = (float)pixels[i] / 255.0f;
    }

    /* Create 4D tensor: [1, ch, h, w] for CNN forward */
    uint32_t dims[4] = {1, ch, h, w};
    nimcp_tensor_t* input = nimcp_tensor_create(dims, 4, NIMCP_DTYPE_F32);
    if (!input) {
        nimcp_free(float_data);
        return NULL;
    }

    /* Reorder from HWC to CHW */
    float* tensor_data = (float*)nimcp_tensor_data(input);
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            for (uint32_t c = 0; c < ch; c++) {
                uint32_t hwc_idx = (y * w + x) * ch + c;
                uint32_t chw_idx = c * (h * w) + y * w + x;
                tensor_data[chw_idx] = float_data[hwc_idx];
            }
        }
    }
    nimcp_free(float_data);

    /* Forward through CNN */
    memset(&proc->last_fwd, 0, sizeof(proc->last_fwd));
    nimcp_error_t err = cnn_trainer_forward(proc->trainer, input, &proc->last_fwd);
    nimcp_tensor_destroy(input);

    if (err != NIMCP_SUCCESS) {
        proc->has_fwd_result = false;
        return NULL;
    }

    proc->has_fwd_result = true;
    proc->forward_steps++;
    extract_embedding(proc);

    /* FNO visual: spectral convolution on grayscale scanline.
     * Captures spatial frequency patterns (edges, textures) that complement CNN.
     *
     * W3 audit (Bug #2): FNO mix-in MUST happen before the substrate
     * activation_mod so substrate damage scales BOTH the CNN and FNO
     * contributions. Previously activation_mod ran before the mix-in, which
     * left the FNO component unscaled and decoupled from substrate state. */
    if (proc->fno_visual && proc->embedding) {
        fno_audio_processor_t* fno = (fno_audio_processor_t*)proc->fno_visual;
        uint32_t fno_in = fno->input_size;
        /* Store target BEFORE adding FNO contribution (pre-FNO embedding[:64]). */
        uint32_t tn = (proc->embedding_dim < 64) ? proc->embedding_dim : 64;
        for (uint32_t i = 0; i < tn; i++) proc->fno_visual_target[i] = proc->embedding[i];
        for (uint32_t i = tn; i < 64; i++) proc->fno_visual_target[i] = 0.0f;
        float* scanline = (float*)nimcp_calloc(fno_in, sizeof(float));
        if (scanline) {
            /* Downsample grayscale to FNO input size */
            uint32_t scan_total = w * h;
            for (uint32_t i = 0; i < fno_in; i++) {
                uint32_t src = (i * scan_total) / fno_in;
                if (src < total) scanline[i] = (float)pixels[src * ch] / 255.0f;
            }
            float fno_emb[64];
            if (fno_audio_forward(fno, scanline, fno_in, fno_emb) == 0) {
                uint32_t n = (proc->embedding_dim < 64) ? proc->embedding_dim : 64;
                float ramp = (float)proc->fno_distill_calls / 1000.0f;
                if (ramp > 1.0f) ramp = 1.0f;
                float mix = 0.2f * ramp;
                for (uint32_t i = 0; i < n; i++) {
                    proc->fno_visual_emb[i] = fno_emb[i];
                    proc->embedding[i] += mix * fno_emb[i];
                }
                proc->fno_visual_has_target = true;
            }
            nimcp_free(scanline);
        }
    }

    /* Canonical post-forward sequence: substrate refresh (counter advances
     * only now, so failed forwards don't drift the cadence) → activation_mod
     * → thalamic gate → activity debit. */
    cortex_cnn_apply_post_forward(proc);
    return proc->embedding;
}

const float* cortex_cnn_forward_audio(cortex_cnn_processor_t* proc,
                                       const float* mel, uint32_t mel_size) {
    if (!proc || proc->type != CORTEX_CNN_AUDIO || !mel) return NULL;
    if (mel_size == 0) return NULL;

    /* FNO path — spectral convolution for native frequency-domain processing */
    if (proc->fno_audio) {
        fno_audio_processor_t* fno = (fno_audio_processor_t*)proc->fno_audio;
        if (!proc->embedding) {
            proc->embedding = nimcp_calloc(proc->embedding_dim, sizeof(float));
            if (!proc->embedding) return NULL;
        }
        int rc = fno_audio_forward(fno, mel, mel_size, proc->embedding);
        if (rc != 0) {
            proc->has_fwd_result = false;
            return NULL;
        }
        proc->has_fwd_result = true;
        proc->forward_steps++;
        /* Canonical post-forward sequence: substrate refresh (counter
         * advances here, NOT earlier, so early-return failures above do not
         * drift the refresh cadence) → activation_mod → thalamic gate →
         * activity debit. */
        cortex_cnn_apply_post_forward(proc);
        /* Compute confidence from (post-mod) embedding norm */
        float norm = 0.0f;
        for (uint32_t i = 0; i < proc->embedding_dim; i++)
            norm += proc->embedding[i] * proc->embedding[i];
        proc->confidence = (norm > 0.0f) ? 1.0f / (1.0f + expf(-sqrtf(norm))) : 0.0f;
        return proc->embedding;
    }

    /* CNN path — Conv2d layers (original architecture) */
    if (!proc->trainer) return NULL;  /* defensive: partial load / racy destroy */
    uint32_t dims[4] = {1, 1, 1, mel_size};
    nimcp_tensor_t* input = nimcp_tensor_create(dims, 4, NIMCP_DTYPE_F32);
    if (!input) return NULL;

    memcpy(nimcp_tensor_data(input), mel, mel_size * sizeof(float));

    memset(&proc->last_fwd, 0, sizeof(proc->last_fwd));
    nimcp_error_t err = cnn_trainer_forward(proc->trainer, input, &proc->last_fwd);
    nimcp_tensor_destroy(input);

    if (err != NIMCP_SUCCESS) {
        proc->has_fwd_result = false;
        return NULL;
    }

    proc->has_fwd_result = true;
    proc->forward_steps++;
    extract_embedding(proc);
    cortex_cnn_apply_post_forward(proc);
    return proc->embedding;
}

const float* cortex_cnn_forward_speech(cortex_cnn_processor_t* proc,
                                        const float* phonemes, uint32_t size) {
    if (!proc || proc->type != CORTEX_CNN_SPEECH || !phonemes) return NULL;
    if (!proc->trainer) return NULL;  /* defensive: partial load / racy destroy */
    if (size == 0) return NULL;

    /* Pack as [1, 1, 1, size] */
    uint32_t dims[4] = {1, 1, 1, size};
    nimcp_tensor_t* input = nimcp_tensor_create(dims, 4, NIMCP_DTYPE_F32);
    if (!input) return NULL;

    memcpy(nimcp_tensor_data(input), phonemes, size * sizeof(float));

    memset(&proc->last_fwd, 0, sizeof(proc->last_fwd));
    nimcp_error_t err = cnn_trainer_forward(proc->trainer, input, &proc->last_fwd);
    nimcp_tensor_destroy(input);

    if (err != NIMCP_SUCCESS) {
        proc->has_fwd_result = false;
        return NULL;
    }

    proc->has_fwd_result = true;
    proc->forward_steps++;
    extract_embedding(proc);

    /* FNO speech: spectral convolution on phoneme features.
     *
     * W3 audit (Bug #2): FNO mix-in MUST happen before the substrate
     * activation_mod so substrate damage scales BOTH the CNN and FNO
     * contributions. Previously activation_mod ran before the mix-in, which
     * left the FNO component unscaled and decoupled from substrate state. */
    if (proc->fno_speech && proc->embedding) {
        fno_audio_processor_t* fno = (fno_audio_processor_t*)proc->fno_speech;
        uint32_t fno_in = fno->input_size;
        /* Store target BEFORE adding FNO contribution. */
        uint32_t tn = (proc->embedding_dim < 64) ? proc->embedding_dim : 64;
        for (uint32_t i = 0; i < tn; i++) proc->fno_speech_target[i] = proc->embedding[i];
        for (uint32_t i = tn; i < 64; i++) proc->fno_speech_target[i] = 0.0f;
        float fno_emb[64];
        float ramp = (float)proc->fno_distill_calls / 1000.0f;
        if (ramp > 1.0f) ramp = 1.0f;
        float mix = 0.2f * ramp;
        if (size == fno_in) {
            if (fno_audio_forward(fno, phonemes, fno_in, fno_emb) == 0) {
                uint32_t n = (proc->embedding_dim < 64) ? proc->embedding_dim : 64;
                for (uint32_t i = 0; i < n; i++) {
                    proc->fno_speech_emb[i] = fno_emb[i];
                    proc->embedding[i] += mix * fno_emb[i];
                }
                proc->fno_speech_has_target = true;
            }
        } else if (fno_in > 0) {
            /* Resample to FNO input size */
            float* resampled = (float*)nimcp_calloc(fno_in, sizeof(float));
            if (resampled) {
                for (uint32_t i = 0; i < fno_in; i++) {
                    uint32_t src = (i * size) / fno_in;
                    if (src < size) resampled[i] = phonemes[src];
                }
                if (fno_audio_forward(fno, resampled, fno_in, fno_emb) == 0) {
                    uint32_t n = (proc->embedding_dim < 64) ? proc->embedding_dim : 64;
                    for (uint32_t i = 0; i < n; i++) {
                        proc->fno_speech_emb[i] = fno_emb[i];
                        proc->embedding[i] += mix * fno_emb[i];
                    }
                    proc->fno_speech_has_target = true;
                }
                nimcp_free(resampled);
            }
        }
    }

    /* Canonical post-forward sequence: substrate refresh → activation_mod
     * (now covers both CNN + FNO contributions) → thalamic gate → debit. */
    cortex_cnn_apply_post_forward(proc);
    return proc->embedding;
}

/**
 * @brief Haar wavelet transform (in-place, 1 level)
 *
 * Decomposes signal into approximation (low-freq) and detail (high-freq) coefficients.
 * Approximation = (a+b)/sqrt(2), Detail = (a-b)/sqrt(2).
 * First half of output = approximation, second half = detail.
 */
static void haar_wavelet_1level(const float* in, float* out, uint32_t n) {
    uint32_t half = n / 2;
    float inv_sqrt2 = 0.70710678f;
    for (uint32_t i = 0; i < half; i++) {
        float a = in[2 * i];
        float b = (2 * i + 1 < n) ? in[2 * i + 1] : 0.0f;
        out[i] = (a + b) * inv_sqrt2;          /* Approximation */
        out[half + i] = (a - b) * inv_sqrt2;   /* Detail */
    }
    /* Handle odd-length signals */
    if (n & 1) out[half] = in[n - 1] * inv_sqrt2;
}

const float* cortex_cnn_forward_somato(cortex_cnn_processor_t* proc,
                                        const float* segments, uint32_t n_segments) {
    if (!proc || proc->type != CORTEX_CNN_SOMATO || !segments) return NULL;
    if (!proc->trainer) return NULL;  /* defensive: partial load / racy destroy */
    if (n_segments == 0) return NULL;

    /* ================================================================
     * Somatosensory preprocessing pipeline (biologically inspired):
     *
     * 1. Weber-Fechner log scaling: perceived = log(1 + k*stimulus)
     *    Matches biological psychophysics where intensity perception is logarithmic.
     *
     * 2. Haar wavelet decomposition (3 levels):
     *    Level 1: full resolution → body-segment pairs (left/right symmetry)
     *    Level 2: half resolution → limb-level groupings
     *    Level 3: quarter resolution → whole-body regions
     *    Captures BOTH spatial location (where on body) and spatial scale
     *    (fine fingertip vs coarse back touch).
     *
     * 3. Multi-scale pooling: concatenate all wavelet levels into a
     *    rich multi-resolution representation.
     *
     * 4. CNN forward on the enriched multi-scale vector.
     * ================================================================ */

    /* Step 1: Weber-Fechner log scaling */
    float* scaled = (float*)nimcp_malloc(n_segments * sizeof(float));
    if (!scaled) {
        /* Fallback path: raw 1D forward still needs the post-forward sequence
         * so substrate / thalamic / debit stay wired on OOM. */
        const float* fb = cortex_forward_1d(proc, segments, n_segments);
        if (fb) cortex_cnn_apply_post_forward(proc);
        return fb;
    }

    float k = 10.0f;  /* Sensitivity constant — controls log compression steepness */
    for (uint32_t i = 0; i < n_segments; i++) {
        float s = segments[i];
        if (s < 0.0f) s = 0.0f;
        scaled[i] = logf(1.0f + k * s) / logf(1.0f + k);  /* Normalize to ~[0,1] */
    }

    /* Step 2: Haar wavelet decomposition (3 levels) */
    uint32_t n0 = n_segments;
    uint32_t n1 = (n0 + 1) / 2;
    uint32_t n2 = (n1 + 1) / 2;
    uint32_t total_wavelet = n0 + n1 + n2;  /* All levels concatenated */

    float* wavelet_buf = (float*)nimcp_calloc(total_wavelet, sizeof(float));
    if (!wavelet_buf) {
        nimcp_free(scaled);
        const float* fb = cortex_forward_1d(proc, segments, n_segments);
        if (fb) cortex_cnn_apply_post_forward(proc);
        return fb;
    }

    /* Level 0: original (log-scaled) signal */
    memcpy(wavelet_buf, scaled, n0 * sizeof(float));

    /* Level 1: wavelet of level 0 */
    float* level1 = (float*)nimcp_malloc(n0 * sizeof(float));
    if (level1) {
        haar_wavelet_1level(scaled, level1, n0);
        memcpy(wavelet_buf + n0, level1, n1 * sizeof(float));

        /* Level 2: wavelet of level 1 approximation */
        float* level2 = (float*)nimcp_malloc(n1 * sizeof(float));
        if (level2) {
            haar_wavelet_1level(level1, level2, n1);
            memcpy(wavelet_buf + n0 + n1, level2, n2 * sizeof(float));
            nimcp_free(level2);
        }
        nimcp_free(level1);
    }
    nimcp_free(scaled);

    /* Step 3: Forward through CNN with multi-scale wavelet features.
     * cortex_forward_1d is now a raw CNN forward — no substrate/thalamic
     * hooks — so we run the canonical post-forward sequence here, which
     * keeps ordering (substrate → thalamic → debit) identical across all
     * four modalities.
     *
     * W3 audit (Bug #1): previously cortex_forward_1d applied the thalamic
     * gate internally BEFORE this caller applied activation_mod, inverting
     * the design invariant. Moving both hooks out of cortex_forward_1d and
     * into this explicit sequence makes the ordering uniform. */
    const float* result = cortex_forward_1d(proc, wavelet_buf, total_wavelet);
    /* FNO somato: spectral analysis on wavelet-decomposed body schema.
     * Pacinian corpuscles (vibration receptors) tune to specific frequencies
     * — FNO captures the Fourier modes of multi-scale touch input. */
    if (result && proc->fno_somato && proc->embedding) {
        fno_audio_processor_t* fno = (fno_audio_processor_t*)proc->fno_somato;
        uint32_t fno_in = fno->input_size;
        /* Store target BEFORE adding FNO contribution. */
        uint32_t tn = (proc->embedding_dim < 64) ? proc->embedding_dim : 64;
        for (uint32_t i = 0; i < tn; i++) proc->fno_somato_target[i] = proc->embedding[i];
        for (uint32_t i = tn; i < 64; i++) proc->fno_somato_target[i] = 0.0f;
        float fno_emb[64];
        float ramp = (float)proc->fno_distill_calls / 1000.0f;
        if (ramp > 1.0f) ramp = 1.0f;
        float mix = 0.2f * ramp;
        if (total_wavelet == fno_in) {
            if (fno_audio_forward(fno, wavelet_buf, fno_in, fno_emb) == 0) {
                for (uint32_t i = 0; i < tn; i++) {
                    proc->fno_somato_emb[i] = fno_emb[i];
                    proc->embedding[i] += mix * fno_emb[i];
                }
                proc->fno_somato_has_target = true;
            }
        } else if (fno_in > 0) {
            float* resampled = (float*)nimcp_calloc(fno_in, sizeof(float));
            if (resampled) {
                for (uint32_t i = 0; i < fno_in; i++) {
                    uint32_t src = (i * total_wavelet) / fno_in;
                    if (src < total_wavelet) resampled[i] = wavelet_buf[src];
                }
                if (fno_audio_forward(fno, resampled, fno_in, fno_emb) == 0) {
                    for (uint32_t i = 0; i < tn; i++) {
                        proc->fno_somato_emb[i] = fno_emb[i];
                        proc->embedding[i] += mix * fno_emb[i];
                    }
                    proc->fno_somato_has_target = true;
                }
                nimcp_free(resampled);
            }
        }
    }
    nimcp_free(wavelet_buf);

    if (result) {
        cortex_cnn_apply_post_forward(proc);
    }

    return result;
}

/* ========================================================================= */
/* Public API: Backward Pass                                                  */
/* ========================================================================= */

float cortex_cnn_backward(cortex_cnn_processor_t* proc,
                           const char* label, uint32_t num_outputs) {
    if (!proc || !proc->trainer || !proc->has_fwd_result || !label) return -1.0f;
    /* Defensive: last_fwd.output / activations may be NULL if a concurrent
     * forward call zeroed last_fwd between the has_fwd_result check above and
     * this backward (e.g. UTM step interleaving with modality-specific block).
     * cnn_trainer_backward asserts on these — guard up-front to avoid the
     * NIMCP_THROW path which is heavier. */
    if (!proc->last_fwd.output || !proc->last_fwd.activations) {
        proc->has_fwd_result = false;
        return -1.0f;
    }
    if (num_outputs == 0) num_outputs = proc->embedding_dim;

    uint32_t label_idx = cortex_get_or_create_label(proc, label);

    /* Build one-hot target */
    uint32_t target_dim = num_outputs;
    uint32_t target_dims[1] = {target_dim};
    nimcp_tensor_t* target = nimcp_tensor_create(target_dims, 1, NIMCP_DTYPE_F32);
    if (!target) return -1.0f;

    float* target_data = (float*)nimcp_tensor_data(target);
    memset(target_data, 0, target_dim * sizeof(float));
    if (label_idx < target_dim) {
        target_data[label_idx] = 1.0f;
    }

    /* Backward + step */
    nimcp_error_t err = cnn_trainer_backward(proc->trainer, target, &proc->last_fwd);
    nimcp_tensor_destroy(target);

    if (err != NIMCP_SUCCESS) {
        static uint32_t _bwd_fail_count[4] = {0};
        uint32_t idx = (proc->type < 4) ? proc->type : 0;
        _bwd_fail_count[idx]++;
        if ((_bwd_fail_count[idx] & (_bwd_fail_count[idx] - 1)) == 0) {  /* log at powers of 2 */
            static const char* names[] = {"Visual", "Audio", "Speech", "Somato"};
            NIMCP_LOGGING_WARN("cortex_cnn_backward(%s): cnn_trainer_backward failed "
                               "(err=%d, fwd_output=%p, fwd_activations=%p, target_dim=%u, "
                               "label='%s', fails=%u)",
                               names[idx], err,
                               proc->last_fwd.output, proc->last_fwd.activations,
                               target_dim, label ? label : "(null)",
                               _bwd_fail_count[idx]);
        }
        return -1.0f;
    }

    /* Substrate plasticity modulation (Phase 3): scale optimizer LR by
     * dend_effects.plasticity_mod for this step only, then restore. When
     * the substrate knob is off (or no substrate attached, or the flag is
     * disabled), lr_scale stays at 1.0 and this is a no-op. */
    nimcp_optimizer_context_t* opt = cnn_trainer_get_optimizer(proc->trainer);
    float original_lr = 0.0f;
    float lr_scale    = 1.0f;
    if (opt && proc->substrate
        && g_cnn_substrate_enabled != 0.0f
        && g_cnn_substrate_plasticity_mod_on != 0.0f) {
        /* Reads cached effects — they are populated during the forward step.
         * If cache is stale (forward hasn't run yet) the values are zero, in
         * which case we skip scaling to avoid multiplying LR by 0. */
        float pmod = proc->cached_dend_effects.plasticity_mod;
        if (pmod > 0.0f && pmod != 1.0f) {
            lr_scale    = pmod;
            original_lr = nimcp_optimizer_get_lr(opt);
            nimcp_optimizer_set_lr(opt, original_lr * lr_scale);
        }
    }

    cnn_trainer_step(proc->trainer);
    proc->backward_steps++;

    /* Restore LR after the step. */
    if (lr_scale != 1.0f && opt) {
        nimcp_optimizer_set_lr(opt, original_lr);
    }

    /* Phase 3: Self-distillation training for cortex FNOs.
     * For visual/speech/somato (where FNO supplements CNN), train FNO to
     * predict the pre-FNO CNN embedding. Loss = MSE(fno_emb, target).
     * Audio's FNO IS the embedding (no pre-FNO target) — skip. */
    {
        const float fno_lr = 0.001f;
        struct { void* fno; float* target; float* emb; bool* has; uint32_t metric_slot; } feeds[3] = {
            { proc->fno_visual, proc->fno_visual_target, proc->fno_visual_emb, &proc->fno_visual_has_target, 0 },
            { proc->fno_speech, proc->fno_speech_target, proc->fno_speech_emb, &proc->fno_speech_has_target, 1 },
            { proc->fno_somato, proc->fno_somato_target, proc->fno_somato_emb, &proc->fno_somato_has_target, 2 },
        };
        bool any_trained = false;
        for (int fi = 0; fi < 3; fi++) {
            if (!feeds[fi].fno || !*feeds[fi].has) continue;
            fno_audio_processor_t* fno = (fno_audio_processor_t*)feeds[fi].fno;
            uint32_t ed = (fno->embed_dim < 64) ? fno->embed_dim : 64;
            float loss = 0.0f;
            float grad[64];
            for (uint32_t i = 0; i < ed; i++) {
                float diff = feeds[fi].emb[i] - feeds[fi].target[i];
                loss += diff * diff;
                grad[i] = 2.0f * diff / (float)ed;
            }
            loss /= (float)ed;
            fno_audio_zero_grad(fno);
            fno_audio_backward(fno, grad, NULL);
            fno_audio_step(fno, fno_lr);
            fno_audio_record_loss(fno, loss);
            *feeds[fi].has = false;  /* one training step per backward call */
            any_trained = true;
        }
        if (any_trained) proc->fno_distill_calls++;
    }

    /* Substrate backward debit — plasticity updates are the expensive metabolic
     * event. Counts 1 unit per backward call (one gradient step). */
    if (proc->substrate && g_cnn_substrate_enabled != 0.0f) {
        substrate_debit_activity(proc->substrate, proc->region_id,
                                 0 /* spikes */, 1 /* plasticity */);
    }

    /* Compute loss approximation from last forward output vs one-hot */
    float loss = 0.0f;
    if (proc->last_fwd.output) {
        const float* out = (const float*)nimcp_tensor_data_const(proc->last_fwd.output);
        size_t numel = nimcp_tensor_numel(proc->last_fwd.output);
        uint32_t n = (target_dim < (uint32_t)numel) ? target_dim : (uint32_t)numel;
        for (uint32_t i = 0; i < n; i++) {
            float tgt = (i == label_idx) ? 1.0f : 0.0f;
            float diff = out[i] - tgt;
            loss += diff * diff;
        }
        if (n > 0) loss /= (float)n;
    }

    proc->last_loss = loss;
    if (proc->ema_loss < 0.0f) {
        proc->ema_loss = loss;
    } else {
        proc->ema_loss = 0.99f * proc->ema_loss + 0.01f * loss;
    }

    /* W5 KG anomaly emit — NaN/Inf or runaway loss indicates gradient
     * explosion / feature collapse. Single hook per cnn_trainer_step. */
    if (s_net_cnn_kg_brain && (!isfinite(loss) || loss > 1.0e6f)) {
        uint64_t ts_us = (uint64_t)time(NULL) * 1000000ULL;
        net_cnn_kg_emit_event(s_net_cnn_kg_brain,
            isfinite(loss) ? "gradient_clip" : "feature_collapse",
            isfinite(loss) ? loss : 0.0f, ts_us);
    }

    return loss;
}

/* ========================================================================= */
/* Public API: Cross-Modal Attention Fusion                                   */
/* ========================================================================= */

uint32_t cortex_cnn_fuse(cortex_cnn_processor_t* procs[], uint32_t count,
                          float* fused_out, uint32_t max_dim) {
    if (!procs || !fused_out || max_dim == 0 || count == 0) return 0;

    /* Count active processors and compute total embedding dim */
    uint32_t active_count = 0;
    uint32_t total_dim = 0;
    float confidences[CORTEX_CNN_COUNT] = {0};
    uint32_t active_indices[CORTEX_CNN_COUNT] = {0};

    for (uint32_t i = 0; i < count && i < CORTEX_CNN_COUNT; i++) {
        if (procs[i] && procs[i]->has_fwd_result && procs[i]->embedding) {
            confidences[active_count] = procs[i]->confidence;
            active_indices[active_count] = i;
            total_dim += procs[i]->embedding_dim;
            active_count++;
        }
    }

    if (active_count == 0) return 0;
    if (total_dim > max_dim) total_dim = max_dim;

    /* Compute attention weights via softmax(confidence / temperature) */
    float attention[CORTEX_CNN_COUNT] = {0};
    float max_conf = -1e30f;
    for (uint32_t i = 0; i < active_count; i++) {
        float c = confidences[i] / CORTEX_CNN_ATTENTION_TEMP;
        if (c > max_conf) max_conf = c;
    }
    float sum_exp = 0.0f;
    for (uint32_t i = 0; i < active_count; i++) {
        attention[i] = expf(confidences[i] / CORTEX_CNN_ATTENTION_TEMP - max_conf);
        sum_exp += attention[i];
    }
    if (sum_exp > 0.0f) {
        for (uint32_t i = 0; i < active_count; i++) {
            attention[i] /= sum_exp;
        }
    }

    /* Concatenate attention-weighted embeddings */
    uint32_t offset = 0;
    for (uint32_t i = 0; i < active_count && offset < max_dim; i++) {
        uint32_t idx = active_indices[i];
        cortex_cnn_processor_t* p = procs[idx];
        uint32_t dim = p->embedding_dim;
        if (offset + dim > max_dim) dim = max_dim - offset;

        float w = attention[i];
        for (uint32_t j = 0; j < dim; j++) {
            fused_out[offset + j] = w * p->embedding[j];
        }
        offset += dim;
    }

    return offset;
}

/* ========================================================================= */
/* Public API: Metrics                                                        */
/* ========================================================================= */

void cortex_cnn_set_fno_audio (cortex_cnn_processor_t* proc, void* fno) { if (proc) proc->fno_audio  = fno; }
void cortex_cnn_set_fno_visual(cortex_cnn_processor_t* proc, void* fno) { if (proc) proc->fno_visual = fno; }
void cortex_cnn_set_fno_speech(cortex_cnn_processor_t* proc, void* fno) { if (proc) proc->fno_speech = fno; }
void cortex_cnn_set_fno_somato(cortex_cnn_processor_t* proc, void* fno) { if (proc) proc->fno_somato = fno; }
void* cortex_cnn_get_fno_audio (const cortex_cnn_processor_t* proc) { return proc ? proc->fno_audio  : NULL; }
void* cortex_cnn_get_fno_visual(const cortex_cnn_processor_t* proc) { return proc ? proc->fno_visual : NULL; }
void* cortex_cnn_get_fno_speech(const cortex_cnn_processor_t* proc) { return proc ? proc->fno_speech : NULL; }
void* cortex_cnn_get_fno_somato(const cortex_cnn_processor_t* proc) { return proc ? proc->fno_somato : NULL; }

int cortex_cnn_get_metrics(const cortex_cnn_processor_t* proc, cortex_cnn_metrics_t* out) {
    if (!proc || !out) return -1;

    out->type = proc->type;
    out->last_loss = proc->last_loss;
    out->ema_loss = (proc->ema_loss >= 0.0f) ? proc->ema_loss : 0.0f;
    out->forward_steps = proc->forward_steps;
    out->backward_steps = proc->backward_steps;
    out->confidence = proc->confidence;
    out->embedding_dim = proc->embedding_dim;
    out->num_params = proc->num_params;

    /* Compute embedding L2 norm */
    float norm = 0.0f;
    if (proc->embedding) {
        for (uint32_t i = 0; i < proc->embedding_dim; i++) {
            norm += proc->embedding[i] * proc->embedding[i];
        }
    }
    out->embedding_norm = sqrtf(norm);

    return 0;
}

const float* cortex_cnn_get_embedding(const cortex_cnn_processor_t* proc, uint32_t* dim) {
    if (!proc || !proc->has_fwd_result) {
        if (dim) *dim = 0;
        return NULL;
    }
    if (dim) *dim = proc->embedding_dim;
    return proc->embedding;
}

cortex_cnn_type_t cortex_cnn_get_type(const cortex_cnn_processor_t* proc) {
    return proc ? proc->type : CORTEX_CNN_VISUAL;
}

const char* cortex_cnn_type_name(cortex_cnn_type_t type) {
    if (type >= 0 && type < CORTEX_CNN_COUNT) return cortex_type_names[type];
    return "Unknown";
}

/* ========================================================================= */
/* Thalamic Router Adapter                                                    */
/* ========================================================================= */

nimcp_error_t cortex_cnn_attach_thalamic_router(cortex_cnn_processor_t* proc,
                                                struct thalamic_router* router) {
    if (!proc) return NIMCP_ERROR_NULL_POINTER;

    /* Single-attach: destroy any existing channel before creating a new
     * one (or leaving detached when router==NULL). */
    if (proc->thalamic_channel) {
        thalamic_channel_destroy((thalamic_channel_t*)proc->thalamic_channel);
        proc->thalamic_channel = NULL;
    }

    if (router) {
        /* Use the processor's cortex type as the router source_id so the
         * router can learn per-cortex routing (Visual=0, Audio=1, Speech=2,
         * Somato=3). */
        thalamic_channel_t* ch = thalamic_channel_create(
            (thalamic_router_t*)router, (uint32_t)proc->type);
        if (ch) {
            /* Pre-register destination 0 so the first forward step has a
             * valid gate slot. Non-fatal on capacity exceed — the channel
             * still works, get_gate falls back to 1.0. */
            thalamic_channel_add_destination(ch, 0);
            proc->thalamic_channel = (struct thalamic_channel_s*)ch;
            NIMCP_LOGGING_INFO(
                "cortex_cnn[%s]: attached thalamic router (source_id=%u)",
                cortex_type_names[proc->type], (uint32_t)proc->type);
        } else {
            NIMCP_LOGGING_WARN(
                "cortex_cnn[%s]: thalamic_channel_create failed",
                cortex_type_names[proc->type]);
        }
    }
    return NIMCP_SUCCESS;
}

/* ========================================================================= */
/* Thalamic Adapter Tunables (process-wide)                                   */
/* ========================================================================= */

void cortex_cnn_tune_set_thalamic_enabled(float v) {
    g_cnn_thalamic_enabled = (v != 0.0f) ? 1.0f : 0.0f;
}
void cortex_cnn_tune_set_thalamic_featuremap_gain_on(float v) {
    g_cnn_thalamic_featuremap_gain_on = (v != 0.0f) ? 1.0f : 0.0f;
}
void cortex_cnn_tune_set_thalamic_burst_dropout_reduce_on(float v) {
    g_cnn_thalamic_burst_dropout_reduce_on = (v != 0.0f) ? 1.0f : 0.0f;
}

float cortex_cnn_tune_get_thalamic_enabled(void) {
    return g_cnn_thalamic_enabled;
}
float cortex_cnn_tune_get_thalamic_featuremap_gain_on(void) {
    return g_cnn_thalamic_featuremap_gain_on;
}
float cortex_cnn_tune_get_thalamic_burst_dropout_reduce_on(void) {
    return g_cnn_thalamic_burst_dropout_reduce_on;
}

/* Test-only accessor: exposes the internal thalamic channel so white-box
 * tests can verify attach/detach behaviour without reaching into the
 * opaque processor struct. Not part of the public API — declared in the
 * test file's extern "C" block. */
thalamic_channel_t* cortex_cnn_test_get_thalamic_channel(
    const cortex_cnn_processor_t* proc) {
    return proc ? (thalamic_channel_t*)proc->thalamic_channel : NULL;
}

/* Test-only accessors: expose the cached substrate effects so white-box
 * tests can verify that cortex_cnn_attach_substrate eagerly populates the
 * cache (F1 CRITICAL mirror). Not part of the public API — declared in
 * the test file's extern "C" block. */
const dendrite_substrate_effects_t* cortex_cnn_test_get_cached_dend_effects(
    const cortex_cnn_processor_t* proc) {
    return proc ? &proc->cached_dend_effects : NULL;
}

const axon_substrate_effects_t* cortex_cnn_test_get_cached_axon_effects(
    const cortex_cnn_processor_t* proc) {
    return proc ? &proc->cached_axon_effects : NULL;
}

/* ========================================================================= */
/* UTM Adapter                                                                */
/* ========================================================================= */

typedef struct {
    cortex_cnn_processor_t* proc;  /* NOT owned */
    uint32_t input_dim;
    uint32_t output_dim;
} cortex_cnn_adapter_ctx_t;

static int cortex_adapter_forward(void* ctx, const float* input, uint32_t input_dim,
                                   float* output, uint32_t output_dim) {
    cortex_cnn_adapter_ctx_t* a = (cortex_cnn_adapter_ctx_t*)ctx;
    if (!a || !a->proc || !input || !output) return -1;

    /* Skip UTM flat-1D forward for Visual and Somato — their CNN architectures
     * require specific input formats (3-channel 2D for visual, wavelet segments
     * for somato). Running flat 1D input through them corrupts batch normalization
     * running stats, causing subsequent modality-specific forwards to produce NaN.
     * These cortexes are trained via modality-specific forward+backward in the
     * UTM block of brain_learn_vector instead. */
    if (a->proc->type == CORTEX_CNN_VISUAL || a->proc->type == CORTEX_CNN_SOMATO) {
        /* Return last embedding if available, else zeros */
        if (a->proc->embedding && a->proc->has_fwd_result) {
            uint32_t copy_dim = (output_dim < a->proc->embedding_dim) ?
                                 output_dim : a->proc->embedding_dim;
            memcpy(output, a->proc->embedding, copy_dim * sizeof(float));
            if (output_dim > copy_dim)
                memset(output + copy_dim, 0, (output_dim - copy_dim) * sizeof(float));
        } else {
            memset(output, 0, output_dim * sizeof(float));
        }
        return 0;
    }

    /* Use the generic 1D forward — works for audio/speech when data is
     * already pre-processed to flat float representation.
     *
     * W4 audit (Bug #3): cortex_forward_1d is now a raw CNN forward and no
     * longer runs substrate / thalamic / debit hooks. The UTM adapter path
     * must apply the canonical post-forward sequence explicitly so audio /
     * speech routed through UTM see the same substrate + thalamic coupling
     * as the public forward_audio / forward_speech APIs. */
    const float* emb = cortex_forward_1d(a->proc, input, input_dim);
    if (!emb) return -1;

    cortex_cnn_apply_post_forward(a->proc);

    uint32_t copy_dim = (output_dim < a->proc->embedding_dim) ?
                         output_dim : a->proc->embedding_dim;
    memcpy(output, a->proc->embedding, copy_dim * sizeof(float));
    if (output_dim > copy_dim) {
        memset(output + copy_dim, 0, (output_dim - copy_dim) * sizeof(float));
    }
    return 0;
}

static int cortex_adapter_backward(void* ctx, const float* dl_doutput, uint32_t output_dim,
                                    float* dl_dinput, uint32_t input_dim) {
    cortex_cnn_adapter_ctx_t* a = (cortex_cnn_adapter_ctx_t*)ctx;
    if (!a || !a->proc || !a->proc->trainer || !dl_doutput) return -1;
    if (!a->proc->has_fwd_result) return -1;
    /* Defensive: cnn_trainer_backward_with_gradient asserts on these — guard
     * up-front. Same race window as cortex_cnn_backward — see comment there. */
    if (!a->proc->last_fwd.output || !a->proc->last_fwd.activations) {
        a->proc->has_fwd_result = false;
        return -1;
    }

    /* Pass external gradient to CNN backward */
    uint32_t dims[1] = {output_dim};
    nimcp_tensor_t* grad_t = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    if (!grad_t) return -1;

    memcpy(nimcp_tensor_data(grad_t), dl_doutput, output_dim * sizeof(float));

    nimcp_error_t err = cnn_trainer_backward_with_gradient(
        a->proc->trainer, grad_t, &a->proc->last_fwd);

    if (err == NIMCP_SUCCESS) {
        /* Substrate plasticity modulation (Phase 3) — same pattern as
         * cortex_cnn_backward. No-op when substrate is NULL or knobs off. */
        nimcp_optimizer_context_t* opt = cnn_trainer_get_optimizer(a->proc->trainer);
        float original_lr = 0.0f;
        float lr_scale    = 1.0f;
        if (opt && a->proc->substrate
            && g_cnn_substrate_enabled != 0.0f
            && g_cnn_substrate_plasticity_mod_on != 0.0f) {
            float pmod = a->proc->cached_dend_effects.plasticity_mod;
            if (pmod > 0.0f && pmod != 1.0f) {
                lr_scale    = pmod;
                original_lr = nimcp_optimizer_get_lr(opt);
                nimcp_optimizer_set_lr(opt, original_lr * lr_scale);
            }
        }

        cnn_trainer_step(a->proc->trainer);
        a->proc->backward_steps++;

        if (lr_scale != 1.0f && opt) {
            nimcp_optimizer_set_lr(opt, original_lr);
        }

        if (a->proc->substrate && g_cnn_substrate_enabled != 0.0f) {
            substrate_debit_activity(a->proc->substrate, a->proc->region_id,
                                     0 /* spikes */, 1 /* plasticity */);
        }
    }

    /* Copy input gradients if available */
    if (dl_dinput) {
        const nimcp_tensor_t* in_grad = cnn_trainer_get_input_grad(a->proc->trainer);
        if (in_grad) {
            const float* grad_data = (const float*)nimcp_tensor_data_const(in_grad);
            size_t grad_numel = nimcp_tensor_numel(in_grad);
            uint32_t copy = (input_dim < (uint32_t)grad_numel) ?
                             input_dim : (uint32_t)grad_numel;
            memcpy(dl_dinput, grad_data, copy * sizeof(float));
            if (input_dim > copy) {
                memset(dl_dinput + copy, 0, (input_dim - copy) * sizeof(float));
            }
        } else {
            memset(dl_dinput, 0, input_dim * sizeof(float));
        }
    }

    nimcp_tensor_destroy(grad_t);
    return (err == NIMCP_SUCCESS) ? 0 : -1;
}

static int cortex_adapter_get_param_groups(void* ctx,
                                            nimcp_utm_param_group_t** groups,
                                            uint32_t* num_groups) {
    (void)ctx;
    /* Cortex CNNs manage their own weights via internal tensors — same pattern as CNN adapter */
    if (groups) *groups = NULL;
    if (num_groups) *num_groups = 0;
    return 0;
}

static int cortex_adapter_zero_grad(void* ctx) {
    cortex_cnn_adapter_ctx_t* a = (cortex_cnn_adapter_ctx_t*)ctx;
    if (!a || !a->proc || !a->proc->trainer) return -1;
    cnn_trainer_zero_grad(a->proc->trainer);
    return 0;
}

static uint32_t cortex_adapter_get_output_dim(void* ctx) {
    cortex_cnn_adapter_ctx_t* a = (cortex_cnn_adapter_ctx_t*)ctx;
    return a ? a->output_dim : 0;
}

static uint32_t cortex_adapter_get_input_dim(void* ctx) {
    cortex_cnn_adapter_ctx_t* a = (cortex_cnn_adapter_ctx_t*)ctx;
    return a ? a->input_dim : 0;
}

static float cortex_adapter_auxiliary_loss(void* ctx) {
    cortex_cnn_adapter_ctx_t* a = (cortex_cnn_adapter_ctx_t*)ctx;
    if (!a || !a->proc) return 0.0f;
    /* Use tracked loss as auxiliary loss signal */
    return (a->proc->last_loss >= 0.0f) ? 0.001f * a->proc->last_loss : 0.0f;
}

static void cortex_adapter_destroy(void* ctx) {
    cortex_cnn_adapter_ctx_t* a = (cortex_cnn_adapter_ctx_t*)ctx;
    if (!a) return;
    /* Don't free proc — not owned */
    nimcp_free(a);
}

static const char* cortex_adapter_names[CORTEX_CNN_COUNT] = {
    "CortexCNN_Visual", "CortexCNN_Audio", "CortexCNN_Speech", "CortexCNN_Somato"
};

/* Per-type vtable instances */
static const nimcp_trainable_network_ops_t cortex_cnn_ops[CORTEX_CNN_COUNT] = {
    [CORTEX_CNN_VISUAL] = {
        .name = "CortexCNN_Visual",
        .type = NIMCP_TRAINABLE_CUSTOM,
        .forward = cortex_adapter_forward,
        .backward = cortex_adapter_backward,
        .get_param_groups = cortex_adapter_get_param_groups,
        .zero_grad = cortex_adapter_zero_grad,
        .get_output_dim = cortex_adapter_get_output_dim,
        .get_input_dim = cortex_adapter_get_input_dim,
        .compute_auxiliary_loss = cortex_adapter_auxiliary_loss,
        .destroy = cortex_adapter_destroy,
        .sync_params = NULL,
    },
    [CORTEX_CNN_AUDIO] = {
        .name = "CortexCNN_Audio",
        .type = NIMCP_TRAINABLE_CUSTOM,
        .forward = cortex_adapter_forward,
        .backward = cortex_adapter_backward,
        .get_param_groups = cortex_adapter_get_param_groups,
        .zero_grad = cortex_adapter_zero_grad,
        .get_output_dim = cortex_adapter_get_output_dim,
        .get_input_dim = cortex_adapter_get_input_dim,
        .compute_auxiliary_loss = cortex_adapter_auxiliary_loss,
        .destroy = cortex_adapter_destroy,
        .sync_params = NULL,
    },
    [CORTEX_CNN_SPEECH] = {
        .name = "CortexCNN_Speech",
        .type = NIMCP_TRAINABLE_CUSTOM,
        .forward = cortex_adapter_forward,
        .backward = cortex_adapter_backward,
        .get_param_groups = cortex_adapter_get_param_groups,
        .zero_grad = cortex_adapter_zero_grad,
        .get_output_dim = cortex_adapter_get_output_dim,
        .get_input_dim = cortex_adapter_get_input_dim,
        .compute_auxiliary_loss = cortex_adapter_auxiliary_loss,
        .destroy = cortex_adapter_destroy,
        .sync_params = NULL,
    },
    [CORTEX_CNN_SOMATO] = {
        .name = "CortexCNN_Somato",
        .type = NIMCP_TRAINABLE_CUSTOM,
        .forward = cortex_adapter_forward,
        .backward = cortex_adapter_backward,
        .get_param_groups = cortex_adapter_get_param_groups,
        .zero_grad = cortex_adapter_zero_grad,
        .get_output_dim = cortex_adapter_get_output_dim,
        .get_input_dim = cortex_adapter_get_input_dim,
        .compute_auxiliary_loss = cortex_adapter_auxiliary_loss,
        .destroy = cortex_adapter_destroy,
        .sync_params = NULL,
    },
};

int cortex_cnn_utm_adapter_create(cortex_cnn_processor_t* proc,
                                   const nimcp_trainable_network_ops_t** ops,
                                   void** ctx) {
    if (!proc || !ops || !ctx) return -1;

    cortex_cnn_adapter_ctx_t* a = (cortex_cnn_adapter_ctx_t*)nimcp_calloc(
        1, sizeof(cortex_cnn_adapter_ctx_t));
    if (!a) return -1;

    a->proc = proc;
    a->output_dim = proc->embedding_dim;

    /* Input dim depends on modality */
    switch (proc->type) {
        case CORTEX_CNN_VISUAL:  a->input_dim = 64 * 64 * 3; break;
        case CORTEX_CNN_AUDIO:   a->input_dim = 128; break;
        case CORTEX_CNN_SPEECH:  a->input_dim = 64; break;
        case CORTEX_CNN_SOMATO:  a->input_dim = 45; break;
        default: a->input_dim = 64; break;
    }

    *ops = &cortex_cnn_ops[proc->type];
    *ctx = a;

    NIMCP_LOGGING_INFO("Created UTM adapter for %s cortex CNN (in=%u, out=%u)",
                      cortex_type_names[proc->type], a->input_dim, a->output_dim);
    return 0;
}

/* ========================================================================= */
/* Checkpoint Save/Load                                                       */
/* ========================================================================= */

int cortex_cnn_save(const cortex_cnn_processor_t* proc, const char* path) {
    if (!proc || !proc->trainer || !path) return -1;
    return cnn_trainer_save(proc->trainer, path);
}

int cortex_cnn_load(cortex_cnn_processor_t* proc, const char* path) {
    if (!proc || !proc->trainer || !path) return -1;
    return cnn_trainer_load_weights(proc->trainer, path);
}
