/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * calcium_pink_noise_bridge_create has zero callers in production
 * code. Wrapper around pink_noise_create that is unused. Either wire
 * a consumer or delete before the next major version. Do not extend.
 */

/**
 * @file nimcp_calcium_pink_noise_bridge.c
 * @brief Calcium Dynamics - Pink Noise Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-21
 */

#include "plasticity/calcium/nimcp_calcium_pink_noise_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(calcium_pink_noise_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(calcium_pink_noise_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int calcium_pink_noise_default_config(calcium_pink_noise_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_default_config: config is NULL");
        return -1;
    }

    /* Pink noise defaults */
    config->pink_noise_alpha = CALCIUM_PINK_NOISE_ALPHA_DEFAULT;
    config->pink_noise_amplitude = CALCIUM_PINK_NOISE_AMPLITUDE_DEFAULT;
    config->pink_noise_min_freq = CALCIUM_PINK_NOISE_MIN_FREQ_DEFAULT;
    config->pink_noise_max_freq = CALCIUM_PINK_NOISE_MAX_FREQ_DEFAULT;
    config->pink_noise_sample_rate = CALCIUM_PINK_NOISE_SAMPLE_RATE_DEFAULT;
    config->pink_noise_seed = 0;  /* Time-based */

    /* Modulation configuration */
    config->mode = CALCIUM_PINK_NOISE_MODE_COMPREHENSIVE;
    config->influx_modulation_strength = CALCIUM_PINK_NOISE_INFLUX_STRENGTH;
    config->transient_modulation_strength = CALCIUM_PINK_NOISE_TRANSIENT_STRENGTH;
    config->decay_modulation_strength = CALCIUM_PINK_NOISE_DECAY_STRENGTH;

    /* Calcium-dependent scaling */
    config->enable_ca_dependent_amplitude = true;
    config->ca_low_scale = CALCIUM_PINK_NOISE_LOW_CA_SCALE;
    config->ca_high_scale = CALCIUM_PINK_NOISE_HIGH_CA_SCALE;

    /* Integration enables */
    config->enable_noise_modulation = true;

    return 0;
}

calcium_pink_noise_bridge_t* calcium_pink_noise_bridge_create(
    const calcium_pink_noise_config_t* config
) {
    /* Use defaults if no config provided */
    calcium_pink_noise_config_t default_config;
    if (!config) {
        calcium_pink_noise_default_config(&default_config);
        config = &default_config;
    }

    /* Validate configuration */
    if (calcium_pink_noise_validate_config(config) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "calcium_pink_noise_bridge_create: invalid configuration");
        return NULL;
    }

    /* Allocate bridge structure */
    calcium_pink_noise_bridge_t* bridge = (calcium_pink_noise_bridge_t*)
        nimcp_malloc(sizeof(calcium_pink_noise_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "calcium_pink_noise_bridge_create: bridge allocation failed");
        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(calcium_pink_noise_bridge_t));

    /* Copy configuration */
    memcpy(&bridge->config, config, sizeof(calcium_pink_noise_config_t));

    /* Create pink noise generator */
    pink_noise_config_t pn_config;
    pn_config.alpha = config->pink_noise_alpha;
    pn_config.amplitude = config->pink_noise_amplitude;
    pn_config.min_frequency = config->pink_noise_min_freq;
    pn_config.max_frequency = config->pink_noise_max_freq;
    pn_config.sample_rate = config->pink_noise_sample_rate;
    pn_config.method = PINK_NOISE_VOSS;  /* Fast, good quality */
    pn_config.seed = config->pink_noise_seed;

    bridge->noise_gen = pink_noise_create(&pn_config);
    if (!bridge->noise_gen) {
        nimcp_free(bridge);
        LOG_ERROR("Pink noise generator creation failed for calcium");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Pink noise generator creation failed for calcium");
        return NULL;
    }

    /* Initialize bridge base (allocates mutex, sets module name) */
    if (bridge_base_init(&bridge->base, 0, "calcium_pink_noise") != 0) {
        pink_noise_destroy(bridge->noise_gen);
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize state */
    bridge->noise_enabled = config->enable_noise_modulation;
    bridge->calcium_connected = false;

    /* Initialize effects to neutral */
    bridge->effects.influx_modulation = 1.0f;
    bridge->effects.transient_modulation = 1.0f;
    bridge->effects.decay_modulation = 1.0f;
    bridge->effects.effective_amplitude = config->pink_noise_amplitude;

    NIMCP_LOGGING_INFO("Calcium-pink noise bridge created");
    return bridge;
}

void calcium_pink_noise_bridge_destroy(calcium_pink_noise_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge) return;

    /* Destroy pink noise generator */
    if (bridge->noise_gen) {
        pink_noise_destroy(bridge->noise_gen);
    }

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Calcium-pink noise bridge destroyed");
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int calcium_pink_noise_connect_calcium(
    calcium_pink_noise_bridge_t* bridge,
    calcium_dynamics_t calcium
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_connect_calcium: bridge is NULL");
        return -1;
    }
    if (!calcium) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_connect_calcium: calcium is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Store calcium pointer */
    bridge->calcium = calcium;
    bridge->calcium_connected = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Calcium system connected to pink noise bridge");
    return 0;
}

int calcium_pink_noise_disconnect_calcium(calcium_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_disconnect_calcium: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->calcium = NULL;
    bridge->calcium_connected = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Calcium system disconnected from pink noise bridge");
    return 0;
}

/* ============================================================================
 * Enable/Disable API Implementation
 * ============================================================================ */

int calcium_pink_noise_enable(calcium_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_enable: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->noise_enabled = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Pink noise modulation enabled");
    return 0;
}

int calcium_pink_noise_disable(calcium_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_disable: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->noise_enabled = false;

    /* Reset modulation to neutral */
    bridge->effects.influx_modulation = 1.0f;
    bridge->effects.transient_modulation = 1.0f;
    bridge->effects.decay_modulation = 1.0f;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Pink noise modulation disabled");
    return 0;
}

bool calcium_pink_noise_is_enabled(const calcium_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->noise_enabled;
}

/* ============================================================================
 * Update API Implementation (Pink Noise → Calcium)
 * ============================================================================ */

/**
 * @brief Compute modulation factors from noise sample
 *
 * WHAT: Apply mode-specific modulation based on noise sample
 * WHY:  Different modes target different calcium variability sources
 * HOW:  Check mode, scale by strength, clamp to safe range
 */
static void compute_modulation_factors(
    calcium_pink_noise_bridge_t* bridge,
    float noise_sample
) {
    calcium_pink_noise_mode_t mode = bridge->config.mode;

    /* INFLUX modulation */
    if (mode == CALCIUM_PINK_NOISE_MODE_INFLUX ||
        mode == CALCIUM_PINK_NOISE_MODE_COMPREHENSIVE) {
        float strength = bridge->config.influx_modulation_strength;
        bridge->effects.influx_modulation = 1.0f + strength * noise_sample;
        bridge->effects.influx_modulation = nimcp_clampf(
            bridge->effects.influx_modulation, 0.5f, 1.5f
        );
    } else {
        bridge->effects.influx_modulation = 1.0f;
    }

    /* TRANSIENT modulation */
    if (mode == CALCIUM_PINK_NOISE_MODE_TRANSIENT ||
        mode == CALCIUM_PINK_NOISE_MODE_COMPREHENSIVE) {
        float strength = bridge->config.transient_modulation_strength;
        bridge->effects.transient_modulation = 1.0f + strength * noise_sample;
        bridge->effects.transient_modulation = nimcp_clampf(
            bridge->effects.transient_modulation, 0.5f, 1.5f
        );
    } else {
        bridge->effects.transient_modulation = 1.0f;
    }

    /* DECAY modulation */
    if (mode == CALCIUM_PINK_NOISE_MODE_DECAY ||
        mode == CALCIUM_PINK_NOISE_MODE_COMPREHENSIVE) {
        float strength = bridge->config.decay_modulation_strength;
        bridge->effects.decay_modulation = 1.0f + strength * noise_sample;
        bridge->effects.decay_modulation = nimcp_clampf(
            bridge->effects.decay_modulation, 0.5f, 1.5f
        );
    } else {
        bridge->effects.decay_modulation = 1.0f;
    }
}

/**
 * @brief Update statistics with new noise sample
 *
 * WHAT: Track running averages and min/max of noise and modulation
 * WHY:  Monitor noise characteristics over time
 * HOW:  Exponential moving average and extrema tracking
 */
static void update_statistics(
    calcium_pink_noise_bridge_t* bridge,
    float noise_sample
) {
    bridge->stats.total_updates++;
    bridge->stats.noise_samples_generated++;

    /* Update running averages (exponential moving average) */
    float alpha = 0.01f;
    bridge->stats.avg_noise_amplitude =
        alpha * fabsf(noise_sample) + (1.0f - alpha) * bridge->stats.avg_noise_amplitude;
    bridge->stats.avg_influx_modulation =
        alpha * bridge->effects.influx_modulation + (1.0f - alpha) * bridge->stats.avg_influx_modulation;
    bridge->stats.avg_transient_modulation =
        alpha * bridge->effects.transient_modulation + (1.0f - alpha) * bridge->stats.avg_transient_modulation;
    bridge->stats.avg_decay_modulation =
        alpha * bridge->effects.decay_modulation + (1.0f - alpha) * bridge->stats.avg_decay_modulation;

    /* Update min/max */
    if (fabsf(noise_sample) > bridge->stats.max_noise_amplitude) {
        bridge->stats.max_noise_amplitude = fabsf(noise_sample);
    }
    if (bridge->stats.total_updates == 1 || fabsf(noise_sample) < bridge->stats.min_noise_amplitude) {
        bridge->stats.min_noise_amplitude = fabsf(noise_sample);
    }
}

int calcium_pink_noise_update(calcium_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Skip if noise disabled */
    if (!bridge->noise_enabled) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Generate new pink noise sample */
    float noise_sample;
    if (!pink_noise_generate_sample(bridge->noise_gen, &noise_sample)) {
        NIMCP_LOGGING_ERROR("Failed to generate pink noise sample");
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "calcium_pink_noise_update: pink_noise_generate_sample is NULL");
        return -1;
    }

    /* Store current noise sample */
    bridge->effects.current_noise_sample = noise_sample;

    /* Update calcium-dependent amplitude if enabled */
    if (bridge->config.enable_ca_dependent_amplitude && bridge->calcium_connected) {
        calcium_pink_noise_update_ca_dependent_amplitude(bridge);
    }

    /* Compute modulation factors and update statistics */
    compute_modulation_factors(bridge, noise_sample);
    update_statistics(bridge, noise_sample);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

float calcium_pink_noise_modulate_influx(
    const calcium_pink_noise_bridge_t* bridge,
    float nmda_activation
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_modulate_influx: bridge is NULL");
        return nmda_activation;
    }
    if (!bridge->noise_enabled) return nmda_activation;

    /* Apply influx modulation */
    float modulated = nmda_activation * bridge->effects.influx_modulation;
    return nimcp_clampf(modulated, 0.0f, 1.0f);
}

float calcium_pink_noise_modulate_transient(
    const calcium_pink_noise_bridge_t* bridge,
    float ca_amplitude
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_modulate_transient: bridge is NULL");
        return ca_amplitude;
    }
    if (!bridge->noise_enabled) return ca_amplitude;

    /* Apply transient modulation */
    return ca_amplitude * bridge->effects.transient_modulation;
}

float calcium_pink_noise_modulate_decay(
    const calcium_pink_noise_bridge_t* bridge,
    float decay_rate
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_modulate_decay: bridge is NULL");
        return decay_rate;
    }
    if (!bridge->noise_enabled) return decay_rate;

    /* Apply decay modulation */
    return decay_rate * bridge->effects.decay_modulation;
}

/* ============================================================================
 * Calcium → Pink Noise Feedback API Implementation
 * ============================================================================ */

int calcium_pink_noise_update_ca_dependent_amplitude(
    calcium_pink_noise_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_update_ca_dependent_amplitude: bridge is NULL");
        return -1;
    }
    if (!bridge->calcium_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "calcium_pink_noise_update_ca_dependent_amplitude: calcium not connected");
        return -1;
    }

    /* Get current calcium concentration */
    float ca_concentration = calcium_get_concentration(bridge->calcium);

    /* Normalize to [0, 1] using max concentration */
    float ca_max = CALCIUM_MAX_CONCENTRATION;
    float ca_normalized = nimcp_clampf(ca_concentration / ca_max, 0.0f, 1.0f);
    bridge->effects.ca_concentration_normalized = ca_normalized;

    /* Interpolate amplitude scale: low Ca → high scale, high Ca → low scale */
    float ca_low_scale = bridge->config.ca_low_scale;
    float ca_high_scale = bridge->config.ca_high_scale;
    float amplitude_scale = ca_low_scale + (ca_high_scale - ca_low_scale) * ca_normalized;

    /* Compute effective amplitude */
    float base_amplitude = bridge->config.pink_noise_amplitude;
    bridge->effects.effective_amplitude = base_amplitude * amplitude_scale;

    return 0;
}

float calcium_pink_noise_get_effective_amplitude(
    const calcium_pink_noise_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_get_effective_amplitude: bridge is NULL");
        return 0.0f;
    }
    return bridge->effects.effective_amplitude;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int calcium_pink_noise_get_effects(
    const calcium_pink_noise_bridge_t* bridge,
    calcium_pink_noise_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_get_effects: effects is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->effects, sizeof(calcium_pink_noise_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int calcium_pink_noise_get_stats(
    const calcium_pink_noise_bridge_t* bridge,
    calcium_pink_noise_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_get_stats: stats is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(calcium_pink_noise_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float calcium_pink_noise_get_current_sample(
    const calcium_pink_noise_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_get_current_sample: bridge is NULL");
        return 0.0f;
    }
    return bridge->effects.current_noise_sample;
}

/* ============================================================================
 * Bio-Async Integration API Implementation
 * ============================================================================ */

int calcium_pink_noise_connect_bio_async(calcium_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_connect_bio_async: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Skip if already connected */
    if (bridge->base.bio_async_enabled) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_CALCIUM_PINK_NOISE,
        .module_name = "calcium_pink_noise_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int calcium_pink_noise_disconnect_bio_async(calcium_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->base.bio_async_enabled && bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
        bridge->base.bio_async_enabled = false;
        NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool calcium_pink_noise_is_bio_async_connected(
    const calcium_pink_noise_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

int calcium_pink_noise_reset(
    calcium_pink_noise_bridge_t* bridge,
    uint32_t new_seed
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_reset: bridge is NULL");
        return -1;
    }
    if (!bridge->noise_gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "calcium_pink_noise_reset: noise_gen is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset pink noise generator */
    uint32_t seed = (new_seed == 0) ? bridge->config.pink_noise_seed : new_seed;
    if (!pink_noise_reset(bridge->noise_gen, seed)) {
        NIMCP_LOGGING_ERROR("Failed to reset pink noise generator");
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "calcium_pink_noise_reset: pink_noise_reset is NULL");
        return -1;
    }

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(calcium_pink_noise_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Pink noise generator reset");
    return 0;
}

int calcium_pink_noise_validate_config(const calcium_pink_noise_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_pink_noise_validate_config: config is NULL");
        return -1;
    }

    /* Validate pink noise parameters */
    if (config->pink_noise_alpha < 0.0f || config->pink_noise_alpha > 3.0f) {
        NIMCP_LOGGING_ERROR("Invalid alpha: %f (must be [0, 3])", config->pink_noise_alpha);
        return -1;
    }

    if (config->pink_noise_amplitude <= 0.0f || config->pink_noise_amplitude > 1.0f) {
        NIMCP_LOGGING_ERROR("Invalid amplitude: %f (must be (0, 1])", config->pink_noise_amplitude);
        return -1;
    }

    if (config->pink_noise_min_freq <= 0.0f || config->pink_noise_min_freq >= config->pink_noise_max_freq) {
        NIMCP_LOGGING_ERROR("Invalid frequency range: [%f, %f]",
            config->pink_noise_min_freq, config->pink_noise_max_freq);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "calcium_pink_noise_validate_config: capacity exceeded");
        return -1;
    }

    if (config->pink_noise_sample_rate < 2.0f * config->pink_noise_max_freq) {
        NIMCP_LOGGING_ERROR("Sample rate %f violates Nyquist (need >= %f)",
            config->pink_noise_sample_rate, 2.0f * config->pink_noise_max_freq);
        return -1;
    }

    /* Validate modulation strengths */
    if (config->influx_modulation_strength < 0.0f || config->influx_modulation_strength > 0.5f) {
        NIMCP_LOGGING_ERROR("Invalid influx modulation strength: %f (must be [0, 0.5])",
            config->influx_modulation_strength);
        return -1;
    }

    if (config->transient_modulation_strength < 0.0f || config->transient_modulation_strength > 0.5f) {
        NIMCP_LOGGING_ERROR("Invalid transient modulation strength: %f (must be [0, 0.5])",
            config->transient_modulation_strength);
        return -1;
    }

    if (config->decay_modulation_strength < 0.0f || config->decay_modulation_strength > 0.5f) {
        NIMCP_LOGGING_ERROR("Invalid decay modulation strength: %f (must be [0, 0.5])",
            config->decay_modulation_strength);
        return -1;
    }

    /* Validate Ca-dependent scales */
    if (config->ca_low_scale <= 0.0f || config->ca_high_scale <= 0.0f) {
        NIMCP_LOGGING_ERROR("Invalid Ca scale factors: low=%f high=%f (must be > 0)",
            config->ca_low_scale, config->ca_high_scale);
        return -1;
    }

    return 0;
}
