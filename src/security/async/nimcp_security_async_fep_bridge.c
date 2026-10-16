/**
 * @file nimcp_security_async_fep_bridge.c
 * @brief Implementation of Security Async FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for asynchronous security operations
 * WHY:  Async anomalies (timing attacks, message floods) are high-surprise events
 * HOW:  Map async security metrics to free energy, use prediction errors for detection
 */

#include "security/async/nimcp_security_async_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_async_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static float compute_free_energy_from_async(float msg_rate,
                                             float rate_deviation,
                                             float queue_util,
                                             const sec_async_fep_config_t* config);

static float compute_surprise_from_timing(float interval_deviation,
                                           float expected_interval);

static sec_async_fep_threat_level_t classify_threat_level(float free_energy,
                                                           const sec_async_fep_config_t* config);

static sec_async_fep_response_t determine_response(sec_async_fep_threat_level_t threat,
                                                    float urgency);

static void update_running_averages(sec_async_fep_bridge_t* bridge,
                                    float free_energy,
                                    float surprise,
                                    float pred_error);

static void update_temporal_pattern(sec_async_fep_bridge_t* bridge,
                                    uint64_t timestamp_ms);

static float compute_rate_deviation(const sec_async_fep_bridge_t* bridge);

static sec_async_anomaly_type_t identify_anomaly_type(
    const sec_async_fep_bridge_t* bridge,
    float free_energy,
    float surprise);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * WHAT: Get default configuration for security async FEP bridge
 * WHY:  Provide sensible starting point for most deployments
 * HOW:  Set biologically-plausible defaults for all parameters
 */
int sec_async_fep_default_config(sec_async_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_default_config: config is NULL");
        return -1;
    }

    /* FEP parameters */
    config->free_energy_threshold = SEC_ASYNC_FEP_ATTACK_THRESHOLD;
    config->surprise_threshold = SEC_ASYNC_FEP_SURPRISE_ANOMALY;
    config->precision_learning_rate = 0.05f;

    /* Detection parameters */
    config->use_fep_detection = true;
    config->enable_precision_modulation = true;
    config->normal_fe_threshold = SEC_ASYNC_FEP_NORMAL_THRESHOLD;
    config->critical_fe_threshold = SEC_ASYNC_FEP_CRITICAL_THRESHOLD;

    /* Async metrics mapping */
    config->rate_to_fe_scale = 5.0f;
    config->timing_pe_weight = 0.4f;
    config->queue_surprise_weight = 0.3f;
    config->routing_anomaly_weight = 0.3f;

    /* Timing analysis */
    config->rate_window_ms = SEC_ASYNC_FEP_RATE_WINDOW_MS;
    config->expected_msg_rate = 100.0f;  /* 100 messages/sec baseline */
    config->rate_deviation_threshold = 3.0f;  /* 3x deviation triggers alarm */

    /* Active inference settings */
    config->enable_active_inference = true;
    config->response_threshold = SEC_ASYNC_FEP_SUSPICIOUS_THRESHOLD;
    config->action_temperature = 1.0f;

    /* Learning */
    config->enable_online_learning = true;
    config->learning_rate = 0.01f;
    config->learn_from_false_positives = true;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Create security async FEP bridge
 * WHY:  Initialize FEP integration for async security detection
 * HOW:  Allocate structure, initialize base, apply configuration
 */
sec_async_fep_bridge_t* sec_async_fep_create(
    const sec_async_fep_config_t* config,
    security_async_bridge_t* security_async,
    fep_system_t* fep_system
) {
    /* Validate required parameters */
    if (!security_async || !fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_create: security_async or fep_system is NULL");
        NIMCP_LOGGING_ERROR("Security Async FEP bridge: NULL system pointers");
        return NULL;
    }

    /* Allocate bridge structure */
    sec_async_fep_bridge_t* bridge = (sec_async_fep_bridge_t*)nimcp_malloc(
        sizeof(sec_async_fep_bridge_t)
    );
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_async_fep_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Security Async FEP bridge: allocation failed");
        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(sec_async_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        sec_async_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->security_async = security_async;
    bridge->fep_system = fep_system;

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "security_async_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "sec_async_fep_create: mutex creation failed");
        NIMCP_LOGGING_ERROR("Security Async FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = SEC_ASYNC_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_threat = SEC_ASYNC_FEP_THREAT_NONE;

    /* Initialize temporal pattern tracking */
    bridge->state.temporal.window_start_ms = nimcp_platform_time_monotonic_ms();
    bridge->state.temporal.messages_in_window = 0;
    bridge->state.temporal.mean_interval_ms = 10.0f;  /* Initial estimate */
    bridge->state.temporal.predicted_rate = bridge->config.expected_msg_rate;
    bridge->state.temporal.predicted_interval = 1000.0f / bridge->config.expected_msg_rate;

    /* Initialize effects */
    bridge->fep_effects.threat_level = SEC_ASYNC_FEP_THREAT_NONE;
    bridge->fep_effects.detection_sensitivity = SEC_ASYNC_FEP_DEFAULT_PRECISION;
    bridge->fep_effects.async_health_estimate = 1.0f;
    bridge->fep_effects.detected_anomaly = SEC_ASYNC_ANOMALY_NONE;

    /* Bio-async not yet connected */
    bridge->base.bio_async_enabled = false;
    bridge->base.module_id = BIO_MODULE_SECURITY_ASYNC_FEP;
    bridge->base.module_name = "sec_async_fep_bridge";

    NIMCP_LOGGING_INFO("Security Async FEP bridge created");
    return bridge;
}

/**
 * WHAT: Destroy security async FEP bridge
 * WHY:  Clean up all resources to prevent memory leaks
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void sec_async_fep_destroy(sec_async_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        sec_async_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    /* Free bridge memory */
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Security Async FEP bridge destroyed");
}

/**
 * WHAT: Reset bridge to initial state
 * WHY:  Allow reuse without full recreation
 * HOW:  Clear state and statistics, preserve connections and config
 */
int sec_async_fep_reset(sec_async_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_reset: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->state.update_count = 0;
    bridge->state.detection_count = 0;
    bridge->state.current_precision = SEC_ASYNC_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_threat = SEC_ASYNC_FEP_THREAT_NONE;
    bridge->state.last_threat_time_ms = 0;

    /* Reset temporal pattern */
    bridge->state.temporal.window_start_ms = nimcp_platform_time_monotonic_ms();
    bridge->state.temporal.messages_in_window = 0;
    memset(bridge->state.temporal.rate_history, 0, sizeof(bridge->state.temporal.rate_history));
    bridge->state.temporal.rate_history_idx = 0;

    /* Reset queue observation */
    memset(&bridge->state.queue_obs, 0, sizeof(async_queue_observation_t));

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(fep_to_async_effects_t));
    bridge->fep_effects.async_health_estimate = 1.0f;
    bridge->fep_effects.detection_sensitivity = SEC_ASYNC_FEP_DEFAULT_PRECISION;

    memset(&bridge->async_effects, 0, sizeof(async_to_fep_effects_t));

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(sec_async_fep_stats_t));
    bridge->stats.current_precision = SEC_ASYNC_FEP_DEFAULT_PRECISION;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Security Async FEP bridge reset");
    return 0;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

/**
 * WHAT: Get current bridge configuration
 * WHY:  Allow inspection of current settings
 * HOW:  Copy configuration to output structure
 */
int sec_async_fep_get_config(
    const sec_async_fep_bridge_t* bridge,
    sec_async_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    *config = bridge->config;
    return 0;
}

/**
 * WHAT: Set bridge configuration
 * WHY:  Allow runtime tuning of detection parameters
 * HOW:  Validate and copy new configuration
 */
int sec_async_fep_set_config(
    sec_async_fep_bridge_t* bridge,
    const sec_async_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Validate critical parameters */
    if (config->free_energy_threshold <= 0.0f ||
        config->surprise_threshold <= 0.0f) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sec_async_fep_set_config: operation failed");
        return -1;
    }

    bridge->config = *config;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

/**
 * WHAT: Compute FEP effects on async security
 * WHY:  Derive threat detection metrics from FEP state
 * HOW:  Query FEP system for free energy, surprise, prediction error
 */
int sec_async_fep_compute_effects(sec_async_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_compute_effects: bridge is NULL");
        return -1;
    }

    if (!bridge->state.active || !bridge->fep_system) {
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
    bridge->fep_effects.surprise_level = surprise;
    bridge->fep_effects.prediction_error = pred_error;

    /* Classify threat level based on free energy */
    bridge->fep_effects.threat_level = classify_threat_level(current_fe, &bridge->config);

    /* Identify anomaly type */
    bridge->fep_effects.detected_anomaly = identify_anomaly_type(bridge, current_fe, surprise);

    /* Compute threat confidence based on precision and stability */
    float confidence = 1.0f - (pred_error / 10.0f);
    if (confidence < 0.0f) confidence = 0.0f;
    if (confidence > 1.0f) confidence = 1.0f;
    bridge->fep_effects.threat_confidence = confidence * bridge->state.current_precision;

    /* Compute detection sensitivity from precision */
    bridge->fep_effects.detection_sensitivity = bridge->state.current_precision;

    /* Estimate async health from FEP (inverted relationship) */
    float health_estimate = 1.0f - (current_fe / bridge->config.critical_fe_threshold);
    if (health_estimate < 0.0f) health_estimate = 0.0f;
    if (health_estimate > 1.0f) health_estimate = 1.0f;
    bridge->fep_effects.async_health_estimate = health_estimate;

    /* Determine recommended response */
    float urgency = current_fe / bridge->config.critical_fe_threshold;
    if (urgency > 1.0f) urgency = 1.0f;
    bridge->fep_effects.response_urgency = urgency;
    bridge->fep_effects.recommended_response = determine_response(
        bridge->fep_effects.threat_level, urgency
    );

    /* Update statistics */
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    bridge->stats.avg_prediction_error = bridge->state.avg_prediction_error;

    if (current_fe > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = current_fe;
    }
    if (surprise > bridge->stats.max_surprise) {
        bridge->stats.max_surprise = surprise;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Process async message for FEP analysis
 * WHY:  Each message updates temporal predictions
 * HOW:  Update timing patterns, compute prediction errors
 */
int sec_async_fep_process_message(
    sec_async_fep_bridge_t* bridge,
    uint32_t source_module,
    uint32_t msg_type,
    uint64_t timestamp_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_process_message: bridge is NULL");
        return -1;
    }

    /* Suppress unused parameter warnings */
    (void)source_module;
    (void)msg_type;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update temporal pattern */
    update_temporal_pattern(bridge, timestamp_ms);

    /* Update async effects */
    bridge->async_effects.normal_operations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Process queue state for FEP analysis
 * WHY:  Queue anomalies indicate potential attacks
 * HOW:  Compare observed vs predicted queue state
 */
int sec_async_fep_observe_queue(
    sec_async_fep_bridge_t* bridge,
    uint32_t queue_depth,
    float fill_rate,
    float drain_rate
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_observe_queue: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update queue observation */
    bridge->state.queue_obs.observed_depth = queue_depth;
    bridge->state.queue_obs.fill_rate = fill_rate;
    bridge->state.queue_obs.drain_rate = drain_rate;

    /* Compute prediction error for queue depth */
    float depth_error = fabsf((float)queue_depth - (float)bridge->state.queue_obs.predicted_depth);
    bridge->state.queue_obs.depth_prediction_error = depth_error;

    /* Update predicted depth using EMA */
    float alpha = 0.1f;
    bridge->state.queue_obs.predicted_depth = (uint32_t)(
        (1.0f - alpha) * (float)bridge->state.queue_obs.predicted_depth +
        alpha * (float)queue_depth
    );

    /* Detect queue anomaly */
    float max_expected_depth = 100.0f;  /* Configurable */
    float queue_util = (float)queue_depth / max_expected_depth;
    bridge->async_effects.queue_utilization = queue_util > 1.0f ? 1.0f : queue_util;

    /* Flag anomaly if depth is too high or prediction error is large */
    bridge->state.queue_obs.anomaly_detected =
        (queue_depth > (uint32_t)(max_expected_depth * 0.9f)) ||
        (depth_error > max_expected_depth * 0.5f);

    if (bridge->state.queue_obs.anomaly_detected) {
        bridge->stats.queue_anomalies++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Main update loop for bridge synchronization
 * WHY:  Maintain bidirectional effects between security and FEP
 * HOW:  Compute effects, apply precision modulation, update state
 */
int sec_async_fep_update(sec_async_fep_bridge_t* bridge, uint64_t delta_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_update: bridge is NULL");
        return -1;
    }

    /* Suppress unused parameter warning */
    (void)delta_ms;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute FEP effects on security */
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    int result = sec_async_fep_compute_effects(bridge);
    if (result != 0) {
        return result;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Apply precision modulation if enabled */
    if (bridge->config.enable_precision_modulation) {
        /*
         * Adapt precision based on detection performance
         * High threat rate -> increase precision (more sensitive)
         * Low threat rate -> decrease precision (less false positives)
         */
        float threat_rate = (float)bridge->async_effects.floods_detected /
                           (float)(bridge->state.detection_count + 1);

        float target_precision = SEC_ASYNC_FEP_DEFAULT_PRECISION;
        if (threat_rate > 0.2f) {
            target_precision = SEC_ASYNC_FEP_MAX_PRECISION;
        } else if (threat_rate < 0.05f) {
            target_precision = SEC_ASYNC_FEP_MIN_PRECISION + 0.5f;
        }

        /* Smooth adaptation */
        float alpha = bridge->config.precision_learning_rate;
        bridge->state.current_precision =
            (1.0f - alpha) * bridge->state.current_precision +
            alpha * target_precision;

        /* Clamp precision */
        if (bridge->state.current_precision < SEC_ASYNC_FEP_MIN_PRECISION) {
            bridge->state.current_precision = SEC_ASYNC_FEP_MIN_PRECISION;
        }
        if (bridge->state.current_precision > SEC_ASYNC_FEP_MAX_PRECISION) {
            bridge->state.current_precision = SEC_ASYNC_FEP_MAX_PRECISION;
        }

        bridge->stats.precision_adaptations++;
    }

    /* Check for rate window rollover */
    uint64_t now_ms = nimcp_platform_time_monotonic_ms();
    if (now_ms - bridge->state.temporal.window_start_ms >= bridge->config.rate_window_ms) {
        /* Store rate in history */
        float rate = (float)bridge->state.temporal.messages_in_window *
                     (1000.0f / (float)bridge->config.rate_window_ms);
        bridge->state.temporal.rate_history[bridge->state.temporal.rate_history_idx] = rate;
        bridge->state.temporal.rate_history_idx =
            (bridge->state.temporal.rate_history_idx + 1) % 16;

        /* Update async effects */
        bridge->async_effects.current_msg_rate = rate;

        /* Track max rate */
        if (rate > bridge->stats.max_msg_rate) {
            bridge->stats.max_msg_rate = rate;
        }

        /* Reset window */
        bridge->state.temporal.window_start_ms = now_ms;
        bridge->state.temporal.messages_in_window = 0;

        /* Update FEP predictions */
        float alpha_rate = 0.1f;
        bridge->state.temporal.predicted_rate =
            (1.0f - alpha_rate) * bridge->state.temporal.predicted_rate +
            alpha_rate * rate;
    }

    /* Update statistics */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    bridge->stats.current_precision = bridge->state.current_precision;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Detection Implementation
 * ============================================================================ */

/**
 * WHAT: Detect threat using FEP analysis
 * WHY:  Combine async metrics with FEP for enhanced detection
 * HOW:  Compute free energy from async metrics, classify threat
 */
int sec_async_fep_detect_threat(
    sec_async_fep_bridge_t* bridge,
    float msg_rate,
    float avg_interval,
    float queue_utilization,
    sec_async_fep_threat_level_t* threat_level_out,
    float* confidence_out
) {
    if (!bridge || !threat_level_out || !confidence_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_detect_threat: required parameter is NULL (bridge, threat_level_out, confidence_out)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute rate deviation from expected */
    float rate_deviation = fabsf(msg_rate - bridge->config.expected_msg_rate) /
                          bridge->config.expected_msg_rate;

    /* Compute free energy from async metrics */
    float free_energy = compute_free_energy_from_async(
        msg_rate, rate_deviation, queue_utilization, &bridge->config
    );

    /* Compute surprise from timing */
    float expected_interval = bridge->state.temporal.predicted_interval;
    float interval_deviation = fabsf(avg_interval - expected_interval);
    float surprise = compute_surprise_from_timing(interval_deviation, expected_interval);

    /* Classify threat level */
    sec_async_fep_threat_level_t threat = classify_threat_level(
        free_energy, &bridge->config
    );

    /* Compute confidence */
    float confidence = bridge->state.current_precision;
    float pe = bridge->fep_effects.prediction_error;
    if (pe > SEC_ASYNC_FEP_PE_TOLERANCE) {
        confidence *= (1.0f - (pe - SEC_ASYNC_FEP_PE_TOLERANCE));
        if (confidence < 0.1f) confidence = 0.1f;
    }

    *threat_level_out = threat;
    *confidence_out = confidence;

    /* Update FEP effects */
    bridge->fep_effects.free_energy = free_energy;
    bridge->fep_effects.surprise_level = surprise;
    bridge->fep_effects.threat_level = threat;
    bridge->fep_effects.threat_confidence = confidence;

    /* Track statistics */
    bridge->stats.fep_detections++;
    bridge->state.detection_count++;

    if (threat >= SEC_ASYNC_FEP_THREAT_MODERATE) {
        bridge->stats.threats_detected++;
        bridge->stats.true_positive_count++;
        bridge->async_effects.under_attack = true;
        bridge->state.last_threat = threat;
        bridge->state.last_threat_time_ms = nimcp_platform_time_monotonic_ms();

        /* Classify by type */
        if (rate_deviation > bridge->config.rate_deviation_threshold) {
            bridge->stats.flood_detections++;
            bridge->async_effects.floods_detected++;
        }
    } else {
        bridge->async_effects.under_attack = false;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Identify the type of async anomaly
 * WHY:  Different anomalies require different responses
 * HOW:  Analyze temporal patterns and queue states
 */
int sec_async_fep_detect_anomaly(
    sec_async_fep_bridge_t* bridge,
    sec_async_anomaly_type_t* anomaly_out,
    float* severity_out
) {
    if (!bridge || !anomaly_out || !severity_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_detect_anomaly: required parameter is NULL (bridge, anomaly_out, severity_out)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    sec_async_anomaly_type_t anomaly = SEC_ASYNC_ANOMALY_NONE;
    float severity = 0.0f;

    /* Check for flood */
    float rate_dev = compute_rate_deviation(bridge);
    if (rate_dev > bridge->config.rate_deviation_threshold) {
        anomaly = SEC_ASYNC_ANOMALY_FLOOD;
        severity = rate_dev / (bridge->config.rate_deviation_threshold * 2.0f);
        if (severity > 1.0f) severity = 1.0f;
    }

    /* Check for timing anomaly */
    float timing_pe = bridge->state.avg_prediction_error;
    if (timing_pe > SEC_ASYNC_FEP_PE_ATTACK && anomaly == SEC_ASYNC_ANOMALY_NONE) {
        anomaly = SEC_ASYNC_ANOMALY_TIMING;
        severity = timing_pe;
        if (severity > 1.0f) severity = 1.0f;
    }

    /* Check for queue anomaly */
    if (bridge->state.queue_obs.anomaly_detected && anomaly == SEC_ASYNC_ANOMALY_NONE) {
        anomaly = SEC_ASYNC_ANOMALY_QUEUE;
        severity = bridge->async_effects.queue_utilization;
    }

    *anomaly_out = anomaly;
    *severity_out = severity;

    /* Update effects */
    bridge->fep_effects.detected_anomaly = anomaly;

    /* Track statistics by type */
    switch (anomaly) {
        case SEC_ASYNC_ANOMALY_TIMING:
            bridge->stats.timing_anomalies++;
            bridge->async_effects.timing_anomalies++;
            break;
        case SEC_ASYNC_ANOMALY_FLOOD:
            /* Already tracked in detect_threat */
            break;
        case SEC_ASYNC_ANOMALY_QUEUE:
            /* Already tracked in observe_queue */
            break;
        case SEC_ASYNC_ANOMALY_ROUTING:
            bridge->stats.routing_anomalies++;
            bridge->async_effects.routing_attacks++;
            break;
        default:
            break;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Get recommended protective response via active inference
 * WHY:  Actions minimize expected free energy
 * HOW:  Evaluate threat level and urgency to select optimal response
 */
int sec_async_fep_get_response(
    sec_async_fep_bridge_t* bridge,
    sec_async_fep_response_t* response_out,
    float* urgency_out
) {
    if (!bridge || !response_out || !urgency_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_get_response: required parameter is NULL (bridge, response_out, urgency_out)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current threat metrics */
    float free_energy = bridge->fep_effects.free_energy;
    sec_async_fep_threat_level_t threat = bridge->fep_effects.threat_level;

    /* Compute urgency */
    float urgency = free_energy / bridge->config.critical_fe_threshold;
    if (urgency > 1.0f) urgency = 1.0f;

    /* Determine response */
    sec_async_fep_response_t response = determine_response(threat, urgency);

    *response_out = response;
    *urgency_out = urgency;

    /* Track if response is taken */
    if (response != SEC_ASYNC_FEP_RESPONSE_NONE) {
        bridge->stats.protective_responses++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report false positive detection
 * WHY:  Reduce precision to prevent similar false positives
 * HOW:  Decrease precision, update statistics
 */
int sec_async_fep_report_false_positive(sec_async_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_report_false_positive: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->async_effects.false_positives++;
    bridge->stats.false_positive_count++;

    /* Reduce precision if learning from FPs enabled */
    if (bridge->config.learn_from_false_positives) {
        float reduction = 0.9f;
        bridge->state.current_precision *= reduction;

        if (bridge->state.current_precision < SEC_ASYNC_FEP_MIN_PRECISION) {
            bridge->state.current_precision = SEC_ASYNC_FEP_MIN_PRECISION;
        }
    }

    /* Update detection accuracy */
    uint64_t total_positives = bridge->stats.true_positive_count +
                               bridge->stats.false_positive_count;
    if (total_positives > 0) {
        bridge->stats.detection_accuracy =
            (float)bridge->stats.true_positive_count / (float)total_positives;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

/**
 * WHAT: Get FEP effects on async security
 * WHY:  Allow inspection of current FEP-derived effects
 * HOW:  Copy effects structure
 */
int sec_async_fep_get_fep_effects(
    const sec_async_fep_bridge_t* bridge,
    fep_to_async_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_get_fep_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

/**
 * WHAT: Get async security effects on FEP
 * WHY:  Allow inspection of async-derived effects
 * HOW:  Copy effects structure
 */
int sec_async_fep_get_async_effects(
    const sec_async_fep_bridge_t* bridge,
    async_to_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_get_async_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->async_effects;
    return 0;
}

/**
 * WHAT: Get current bridge state
 * WHY:  Allow inspection of operational state
 * HOW:  Copy state structure
 */
int sec_async_fep_get_state(
    const sec_async_fep_bridge_t* bridge,
    sec_async_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    *state = bridge->state;
    return 0;
}

/**
 * WHAT: Get bridge statistics
 * WHY:  Performance monitoring and tuning
 * HOW:  Copy statistics structure
 */
int sec_async_fep_get_stats(
    const sec_async_fep_bridge_t* bridge,
    sec_async_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

/**
 * WHAT: Get current free energy
 * WHY:  Quick access to key metric
 * HOW:  Return from effects
 */
float sec_async_fep_get_free_energy(const sec_async_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->fep_effects.free_energy;
}

/**
 * WHAT: Get current surprise level
 * WHY:  Quick access to key metric
 * HOW:  Return from effects
 */
float sec_async_fep_get_surprise(const sec_async_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->fep_effects.surprise_level;
}

/**
 * WHAT: Get current threat level
 * WHY:  Quick access to threat classification
 * HOW:  Return from effects
 */
sec_async_fep_threat_level_t sec_async_fep_get_threat_level(
    const sec_async_fep_bridge_t* bridge
) {
    if (!bridge) {
        return SEC_ASYNC_FEP_THREAT_NONE;
    }
    return bridge->fep_effects.threat_level;
}

/**
 * WHAT: Get current message rate
 * WHY:  Quick access to async metric
 * HOW:  Return from async effects
 */
float sec_async_fep_get_message_rate(const sec_async_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->async_effects.current_msg_rate;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

/**
 * WHAT: Connect to bio-async router
 * WHY:  Enable inter-module security notifications
 * HOW:  Register module with router, setup inbox
 */
int sec_async_fep_connect_bio_async(sec_async_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_async_fep_connect_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_ASYNC_FEP,
        .module_name = "sec_async_fep_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Security Async FEP bridge connected to bio-async");
    }

    return 0;
}

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of inter-module communication
 * HOW:  Unregister module from router
 */
int sec_async_fep_disconnect_bio_async(sec_async_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Security Async FEP bridge disconnected from bio-async");
    return 0;
}

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Allow callers to verify connection status
 * HOW:  Return connection flag
 */
bool sec_async_fep_is_bio_async_connected(const sec_async_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/**
 * WHAT: Process pending bio-async messages
 * WHY:  Handle incoming security notifications
 * HOW:  Uses bio_router_process_inbox for message handling
 */
uint32_t sec_async_fep_process_bio_messages(
    sec_async_fep_bridge_t* bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;
    }

    return bio_router_process_inbox(bridge->base.bio_ctx, max_messages);
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

/**
 * WHAT: Get human-readable name for threat level
 * WHY:  Facilitate logging and debugging
 * HOW:  Switch on enum value
 */
const char* sec_async_fep_threat_level_name(sec_async_fep_threat_level_t level) {
    switch (level) {
        case SEC_ASYNC_FEP_THREAT_NONE:
            return "None";
        case SEC_ASYNC_FEP_THREAT_SUSPICIOUS:
            return "Suspicious";
        case SEC_ASYNC_FEP_THREAT_MODERATE:
            return "Moderate";
        case SEC_ASYNC_FEP_THREAT_HIGH:
            return "High";
        case SEC_ASYNC_FEP_THREAT_CRITICAL:
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
const char* sec_async_fep_response_name(sec_async_fep_response_t response) {
    switch (response) {
        case SEC_ASYNC_FEP_RESPONSE_NONE:
            return "None";
        case SEC_ASYNC_FEP_RESPONSE_MONITOR:
            return "Monitor";
        case SEC_ASYNC_FEP_RESPONSE_THROTTLE:
            return "Throttle";
        case SEC_ASYNC_FEP_RESPONSE_ISOLATE:
            return "Isolate";
        case SEC_ASYNC_FEP_RESPONSE_BLOCK:
            return "Block";
        default:
            return "Unknown";
    }
}

/**
 * WHAT: Get human-readable name for anomaly type
 * WHY:  Facilitate logging and debugging
 * HOW:  Switch on enum value
 */
const char* sec_async_fep_anomaly_name(sec_async_anomaly_type_t anomaly) {
    switch (anomaly) {
        case SEC_ASYNC_ANOMALY_NONE:
            return "None";
        case SEC_ASYNC_ANOMALY_TIMING:
            return "Timing";
        case SEC_ASYNC_ANOMALY_FLOOD:
            return "Flood";
        case SEC_ASYNC_ANOMALY_QUEUE:
            return "Queue";
        case SEC_ASYNC_ANOMALY_ROUTING:
            return "Routing";
        case SEC_ASYNC_ANOMALY_PATTERN:
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
void sec_async_fep_print_summary(const sec_async_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Security Async FEP Bridge: NULL\n");
        return;
    }

    printf("=== Security Async FEP Bridge Summary ===\n");
    printf("State:\n");
    printf("  Active: %s\n", bridge->state.active ? "yes" : "no");
    printf("  Updates: %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Detections: %lu\n", (unsigned long)bridge->state.detection_count);
    printf("  Precision: %.3f\n", bridge->state.current_precision);
    printf("\n");
    printf("FEP Effects:\n");
    printf("  Free Energy: %.3f\n", bridge->fep_effects.free_energy);
    printf("  Surprise: %.3f\n", bridge->fep_effects.surprise_level);
    printf("  Prediction Error: %.3f\n", bridge->fep_effects.prediction_error);
    printf("  Threat Level: %s\n",
           sec_async_fep_threat_level_name(bridge->fep_effects.threat_level));
    printf("  Threat Confidence: %.3f\n", bridge->fep_effects.threat_confidence);
    printf("  Async Health: %.3f\n", bridge->fep_effects.async_health_estimate);
    printf("  Detected Anomaly: %s\n",
           sec_async_fep_anomaly_name(bridge->fep_effects.detected_anomaly));
    printf("  Recommended Response: %s\n",
           sec_async_fep_response_name(bridge->fep_effects.recommended_response));
    printf("\n");
    printf("Async Effects:\n");
    printf("  Message Rate: %.2f msg/sec\n", bridge->async_effects.current_msg_rate);
    printf("  Avg Interval: %.2f ms\n", bridge->async_effects.avg_interval_ms);
    printf("  Queue Util: %.2f%%\n", bridge->async_effects.queue_utilization * 100.0f);
    printf("  Floods Detected: %lu\n",
           (unsigned long)bridge->async_effects.floods_detected);
    printf("  Timing Anomalies: %lu\n",
           (unsigned long)bridge->async_effects.timing_anomalies);
    printf("  Under Attack: %s\n",
           bridge->async_effects.under_attack ? "yes" : "no");
    printf("==========================================\n");
}

/**
 * WHAT: Print statistics
 * WHY:  Performance monitoring support
 * HOW:  Format and print statistics
 */
void sec_async_fep_print_stats(const sec_async_fep_stats_t* stats) {
    if (!stats) {
        printf("Statistics: NULL\n");
        return;
    }

    printf("=== Security Async FEP Statistics ===\n");
    printf("Total Updates: %lu\n", (unsigned long)stats->total_updates);
    printf("FEP Detections: %lu\n", (unsigned long)stats->fep_detections);
    printf("Threats Detected: %lu\n", (unsigned long)stats->threats_detected);
    printf("Protective Responses: %lu\n", (unsigned long)stats->protective_responses);
    printf("Precision Adaptations: %lu\n", (unsigned long)stats->precision_adaptations);
    printf("\n");
    printf("By Anomaly Type:\n");
    printf("  Timing Anomalies: %lu\n", (unsigned long)stats->timing_anomalies);
    printf("  Flood Detections: %lu\n", (unsigned long)stats->flood_detections);
    printf("  Queue Anomalies: %lu\n", (unsigned long)stats->queue_anomalies);
    printf("  Routing Anomalies: %lu\n", (unsigned long)stats->routing_anomalies);
    printf("\n");
    printf("Averages:\n");
    printf("  Free Energy: %.3f\n", stats->avg_free_energy);
    printf("  Surprise: %.3f\n", stats->avg_surprise);
    printf("  Prediction Error: %.3f\n", stats->avg_prediction_error);
    printf("  Precision: %.3f\n", stats->current_precision);
    printf("\n");
    printf("Maximums:\n");
    printf("  Free Energy: %.3f\n", stats->max_free_energy);
    printf("  Surprise: %.3f\n", stats->max_surprise);
    printf("  Message Rate: %.2f msg/sec\n", stats->max_msg_rate);
    printf("\n");
    printf("Detection Performance:\n");
    printf("  True Positives: %lu\n", (unsigned long)stats->true_positive_count);
    printf("  False Positives: %lu\n", (unsigned long)stats->false_positive_count);
    printf("  Accuracy: %.3f\n", stats->detection_accuracy);
    printf("======================================\n");
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

/**
 * WHAT: Compute free energy from async metrics
 * WHY:  Map async security domain to FEP domain
 * HOW:  Weighted combination of rate deviation, timing, queue state
 *
 * High rate deviation = high free energy (flood attack)
 * Timing anomalies = prediction error contribution
 * Queue congestion = surprise contribution
 */
static float compute_free_energy_from_async(
    float msg_rate,
    float rate_deviation,
    float queue_util,
    const sec_async_fep_config_t* config
) {
    /*
     * Free energy increases as async behavior deviates from expected
     * F = rate_scale * rate_dev + timing_weight * timing_pe + queue_weight * queue_util
     */

    /* Rate deviation contribution */
    float rate_fe = config->rate_to_fe_scale * rate_deviation;

    /* Queue utilization contribution (high util = high surprise) */
    float queue_fe = config->queue_surprise_weight * queue_util * 10.0f;

    /* Message rate absolute component (very high rates are suspicious) */
    float rate_abs_fe = 0.0f;
    if (msg_rate > config->expected_msg_rate * 5.0f) {
        rate_abs_fe = (msg_rate / config->expected_msg_rate - 5.0f) * 2.0f;
    }

    return rate_fe + queue_fe + rate_abs_fe;
}

/**
 * WHAT: Compute surprise from timing deviation
 * WHY:  Surprise = -log(probability of observation)
 * HOW:  Approximate using deviation from expected interval
 */
static float compute_surprise_from_timing(
    float interval_deviation,
    float expected_interval
) {
    /*
     * Surprise proportional to deviation from expectation
     * Higher deviation = less probable = more surprising
     */
    float normalized_dev = interval_deviation / (expected_interval + 0.01f);

    /* Apply log-like transformation for surprise */
    float surprise = -logf(1.0f - normalized_dev + 0.01f);

    if (surprise < 0.0f) surprise = 0.0f;
    if (surprise > 20.0f) surprise = 20.0f;

    return surprise;
}

/**
 * WHAT: Classify threat level from free energy
 * WHY:  Map continuous FE to discrete threat categories
 * HOW:  Threshold-based classification
 */
static sec_async_fep_threat_level_t classify_threat_level(
    float free_energy,
    const sec_async_fep_config_t* config
) {
    if (free_energy >= config->critical_fe_threshold) {
        return SEC_ASYNC_FEP_THREAT_CRITICAL;
    } else if (free_energy >= config->free_energy_threshold) {
        return SEC_ASYNC_FEP_THREAT_HIGH;
    } else if (free_energy >= SEC_ASYNC_FEP_SUSPICIOUS_THRESHOLD) {
        return SEC_ASYNC_FEP_THREAT_MODERATE;
    } else if (free_energy >= config->normal_fe_threshold) {
        return SEC_ASYNC_FEP_THREAT_SUSPICIOUS;
    } else {
        return SEC_ASYNC_FEP_THREAT_NONE;
    }
}

/**
 * WHAT: Determine appropriate response based on threat and urgency
 * WHY:  Active inference selects actions to minimize expected FE
 * HOW:  Map threat level and urgency to response type
 */
static sec_async_fep_response_t determine_response(
    sec_async_fep_threat_level_t threat,
    float urgency
) {
    switch (threat) {
        case SEC_ASYNC_FEP_THREAT_CRITICAL:
            if (urgency > 0.8f) {
                return SEC_ASYNC_FEP_RESPONSE_BLOCK;
            } else {
                return SEC_ASYNC_FEP_RESPONSE_ISOLATE;
            }

        case SEC_ASYNC_FEP_THREAT_HIGH:
            return SEC_ASYNC_FEP_RESPONSE_ISOLATE;

        case SEC_ASYNC_FEP_THREAT_MODERATE:
            return SEC_ASYNC_FEP_RESPONSE_THROTTLE;

        case SEC_ASYNC_FEP_THREAT_SUSPICIOUS:
            return SEC_ASYNC_FEP_RESPONSE_MONITOR;

        case SEC_ASYNC_FEP_THREAT_NONE:
        default:
            return SEC_ASYNC_FEP_RESPONSE_NONE;
    }
}

/**
 * WHAT: Update running averages for metrics
 * WHY:  Smooth metrics over time for stability
 * HOW:  Exponential moving average
 */
static void update_running_averages(
    sec_async_fep_bridge_t* bridge,
    float free_energy,
    float surprise,
    float pred_error
) {
    const float alpha = 0.1f;  /* EMA smoothing factor */

    bridge->state.avg_surprise =
        (1.0f - alpha) * bridge->state.avg_surprise + alpha * surprise;

    bridge->state.avg_prediction_error =
        (1.0f - alpha) * bridge->state.avg_prediction_error + alpha * pred_error;

    /* Also track in stats */
    bridge->stats.avg_free_energy =
        (1.0f - alpha) * bridge->stats.avg_free_energy + alpha * free_energy;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    bridge->stats.avg_prediction_error = bridge->state.avg_prediction_error;
}

/**
 * WHAT: Update temporal pattern tracking
 * WHY:  Track message timing for FEP predictions
 * HOW:  Compute intervals, update statistics
 */
static void update_temporal_pattern(
    sec_async_fep_bridge_t* bridge,
    uint64_t timestamp_ms
) {
    async_temporal_pattern_t* temporal = &bridge->state.temporal;

    /* Compute interval from last message */
    float interval_ms = 0.0f;
    if (temporal->last_message_time_ms > 0) {
        interval_ms = (float)(timestamp_ms - temporal->last_message_time_ms);
    }
    temporal->last_message_time_ms = timestamp_ms;

    /* Update interval statistics using EMA */
    if (interval_ms > 0.0f) {
        float alpha = 0.1f;
        temporal->mean_interval_ms =
            (1.0f - alpha) * temporal->mean_interval_ms + alpha * interval_ms;

        /* Update variance using Welford's online algorithm approximation */
        float diff = interval_ms - temporal->mean_interval_ms;
        temporal->variance_interval_ms =
            (1.0f - alpha) * temporal->variance_interval_ms + alpha * diff * diff;

        /* Update async effects */
        bridge->async_effects.avg_interval_ms = temporal->mean_interval_ms;
    }

    /* Count message in window */
    temporal->messages_in_window++;

    /* Update predicted interval */
    float alpha_pred = 0.05f;
    temporal->predicted_interval =
        (1.0f - alpha_pred) * temporal->predicted_interval +
        alpha_pred * temporal->mean_interval_ms;
}

/**
 * WHAT: Compute rate deviation from expected
 * WHY:  Rate deviation is key FEP signal
 * HOW:  Compare current rate to predicted rate
 */
static float compute_rate_deviation(const sec_async_fep_bridge_t* bridge) {
    float current_rate = bridge->async_effects.current_msg_rate;
    float predicted_rate = bridge->state.temporal.predicted_rate;

    if (predicted_rate < 0.1f) {
        predicted_rate = bridge->config.expected_msg_rate;
    }

    return fabsf(current_rate - predicted_rate) / predicted_rate;
}

/**
 * WHAT: Identify the type of anomaly from FEP metrics
 * WHY:  Different anomaly types trigger different responses
 * HOW:  Analyze which metrics are most anomalous
 */
static sec_async_anomaly_type_t identify_anomaly_type(
    const sec_async_fep_bridge_t* bridge,
    float free_energy,
    float surprise
) {
    /* If no anomaly, return none */
    if (free_energy < SEC_ASYNC_FEP_NORMAL_THRESHOLD) {
        return SEC_ASYNC_ANOMALY_NONE;
    }

    /* Check rate deviation (flood detection) */
    float rate_dev = compute_rate_deviation(bridge);
    if (rate_dev > bridge->config.rate_deviation_threshold) {
        return SEC_ASYNC_ANOMALY_FLOOD;
    }

    /* Check queue anomaly */
    if (bridge->state.queue_obs.anomaly_detected) {
        return SEC_ASYNC_ANOMALY_QUEUE;
    }

    /* Check timing anomaly (high surprise with moderate free energy) */
    if (surprise > SEC_ASYNC_FEP_SURPRISE_ANOMALY) {
        return SEC_ASYNC_ANOMALY_TIMING;
    }

    /* Default to general pattern anomaly */
    return SEC_ASYNC_ANOMALY_PATTERN;
}
