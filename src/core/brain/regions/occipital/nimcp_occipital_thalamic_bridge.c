/**
 * @file nimcp_occipital_thalamic_bridge.c
 * @brief Bridge between Occipital Cortex and thalamic router
 *
 * WHAT: Routes occipital visual signals through thalamic relay nuclei
 * WHY: All primary visual information passes through LGN before reaching V1
 * HOW: Manages LGN, Pulvinar, SC pathways with attention gating
 *
 * @author NIMCP Team
 * @date 2025-01-01
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/occipital/nimcp_occipital_thalamic_bridge.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(occipital_thalamic_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "OCCIPITAL_THALAMIC_BRIDGE"


/*=============================================================================
 * Internal Structure
 *===========================================================================*/

struct occipital_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* occipital;                          /**< Occipital adapter handle */
    thalamic_router_t* router;                /**< Thalamic router handle */
    occipital_thalamic_config_t config;       /**< Bridge configuration */
    occipital_thalamic_state_t state;         /**< Current routing state */
    occipital_thalamic_stats_t stats;         /**< Runtime statistics */
    bio_router_t* bio_router;                 /**< Bio-async router */
    bool bio_async_connected;                 /**< Bio-async registration flag */
    float* feedback_buffer;                   /**< Cortical feedback buffer */
    uint32_t feedback_dim;                    /**< Feedback buffer dimension */
};

/*=============================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Update running average statistic
 */
static float update_avg(float old_avg, float new_val, float alpha) {
    float result = (1.0f - alpha) * old_avg + alpha * new_val;
    return isfinite(result) ? result : old_avg;
}

/**
 * @brief Compute attention modulation for a location
 */
static float compute_attention_modulation(
    const occipital_thalamic_bridge_t* bridge,
    float retino_x, float retino_y)
{
    if (!bridge->config.enable_attention_gating) {
        return 1.0f;
    }

    float dx = retino_x - bridge->state.attention_x;
    float dy = retino_y - bridge->state.attention_y;
    float dist = sqrtf(dx * dx + dy * dy);

    if (dist <= bridge->state.attention_radius) {
        /* Within attention spotlight - full boost */
        return 1.0f + bridge->state.pulvinar_attention * 0.5f;
    } else {
        /* Outside spotlight - attenuate based on distance */
        float falloff = (dist - bridge->state.attention_radius) * 2.0f;
        float attenuation = nimcp_clampf(1.0f - falloff, 0.5f, 1.0f);
        return attenuation;
    }
}

/**
 * @brief Determine which LGN pathway a signal should use
 */
static lgn_pathway_type_t determine_pathway(const occipital_thalamic_signal_t* signal) {
    /* Motion signals → Magnocellular */
    if (signal->signal_type == OCCIPITAL_SIGNAL_V5 ||
        signal->signal_type == OCCIPITAL_SIGNAL_DORSAL ||
        signal->signal_type == OCCIPITAL_SIGNAL_MAGNO) {
        return LGN_PATHWAY_MAGNOCELLULAR;
    }

    /* Color/form signals → Parvocellular or Koniocellular */
    if (signal->signal_type == OCCIPITAL_SIGNAL_KONIO) {
        return LGN_PATHWAY_KONIOCELLULAR;
    }
    if (signal->signal_type == OCCIPITAL_SIGNAL_PARVO ||
        signal->signal_type == OCCIPITAL_SIGNAL_V4 ||
        signal->signal_type == OCCIPITAL_SIGNAL_VENTRAL) {
        return LGN_PATHWAY_PARVOCELLULAR;
    }

    /* Use signal's preferred pathway if specified */
    if (signal->pathway < LGN_PATHWAY_COUNT) {
        return signal->pathway;
    }

    /* Default: high temporal frequency → magno, high spatial → parvo */
    if (signal->temporal_frequency > 10.0f) {
        return LGN_PATHWAY_MAGNOCELLULAR;
    }
    if (signal->spatial_frequency > 4.0f) {
        return LGN_PATHWAY_PARVOCELLULAR;
    }

    return LGN_PATHWAY_PARVOCELLULAR;  /* Default */
}

/**
 * @brief Get pathway-specific gain
 */
static float get_pathway_gain(
    const occipital_thalamic_bridge_t* bridge,
    lgn_pathway_type_t pathway)
{
    switch (pathway) {
        case LGN_PATHWAY_MAGNOCELLULAR:
            return bridge->state.magno_gain * bridge->config.magnocellular_boost;
        case LGN_PATHWAY_PARVOCELLULAR:
            return bridge->state.parvo_gain * bridge->config.parvocellular_boost;
        case LGN_PATHWAY_KONIOCELLULAR:
            return bridge->state.konio_gain * bridge->config.koniocellular_boost;
        default:
            return 1.0f;
    }
}

/**
 * @brief Apply TRN gating
 */
static float apply_trn_gating(
    const occipital_thalamic_bridge_t* bridge,
    float signal_strength)
{
    /* TRN gate of 0 = full suppression, 1 = no suppression */
    return signal_strength * bridge->state.trn_gate;
}

/**
 * @brief Update pathway activity flags
 */
static void update_pathway_activity(
    occipital_thalamic_bridge_t* bridge,
    lgn_pathway_type_t pathway)
{
    switch (pathway) {
        case LGN_PATHWAY_MAGNOCELLULAR:
            bridge->state.magnocellular_active = true;
            break;
        case LGN_PATHWAY_PARVOCELLULAR:
            bridge->state.parvocellular_active = true;
            break;
        case LGN_PATHWAY_KONIOCELLULAR:
            bridge->state.koniocellular_active = true;
            break;
        default:
            break;
    }
}

/**
 * @brief Update routing statistics
 */
static void update_routing_stats(
    occipital_thalamic_bridge_t* bridge,
    const occipital_thalamic_signal_t* signal,
    lgn_pathway_type_t pathway,
    bool was_suppressed)
{
    const float alpha = 0.1f;

    /* Update signal type counts */
    switch (signal->signal_type) {
        case OCCIPITAL_SIGNAL_V1:
            bridge->stats.v1_signals_routed++;
            break;
        case OCCIPITAL_SIGNAL_V2:
            bridge->stats.v2_signals_routed++;
            break;
        case OCCIPITAL_SIGNAL_DORSAL:
        case OCCIPITAL_SIGNAL_V5:
            bridge->stats.dorsal_signals++;
            break;
        case OCCIPITAL_SIGNAL_VENTRAL:
        case OCCIPITAL_SIGNAL_V4:
            bridge->stats.ventral_signals++;
            break;
        default:
            break;
    }

    /* Update pathway counts */
    switch (pathway) {
        case LGN_PATHWAY_MAGNOCELLULAR:
            bridge->stats.magno_signals++;
            break;
        case LGN_PATHWAY_PARVOCELLULAR:
            bridge->stats.parvo_signals++;
            break;
        case LGN_PATHWAY_KONIOCELLULAR:
            bridge->stats.konio_signals++;
            break;
        default:
            break;
    }

    if (was_suppressed) {
        bridge->stats.signals_suppressed++;
    }

    /* Update averages */
    bridge->stats.avg_visual_intensity = update_avg(
        bridge->stats.avg_visual_intensity,
        signal->visual_intensity, alpha);

    bridge->stats.avg_lgn_gain = update_avg(
        bridge->stats.avg_lgn_gain,
        bridge->state.lgn_gain, alpha);

    bridge->stats.avg_pulvinar_attention = update_avg(
        bridge->stats.avg_pulvinar_attention,
        bridge->state.pulvinar_attention, alpha);
}

/*=============================================================================
 * Configuration API
 *===========================================================================*/

occipital_thalamic_config_t occipital_thalamic_default_config(void) {
    occipital_thalamic_config_t config = {
        /* Enable/disable toggles */
        .enable_attention_gating = true,
        .enable_contrast_boost = true,
        .enable_retinotopic_routing = true,
        .enable_magno_parvo_separation = true,
        .enable_cortical_feedback = true,
        .enable_bio_async = false,

        /* Threshold parameters */
        .min_visual_intensity = 0.1f,
        .contrast_threshold = 0.3f,
        .attention_threshold = 0.2f,

        /* Pathway boost factors */
        .magnocellular_boost = 1.0f,
        .parvocellular_boost = 1.0f,
        .koniocellular_boost = 1.0f,

        /* Decay rates */
        .attention_decay_rate = 0.05f,
        .feedback_decay_rate = 0.1f,

        /* Latency simulation */
        .lgn_latency_ms = 2.0f,
        .pulvinar_latency_ms = 5.0f
    };
    return config;
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

occipital_thalamic_bridge_t* occipital_thalamic_bridge_create(
    void* occipital,
    thalamic_router_t* router,
    const occipital_thalamic_config_t* config)
{
    /* NULL router is allowed - can be connected later or used for testing */

    occipital_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(occipital_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    if (bridge_base_init(&bridge->base, 0, "occipital_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    bridge->occipital = occipital;
    bridge->router = router;
    bridge->config = config ? *config : occipital_thalamic_default_config();
    bridge->bio_router = NULL;
    bridge->bio_async_connected = false;
    bridge->feedback_buffer = NULL;
    bridge->feedback_dim = 0;

    /* Initialize state */
    bridge->state.lgn_gain = 1.0f;
    bridge->state.pulvinar_attention = 0.5f;
    bridge->state.sc_saccade_readiness = 0.0f;
    bridge->state.trn_gate = 1.0f;  /* Fully open */

    bridge->state.magno_gain = 1.0f;
    bridge->state.parvo_gain = 1.0f;
    bridge->state.konio_gain = 1.0f;

    bridge->state.magnocellular_active = false;
    bridge->state.parvocellular_active = false;
    bridge->state.koniocellular_active = false;

    bridge->state.attention_x = 0.5f;  /* Center */
    bridge->state.attention_y = 0.5f;
    bridge->state.attention_radius = 0.25f;

    bridge->state.cortical_feedback = 0.0f;
    bridge->state.feedback_active = false;

    /* Initialize stats */
    memset(&bridge->stats, 0, sizeof(occipital_thalamic_stats_t));

    return bridge;
}

void occipital_thalamic_bridge_destroy(occipital_thalamic_bridge_t* bridge) {
    if (bridge) {
        if (bridge->feedback_buffer) {
            nimcp_free(bridge->feedback_buffer);
        }
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
    }
}

int occipital_thalamic_bridge_reset(occipital_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Reset state */
    bridge->state.lgn_gain = 1.0f;
    bridge->state.pulvinar_attention = 0.5f;
    bridge->state.sc_saccade_readiness = 0.0f;
    bridge->state.trn_gate = 1.0f;

    bridge->state.magno_gain = 1.0f;
    bridge->state.parvo_gain = 1.0f;
    bridge->state.konio_gain = 1.0f;

    bridge->state.magnocellular_active = false;
    bridge->state.parvocellular_active = false;
    bridge->state.koniocellular_active = false;

    bridge->state.attention_x = 0.5f;
    bridge->state.attention_y = 0.5f;
    bridge->state.attention_radius = 0.25f;

    bridge->state.cortical_feedback = 0.0f;
    bridge->state.feedback_active = false;

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(occipital_thalamic_stats_t));

    return 0;
}

/*=============================================================================
 * Signal Routing API
 *===========================================================================*/

int occipital_thalamic_route_signal(
    occipital_thalamic_bridge_t* bridge,
    const occipital_thalamic_signal_t* signal)
{
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_thalamic_route_signal: required parameter is NULL");
        return -1;
    }

    /* Check minimum intensity threshold */
    if (signal->visual_intensity < bridge->config.min_visual_intensity) {
        return 0;  /* Signal below threshold, not an error */
    }

    /* Determine pathway */
    lgn_pathway_type_t pathway = determine_pathway(signal);
    update_pathway_activity(bridge, pathway);

    /* Compute effective gain */
    float base_gain = bridge->state.lgn_gain;
    float pathway_gain = get_pathway_gain(bridge, pathway);

    /* Apply attention modulation */
    float attention_mod = compute_attention_modulation(bridge,
        signal->retinotopic_x, signal->retinotopic_y);

    /* Apply contrast boost */
    float contrast_boost = 1.0f;
    if (bridge->config.enable_contrast_boost &&
        signal->contrast > bridge->config.contrast_threshold) {
        contrast_boost = 1.0f + (signal->contrast - bridge->config.contrast_threshold);
    }

    /* Compute final signal strength */
    float effective_strength = signal->visual_intensity *
                               base_gain *
                               pathway_gain *
                               attention_mod *
                               contrast_boost;

    /* Apply TRN gating */
    float gated_strength = apply_trn_gating(bridge, effective_strength);
    bool was_suppressed = (gated_strength < effective_strength * 0.5f);

    /* Apply cortical feedback modulation */
    if (bridge->config.enable_cortical_feedback && bridge->state.feedback_active) {
        gated_strength *= (1.0f + bridge->state.cortical_feedback * 0.3f);
    }

    /* Update statistics */
    update_routing_stats(bridge, signal, pathway, was_suppressed);

    (void)gated_strength;  /* Would be sent to thalamic router */

    return 0;
}

int occipital_thalamic_route_v1(
    occipital_thalamic_bridge_t* bridge,
    const void* visual_data,
    float intensity)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    occipital_thalamic_signal_t signal = {
        .signal_type = OCCIPITAL_SIGNAL_V1,
        .pathway = LGN_PATHWAY_PARVOCELLULAR,
        .visual_intensity = intensity,
        .contrast = 0.5f,
        .spatial_frequency = 2.0f,
        .temporal_frequency = 0.0f,
        .retinotopic_x = 0.5f,
        .retinotopic_y = 0.5f,
        .eccentricity = 0.0f,
        .content = (void*)visual_data,
        .content_size = 0,
        .timestamp_us = 0
    };

    return occipital_thalamic_route_signal(bridge, &signal);
}

int occipital_thalamic_route_dorsal(
    occipital_thalamic_bridge_t* bridge,
    const void* motion_data,
    float motion_strength)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    occipital_thalamic_signal_t signal = {
        .signal_type = OCCIPITAL_SIGNAL_DORSAL,
        .pathway = LGN_PATHWAY_MAGNOCELLULAR,
        .visual_intensity = motion_strength,
        .contrast = 0.3f,  /* Motion pathway less contrast-dependent */
        .spatial_frequency = 1.0f,
        .temporal_frequency = 15.0f,  /* High temporal frequency for motion */
        .retinotopic_x = 0.5f,
        .retinotopic_y = 0.5f,
        .eccentricity = 0.2f,  /* Motion more peripheral */
        .content = (void*)motion_data,
        .content_size = 0,
        .timestamp_us = 0
    };

    return occipital_thalamic_route_signal(bridge, &signal);
}

int occipital_thalamic_route_ventral(
    occipital_thalamic_bridge_t* bridge,
    const void* form_data,
    float form_strength)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    occipital_thalamic_signal_t signal = {
        .signal_type = OCCIPITAL_SIGNAL_VENTRAL,
        .pathway = LGN_PATHWAY_PARVOCELLULAR,
        .visual_intensity = form_strength,
        .contrast = 0.6f,  /* Form pathway more contrast-dependent */
        .spatial_frequency = 4.0f,  /* High spatial frequency for detail */
        .temporal_frequency = 5.0f,
        .retinotopic_x = 0.5f,
        .retinotopic_y = 0.5f,
        .eccentricity = 0.1f,  /* Form processing more foveal */
        .content = (void*)form_data,
        .content_size = 0,
        .timestamp_us = 0
    };

    return occipital_thalamic_route_signal(bridge, &signal);
}

int occipital_thalamic_route_advanced(
    occipital_thalamic_bridge_t* bridge,
    const occipital_thalamic_request_t* request,
    occipital_thalamic_response_t* response)
{
    if (!bridge || !request || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_thalamic_route_advanced: required parameter is NULL");
        return -1;
    }

    /* Determine effective gain based on source nucleus */
    float nucleus_gain = 1.0f;
    switch (request->source) {
        case OCCIPITAL_THALAMIC_LGN:
            nucleus_gain = bridge->state.lgn_gain;
            break;
        case OCCIPITAL_THALAMIC_PULVINAR:
            nucleus_gain = bridge->state.pulvinar_attention;
            break;
        case OCCIPITAL_THALAMIC_SC:
            nucleus_gain = bridge->state.sc_saccade_readiness;
            break;
        case OCCIPITAL_THALAMIC_TRN:
            nucleus_gain = bridge->state.trn_gate;
            break;
        default:
            break;
    }

    /* Apply attention and urgency modulation */
    float effective_gain = nucleus_gain * (1.0f + request->attention_boost);
    effective_gain *= (1.0f + request->urgency * 0.5f);

    /* Apply TRN gating */
    float gating = bridge->state.trn_gate;
    bool was_suppressed = (gating < 0.5f);

    /* Fill response */
    response->routed_signal = NULL;  /* Would copy/transform signal */
    response->signal_dim = request->signal_dim;
    response->effective_gain = effective_gain;
    response->gating_applied = gating;
    response->was_suppressed = was_suppressed;
    response->latency_ms = (request->source == OCCIPITAL_THALAMIC_PULVINAR) ?
                           bridge->config.pulvinar_latency_ms :
                           bridge->config.lgn_latency_ms;
    response->routed_via = request->source;

    if (was_suppressed) {
        bridge->stats.signals_suppressed++;
    }

    return 0;
}

/*=============================================================================
 * Attention and State API
 *===========================================================================*/

int occipital_thalamic_set_attention(
    occipital_thalamic_bridge_t* bridge,
    float attention)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Clamp attention to valid range */
    bridge->state.pulvinar_attention = nimcp_clampf(attention, 0.0f, 1.0f);
    bridge->stats.attention_requests++;

    return 0;
}

int occipital_thalamic_set_spatial_attention(
    occipital_thalamic_bridge_t* bridge,
    float x,
    float y,
    float radius)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->state.attention_x = nimcp_clampf(x, 0.0f, 1.0f);
    bridge->state.attention_y = nimcp_clampf(y, 0.0f, 1.0f);
    bridge->state.attention_radius = nimcp_clampf(radius, 0.0f, 1.0f);
    bridge->stats.attention_requests++;

    return 0;
}

int occipital_thalamic_get_attention(
    const occipital_thalamic_bridge_t* bridge,
    float* attention)
{
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_thalamic_get_attention: required parameter is NULL");
        return -1;
    }

    *attention = bridge->state.pulvinar_attention;
    return 0;
}

int occipital_thalamic_set_nucleus_gain(
    occipital_thalamic_bridge_t* bridge,
    occipital_thalamic_nucleus_t nucleus,
    float gain)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    gain = nimcp_clampf(gain, 0.0f, 2.0f);

    switch (nucleus) {
        case OCCIPITAL_THALAMIC_LGN:
            bridge->state.lgn_gain = gain;
            break;
        case OCCIPITAL_THALAMIC_PULVINAR:
            bridge->state.pulvinar_attention = nimcp_clampf(gain, 0.0f, 1.0f);
            break;
        case OCCIPITAL_THALAMIC_SC:
            bridge->state.sc_saccade_readiness = nimcp_clampf(gain, 0.0f, 1.0f);
            break;
        case OCCIPITAL_THALAMIC_TRN:
            bridge->state.trn_gate = nimcp_clampf(gain, 0.0f, 1.0f);
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_thalamic_set_nucleus_gain: operation failed");
            return -1;
    }

    return 0;
}

int occipital_thalamic_get_state(
    const occipital_thalamic_bridge_t* bridge,
    occipital_thalamic_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_thalamic_get_state: required parameter is NULL");
        return -1;
    }

    *state = bridge->state;
    return 0;
}

int occipital_thalamic_apply_feedback(
    occipital_thalamic_bridge_t* bridge,
    const float* feedback_signal,
    uint32_t signal_dim)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->config.enable_cortical_feedback) {
        return 0;  /* Feedback disabled */
    }

    if (!feedback_signal || signal_dim == 0) {
        bridge->state.feedback_active = false;
        bridge->state.cortical_feedback = 0.0f;
        return 0;
    }

    /* Compute average feedback strength */
    float sum = 0.0f;
    for (uint32_t i = 0; i < signal_dim; i++) {
        sum += feedback_signal[i];
    }
    float avg_feedback = sum / (float)signal_dim;

    bridge->state.cortical_feedback = nimcp_clampf(avg_feedback, -1.0f, 1.0f);
    bridge->state.feedback_active = true;

    /* Store feedback buffer if needed for detailed processing */
    if (bridge->feedback_buffer && bridge->feedback_dim == signal_dim) {
        memcpy(bridge->feedback_buffer, feedback_signal, signal_dim * sizeof(float));
    }

    return 0;
}

/*=============================================================================
 * Bio-Async Communication
 *===========================================================================*/

int occipital_thalamic_bridge_register_bio_async(
    occipital_thalamic_bridge_t* bridge,
    bio_router_t* router)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->bio_router = router;
    bridge->bio_async_connected = (router != NULL);

    if (router != NULL) {
        /* Would register message handlers here:
         * - BIO_MSG_ATTENTION_MODULATE
         * - BIO_MSG_VISUAL_INPUT
         * - BIO_MSG_SACCADE_COMMAND
         */
    }

    return 0;
}

int occipital_thalamic_bridge_broadcast_routing(
    occipital_thalamic_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->bio_async_connected || !bridge->bio_router) {
        return 0;
    }

    /* Would broadcast routing event message here */
    bridge->stats.bio_messages_sent++;

    return 0;
}

int occipital_thalamic_bridge_process_messages(
    occipital_thalamic_bridge_t* bridge,
    uint32_t max_messages)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->bio_async_connected || !bridge->bio_router) {
        return 0;
    }

    /* Would process pending bio-async messages here */
    (void)max_messages;

    return 0;
}

/*=============================================================================
 * Statistics API
 *===========================================================================*/

int occipital_thalamic_bridge_get_stats(
    const occipital_thalamic_bridge_t* bridge,
    occipital_thalamic_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_thalamic_bridge_get_stats: required parameter is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

void occipital_thalamic_bridge_reset_stats(occipital_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_thalamic_bridge_reset_stats: bridge is NULL");
        return;
    }
    memset(&bridge->stats, 0, sizeof(occipital_thalamic_stats_t));
}

/*=============================================================================
 * Query API
 *===========================================================================*/

bool occipital_thalamic_is_lgn_active(
    const occipital_thalamic_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->state.lgn_gain > 0.0f;
}

int occipital_thalamic_bridge_get_config(
    const occipital_thalamic_bridge_t* bridge,
    occipital_thalamic_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_thalamic_bridge_get_config: required parameter is NULL");
        return -1;
    }

    *config = bridge->config;
    return 0;
}

const char* occipital_thalamic_nucleus_name(occipital_thalamic_nucleus_t nucleus) {
    switch (nucleus) {
        case OCCIPITAL_THALAMIC_LGN:
            return "Lateral Geniculate Nucleus (LGN)";
        case OCCIPITAL_THALAMIC_PULVINAR:
            return "Pulvinar";
        case OCCIPITAL_THALAMIC_SC:
            return "Superior Colliculus";
        case OCCIPITAL_THALAMIC_TRN:
            return "Thalamic Reticular Nucleus";
        default:
            return "Unknown";
    }
}

const char* occipital_thalamic_pathway_name(lgn_pathway_type_t pathway) {
    switch (pathway) {
        case LGN_PATHWAY_MAGNOCELLULAR:
            return "Magnocellular (M-pathway)";
        case LGN_PATHWAY_PARVOCELLULAR:
            return "Parvocellular (P-pathway)";
        case LGN_PATHWAY_KONIOCELLULAR:
            return "Koniocellular (K-pathway)";
        default:
            return "Unknown";
    }
}
