/**
 * @file nimcp_snn_somatosensory_bridge.c
 * @brief SNN-Somatosensory Cortex integration bridge implementation
 *
 * WHAT: Bidirectional bridge between SNN and somatosensory cortex
 * WHY:  Enable spike-based body-state processing with population coding
 * HOW:  Cortical magnification + Gaussian tuning curves + pain priority encoding
 *
 * BIOLOGICAL BASIS:
 * - S1 cortex uses population coding for touch location and intensity
 * - Proprioceptive neurons have Gaussian tuning curves for joint angles
 * - Cortical magnification: fingertips ~15x, lips ~25x receptor density
 * - Pain pathways use burst coding (A-delta) and sustained firing (C-fiber)
 *
 * @author NIMCP Development Team
 * @date 2026-03-09
 */

#include "snn/bridges/nimcp_snn_somatosensory_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE "snn_somatosensory_bridge"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_somatosensory_bridge)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//=============================================================================
// Body segment classification for cortical magnification
//=============================================================================

/**
 * WHAT: Map body segment index to magnification region
 * WHY:  Different body regions have different cortical representation density
 * HOW:  Segments 0-4 = fingers, 5-6 = lips, 7 = tongue, 8-12 = face, rest = trunk/limbs
 */
static float get_segment_magnification(
    const snn_somatosensory_config_t* config,
    uint32_t segment)
{
    if (segment < 5)  return config->magnification_fingers;   /* Fingers (0-4) */
    if (segment < 7)  return config->magnification_lips;      /* Lips (5-6) */
    if (segment == 7) return config->magnification_tongue;    /* Tongue (7) */
    if (segment < 13) return config->magnification_face;      /* Face (8-12) */
    return 1.0f;  /* Trunk, arms, legs — baseline magnification */
}

//=============================================================================
// Default Configuration
//=============================================================================

void snn_somatosensory_config_default(snn_somatosensory_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_somatosensory_config_default: null config");
        return;
    }

    config->encoding_method       = SNN_ENCODE_RATE;
    config->max_spike_rate        = SNN_SOMA_DEFAULT_MAX_RATE;
    config->min_spike_rate        = SNN_SOMA_DEFAULT_MIN_RATE;
    config->temporal_window_ms    = SNN_SOMA_DEFAULT_TEMPORAL_WINDOW;
    config->body_segments         = SNN_SOMA_DEFAULT_BODY_SEGMENTS;
    config->neurons_per_segment   = SNN_SOMA_DEFAULT_NEURONS_PER_SEG;
    config->magnification_fingers = 15.0f;
    config->magnification_lips    = 25.0f;
    config->magnification_tongue  = 20.0f;
    config->magnification_face    = 10.0f;
    config->proprioception_dims   = 3;  /* position, velocity, tension */
    config->enable_bio_async      = false;
    config->update_interval_ms    = 10.0f;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * WHAT: Initialize Gaussian tuning curves for proprioceptive neurons
 * WHY:  Each neuron responds maximally to a preferred joint angle
 * HOW:  Distribute preferred angles evenly across [0, 2*PI]
 */
static void init_tuning_curves(snn_somatosensory_bridge_t* bridge)
{
    uint32_t nps = bridge->config.neurons_per_segment;
    uint32_t segs = bridge->config.body_segments;

    for (uint32_t s = 0; s < segs; s++) {
        for (uint32_t n = 0; n < nps; n++) {
            float fraction = (float)n / (float)nps;
            bridge->tuning_preferred_angles[s * nps + n] =
                fraction * 2.0f * (float)M_PI;
        }
    }
}

/**
 * WHAT: Build per-segment cortical magnification lookup table
 * WHY:  Avoid recomputing magnification on every encode call
 * HOW:  Map segment index to magnification factor once at init
 */
static void init_magnification_table(snn_somatosensory_bridge_t* bridge)
{
    for (uint32_t s = 0; s < bridge->config.body_segments; s++) {
        bridge->magnification_table[s] =
            get_segment_magnification(&bridge->config, s);
    }
}

snn_somatosensory_bridge_t* snn_somatosensory_bridge_create(
    const snn_somatosensory_config_t* config,
    snn_network_t* snn)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_somatosensory_bridge_create: null config");
        return NULL;
    }
    if (config->body_segments == 0 || config->body_segments > SNN_SOMA_MAX_BODY_SEGMENTS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_somatosensory_bridge_create: invalid body_segments");
        return NULL;
    }

    snn_somatosensory_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_MEMORY,
            "snn_somatosensory_bridge_create: failed to allocate bridge");
        return NULL;
    }

    bbb_register_module("snn_somatosensory_bridge", BBB_MODULE_TYPE_COGNITIVE);

    bridge->config = *config;
    bridge->snn = snn;

    uint32_t total_neurons = config->body_segments * config->neurons_per_segment;

    /* Allocate working buffers */
    bridge->receptor_buffer = nimcp_calloc(total_neurons, sizeof(float));
    bridge->proprioception_buffer = nimcp_calloc(
        config->body_segments * config->proprioception_dims, sizeof(float));
    bridge->body_map_buffer = nimcp_calloc(config->body_segments, sizeof(float));
    bridge->magnification_table = nimcp_calloc(config->body_segments, sizeof(float));
    bridge->tuning_preferred_angles = nimcp_calloc(total_neurons, sizeof(float));

    if (!bridge->receptor_buffer || !bridge->proprioception_buffer ||
        !bridge->body_map_buffer || !bridge->magnification_table ||
        !bridge->tuning_preferred_angles) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_MEMORY,
            "snn_somatosensory_bridge_create: failed to allocate working buffers");
        snn_somatosensory_bridge_destroy(bridge);
        return NULL;
    }

    /* Create encoder (rate coding, one per neuron) */
    {
        snn_rate_encoder_config_t enc_cfg;
        snn_rate_encoder_config_default(&enc_cfg);
        enc_cfg.min_rate = config->min_spike_rate;
        enc_cfg.max_rate = config->max_spike_rate;
        bridge->encoder = snn_encoder_create_rate(total_neurons, &enc_cfg);
    }

    /* Initialize bridge base */
    bridge_base_init(&bridge->base, BIO_MODULE_SNN_SOMATOSENSORY,
                     "snn_somatosensory_bridge");

    /* Initialize tuning curves and magnification */
    init_tuning_curves(bridge);
    init_magnification_table(bridge);

    bridge->connected = true;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    LOG_INFO(LOG_MODULE, "Created: segments=%u, neurons/seg=%u, total=%u",
             config->body_segments, config->neurons_per_segment, total_neurons);
    return bridge;
}

void snn_somatosensory_bridge_destroy(snn_somatosensory_bridge_t* bridge)
{
    if (!bridge) return;

    if (bridge->encoder) snn_encoder_destroy(bridge->encoder);
    if (bridge->decoder) snn_decoder_destroy(bridge->decoder);

    nimcp_free(bridge->receptor_buffer);
    nimcp_free(bridge->proprioception_buffer);
    nimcp_free(bridge->body_map_buffer);
    nimcp_free(bridge->magnification_table);
    nimcp_free(bridge->tuning_preferred_angles);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int snn_somatosensory_bridge_connect_bio_async(snn_somatosensory_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_somatosensory_bridge_connect_bio_async: null bridge");
        return -1;
    }

    bio_module_info_t info = {
        .module_id     = BIO_MODULE_SNN_SOMATOSENSORY,
        .module_name   = "SNN_Somatosensory",
        .inbox_capacity = 128,
        .user_data     = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    bridge->bio_async_enabled = (bridge->bio_ctx != NULL);

    return bridge->bio_async_enabled ? 0 : -1;
}

int snn_somatosensory_bridge_disconnect_bio_async(snn_somatosensory_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_somatosensory_bridge_disconnect_bio_async: null bridge");
        return -1;
    }
    bridge->bio_async_enabled = false;
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * WHAT: Clamp a float value to [lo, hi]
 * WHY:  Ensure spike rates stay within valid range
 */
static inline float clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

int snn_somatosensory_bridge_encode_touch(
    snn_somatosensory_bridge_t* bridge,
    uint32_t body_segment,
    float intensity,
    float velocity,
    float texture)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_somatosensory_bridge_encode_touch: null bridge");
        return -1;
    }
    if (body_segment >= bridge->config.body_segments) return -1;

    uint32_t nps = bridge->config.neurons_per_segment;
    float mag = bridge->magnification_table[body_segment];
    float base_rate = bridge->config.min_spike_rate;
    float max_rate  = bridge->config.max_spike_rate;

    /* Combined stimulus: intensity dominant, velocity/texture modulate */
    float stimulus = clampf(intensity, 0.0f, 1.0f) * 0.6f
                   + clampf(velocity, 0.0f, 1.0f) * 0.2f
                   + clampf(texture, 0.0f, 1.0f) * 0.2f;

    /* Scale by cortical magnification — more cortex = higher rate */
    float scaled_rate = base_rate + stimulus * (max_rate - base_rate);
    scaled_rate *= (1.0f + logf(mag) / logf(25.0f));  /* log-scale magnification */
    scaled_rate = clampf(scaled_rate, base_rate, max_rate * 2.0f);

    /* Distribute across population: center neurons strongest */
    uint32_t offset = body_segment * nps;
    float center = (float)(nps - 1) / 2.0f;
    for (uint32_t n = 0; n < nps; n++) {
        float dist = fabsf((float)n - center) / center;
        float rate = scaled_rate * (1.0f - 0.5f * dist * dist);
        bridge->receptor_buffer[offset + n] = rate;
    }

    /* Update body map */
    bridge->body_map_buffer[body_segment] = stimulus;

    /* Update stats */
    bridge->stats.touch_events_encoded++;
    bridge->stats.total_spikes += nps;
    if (scaled_rate > bridge->stats.peak_spike_rate) {
        bridge->stats.peak_spike_rate = scaled_rate;
    }

    return 0;
}

int snn_somatosensory_bridge_encode_proprioception(
    snn_somatosensory_bridge_t* bridge,
    uint32_t segment,
    float position,
    float velocity,
    float muscle_tension)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_somatosensory_bridge_encode_proprioception: null bridge");
        return -1;
    }
    if (segment >= bridge->config.body_segments) return -1;

    uint32_t nps = bridge->config.neurons_per_segment;
    float base_rate = bridge->config.min_spike_rate;
    float max_rate  = bridge->config.max_spike_rate;

    /* Store raw proprioceptive values */
    uint32_t pd = bridge->config.proprioception_dims;
    bridge->proprioception_buffer[segment * pd + 0] = position;
    if (pd > 1) bridge->proprioception_buffer[segment * pd + 1] = velocity;
    if (pd > 2) bridge->proprioception_buffer[segment * pd + 2] = muscle_tension;

    /* Gaussian tuning: each neuron fires maximally at its preferred angle */
    uint32_t offset = segment * nps;
    float sigma = (float)M_PI / (float)nps;  /* Tuning width */

    for (uint32_t n = 0; n < nps; n++) {
        float preferred = bridge->tuning_preferred_angles[offset + n];
        /* Circular distance on [0, 2*PI] */
        float diff = fabsf(position - preferred);
        if (diff > (float)M_PI) diff = 2.0f * (float)M_PI - diff;

        float gaussian = expf(-0.5f * (diff * diff) / (sigma * sigma));

        /* Velocity modulates overall gain */
        float vel_gain = 1.0f + 0.3f * clampf(fabsf(velocity), 0.0f, 5.0f) / 5.0f;
        /* Tension adds baseline boost */
        float tension_boost = 0.2f * clampf(muscle_tension, 0.0f, 1.0f);

        float rate = base_rate + gaussian * (max_rate - base_rate) * vel_gain + tension_boost * max_rate;
        bridge->receptor_buffer[offset + n] = clampf(rate, base_rate, max_rate * 1.5f);
    }

    bridge->stats.proprioception_updates++;

    return 0;
}

int snn_somatosensory_bridge_encode_pain(
    snn_somatosensory_bridge_t* bridge,
    uint32_t segment,
    float intensity,
    snn_soma_pain_type_t type)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_somatosensory_bridge_encode_pain: null bridge");
        return -1;
    }
    if (segment >= bridge->config.body_segments) return -1;

    uint32_t nps = bridge->config.neurons_per_segment;
    float max_rate = bridge->config.max_spike_rate;
    float clamped_intensity = clampf(intensity, 0.0f, 1.0f);
    uint32_t offset = segment * nps;

    /* Pain type determines firing pattern */
    float rate_multiplier;
    float spread;

    switch (type) {
    case SNN_SOMA_PAIN_SHARP:
        /* A-delta: high-rate burst, narrow spatial spread */
        rate_multiplier = 2.0f;  /* Double max rate for priority */
        spread = 0.3f;           /* Narrow — well-localized */
        break;
    case SNN_SOMA_PAIN_DULL:
        /* C-fiber: sustained moderate rate, wide spread */
        rate_multiplier = 1.2f;
        spread = 0.8f;           /* Wide — diffuse */
        break;
    case SNN_SOMA_PAIN_BURNING:
        /* Thermal: moderate-high rate, moderate spread */
        rate_multiplier = 1.5f;
        spread = 0.5f;
        break;
    default:
        rate_multiplier = 1.0f;
        spread = 0.5f;
        break;
    }

    float pain_rate = max_rate * rate_multiplier * clamped_intensity;
    float center = (float)(nps - 1) / 2.0f;

    for (uint32_t n = 0; n < nps; n++) {
        float dist = fabsf((float)n - center) / center;
        float spatial_weight = expf(-dist * dist / (2.0f * spread * spread));
        bridge->receptor_buffer[offset + n] = pain_rate * spatial_weight;
    }

    bridge->stats.pain_events_encoded++;
    bridge->stats.total_spikes += nps;
    if (pain_rate > bridge->stats.peak_spike_rate) {
        bridge->stats.peak_spike_rate = pain_rate;
    }

    return 0;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int snn_somatosensory_bridge_decode_body_state(
    snn_somatosensory_bridge_t* bridge,
    const float* spike_rates,
    float* position_out,
    float* velocity_out)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_somatosensory_bridge_decode_body_state: null bridge");
        return -1;
    }
    if (!spike_rates || !position_out) return -1;

    uint32_t nps = bridge->config.neurons_per_segment;
    uint32_t segs = bridge->config.body_segments;

    for (uint32_t s = 0; s < segs; s++) {
        uint32_t offset = s * nps;

        /* Population vector decoding: weighted sum of preferred angles */
        float sin_sum = 0.0f;
        float cos_sum = 0.0f;
        float rate_sum = 0.0f;

        for (uint32_t n = 0; n < nps; n++) {
            float rate = spike_rates[offset + n];
            float pref = bridge->tuning_preferred_angles[offset + n];
            sin_sum += rate * sinf(pref);
            cos_sum += rate * cosf(pref);
            rate_sum += rate;
        }

        /* Circular mean via atan2 */
        if (rate_sum > 1e-6f) {
            position_out[s] = atan2f(sin_sum, cos_sum);
            if (position_out[s] < 0.0f) {
                position_out[s] += 2.0f * (float)M_PI;
            }
        } else {
            position_out[s] = 0.0f;
        }

        /* Velocity estimate from rate magnitude (heuristic) */
        if (velocity_out) {
            velocity_out[s] = rate_sum / (float)nps;
        }
    }

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int snn_somatosensory_bridge_get_stats(
    const snn_somatosensory_bridge_t* bridge,
    snn_somatosensory_encode_stats_t* stats)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "snn_somatosensory_bridge_get_stats: null bridge");
        return -1;
    }
    if (!stats) return -1;

    *stats = bridge->stats;

    /* Compute derived averages */
    uint64_t total_events = bridge->stats.touch_events_encoded
                          + bridge->stats.proprioception_updates
                          + bridge->stats.pain_events_encoded;
    if (total_events > 0) {
        stats->avg_spikes_per_event =
            (float)bridge->stats.total_spikes / (float)total_events;
    }

    return 0;
}
