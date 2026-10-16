/**
 * @file nimcp_hypo_logging_fep_bridge.c
 * @brief Implementation of Hypothalamus Logging FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hypothalamic system logging
 * WHY:  Event frequency anomalies represent system stress deviations
 * HOW:  Map event frequency to free energy, anomaly detection to prediction error
 */

#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_logging_fep_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hypo_logging_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static float compute_fe_from_events(const hypo_logging_fep_bridge_t* bridge);

static hypo_logging_fep_level_t classify_stress_level(
    float free_energy,
    const hypo_logging_fep_config_t* config);

static hypo_logging_fep_response_t determine_response(
    hypo_logging_fep_level_t level,
    float urgency);

static hypo_logging_anomaly_type_t identify_anomaly(
    const hypo_logging_fep_bridge_t* bridge,
    float* severity_out);

static void update_running_averages(hypo_logging_fep_bridge_t* bridge,
                                    float free_energy,
                                    float surprise,
                                    float pred_error);

static void update_rate_tracking(hypo_logging_fep_bridge_t* bridge,
                                 uint64_t timestamp_ms);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * WHAT: Get default configuration for hypothalamus logging FEP bridge
 * WHY:  Provide sensible starting point for event monitoring
 * HOW:  Set balanced defaults for severity weights and thresholds
 */
int hypo_logging_fep_default_config(hypo_logging_fep_config_t* config) {
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

    /* Event rate parameters */
    config->rate_window_ms = HYPO_LOGGING_FEP_RATE_WINDOW_MS;
    config->expected_event_rate = 100.0f;  /* 100 events/sec baseline */
    config->rate_deviation_threshold = 3.0f;

    /* Severity weights - higher severity = higher FE contribution */
    config->debug_weight = 0.1f;
    config->info_weight = 0.2f;
    config->warning_weight = 1.0f;
    config->error_weight = 3.0f;
    config->critical_weight = 5.0f;

    /* Detection parameters */
    config->free_energy_threshold = HYPO_LOGGING_FEP_ANOMALY_THRESHOLD;
    config->surprise_threshold = 8.0f;
    config->precision_learning_rate = 0.05f;
    config->error_rate_threshold = HYPO_LOGGING_FEP_ERROR_RATE_THRESHOLD;
    config->warning_rate_threshold = HYPO_LOGGING_FEP_WARNING_RATE_THRESHOLD;

    /* Learning */
    config->enable_online_learning = true;
    config->learning_rate = 0.01f;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Create hypothalamus logging FEP bridge
 * WHY:  Initialize FEP integration for event monitoring
 * HOW:  Allocate structure, initialize base, apply configuration
 */
hypo_logging_fep_bridge_t* hypo_logging_fep_create(
    const hypo_logging_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
) {
    /* Validate required parameters */
    if (!drive_system || !fep_system) {
        NIMCP_LOGGING_ERROR("Hypo Logging FEP bridge: NULL system pointers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_fep_create: required parameter is NULL (drive_system, fep_system)");
        return NULL;
    }

    /* Allocate bridge structure */
    hypo_logging_fep_bridge_t* bridge = (hypo_logging_fep_bridge_t*)nimcp_malloc(
        sizeof(hypo_logging_fep_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Hypo Logging FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_logging_fep_create: bridge is NULL");
        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(hypo_logging_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hypo_logging_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->drive_system = drive_system;
    bridge->fep_system = fep_system;

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "hypo_logging_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Hypo Logging FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = HYPO_LOGGING_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_level = HYPO_LOGGING_FEP_LEVEL_NORMAL;

    /* Initialize rate tracking */
    bridge->state.rate_tracking.window_start_ms = nimcp_platform_time_monotonic_ms();
    bridge->state.rate_tracking.events_in_window = 0;
    bridge->state.rate_tracking.predicted_rate = bridge->config.expected_event_rate;
    bridge->state.rate_tracking.mean_rate = bridge->config.expected_event_rate;

    /* Initialize FEP effects */
    bridge->fep_effects.stress_level = HYPO_LOGGING_FEP_LEVEL_NORMAL;
    bridge->fep_effects.precision = HYPO_LOGGING_FEP_DEFAULT_PRECISION;
    bridge->fep_effects.system_health = 1.0f;
    bridge->fep_effects.detected_anomaly = HYPO_LOGGING_ANOMALY_NONE;

    /* Bio-async not yet connected */
    bridge->base.bio_async_enabled = false;
    bridge->base.module_id = BIO_MODULE_HYPO_LOGGING_FEP;
    bridge->base.module_name = "hypo_logging_fep_bridge";

    NIMCP_LOGGING_INFO("Hypo Logging FEP bridge created");
    return bridge;
}

/**
 * WHAT: Destroy hypothalamus logging FEP bridge
 * WHY:  Clean up all resources
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void hypo_logging_fep_destroy(hypo_logging_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        hypo_logging_fep_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Hypo Logging FEP bridge destroyed");
}

/**
 * WHAT: Reset bridge to initial state
 * WHY:  Allow reuse without full recreation
 * HOW:  Clear state and statistics, preserve connections
 */
int hypo_logging_fep_reset(hypo_logging_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->state.update_count = 0;
    bridge->state.detection_count = 0;
    bridge->state.current_precision = HYPO_LOGGING_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_level = HYPO_LOGGING_FEP_LEVEL_NORMAL;
    bridge->state.last_detection_time_ms = 0;

    /* Reset rate tracking */
    memset(&bridge->state.rate_tracking, 0, sizeof(hypo_logging_rate_tracking_t));
    bridge->state.rate_tracking.window_start_ms = nimcp_platform_time_monotonic_ms();
    bridge->state.rate_tracking.predicted_rate = bridge->config.expected_event_rate;
    bridge->state.rate_tracking.mean_rate = bridge->config.expected_event_rate;

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(hypo_logging_fep_effects_t));
    bridge->fep_effects.system_health = 1.0f;
    bridge->fep_effects.precision = HYPO_LOGGING_FEP_DEFAULT_PRECISION;

    memset(&bridge->log_effects, 0, sizeof(logging_to_fep_effects_t));

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(hypo_logging_fep_stats_t));
    bridge->stats.current_precision = HYPO_LOGGING_FEP_DEFAULT_PRECISION;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Hypo Logging FEP bridge reset");
    return 0;
}

/**
 * WHAT: Update bridge state
 * WHY:  Main update loop for bridge synchronization
 * HOW:  Compute effects, apply precision modulation, update state
 */
int hypo_logging_fep_update(hypo_logging_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update rate tracking */
    uint64_t now_ms = nimcp_platform_time_monotonic_ms();
    update_rate_tracking(bridge, now_ms);

    /* Compute free energy from events */
    float current_fe = compute_fe_from_events(bridge);

    /* Get FEP system metrics */
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    /* Update running averages */
    update_running_averages(bridge, current_fe, surprise, pred_error);

    /* Store in FEP effects */
    bridge->fep_effects.free_energy = current_fe;
    bridge->fep_effects.prediction_error = pred_error;
    bridge->fep_effects.precision = bridge->state.current_precision;

    /* Classify stress level */
    bridge->fep_effects.stress_level = classify_stress_level(current_fe, &bridge->config);

    /* Identify anomaly */
    float anomaly_severity = 0.0f;
    bridge->fep_effects.detected_anomaly = identify_anomaly(bridge, &anomaly_severity);
    bridge->fep_effects.anomaly_score = anomaly_severity;

    /* Compute system health estimate */
    float health = 1.0f - (current_fe / HYPO_LOGGING_FEP_CRITICAL_THRESHOLD);
    if (health < 0.0f) health = 0.0f;
    if (health > 1.0f) health = 1.0f;
    bridge->fep_effects.system_health = health;

    /* Compute event metrics */
    bridge->fep_effects.event_rate = bridge->log_effects.current_event_rate;
    bridge->fep_effects.error_rate = bridge->log_effects.current_error_rate;
    bridge->fep_effects.warning_rate = bridge->log_effects.current_warning_rate;

    /* Determine response */
    float urgency = current_fe / HYPO_LOGGING_FEP_CRITICAL_THRESHOLD;
    if (urgency > 1.0f) urgency = 1.0f;
    bridge->fep_effects.response_urgency = urgency;
    bridge->fep_effects.recommended_response = determine_response(
        bridge->fep_effects.stress_level, urgency
    );

    /* Update log effects state */
    bridge->log_effects.system_stressed =
        (bridge->fep_effects.stress_level >= HYPO_LOGGING_FEP_LEVEL_STRESSED);
    bridge->log_effects.anomaly_active =
        (bridge->fep_effects.detected_anomaly != HYPO_LOGGING_ANOMALY_NONE);

    /* Update statistics */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    bridge->stats.current_precision = bridge->state.current_precision;

    if (current_fe > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = current_fe;
    }
    if (bridge->fep_effects.event_rate > bridge->stats.max_event_rate) {
        bridge->stats.max_event_rate = bridge->fep_effects.event_rate;
    }
    if (bridge->fep_effects.error_rate > bridge->stats.max_error_rate) {
        bridge->stats.max_error_rate = bridge->fep_effects.error_rate;
    }

    /* Track anomalies */
    if (bridge->fep_effects.detected_anomaly != HYPO_LOGGING_ANOMALY_NONE) {
        bridge->stats.anomalies_detected++;
        switch (bridge->fep_effects.detected_anomaly) {
            case HYPO_LOGGING_ANOMALY_RATE:
                bridge->stats.rate_anomalies++;
                bridge->log_effects.rate_anomalies++;
                break;
            case HYPO_LOGGING_ANOMALY_ERROR_SPIKE:
                bridge->stats.error_spikes++;
                bridge->log_effects.error_spikes++;
                break;
            case HYPO_LOGGING_ANOMALY_WARNING_SPIKE:
                bridge->stats.warning_spikes++;
                bridge->log_effects.warning_spikes++;
                break;
            case HYPO_LOGGING_ANOMALY_PATTERN:
                bridge->stats.pattern_anomalies++;
                bridge->log_effects.pattern_anomalies++;
                break;
            default:
                break;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Core Operations Implementation
 * ============================================================================ */

/**
 * WHAT: Compute free energy from drive state
 * WHY:  Core FEP computation for logging monitoring
 * HOW:  Map drive deviations and event patterns to free energy
 */
int hypo_logging_fep_compute_fe(
    hypo_logging_fep_bridge_t* bridge,
    const hypo_drive_system_t* drives
) {
    if (!bridge || !drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_fep_compute_fe: bridge or drives is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute FE from drive deviations */
    float drive_fe = 0.0f;
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        float deviation = fabsf(drives->drives[i].deviation);
        float urgency = drives->drives[i].urgency;
        drive_fe += deviation * urgency * bridge->config.drive_fe_weight;
    }

    /* Compute FE from events */
    float event_fe = compute_fe_from_events(bridge);

    /* Total free energy */
    float total_fe = drive_fe + event_fe;

    /* Update effects */
    bridge->fep_effects.free_energy = total_fe;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Modulate precision based on fatigue
 * WHY:  Precision represents confidence; fatigue reduces confidence
 * HOW:  Scale precision inversely with fatigue
 */
int hypo_logging_fep_modulate_precision(
    hypo_logging_fep_bridge_t* bridge,
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

    float new_precision = HYPO_LOGGING_FEP_DEFAULT_PRECISION * precision_mod;

    /* Smooth adaptation */
    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * new_precision;

    /* Clamp */
    if (bridge->state.current_precision < HYPO_LOGGING_FEP_MIN_PRECISION) {
        bridge->state.current_precision = HYPO_LOGGING_FEP_MIN_PRECISION;
    }
    if (bridge->state.current_precision > HYPO_LOGGING_FEP_MAX_PRECISION) {
        bridge->state.current_precision = HYPO_LOGGING_FEP_MAX_PRECISION;
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
int hypo_logging_fep_get_effects(
    const hypo_logging_fep_bridge_t* bridge,
    hypo_logging_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_fep_get_effects: bridge or effects is NULL");
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
int hypo_logging_fep_get_stats(
    const hypo_logging_fep_bridge_t* bridge,
    hypo_logging_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_fep_get_stats: bridge or stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Event Processing Implementation
 * ============================================================================ */

/**
 * WHAT: Log an event for FEP processing
 * WHY:  Events update FEP beliefs about system state
 * HOW:  Track event counts and rates by severity
 */
int hypo_logging_fep_log_event(
    hypo_logging_fep_bridge_t* bridge,
    hypo_log_severity_t severity,
    uint64_t timestamp_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (severity >= HYPO_LOG_SEVERITY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_logging_fep_log_event: invalid severity");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update counts */
    switch (severity) {
        case HYPO_LOG_SEVERITY_DEBUG:
            bridge->log_effects.debug_count++;
            break;
        case HYPO_LOG_SEVERITY_INFO:
            bridge->log_effects.info_count++;
            break;
        case HYPO_LOG_SEVERITY_WARNING:
            bridge->log_effects.warning_count++;
            break;
        case HYPO_LOG_SEVERITY_ERROR:
            bridge->log_effects.error_count++;
            break;
        case HYPO_LOG_SEVERITY_CRITICAL:
            bridge->log_effects.critical_count++;
            break;
        default:
            break;
    }
    bridge->log_effects.total_events++;

    /* Update rate tracking */
    bridge->state.rate_tracking.events_in_window++;
    bridge->state.rate_tracking.by_severity[severity]++;

    /* Check window rollover */
    if (timestamp_ms - bridge->state.rate_tracking.window_start_ms >=
        bridge->config.rate_window_ms) {
        update_rate_tracking(bridge, timestamp_ms);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Detect anomaly in current event pattern
 * WHY:  Anomaly detection is core FEP function
 * HOW:  Analyze current metrics against thresholds
 */
int hypo_logging_fep_detect_anomaly(
    hypo_logging_fep_bridge_t* bridge,
    hypo_logging_anomaly_type_t* anomaly_out,
    float* severity_out
) {
    if (!bridge || !anomaly_out || !severity_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_logging_fep_detect_anomaly: NULL parameter");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    *anomaly_out = identify_anomaly(bridge, severity_out);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int hypo_logging_fep_connect_bio_async(
    hypo_logging_fep_bridge_t* bridge,
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
        .module_id = BIO_MODULE_HYPO_LOGGING_FEP,
        .module_name = "hypo_logging_fep_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hypo Logging FEP bridge connected to bio-async");
    }

    return 0;
}

int hypo_logging_fep_disconnect_bio_async(hypo_logging_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Hypo Logging FEP bridge disconnected from bio-async");
    return 0;
}

int hypo_logging_fep_process_messages(
    hypo_logging_fep_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;
    }

    return (int)bio_router_process_inbox(bridge->base.bio_ctx, max_messages);
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

const char* hypo_logging_fep_level_name(hypo_logging_fep_level_t level) {
    switch (level) {
        case HYPO_LOGGING_FEP_LEVEL_NORMAL:
            return "Normal";
        case HYPO_LOGGING_FEP_LEVEL_ELEVATED:
            return "Elevated";
        case HYPO_LOGGING_FEP_LEVEL_STRESSED:
            return "Stressed";
        case HYPO_LOGGING_FEP_LEVEL_ANOMALOUS:
            return "Anomalous";
        case HYPO_LOGGING_FEP_LEVEL_CRITICAL:
            return "Critical";
        default:
            return "Unknown";
    }
}

const char* hypo_logging_fep_response_name(hypo_logging_fep_response_t response) {
    switch (response) {
        case HYPO_LOGGING_FEP_RESPONSE_NONE:
            return "None";
        case HYPO_LOGGING_FEP_RESPONSE_MONITOR:
            return "Monitor";
        case HYPO_LOGGING_FEP_RESPONSE_THROTTLE:
            return "Throttle";
        case HYPO_LOGGING_FEP_RESPONSE_ALERT:
            return "Alert";
        case HYPO_LOGGING_FEP_RESPONSE_EMERGENCY:
            return "Emergency";
        default:
            return "Unknown";
    }
}

const char* hypo_logging_fep_anomaly_name(hypo_logging_anomaly_type_t anomaly) {
    switch (anomaly) {
        case HYPO_LOGGING_ANOMALY_NONE:
            return "None";
        case HYPO_LOGGING_ANOMALY_RATE:
            return "Rate";
        case HYPO_LOGGING_ANOMALY_ERROR_SPIKE:
            return "ErrorSpike";
        case HYPO_LOGGING_ANOMALY_WARNING_SPIKE:
            return "WarningSpike";
        case HYPO_LOGGING_ANOMALY_PATTERN:
            return "Pattern";
        default:
            return "Unknown";
    }
}

const char* hypo_logging_fep_severity_name(hypo_log_severity_t severity) {
    switch (severity) {
        case HYPO_LOG_SEVERITY_DEBUG:
            return "Debug";
        case HYPO_LOG_SEVERITY_INFO:
            return "Info";
        case HYPO_LOG_SEVERITY_WARNING:
            return "Warning";
        case HYPO_LOG_SEVERITY_ERROR:
            return "Error";
        case HYPO_LOG_SEVERITY_CRITICAL:
            return "Critical";
        default:
            return "Unknown";
    }
}

void hypo_logging_fep_print_summary(const hypo_logging_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Hypo Logging FEP Bridge: NULL\n");
        return;
    }

    printf("=== Hypothalamus Logging FEP Bridge Summary ===\n");
    printf("State:\n");
    printf("  Active: %s\n", bridge->state.active ? "yes" : "no");
    printf("  Updates: %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Precision: %.3f\n", bridge->state.current_precision);
    printf("\n");
    printf("Event Counts:\n");
    printf("  Debug: %lu\n", (unsigned long)bridge->log_effects.debug_count);
    printf("  Info: %lu\n", (unsigned long)bridge->log_effects.info_count);
    printf("  Warning: %lu\n", (unsigned long)bridge->log_effects.warning_count);
    printf("  Error: %lu\n", (unsigned long)bridge->log_effects.error_count);
    printf("  Critical: %lu\n", (unsigned long)bridge->log_effects.critical_count);
    printf("  Total: %lu\n", (unsigned long)bridge->log_effects.total_events);
    printf("\n");
    printf("FEP Effects:\n");
    printf("  Free Energy: %.3f\n", bridge->fep_effects.free_energy);
    printf("  Stress Level: %s\n",
           hypo_logging_fep_level_name(bridge->fep_effects.stress_level));
    printf("  Event Rate: %.2f/sec\n", bridge->fep_effects.event_rate);
    printf("  Error Rate: %.3f\n", bridge->fep_effects.error_rate);
    printf("  Anomaly: %s\n",
           hypo_logging_fep_anomaly_name(bridge->fep_effects.detected_anomaly));
    printf("  System Health: %.3f\n", bridge->fep_effects.system_health);
    printf("  Recommended Response: %s\n",
           hypo_logging_fep_response_name(bridge->fep_effects.recommended_response));
    printf("================================================\n");
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

/**
 * WHAT: Compute free energy from event patterns
 * WHY:  Map event domain to FEP domain
 * HOW:  Weighted combination of event deviations by severity
 */
static float compute_fe_from_events(const hypo_logging_fep_bridge_t* bridge) {
    float fe = 0.0f;

    /* Rate deviation contribution */
    float rate_dev = 0.0f;
    if (bridge->state.rate_tracking.predicted_rate > 0.01f) {
        rate_dev = fabsf(bridge->log_effects.current_event_rate -
                         bridge->state.rate_tracking.predicted_rate) /
                   bridge->state.rate_tracking.predicted_rate;
    }
    fe += rate_dev * bridge->config.prediction_error_gain;

    /* Error rate contribution */
    if (bridge->log_effects.current_error_rate > bridge->config.error_rate_threshold) {
        float error_excess = bridge->log_effects.current_error_rate -
                             bridge->config.error_rate_threshold;
        fe += error_excess * bridge->config.error_weight * 10.0f;
    }

    /* Warning rate contribution */
    if (bridge->log_effects.current_warning_rate > bridge->config.warning_rate_threshold) {
        float warning_excess = bridge->log_effects.current_warning_rate -
                               bridge->config.warning_rate_threshold;
        fe += warning_excess * bridge->config.warning_weight * 5.0f;
    }

    return fe;
}

/**
 * WHAT: Classify stress level from free energy
 * WHY:  Map continuous FE to discrete categories
 * HOW:  Threshold-based classification
 */
static hypo_logging_fep_level_t classify_stress_level(
    float free_energy,
    const hypo_logging_fep_config_t* config
) {
    if (free_energy >= HYPO_LOGGING_FEP_CRITICAL_THRESHOLD) {
        return HYPO_LOGGING_FEP_LEVEL_CRITICAL;
    } else if (free_energy >= config->free_energy_threshold) {
        return HYPO_LOGGING_FEP_LEVEL_ANOMALOUS;
    } else if (free_energy >= HYPO_LOGGING_FEP_ELEVATED_THRESHOLD) {
        return HYPO_LOGGING_FEP_LEVEL_STRESSED;
    } else if (free_energy >= HYPO_LOGGING_FEP_NORMAL_THRESHOLD) {
        return HYPO_LOGGING_FEP_LEVEL_ELEVATED;
    } else {
        return HYPO_LOGGING_FEP_LEVEL_NORMAL;
    }
}

/**
 * WHAT: Determine appropriate response
 * WHY:  Active inference selects actions to minimize expected FE
 * HOW:  Map stress level and urgency to response type
 */
static hypo_logging_fep_response_t determine_response(
    hypo_logging_fep_level_t level,
    float urgency
) {
    switch (level) {
        case HYPO_LOGGING_FEP_LEVEL_CRITICAL:
            return HYPO_LOGGING_FEP_RESPONSE_EMERGENCY;

        case HYPO_LOGGING_FEP_LEVEL_ANOMALOUS:
            if (urgency > 0.8f) {
                return HYPO_LOGGING_FEP_RESPONSE_EMERGENCY;
            }
            return HYPO_LOGGING_FEP_RESPONSE_ALERT;

        case HYPO_LOGGING_FEP_LEVEL_STRESSED:
            return HYPO_LOGGING_FEP_RESPONSE_THROTTLE;

        case HYPO_LOGGING_FEP_LEVEL_ELEVATED:
            return HYPO_LOGGING_FEP_RESPONSE_MONITOR;

        case HYPO_LOGGING_FEP_LEVEL_NORMAL:
        default:
            return HYPO_LOGGING_FEP_RESPONSE_NONE;
    }
}

/**
 * WHAT: Identify anomaly type
 * WHY:  Classify the nature of the logging anomaly
 * HOW:  Analyze event patterns and rates
 */
static hypo_logging_anomaly_type_t identify_anomaly(
    const hypo_logging_fep_bridge_t* bridge,
    float* severity_out
) {
    hypo_logging_anomaly_type_t anomaly = HYPO_LOGGING_ANOMALY_NONE;
    float severity = 0.0f;

    /* Check for error spike */
    if (bridge->log_effects.current_error_rate > bridge->config.error_rate_threshold) {
        anomaly = HYPO_LOGGING_ANOMALY_ERROR_SPIKE;
        severity = bridge->log_effects.current_error_rate /
                   (bridge->config.error_rate_threshold * 2.0f);
        if (severity > 1.0f) severity = 1.0f;
    }

    /* Check for warning spike (only if no error spike) */
    if (anomaly == HYPO_LOGGING_ANOMALY_NONE &&
        bridge->log_effects.current_warning_rate > bridge->config.warning_rate_threshold) {
        anomaly = HYPO_LOGGING_ANOMALY_WARNING_SPIKE;
        severity = bridge->log_effects.current_warning_rate /
                   (bridge->config.warning_rate_threshold * 2.0f);
        if (severity > 1.0f) severity = 1.0f;
    }

    /* Check for rate anomaly */
    if (anomaly == HYPO_LOGGING_ANOMALY_NONE) {
        float rate_dev = 0.0f;
        if (bridge->state.rate_tracking.predicted_rate > 0.01f) {
            rate_dev = fabsf(bridge->log_effects.current_event_rate -
                             bridge->state.rate_tracking.predicted_rate) /
                       bridge->state.rate_tracking.predicted_rate;
        }
        if (rate_dev > bridge->config.rate_deviation_threshold) {
            anomaly = HYPO_LOGGING_ANOMALY_RATE;
            severity = rate_dev / (bridge->config.rate_deviation_threshold * 2.0f);
            if (severity > 1.0f) severity = 1.0f;
        }
    }

    *severity_out = severity;
    return anomaly;
}

/**
 * WHAT: Update running averages for metrics
 * WHY:  Smooth metrics over time for stability
 * HOW:  Exponential moving average
 */
static void update_running_averages(
    hypo_logging_fep_bridge_t* bridge,
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
 * WHAT: Update rate tracking
 * WHY:  Track event rates for FEP predictions
 * HOW:  Calculate rates, update predictions
 */
static void update_rate_tracking(
    hypo_logging_fep_bridge_t* bridge,
    uint64_t timestamp_ms
) {
    hypo_logging_rate_tracking_t* tracking = &bridge->state.rate_tracking;

    uint64_t elapsed = timestamp_ms - tracking->window_start_ms;
    if (elapsed < bridge->config.rate_window_ms) {
        return;  /* Window not complete */
    }

    /* Calculate rates */
    float rate = (float)tracking->events_in_window *
                 (1000.0f / (float)elapsed);
    bridge->log_effects.current_event_rate = rate;

    /* Store in history */
    tracking->rate_history[tracking->rate_history_idx] = rate;
    tracking->rate_history_idx = (tracking->rate_history_idx + 1) % 16;

    /* Calculate error and warning rates */
    uint32_t total = tracking->events_in_window;
    if (total > 0) {
        bridge->log_effects.current_error_rate =
            (float)(tracking->by_severity[HYPO_LOG_SEVERITY_ERROR] +
                    tracking->by_severity[HYPO_LOG_SEVERITY_CRITICAL]) / (float)total;
        bridge->log_effects.current_warning_rate =
            (float)tracking->by_severity[HYPO_LOG_SEVERITY_WARNING] / (float)total;
    } else {
        bridge->log_effects.current_error_rate = 0.0f;
        bridge->log_effects.current_warning_rate = 0.0f;
    }

    /* Update predictions */
    float alpha = 0.1f;
    float new_predicted = (1.0f - alpha) * tracking->predicted_rate + alpha * rate;
    if (isfinite(new_predicted)) tracking->predicted_rate = new_predicted;
    float new_mean = (1.0f - alpha) * tracking->mean_rate + alpha * rate;
    if (isfinite(new_mean)) tracking->mean_rate = new_mean;

    /* Reset window */
    tracking->window_start_ms = timestamp_ms;
    tracking->events_in_window = 0;
    memset(tracking->by_severity, 0, sizeof(tracking->by_severity));
}
