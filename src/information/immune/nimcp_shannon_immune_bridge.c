/**
 * @file nimcp_shannon_immune_bridge.c
 * @brief Shannon Entropy-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "information/immune/nimcp_shannon_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(shannon_immune_bridge)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 *
 * WHAT: Get system time for duration tracking
 * WHY:  Track chronic inflammation and bottleneck duration
 * HOW:  Platform-specific time retrieval
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Get inflammation channel capacity factor
 *
 * WHAT: Map inflammation level to channel capacity reduction
 * WHY:  Different inflammation levels have different capacity impacts
 * HOW:  Return predefined factor based on level
 */
static float get_inflammation_channel_factor(brain_inflammation_level_t level) {
    float cont = inflammation_level_to_continuous(level);
    return inflammation_compute_factor(cont,
        INFLAMMATION_NONE_CHANNEL_FACTOR,
        INFLAMMATION_STORM_CHANNEL_FACTOR);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int shannon_immune_default_config(shannon_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Enable all features by default */
    config->enable_cytokine_entropy_impairment = true;
    config->enable_inflammation_capacity_reduction = true;
    config->enable_pattern_recognition_immune = true;
    config->enable_anomaly_detection = true;
    config->enable_capacity_stress_response = true;

    /* Default sensitivity (1.0 = normal) */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->anomaly_sensitivity = ENTROPY_ANOMALY_SENSITIVITY;

    /* Default thresholds */
    config->entropy_collapse_threshold = ENTROPY_COLLAPSE_THRESHOLD;
    config->entropy_overload_threshold = ENTROPY_OVERLOAD_THRESHOLD;
    config->capacity_bottleneck_threshold = CAPACITY_BOTTLENECK_THRESHOLD;

    return 0;
}

shannon_immune_bridge_t* shannon_immune_create(
    const shannon_immune_config_t* config,
    brain_immune_system_t* immune_system,
    shannon_channel_t* shannon_channel
) {
    /* Guard: require immune system */
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("shannon_immune_create: immune_system required");
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "shannon_immune_create: immune_system required");
        return NULL;
    }

    /* Allocate bridge */
    shannon_immune_bridge_t* bridge = nimcp_malloc(sizeof(shannon_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("shannon_immune_create: allocation failed");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(shannon_immune_bridge_t),
                          "shannon_immune_create: Failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(shannon_immune_bridge_t));

    /* Apply configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(shannon_immune_config_t));
    } else {
        shannon_immune_default_config(&bridge->config);
    }

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->shannon_channel = shannon_channel;

    /* Initialize timing */
    bridge->last_update_time = get_time_ms();

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "shannon_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_WARN("shannon_immune_create: mutex creation failed");
        NIMCP_THROW_THREADING(NIMCP_ERROR_MUTEX_INIT, 0,
                             "shannon_immune_create: mutex creation failed");
        /* Continue without mutex - non-fatal but log warning */
    }

    NIMCP_LOGGING_INFO("shannon_immune_bridge: created successfully");
    return bridge;
}

void shannon_immune_destroy(shannon_immune_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        shannon_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Immune → Shannon API
 * ============================================================================ */

int shannon_immune_apply_cytokine_effects(shannon_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "shannon_immune_apply_cytokine_effects: bridge or immune_system is NULL");
        return -1;
    }

    if (!bridge->config.enable_cytokine_entropy_impairment) {
        return 0;  /* Feature disabled */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get immune stats */
    brain_immune_stats_t stats;
    brain_immune_get_stats(bridge->immune_system, &stats);

    /* Reset effects */
    memset(&bridge->cytokine_effects, 0, sizeof(shannon_cytokine_effects_t));

    /* Compute cytokine-induced entropy errors from actual cytokine levels */
    bridge->cytokine_effects.il1_entropy_error =
        stats.cytokine_il1 * CYTOKINE_IL1_ENTROPY_IMPACT * bridge->config.cytokine_sensitivity;
    bridge->cytokine_effects.il6_entropy_error =
        stats.cytokine_il6 * CYTOKINE_IL6_ENTROPY_IMPACT * bridge->config.cytokine_sensitivity;
    bridge->cytokine_effects.tnf_entropy_error =
        stats.cytokine_tnf * CYTOKINE_TNF_ENTROPY_IMPACT * bridge->config.cytokine_sensitivity;
    bridge->cytokine_effects.ifn_gamma_entropy_error =
        stats.cytokine_ifn_gamma * CYTOKINE_IFN_GAMMA_ENTROPY_IMPACT * bridge->config.cytokine_sensitivity;

    /* Total entropy error */
    bridge->cytokine_effects.total_entropy_error = nimcp_clamp01(
        fabsf(bridge->cytokine_effects.il1_entropy_error) +
        fabsf(bridge->cytokine_effects.il6_entropy_error) +
        fabsf(bridge->cytokine_effects.tnf_entropy_error) +
        fabsf(bridge->cytokine_effects.ifn_gamma_entropy_error)
    );

    /* Capacity reduction from cytokines */
    bridge->cytokine_effects.capacity_reduction =
        bridge->cytokine_effects.total_entropy_error * 0.7f;

    /* SNR degradation */
    bridge->cytokine_effects.snr_degradation =
        bridge->cytokine_effects.total_entropy_error * 0.6f;

    /* Bandwidth narrowing */
    bridge->cytokine_effects.bandwidth_narrowing =
        bridge->cytokine_effects.total_entropy_error * 0.5f;

    bridge->cytokine_impairments++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shannon_immune_apply_inflammation_effects(shannon_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "shannon_immune_apply_inflammation_effects: bridge or immune_system is NULL");
        return -1;
    }

    if (!bridge->config.enable_inflammation_capacity_reduction) {
        return 0;  /* Feature disabled */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get immune stats */
    brain_immune_stats_t stats;
    brain_immune_get_stats(bridge->immune_system, &stats);

    /* Use inflammation level from immune stats */
    brain_inflammation_level_t level = stats.inflammation_level;
    bridge->inflammation_state.current_level = level;

    /* Channel capacity factor based on inflammation */
    bridge->inflammation_state.channel_capacity_factor =
        get_inflammation_channel_factor(level) * bridge->config.inflammation_sensitivity;

    /* Entropy estimation error increases with inflammation */
    bridge->inflammation_state.entropy_estimation_error =
        (1.0f - bridge->inflammation_state.channel_capacity_factor) * 0.8f;

    /* SNR reduction */
    float snr_reduction = INFLAMMATION_SNR_DEGRADATION_BASE +
                         (level * INFLAMMATION_SNR_DEGRADATION_PER_LEVEL);
    bridge->inflammation_state.snr_reduction = nimcp_clamp01(snr_reduction);

    /* Bandwidth reduction */
    bridge->inflammation_state.bandwidth_reduction =
        1.0f - bridge->inflammation_state.channel_capacity_factor;

    /* Mutual information degradation */
    bridge->inflammation_state.mutual_info_degradation =
        (1.0f - bridge->inflammation_state.channel_capacity_factor) * 0.75f;

    /* Discrimination impairment */
    bridge->inflammation_state.discrimination_impairment =
        bridge->inflammation_state.entropy_estimation_error * 0.9f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float shannon_immune_compute_capacity(const shannon_immune_bridge_t* bridge) {
    if (!bridge) {
        return 1.0f;  /* Normal capacity */
    }

    /* Combine cytokine and inflammation effects */
    float cytokine_reduction = bridge->cytokine_effects.capacity_reduction;
    float inflammation_factor = bridge->inflammation_state.channel_capacity_factor;

    /* Combined capacity: multiplicative reduction */
    float capacity = inflammation_factor * (1.0f - cytokine_reduction);
    return nimcp_clamp01(capacity);
}

float shannon_immune_compute_snr_degradation(const shannon_immune_bridge_t* bridge) {
    if (!bridge) {
        return 0.0f;  /* No degradation */
    }

    /* Combine cytokine and inflammation SNR degradation */
    float cytokine_snr = bridge->cytokine_effects.snr_degradation;
    float inflammation_snr = bridge->inflammation_state.snr_reduction;

    /* Take maximum (most severe degradation) */
    float degradation = fmaxf(cytokine_snr, inflammation_snr);
    return nimcp_clamp01(degradation);
}

/* ============================================================================
 * Shannon → Immune API
 * ============================================================================ */

int shannon_immune_detect_pattern_threat(
    shannon_immune_bridge_t* bridge,
    float entropy
) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "shannon_immune_detect_pattern_threat: bridge or immune_system is NULL");
        return -1;
    }

    if (!bridge->config.enable_pattern_recognition_immune) {
        return 0;  /* Feature disabled */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->immune_modulation.current_entropy = entropy;

    /* Detect low-entropy patterns */
    if (entropy < bridge->config.entropy_collapse_threshold) {
        bridge->immune_modulation.low_entropy_pattern_detected = true;
        bridge->immune_modulation.pattern_recognition_alert =
            1.0f - (entropy / bridge->config.entropy_collapse_threshold);
        bridge->immune_modulation.immune_surveillance_boost =
            bridge->immune_modulation.pattern_recognition_alert * 0.5f;
        bridge->pattern_alerts++;
    } else {
        bridge->immune_modulation.low_entropy_pattern_detected = false;
        bridge->immune_modulation.pattern_recognition_alert = 0.0f;
        bridge->immune_modulation.immune_surveillance_boost = 0.0f;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shannon_immune_detect_anomaly(
    shannon_immune_bridge_t* bridge,
    float entropy
) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "shannon_immune_detect_anomaly: bridge or immune_system is NULL");
        return -1;
    }

    if (!bridge->config.enable_anomaly_detection) {
        return 0;  /* Feature disabled */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Detect high-entropy anomalies */
    if (entropy > bridge->config.entropy_overload_threshold) {
        bridge->immune_modulation.high_entropy_anomaly = true;
        bridge->anomaly_detections++;
        /* Could trigger immune response here */
    } else {
        bridge->immune_modulation.high_entropy_anomaly = false;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shannon_immune_trigger_capacity_stress(
    shannon_immune_bridge_t* bridge,
    float capacity
) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "shannon_immune_trigger_capacity_stress: bridge or immune_system is NULL");
        return -1;
    }

    if (!bridge->config.enable_capacity_stress_response) {
        return 0;  /* Feature disabled */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->immune_modulation.channel_capacity = capacity;

    /* Detect capacity bottleneck */
    if (capacity < bridge->config.capacity_bottleneck_threshold) {
        bridge->immune_modulation.capacity_bottleneck = true;
        bridge->immune_modulation.stress_inflammation_trigger = true;
        bridge->capacity_stress_events++;
        /* Could trigger cytokine release here */
    } else {
        bridge->immune_modulation.capacity_bottleneck = false;
        bridge->immune_modulation.stress_inflammation_trigger = false;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int shannon_immune_update(
    shannon_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update timing */
    bridge->last_update_time = get_time_ms();
    bridge->total_updates++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* IMMUNE → SHANNON pathways */
    shannon_immune_apply_cytokine_effects(bridge);
    shannon_immune_apply_inflammation_effects(bridge);

    /* SHANNON → IMMUNE pathways */
    float current_entropy = bridge->immune_modulation.current_entropy;
    float current_capacity = bridge->immune_modulation.channel_capacity;

    shannon_immune_detect_pattern_threat(bridge, current_entropy);
    shannon_immune_detect_anomaly(bridge, current_entropy);
    shannon_immune_trigger_capacity_stress(bridge, current_capacity);

    return 0;
}

int shannon_immune_apply_modulation(shannon_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* If we have a shannon_channel, apply modulation to it */
    if (bridge->shannon_channel) {
        /* Would modify shannon_channel fields based on immune state */
        /* For example: adjust capacity, SNR, etc. */
    }

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int shannon_immune_get_cytokine_effects(
    const shannon_immune_bridge_t* bridge,
    shannon_cytokine_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "shannon_immune_get_cytokine_effects: bridge or effects is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(shannon_cytokine_effects_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int shannon_immune_get_inflammation_state(
    const shannon_immune_bridge_t* bridge,
    shannon_inflammation_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "shannon_immune_get_inflammation_state: bridge or state is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(shannon_inflammation_state_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool shannon_immune_has_capacity_deficit(const shannon_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    float capacity = shannon_immune_compute_capacity(bridge);
    return capacity < 0.6f;  /* >40% capacity loss */
}

float shannon_immune_get_capacity_factor(const shannon_immune_bridge_t* bridge) {
    return shannon_immune_compute_capacity(bridge);
}

float shannon_immune_get_entropy_error(const shannon_immune_bridge_t* bridge) {
    if (!bridge) {
        return 0.0f;
    }
    return bridge->cytokine_effects.total_entropy_error;
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/** Module name for logging */
#define SHANNON_IMMUNE_MODULE_NAME "shannon_immune_bridge"

int shannon_immune_connect_bio_async(shannon_immune_bridge_t* bridge) {
    /**
     * WHAT: Register bridge with bio-async router
     * WHY:  Enable distributed immune signaling via NOREPINEPHRINE channel
     * HOW:  Use bio_router_register_module with immune module ID
     */

    /* Guard: null check */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("shannon_immune_connect_bio_async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "shannon_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Guard: already connected */
    if (bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_WARN("shannon_immune_bridge: Already connected to bio-async");
        return 0;
    }

    /* Build module info */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SHANNON,
        .module_name = SHANNON_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Register with router */
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("shannon_immune_bridge: Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("shannon_immune_bridge: Bio-async router not available, skipping registration");
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shannon_immune_disconnect_bio_async(shannon_immune_bridge_t* bridge) {
    /**
     * WHAT: Unregister bridge from bio-async router
     * WHY:  Clean shutdown of messaging infrastructure
     * HOW:  Use bio_router_unregister_module
     */

    /* Guard: null check */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Guard: not connected */
    if (!bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Unregister */
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("shannon_immune_bridge: Disconnected from bio-async router");

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool shannon_immune_is_bio_async_connected(const shannon_immune_bridge_t* bridge) {
    /**
     * WHAT: Check bio-async connection status
     * WHY:  Allow callers to verify messaging capability
     * HOW:  Return internal flag
     */
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
