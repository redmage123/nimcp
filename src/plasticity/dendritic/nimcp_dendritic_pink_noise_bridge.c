/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * dendritic_pink_noise_bridge_create has zero callers in production
 * code. Wrapper around pink_noise_create that is unused. Either wire
 * a consumer or delete before the next major version. Do not extend.
 */

/**
 * @file nimcp_dendritic_pink_noise_bridge.c
 * @brief Implementation of Dendritic Integration - Pink Noise Integration Bridge
 * @version 1.0.0
 * @date 2025-12-21
 */

#include "plasticity/dendritic/nimcp_dendritic_pink_noise_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dendritic_pink_noise_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(dendritic_pink_noise_bridge)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute combined noise scaling
 *
 * WHAT: Combine activity, NMDA, and calcium scaling factors
 * WHY:  Multiple factors suppress noise independently
 * HOW:  Multiply all suppression factors
 */
static float compute_combined_scaling(
    float activity_scale,
    float nmda_suppression,
    float calcium_suppression
) {
    return activity_scale * nmda_suppression * calcium_suppression;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int dendritic_pink_noise_default_config(dendritic_pink_noise_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("Null config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_default_config: config is NULL");
        return -1;
    }

    /* Feature enables - all on by default */
    config->enable_voltage_noise = true;
    config->enable_nmda_noise = true;
    config->enable_synapse_noise = true;
    config->enable_calcium_noise = true;
    config->enable_activity_modulation = true;
    config->enable_nmda_modulation = true;
    config->enable_calcium_modulation = true;

    /* Noise amplitudes - biologically realistic defaults */
    config->voltage_noise_amplitude = DENDRITIC_PINK_NOISE_VOLTAGE_DEFAULT;
    config->nmda_noise_amplitude = DENDRITIC_PINK_NOISE_NMDA_DEFAULT;
    config->synapse_noise_amplitude = DENDRITIC_PINK_NOISE_SYNAPSE_DEFAULT;
    config->calcium_noise_amplitude = DENDRITIC_PINK_NOISE_CALCIUM_DEFAULT;

    /* Spectral parameters - pink noise (1/f) */
    config->pink_noise_alpha = DENDRITIC_PINK_NOISE_ALPHA_DEFAULT;
    config->pink_noise_sample_rate = 1000.0f;  // 1ms resolution
    config->pink_noise_min_freq = 0.1f;        // 10s timescale
    config->pink_noise_max_freq = 100.0f;      // 10ms timescale

    /* Activity-dependent modulation gains */
    config->activity_noise_gain = DENDRITIC_ACTIVITY_NOISE_GAIN;
    config->nmda_suppression_factor = DENDRITIC_NMDA_NOISE_SUPPRESSION;
    config->calcium_suppression_factor = DENDRITIC_CALCIUM_NOISE_SUPPRESSION;

    /* Random seed (0 = use time) */
    config->random_seed = 0;

    return 0;
}

dendritic_pink_noise_bridge_t* dendritic_pink_noise_bridge_create(
    const dendritic_pink_noise_config_t* config,
    dendritic_tree_t dendritic_tree
) {
    /* Guard: Validate inputs */
    if (!dendritic_tree) {
        NIMCP_LOGGING_ERROR("Null dendritic tree");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_tree is NULL");

        return NULL;
    }

    /* Allocate bridge structure */
    dendritic_pink_noise_bridge_t* bridge = (dendritic_pink_noise_bridge_t*)
        nimcp_malloc(sizeof(dendritic_pink_noise_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendritic_pink_noise_bridge_create: failed to allocate bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(dendritic_pink_noise_bridge_t));

    /* Use default config if none provided */
    if (config) {
        bridge->config = *config;
    } else {
        dendritic_pink_noise_default_config(&bridge->config);
    }

    /* Create pink noise generator */
    pink_noise_config_t noise_config = pink_noise_default_config();
    noise_config.alpha = bridge->config.pink_noise_alpha;
    noise_config.amplitude = 1.0f;  // We'll scale manually
    noise_config.min_frequency = bridge->config.pink_noise_min_freq;
    noise_config.max_frequency = bridge->config.pink_noise_max_freq;
    noise_config.sample_rate = bridge->config.pink_noise_sample_rate;
    noise_config.seed = bridge->config.random_seed;

    bridge->noise_generator = pink_noise_create(&noise_config);
    if (!bridge->noise_generator) {
        NIMCP_LOGGING_ERROR("Failed to create pink noise generator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendritic_pink_noise_bridge_create: failed to create noise generator");
        nimcp_free(bridge);
        return NULL;
    }

    /* Link dendritic tree */
    bridge->dendritic_tree = dendritic_tree;

    /* Initialize state */
    bridge->dendritic_modulation.combined_noise_scale = 1.0f;
    bridge->noise_effects.effective_amplitude = 1.0f;
    bridge->noise_effects.effective_alpha = bridge->config.pink_noise_alpha;

    /* Initialize bridge base (allocates mutex, sets module name) */
    if (bridge_base_init(&bridge->base, 0, "dendritic_pink_noise") != 0) {
        NIMCP_LOGGING_WARN("Failed to init bridge base, continuing without thread safety");
    }

    NIMCP_LOGGING_INFO("Created dendritic-pink noise bridge");
    return bridge;
}

void dendritic_pink_noise_bridge_destroy(dendritic_pink_noise_bridge_t* bridge) {
    /* Guard: Check for null */
    if (!bridge) return;

    /* Destroy pink noise generator */
    if (bridge->noise_generator) {
        pink_noise_destroy(bridge->noise_generator);
    }

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge structure */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed dendritic-pink noise bridge");
}

/* ============================================================================
 * Dendritic → Pink Noise API Implementation
 * ============================================================================ */

int dendritic_pink_noise_update_modulation(dendritic_pink_noise_bridge_t* bridge) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Invalid bridge or dendritic tree");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_update_modulation: bridge is NULL");
        return -1;
    }
    if (!bridge->dendritic_tree) {
        NIMCP_LOGGING_ERROR("Invalid bridge or dendritic tree");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "dendritic_pink_noise_update_modulation: dendritic_tree is NULL");
        return -1;
    }

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    /* Get dendritic tree statistics */
    dendritic_tree_stats_t stats;
    if (!dendritic_tree_get_stats(bridge->dendritic_tree, &stats)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "dendritic_pink_noise_update_modulation: failed to get tree stats");
        if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    /* Extract state variables (normalized to [0-1]) */
    bridge->dendritic_modulation.activity_level =
        nimcp_clampf(stats.mean_voltage / 50.0f, 0.0f, 1.0f);  // Assume 50mV max
    bridge->dendritic_modulation.nmda_activation =
        nimcp_clampf(stats.nmda_activations / 100.0f, 0.0f, 1.0f);  // Normalize
    bridge->dendritic_modulation.calcium_level =
        nimcp_clampf(stats.max_calcium / 10.0f, 0.0f, 1.0f);  // Assume 10μM max

    /* Compute activity-dependent noise scaling */
    if (bridge->config.enable_activity_modulation) {
        float activity = bridge->dendritic_modulation.activity_level;
        bridge->dendritic_modulation.activity_noise_scale =
            1.0f + bridge->config.activity_noise_gain * activity;
        bridge->dendritic_modulation.activity_noise_scale =
            nimcp_clampf(bridge->dendritic_modulation.activity_noise_scale, 0.1f, 1.0f);
    } else {
        bridge->dendritic_modulation.activity_noise_scale = 1.0f;
    }

    /* Compute NMDA-dependent noise suppression */
    if (bridge->config.enable_nmda_modulation) {
        float nmda = bridge->dendritic_modulation.nmda_activation;
        bridge->dendritic_modulation.nmda_noise_suppression =
            1.0f - (1.0f - bridge->config.nmda_suppression_factor) * nmda;
    } else {
        bridge->dendritic_modulation.nmda_noise_suppression = 1.0f;
    }

    /* Compute calcium-dependent noise suppression */
    if (bridge->config.enable_calcium_modulation) {
        float calcium = bridge->dendritic_modulation.calcium_level;
        bridge->dendritic_modulation.calcium_noise_suppression =
            1.0f - (1.0f - bridge->config.calcium_suppression_factor) * calcium;
    } else {
        bridge->dendritic_modulation.calcium_noise_suppression = 1.0f;
    }

    /* Combine all scaling factors */
    bridge->dendritic_modulation.combined_noise_scale = compute_combined_scaling(
        bridge->dendritic_modulation.activity_noise_scale,
        bridge->dendritic_modulation.nmda_noise_suppression,
        bridge->dendritic_modulation.calcium_noise_suppression
    );

    /* Update effective noise parameters */
    bridge->noise_effects.effective_amplitude =
        bridge->dendritic_modulation.combined_noise_scale;

    /* Unlock */
    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float dendritic_pink_noise_compute_activity_scaling(
    const dendritic_pink_noise_bridge_t* bridge
) {
    /* Guard: Validate input */
    if (!bridge) return 1.0f;

    return bridge->dendritic_modulation.activity_noise_scale;
}

float dendritic_pink_noise_compute_nmda_suppression(
    const dendritic_pink_noise_bridge_t* bridge
) {
    /* Guard: Validate input */
    if (!bridge) return 1.0f;

    return bridge->dendritic_modulation.nmda_noise_suppression;
}

/* ============================================================================
 * Pink Noise → Dendritic API Implementation
 * ============================================================================ */

int dendritic_pink_noise_apply_voltage_noise(
    dendritic_pink_noise_bridge_t* bridge,
    uint32_t branch_id,
    uint32_t compartment_id,
    float* voltage_noise_out
) {
    /* Guard: Validate inputs */
    BRIDGE_BBB_VALIDATE(bridge, voltage_noise_out, sizeof(*voltage_noise_out));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_apply_voltage_noise: bridge is NULL");
        return -1;
    }
    if (!voltage_noise_out) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_apply_voltage_noise: voltage_noise_out is NULL");
        return -1;
    }

    /* Guard: Check if voltage noise is enabled */
    if (!bridge->config.enable_voltage_noise) {
        *voltage_noise_out = 0.0f;
        return 0;
    }

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    /* Generate pink noise sample */
    float noise_sample = 0.0f;
    if (!pink_noise_generate_sample(bridge->noise_generator, &noise_sample)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "dendritic_pink_noise_apply_voltage_noise: failed to generate sample");
        if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    /* Scale by configured amplitude and activity modulation */
    float scaled_noise = noise_sample *
                        bridge->config.voltage_noise_amplitude *
                        bridge->noise_effects.effective_amplitude;

    /* Store sample and output */
    bridge->noise_effects.voltage_noise_sample = noise_sample;
    bridge->noise_effects.voltage_noise_amplitude =
        bridge->config.voltage_noise_amplitude * bridge->noise_effects.effective_amplitude;
    *voltage_noise_out = scaled_noise;

    /* Update statistics */
    bridge->voltage_noise_applications++;

    /* Unlock */
    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dendritic_pink_noise_apply_nmda_noise(
    dendritic_pink_noise_bridge_t* bridge,
    float base_conductance,
    float* noisy_conductance_out
) {
    /* Guard: Validate inputs */
    BRIDGE_BBB_VALIDATE(bridge, noisy_conductance_out, sizeof(*noisy_conductance_out));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_apply_nmda_noise: bridge is NULL");
        return -1;
    }
    if (!noisy_conductance_out) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_apply_nmda_noise: noisy_conductance_out is NULL");
        return -1;
    }

    /* Guard: Check if NMDA noise is enabled */
    if (!bridge->config.enable_nmda_noise) {
        *noisy_conductance_out = base_conductance;
        return 0;
    }

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    /* Generate pink noise sample */
    float noise_sample = 0.0f;
    if (!pink_noise_generate_sample(bridge->noise_generator, &noise_sample)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "dendritic_pink_noise_apply_nmda_noise: failed to generate sample");
        if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    /* Apply multiplicative noise: g_noisy = g_base * (1 + noise * amplitude) */
    float noise_factor = 1.0f + noise_sample *
                        bridge->config.nmda_noise_amplitude *
                        bridge->noise_effects.effective_amplitude;
    noise_factor = nimcp_clampf(noise_factor, 0.0f, 2.0f);  // Prevent negative/excessive

    *noisy_conductance_out = base_conductance * noise_factor;

    /* Store sample */
    bridge->noise_effects.nmda_noise_sample = noise_sample;
    bridge->noise_effects.nmda_noise_amplitude =
        bridge->config.nmda_noise_amplitude * bridge->noise_effects.effective_amplitude;

    /* Update statistics */
    bridge->nmda_noise_applications++;

    /* Unlock */
    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dendritic_pink_noise_apply_synapse_noise(
    dendritic_pink_noise_bridge_t* bridge,
    float base_weight,
    float* noisy_weight_out
) {
    /* Guard: Validate inputs */
    BRIDGE_BBB_VALIDATE(bridge, noisy_weight_out, sizeof(*noisy_weight_out));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_apply_synapse_noise: bridge is NULL");
        return -1;
    }
    if (!noisy_weight_out) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_apply_synapse_noise: noisy_weight_out is NULL");
        return -1;
    }

    /* Guard: Check if synapse noise is enabled */
    if (!bridge->config.enable_synapse_noise) {
        *noisy_weight_out = base_weight;
        return 0;
    }

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    /* Generate pink noise sample */
    float noise_sample = 0.0f;
    if (!pink_noise_generate_sample(bridge->noise_generator, &noise_sample)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "dendritic_pink_noise_apply_synapse_noise: failed to generate sample");
        if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    /* Apply multiplicative noise */
    float noise_factor = 1.0f + noise_sample *
                        bridge->config.synapse_noise_amplitude *
                        bridge->noise_effects.effective_amplitude;
    noise_factor = nimcp_clampf(noise_factor, 0.5f, 1.5f);  // ±50% variation max

    *noisy_weight_out = base_weight * noise_factor;

    /* Store sample */
    bridge->noise_effects.synapse_noise_sample = noise_sample;
    bridge->noise_effects.synapse_noise_amplitude =
        bridge->config.synapse_noise_amplitude * bridge->noise_effects.effective_amplitude;

    /* Update statistics */
    bridge->synapse_noise_applications++;

    /* Unlock */
    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dendritic_pink_noise_apply_calcium_noise(
    dendritic_pink_noise_bridge_t* bridge,
    float base_influx,
    float* noisy_influx_out
) {
    /* Guard: Validate inputs */
    BRIDGE_BBB_VALIDATE(bridge, noisy_influx_out, sizeof(*noisy_influx_out));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_apply_calcium_noise: bridge is NULL");
        return -1;
    }
    if (!noisy_influx_out) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_apply_calcium_noise: noisy_influx_out is NULL");
        return -1;
    }

    /* Guard: Check if calcium noise is enabled */
    if (!bridge->config.enable_calcium_noise) {
        *noisy_influx_out = base_influx;
        return 0;
    }

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    /* Generate pink noise sample */
    float noise_sample = 0.0f;
    if (!pink_noise_generate_sample(bridge->noise_generator, &noise_sample)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "dendritic_pink_noise_apply_calcium_noise: failed to generate sample");
        if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    /* Apply multiplicative noise */
    float noise_factor = 1.0f + noise_sample *
                        bridge->config.calcium_noise_amplitude *
                        bridge->noise_effects.effective_amplitude;
    noise_factor = nimcp_clampf(noise_factor, 0.3f, 1.7f);  // Wide variation for Ca²⁺

    *noisy_influx_out = base_influx * noise_factor;

    /* Store sample */
    bridge->noise_effects.calcium_noise_sample = noise_sample;
    bridge->noise_effects.calcium_noise_amplitude =
        bridge->config.calcium_noise_amplitude * bridge->noise_effects.effective_amplitude;

    /* Update statistics */
    bridge->calcium_noise_applications++;

    /* Unlock */
    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bidirectional Update API Implementation
 * ============================================================================ */

int dendritic_pink_noise_bridge_update(
    dendritic_pink_noise_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard: Validate input */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_bridge_update: bridge is NULL");
        return -1;
    }

    /* Update dendritic state → noise modulation */
    if (dendritic_pink_noise_update_modulation(bridge) != 0) {
        NIMCP_LOGGING_ERROR("Failed to update modulation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "dendritic_pink_noise_bridge_update: modulation update failed");
        return -1;
    }

    /* Update statistics */
    bridge->total_updates++;

    /* Update running average of noise amplitude */
    float alpha = 0.01f;  // Exponential moving average factor
    bridge->avg_noise_amplitude = (1.0f - alpha) * bridge->avg_noise_amplitude +
                                  alpha * bridge->noise_effects.effective_amplitude;
    bridge->avg_activity_scaling = (1.0f - alpha) * bridge->avg_activity_scaling +
                                   alpha * bridge->dendritic_modulation.activity_noise_scale;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int dendritic_pink_noise_get_modulation(
    const dendritic_pink_noise_bridge_t* bridge,
    dendritic_noise_modulation_t* modulation
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_get_modulation: bridge is NULL");
        return -1;
    }
    if (!modulation) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_get_modulation: modulation is NULL");
        return -1;
    }

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    /* Copy modulation state */
    *modulation = bridge->dendritic_modulation;

    /* Unlock */
    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dendritic_pink_noise_get_effects(
    const dendritic_pink_noise_bridge_t* bridge,
    dendritic_pink_noise_effects_t* effects
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_LOGGING_ERROR("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_get_effects: effects is NULL");
        return -1;
    }

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    /* Copy effects state */
    *effects = bridge->noise_effects;

    /* Unlock */
    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float dendritic_pink_noise_get_effective_amplitude(
    const dendritic_pink_noise_bridge_t* bridge
) {
    /* Guard: Validate input */
    if (!bridge) return 0.0f;

    return bridge->noise_effects.effective_amplitude;
}

float dendritic_pink_noise_get_activity_scaling(
    const dendritic_pink_noise_bridge_t* bridge
) {
    /* Guard: Validate input */
    if (!bridge) return 1.0f;

    return bridge->dendritic_modulation.activity_noise_scale;
}

int dendritic_pink_noise_set_enables(
    dendritic_pink_noise_bridge_t* bridge,
    bool enable_voltage,
    bool enable_nmda,
    bool enable_synapse,
    bool enable_calcium
) {
    /* Guard: Validate input */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_set_enables: bridge is NULL");
        return -1;
    }

    /* Lock for thread safety */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    /* Update configuration */
    bridge->config.enable_voltage_noise = enable_voltage;
    bridge->config.enable_nmda_noise = enable_nmda;
    bridge->config.enable_synapse_noise = enable_synapse;
    bridge->config.enable_calcium_noise = enable_calcium;

    /* Unlock */
    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Updated noise channel enables");
    return 0;
}

/* ============================================================================
 * Bio-Async Integration API Implementation
 * ============================================================================ */

int dendritic_pink_noise_connect_bio_async(dendritic_pink_noise_bridge_t* bridge) {
    /* Guard: Validate input */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Guard: Already connected */
    if (bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_INFO("Already connected to bio-async");
        return 0;
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_PINK_NOISE_DENDRITIC,
        .module_name = "dendritic_pink_noise_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected dendritic-pink noise bridge to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendritic_pink_noise_connect_bio_async: validation failed");
    return -1;
}

int dendritic_pink_noise_disconnect_bio_async(dendritic_pink_noise_bridge_t* bridge) {
    /* Guard: Validate input */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_pink_noise_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    /* Guard: Not connected */
    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    /* Unregister from bio-async router */
    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected dendritic-pink noise bridge from bio-async router");
    return 0;
}

bool dendritic_pink_noise_is_bio_async_connected(
    const dendritic_pink_noise_bridge_t* bridge
) {
    /* Guard: Validate input */
    if (!bridge) {
        return false;
    }

    return bridge->base.bio_async_enabled;
}
