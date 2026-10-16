/**
 * @file nimcp_hypo_bio_async_fep_bridge.c
 * @brief Implementation of Hypothalamus Bio-Async FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hypothalamic bio-async message processing
 * WHY:  Message timing anomalies represent high-surprise homeostatic events
 * HOW:  Map message timing to prediction errors, rate to free energy
 */

#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_bio_async_fep_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hypo_bio_async_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static float compute_fe_from_timing(float rate_deviation,
                                     float interval_deviation,
                                     const hypo_bio_async_fep_config_t* config);

static hypo_bio_async_fep_level_t classify_disruption_level(
    float free_energy,
    const hypo_bio_async_fep_config_t* config);

static hypo_bio_async_fep_response_t determine_response(
    hypo_bio_async_fep_level_t level,
    float urgency);

static void update_running_averages(hypo_bio_async_fep_bridge_t* bridge,
                                    float free_energy,
                                    float surprise,
                                    float pred_error);

static void update_temporal_pattern(hypo_bio_async_fep_bridge_t* bridge,
                                    uint64_t timestamp_ms);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * WHAT: Get default configuration for hypothalamus bio-async FEP bridge
 * WHY:  Provide sensible starting point for homeostatic monitoring
 * HOW:  Set biologically-plausible defaults for all parameters
 */
int hypo_bio_async_fep_default_config(hypo_bio_async_fep_config_t* config) {
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

    /* Detection parameters */
    config->free_energy_threshold = HYPO_BIO_ASYNC_FEP_SIGNIFICANT_THRESHOLD;
    config->surprise_threshold = HYPO_BIO_ASYNC_FEP_SURPRISE_ANOMALY;
    config->precision_learning_rate = 0.05f;

    /* Timing analysis */
    config->rate_window_ms = HYPO_BIO_ASYNC_FEP_RATE_WINDOW_MS;
    config->expected_msg_rate = 50.0f;  /* 50 messages/sec baseline */
    config->rate_deviation_threshold = 3.0f;

    /* Drive integration */
    config->modulate_by_drive_urgency = true;
    config->urgency_precision_scale = 2.0f;

    /* Learning */
    config->enable_online_learning = true;
    config->learning_rate = 0.01f;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Create hypothalamus bio-async FEP bridge
 * WHY:  Initialize FEP integration for homeostatic monitoring
 * HOW:  Allocate structure, initialize base, apply configuration
 */
hypo_bio_async_fep_bridge_t* hypo_bio_async_fep_create(
    const hypo_bio_async_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
) {
    /* Validate required parameters - fep_system required, drive_system optional */
    if (!fep_system) {
        NIMCP_LOGGING_ERROR("Hypo Bio-Async FEP bridge: fep_system is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_fep_create: fep_system is NULL");
        return NULL;
    }

    /* Allocate bridge structure */
    hypo_bio_async_fep_bridge_t* bridge = (hypo_bio_async_fep_bridge_t*)nimcp_malloc(
        sizeof(hypo_bio_async_fep_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Hypo Bio-Async FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_bio_async_fep_create: bridge is NULL");
        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(hypo_bio_async_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hypo_bio_async_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->drive_system = drive_system;
    bridge->fep_system = fep_system;

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "hypo_bio_async_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Hypo Bio-Async FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = HYPO_BIO_ASYNC_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_level = HYPO_BIO_ASYNC_FEP_LEVEL_NORMAL;

    /* Initialize temporal pattern tracking */
    bridge->state.temporal.window_start_ms = nimcp_platform_time_monotonic_ms();
    bridge->state.temporal.messages_in_window = 0;
    bridge->state.temporal.mean_interval_ms = 20.0f;
    bridge->state.temporal.predicted_rate = bridge->config.expected_msg_rate;
    bridge->state.temporal.predicted_interval = 1000.0f / bridge->config.expected_msg_rate;

    /* Initialize effects */
    bridge->fep_effects.disruption_level = HYPO_BIO_ASYNC_FEP_LEVEL_NORMAL;
    bridge->fep_effects.precision = HYPO_BIO_ASYNC_FEP_DEFAULT_PRECISION;
    bridge->fep_effects.homeostatic_health = 1.0f;
    bridge->fep_effects.detected_anomaly = HYPO_BIO_ASYNC_ANOMALY_NONE;

    /* Bio-async not yet connected */
    bridge->base.bio_async_enabled = false;
    bridge->base.module_id = BIO_MODULE_HYPO_BIO_ASYNC_FEP;
    bridge->base.module_name = "hypo_bio_async_fep_bridge";

    NIMCP_LOGGING_INFO("Hypo Bio-Async FEP bridge created");
    return bridge;
}

/**
 * WHAT: Destroy hypothalamus bio-async FEP bridge
 * WHY:  Clean up all resources to prevent memory leaks
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void hypo_bio_async_fep_destroy(hypo_bio_async_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        hypo_bio_async_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    /* Free bridge memory */
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Hypo Bio-Async FEP bridge destroyed");
}

/**
 * WHAT: Reset bridge to initial state
 * WHY:  Allow reuse without full recreation
 * HOW:  Clear state and statistics, preserve connections
 */
int hypo_bio_async_fep_reset(hypo_bio_async_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->state.update_count = 0;
    bridge->state.detection_count = 0;
    bridge->state.current_precision = HYPO_BIO_ASYNC_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_level = HYPO_BIO_ASYNC_FEP_LEVEL_NORMAL;
    bridge->state.last_detection_time_ms = 0;

    /* Reset temporal pattern */
    bridge->state.temporal.window_start_ms = nimcp_platform_time_monotonic_ms();
    bridge->state.temporal.messages_in_window = 0;
    memset(bridge->state.temporal.rate_history, 0,
           sizeof(bridge->state.temporal.rate_history));
    bridge->state.temporal.rate_history_idx = 0;

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(hypo_bio_async_fep_effects_t));
    bridge->fep_effects.homeostatic_health = 1.0f;
    bridge->fep_effects.precision = HYPO_BIO_ASYNC_FEP_DEFAULT_PRECISION;

    memset(&bridge->async_effects, 0, sizeof(bio_async_to_fep_effects_t));

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(hypo_bio_async_fep_stats_t));
    bridge->stats.current_precision = HYPO_BIO_ASYNC_FEP_DEFAULT_PRECISION;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Hypo Bio-Async FEP bridge reset");
    return 0;
}

/**
 * WHAT: Update bridge state
 * WHY:  Main update loop for bridge synchronization
 * HOW:  Compute effects, apply precision modulation, update state
 */
int hypo_bio_async_fep_update(hypo_bio_async_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current FEP metrics */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    /* Update running averages */
    update_running_averages(bridge, current_fe, surprise, pred_error);

    /* Store in FEP effects */
    bridge->fep_effects.free_energy = current_fe;
    bridge->fep_effects.prediction_error = pred_error;
    bridge->fep_effects.precision = bridge->state.current_precision;

    /* Classify disruption level */
    bridge->fep_effects.disruption_level = classify_disruption_level(
        current_fe, &bridge->config
    );

    /* Compute homeostatic health estimate */
    float health = 1.0f - (current_fe / HYPO_BIO_ASYNC_FEP_CRITICAL_THRESHOLD);
    if (health < 0.0f) health = 0.0f;
    if (health > 1.0f) health = 1.0f;
    bridge->fep_effects.homeostatic_health = health;

    /* Determine response */
    float urgency = current_fe / HYPO_BIO_ASYNC_FEP_CRITICAL_THRESHOLD;
    if (urgency > 1.0f) urgency = 1.0f;
    bridge->fep_effects.response_urgency = urgency;
    bridge->fep_effects.recommended_response = determine_response(
        bridge->fep_effects.disruption_level, urgency
    );

    /* Check rate window rollover */
    uint64_t now_ms = nimcp_platform_time_monotonic_ms();
    if (now_ms - bridge->state.temporal.window_start_ms >= bridge->config.rate_window_ms) {
        float rate = (float)bridge->state.temporal.messages_in_window *
                     (1000.0f / (float)bridge->config.rate_window_ms);
        bridge->state.temporal.rate_history[bridge->state.temporal.rate_history_idx] = rate;
        bridge->state.temporal.rate_history_idx =
            (bridge->state.temporal.rate_history_idx + 1) % 16;

        bridge->async_effects.current_msg_rate = rate;
        bridge->state.temporal.window_start_ms = now_ms;
        bridge->state.temporal.messages_in_window = 0;

        /* Update prediction */
        float alpha = 0.1f;
        float new_predicted = (1.0f - alpha) * bridge->state.temporal.predicted_rate + alpha * rate;
        if (isfinite(new_predicted)) bridge->state.temporal.predicted_rate = new_predicted;
    }

    /* Update statistics */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    bridge->stats.current_precision = bridge->state.current_precision;

    if (current_fe > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = current_fe;
    }
    if (surprise > bridge->stats.max_surprise) {
        bridge->stats.max_surprise = surprise;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Core Operations Implementation
 * ============================================================================ */

/**
 * WHAT: Compute free energy from drive state
 * WHY:  Core FEP computation for homeostatic monitoring
 * HOW:  Map drive deviations to free energy
 */
int hypo_bio_async_fep_compute_fe(
    hypo_bio_async_fep_bridge_t* bridge,
    const hypo_drive_system_t* drives
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_fep_compute_fe: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute FE from drive deviations */
    float total_fe = 0.0f;

    if (drives) {
        for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
            float deviation = fabsf(drives->drives[i].deviation);
            float urgency = drives->drives[i].urgency;

            /* Higher deviation from setpoint = higher free energy */
            total_fe += deviation * urgency * bridge->config.drive_fe_weight;
        }
    }

    /* Add timing-based free energy */
    float rate_deviation = 0.0f;
    if (bridge->state.temporal.predicted_rate > 0.01f) {
        rate_deviation = fabsf(bridge->async_effects.current_msg_rate -
                               bridge->state.temporal.predicted_rate) /
                         bridge->state.temporal.predicted_rate;
    }

    float interval_deviation = 0.0f;
    if (bridge->state.temporal.predicted_interval > 0.01f) {
        interval_deviation = fabsf(bridge->state.temporal.mean_interval_ms -
                                   bridge->state.temporal.predicted_interval) /
                             bridge->state.temporal.predicted_interval;
    }

    float timing_fe = compute_fe_from_timing(rate_deviation, interval_deviation,
                                              &bridge->config);
    total_fe += timing_fe;

    /* Update effects */
    bridge->fep_effects.free_energy = total_fe;

    /* Update drive urgency tracking */
    if (drives) {
        bridge->async_effects.current_drive_urgency = drives->drives[drives->highest_priority].urgency;
    }
    bridge->async_effects.homeostatic_stress = (total_fe > HYPO_BIO_ASYNC_FEP_SIGNIFICANT_THRESHOLD);

    /* Track detection */
    if (total_fe > bridge->config.free_energy_threshold) {
        bridge->state.detection_count++;
        bridge->stats.fep_detections++;
        bridge->stats.disruptions_detected++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Modulate precision based on fatigue
 * WHY:  Precision represents confidence; fatigue reduces confidence
 * HOW:  Scale precision inversely with fatigue
 */
int hypo_bio_async_fep_modulate_precision(
    hypo_bio_async_fep_bridge_t* bridge,
    float fatigue
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (fatigue < 0.0f) fatigue = 0.0f;
    if (fatigue > 1.0f) fatigue = 1.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Higher fatigue = lower precision */
    float precision_mod = 1.0f - (fatigue * bridge->config.precision_modulation);
    if (precision_mod < 0.1f) precision_mod = 0.1f;

    float new_precision = HYPO_BIO_ASYNC_FEP_DEFAULT_PRECISION * precision_mod;

    /* Smooth adaptation */
    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * new_precision;

    /* Clamp */
    if (bridge->state.current_precision < HYPO_BIO_ASYNC_FEP_MIN_PRECISION) {
        bridge->state.current_precision = HYPO_BIO_ASYNC_FEP_MIN_PRECISION;
    }
    if (bridge->state.current_precision > HYPO_BIO_ASYNC_FEP_MAX_PRECISION) {
        bridge->state.current_precision = HYPO_BIO_ASYNC_FEP_MAX_PRECISION;
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
int hypo_bio_async_fep_get_effects(
    const hypo_bio_async_fep_bridge_t* bridge,
    hypo_bio_async_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_fep_get_effects: bridge or effects is NULL");
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
int hypo_bio_async_fep_get_stats(
    const hypo_bio_async_fep_bridge_t* bridge,
    hypo_bio_async_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_bio_async_fep_get_stats: bridge or stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

/**
 * WHAT: Connect to bio-async router
 * WHY:  Enable inter-module homeostatic notifications
 * HOW:  Register module with router
 */
int hypo_bio_async_fep_connect_bio_async(
    hypo_bio_async_fep_bridge_t* bridge,
    bio_router_t* router
) {
    if (!bridge) {
        return 0;  /* Graceful no-op for NULL bridge */
    }

    /* Suppress unused parameter warning - router used via global */
    (void)router;

    if (bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HYPO_BIO_ASYNC_FEP,
        .module_name = "hypo_bio_async_fep_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hypo Bio-Async FEP bridge connected to bio-async");
    }

    return 0;
}

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of inter-module communication
 * HOW:  Unregister module from router
 */
int hypo_bio_async_fep_disconnect_bio_async(hypo_bio_async_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Hypo Bio-Async FEP bridge disconnected from bio-async");
    return 0;
}

/**
 * WHAT: Process pending bio-async messages
 * WHY:  Handle incoming homeostatic notifications
 * HOW:  Uses bio_router_process_inbox for message handling
 */
int hypo_bio_async_fep_process_messages(
    hypo_bio_async_fep_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    uint32_t processed = bio_router_process_inbox(bridge->base.bio_ctx, max_messages);

    /* Update temporal pattern for each message processed */
    uint64_t now_ms = nimcp_platform_time_monotonic_ms();
    for (uint32_t i = 0; i < processed; i++) {
        update_temporal_pattern(bridge, now_ms);
        bridge->async_effects.messages_processed++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return (int)processed;
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

/**
 * WHAT: Get human-readable name for disruption level
 * WHY:  Facilitate logging and debugging
 * HOW:  Switch on enum value
 */
const char* hypo_bio_async_fep_level_name(hypo_bio_async_fep_level_t level) {
    switch (level) {
        case HYPO_BIO_ASYNC_FEP_LEVEL_NORMAL:
            return "Normal";
        case HYPO_BIO_ASYNC_FEP_LEVEL_MILD:
            return "Mild";
        case HYPO_BIO_ASYNC_FEP_LEVEL_MODERATE:
            return "Moderate";
        case HYPO_BIO_ASYNC_FEP_LEVEL_SIGNIFICANT:
            return "Significant";
        case HYPO_BIO_ASYNC_FEP_LEVEL_CRITICAL:
            return "Critical";
        default:
            return "Unknown";
    }
}

/**
 * WHAT: Get human-readable name for response type
 * WHY:  Facilitate logging and debugging
 * HOW:  Switch on enum value
 */
const char* hypo_bio_async_fep_response_name(hypo_bio_async_fep_response_t response) {
    switch (response) {
        case HYPO_BIO_ASYNC_FEP_RESPONSE_NONE:
            return "None";
        case HYPO_BIO_ASYNC_FEP_RESPONSE_MONITOR:
            return "Monitor";
        case HYPO_BIO_ASYNC_FEP_RESPONSE_THROTTLE:
            return "Throttle";
        case HYPO_BIO_ASYNC_FEP_RESPONSE_PRIORITIZE:
            return "Prioritize";
        case HYPO_BIO_ASYNC_FEP_RESPONSE_EMERGENCY:
            return "Emergency";
        default:
            return "Unknown";
    }
}

/**
 * WHAT: Get human-readable name for anomaly type
 * WHY:  Facilitate logging and debugging
 * HOW:  Switch on enum value
 */
const char* hypo_bio_async_fep_anomaly_name(hypo_bio_async_anomaly_type_t anomaly) {
    switch (anomaly) {
        case HYPO_BIO_ASYNC_ANOMALY_NONE:
            return "None";
        case HYPO_BIO_ASYNC_ANOMALY_TIMING:
            return "Timing";
        case HYPO_BIO_ASYNC_ANOMALY_RATE:
            return "Rate";
        case HYPO_BIO_ASYNC_ANOMALY_QUEUE:
            return "Queue";
        case HYPO_BIO_ASYNC_ANOMALY_PATTERN:
            return "Pattern";
        default:
            return "Unknown";
    }
}

/**
 * WHAT: Print bridge summary
 * WHY:  Debug and monitoring support
 * HOW:  Format and print key metrics
 */
void hypo_bio_async_fep_print_summary(const hypo_bio_async_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Hypo Bio-Async FEP Bridge: NULL\n");
        return;
    }

    printf("=== Hypothalamus Bio-Async FEP Bridge Summary ===\n");
    printf("State:\n");
    printf("  Active: %s\n", bridge->state.active ? "yes" : "no");
    printf("  Updates: %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Precision: %.3f\n", bridge->state.current_precision);
    printf("\n");
    printf("FEP Effects:\n");
    printf("  Free Energy: %.3f\n", bridge->fep_effects.free_energy);
    printf("  Prediction Error: %.3f\n", bridge->fep_effects.prediction_error);
    printf("  Disruption Level: %s\n",
           hypo_bio_async_fep_level_name(bridge->fep_effects.disruption_level));
    printf("  Homeostatic Health: %.3f\n", bridge->fep_effects.homeostatic_health);
    printf("  Recommended Response: %s\n",
           hypo_bio_async_fep_response_name(bridge->fep_effects.recommended_response));
    printf("================================================\n");
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

/**
 * WHAT: Compute free energy from timing metrics
 * WHY:  Map timing domain to FEP domain
 * HOW:  Weighted combination of rate and interval deviations
 */
static float compute_fe_from_timing(
    float rate_deviation,
    float interval_deviation,
    const hypo_bio_async_fep_config_t* config
) {
    float rate_fe = config->prediction_error_gain * rate_deviation;
    float interval_fe = config->prediction_error_gain * interval_deviation;

    return rate_fe + interval_fe;
}

/**
 * WHAT: Classify disruption level from free energy
 * WHY:  Map continuous FE to discrete categories
 * HOW:  Threshold-based classification
 */
static hypo_bio_async_fep_level_t classify_disruption_level(
    float free_energy,
    const hypo_bio_async_fep_config_t* config
) {
    if (free_energy >= HYPO_BIO_ASYNC_FEP_CRITICAL_THRESHOLD) {
        return HYPO_BIO_ASYNC_FEP_LEVEL_CRITICAL;
    } else if (free_energy >= config->free_energy_threshold) {
        return HYPO_BIO_ASYNC_FEP_LEVEL_SIGNIFICANT;
    } else if (free_energy >= HYPO_BIO_ASYNC_FEP_MILD_THRESHOLD) {
        return HYPO_BIO_ASYNC_FEP_LEVEL_MODERATE;
    } else if (free_energy >= HYPO_BIO_ASYNC_FEP_NORMAL_THRESHOLD) {
        return HYPO_BIO_ASYNC_FEP_LEVEL_MILD;
    } else {
        return HYPO_BIO_ASYNC_FEP_LEVEL_NORMAL;
    }
}

/**
 * WHAT: Determine appropriate response
 * WHY:  Active inference selects actions to minimize expected FE
 * HOW:  Map disruption level and urgency to response type
 */
static hypo_bio_async_fep_response_t determine_response(
    hypo_bio_async_fep_level_t level,
    float urgency
) {
    switch (level) {
        case HYPO_BIO_ASYNC_FEP_LEVEL_CRITICAL:
            return HYPO_BIO_ASYNC_FEP_RESPONSE_EMERGENCY;

        case HYPO_BIO_ASYNC_FEP_LEVEL_SIGNIFICANT:
            if (urgency > 0.8f) {
                return HYPO_BIO_ASYNC_FEP_RESPONSE_EMERGENCY;
            }
            return HYPO_BIO_ASYNC_FEP_RESPONSE_PRIORITIZE;

        case HYPO_BIO_ASYNC_FEP_LEVEL_MODERATE:
            return HYPO_BIO_ASYNC_FEP_RESPONSE_THROTTLE;

        case HYPO_BIO_ASYNC_FEP_LEVEL_MILD:
            return HYPO_BIO_ASYNC_FEP_RESPONSE_MONITOR;

        case HYPO_BIO_ASYNC_FEP_LEVEL_NORMAL:
        default:
            return HYPO_BIO_ASYNC_FEP_RESPONSE_NONE;
    }
}

/**
 * WHAT: Update running averages for metrics
 * WHY:  Smooth metrics over time for stability
 * HOW:  Exponential moving average
 */
static void update_running_averages(
    hypo_bio_async_fep_bridge_t* bridge,
    float free_energy,
    float surprise,
    float pred_error
) {
    const float alpha = 0.1f;

    float new_surprise = (1.0f - alpha) * bridge->state.avg_surprise + alpha * surprise;
    if (isfinite(new_surprise)) bridge->state.avg_surprise = new_surprise;

    float new_pred_error = (1.0f - alpha) * bridge->state.avg_prediction_error + alpha * pred_error;
    if (isfinite(new_pred_error)) bridge->state.avg_prediction_error = new_pred_error;

    float new_avg_fe = (1.0f - alpha) * bridge->stats.avg_free_energy + alpha * free_energy;
    if (isfinite(new_avg_fe)) bridge->stats.avg_free_energy = new_avg_fe;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    bridge->stats.avg_prediction_error = bridge->state.avg_prediction_error;
}

/**
 * WHAT: Update temporal pattern tracking
 * WHY:  Track message timing for FEP predictions
 * HOW:  Compute intervals, update statistics
 */
static void update_temporal_pattern(
    hypo_bio_async_fep_bridge_t* bridge,
    uint64_t timestamp_ms
) {
    hypo_bio_async_temporal_pattern_t* temporal = &bridge->state.temporal;

    float interval_ms = 0.0f;
    if (temporal->last_message_time_ms > 0) {
        interval_ms = (float)(timestamp_ms - temporal->last_message_time_ms);
    }
    temporal->last_message_time_ms = timestamp_ms;

    if (interval_ms > 0.0f) {
        float alpha = 0.1f;
        float new_mean = (1.0f - alpha) * temporal->mean_interval_ms + alpha * interval_ms;
        if (isfinite(new_mean)) temporal->mean_interval_ms = new_mean;

        float diff = interval_ms - temporal->mean_interval_ms;
        float new_var = (1.0f - alpha) * temporal->variance_interval_ms + alpha * diff * diff;
        if (isfinite(new_var)) temporal->variance_interval_ms = new_var;

        bridge->async_effects.avg_interval_ms = temporal->mean_interval_ms;
    }

    temporal->messages_in_window++;

    float alpha_pred = 0.05f;
    float new_pred_interval = (1.0f - alpha_pred) * temporal->predicted_interval +
        alpha_pred * temporal->mean_interval_ms;
    if (isfinite(new_pred_interval)) temporal->predicted_interval = new_pred_interval;
}
