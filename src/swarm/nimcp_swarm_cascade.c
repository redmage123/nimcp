/**
 * @file nimcp_swarm_cascade.c
 * @brief Cascading Failure Prevention System Implementation
 *
 * Biological Inspiration:
 * - Power grid circuit breakers that isolate faults
 * - Neural network graceful degradation under damage
 * - Immune system isolation and recovery protocols
 * - Homeostatic regulation maintaining system stability
 */

#include "swarm/nimcp_swarm_cascade.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "swarm_cascade"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_cascade)

#include <string.h>
#include <math.h>
#include <float.h>
#include <stdio.h>

/* ============================================================================
 * Internal Constants
 * ========================================================================== */

#define CASCADE_DEFAULT_TELEMETRY_WINDOW 100
#define CASCADE_DEFAULT_BREAKER_CAPACITY 16
#define CASCADE_DEFAULT_CAPABILITY_CAPACITY 32
#define CASCADE_DEFAULT_GROUP_CAPACITY 8
#define CASCADE_ANOMALY_SIGMA_THRESHOLD 3.0
#define CASCADE_MIN_SAMPLES_FOR_BASELINE 10
#define CASCADE_CASCADE_WINDOW_MS 5000
#define CASCADE_CASCADE_RATE_THRESHOLD 2.0  /* failures per second */
#define CASCADE_CORRELATION_THRESHOLD 0.7

/* Bio-async message types */
#define BIOMSG_HEALTH_STATUS 0x1001
#define BIOMSG_FAILURE_ALERT 0x1002
#define BIOMSG_RECOVERY_STATUS 0x1003
#define BIOMSG_HEARTBEAT 0x1004
#define BIOMSG_FAILOVER 0x1005

/* ============================================================================
 * Internal Helper Functions
 * ========================================================================== */

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Calculate mean of telemetry samples
 */
static double calculate_mean(const double *values, uint32_t count) {
    if (count == 0) return 0.0;

    double sum = 0.0;
    for (uint32_t i = 0; i < count; i++) {
        sum += values[i];
    }
    return sum / count;
}

/**
 * @brief Calculate standard deviation
 */
static double calculate_std(const double *values, uint32_t count, double mean) {
    if (count < 2) return 0.0;

    double sum_sq_diff = 0.0;
    for (uint32_t i = 0; i < count; i++) {
        double diff = values[i] - mean;
        sum_sq_diff += diff * diff;
    }
    return sqrt(sum_sq_diff / (count - 1));
}

/**
 * @brief Update statistical baseline from telemetry history
 */
static void update_baseline(nimcp_telemetry_history_t *history) {
    if (history->count < CASCADE_MIN_SAMPLES_FOR_BASELINE) {
        return;
    }

    /* Collect CPU, memory, and latency samples */
    double *cpu_samples = (double*)nimcp_malloc(history->count * sizeof(double));
    double *mem_samples = (double*)nimcp_malloc(history->count * sizeof(double));
    double *lat_samples = (double*)nimcp_malloc(history->count * sizeof(double));

    if (!cpu_samples || !mem_samples || !lat_samples) {
        nimcp_free(cpu_samples);
        nimcp_free(mem_samples);
        nimcp_free(lat_samples);
        return;
    }

    for (uint32_t i = 0; i < history->count; i++) {
        uint32_t idx = (history->head + history->capacity - history->count + i) % history->capacity;
        cpu_samples[i] = history->samples[idx].cpu_usage;
        mem_samples[i] = history->samples[idx].memory_usage;
        lat_samples[i] = history->samples[idx].network_latency_ms;
    }

    /* Calculate statistics */
    history->mean_cpu = calculate_mean(cpu_samples, history->count);
    history->mean_memory = calculate_mean(mem_samples, history->count);
    history->mean_latency = calculate_mean(lat_samples, history->count);

    history->std_cpu = calculate_std(cpu_samples, history->count, history->mean_cpu);
    history->std_memory = calculate_std(mem_samples, history->count, history->mean_memory);
    history->std_latency = calculate_std(lat_samples, history->count, history->mean_latency);

    nimcp_free(cpu_samples);
    nimcp_free(mem_samples);
    nimcp_free(lat_samples);
}

/**
 * @brief Calculate health state from telemetry
 */
static nimcp_health_state_t calculate_health_state(
    const nimcp_health_telemetry_t *telemetry
) {
    /* Simple heuristic-based health assessment */
    double error_rate = (double)telemetry->failed_ops /
                       (telemetry->successful_ops + telemetry->failed_ops + 1);

    /* Failed state: critical metrics */
    if (error_rate > 0.9 || telemetry->cpu_usage > 0.99 ||
        telemetry->packet_loss_rate > 0.9) {
        return HEALTH_FAILED;
    }

    /* Failing state: severe degradation */
    if (error_rate > 0.5 || telemetry->cpu_usage > 0.95 ||
        telemetry->memory_usage > 0.95 || telemetry->packet_loss_rate > 0.5) {
        return HEALTH_FAILING;
    }

    /* Degraded state: moderate issues */
    if (error_rate > 0.2 || telemetry->cpu_usage > 0.85 ||
        telemetry->memory_usage > 0.85 || telemetry->network_latency_ms > 1000 ||
        telemetry->packet_loss_rate > 0.2) {
        return HEALTH_DEGRADED;
    }

    /* Optimal state */
    return HEALTH_OPTIMAL;
}

/**
 * @brief Send bio-async message
 */
static nimcp_result_t send_bio_message(
    void *bio_ctx,
    uint32_t msg_type,
    const void *data,
    size_t data_size
) {
    if (!bio_ctx || !data) {
        return NIMCP_INVALID_PARAM;
    }

    /* Build message with header */
    size_t total_size = sizeof(bio_message_header_t) + data_size;
    uint8_t* msg_buffer = nimcp_malloc(total_size);
    if (!msg_buffer) {
        LOG_ERROR("Failed to allocate message buffer");
        return NIMCP_ERROR_MEMORY;
    }

    bio_message_header_t* header = (bio_message_header_t*)msg_buffer;
    header->type = (bio_message_type_t)msg_type;
    header->sequence_id = 0;  /* Router will assign */
    header->source_module = BIO_MODULE_SWARM_CASCADE;
    header->target_module = BIO_MODULE_ALL;  /* Broadcast */
    header->timestamp_us = get_time_us();
    header->channel = BIO_CHANNEL_NOREPINEPHRINE;  /* Alert channel */
    header->payload_size = (uint32_t)data_size;
    header->flags = BIO_MSG_FLAG_BROADCAST;

    /* Copy payload */
    memcpy(msg_buffer + sizeof(bio_message_header_t), data, data_size);

    /* Send via bio-router if available */
    nimcp_result_t result = NIMCP_SUCCESS;
    bio_module_context_t ctx = (bio_module_context_t)bio_ctx;
    if (bio_router_is_initialized()) {
        nimcp_error_t err = bio_router_broadcast(ctx, msg_buffer, total_size);
        if (err != NIMCP_SUCCESS) {
            LOG_WARN("Cascade: bio-async broadcast failed: %d", err);
            result = (nimcp_result_t)err;
        } else {
            LOG_DEBUG("Cascade: sent bio-async message type=0x%x size=%zu", msg_type, data_size);
        }
    } else {
        LOG_DEBUG("Cascade: bio-router not initialized, message type=0x%x queued", msg_type);
    }

    nimcp_free(msg_buffer);
    return result;
}

/* ============================================================================
 * System Lifecycle
 * ========================================================================== */

nimcp_result_t nimcp_cascade_get_default_config(nimcp_cascade_config_t *config) {
    if (!config) {
        return NIMCP_INVALID_PARAM;
    }

    memset(config, 0, sizeof(nimcp_cascade_config_t));

    /* Failure prediction settings */
    config->enable_ml_prediction = true;
    config->anomaly_threshold = 0.8;
    config->telemetry_window_size = CASCADE_DEFAULT_TELEMETRY_WINDOW;

    /* Circuit breaker defaults */
    config->default_breaker_config.failure_threshold = 5;
    config->default_breaker_config.timeout_us = 30000000;  /* 30 seconds */
    config->default_breaker_config.success_threshold = 3;
    config->default_breaker_config.half_open_max_calls = 5;

    /* Load shedding settings */
    config->enable_auto_shedding = true;
    config->shedding_threshold = 0.7;

    /* Cascade detection settings */
    config->cascade_window_ms = CASCADE_CASCADE_WINDOW_MS;
    config->cascade_rate_threshold = CASCADE_CASCADE_RATE_THRESHOLD;

    /* Recovery settings */
    config->default_recovery_strategy = RECOVERY_GRADUAL;
    config->recovery_timeout_us = 300000000;  /* 5 minutes */

    /* Redundancy settings */
    config->enable_auto_failover = true;
    config->heartbeat_interval_us = 1000000;  /* 1 second */

    return NIMCP_SUCCESS;
}

nimcp_cascade_system_t* nimcp_cascade_create(
    const nimcp_cascade_config_t *config,
    void *bio_ctx
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_cascade_create: config is NULL");
        return NULL;
    }

    LOG_INFO( "Cascade: Creating failure prevention system");

    /* Allocate system structure */
    nimcp_cascade_system_t *system = (nimcp_cascade_system_t*)
        nimcp_calloc(1, sizeof(nimcp_cascade_system_t));
    if (!system) {
        LOG_ERROR( "Cascade: Failed to allocate system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_cascade_create: system is NULL");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&system->config, config, sizeof(nimcp_cascade_config_t));

    /* Initialize telemetry history */
    system->telemetry_history.capacity = config->telemetry_window_size;
    system->telemetry_history.samples = (nimcp_health_telemetry_t*)
        nimcp_calloc(config->telemetry_window_size, sizeof(nimcp_health_telemetry_t));
    if (!system->telemetry_history.samples) {
        LOG_ERROR( "Cascade: Failed to allocate telemetry history");
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_cascade_create: system->telemetry_history is NULL");
        return NULL;
    }

    /* Allocate circuit breakers */
    system->breaker_capacity = CASCADE_DEFAULT_BREAKER_CAPACITY;
    system->breakers = (nimcp_circuit_breaker_t*)
        nimcp_calloc(system->breaker_capacity, sizeof(nimcp_circuit_breaker_t));
    if (!system->breakers) {
        LOG_ERROR( "Cascade: Failed to allocate breakers");
        nimcp_free(system->telemetry_history.samples);
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_cascade_create: system->breakers is NULL");
        return NULL;
    }

    /* Allocate capabilities */
    system->capability_capacity = CASCADE_DEFAULT_CAPABILITY_CAPACITY;
    system->capabilities = (nimcp_capability_t*)
        nimcp_calloc(system->capability_capacity, sizeof(nimcp_capability_t));
    if (!system->capabilities) {
        LOG_ERROR( "Cascade: Failed to allocate capabilities");
        nimcp_free(system->breakers);
        nimcp_free(system->telemetry_history.samples);
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_cascade_create: system->capabilities is NULL");
        return NULL;
    }

    /* Allocate redundancy groups */
    system->group_capacity = CASCADE_DEFAULT_GROUP_CAPACITY;
    system->groups = (nimcp_redundancy_group_t*)
        nimcp_calloc(system->group_capacity, sizeof(nimcp_redundancy_group_t));
    if (!system->groups) {
        LOG_ERROR( "Cascade: Failed to allocate groups");
        nimcp_free(system->capabilities);
        nimcp_free(system->breakers);
        nimcp_free(system->telemetry_history.samples);
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_cascade_create: system->groups is NULL");
        return NULL;
    }

    /* Initialize state */
    system->health_state = HEALTH_OPTIMAL;
    system->bio_ctx = bio_ctx;
    system->bio_enabled = (bio_ctx != NULL);

    /* Create lock */
    system->lock = nimcp_platform_mutex_create();
    if (!system->lock) {
        LOG_ERROR( "Cascade: Failed to create lock");
        nimcp_free(system->groups);
        nimcp_free(system->capabilities);
        nimcp_free(system->breakers);
        nimcp_free(system->telemetry_history.samples);
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_cascade_create: system->lock is NULL");
        return NULL;
    }

    LOG_INFO( "Cascade: System created successfully");
    return system;
}

void nimcp_cascade_destroy(nimcp_cascade_system_t *system) {
    if (!system) {
        return;
    }

    LOG_INFO( "Cascade: Destroying system");

    if (system->lock) {
        nimcp_platform_mutex_destroy(system->lock);
        nimcp_free(system->lock);
        system->lock = NULL;
    }

    nimcp_free(system->groups);
    nimcp_free(system->capabilities);
    nimcp_free(system->breakers);
    nimcp_free(system->telemetry_history.samples);
    nimcp_free(system);
}

/* ============================================================================
 * Health Monitoring
 * ========================================================================== */

nimcp_result_t nimcp_cascade_update_telemetry(
    nimcp_cascade_system_t *system,
    const nimcp_health_telemetry_t *telemetry
) {
    if (!system || !telemetry) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    /* Update current telemetry */
    memcpy(&system->current_telemetry, telemetry, sizeof(nimcp_health_telemetry_t));

    /* Add to history */
    nimcp_telemetry_history_t *history = &system->telemetry_history;
    memcpy(&history->samples[history->head], telemetry, sizeof(nimcp_health_telemetry_t));
    history->head = (history->head + 1) % history->capacity;
    if (history->count < history->capacity) {
        history->count++;
    }

    /* Update baseline statistics */
    update_baseline(history);

    /* Calculate new health state */
    nimcp_health_state_t old_state = system->health_state;
    nimcp_health_state_t new_state = calculate_health_state(telemetry);
    system->health_state = new_state;

    /* Log state changes */
    if (old_state != new_state) {
        LOG_INFO( "Cascade: Health state changed from %s to %s",
                  nimcp_cascade_health_state_string(old_state),
                  nimcp_cascade_health_state_string(new_state));

        /* Record failure event if degrading */
        if (new_state > old_state && new_state >= HEALTH_DEGRADED) {
            nimcp_failure_event_t event;
            event.node_id = system->node_id;
            event.prev_state = old_state;
            event.new_state = new_state;
            event.severity = (new_state == HEALTH_FAILING) ? FAILURE_SEVERITY_MAJOR : FAILURE_SEVERITY_MODERATE;
            event.timestamp_us = get_time_us();
            snprintf(event.description, sizeof(event.description),
                     "Health degraded to %s", nimcp_cascade_health_state_string(new_state));

            nimcp_cascade_record_failure(system, &event);
        }

        /* Broadcast health status if bio-async enabled */
        if (system->bio_enabled) {
            nimcp_cascade_broadcast_health(system);
        }
    }

    nimcp_platform_mutex_unlock(system->lock);
    return NIMCP_SUCCESS;
}

nimcp_health_state_t nimcp_cascade_get_health_state(
    const nimcp_cascade_system_t *system
) {
    if (!system) {
        return HEALTH_UNKNOWN;
    }

    return system->health_state;
}

const char* nimcp_cascade_health_state_string(nimcp_health_state_t state) {
    switch (state) {
        case HEALTH_OPTIMAL: return "OPTIMAL";
        case HEALTH_DEGRADED: return "DEGRADED";
        case HEALTH_FAILING: return "FAILING";
        case HEALTH_FAILED: return "FAILED";
        case HEALTH_RECOVERING: return "RECOVERING";
        case HEALTH_UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

/* ============================================================================
 * Failure Prediction
 * ========================================================================== */

nimcp_result_t nimcp_cascade_detect_anomaly(
    nimcp_cascade_system_t *system,
    const nimcp_health_telemetry_t *telemetry,
    nimcp_anomaly_detection_t *result
) {
    if (!system || !telemetry || !result) {
        return NIMCP_INVALID_PARAM;
    }

    memset(result, 0, sizeof(nimcp_anomaly_detection_t));
    result->detection_time_us = get_time_us();

    nimcp_platform_mutex_lock(system->lock);

    nimcp_telemetry_history_t *history = &system->telemetry_history;

    /* Need sufficient baseline data */
    if (history->count < CASCADE_MIN_SAMPLES_FOR_BASELINE) {
        nimcp_platform_mutex_unlock(system->lock);
        result->is_anomalous = false;
        return NIMCP_SUCCESS;
    }

    /* Check CPU usage anomaly */
    double cpu_deviation = 0.0;
    if (history->std_cpu > 0.001) {
        cpu_deviation = fabs(telemetry->cpu_usage - history->mean_cpu) / history->std_cpu;
        if (cpu_deviation > CASCADE_ANOMALY_SIGMA_THRESHOLD) {
            result->is_anomalous = true;
            result->deviation_sigma = cpu_deviation;
            result->anomaly_score = fmin(cpu_deviation / 5.0, 1.0);
            strncpy(result->metric_name, "cpu_usage", sizeof(result->metric_name) - 1);
        }
    }

    /* Check memory usage anomaly */
    double mem_deviation = 0.0;
    if (history->std_memory > 0.001) {
        mem_deviation = fabs(telemetry->memory_usage - history->mean_memory) / history->std_memory;
        if (mem_deviation > CASCADE_ANOMALY_SIGMA_THRESHOLD && mem_deviation > cpu_deviation) {
            result->is_anomalous = true;
            result->deviation_sigma = mem_deviation;
            result->anomaly_score = fmin(mem_deviation / 5.0, 1.0);
            strncpy(result->metric_name, "memory_usage", sizeof(result->metric_name) - 1);
        }
    }

    /* Check latency anomaly */
    double lat_deviation = 0.0;
    if (history->std_latency > 0.001) {
        lat_deviation = fabs(telemetry->network_latency_ms - history->mean_latency) /
                       history->std_latency;
        if (lat_deviation > CASCADE_ANOMALY_SIGMA_THRESHOLD &&
            lat_deviation > cpu_deviation && lat_deviation > mem_deviation) {
            result->is_anomalous = true;
            result->deviation_sigma = lat_deviation;
            result->anomaly_score = fmin(lat_deviation / 5.0, 1.0);
            strncpy(result->metric_name, "network_latency_ms", sizeof(result->metric_name) - 1);
        }
    }

    /* Store result */
    memcpy(&system->last_anomaly, result, sizeof(nimcp_anomaly_detection_t));

    nimcp_platform_mutex_unlock(system->lock);

    if (result->is_anomalous) {
        LOG_WARN(
                  "Cascade: Anomaly detected in %s (%.2f sigma, score %.2f)",
                  result->metric_name, result->deviation_sigma, result->anomaly_score);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_predict_failure(
    nimcp_cascade_system_t *system,
    nimcp_failure_prediction_t *prediction
) {
    if (!system || !prediction) {
        return NIMCP_INVALID_PARAM;
    }

    memset(prediction, 0, sizeof(nimcp_failure_prediction_t));
    prediction->prediction_time_us = get_time_us();

    nimcp_platform_mutex_lock(system->lock);

    /* Simple heuristic-based prediction */
    /* In production, this would use ML models */

    nimcp_health_state_t state = system->health_state;
    const nimcp_health_telemetry_t *telemetry = &system->current_telemetry;

    /* Already failed */
    if (state == HEALTH_FAILED) {
        prediction->failure_predicted = true;
        prediction->confidence = 1.0;
        prediction->time_to_failure_us = 0;
        prediction->severity = FAILURE_SEVERITY_CATASTROPHIC;
        strncpy(prediction->cause, "System already failed", sizeof(prediction->cause) - 1);
    }
    /* Failing state - imminent failure */
    else if (state == HEALTH_FAILING) {
        prediction->failure_predicted = true;
        prediction->confidence = 0.85;
        prediction->time_to_failure_us = 60000000;  /* ~1 minute */
        prediction->severity = FAILURE_SEVERITY_CRITICAL;
        strncpy(prediction->cause, "Critical resource exhaustion", sizeof(prediction->cause) - 1);
    }
    /* Degraded state - potential failure */
    else if (state == HEALTH_DEGRADED) {
        prediction->failure_predicted = true;
        prediction->confidence = 0.6;
        prediction->time_to_failure_us = 300000000;  /* ~5 minutes */
        prediction->severity = FAILURE_SEVERITY_MAJOR;

        if (telemetry->cpu_usage > 0.85) {
            strncpy(prediction->cause, "High CPU usage trend", sizeof(prediction->cause) - 1);
        } else if (telemetry->memory_usage > 0.85) {
            strncpy(prediction->cause, "High memory usage trend", sizeof(prediction->cause) - 1);
        } else if (telemetry->error_rate > 0.2) {
            strncpy(prediction->cause, "Elevated error rate", sizeof(prediction->cause) - 1);
        } else {
            strncpy(prediction->cause, "Multiple degraded metrics", sizeof(prediction->cause) - 1);
        }
    }
    /* Check anomalies in optimal state */
    else if (system->last_anomaly.is_anomalous) {
        prediction->failure_predicted = true;
        prediction->confidence = system->last_anomaly.anomaly_score;
        prediction->time_to_failure_us = 600000000;  /* ~10 minutes */
        prediction->severity = FAILURE_SEVERITY_MODERATE;
        snprintf(prediction->cause, sizeof(prediction->cause),
                 "Anomaly in %s", system->last_anomaly.metric_name);
    }

    /* Store prediction */
    memcpy(&system->last_prediction, prediction, sizeof(nimcp_failure_prediction_t));

    nimcp_platform_mutex_unlock(system->lock);

    if (prediction->failure_predicted) {
        LOG_WARN(
                  "Cascade: Failure predicted (confidence %.2f, TTF %lu us): %s",
                  prediction->confidence, prediction->time_to_failure_us, prediction->cause);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Circuit Breakers
 * ========================================================================== */

nimcp_result_t nimcp_cascade_register_breaker(
    nimcp_cascade_system_t *system,
    const char *service_name,
    const nimcp_breaker_config_t *config,
    uint32_t *breaker_id
) {
    if (!system || !service_name || !config || !breaker_id) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    /* Check capacity */
    if (system->num_breakers >= system->breaker_capacity) {
        nimcp_platform_mutex_unlock(system->lock);
        LOG_ERROR( "Cascade: Breaker capacity exceeded");
        return NIMCP_NO_MEMORY;
    }

    /* Initialize breaker */
    uint32_t id = system->num_breakers++;
    nimcp_circuit_breaker_t *breaker = &system->breakers[id];

    memset(breaker, 0, sizeof(nimcp_circuit_breaker_t));
    breaker->state = BREAKER_CLOSED;
    memcpy(&breaker->config, config, sizeof(nimcp_breaker_config_t));

    *breaker_id = id;

    nimcp_platform_mutex_unlock(system->lock);

    LOG_INFO( "Cascade: Registered breaker %u for service '%s'",
              id, service_name);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_record_operation(
    nimcp_cascade_system_t *system,
    uint32_t breaker_id,
    bool success
) {
    if (!system) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    if (breaker_id >= system->num_breakers) {
        nimcp_platform_mutex_unlock(system->lock);
        return NIMCP_INVALID_PARAM;
    }

    nimcp_circuit_breaker_t *breaker = &system->breakers[breaker_id];
    uint64_t now = get_time_us();

    switch (breaker->state) {
        case BREAKER_CLOSED:
            if (success) {
                breaker->failure_count = 0;
            } else {
                breaker->failure_count++;
                breaker->last_failure_time_us = now;

                /* Trip if threshold exceeded */
                if (breaker->failure_count >= breaker->config.failure_threshold) {
                    breaker->state = BREAKER_OPEN;
                    breaker->trip_time_us = now;
                    breaker->total_trips++;
                    LOG_WARN(
                              "Cascade: Breaker %u tripped (%u failures)",
                              breaker_id, breaker->failure_count);
                }
            }
            break;

        case BREAKER_OPEN:
            /* Check if timeout has elapsed */
            if (now - breaker->trip_time_us >= breaker->config.timeout_us) {
                breaker->state = BREAKER_HALF_OPEN;
                breaker->success_count = 0;
                breaker->failure_count = 0;
                LOG_INFO( "Cascade: Breaker %u entering half-open state",
                          breaker_id);
            }
            break;

        case BREAKER_HALF_OPEN:
            if (success) {
                breaker->success_count++;

                /* Close if success threshold met */
                if (breaker->success_count >= breaker->config.success_threshold) {
                    breaker->state = BREAKER_CLOSED;
                    breaker->failure_count = 0;
                    LOG_INFO( "Cascade: Breaker %u closed after recovery",
                              breaker_id);
                }
            } else {
                /* Re-trip on any failure */
                breaker->state = BREAKER_OPEN;
                breaker->trip_time_us = now;
                breaker->failure_count = 1;
                breaker->success_count = 0;
                LOG_WARN( "Cascade: Breaker %u re-tripped",
                          breaker_id);
            }
            break;
    }

    nimcp_platform_mutex_unlock(system->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_check_breaker(
    nimcp_cascade_system_t *system,
    uint32_t breaker_id,
    bool *allowed
) {
    if (!system || !allowed) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    if (breaker_id >= system->num_breakers) {
        nimcp_platform_mutex_unlock(system->lock);
        return NIMCP_INVALID_PARAM;
    }

    nimcp_circuit_breaker_t *breaker = &system->breakers[breaker_id];

    *allowed = (breaker->state != BREAKER_OPEN);

    nimcp_platform_mutex_unlock(system->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_get_breaker_state(
    const nimcp_cascade_system_t *system,
    uint32_t breaker_id,
    nimcp_circuit_breaker_t *breaker
) {
    if (!system || !breaker) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    if (breaker_id >= system->num_breakers) {
        nimcp_platform_mutex_unlock(system->lock);
        return NIMCP_INVALID_PARAM;
    }

    memcpy(breaker, &system->breakers[breaker_id], sizeof(nimcp_circuit_breaker_t));

    nimcp_platform_mutex_unlock(system->lock);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Load Shedding
 * ========================================================================== */

nimcp_result_t nimcp_cascade_register_capability(
    nimcp_cascade_system_t *system,
    const nimcp_capability_t *capability,
    uint32_t *capability_id
) {
    if (!system || !capability || !capability_id) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    /* Check capacity */
    if (system->num_capabilities >= system->capability_capacity) {
        nimcp_platform_mutex_unlock(system->lock);
        LOG_ERROR( "Cascade: Capability capacity exceeded");
        return NIMCP_NO_MEMORY;
    }

    /* Register capability */
    uint32_t id = system->num_capabilities++;
    memcpy(&system->capabilities[id], capability, sizeof(nimcp_capability_t));

    *capability_id = id;

    nimcp_platform_mutex_unlock(system->lock);

    LOG_INFO( "Cascade: Registered capability %u: '%s' (priority %d)",
              id, capability->name, capability->priority);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_decide_load_shedding(
    nimcp_cascade_system_t *system,
    nimcp_health_state_t target_health,
    nimcp_load_shedding_decision_t *decision
) {
    if (!system || !decision) {
        return NIMCP_INVALID_PARAM;
    }

    memset(decision, 0, sizeof(nimcp_load_shedding_decision_t));
    decision->target_state = target_health;

    nimcp_platform_mutex_lock(system->lock);

    /* Determine how much to shed based on current vs target health */
    nimcp_health_state_t current = system->health_state;

    /* No shedding needed if already at or better than target */
    if (current <= target_health) {
        nimcp_platform_mutex_unlock(system->lock);
        return NIMCP_SUCCESS;
    }

    /* Select capabilities to shed in priority order */
    /* Shed lower priority items first */
    for (int priority = PRIORITY_BACKGROUND; priority >= PRIORITY_LOW; priority--) {
        if (decision->capabilities_to_shed >= 32) {
            break;  /* Max capacity */
        }

        for (uint32_t i = 0; i < system->num_capabilities; i++) {
            nimcp_capability_t *cap = &system->capabilities[i];

            if (cap->enabled && cap->priority == priority) {
                decision->capability_indices[decision->capabilities_to_shed++] = i;
                decision->estimated_relief += cap->resource_cost;

                /* Stop if we've shed enough */
                if (decision->estimated_relief >= 0.3 && target_health == HEALTH_DEGRADED) {
                    goto done;
                }
                if (decision->estimated_relief >= 0.5 && target_health == HEALTH_OPTIMAL) {
                    goto done;
                }
            }

            if (decision->capabilities_to_shed >= 32) {
                break;
            }
        }
    }

done:
    nimcp_platform_mutex_unlock(system->lock);

    LOG_INFO(
              "Cascade: Load shedding decision: %u capabilities, %.2f relief",
              decision->capabilities_to_shed, decision->estimated_relief);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_apply_load_shedding(
    nimcp_cascade_system_t *system,
    const nimcp_load_shedding_decision_t *decision
) {
    if (!system || !decision) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    uint64_t now = get_time_us();

    for (uint32_t i = 0; i < decision->capabilities_to_shed; i++) {
        uint32_t idx = decision->capability_indices[i];
        if (idx < system->num_capabilities) {
            nimcp_capability_t *cap = &system->capabilities[idx];
            cap->enabled = false;
            cap->disable_time_us = now;

            LOG_INFO( "Cascade: Shed capability '%s'", cap->name);
        }
    }

    nimcp_platform_mutex_unlock(system->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_restore_capabilities(
    nimcp_cascade_system_t *system,
    uint32_t num_to_restore
) {
    if (!system) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    uint32_t restored = 0;

    /* Restore in reverse priority order (critical first) */
    for (int priority = PRIORITY_CRITICAL; priority <= PRIORITY_BACKGROUND; priority++) {
        for (uint32_t i = 0; i < system->num_capabilities && restored < num_to_restore; i++) {
            nimcp_capability_t *cap = &system->capabilities[i];

            if (!cap->enabled && cap->priority == priority) {
                cap->enabled = true;
                cap->disable_time_us = 0;
                restored++;

                LOG_INFO( "Cascade: Restored capability '%s'", cap->name);
            }
        }
    }

    nimcp_platform_mutex_unlock(system->lock);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Redundancy and Failover
 * ========================================================================== */

nimcp_result_t nimcp_cascade_register_redundancy_group(
    nimcp_cascade_system_t *system,
    const nimcp_redundancy_group_t *group,
    uint32_t *group_id
) {
    if (!system || !group || !group_id) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    if (system->num_groups >= system->group_capacity) {
        nimcp_platform_mutex_unlock(system->lock);
        return NIMCP_NO_MEMORY;
    }

    uint32_t id = system->num_groups++;
    memcpy(&system->groups[id], group, sizeof(nimcp_redundancy_group_t));

    *group_id = id;

    nimcp_platform_mutex_unlock(system->lock);

    LOG_INFO( "Cascade: Registered redundancy group %u: '%s'",
              id, group->group_name);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_update_heartbeat(
    nimcp_cascade_system_t *system,
    uint32_t group_id,
    uint32_t node_id
) {
    if (!system) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    if (group_id >= system->num_groups) {
        nimcp_platform_mutex_unlock(system->lock);
        return NIMCP_INVALID_PARAM;
    }

    nimcp_redundancy_group_t *group = &system->groups[group_id];

    if (node_id == group->primary_node_id) {
        group->last_heartbeat_us = get_time_us();
    }

    nimcp_platform_mutex_unlock(system->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_check_failover(
    nimcp_cascade_system_t *system,
    uint32_t group_id,
    nimcp_failover_decision_t *decision
) {
    if (!system || !decision) {
        return NIMCP_INVALID_PARAM;
    }

    memset(decision, 0, sizeof(nimcp_failover_decision_t));
    decision->decision_time_us = get_time_us();

    nimcp_platform_mutex_lock(system->lock);

    if (group_id >= system->num_groups) {
        nimcp_platform_mutex_unlock(system->lock);
        return NIMCP_INVALID_PARAM;
    }

    nimcp_redundancy_group_t *group = &system->groups[group_id];
    uint64_t now = get_time_us();

    /* Check if primary has timed out */
    if (now - group->last_heartbeat_us > group->failover_timeout_us) {
        decision->should_failover = true;
        decision->failed_node_id = group->primary_node_id;

        /* Select first hot standby as new primary */
        for (uint32_t i = 0; i < group->num_standbys; i++) {
            if (group->roles[i] == ROLE_HOT_STANDBY) {
                decision->new_primary_id = group->standby_node_ids[i];
                break;
            }
        }

        /* Fallback to any standby */
        if (decision->new_primary_id == 0 && group->num_standbys > 0) {
            decision->new_primary_id = group->standby_node_ids[0];
        }

        snprintf(decision->reason, sizeof(decision->reason),
                 "Primary node %u heartbeat timeout", group->primary_node_id);

        LOG_WARN( "Cascade: Failover needed in group %u: %s",
                  group_id, decision->reason);
    }

    nimcp_platform_mutex_unlock(system->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_execute_failover(
    nimcp_cascade_system_t *system,
    const nimcp_failover_decision_t *decision
) {
    if (!system || !decision || !decision->should_failover) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    /* Find group by primary node */
    for (uint32_t i = 0; i < system->num_groups; i++) {
        nimcp_redundancy_group_t *group = &system->groups[i];

        if (group->primary_node_id == decision->failed_node_id) {
            /* Update primary */
            group->primary_node_id = decision->new_primary_id;
            group->last_heartbeat_us = get_time_us();

            LOG_INFO(
                      "Cascade: Executed failover in group '%s': %u -> %u",
                      group->group_name, decision->failed_node_id,
                      decision->new_primary_id);

            /* Send bio-async failover message */
            if (system->bio_enabled) {
                send_bio_message(system->bio_ctx, BIOMSG_FAILOVER,
                               decision, sizeof(nimcp_failover_decision_t));
            }

            break;
        }
    }

    nimcp_platform_mutex_unlock(system->lock);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Cascade Detection
 * ========================================================================== */

nimcp_result_t nimcp_cascade_record_failure(
    nimcp_cascade_system_t *system,
    const nimcp_failure_event_t *event
) {
    if (!system || !event) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    /* Add to cascade detection history */
    nimcp_cascade_detection_t *cascade = &system->cascade_state;

    if (cascade->num_events < 32) {
        memcpy(&cascade->events[cascade->num_events++], event,
               sizeof(nimcp_failure_event_t));
    } else {
        /* Shift and add */
        memmove(&cascade->events[0], &cascade->events[1],
                31 * sizeof(nimcp_failure_event_t));
        memcpy(&cascade->events[31], event, sizeof(nimcp_failure_event_t));
    }

    system->total_failures++;

    nimcp_platform_mutex_unlock(system->lock);

    LOG_WARN( "Cascade: Recorded failure: node %u, %s -> %s",
              event->node_id,
              nimcp_cascade_health_state_string(event->prev_state),
              nimcp_cascade_health_state_string(event->new_state));

    /* Send failure alert if bio-async enabled */
    if (system->bio_enabled) {
        nimcp_cascade_send_failure_alert(system, event);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_detect_cascade(
    nimcp_cascade_system_t *system,
    nimcp_cascade_detection_t *detection
) {
    if (!system || !detection) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    memcpy(detection, &system->cascade_state, sizeof(nimcp_cascade_detection_t));

    /* Calculate cascade metrics */
    uint64_t now = get_time_us();
    uint64_t window_us = system->config.cascade_window_ms * 1000ULL;

    /* Count failures in window */
    uint32_t recent_failures = 0;
    uint64_t earliest_time = 0;

    for (uint32_t i = 0; i < detection->num_events; i++) {
        if (now - detection->events[i].timestamp_us <= window_us) {
            recent_failures++;
            if (earliest_time == 0 || detection->events[i].timestamp_us < earliest_time) {
                earliest_time = detection->events[i].timestamp_us;
            }
        }
    }

    detection->affected_nodes = recent_failures;

    /* Calculate failure rate */
    if (recent_failures > 0 && earliest_time > 0) {
        double time_span_sec = (now - earliest_time) / 1000000.0;
        if (time_span_sec > 0.001) {
            detection->cascade_rate = recent_failures / time_span_sec;
        }
    }

    /* Simple correlation: multiple failures in short time */
    if (recent_failures >= 3) {
        detection->correlation_score = fmin(recent_failures / 10.0, 1.0);
    }

    /* Detect cascade */
    detection->cascade_detected =
        (detection->cascade_rate >= system->config.cascade_rate_threshold) ||
        (detection->correlation_score >= CASCADE_CORRELATION_THRESHOLD);

    if (detection->cascade_detected) {
        detection->cascade_start_us = earliest_time;
        system->cascades_prevented++;
    }

    nimcp_platform_mutex_unlock(system->lock);

    if (detection->cascade_detected) {
        LOG_ERROR("Cascade: CASCADE DETECTED! %u failures, rate %.2f/sec, correlation %.2f",
                  detection->affected_nodes, detection->cascade_rate,
                  detection->correlation_score);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Recovery
 * ========================================================================== */

nimcp_result_t nimcp_cascade_start_recovery(
    nimcp_cascade_system_t *system,
    uint32_t node_id,
    nimcp_recovery_strategy_t strategy
) {
    if (!system) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    nimcp_recovery_state_t *state = &system->recovery_state;

    memset(state, 0, sizeof(nimcp_recovery_state_t));
    state->node_id = node_id;
    state->phase = RECOVERY_PHASE_ISOLATE;
    state->strategy = strategy;
    state->progress = 0.0;
    state->recovery_start_us = get_time_us();
    state->estimated_completion_us = state->recovery_start_us +
                                    system->config.recovery_timeout_us;

    snprintf(state->status_message, sizeof(state->status_message),
             "Recovery started with %s strategy",
             (strategy == RECOVERY_IMMEDIATE) ? "immediate" :
             (strategy == RECOVERY_GRADUAL) ? "gradual" :
             (strategy == RECOVERY_SUPERVISED) ? "supervised" : "isolated");

    system->health_state = HEALTH_RECOVERING;

    nimcp_platform_mutex_unlock(system->lock);

    LOG_INFO( "Cascade: Started recovery for node %u: %s",
              node_id, state->status_message);

    /* Send recovery message if bio-async enabled */
    if (system->bio_enabled) {
        nimcp_cascade_send_recovery_message(system, state);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_update_recovery(
    nimcp_cascade_system_t *system,
    nimcp_recovery_phase_t phase,
    double progress
) {
    if (!system) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    nimcp_recovery_state_t *state = &system->recovery_state;
    state->phase = phase;
    state->progress = fmin(fmax(progress, 0.0), 1.0);

    const char *phase_name =
        (phase == RECOVERY_PHASE_ISOLATE) ? "Isolating" :
        (phase == RECOVERY_PHASE_DIAGNOSE) ? "Diagnosing" :
        (phase == RECOVERY_PHASE_REPAIR) ? "Repairing" :
        (phase == RECOVERY_PHASE_VERIFY) ? "Verifying" :
        (phase == RECOVERY_PHASE_REINTEGRATE) ? "Reintegrating" :
        "Complete";

    snprintf(state->status_message, sizeof(state->status_message),
             "%s (%.0f%%)", phase_name, state->progress * 100.0);

    nimcp_platform_mutex_unlock(system->lock);

    LOG_DEBUG( "Cascade: Recovery progress: %s", state->status_message);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_verify_health(
    nimcp_cascade_system_t *system,
    uint32_t node_id,
    bool *passed
) {
    if (!system || !passed) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    /* Simple verification: check current health state */
    *passed = (system->health_state == HEALTH_OPTIMAL ||
               system->health_state == HEALTH_RECOVERING);

    /* Check telemetry */
    const nimcp_health_telemetry_t *t = &system->current_telemetry;
    if (t->cpu_usage > 0.9 || t->memory_usage > 0.9 ||
        t->error_rate > 0.1 || t->packet_loss_rate > 0.1) {
        *passed = false;
    }

    system->recovery_state.verification_passed = *passed;

    nimcp_platform_mutex_unlock(system->lock);

    LOG_INFO( "Cascade: Health verification for node %u: %s",
              node_id, *passed ? "PASSED" : "FAILED");

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_complete_recovery(
    nimcp_cascade_system_t *system,
    uint32_t node_id
) {
    if (!system) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    nimcp_recovery_state_t *state = &system->recovery_state;
    state->phase = RECOVERY_PHASE_COMPLETE;
    state->progress = 1.0;
    snprintf(state->status_message, sizeof(state->status_message),
             "Recovery complete");

    system->health_state = HEALTH_OPTIMAL;
    system->successful_recoveries++;

    nimcp_platform_mutex_unlock(system->lock);

    LOG_INFO( "Cascade: Recovery complete for node %u", node_id);

    /* Send recovery completion message */
    if (system->bio_enabled) {
        nimcp_cascade_send_recovery_message(system, state);
        nimcp_cascade_broadcast_health(system);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_get_recovery_state(
    const nimcp_cascade_system_t *system,
    nimcp_recovery_state_t *state
) {
    if (!system || !state) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);
    memcpy(state, &system->recovery_state, sizeof(nimcp_recovery_state_t));
    nimcp_platform_mutex_unlock(system->lock);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Integration
 * ========================================================================== */

nimcp_result_t nimcp_cascade_enable_bio_async(
    nimcp_cascade_system_t *system,
    void *bio_ctx
) {
    if (!system || !bio_ctx) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);
    system->bio_ctx = bio_ctx;
    system->bio_enabled = true;
    nimcp_platform_mutex_unlock(system->lock);

    LOG_INFO( "Cascade: Bio-async integration enabled");
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_broadcast_health(
    nimcp_cascade_system_t *system
) {
    if (!system || !system->bio_enabled) {
        return NIMCP_INVALID_PARAM;
    }

    /* Package health status */
    struct {
        uint32_t node_id;
        nimcp_health_state_t health_state;
        nimcp_health_telemetry_t telemetry;
        uint64_t timestamp_us;
    } health_msg;

    nimcp_platform_mutex_lock(system->lock);
    health_msg.node_id = system->node_id;
    health_msg.health_state = system->health_state;
    memcpy(&health_msg.telemetry, &system->current_telemetry,
           sizeof(nimcp_health_telemetry_t));
    health_msg.timestamp_us = get_time_us();
    nimcp_platform_mutex_unlock(system->lock);

    return send_bio_message(system->bio_ctx, BIOMSG_HEALTH_STATUS,
                           &health_msg, sizeof(health_msg));
}

nimcp_result_t nimcp_cascade_send_failure_alert(
    nimcp_cascade_system_t *system,
    const nimcp_failure_event_t *event
) {
    if (!system || !event || !system->bio_enabled) {
        return NIMCP_INVALID_PARAM;
    }

    return send_bio_message(system->bio_ctx, BIOMSG_FAILURE_ALERT,
                           event, sizeof(nimcp_failure_event_t));
}

nimcp_result_t nimcp_cascade_send_recovery_message(
    nimcp_cascade_system_t *system,
    const nimcp_recovery_state_t *state
) {
    if (!system || !state || !system->bio_enabled) {
        return NIMCP_INVALID_PARAM;
    }

    return send_bio_message(system->bio_ctx, BIOMSG_RECOVERY_STATUS,
                           state, sizeof(nimcp_recovery_state_t));
}

/* ============================================================================
 * Statistics and Monitoring
 * ========================================================================== */

nimcp_result_t nimcp_cascade_get_statistics(
    const nimcp_cascade_system_t *system,
    uint64_t *total_failures,
    uint64_t *cascades_prevented,
    uint64_t *successful_recoveries
) {
    if (!system) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    if (total_failures) {
        *total_failures = system->total_failures;
    }
    if (cascades_prevented) {
        *cascades_prevented = system->cascades_prevented;
    }
    if (successful_recoveries) {
        *successful_recoveries = system->successful_recoveries;
    }

    nimcp_platform_mutex_unlock(system->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cascade_get_health_summary(
    const nimcp_cascade_system_t *system,
    char *buffer,
    size_t buffer_size
) {
    if (!system || !buffer || buffer_size == 0) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->lock);

    int written = snprintf(buffer, buffer_size,
        "Cascade Prevention System Health Summary\n"
        "=========================================\n"
        "Health State: %s\n"
        "Node ID: %u\n"
        "Total Failures: %lu\n"
        "Cascades Prevented: %lu\n"
        "Successful Recoveries: %lu\n"
        "Active Breakers: %u\n"
        "Registered Capabilities: %u\n"
        "Redundancy Groups: %u\n"
        "Bio-Async: %s\n",
        nimcp_cascade_health_state_string(system->health_state),
        system->node_id,
        system->total_failures,
        system->cascades_prevented,
        system->successful_recoveries,
        system->num_breakers,
        system->num_capabilities,
        system->num_groups,
        system->bio_enabled ? "Enabled" : "Disabled"
    );

    if (written > 0 && (size_t)written < buffer_size) {
        /* Add telemetry info */
        const nimcp_health_telemetry_t *t = &system->current_telemetry;
        written += snprintf(buffer + written, buffer_size - written,
            "\nCurrent Telemetry:\n"
            "  CPU: %.1f%%\n"
            "  Memory: %.1f%%\n"
            "  Latency: %.1f ms\n"
            "  Packet Loss: %.1f%%\n"
            "  Error Rate: %.1f%%\n",
            t->cpu_usage * 100.0,
            t->memory_usage * 100.0,
            t->network_latency_ms,
            t->packet_loss_rate * 100.0,
            t->error_rate * 100.0
        );
    }

    nimcp_platform_mutex_unlock(system->lock);
    return NIMCP_SUCCESS;
}
