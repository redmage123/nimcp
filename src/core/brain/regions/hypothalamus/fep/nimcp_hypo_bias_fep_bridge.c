/**
 * @file nimcp_hypo_bias_fep_bridge.c
 * @brief Implementation of Hypothalamus Bias FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hypothalamic bias modulation through drives
 * WHY:  Drives create cognitive biases; FEP enables detection and correction
 * HOW:  Map drive urgency to bias strength, bias detection to prediction error
 */

#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_bias_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypo_bias_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static void compute_bias_strengths(hypo_bias_fep_bridge_t* bridge);

static float compute_fe_from_biases(const hypo_bias_fep_bridge_t* bridge);

static hypo_bias_fep_level_t classify_bias_level(
    float free_energy,
    const hypo_bias_fep_config_t* config);

static hypo_bias_fep_response_t determine_response(
    hypo_bias_fep_level_t level,
    float total_bias);

static void update_running_averages(hypo_bias_fep_bridge_t* bridge,
                                    float free_energy,
                                    float surprise,
                                    float pred_error);

static void update_bias_tracking(hypo_bias_fep_bridge_t* bridge);

static hypo_bias_type_t map_drive_to_bias(hypo_drive_type_t drive);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * WHAT: Get default configuration for hypothalamus bias FEP bridge
 * WHY:  Provide sensible starting point for bias detection
 * HOW:  Set biologically-plausible defaults for bias parameters
 */
int hypo_bias_fep_default_config(hypo_bias_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* FEP parameters */
    config->drive_fe_weight = 1.0f;
    config->prediction_error_gain = 2.0f;
    config->precision_modulation = 1.0f;
    config->enable_active_inference = true;
    config->enable_bio_async = true;

    /* Bias computation - drive to bias mapping */
    /* SAFETY -> Risk aversion */
    config->drive_to_bias_scale[HYPO_DRIVE_SAFETY] = 1.2f;
    /* HUNGER -> Present bias */
    config->drive_to_bias_scale[HYPO_DRIVE_HUNGER] = 1.0f;
    /* THIRST -> Similar to hunger */
    config->drive_to_bias_scale[HYPO_DRIVE_THIRST] = 0.8f;
    /* FATIGUE -> Availability bias (poor judgment when tired) */
    config->drive_to_bias_scale[HYPO_DRIVE_FATIGUE] = 0.6f;
    /* TEMPERATURE -> Low effect */
    config->drive_to_bias_scale[HYPO_DRIVE_TEMPERATURE] = 0.3f;
    /* SOCIAL -> Ingroup favoritism */
    config->drive_to_bias_scale[HYPO_DRIVE_SOCIAL] = 1.1f;
    /* AUTONOMY -> Self-determination bias */
    config->drive_to_bias_scale[HYPO_DRIVE_AUTONOMY] = 0.9f;
    /* CURIOSITY -> Novelty seeking, confirmation bias */
    config->drive_to_bias_scale[HYPO_DRIVE_CURIOSITY] = 0.7f;

    config->bias_decay_rate = 0.05f;
    config->awareness_boost = 0.3f;

    /* Cognitive load effects */
    config->load_precision_scale = HYPO_BIAS_FEP_LOAD_PRECISION_SCALE;
    config->load_bias_amplify = HYPO_BIAS_FEP_LOAD_BIAS_AMPLIFY;
    config->enable_load_effects = true;

    /* Debiasing parameters */
    config->debias_threshold = HYPO_BIAS_FEP_HIGH_THRESHOLD;
    config->debias_strength = 0.5f;
    config->debias_learning_rate = 0.1f;

    /* Detection parameters */
    config->free_energy_threshold = HYPO_BIAS_FEP_HIGH_THRESHOLD;
    config->surprise_threshold = 8.0f;
    config->precision_learning_rate = 0.05f;

    /* Learning */
    config->enable_online_learning = true;
    config->learning_rate = 0.01f;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Create hypothalamus bias FEP bridge
 * WHY:  Initialize FEP integration for bias detection
 * HOW:  Allocate structure, initialize base, apply configuration
 */
hypo_bias_fep_bridge_t* hypo_bias_fep_create(
    const hypo_bias_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
) {
    /* Validate required parameters */
    if (!drive_system || !fep_system) {
        NIMCP_LOGGING_ERROR("Hypo Bias FEP bridge: NULL system pointers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bias_fep_create: required parameter is NULL (drive_system, fep_system)");
        return NULL;
    }

    /* Allocate bridge structure */
    hypo_bias_fep_bridge_t* bridge = (hypo_bias_fep_bridge_t*)nimcp_malloc(
        sizeof(hypo_bias_fep_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Hypo Bias FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_bias_fep_create: bridge is NULL");
        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(hypo_bias_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hypo_bias_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->drive_system = drive_system;
    bridge->fep_system = fep_system;

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "hypo_bias_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Hypo Bias FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bias_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = HYPO_BIAS_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_level = HYPO_BIAS_FEP_LEVEL_MINIMAL;

    /* Initialize bias strengths to zero (no bias) */
    for (int i = 0; i < HYPO_BIAS_TYPE_COUNT; i++) {
        bridge->fep_effects.bias_strengths[i] = 0.0f;
        bridge->state.bias_tracking.predicted_bias[i] = 0.0f;
    }
    bridge->fep_effects.total_bias = 0.0f;

    /* Initialize FEP effects */
    bridge->fep_effects.bias_level = HYPO_BIAS_FEP_LEVEL_MINIMAL;
    bridge->fep_effects.precision = HYPO_BIAS_FEP_DEFAULT_PRECISION;
    bridge->fep_effects.debiasing_potential = 1.0f;
    bridge->fep_effects.debiasing_recommended = false;

    /* Initialize bias effects */
    bridge->bias_effects.awareness = 0.5f;  /* Default awareness */

    /* Bio-async not yet connected */
    bridge->base.bio_async_enabled = false;
    bridge->base.module_id = BIO_MODULE_HYPO_BIAS_FEP;
    bridge->base.module_name = "hypo_bias_fep_bridge";

    NIMCP_LOGGING_INFO("Hypo Bias FEP bridge created");
    return bridge;
}

/**
 * WHAT: Destroy hypothalamus bias FEP bridge
 * WHY:  Clean up all resources
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void hypo_bias_fep_destroy(hypo_bias_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        hypo_bias_fep_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Hypo Bias FEP bridge destroyed");
}

/**
 * WHAT: Reset bridge to initial state
 * WHY:  Allow reuse without full recreation
 * HOW:  Clear state and statistics, preserve connections
 */
int hypo_bias_fep_reset(hypo_bias_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->state.update_count = 0;
    bridge->state.detection_count = 0;
    bridge->state.current_precision = HYPO_BIAS_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_level = HYPO_BIAS_FEP_LEVEL_MINIMAL;
    bridge->state.last_detection_time_ms = 0;

    /* Reset bias tracking */
    memset(&bridge->state.bias_tracking, 0, sizeof(hypo_bias_tracking_t));

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(hypo_bias_fep_effects_t));
    bridge->fep_effects.precision = HYPO_BIAS_FEP_DEFAULT_PRECISION;
    bridge->fep_effects.debiasing_potential = 1.0f;

    memset(&bridge->bias_effects, 0, sizeof(bias_to_fep_effects_t));
    bridge->bias_effects.awareness = 0.5f;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(hypo_bias_fep_stats_t));
    bridge->stats.current_precision = HYPO_BIAS_FEP_DEFAULT_PRECISION;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Hypo Bias FEP bridge reset");
    return 0;
}

/**
 * WHAT: Update bridge state
 * WHY:  Main update loop for bridge synchronization
 * HOW:  Compute bias from drives, update FEP metrics
 */
int hypo_bias_fep_update(hypo_bias_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute bias strengths from drives */
    compute_bias_strengths(bridge);

    /* Compute free energy from biases */
    float current_fe = compute_fe_from_biases(bridge);

    /* Get FEP system metrics */
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    /* Update running averages */
    update_running_averages(bridge, current_fe, surprise, pred_error);

    /* Store in FEP effects */
    bridge->fep_effects.free_energy = current_fe;
    bridge->fep_effects.prediction_error = pred_error;
    bridge->fep_effects.precision = bridge->state.current_precision;

    /* Classify bias level */
    bridge->fep_effects.bias_level = classify_bias_level(current_fe, &bridge->config);

    /* Determine response */
    bridge->fep_effects.recommended_response = determine_response(
        bridge->fep_effects.bias_level,
        bridge->fep_effects.total_bias
    );

    /* Compute debiasing potential */
    float awareness = bridge->bias_effects.awareness;
    float load = bridge->bias_effects.cognitive_load;
    bridge->fep_effects.debiasing_potential = awareness * (1.0f - load * 0.5f);

    /* Should we recommend debiasing? */
    bridge->fep_effects.debiasing_recommended =
        (current_fe >= bridge->config.debias_threshold) &&
        (bridge->fep_effects.debiasing_potential > 0.3f);

    /* Response urgency */
    float urgency = current_fe / HYPO_BIAS_FEP_SEVERE_THRESHOLD;
    if (urgency > 1.0f) urgency = 1.0f;
    bridge->fep_effects.response_urgency = urgency;

    /* Bias confidence based on precision */
    bridge->fep_effects.bias_confidence =
        bridge->state.current_precision / HYPO_BIAS_FEP_MAX_PRECISION;
    if (bridge->fep_effects.bias_confidence > 1.0f) {
        bridge->fep_effects.bias_confidence = 1.0f;
    }

    /* Update bias tracking */
    update_bias_tracking(bridge);

    /* Update statistics */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    bridge->stats.current_precision = bridge->state.current_precision;

    if (current_fe > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = current_fe;
    }
    if (bridge->fep_effects.dominant_strength > bridge->stats.max_bias_strength) {
        bridge->stats.max_bias_strength = bridge->fep_effects.dominant_strength;
    }

    /* Track detections by type */
    if (bridge->fep_effects.bias_level >= HYPO_BIAS_FEP_LEVEL_MODERATE) {
        bridge->stats.biases_detected++;
        bridge->bias_effects.biases_detected++;
        if (bridge->fep_effects.dominant_bias < HYPO_BIAS_TYPE_COUNT) {
            bridge->stats.detection_counts[bridge->fep_effects.dominant_bias]++;
        }
    }

    /* Track System 2 engagement */
    if (bridge->fep_effects.recommended_response >= HYPO_BIAS_FEP_RESPONSE_SLOW_DOWN) {
        float alpha = 0.05f;
        bridge->stats.system2_engagement_rate =
            (1.0f - alpha) * bridge->stats.system2_engagement_rate + alpha;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Core Operations Implementation
 * ============================================================================ */

/**
 * WHAT: Compute free energy from drive-induced bias
 * WHY:  Core FEP computation for bias detection
 * HOW:  Map drive urgencies to bias strengths
 */
int hypo_bias_fep_compute_fe(
    hypo_bias_fep_bridge_t* bridge,
    const hypo_drive_system_t* drives
) {
    if (!bridge || !drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bias_fep_compute_fe: bridge or drives is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Store drive urgencies */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->bias_effects.drive_urgencies[i] = drives->drives[i].urgency;
    }
    bridge->bias_effects.dominant_drive = drives->highest_priority;

    /* Compute bias strengths from drives */
    compute_bias_strengths(bridge);

    /* Compute free energy */
    float total_fe = compute_fe_from_biases(bridge);
    bridge->fep_effects.free_energy = total_fe;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Modulate precision based on cognitive load
 * WHY:  High load reduces ability to detect bias
 * HOW:  Scale precision inversely with load
 */
int hypo_bias_fep_modulate_precision(
    hypo_bias_fep_bridge_t* bridge,
    float cognitive_load
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (cognitive_load < 0.0f) cognitive_load = 0.0f;
    if (cognitive_load > 1.0f) cognitive_load = 1.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Store cognitive load */
    bridge->bias_effects.cognitive_load = cognitive_load;

    if (!bridge->config.enable_load_effects) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Higher load = lower precision (harder to detect bias) */
    float precision_mod = 1.0f - (cognitive_load * bridge->config.load_precision_scale);
    if (precision_mod < 0.1f) precision_mod = 0.1f;

    /* Add awareness boost */
    precision_mod += bridge->bias_effects.awareness * bridge->config.awareness_boost;

    float new_precision = HYPO_BIAS_FEP_DEFAULT_PRECISION * precision_mod;

    /* Smooth adaptation */
    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * new_precision;

    /* Clamp */
    if (bridge->state.current_precision < HYPO_BIAS_FEP_MIN_PRECISION) {
        bridge->state.current_precision = HYPO_BIAS_FEP_MIN_PRECISION;
    }
    if (bridge->state.current_precision > HYPO_BIAS_FEP_MAX_PRECISION) {
        bridge->state.current_precision = HYPO_BIAS_FEP_MAX_PRECISION;
    }

    bridge->fep_effects.precision = bridge->state.current_precision;
    bridge->stats.precision_adaptations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Get current FEP effects
 * WHY:  Allow inspection of current effects
 * HOW:  Copy effects structure
 */
int hypo_bias_fep_get_effects(
    const hypo_bias_fep_bridge_t* bridge,
    hypo_bias_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bias_fep_get_effects: bridge or effects is NULL");
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

/**
 * WHAT: Get bridge statistics
 * WHY:  Performance monitoring
 * HOW:  Copy statistics structure
 */
int hypo_bias_fep_get_stats(
    const hypo_bias_fep_bridge_t* bridge,
    hypo_bias_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bias_fep_get_stats: bridge or stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Bias API Implementation
 * ============================================================================ */

/**
 * WHAT: Get bias strength for a specific type
 * WHY:  Enable bias-specific queries
 * HOW:  Return cached strength
 */
float hypo_bias_fep_get_strength(
    const hypo_bias_fep_bridge_t* bridge,
    hypo_bias_type_t bias_type
) {
    if (!bridge || bias_type >= HYPO_BIAS_TYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(bridge ? NIMCP_ERROR_INVALID_PARAM : NIMCP_ERROR_NULL_POINTER, "hypo_bias_fep_get_strength: bridge is NULL or bias_type invalid");
        return -1.0f;
    }

    return bridge->fep_effects.bias_strengths[bias_type];
}

/**
 * WHAT: Get all bias strengths
 * WHY:  Enable comprehensive bias assessment
 * HOW:  Copy strength array
 */
int hypo_bias_fep_get_strengths(
    const hypo_bias_fep_bridge_t* bridge,
    float* strengths
) {
    if (!bridge || !strengths) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bias_fep_get_strengths: bridge or strengths is NULL");
        return -1;
    }

    for (int i = 0; i < HYPO_BIAS_TYPE_COUNT; i++) {
        strengths[i] = bridge->fep_effects.bias_strengths[i];
    }

    return 0;
}

/**
 * WHAT: Trigger debiasing for a specific bias type
 * WHY:  Active inference response to high bias
 * HOW:  Reduce bias strength, update stats
 */
int hypo_bias_fep_trigger_debias(
    hypo_bias_fep_bridge_t* bridge,
    hypo_bias_type_t bias_type,
    float intensity
) {
    if (!bridge || bias_type >= HYPO_BIAS_TYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(bridge ? NIMCP_ERROR_INVALID_PARAM : NIMCP_ERROR_NULL_POINTER, "hypo_bias_fep_trigger_debias: bridge is NULL or bias_type invalid");
        return -1;
    }

    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Apply debiasing with diminishing returns */
    float current_strength = bridge->fep_effects.bias_strengths[bias_type];
    float reduction = current_strength * intensity * bridge->config.debias_strength;

    bridge->fep_effects.bias_strengths[bias_type] -= reduction;
    if (bridge->fep_effects.bias_strengths[bias_type] < 0.0f) {
        bridge->fep_effects.bias_strengths[bias_type] = 0.0f;
    }

    /* Update tracking */
    bridge->state.bias_tracking.last_debias_time_ms = nimcp_platform_time_monotonic_ms();
    bridge->state.bias_tracking.last_debiased = bias_type;

    /* Update stats */
    bridge->stats.debias_triggered++;
    bridge->bias_effects.debias_attempts++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report debiasing outcome
 * WHY:  Learn from intervention outcomes
 * HOW:  Update running average of debiasing effectiveness
 */
int hypo_bias_fep_report_debias_outcome(
    hypo_bias_fep_bridge_t* bridge,
    hypo_bias_type_t bias_type,
    float reduction
) {
    if (!bridge || bias_type >= HYPO_BIAS_TYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(bridge ? NIMCP_ERROR_INVALID_PARAM : NIMCP_ERROR_NULL_POINTER, "hypo_bias_fep_report_debias_outcome: bridge is NULL or bias_type invalid");
        return -1;
    }

    if (reduction < 0.0f) reduction = 0.0f;
    if (reduction > 1.0f) reduction = 1.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update running average */
    float alpha = bridge->config.debias_learning_rate;
    bridge->bias_effects.avg_bias_reduction =
        (1.0f - alpha) * bridge->bias_effects.avg_bias_reduction + alpha * reduction;

    bridge->stats.avg_debias_reduction = bridge->bias_effects.avg_bias_reduction;

    if (reduction > 0.1f) {
        bridge->stats.debias_successful++;
        bridge->bias_effects.debias_successes++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int hypo_bias_fep_connect_bio_async(
    hypo_bias_fep_bridge_t* bridge,
    bio_router_t* router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    (void)router;

    if (bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HYPO_BIAS_FEP,
        .module_name = "hypo_bias_fep_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hypo Bias FEP bridge connected to bio-async");
    }

    return 0;
}

int hypo_bias_fep_disconnect_bio_async(hypo_bias_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Hypo Bias FEP bridge disconnected from bio-async");
    return 0;
}

int hypo_bias_fep_process_messages(
    hypo_bias_fep_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }
    (void)max_messages;
    return 0;
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

const char* hypo_bias_fep_type_name(hypo_bias_type_t bias_type) {
    switch (bias_type) {
        case HYPO_BIAS_RISK_AVERSION:
            return "Risk Aversion";
        case HYPO_BIAS_PRESENT_BIAS:
            return "Present Bias";
        case HYPO_BIAS_INGROUP_FAVORITISM:
            return "Ingroup Favoritism";
        case HYPO_BIAS_MATE_PREFERENCE:
            return "Mate Preference";
        case HYPO_BIAS_NOVELTY_SEEKING:
            return "Novelty Seeking";
        case HYPO_BIAS_CONFIRMATION:
            return "Confirmation";
        case HYPO_BIAS_AVAILABILITY:
            return "Availability";
        case HYPO_BIAS_ANCHORING:
            return "Anchoring";
        default:
            return "Unknown";
    }
}

const char* hypo_bias_fep_level_name(hypo_bias_fep_level_t level) {
    switch (level) {
        case HYPO_BIAS_FEP_LEVEL_MINIMAL:
            return "Minimal";
        case HYPO_BIAS_FEP_LEVEL_MILD:
            return "Mild";
        case HYPO_BIAS_FEP_LEVEL_MODERATE:
            return "Moderate";
        case HYPO_BIAS_FEP_LEVEL_STRONG:
            return "Strong";
        case HYPO_BIAS_FEP_LEVEL_SEVERE:
            return "Severe";
        default:
            return "Unknown";
    }
}

const char* hypo_bias_fep_response_name(hypo_bias_fep_response_t response) {
    switch (response) {
        case HYPO_BIAS_FEP_RESPONSE_NONE:
            return "None";
        case HYPO_BIAS_FEP_RESPONSE_MONITOR:
            return "Monitor";
        case HYPO_BIAS_FEP_RESPONSE_ALERT:
            return "Alert";
        case HYPO_BIAS_FEP_RESPONSE_SLOW_DOWN:
            return "Slow Down";
        case HYPO_BIAS_FEP_RESPONSE_DEBIAS:
            return "Debias";
        default:
            return "Unknown";
    }
}

void hypo_bias_fep_print_summary(const hypo_bias_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Hypo Bias FEP Bridge: NULL\n");
        return;
    }

    printf("=== Hypothalamus Bias FEP Bridge Summary ===\n");
    printf("State:\n");
    printf("  Active: %s\n", bridge->state.active ? "yes" : "no");
    printf("  Updates: %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Precision: %.3f\n", bridge->state.current_precision);
    printf("\n");
    printf("Bias Strengths:\n");
    for (int i = 0; i < HYPO_BIAS_TYPE_COUNT; i++) {
        printf("  %s: %.3f\n",
               hypo_bias_fep_type_name((hypo_bias_type_t)i),
               bridge->fep_effects.bias_strengths[i]);
    }
    printf("  Total: %.3f\n", bridge->fep_effects.total_bias);
    printf("  Dominant: %s (%.3f)\n",
           hypo_bias_fep_type_name(bridge->fep_effects.dominant_bias),
           bridge->fep_effects.dominant_strength);
    printf("\n");
    printf("FEP Effects:\n");
    printf("  Free Energy: %.3f\n", bridge->fep_effects.free_energy);
    printf("  Bias Level: %s\n",
           hypo_bias_fep_level_name(bridge->fep_effects.bias_level));
    printf("  Debiasing Potential: %.3f\n", bridge->fep_effects.debiasing_potential);
    printf("  Debiasing Recommended: %s\n",
           bridge->fep_effects.debiasing_recommended ? "yes" : "no");
    printf("  Recommended Response: %s\n",
           hypo_bias_fep_response_name(bridge->fep_effects.recommended_response));
    printf("\n");
    printf("Statistics:\n");
    printf("  Biases Detected: %lu\n", (unsigned long)bridge->stats.biases_detected);
    printf("  Debias Triggered: %lu\n", (unsigned long)bridge->stats.debias_triggered);
    printf("  Debias Successful: %lu\n", (unsigned long)bridge->stats.debias_successful);
    printf("  System 2 Engagement: %.1f%%\n",
           bridge->stats.system2_engagement_rate * 100.0f);
    printf("=============================================\n");
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

/**
 * WHAT: Map drive type to primary bias type
 * WHY:  Each drive induces specific biases
 * HOW:  Direct mapping based on biological basis
 */
static hypo_bias_type_t map_drive_to_bias(hypo_drive_type_t drive) {
    switch (drive) {
        case HYPO_DRIVE_SAFETY:
            return HYPO_BIAS_RISK_AVERSION;
        case HYPO_DRIVE_HUNGER:
        case HYPO_DRIVE_THIRST:
            return HYPO_BIAS_PRESENT_BIAS;
        case HYPO_DRIVE_SOCIAL:
            return HYPO_BIAS_INGROUP_FAVORITISM;
        case HYPO_DRIVE_AUTONOMY:
            return HYPO_BIAS_MATE_PREFERENCE;
        case HYPO_DRIVE_CURIOSITY:
            return HYPO_BIAS_NOVELTY_SEEKING;
        case HYPO_DRIVE_FATIGUE:
            return HYPO_BIAS_AVAILABILITY;
        default:
            return HYPO_BIAS_ANCHORING;
    }
}

/**
 * WHAT: Compute bias strengths from drive urgencies
 * WHY:  Drives induce cognitive biases
 * HOW:  Map drive urgency to bias strength via scaling
 */
static void compute_bias_strengths(hypo_bias_fep_bridge_t* bridge) {
    /* Reset all biases */
    for (int i = 0; i < HYPO_BIAS_TYPE_COUNT; i++) {
        bridge->fep_effects.bias_strengths[i] = 0.0f;
    }

    /* Map each drive to its primary bias */
    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        float urgency = bridge->bias_effects.drive_urgencies[d];
        float scale = bridge->config.drive_to_bias_scale[d];
        hypo_bias_type_t bias = map_drive_to_bias((hypo_drive_type_t)d);

        /* Accumulate (drives can contribute to same bias) */
        float contribution = urgency * scale;

        /* Amplify under cognitive load */
        if (bridge->config.enable_load_effects) {
            float load = bridge->bias_effects.cognitive_load;
            contribution *= (1.0f + load * bridge->config.load_bias_amplify);
        }

        bridge->fep_effects.bias_strengths[bias] += contribution;
    }

    /* Clamp and find dominant */
    float total = 0.0f;
    float max_strength = 0.0f;
    hypo_bias_type_t dominant = HYPO_BIAS_RISK_AVERSION;

    for (int i = 0; i < HYPO_BIAS_TYPE_COUNT; i++) {
        if (bridge->fep_effects.bias_strengths[i] > HYPO_BIAS_FEP_MAX_STRENGTH) {
            bridge->fep_effects.bias_strengths[i] = HYPO_BIAS_FEP_MAX_STRENGTH;
        }

        total += bridge->fep_effects.bias_strengths[i];

        if (bridge->fep_effects.bias_strengths[i] > max_strength) {
            max_strength = bridge->fep_effects.bias_strengths[i];
            dominant = (hypo_bias_type_t)i;
        }
    }

    bridge->fep_effects.total_bias = total;
    bridge->fep_effects.dominant_bias = dominant;
    bridge->fep_effects.dominant_strength = max_strength;
}

/**
 * WHAT: Compute free energy from bias strengths
 * WHY:  Map bias domain to FEP domain
 * HOW:  Precision-weighted sum of bias deviations from baseline
 */
static float compute_fe_from_biases(const hypo_bias_fep_bridge_t* bridge) {
    float fe = 0.0f;

    /* FE from bias strengths (deviation from rational baseline = 0) */
    for (int i = 0; i < HYPO_BIAS_TYPE_COUNT; i++) {
        float strength = bridge->fep_effects.bias_strengths[i];
        /* Quadratic cost for bias */
        fe += strength * strength * bridge->config.drive_fe_weight;
    }

    /* Scale by precision */
    fe *= bridge->state.current_precision;

    return fe;
}

/**
 * WHAT: Classify bias level from free energy
 * WHY:  Map continuous FE to discrete categories
 * HOW:  Threshold-based classification
 */
static hypo_bias_fep_level_t classify_bias_level(
    float free_energy,
    const hypo_bias_fep_config_t* config
) {
    if (free_energy >= HYPO_BIAS_FEP_SEVERE_THRESHOLD) {
        return HYPO_BIAS_FEP_LEVEL_SEVERE;
    } else if (free_energy >= config->free_energy_threshold) {
        return HYPO_BIAS_FEP_LEVEL_STRONG;
    } else if (free_energy >= HYPO_BIAS_FEP_MEDIUM_THRESHOLD) {
        return HYPO_BIAS_FEP_LEVEL_MODERATE;
    } else if (free_energy >= HYPO_BIAS_FEP_LOW_THRESHOLD) {
        return HYPO_BIAS_FEP_LEVEL_MILD;
    } else {
        return HYPO_BIAS_FEP_LEVEL_MINIMAL;
    }
}

/**
 * WHAT: Determine appropriate response
 * WHY:  Active inference selects debiasing action
 * HOW:  Map bias level to response type
 */
static hypo_bias_fep_response_t determine_response(
    hypo_bias_fep_level_t level,
    float total_bias
) {
    switch (level) {
        case HYPO_BIAS_FEP_LEVEL_SEVERE:
            return HYPO_BIAS_FEP_RESPONSE_DEBIAS;

        case HYPO_BIAS_FEP_LEVEL_STRONG:
            if (total_bias > 1.5f) {
                return HYPO_BIAS_FEP_RESPONSE_DEBIAS;
            }
            return HYPO_BIAS_FEP_RESPONSE_SLOW_DOWN;

        case HYPO_BIAS_FEP_LEVEL_MODERATE:
            return HYPO_BIAS_FEP_RESPONSE_ALERT;

        case HYPO_BIAS_FEP_LEVEL_MILD:
            return HYPO_BIAS_FEP_RESPONSE_MONITOR;

        case HYPO_BIAS_FEP_LEVEL_MINIMAL:
        default:
            return HYPO_BIAS_FEP_RESPONSE_NONE;
    }
}

/**
 * WHAT: Update running averages for metrics
 * WHY:  Smooth metrics over time for stability
 * HOW:  Exponential moving average
 */
static void update_running_averages(
    hypo_bias_fep_bridge_t* bridge,
    float free_energy,
    float surprise,
    float pred_error
) {
    const float alpha = 0.1f;

    bridge->state.avg_surprise =
        (1.0f - alpha) * bridge->state.avg_surprise + alpha * surprise;

    bridge->state.avg_prediction_error =
        (1.0f - alpha) * bridge->state.avg_prediction_error + alpha * pred_error;

    bridge->stats.avg_free_energy =
        (1.0f - alpha) * bridge->stats.avg_free_energy + alpha * free_energy;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    bridge->stats.avg_prediction_error = bridge->state.avg_prediction_error;
}

/**
 * WHAT: Update bias tracking
 * WHY:  Track bias evolution for predictions
 * HOW:  Store history, update predictions
 */
static void update_bias_tracking(hypo_bias_fep_bridge_t* bridge) {
    hypo_bias_tracking_t* tracking = &bridge->state.bias_tracking;

    /* Store in history */
    uint32_t idx = tracking->history_idx;
    for (int i = 0; i < HYPO_BIAS_TYPE_COUNT; i++) {
        tracking->bias_history[i][idx] = bridge->fep_effects.bias_strengths[i];

        /* Update prediction using EMA */
        float alpha = 0.1f;
        tracking->predicted_bias[i] =
            (1.0f - alpha) * tracking->predicted_bias[i] +
            alpha * bridge->fep_effects.bias_strengths[i];

        /* Compute velocity */
        if (idx > 0) {
            float prev = tracking->bias_history[i][(idx - 1) % 16];
            tracking->bias_velocity[i] = bridge->fep_effects.bias_strengths[i] - prev;
        }
    }

    tracking->history_idx = (idx + 1) % 16;
}
