/**
 * @file nimcp_cochlea_thalamic_bridge.c
 * @brief Cochlea-Thalamus (MGN) integration implementation
 *
 * WHAT: Connect cochlear output to thalamic relay (Medial Geniculate Nucleus)
 * WHY:  Enable attention gating and cortical routing of auditory information
 * HOW:  MGN relay with TRN (thalamic reticular nucleus) attention control
 *
 * BIOLOGICAL BASIS:
 * - MGN receives ascending auditory input from inferior colliculus
 * - TRN provides inhibitory gating based on attention state
 * - Tonic mode: Faithful relay for attended sounds
 * - Burst mode: Attention-grabbing for novel/salient sounds
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/bridges/nimcp_cochlea_thalamic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_thalamic_bridge)

#define LOG_MODULE "COCHLEA_THALAMIC_BRIDGE"

//=============================================================================
// Internal Structure
//=============================================================================

/** Bridge statistics */
typedef struct {
    uint64_t total_updates;
    uint64_t burst_events;
    float avg_relay_strength;
} cochlea_thalamic_stats_t;

/** Bridge internal structure */
struct cochlea_thalamic_bridge {
    bridge_base_t base;                         /**< MUST be first */
    cochlea_t* cochlea;                         /**< Cochlea instance */
    thalamus_t* thalamus;                       /**< Thalamus instance */
    cochlea_thalamic_config_t config;           /**< Bridge configuration */
    cochlea_thalamic_stats_t stats;             /**< Runtime statistics */

    /* MGN relay state */
    float* relay_activation;                    /**< Per-channel relay [num_channels] */
    mgn_mode_t* channel_mode;                   /**< Per-channel mode [num_channels] */
    float* burst_strength;                      /**< Per-channel burst [num_channels] */
    float* burst_timer;                         /**< Burst duration timer [num_channels] */

    /* MGN output cache */
    mgn_output_t output;                        /**< Current output */

    /* Attention state */
    attention_state_t attention;                /**< Current attention */

    /* Bidirectional timestamps */
    uint64_t last_outbound_ts;                  /**< Last outbound timestamp */
    uint64_t last_inbound_ts;                   /**< Last inbound timestamp */
};

//=============================================================================
// Helper: Current time in ms
//=============================================================================

static uint64_t cochlea_thalamic_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

//=============================================================================
// Configuration Helpers
//=============================================================================

cochlea_thalamic_config_t cochlea_thalamic_config_default(void) {
    cochlea_thalamic_config_t config;
    memset(&config, 0, sizeof(config));
    config.num_mgn_channels = COCHLEA_THALAMIC_MGN_CHANNELS;
    config.relay_latency_ms = COCHLEA_THALAMIC_RELAY_LATENCY_MS;
    config.burst_threshold = 0.8f;
    config.burst_duration_ms = 20.0f;
    config.attention_bandwidth = COCHLEA_THALAMIC_ATTN_BANDWIDTH;
    config.max_attention_gain_db = COCHLEA_THALAMIC_ATTN_GAIN_DB;
    config.enable_spatial_attention = false;
    config.trn_inhibition_strength = 0.5f;
    config.trn_decay_ms = 50.0f;
    return config;
}

//=============================================================================
// Helper: Allocate per-channel arrays
//=============================================================================

static int cochlea_thalamic_alloc_arrays(cochlea_thalamic_bridge_t* bridge, uint32_t n) {
    bridge->relay_activation = (float*)nimcp_calloc(n, sizeof(float));
    bridge->channel_mode = (mgn_mode_t*)nimcp_calloc(n, sizeof(mgn_mode_t));
    bridge->burst_strength = (float*)nimcp_calloc(n, sizeof(float));
    bridge->burst_timer = (float*)nimcp_calloc(n, sizeof(float));

    bridge->attention.channel_weights = (float*)nimcp_calloc(n, sizeof(float));
    bridge->attention.trn_inhibition = (float*)nimcp_calloc(n, sizeof(float));
    bridge->attention.num_channels = n;

    if (!bridge->relay_activation || !bridge->channel_mode ||
        !bridge->burst_strength || !bridge->burst_timer ||
        !bridge->attention.channel_weights || !bridge->attention.trn_inhibition) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_thalamic_alloc_arrays: operation failed");
        return -1;
    }

    /* Initialize attention weights to uniform 1.0 */
    for (uint32_t i = 0; i < n; i++) {
        bridge->attention.channel_weights[i] = 1.0f;
    }

    return 0;
}

static void cochlea_thalamic_free_arrays(cochlea_thalamic_bridge_t* bridge) {
    if (bridge->relay_activation) { nimcp_free(bridge->relay_activation); bridge->relay_activation = NULL; }
    if (bridge->channel_mode) { nimcp_free(bridge->channel_mode); bridge->channel_mode = NULL; }
    if (bridge->burst_strength) { nimcp_free(bridge->burst_strength); bridge->burst_strength = NULL; }
    if (bridge->burst_timer) { nimcp_free(bridge->burst_timer); bridge->burst_timer = NULL; }
    if (bridge->attention.channel_weights) { nimcp_free(bridge->attention.channel_weights); bridge->attention.channel_weights = NULL; }
    if (bridge->attention.trn_inhibition) { nimcp_free(bridge->attention.trn_inhibition); bridge->attention.trn_inhibition = NULL; }
}

//=============================================================================
// Core API
//=============================================================================

cochlea_thalamic_bridge_t* cochlea_thalamic_bridge_create(
    cochlea_t* cochlea,
    thalamus_t* thalamus,
    const cochlea_thalamic_config_t* config)
{
    cochlea_thalamic_bridge_t* bridge = (cochlea_thalamic_bridge_t*)nimcp_calloc(1, sizeof(cochlea_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_thalamic_bridge_create: alloc failed");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cochlea_thalamic_config_default();
    }

    if (bridge_base_init(&bridge->base, 0, "cochlea_thalamic_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cochlea_thalamic_bridge_create: validation failed");
        return NULL;
    }

    bridge->cochlea = cochlea;
    bridge->thalamus = thalamus;

    /* Connect systems to base */
    if (cochlea) {
        bridge_base_connect_a_unlocked(&bridge->base, cochlea);
    }
    if (thalamus) {
        bridge_base_connect_b_unlocked(&bridge->base, thalamus);
    }

    /* Allocate per-channel arrays */
    uint32_t n = bridge->config.num_mgn_channels;
    if (cochlea_thalamic_alloc_arrays(bridge, n) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_thalamic_bridge_create: array alloc failed");
        bridge_base_cleanup(&bridge->base);
        cochlea_thalamic_free_arrays(bridge);
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize output cache */
    bridge->output.relay_activation = bridge->relay_activation;
    bridge->output.channel_mode = bridge->channel_mode;
    bridge->output.burst_strength = bridge->burst_strength;
    bridge->output.num_channels = n;
    bridge->output.total_relay_strength = 0.0f;
    bridge->output.attention_event = false;
    bridge->output.peak_frequency_hz = 0.0f;

    /* Initialize attention state */
    bridge->attention.source = ATTN_SOURCE_BOTTOM_UP;
    bridge->attention.attended_freq_hz = 1000.0f;
    bridge->attention.attention_bandwidth = bridge->config.attention_bandwidth;
    bridge->attention.attention_gain_db = 0.0f;
    bridge->attention.attended_azimuth = 0.0f;
    bridge->attention.spatial_bandwidth = 60.0f;

    cochlea_thalamic_bridge_heartbeat("create", 1.0f);
    return bridge;
}

void cochlea_thalamic_bridge_destroy(cochlea_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cochlea_thalamic");
    cochlea_thalamic_bridge_heartbeat("destroy", 0.0f);
    cochlea_thalamic_free_arrays(bridge);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

nimcp_error_t cochlea_thalamic_bridge_update(
    cochlea_thalamic_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_thalamic_bridge_update: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!cochlea_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_thalamic_bridge_update: cochlea_output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_thalamic_bridge_heartbeat("update", 0.1f);

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t n = bridge->config.num_mgn_channels;
    float total = 0.0f;
    float peak_val = 0.0f;
    uint32_t peak_ch = 0;
    bool burst_event = false;

    for (uint32_t i = 0; i < n; i++) {
        /* Compute base relay activation from attention weight and TRN inhibition */
        float attn = bridge->attention.channel_weights[i];
        float trn_inhib = bridge->attention.trn_inhibition[i];
        float relay = attn * (1.0f - trn_inhib);

        /* Clamp relay to [0, 1] */
        if (relay < 0.0f) relay = 0.0f;
        if (relay > 1.0f) relay = 1.0f;

        bridge->relay_activation[i] = relay;
        total += relay;

        /* Track peak */
        if (relay > peak_val) {
            peak_val = relay;
            peak_ch = i;
        }

        /* Update burst timer */
        if (bridge->burst_timer[i] > 0.0f) {
            bridge->burst_timer[i] -= dt_ms;
            if (bridge->burst_timer[i] <= 0.0f) {
                bridge->burst_timer[i] = 0.0f;
                bridge->channel_mode[i] = MGN_MODE_TONIC;
                bridge->burst_strength[i] = 0.0f;
            }
        }

        /* Check burst threshold */
        if (bridge->channel_mode[i] != MGN_MODE_BURST &&
            relay >= bridge->config.burst_threshold) {
            bridge->channel_mode[i] = MGN_MODE_BURST;
            bridge->burst_strength[i] = relay;
            bridge->burst_timer[i] = bridge->config.burst_duration_ms;
            burst_event = true;
            bridge->stats.burst_events++;
        }

        /* Check suppression from TRN */
        if (trn_inhib > 0.9f) {
            bridge->channel_mode[i] = MGN_MODE_SUPPRESSED;
        }

        /* Decay TRN inhibition */
        if (bridge->config.trn_decay_ms > 0.0f) {
            float decay = dt_ms / bridge->config.trn_decay_ms;
            if (decay > 1.0f) decay = 1.0f;  /* Clamp to prevent sign flip */
            float new_inhib = bridge->attention.trn_inhibition[i] * (1.0f - decay);
            if (isfinite(new_inhib) && new_inhib >= 0.001f) {
                bridge->attention.trn_inhibition[i] = new_inhib;
            } else {
                bridge->attention.trn_inhibition[i] = 0.0f;
            }
        }
    }

    /* Update output cache */
    bridge->output.total_relay_strength = total;
    bridge->output.attention_event = burst_event;
    /* Estimate peak frequency from channel index (linear mapping across audible range) */
    bridge->output.peak_frequency_hz = 20.0f + (float)peak_ch / (float)(n > 1 ? n - 1 : 1) * (20000.0f - 20.0f);

    /* Update timestamps */
    bridge->last_outbound_ts = cochlea_thalamic_time_ms();

    /* Update stats */
    bridge->stats.total_updates++;
    float alpha = 0.05f;
    float new_avg = (1.0f - alpha) * bridge->stats.avg_relay_strength + alpha * total;
    if (isfinite(new_avg)) {
        bridge->stats.avg_relay_strength = new_avg;
    }

    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_thalamic_bridge_heartbeat("update", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_thalamic_bridge_reset(cochlea_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_thalamic_bridge_reset: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t n = bridge->config.num_mgn_channels;
    memset(bridge->relay_activation, 0, n * sizeof(float));
    memset(bridge->burst_strength, 0, n * sizeof(float));
    memset(bridge->burst_timer, 0, n * sizeof(float));
    memset(bridge->attention.trn_inhibition, 0, n * sizeof(float));
    for (uint32_t i = 0; i < n; i++) {
        bridge->channel_mode[i] = MGN_MODE_TONIC;
        bridge->attention.channel_weights[i] = 1.0f;
    }

    bridge->output.total_relay_strength = 0.0f;
    bridge->output.attention_event = false;
    bridge->output.peak_frequency_hz = 0.0f;

    memset(&bridge->stats, 0, sizeof(cochlea_thalamic_stats_t));
    bridge->last_outbound_ts = 0;
    bridge->last_inbound_ts = 0;

    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_thalamic_bridge_heartbeat("reset", 1.0f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Attention Control
//=============================================================================

nimcp_error_t cochlea_thalamic_set_frequency_attention(
    cochlea_thalamic_bridge_t* bridge,
    float frequency_hz,
    float bandwidth,
    float gain_db)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_thalamic_set_frequency_attention: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->attention.source = ATTN_SOURCE_FEATURE;
    bridge->attention.attended_freq_hz = frequency_hz;
    bridge->attention.attention_bandwidth = bandwidth;
    if (gain_db > bridge->config.max_attention_gain_db) {
        gain_db = bridge->config.max_attention_gain_db;
    }
    bridge->attention.attention_gain_db = gain_db;

    /* Compute per-channel attention weights based on distance from attended frequency */
    uint32_t n = bridge->config.num_mgn_channels;
    float gain_linear = powf(10.0f, gain_db / 20.0f);
    for (uint32_t i = 0; i < n; i++) {
        float ch_freq = 20.0f + (float)i / (float)(n > 1 ? n - 1 : 1) * (20000.0f - 20.0f);
        float octave_dist = fabsf(log2f(ch_freq / frequency_hz));
        float weight;
        if (bandwidth > 0.0f) {
            weight = expf(-(octave_dist * octave_dist) / (2.0f * bandwidth * bandwidth));
        } else {
            weight = (octave_dist < 0.01f) ? 1.0f : 0.0f;
        }
        bridge->attention.channel_weights[i] = 1.0f + (gain_linear - 1.0f) * weight;
    }

    bridge->last_inbound_ts = cochlea_thalamic_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_thalamic_bridge_heartbeat("set_frequency_attention", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_thalamic_set_spatial_attention(
    cochlea_thalamic_bridge_t* bridge,
    float azimuth_deg,
    float bandwidth)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_thalamic_set_spatial_attention: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->attention.source = ATTN_SOURCE_SPATIAL;
    bridge->attention.attended_azimuth = azimuth_deg;
    bridge->attention.spatial_bandwidth = bandwidth;
    bridge->last_inbound_ts = cochlea_thalamic_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_thalamic_bridge_heartbeat("set_spatial_attention", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_thalamic_clear_attention(cochlea_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_thalamic_clear_attention: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->attention.source = ATTN_SOURCE_BOTTOM_UP;
    bridge->attention.attention_gain_db = 0.0f;
    uint32_t n = bridge->config.num_mgn_channels;
    for (uint32_t i = 0; i < n; i++) {
        bridge->attention.channel_weights[i] = 1.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_thalamic_bridge_heartbeat("clear_attention", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_thalamic_get_attention(
    const cochlea_thalamic_bridge_t* bridge,
    attention_state_t* state)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_thalamic_get_attention: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_thalamic_get_attention: state is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    state->source = bridge->attention.source;
    state->attended_freq_hz = bridge->attention.attended_freq_hz;
    state->attention_bandwidth = bridge->attention.attention_bandwidth;
    state->attention_gain_db = bridge->attention.attention_gain_db;
    state->attended_azimuth = bridge->attention.attended_azimuth;
    state->spatial_bandwidth = bridge->attention.spatial_bandwidth;
    state->num_channels = bridge->attention.num_channels;
    /* Caller-provided pointers point to internal data (read-only view) */
    state->channel_weights = bridge->attention.channel_weights;
    state->trn_inhibition = bridge->attention.trn_inhibition;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// MGN Access
//=============================================================================

nimcp_error_t cochlea_thalamic_get_mgn_output(
    const cochlea_thalamic_bridge_t* bridge,
    mgn_output_t* output)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_thalamic_get_mgn_output: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_thalamic_get_mgn_output: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    output->relay_activation = bridge->output.relay_activation;
    output->channel_mode = bridge->output.channel_mode;
    output->burst_strength = bridge->output.burst_strength;
    output->num_channels = bridge->output.num_channels;
    output->total_relay_strength = bridge->output.total_relay_strength;
    output->attention_event = bridge->output.attention_event;
    output->peak_frequency_hz = bridge->output.peak_frequency_hz;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_thalamic_trigger_burst(
    cochlea_thalamic_bridge_t* bridge,
    uint32_t channel)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_thalamic_trigger_burst: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (channel >= bridge->config.num_mgn_channels) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    bridge->channel_mode[channel] = MGN_MODE_BURST;
    bridge->burst_strength[channel] = 1.0f;
    bridge->burst_timer[channel] = bridge->config.burst_duration_ms;
    bridge->stats.burst_events++;

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_thalamic_bridge_heartbeat("trigger_burst", 1.0f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_thalamic_verify_bidirectional(
    const cochlea_thalamic_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_thalamic_verify_bidirectional: bridge is NULL");
        return false;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bool result = (bridge->last_outbound_ts > 0 && bridge->last_inbound_ts > 0);
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

uint64_t cochlea_thalamic_get_last_outbound(
    const cochlea_thalamic_bridge_t* bridge)
{
    if (!bridge) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_outbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}

uint64_t cochlea_thalamic_get_last_inbound(
    const cochlea_thalamic_bridge_t* bridge)
{
    if (!bridge) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_inbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}
