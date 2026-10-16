/**
 * @file nimcp_vae_thalamic_bridge.c
 * @brief VAE-Thalamic Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * Implements attention-gated VAE encoding via thalamic relay.
 *
 * BIO_MODULE: 0x1F1F
 */

#include "cognitive/vae/bridges/nimcp_vae_thalamic_bridge.h"
#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/vae/nimcp_vae_latent.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "utils/math/nimcp_math_helpers.h"

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define VAE_THAL_MODULE_ID           BIO_MODULE_VAE_THALAMIC_BRIDGE
#define VAE_THAL_DEFAULT_LATENT_DIM  32
#define VAE_THAL_EMA_ALPHA           0.9f
#define VAE_THAL_BURST_DURATION_MS   50.0f
#define VAE_THAL_T_CHANNEL_RECOVERY  100.0f  /* ms */

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_timestamp_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Sigmoid function for gating
 */
static float sigmoid(float x, float slope)
{
    return 1.0f / (1.0f + expf(-slope * x));
}

/**
 * @brief Get latent dimension range for a nucleus
 */
static void get_nucleus_latent_range(const vae_thalamic_bridge_t* bridge,
                                      vae_thalamic_nucleus_t nucleus,
                                      uint32_t* start, uint32_t* count)
{
    if (!bridge || nucleus >= VAE_THAL_NUC_COUNT) {
        if (start) *start = 0;
        if (count) *count = 0;
        return;
    }

    const vae_thalamic_nucleus_config_t* nc = &bridge->config.nuclei[nucleus];
    if (start) *start = nc->latent_dim_start;
    if (count) *count = nc->latent_dim_count;
}

/**
 * @brief Compute gating value based on attention and TRN
 */
static float compute_gate(const vae_thalamic_bridge_t* bridge,
                          vae_thalamic_nucleus_t nucleus,
                          float attention)
{
    if (!bridge) return 1.0f;

    const vae_thalamic_nucleus_state_t* ns = &bridge->thalamic_state.nuclei[nucleus];
    const vae_thalamic_trn_config_t* trn_cfg = &bridge->config.trn;

    /* Base gate from attention */
    float gate = attention;

    /* Apply TRN inhibition if enabled */
    if (trn_cfg->enable_trn_gating) {
        float trn_effect = ns->trn_inhibition_level;
        if (trn_effect > trn_cfg->trn_threshold) {
            gate *= sigmoid(trn_cfg->trn_threshold - trn_effect,
                           trn_cfg->gating_slope);
        }
    }

    /* Mode-dependent modulation */
    if (ns->mode == VAE_THAL_MODE_INHIBITED) {
        gate *= 0.1f; /* Heavily suppress */
    } else if (ns->mode == VAE_THAL_MODE_BURST) {
        gate *= 1.5f; /* Amplify during burst */
    }

    return nimcp_clampf(gate, 0.0f, 1.0f);
}

/**
 * @brief Update T-channel state for burst mode
 */
static void update_t_channel(vae_thalamic_nucleus_state_t* ns, float dt_ms)
{
    if (!ns) return;

    if (ns->is_bursting) {
        /* During burst, T-channels inactivate */
        ns->t_channel_state *= expf(-dt_ms / 20.0f);
        if (ns->t_channel_state < 0.1f) {
            ns->is_bursting = false;
            ns->mode = VAE_THAL_MODE_TONIC;
        }
    } else {
        /* Recovery when hyperpolarized */
        ns->t_channel_state += (1.0f - ns->t_channel_state) *
                              (1.0f - expf(-dt_ms / VAE_THAL_T_CHANNEL_RECOVERY));
    }
}

/**
 * @brief Update nucleus state
 */
static void update_nucleus_state(vae_thalamic_bridge_t* bridge,
                                  vae_thalamic_nucleus_t nucleus)
{
    if (!bridge || nucleus >= VAE_THAL_NUC_COUNT) return;

    vae_thalamic_nucleus_state_t* ns = &bridge->thalamic_state.nuclei[nucleus];
    const vae_thalamic_nucleus_config_t* nc = &bridge->config.nuclei[nucleus];

    /* Update T-channel */
    update_t_channel(ns, bridge->config.update_interval_ms);

    /* Update attention with decay */
    ns->current_attention *= (1.0f - bridge->config.attention.attention_decay);
    ns->current_attention = nimcp_clampf(ns->current_attention,
                                   bridge->config.attention.attention_min,
                                   bridge->config.attention.attention_max);

    /* Check for burst trigger */
    if (ns->t_channel_state > nc->burst_threshold && !ns->is_bursting) {
        /* Could transition to burst if inhibition releases */
        if (ns->trn_inhibition_level < 0.3f) {
            ns->is_bursting = true;
            ns->mode = VAE_THAL_MODE_BURST;
            ns->burst_count++;
        }
    }

    ns->last_spike_us = get_timestamp_us();
}

/**
 * @brief Update bridge statistics
 */
static void update_stats(vae_thalamic_bridge_t* bridge, vae_thalamic_nucleus_t nucleus,
                         bool was_burst, bool was_gated)
{
    if (!bridge) return;

    bridge->stats.total_relays++;
    if (was_burst) bridge->stats.total_bursts++;
    if (was_gated) bridge->stats.total_gatings++;

    /* Update per-nucleus activity */
    if (nucleus < VAE_THAL_NUC_COUNT) {
        bridge->stats.per_nucleus_activity[nucleus] =
            VAE_THAL_EMA_ALPHA * bridge->stats.per_nucleus_activity[nucleus] +
            (1.0f - VAE_THAL_EMA_ALPHA) * 1.0f;
    }

    /* Update attention average */
    bridge->stats.avg_attention_level =
        VAE_THAL_EMA_ALPHA * bridge->stats.avg_attention_level +
        (1.0f - VAE_THAL_EMA_ALPHA) * bridge->global_attention;

    /* Update mode time percentages */
    bool any_burst = false;
    for (int n = 0; n < VAE_THAL_NUC_COUNT; n++) {
        if (bridge->thalamic_state.nuclei[n].mode == VAE_THAL_MODE_BURST) {
            any_burst = true;
            break;
        }
    }

    float time_weight = 0.01f;
    if (any_burst) {
        bridge->stats.time_in_burst_mode_pct =
            (1.0f - time_weight) * bridge->stats.time_in_burst_mode_pct +
            time_weight * 100.0f;
        bridge->stats.time_in_tonic_mode_pct =
            (1.0f - time_weight) * bridge->stats.time_in_tonic_mode_pct;
    } else {
        bridge->stats.time_in_tonic_mode_pct =
            (1.0f - time_weight) * bridge->stats.time_in_tonic_mode_pct +
            time_weight * 100.0f;
        bridge->stats.time_in_burst_mode_pct =
            (1.0f - time_weight) * bridge->stats.time_in_burst_mode_pct;
    }

    bridge->stats.last_relay_us = get_timestamp_us();
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_thalamic_bridge_default_config(vae_thalamic_bridge_config_t* config)
{
    if (!config) return NIMCP_ERROR_VAE_THAL_NULL;

    memset(config, 0, sizeof(*config));

    /* Initialize each nucleus */
    for (int n = 0; n < VAE_THAL_NUC_COUNT; n++) {
        config->nuclei[n].nucleus = (vae_thalamic_nucleus_t)n;
        config->nuclei[n].num_channels = 32;
        config->nuclei[n].order = VAE_THAL_FIRST_ORDER;
        config->nuclei[n].burst_threshold = VAE_THALAMIC_BURST_THRESHOLD;
        config->nuclei[n].attention_weight = 1.0f;
        config->nuclei[n].trn_inhibition = VAE_THALAMIC_TRN_STRENGTH;
        config->nuclei[n].enable_adaptation = true;
        config->nuclei[n].latent_dim_start = n * 4;
        config->nuclei[n].latent_dim_count = 4;
    }

    /* Special configurations for specific nuclei */
    config->nuclei[VAE_THAL_NUC_LGN].order = VAE_THAL_FIRST_ORDER;
    config->nuclei[VAE_THAL_NUC_MGN].order = VAE_THAL_FIRST_ORDER;
    config->nuclei[VAE_THAL_NUC_PULVINAR].order = VAE_THAL_HIGHER_ORDER;
    config->nuclei[VAE_THAL_NUC_MD].order = VAE_THAL_HIGHER_ORDER;

    /* Enable primary sensory nuclei by default */
    config->active_nuclei_mask = (1 << VAE_THAL_NUC_LGN) |
                                  (1 << VAE_THAL_NUC_MGN) |
                                  (1 << VAE_THAL_NUC_VPL) |
                                  (1 << VAE_THAL_NUC_PULVINAR) |
                                  (1 << VAE_THAL_NUC_TRN);

    /* Attention config */
    config->attention.enable_attention_gating = true;
    config->attention.attention_min = 0.1f;
    config->attention.attention_max = 1.0f;
    config->attention.attention_decay = 0.01f;
    config->attention.pulvinar_modulation = true;

    /* Burst config */
    config->burst.burst_triggers_exploration = true;
    config->burst.burst_temperature_scale = 2.0f;
    config->burst.burst_spike_count = 5;
    config->burst.t_channel_recovery_ms = VAE_THAL_T_CHANNEL_RECOVERY;

    /* TRN config */
    config->trn.enable_trn_gating = true;
    config->trn.trn_threshold = 0.5f;
    config->trn.gating_slope = 5.0f;
    config->trn.lateral_inhibition = true;

    /* Routing */
    config->enable_multimodal_routing = true;
    config->enable_cortical_feedback = true;
    config->feedback_strength = 0.3f;

    /* Timing */
    config->relay_latency_ms = 5.0f;
    config->update_interval_ms = 10.0f;

    config->enable_logging = false;

    return 0;
}

vae_thalamic_bridge_t* vae_thalamic_bridge_create(const vae_thalamic_bridge_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_thalamic_bridge_create: config is NULL");
        return NULL;
    }

    vae_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(vae_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_thalamic_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->config = *config;
    bridge->state = VAE_THAL_STATE_DISCONNECTED;
    bridge->is_initialized = false;
    bridge->creation_time_us = get_timestamp_us();

    /* Initialize nucleus states */
    for (int n = 0; n < VAE_THAL_NUC_COUNT; n++) {
        bridge->thalamic_state.nuclei[n].nucleus = (vae_thalamic_nucleus_t)n;
        bridge->thalamic_state.nuclei[n].mode = VAE_THAL_MODE_TONIC;
        bridge->thalamic_state.nuclei[n].current_attention = 0.5f;
        bridge->thalamic_state.nuclei[n].t_channel_state = 1.0f;
        bridge->thalamic_state.nuclei[n].is_bursting = false;
        bridge->thalamic_state.nuclei[n].burst_count = 0;
    }

    bridge->thalamic_state.global_attention = 0.5f;
    bridge->thalamic_state.global_arousal = 1.0f;
    bridge->thalamic_state.dominant_mode = VAE_THAL_MODE_TONIC;
    bridge->global_attention = 0.5f;

    /* Initialize statistics */
    bridge->stats.creation_time_us = bridge->creation_time_us;

    bridge->is_initialized = true;

    if (config->enable_logging) {
        nimcp_log_info(VAE_THAL_MODULE_ID, "VAE-Thalamic Bridge created");
    }

    return bridge;
}

void vae_thalamic_bridge_destroy(vae_thalamic_bridge_t* bridge)
{
    if (!bridge) return;

    vae_thalamic_bridge_disconnect(bridge);

    /* Free per-nucleus buffers */
    for (int n = 0; n < VAE_THAL_NUC_COUNT; n++) {
        nimcp_free(bridge->nucleus_latents[n]);
        nimcp_free(bridge->nucleus_gates[n]);
    }

    nimcp_free(bridge->attention_weights);
    nimcp_free(bridge->trn_inhibition);
    nimcp_free(bridge->feedback_buffer);
    nimcp_free(bridge->relay_buffer);
    nimcp_free(bridge->gate_buffer);

    nimcp_free(bridge);
    bridge = NULL;
}

int vae_thalamic_bridge_connect_vae(vae_thalamic_bridge_t* bridge, vae_system_t* vae)
{
    if (!bridge) return NIMCP_ERROR_VAE_THAL_NULL;
    if (!vae) return NIMCP_ERROR_VAE_THAL_NULL;

    bridge->vae = vae;

    /* Allocate buffers based on VAE dimensions */
    uint32_t latent_dim = vae_get_latent_dim(vae);
    if (latent_dim == 0) latent_dim = VAE_THAL_DEFAULT_LATENT_DIM;

    /* Attention weights */
    nimcp_free(bridge->attention_weights);
    bridge->attention_weights = nimcp_calloc(latent_dim, sizeof(float));
    if (!bridge->attention_weights) return NIMCP_ERROR_VAE_THAL_NO_MEMORY;

    /* Initialize attention to 0.5 */
    for (uint32_t i = 0; i < latent_dim; i++) {
        bridge->attention_weights[i] = 0.5f;
    }

    /* Working buffers */
    nimcp_free(bridge->relay_buffer);
    bridge->relay_buffer = nimcp_calloc(latent_dim, sizeof(float));
    nimcp_free(bridge->gate_buffer);
    bridge->gate_buffer = nimcp_calloc(latent_dim, sizeof(float));

    if (!bridge->relay_buffer || !bridge->gate_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_THAL_NO_MEMORY, "vae_thalamic_bridge: error condition");
        return NIMCP_ERROR_VAE_THAL_NO_MEMORY;
    }

    /* Allocate per-nucleus buffers */
    for (int n = 0; n < VAE_THAL_NUC_COUNT; n++) {
        uint32_t nuc_dim = bridge->config.nuclei[n].latent_dim_count;
        if (nuc_dim == 0) nuc_dim = 4;

        nimcp_free(bridge->nucleus_latents[n]);
        bridge->nucleus_latents[n] = nimcp_calloc(nuc_dim, sizeof(float));
        nimcp_free(bridge->nucleus_gates[n]);
        bridge->nucleus_gates[n] = nimcp_calloc(nuc_dim, sizeof(float));
    }

    if (bridge->thalamus) {
        bridge->state = VAE_THAL_STATE_CONNECTED;
    }

    if (bridge->config.enable_logging) {
        nimcp_log_info(VAE_THAL_MODULE_ID, "VAE connected (latent_dim=%u)", latent_dim);
    }

    return 0;
}

int vae_thalamic_bridge_connect_thalamus(vae_thalamic_bridge_t* bridge, void* thalamus)
{
    if (!bridge) return NIMCP_ERROR_VAE_THAL_NULL;

    bridge->thalamus = thalamus;

    /* Allocate TRN inhibition buffer */
    bridge->trn_dim = VAE_THALAMIC_MAX_CHANNELS;
    nimcp_free(bridge->trn_inhibition);
    bridge->trn_inhibition = nimcp_calloc(bridge->trn_dim, sizeof(float));
    if (!bridge->trn_inhibition) return -1;

    if (bridge->vae) {
        bridge->state = VAE_THAL_STATE_CONNECTED;
    }

    if (bridge->config.enable_logging) {
        nimcp_log_info(VAE_THAL_MODULE_ID, "Thalamus system connected");
    }

    return 0;
}

int vae_thalamic_bridge_disconnect(vae_thalamic_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_THAL_NULL;

    bridge->vae = NULL;
    bridge->thalamus = NULL;
    bridge->state = VAE_THAL_STATE_DISCONNECTED;

    return 0;
}

bool vae_thalamic_bridge_is_connected(const vae_thalamic_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->state == VAE_THAL_STATE_CONNECTED ||
           bridge->state == VAE_THAL_STATE_RELAYING ||
           bridge->state == VAE_THAL_STATE_GATING;
}

/* ============================================================================
 * Relay API
 * ============================================================================ */

int vae_thalamic_relay(vae_thalamic_bridge_t* bridge,
                        vae_thalamic_nucleus_t nucleus,
                        const float* input_latent, uint32_t latent_dim,
                        vae_thalamic_relay_result_t* result)
{
    if (!bridge || !input_latent || !result) return NIMCP_ERROR_VAE_THAL_NULL;
    if (nucleus >= VAE_THAL_NUC_COUNT) return NIMCP_ERROR_VAE_THAL_INVALID_NUC;
    if (!(bridge->config.active_nuclei_mask & (1 << nucleus))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_THAL_INVALID_NUC, "vae_thalamic_bridge: error condition");
        return NIMCP_ERROR_VAE_THAL_INVALID_NUC;
    }

    uint64_t start_us = get_timestamp_us();
    bridge->state = VAE_THAL_STATE_RELAYING;

    memset(result, 0, sizeof(*result));
    result->latent_dim = latent_dim;
    result->relayed_latent = nimcp_calloc(latent_dim, sizeof(float));
    result->attention_weights = nimcp_calloc(latent_dim, sizeof(float));
    if (!result->attention_weights) return -1;
    result->gate_values = nimcp_calloc(latent_dim, sizeof(float));

    if (!result->relayed_latent || !result->attention_weights || !result->gate_values) {
        vae_thalamic_relay_result_free(result);
        bridge->state = VAE_THAL_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_THAL_NO_MEMORY, "vae_thalamic_bridge: error condition");
        return NIMCP_ERROR_VAE_THAL_NO_MEMORY;
    }

    /* Get nucleus state */
    vae_thalamic_nucleus_state_t* ns = &bridge->thalamic_state.nuclei[nucleus];
    result->mode_used = ns->mode;

    /* Get latent range for this nucleus */
    uint32_t nuc_start, nuc_count;
    get_nucleus_latent_range(bridge, nucleus, &nuc_start, &nuc_count);

    /* Compute effective gain based on mode */
    float gain = 1.0f;
    if (ns->mode == VAE_THAL_MODE_BURST) {
        gain = bridge->config.burst.burst_temperature_scale;
    } else if (ns->mode == VAE_THAL_MODE_INHIBITED) {
        gain = 0.1f;
    }
    result->effective_gain = gain;

    /* Apply attention-gated relay */
    float total_gate = 0.0f;
    bool any_gated = false;

    for (uint32_t d = 0; d < latent_dim; d++) {
        /* Get attention weight for this dimension */
        float attention = (d < latent_dim && bridge->attention_weights) ?
                         bridge->attention_weights[d] : 0.5f;

        /* Apply nucleus-specific attention if in range */
        if (d >= nuc_start && d < nuc_start + nuc_count) {
            attention *= bridge->config.nuclei[nucleus].attention_weight;
        }

        /* Compute gate value */
        float gate = compute_gate(bridge, nucleus, attention);
        result->gate_values[d] = gate;
        result->attention_weights[d] = attention;
        total_gate += gate;

        if (gate < 0.5f) any_gated = true;

        /* Relay with gating and gain */
        result->relayed_latent[d] = input_latent[d] * gate * gain;
    }

    /* Update state */
    update_nucleus_state(bridge, nucleus);

    /* Update statistics */
    update_stats(bridge, nucleus, ns->is_bursting, any_gated);

    result->relay_time_us = get_timestamp_us() - start_us;
    bridge->state = VAE_THAL_STATE_CONNECTED;

    return 0;
}

int vae_thalamic_relay_multimodal(vae_thalamic_bridge_t* bridge,
                                   const float* visual, uint32_t vis_dim,
                                   const float* auditory, uint32_t aud_dim,
                                   const float* somatosensory, uint32_t soma_dim,
                                   vae_thalamic_routing_result_t* result)
{
    if (!bridge || !result) return NIMCP_ERROR_VAE_THAL_NULL;

    uint64_t start_us = get_timestamp_us();
    memset(result, 0, sizeof(*result));

    /* Route visual through LGN */
    if (visual && vis_dim > 0) {
        vae_thalamic_relay_result_t vis_result;
        if (vae_thalamic_relay(bridge, VAE_THAL_NUC_LGN, visual, vis_dim,
                               &vis_result) == 0) {
            result->visual_latent = vis_result.relayed_latent;
            result->visual_dim = vis_result.latent_dim;
            vis_result.relayed_latent = NULL; /* Transfer ownership */
            vae_thalamic_relay_result_free(&vis_result);
        }
    }

    /* Route auditory through MGN */
    if (auditory && aud_dim > 0) {
        vae_thalamic_relay_result_t aud_result;
        if (vae_thalamic_relay(bridge, VAE_THAL_NUC_MGN, auditory, aud_dim,
                               &aud_result) == 0) {
            result->auditory_latent = aud_result.relayed_latent;
            result->auditory_dim = aud_result.latent_dim;
            aud_result.relayed_latent = NULL;
            vae_thalamic_relay_result_free(&aud_result);
        }
    }

    /* Route somatosensory through VPL */
    if (somatosensory && soma_dim > 0) {
        vae_thalamic_relay_result_t soma_result;
        if (vae_thalamic_relay(bridge, VAE_THAL_NUC_VPL, somatosensory, soma_dim,
                               &soma_result) == 0) {
            result->somatosensory_latent = soma_result.relayed_latent;
            result->somato_dim = soma_result.latent_dim;
            soma_result.relayed_latent = NULL;
            vae_thalamic_relay_result_free(&soma_result);
        }
    }

    /* Compute modality weights based on attention */
    uint32_t num_modalities = (vis_dim > 0 ? 1 : 0) +
                             (aud_dim > 0 ? 1 : 0) +
                             (soma_dim > 0 ? 1 : 0);
    if (num_modalities > 0) {
        result->modality_weights = nimcp_calloc(3, sizeof(float));
        if (result->modality_weights) {
            result->modality_weights[0] = bridge->thalamic_state.nuclei[VAE_THAL_NUC_LGN].current_attention;
            result->modality_weights[1] = bridge->thalamic_state.nuclei[VAE_THAL_NUC_MGN].current_attention;
            result->modality_weights[2] = bridge->thalamic_state.nuclei[VAE_THAL_NUC_VPL].current_attention;
        }
    }

    result->routing_time_us = get_timestamp_us() - start_us;

    return 0;
}

int vae_thalamic_relay_with_feedback(vae_thalamic_bridge_t* bridge,
                                      vae_thalamic_nucleus_t nucleus,
                                      const float* input_latent, uint32_t latent_dim,
                                      const float* cortical_feedback, uint32_t fb_dim,
                                      vae_thalamic_relay_result_t* result)
{
    if (!bridge || !input_latent || !result) return NIMCP_ERROR_VAE_THAL_NULL;

    /* Apply cortical feedback to modulate attention */
    if (cortical_feedback && fb_dim > 0 && bridge->config.enable_cortical_feedback) {
        float fb_strength = bridge->config.feedback_strength;
        uint32_t min_dim = (fb_dim < latent_dim) ? fb_dim : latent_dim;

        if (bridge->attention_weights) {
            for (uint32_t d = 0; d < min_dim; d++) {
                bridge->attention_weights[d] =
                    (1.0f - fb_strength) * bridge->attention_weights[d] +
                    fb_strength * cortical_feedback[d];
            }
        }
    }

    /* Perform standard relay */
    return vae_thalamic_relay(bridge, nucleus, input_latent, latent_dim, result);
}

/* ============================================================================
 * Attention API
 * ============================================================================ */

int vae_thalamic_set_attention(vae_thalamic_bridge_t* bridge,
                                vae_thalamic_nucleus_t nucleus,
                                float attention_level)
{
    if (!bridge) return NIMCP_ERROR_VAE_THAL_NULL;
    if (nucleus >= VAE_THAL_NUC_COUNT) return NIMCP_ERROR_VAE_THAL_INVALID_NUC;

    bridge->thalamic_state.nuclei[nucleus].current_attention =
        nimcp_clampf(attention_level,
               bridge->config.attention.attention_min,
               bridge->config.attention.attention_max);

    return 0;
}

int vae_thalamic_set_global_attention(vae_thalamic_bridge_t* bridge,
                                       float attention_level)
{
    if (!bridge) return NIMCP_ERROR_VAE_THAL_NULL;

    attention_level = nimcp_clampf(attention_level,
                            bridge->config.attention.attention_min,
                            bridge->config.attention.attention_max);

    bridge->global_attention = attention_level;
    bridge->thalamic_state.global_attention = attention_level;

    /* Propagate to all nuclei */
    for (int n = 0; n < VAE_THAL_NUC_COUNT; n++) {
        bridge->thalamic_state.nuclei[n].current_attention = attention_level;
    }

    return 0;
}

int vae_thalamic_update_attention_from_pulvinar(vae_thalamic_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_THAL_NULL;

    if (!bridge->config.attention.pulvinar_modulation) {
        return 0;
    }

    /* Pulvinar modulates attention to other nuclei */
    float pulv_activity = bridge->thalamic_state.nuclei[VAE_THAL_NUC_PULVINAR].current_attention;

    /* Modulate sensory nuclei based on pulvinar */
    for (int n = 0; n < VAE_THAL_NUC_COUNT; n++) {
        if (n == VAE_THAL_NUC_PULVINAR || n == VAE_THAL_NUC_TRN) continue;

        float current = bridge->thalamic_state.nuclei[n].current_attention;
        float modulated = current * (0.5f + 0.5f * pulv_activity);
        bridge->thalamic_state.nuclei[n].current_attention =
            nimcp_clampf(modulated,
                   bridge->config.attention.attention_min,
                   bridge->config.attention.attention_max);
    }

    return 0;
}

float vae_thalamic_get_attention(const vae_thalamic_bridge_t* bridge,
                                  vae_thalamic_nucleus_t nucleus)
{
    if (!bridge || nucleus >= VAE_THAL_NUC_COUNT) return 0.0f;
    return bridge->thalamic_state.nuclei[nucleus].current_attention;
}

/* ============================================================================
 * Gating API
 * ============================================================================ */

int vae_thalamic_compute_gating(vae_thalamic_bridge_t* bridge,
                                 vae_thalamic_nucleus_t nucleus,
                                 vae_thalamic_gating_result_t* result)
{
    if (!bridge || !result) return NIMCP_ERROR_VAE_THAL_NULL;
    if (nucleus >= VAE_THAL_NUC_COUNT) return NIMCP_ERROR_VAE_THAL_INVALID_NUC;

    bridge->state = VAE_THAL_STATE_GATING;
    memset(result, 0, sizeof(*result));

    vae_thalamic_nucleus_state_t* ns = &bridge->thalamic_state.nuclei[nucleus];

    /* Determine gating action based on state */
    if (ns->mode == VAE_THAL_MODE_INHIBITED || ns->trn_inhibition_level > 0.8f) {
        result->action = VAE_THAL_GATE_SUPPRESS;
        result->gate_strength = 1.0f - ns->trn_inhibition_level;
        snprintf(result->reason, sizeof(result->reason), "TRN suppression");
    } else if (ns->mode == VAE_THAL_MODE_BURST) {
        result->action = VAE_THAL_GATE_AMPLIFY;
        result->gate_strength = bridge->config.burst.burst_temperature_scale;
        snprintf(result->reason, sizeof(result->reason), "Burst amplification");
    } else if (ns->current_attention < 0.3f) {
        result->action = VAE_THAL_GATE_MODULATE;
        result->gate_strength = ns->current_attention;
        snprintf(result->reason, sizeof(result->reason), "Low attention");
    } else {
        result->action = VAE_THAL_GATE_PASS;
        result->gate_strength = 1.0f;
        snprintf(result->reason, sizeof(result->reason), "Normal relay");
    }

    result->gating_source = nucleus;
    result->trn_contribution = ns->trn_inhibition_level;

    bridge->state = VAE_THAL_STATE_CONNECTED;
    return 0;
}

int vae_thalamic_apply_trn_inhibition(vae_thalamic_bridge_t* bridge,
                                       float* latent, uint32_t dim)
{
    if (!bridge || !latent) return NIMCP_ERROR_VAE_THAL_NULL;

    if (!bridge->config.trn.enable_trn_gating) {
        return 0;
    }

    vae_thalamic_nucleus_state_t* trn = &bridge->thalamic_state.nuclei[VAE_THAL_NUC_TRN];

    /* Apply TRN inhibition across all latent dimensions */
    float inhibition = trn->trn_inhibition_level;
    float gate = sigmoid(bridge->config.trn.trn_threshold - inhibition,
                        bridge->config.trn.gating_slope);

    for (uint32_t d = 0; d < dim; d++) {
        latent[d] *= gate;
    }

    if (gate < 0.5f) {
        bridge->stats.trn_suppressions++;
    }

    return 0;
}

int vae_thalamic_set_trn_activity(vae_thalamic_bridge_t* bridge,
                                   const float* trn_activity, uint32_t dim)
{
    if (!bridge || !trn_activity) return NIMCP_ERROR_VAE_THAL_NULL;

    /* Update TRN inhibition levels */
    float avg_trn = 0.0f;
    uint32_t copy_dim = (dim < bridge->trn_dim) ? dim : bridge->trn_dim;

    for (uint32_t i = 0; i < copy_dim; i++) {
        if (bridge->trn_inhibition) {
            bridge->trn_inhibition[i] = trn_activity[i];
        }
        avg_trn += trn_activity[i];
    }
    avg_trn /= copy_dim;

    /* Update TRN nucleus state */
    bridge->thalamic_state.nuclei[VAE_THAL_NUC_TRN].trn_inhibition_level = avg_trn;
    bridge->thalamic_state.trn_active = (avg_trn > bridge->config.trn.trn_threshold);

    /* Propagate TRN inhibition to other nuclei */
    if (bridge->config.trn.lateral_inhibition) {
        for (int n = 0; n < VAE_THAL_NUC_COUNT; n++) {
            if (n != VAE_THAL_NUC_TRN) {
                bridge->thalamic_state.nuclei[n].trn_inhibition_level = avg_trn;
            }
        }
    }

    return 0;
}

/* ============================================================================
 * Mode API
 * ============================================================================ */

int vae_thalamic_set_mode(vae_thalamic_bridge_t* bridge,
                           vae_thalamic_nucleus_t nucleus,
                           vae_thalamic_mode_t mode)
{
    if (!bridge) return NIMCP_ERROR_VAE_THAL_NULL;
    if (nucleus >= VAE_THAL_NUC_COUNT) return NIMCP_ERROR_VAE_THAL_INVALID_NUC;

    bridge->thalamic_state.nuclei[nucleus].mode = mode;
    bridge->thalamic_state.nuclei[nucleus].is_bursting = (mode == VAE_THAL_MODE_BURST);

    return 0;
}

vae_thalamic_mode_t vae_thalamic_get_mode(const vae_thalamic_bridge_t* bridge,
                                           vae_thalamic_nucleus_t nucleus)
{
    if (!bridge || nucleus >= VAE_THAL_NUC_COUNT) return VAE_THAL_MODE_TONIC;
    return bridge->thalamic_state.nuclei[nucleus].mode;
}

int vae_thalamic_trigger_burst(vae_thalamic_bridge_t* bridge,
                                vae_thalamic_nucleus_t nucleus)
{
    if (!bridge) return NIMCP_ERROR_VAE_THAL_NULL;
    if (nucleus >= VAE_THAL_NUC_COUNT) return NIMCP_ERROR_VAE_THAL_INVALID_NUC;

    vae_thalamic_nucleus_state_t* ns = &bridge->thalamic_state.nuclei[nucleus];

    /* Can only burst if T-channels are available */
    if (ns->t_channel_state > bridge->config.nuclei[nucleus].burst_threshold) {
        ns->mode = VAE_THAL_MODE_BURST;
        ns->is_bursting = true;
        ns->burst_count++;

        if (bridge->config.enable_logging) {
            LOG_DEBUG("Burst triggered in %s",
                      vae_thalamic_nucleus_to_string(nucleus));
        }
    }

    return 0;
}

bool vae_thalamic_is_bursting(const vae_thalamic_bridge_t* bridge,
                               vae_thalamic_nucleus_t nucleus)
{
    if (!bridge || nucleus >= VAE_THAL_NUC_COUNT) {
        return false;
    }
    return bridge->thalamic_state.nuclei[nucleus].is_bursting;
}

/* ============================================================================
 * State API
 * ============================================================================ */

int vae_thalamic_update_state(vae_thalamic_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_THAL_NULL;

    /* Update all active nuclei */
    for (int n = 0; n < VAE_THAL_NUC_COUNT; n++) {
        if (bridge->config.active_nuclei_mask & (1 << n)) {
            update_nucleus_state(bridge, (vae_thalamic_nucleus_t)n);
        }
    }

    /* Update global state */
    bridge->thalamic_state.state_time_us = get_timestamp_us();

    /* Determine dominant mode */
    int burst_count = 0;
    int inhibited_count = 0;
    for (int n = 0; n < VAE_THAL_NUC_COUNT; n++) {
        if (bridge->thalamic_state.nuclei[n].mode == VAE_THAL_MODE_BURST) burst_count++;
        if (bridge->thalamic_state.nuclei[n].mode == VAE_THAL_MODE_INHIBITED) inhibited_count++;
    }

    if (burst_count > VAE_THAL_NUC_COUNT / 3) {
        bridge->thalamic_state.dominant_mode = VAE_THAL_MODE_BURST;
    } else if (inhibited_count > VAE_THAL_NUC_COUNT / 3) {
        bridge->thalamic_state.dominant_mode = VAE_THAL_MODE_INHIBITED;
    } else {
        bridge->thalamic_state.dominant_mode = VAE_THAL_MODE_TONIC;
    }

    /* Update pulvinar-based attention if enabled */
    vae_thalamic_update_attention_from_pulvinar(bridge);

    return 0;
}

int vae_thalamic_get_state(const vae_thalamic_bridge_t* bridge,
                            vae_thalamic_state_t* state)
{
    if (!bridge || !state) return NIMCP_ERROR_VAE_THAL_NULL;
    *state = bridge->thalamic_state;
    return 0;
}

int vae_thalamic_get_nucleus_state(const vae_thalamic_bridge_t* bridge,
                                    vae_thalamic_nucleus_t nucleus,
                                    vae_thalamic_nucleus_state_t* state)
{
    if (!bridge || !state) return NIMCP_ERROR_VAE_THAL_NULL;
    if (nucleus >= VAE_THAL_NUC_COUNT) return NIMCP_ERROR_VAE_THAL_INVALID_NUC;

    *state = bridge->thalamic_state.nuclei[nucleus];
    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_thalamic_bridge_state_t vae_thalamic_bridge_get_state(const vae_thalamic_bridge_t* bridge)
{
    if (!bridge) return VAE_THAL_STATE_ERROR;
    return bridge->state;
}

int vae_thalamic_bridge_get_stats(const vae_thalamic_bridge_t* bridge,
                                   vae_thalamic_bridge_stats_t* stats)
{
    if (!bridge || !stats) return NIMCP_ERROR_VAE_THAL_NULL;
    *stats = bridge->stats;
    return 0;
}

const char* vae_thalamic_nucleus_to_string(vae_thalamic_nucleus_t nucleus)
{
    switch (nucleus) {
        case VAE_THAL_NUC_LGN: return "LGN";
        case VAE_THAL_NUC_MGN: return "MGN";
        case VAE_THAL_NUC_VPL: return "VPL";
        case VAE_THAL_NUC_VPM: return "VPM";
        case VAE_THAL_NUC_VA: return "VA";
        case VAE_THAL_NUC_VL: return "VL";
        case VAE_THAL_NUC_PULVINAR: return "Pulvinar";
        case VAE_THAL_NUC_MD: return "MD";
        case VAE_THAL_NUC_ANTERIOR: return "Anterior";
        case VAE_THAL_NUC_TRN: return "TRN";
        default: return "unknown";
    }
}

const char* vae_thalamic_mode_to_string(vae_thalamic_mode_t mode)
{
    switch (mode) {
        case VAE_THAL_MODE_TONIC: return "tonic";
        case VAE_THAL_MODE_BURST: return "burst";
        case VAE_THAL_MODE_INHIBITED: return "inhibited";
        default: return "unknown";
    }
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_thalamic_relay_result_free(vae_thalamic_relay_result_t* result)
{
    if (!result) return;

    nimcp_free(result->relayed_latent);
    nimcp_free(result->attention_weights);
    nimcp_free(result->gate_values);

    memset(result, 0, sizeof(*result));
}

void vae_thalamic_routing_result_free(vae_thalamic_routing_result_t* result)
{
    if (!result) return;

    nimcp_free(result->visual_latent);
    nimcp_free(result->auditory_latent);
    nimcp_free(result->somatosensory_latent);
    nimcp_free(result->modality_weights);

    memset(result, 0, sizeof(*result));
}
