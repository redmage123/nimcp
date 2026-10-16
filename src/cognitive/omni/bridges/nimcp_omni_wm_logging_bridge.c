/**
 * @file nimcp_omni_wm_logging_bridge.c
 * @brief World Model Logging Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with logging/audit systems
 * WHY:  Enable comprehensive audit trails, debugging, and performance monitoring
 * HOW:  Capture predictions, training updates, anomalies, and operational metrics
 *
 * IMPLEMENTATION NOTES:
 * =====================
 * This implementation provides:
 *
 * 1. PREDICTION LOGGING:
 *    - Input/output state summaries with truncation for storage
 *    - Confidence and prediction error tracking
 *    - Sampling support for high-frequency predictions
 *
 * 2. TRAINING LOGGING:
 *    - Loss and gradient metrics
 *    - Learning rate and weight statistics
 *    - Batch and replay buffer tracking
 *
 * 3. ANOMALY DETECTION:
 *    - Multiple anomaly types (NaN, divergence, gradient issues)
 *    - Severity classification
 *    - Resolution tracking
 *
 * 4. CALIBRATION TRACKING:
 *    - Confidence vs accuracy correlation
 *    - Calibration error computation
 *    - Histogram of confidence bins
 */

#include "cognitive/omni/bridges/nimcp_omni_wm_logging_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>

/* ============================================================================
 * Module-level Constants
 * ============================================================================ */

#define LOG_MODULE "wm_logging_bridge"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(omni_wm_logging_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

void omni_wm_logging_bridge_set_instance_health_agent(
    omni_wm_logging_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "omni_wm_logging_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int omni_wm_logging_bridge_training_begin(omni_wm_logging_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_logging_bridge_training_begin: NULL argument");
        return -1;
    }
    omni_wm_logging_bridge_heartbeat_instance(g_omni_wm_logging_bridge_health_agent, "training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int omni_wm_logging_bridge_training_step(omni_wm_logging_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_logging_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_wm_logging_bridge_heartbeat_instance(g_omni_wm_logging_bridge_health_agent, "training_step", progress);
    (void)bridge;
    return 0;
}

int omni_wm_logging_bridge_training_end(omni_wm_logging_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_logging_bridge_training_end: NULL argument");
        return -1;
    }
    omni_wm_logging_bridge_heartbeat_instance(g_omni_wm_logging_bridge_health_agent, "training_end", 1.0f);
    (void)bridge;
    return 0;
}

/** Default buffer capacities */
#define DEFAULT_LOG_BUFFER_CAPACITY     256
#define DEFAULT_PRED_BUFFER_CAPACITY    64
#define DEFAULT_TRAIN_BUFFER_CAPACITY   32

/** EMA decay factor for metrics */
#define EMA_DECAY_FACTOR 0.95f

/* ============================================================================
 * Internal Helper Forward Declarations
 * ============================================================================ */

static nimcp_error_t allocate_buffers(omni_wm_logging_bridge_t* bridge);
static void free_buffers(omni_wm_logging_bridge_t* bridge);
static uint64_t get_current_time_ns(void);
static uint64_t get_current_time_us(void);
static float compute_vector_norm(const float* vec, uint32_t dim);
static void summarize_vector(const float* src, uint32_t src_dim,
                             float* dst, uint32_t dst_dim);
static nimcp_error_t write_entry_to_outputs(omni_wm_logging_bridge_t* bridge,
                                             const wm_general_log_entry_t* entry);
static nimcp_error_t flush_prediction_buffer(omni_wm_logging_bridge_t* bridge);
static nimcp_error_t flush_training_buffer(omni_wm_logging_bridge_t* bridge);
static bool should_sample_prediction(omni_wm_logging_bridge_t* bridge);
static nimcp_error_t update_effects(omni_wm_logging_bridge_t* bridge);

/* Bio-async handlers */
static nimcp_error_t handle_log_prediction(const void* msg, size_t msg_size,
                                            nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_log_training(const void* msg, size_t msg_size,
                                          nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_log_anomaly(const void* msg, size_t msg_size,
                                         nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_flush(const void* msg, size_t msg_size,
                                   nimcp_bio_promise_t promise, void* user_data);

/* ============================================================================
 * String Tables
 * ============================================================================ */

static const char* s_category_names[] = {
    "PREDICTION",
    "TRAINING",
    "ANOMALY",
    "CONFIDENCE",
    "REPLAY",
    "ROLLOUT",
    "DREAMING",
    "SYSTEM"
};

static const char* s_severity_names[] = {
    "TRACE",
    "DEBUG",
    "INFO",
    "NOTICE",
    "WARNING",
    "ERROR",
    "CRITICAL"
};

static const char* s_anomaly_names[] = {
    "NONE",
    "HIGH_PE",
    "DIVERGENCE",
    "NAN_INF",
    "GRADIENT_EXPLODE",
    "GRADIENT_VANISH",
    "CONFIDENCE_DROP",
    "REPLAY_CORRUPT",
    "STATE_OOB",
    "CUSTOM"
};

static const char* s_msg_type_names[] = {
    "LOG_PREDICTION",
    "LOG_PREDICTION_ERROR",
    "LOG_PREDICTION_BATCH",
    "LOG_COUNTERFACTUAL",
    "LOG_TRAINING",
    "LOG_TRAINING_BATCH",
    "LOG_LEARNING_RATE",
    "LOG_WEIGHT_UPDATE",
    "LOG_ANOMALY",
    "LOG_ANOMALY_RESOLVED",
    "LOG_DIVERGENCE",
    "LOG_INSTABILITY",
    "LOG_CONFIDENCE",
    "LOG_CALIBRATION",
    "LOG_UNCERTAINTY",
    "LOG_REPLAY_ADD",
    "LOG_REPLAY_SAMPLE",
    "LOG_REPLAY_CLEAR",
    "LOG_DREAM_EPISODE",
    "LOG_BRIDGE_STATUS",
    "LOG_BRIDGE_ERROR",
    "LOG_STATS_UPDATE",
    "LOG_FLUSH"
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in nanoseconds
 */
static uint64_t get_current_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000;
}

/**
 * @brief Compute L2 norm of a vector
 */
static float compute_vector_norm(const float* vec, uint32_t dim) {
    if (!vec || dim == 0) return 0.0f;

    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            omni_wm_logging_bridge_heartbeat("omni_wm_logg_loop",
                             (float)(i + 1) / (float)dim);
        }

        sum_sq += vec[i] * vec[i];
    }
    return sqrtf(sum_sq);
}

/**
 * @brief Summarize a vector by taking first N elements
 */
static void summarize_vector(const float* src, uint32_t src_dim,
                             float* dst, uint32_t dst_dim) {
    if (!src || !dst) return;

    uint32_t copy_dim = (src_dim < dst_dim) ? src_dim : dst_dim;
    memcpy(dst, src, copy_dim * sizeof(float));

    /* Zero remaining elements if dst is larger */
    if (copy_dim < dst_dim) {
        memset(dst + copy_dim, 0, (dst_dim - copy_dim) * sizeof(float));
    }
}

/**
 * @brief Allocate internal buffers
 */
static nimcp_error_t allocate_buffers(omni_wm_logging_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Allocate general log buffer */
    uint32_t log_capacity = bridge->config.buffer_size;
    if (log_capacity == 0) log_capacity = DEFAULT_LOG_BUFFER_CAPACITY;

    bridge->log_buffer = nimcp_calloc(log_capacity, sizeof(wm_general_log_entry_t));
    if (!bridge->log_buffer) return -1;
    NIMCP_CHECK_THROW(bridge->log_buffer, NIMCP_ERROR_NO_MEMORY, "failed to allocate log_buffer");
    bridge->buffer_capacity = log_capacity;
    bridge->buffer_head = 0;
    bridge->buffer_tail = 0;
    bridge->buffer_count = 0;

    /* Allocate prediction buffer */
    uint32_t pred_capacity = DEFAULT_PRED_BUFFER_CAPACITY;
    bridge->pred_buffer = nimcp_calloc(pred_capacity, sizeof(wm_prediction_log_entry_t));
    if (!bridge->pred_buffer) {
        nimcp_free(bridge->log_buffer);
        bridge->log_buffer = NULL;
        return NIMCP_ERROR_NO_MEMORY;
    }
    bridge->pred_buffer_capacity = pred_capacity;
    bridge->pred_buffer_count = 0;

    /* Allocate training buffer */
    uint32_t train_capacity = DEFAULT_TRAIN_BUFFER_CAPACITY;
    bridge->train_buffer = nimcp_calloc(train_capacity, sizeof(wm_training_log_entry_t));
    if (!bridge->train_buffer) {
        nimcp_free(bridge->log_buffer);
        nimcp_free(bridge->pred_buffer);
        bridge->log_buffer = NULL;
        bridge->pred_buffer = NULL;
        return NIMCP_ERROR_NO_MEMORY;
    }
    bridge->train_buffer_capacity = train_capacity;
    bridge->train_buffer_count = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Free internal buffers
 */
static void free_buffers(omni_wm_logging_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->log_buffer);
    bridge->log_buffer = NULL;
    bridge->buffer_capacity = 0;
    bridge->buffer_count = 0;

    nimcp_free(bridge->pred_buffer);
    bridge->pred_buffer = NULL;
    bridge->pred_buffer_capacity = 0;
    bridge->pred_buffer_count = 0;

    nimcp_free(bridge->train_buffer);
    bridge->train_buffer = NULL;
    bridge->train_buffer_capacity = 0;
    bridge->train_buffer_count = 0;
}

/**
 * @brief Determine if prediction should be sampled (for rate limiting)
 */
static bool should_sample_prediction(omni_wm_logging_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    if (bridge->config.prediction_sample_rate >= 1.0f) return true;
    if (bridge->config.prediction_sample_rate <= 0.0f) {
        return false;
    }

    /* Simple deterministic sampling based on sequence number */
    float threshold = bridge->config.prediction_sample_rate;
    float hash = (float)(bridge->sequence_number % 1000) / 1000.0f;
    return hash < threshold;
}

/**
 * @brief Write a log entry to configured outputs
 */
static nimcp_error_t write_entry_to_outputs(omni_wm_logging_bridge_t* bridge,
                                             const wm_general_log_entry_t* entry) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(entry, NIMCP_ERROR_NULL_POINTER, "entry is NULL");

    /* Map WM severity to NIMCP log level */
    log_level_t level = LOG_LEVEL_INFO;
    switch (entry->severity) {
        case WM_LOG_SEV_TRACE:    level = LOG_LEVEL_TRACE; break;
        case WM_LOG_SEV_DEBUG:    level = LOG_LEVEL_DEBUG; break;
        case WM_LOG_SEV_INFO:     level = LOG_LEVEL_INFO; break;
        case WM_LOG_SEV_NOTICE:   level = LOG_LEVEL_INFO; break;
        case WM_LOG_SEV_WARNING:  level = LOG_LEVEL_WARN; break;
        case WM_LOG_SEV_ERROR:    level = LOG_LEVEL_ERROR; break;
        case WM_LOG_SEV_CRITICAL: level = LOG_LEVEL_FATAL; break;
        default: break;
    }

    /* Write to console if enabled */
    if (bridge->config.log_to_console) {
        nimcp_log_write(NULL, level, LOG_MODULE, __FILE__, __LINE__,
                        "[%s][%s] %s",
                        wm_log_category_to_string(entry->category),
                        wm_log_severity_to_string(entry->severity),
                        entry->message);
        bridge->stats.entries_to_console++;
    }

    /* Write to NIMCP logger if connected */
    if (bridge->config.log_to_file && bridge->logger) {
        nimcp_log_write(bridge->logger, level, entry->source, NULL, 0,
                        "%s", entry->message);
        bridge->stats.entries_to_file++;
    }

    /* Write to audit log if connected and enabled */
    if (bridge->config.log_to_audit && bridge->audit_log) {
        /* Audit logging would be called here */
        bridge->stats.entries_to_audit++;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Flush prediction buffer to outputs
 */
static nimcp_error_t flush_prediction_buffer(omni_wm_logging_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->pred_buffer_count == 0) return NIMCP_SUCCESS;

    /* Create general log entries from prediction entries */
    for (uint32_t i = 0; i < bridge->pred_buffer_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->pred_buffer_count > 256) {
            omni_wm_logging_bridge_heartbeat("omni_wm_logg_loop",
                             (float)(i + 1) / (float)bridge->pred_buffer_count);
        }

        wm_prediction_log_entry_t* pred = &bridge->pred_buffer[i];
        wm_general_log_entry_t entry;
        memset(&entry, 0, sizeof(entry));

        entry.entry_id = pred->entry_id;
        entry.timestamp_ns = pred->timestamp_ns;
        entry.category = WM_LOG_CAT_PREDICTION;
        entry.severity = WM_LOG_SEV_DEBUG;
        snprintf(entry.source, sizeof(entry.source), "world_model");
        snprintf(entry.message, sizeof(entry.message),
                 "Prediction: conf=%.3f, pe=%.4f, horizon=%u, cf=%s",
                 pred->confidence, pred->prediction_error,
                 pred->horizon, pred->is_counterfactual ? "yes" : "no");

        write_entry_to_outputs(bridge, &entry);
        bridge->stats.predictions_logged++;
    }

    bridge->pred_buffer_count = 0;
    return NIMCP_SUCCESS;
}

/**
 * @brief Flush training buffer to outputs
 */
static nimcp_error_t flush_training_buffer(omni_wm_logging_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->train_buffer_count == 0) return NIMCP_SUCCESS;

    /* Create general log entries from training entries */
    for (uint32_t i = 0; i < bridge->train_buffer_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->train_buffer_count > 256) {
            omni_wm_logging_bridge_heartbeat("omni_wm_logg_loop",
                             (float)(i + 1) / (float)bridge->train_buffer_count);
        }

        wm_training_log_entry_t* train = &bridge->train_buffer[i];
        wm_general_log_entry_t entry;
        memset(&entry, 0, sizeof(entry));

        entry.entry_id = train->entry_id;
        entry.timestamp_ns = train->timestamp_ns;
        entry.category = WM_LOG_CAT_TRAINING;
        entry.severity = WM_LOG_SEV_INFO;
        snprintf(entry.source, sizeof(entry.source), "world_model");
        snprintf(entry.message, sizeof(entry.message),
                 "Training step %u: loss=%.4f, grad=%.4f, lr=%.6f, batch=%u",
                 train->step_number, train->total_loss,
                 train->gradient_norm, train->learning_rate, train->batch_size);

        write_entry_to_outputs(bridge, &entry);
        bridge->stats.training_steps_logged++;
    }

    bridge->train_buffer_count = 0;
    return NIMCP_SUCCESS;
}

/**
 * @brief Update bidirectional effects
 */
static nimcp_error_t update_effects(omni_wm_logging_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    omni_wm_to_logging_effects_t* wm_effects = &bridge->wm_to_logging;

    /* Update pending counts */
    wm_effects->predictions_pending = bridge->pred_buffer_count;
    wm_effects->training_steps_pending = bridge->train_buffer_count;
    wm_effects->anomalies_pending = 0; /* Not buffered currently */

    /* Current metrics */
    wm_effects->current_loss = bridge->training_metrics.loss;
    wm_effects->current_pe = bridge->training_metrics.mean_pe;

    /* Analyze logging feedback */
    logging_to_omni_wm_effects_t* log_effects = &bridge->logging_to_wm;

    /* Simple stability detection based on recent metrics */
    log_effects->training_stable = (bridge->training_metrics.gradient_norm < 10.0f &&
                                    !isnan(bridge->training_metrics.loss) &&
                                    !isinf(bridge->training_metrics.loss));

    /* Compute performance trend */
    if (bridge->stats.training_steps_logged > 10) {
        float loss_ema = bridge->training_metrics.loss_ema;
        float current_loss = bridge->training_metrics.loss;
        log_effects->performance_trend = (loss_ema > current_loss) ? 0.1f : -0.1f;
    } else {
        log_effects->performance_trend = 0.0f;
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

static nimcp_error_t handle_log_prediction(const void* msg, size_t msg_size,
                                            nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");
    omni_wm_logging_bridge_t* bridge = (omni_wm_logging_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.predictions_logged++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_log_training(const void* msg, size_t msg_size,
                                          nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");
    omni_wm_logging_bridge_t* bridge = (omni_wm_logging_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.training_steps_logged++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_log_anomaly(const void* msg, size_t msg_size,
                                         nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");
    omni_wm_logging_bridge_t* bridge = (omni_wm_logging_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.anomalies_logged++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_flush(const void* msg, size_t msg_size,
                                   nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");
    omni_wm_logging_bridge_t* bridge = (omni_wm_logging_bridge_t*)user_data;

    return omni_wm_logging_bridge_flush(bridge);
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_logging_bridge_default_config(
    omni_wm_logging_bridge_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(*config));

    /* General settings */
    config->enable_modulation = true;
    config->sensitivity = 1.0f;

    /* Filtering */
    config->min_severity = WM_LOG_SEV_INFO;
    config->enabled_categories = WM_LOG_CAT_ALL;

    /* Prediction logging */
    config->enable_prediction_logging = true;
    config->prediction_sample_rate = 0.1f;  /* 10% sampling */
    config->log_input_output_vectors = true;
    config->high_pe_threshold = 0.5f;

    /* Training logging */
    config->enable_training_logging = true;
    config->training_log_interval = 10;  /* Every 10 steps */
    config->log_gradient_stats = true;
    config->log_weight_stats = false;

    /* Anomaly logging */
    config->enable_anomaly_logging = true;
    config->anomaly_threshold = 0.8f;
    config->auto_alert_on_critical = true;

    /* Confidence/calibration */
    config->enable_calibration_logging = true;
    config->calibration_update_interval = 100;

    /* Replay logging */
    config->enable_replay_logging = true;
    config->log_dream_episodes = true;

    /* Output settings */
    config->log_to_console = true;
    config->log_to_file = false;
    config->log_to_audit = false;
    config->enable_json_format = false;

    /* Buffering */
    config->buffer_size = WM_LOG_DEFAULT_BUFFER_SIZE;
    config->flush_interval_ms = WM_LOG_DEFAULT_FLUSH_INTERVAL_MS;
    config->batch_size = WM_LOG_DEFAULT_BATCH_SIZE;

    /* Bio-async */
    config->enable_bio_async = true;

    return NIMCP_SUCCESS;
}

omni_wm_logging_bridge_t* omni_wm_logging_bridge_create(
    const omni_wm_logging_bridge_config_t* config) {

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_create", 0.0f);


    omni_wm_logging_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_wm_logging_bridge_t));
    if (!bridge) {
        LOG_ERROR("Failed to allocate WM logging bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "update_effects: bridge is NULL");
        return NULL;
    }

    /* Initialize base */
    if (bridge_base_init(&bridge->base, BIO_MODULE_WM_LOGGING_BRIDGE,
                         "wm_logging_bridge") != 0) {
        LOG_ERROR("Failed to initialize bridge base");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "update_effects: operation failed");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        omni_wm_logging_bridge_default_config(&bridge->config);
    }

    /* Allocate buffers */
    if (allocate_buffers(bridge) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to allocate logging buffers");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "update_effects: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->logging_active = true;
    bridge->next_entry_id = 1;
    bridge->sequence_number = 0;
    bridge->last_flush_time_us = get_current_time_us();

    /* Initialize metrics */
    memset(&bridge->training_metrics, 0, sizeof(bridge->training_metrics));
    memset(&bridge->calibration, 0, sizeof(bridge->calibration));

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    LOG_INFO("WM logging bridge created (buffer=%u)", bridge->config.buffer_size);

    return bridge;
}

void omni_wm_logging_bridge_destroy(omni_wm_logging_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "omni_wm_logging");

    /* Flush any remaining entries */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_destroy", 0.0f);


    omni_wm_logging_bridge_flush(bridge);

    /* Free buffers */
    free_buffers(bridge);

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);
    bridge = NULL;

    LOG_DEBUG("WM logging bridge destroyed");
}

nimcp_error_t omni_wm_logging_bridge_reset(omni_wm_logging_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_reset", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset buffers */
    bridge->buffer_head = 0;
    bridge->buffer_tail = 0;
    bridge->buffer_count = 0;
    bridge->pred_buffer_count = 0;
    bridge->train_buffer_count = 0;

    /* Reset state */
    bridge->next_entry_id = 1;
    bridge->sequence_number = 0;
    bridge->last_flush_time_us = get_current_time_us();

    /* Reset metrics */
    memset(&bridge->training_metrics, 0, sizeof(bridge->training_metrics));
    memset(&bridge->calibration, 0, sizeof(bridge->calibration));

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Reset effects */
    memset(&bridge->wm_to_logging, 0, sizeof(bridge->wm_to_logging));
    memset(&bridge->logging_to_wm, 0, sizeof(bridge->logging_to_wm));

    /* Reset base (unlocked since we already hold the mutex) */
    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_DEBUG("WM logging bridge reset");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_logging_bridge_connect(
    omni_wm_logging_bridge_t* bridge,
    omni_world_model_t* world_model,
    nimcp_logger_t logger,
    nimcp_audit_log_t* audit_log) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_connect", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_INVALID_PARAMETER, "world_model is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->world_model = world_model;
    bridge->logger = logger;
    bridge->audit_log = audit_log;

    /* Update base connection state (unlocked since we already hold the mutex) */
    bridge_base_connect_a_unlocked(&bridge->base, world_model);
    if (logger) {
        bridge_base_connect_b_unlocked(&bridge->base, logger);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    LOG_INFO("WM logging bridge connected (logger=%s, audit=%s)",
             logger ? "yes" : "no", audit_log ? "yes" : "no");

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_logging_bridge_connect_world_model(
    omni_wm_logging_bridge_t* bridge,
    omni_world_model_t* world_model) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_connect_world_model", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_INVALID_PARAMETER, "world_model is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->world_model = world_model;
    bridge_base_connect_a_unlocked(&bridge->base, world_model);
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_logging_bridge_connect_logger(
    omni_wm_logging_bridge_t* bridge,
    nimcp_logger_t logger) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_connect_logger", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->logger = logger;
    if (logger) {
        bridge_base_connect_b_unlocked(&bridge->base, logger);
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_logging_bridge_connect_audit(
    omni_wm_logging_bridge_t* bridge,
    nimcp_audit_log_t* audit_log) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_connect_audit", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->audit_log = audit_log;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

bool omni_wm_logging_bridge_is_connected(const omni_wm_logging_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_is_connected", 0.0f);


    return bridge->world_model != NULL;
}

/* ============================================================================
 * Update API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_logging_bridge_update(
    omni_wm_logging_bridge_t* bridge,
    float dt) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_update", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Validate dt - negative timesteps are invalid */
    if (dt < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "omni_wm_logging_bridge_update: negative dt (%.4f)", dt);
        return NIMCP_ERROR_INVALID_PARAM;
    }
    (void)dt;

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if auto-flush is needed */
    uint64_t current_time = get_current_time_us();
    uint64_t elapsed_ms = (current_time - bridge->last_flush_time_us) / 1000;

    if (elapsed_ms >= bridge->config.flush_interval_ms) {
        flush_prediction_buffer(bridge);
        flush_training_buffer(bridge);
        bridge->last_flush_time_us = current_time;
        bridge->stats.buffer_flushes++;
    }

    /* Update effects */
    update_effects(bridge);

    /* Record update */
    bridge_base_record_update(&bridge->base);
    bridge->stats.total_updates++;

    /* Update timing stats */
    uint64_t elapsed_us = get_current_time_us() - start_time;
    bridge->stats.total_logging_time_ms += (double)elapsed_us / 1000.0;
    bridge->stats.last_update_time_us = current_time;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_logging_bridge_flush(omni_wm_logging_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_flush", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    flush_prediction_buffer(bridge);
    flush_training_buffer(bridge);
    bridge->last_flush_time_us = get_current_time_us();
    bridge->stats.buffer_flushes++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Prediction Logging API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_logging_bridge_log_prediction(
    omni_wm_logging_bridge_t* bridge,
    const float* input,
    uint32_t input_dim,
    const float* output,
    uint32_t output_dim,
    float confidence) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_log_prediction", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_INVALID_PARAM, "log_prediction: input is NULL");
    NIMCP_CHECK_THROW(output, NIMCP_ERROR_INVALID_PARAM, "log_prediction: output is NULL");
    NIMCP_CHECK_THROW(input_dim > 0, NIMCP_ERROR_INVALID_PARAM, "log_prediction: input_dim is zero");
    NIMCP_CHECK_THROW(output_dim > 0, NIMCP_ERROR_INVALID_PARAM, "log_prediction: output_dim is zero");
    if (!bridge->config.enable_prediction_logging) return NIMCP_SUCCESS;
    if (!bridge->logging_active) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check sampling */
    bridge->sequence_number++;
    if (!should_sample_prediction(bridge)) {
        bridge->stats.predictions_sampled_out++;
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_SUCCESS;
    }

    /* Check buffer space */
    if (bridge->pred_buffer_count >= bridge->pred_buffer_capacity) {
        flush_prediction_buffer(bridge);
    }

    /* Create prediction entry */
    wm_prediction_log_entry_t* entry = &bridge->pred_buffer[bridge->pred_buffer_count];
    memset(entry, 0, sizeof(*entry));

    entry->entry_id = bridge->next_entry_id++;
    entry->timestamp_ns = get_current_time_ns();
    entry->sequence_number = bridge->sequence_number;

    /* Summarize input */
    if (input && input_dim > 0 && bridge->config.log_input_output_vectors) {
        summarize_vector(input, input_dim, entry->input_summary, WM_LOG_MAX_VECTOR_DIM);
        entry->input_dim = input_dim;
        entry->input_norm = compute_vector_norm(input, input_dim);
    }

    /* Summarize output */
    if (output && output_dim > 0 && bridge->config.log_input_output_vectors) {
        summarize_vector(output, output_dim, entry->output_summary, WM_LOG_MAX_VECTOR_DIM);
        entry->output_dim = output_dim;
        entry->output_norm = compute_vector_norm(output, output_dim);
    }

    entry->confidence = confidence;
    entry->is_counterfactual = false;

    bridge->pred_buffer_count++;

    /* Update running stats */
    bridge->stats.mean_logged_confidence =
        bridge->stats.mean_logged_confidence * 0.99f + confidence * 0.01f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_logging_bridge_log_prediction_entry(
    omni_wm_logging_bridge_t* bridge,
    const wm_prediction_log_entry_t* entry) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_log_prediction_entry", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(entry, NIMCP_ERROR_NULL_POINTER, "entry is NULL");
    if (!bridge->config.enable_prediction_logging) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->pred_buffer_count >= bridge->pred_buffer_capacity) {
        flush_prediction_buffer(bridge);
    }

    bridge->pred_buffer[bridge->pred_buffer_count++] = *entry;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_logging_bridge_log_prediction_error(
    omni_wm_logging_bridge_t* bridge,
    const float* predicted,
    const float* actual,
    uint32_t dim,
    float error_magnitude) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_log_prediction_error", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_prediction_logging) return NIMCP_SUCCESS;

    (void)predicted;
    (void)actual;
    (void)dim;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update training metrics with prediction error */
    bridge->training_metrics.mean_pe =
        bridge->training_metrics.mean_pe * EMA_DECAY_FACTOR +
        error_magnitude * (1.0f - EMA_DECAY_FACTOR);

    bridge->stats.mean_logged_pe =
        bridge->stats.mean_logged_pe * 0.99f + error_magnitude * 0.01f;

    /* Check for high PE anomaly */
    if (error_magnitude > bridge->config.high_pe_threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return omni_wm_logging_bridge_log_anomaly(bridge, WM_ANOMALY_HIGH_PE,
                                                   "High prediction error detected");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Training Logging API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_logging_bridge_log_training_step(
    omni_wm_logging_bridge_t* bridge,
    float loss,
    const wm_training_metrics_t* metrics) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_log_training_step", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(metrics, NIMCP_ERROR_INVALID_PARAM, "log_training_step: metrics is NULL");
    if (!bridge->config.enable_training_logging) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check logging interval */
    bridge->training_metrics.steps++;
    if (bridge->training_metrics.steps % bridge->config.training_log_interval != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_SUCCESS;
    }

    /* Check buffer space */
    if (bridge->train_buffer_count >= bridge->train_buffer_capacity) {
        flush_training_buffer(bridge);
    }

    /* Create training entry */
    wm_training_log_entry_t* entry = &bridge->train_buffer[bridge->train_buffer_count];
    memset(entry, 0, sizeof(*entry));

    entry->entry_id = bridge->next_entry_id++;
    entry->timestamp_ns = get_current_time_ns();
    entry->step_number = (uint32_t)bridge->training_metrics.steps;
    entry->total_loss = loss;

    if (metrics) {
        entry->dynamics_loss = metrics->dynamics_loss;
        entry->kl_divergence = metrics->kl_loss;
        entry->gradient_norm = metrics->gradient_norm;
        entry->learning_rate = metrics->learning_rate;
    }

    bridge->train_buffer_count++;

    /* Update running metrics */
    bridge->training_metrics.loss = loss;
    bridge->training_metrics.loss_ema =
        bridge->training_metrics.loss_ema * EMA_DECAY_FACTOR +
        loss * (1.0f - EMA_DECAY_FACTOR);

    if (metrics) {
        bridge->training_metrics.gradient_norm = metrics->gradient_norm;
        bridge->training_metrics.learning_rate = metrics->learning_rate;
    }

    bridge->stats.mean_logged_loss =
        bridge->stats.mean_logged_loss * 0.99f + loss * 0.01f;

    /* Check for anomalies */
    if (isnan(loss) || isinf(loss)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return omni_wm_logging_bridge_log_anomaly(bridge, WM_ANOMALY_NAN_INF,
                                                   "NaN/Inf in training loss");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_logging_bridge_log_training_entry(
    omni_wm_logging_bridge_t* bridge,
    const wm_training_log_entry_t* entry) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_log_training_entry", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(entry, NIMCP_ERROR_NULL_POINTER, "entry is NULL");
    if (!bridge->config.enable_training_logging) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->train_buffer_count >= bridge->train_buffer_capacity) {
        flush_training_buffer(bridge);
    }

    bridge->train_buffer[bridge->train_buffer_count++] = *entry;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_logging_bridge_log_lr_change(
    omni_wm_logging_bridge_t* bridge,
    float old_lr,
    float new_lr,
    const char* reason) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_log_lr_change", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    return omni_wm_logging_bridge_logf(bridge, WM_LOG_CAT_TRAINING, WM_LOG_SEV_NOTICE,
                                        "Learning rate changed: %.6f -> %.6f (%s)",
                                        old_lr, new_lr, reason ? reason : "unspecified");
}

/* ============================================================================
 * Anomaly Logging API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_logging_bridge_log_anomaly(
    omni_wm_logging_bridge_t* bridge,
    wm_anomaly_type_t type,
    const char* details) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_log_anomaly", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_anomaly_logging) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Determine severity based on type */
    wm_log_severity_t severity = WM_LOG_SEV_WARNING;
    switch (type) {
        case WM_ANOMALY_NAN_INF:
        case WM_ANOMALY_GRADIENT_EXPLODE:
            severity = WM_LOG_SEV_ERROR;
            break;
        case WM_ANOMALY_HIGH_PE:
        case WM_ANOMALY_DIVERGENCE:
            severity = WM_LOG_SEV_WARNING;
            break;
        case WM_ANOMALY_REPLAY_CORRUPT:
        case WM_ANOMALY_STATE_OOB:
            severity = WM_LOG_SEV_CRITICAL;
            break;
        default:
            break;
    }

    /* Create general log entry for anomaly */
    wm_general_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    entry.entry_id = bridge->next_entry_id++;
    entry.timestamp_ns = get_current_time_ns();
    entry.category = WM_LOG_CAT_ANOMALY;
    entry.severity = severity;
    snprintf(entry.source, sizeof(entry.source), "world_model");
    snprintf(entry.message, sizeof(entry.message), "ANOMALY [%s]: %s",
             wm_anomaly_type_to_string(type), details ? details : "");

    /* Write immediately (anomalies bypass buffer) */
    write_entry_to_outputs(bridge, &entry);

    /* Update stats */
    bridge->stats.anomalies_logged++;
    if ((uint32_t)type < WM_ANOMALY_COUNT) {
        bridge->stats.anomalies_by_type[type]++;
    }
    if (severity >= WM_LOG_SEV_CRITICAL) {
        bridge->stats.critical_anomalies++;
    }

    /* Update effects */
    bridge->wm_to_logging.has_critical_anomaly = (severity >= WM_LOG_SEV_CRITICAL);
    if ((uint32_t)severity > (uint32_t)bridge->wm_to_logging.max_severity) {
        bridge->wm_to_logging.max_severity = severity;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_logging_bridge_log_anomaly_entry(
    omni_wm_logging_bridge_t* bridge,
    const wm_anomaly_log_entry_t* entry) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_log_anomaly_entry", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(entry, NIMCP_ERROR_NULL_POINTER, "entry is NULL");

    return omni_wm_logging_bridge_log_anomaly(bridge, entry->type, entry->details);
}

nimcp_error_t omni_wm_logging_bridge_log_anomaly_resolved(
    omni_wm_logging_bridge_t* bridge,
    uint64_t original_entry_id,
    const char* resolution_details) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_log_anomaly_resolved", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    bridge->stats.auto_resolved_anomalies++;

    return omni_wm_logging_bridge_logf(bridge, WM_LOG_CAT_ANOMALY, WM_LOG_SEV_INFO,
                                        "Anomaly %lu resolved: %s",
                                        (unsigned long)original_entry_id,
                                        resolution_details ? resolution_details : "");
}

/* ============================================================================
 * Confidence/Calibration Logging API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_logging_bridge_log_calibration(
    omni_wm_logging_bridge_t* bridge,
    const wm_calibration_metrics_t* calibration) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_log_calibration", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(calibration, NIMCP_ERROR_NULL_POINTER, "calibration is NULL");
    if (!bridge->config.enable_calibration_logging) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->calibration = *calibration;
    bridge->stats.calibration_updates++;
    bridge->stats.current_calibration_error = calibration->calibration_error;

    nimcp_mutex_unlock(bridge->base.mutex);

    return omni_wm_logging_bridge_logf(bridge, WM_LOG_CAT_CONFIDENCE, WM_LOG_SEV_INFO,
                                        "Calibration updated: ECE=%.4f, mean_conf=%.3f, mean_acc=%.3f",
                                        calibration->calibration_error,
                                        calibration->mean_confidence,
                                        calibration->mean_accuracy);
}

nimcp_error_t omni_wm_logging_bridge_update_calibration(
    omni_wm_logging_bridge_t* bridge,
    float confidence,
    bool was_accurate) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_update_calibration", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    wm_calibration_metrics_t* cal = &bridge->calibration;

    /* Update running statistics */
    float alpha = 0.01f;
    cal->mean_confidence = cal->mean_confidence * (1.0f - alpha) + confidence * alpha;
    cal->mean_accuracy = cal->mean_accuracy * (1.0f - alpha) + (was_accurate ? 1.0f : 0.0f) * alpha;

    /* Update histogram bin */
    int bin = (int)(confidence * 10.0f);
    if (bin < 0) bin = 0;
    if (bin > 9) bin = 9;
    cal->calibration_bins[bin]++;

    cal->total_predictions++;

    /* Compute calibration error periodically */
    if (cal->total_predictions % bridge->config.calibration_update_interval == 0) {
        cal->calibration_error = fabsf(cal->mean_confidence - cal->mean_accuracy);
        cal->overconfidence_rate = (cal->mean_confidence > cal->mean_accuracy) ?
                                    cal->mean_confidence - cal->mean_accuracy : 0.0f;
        cal->underconfidence_rate = (cal->mean_accuracy > cal->mean_confidence) ?
                                     cal->mean_accuracy - cal->mean_confidence : 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Replay Buffer Logging API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_logging_bridge_log_replay_operation(
    omni_wm_logging_bridge_t* bridge,
    const char* operation,
    uint32_t count,
    uint32_t buffer_size) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_log_replay_operation", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_replay_logging) return NIMCP_SUCCESS;

    return omni_wm_logging_bridge_logf(bridge, WM_LOG_CAT_REPLAY, WM_LOG_SEV_DEBUG,
                                        "Replay %s: count=%u, buffer_size=%u",
                                        operation ? operation : "operation",
                                        count, buffer_size);
}

nimcp_error_t omni_wm_logging_bridge_log_dream(
    omni_wm_logging_bridge_t* bridge,
    uint32_t episode_length,
    float episode_reward,
    uint32_t training_updates) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_log_dream", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.log_dream_episodes) return NIMCP_SUCCESS;

    return omni_wm_logging_bridge_logf(bridge, WM_LOG_CAT_DREAMING, WM_LOG_SEV_INFO,
                                        "Dream episode: length=%u, reward=%.3f, updates=%u",
                                        episode_length, episode_reward, training_updates);
}

/* ============================================================================
 * General Logging API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_logging_bridge_log(
    omni_wm_logging_bridge_t* bridge,
    wm_log_category_t category,
    wm_log_severity_t severity,
    const char* message) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_log", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(message, NIMCP_ERROR_INVALID_PARAM, "log: message is NULL");
    if (!bridge->logging_active) return NIMCP_SUCCESS;

    /* Check severity filter */
    if ((uint32_t)severity < (uint32_t)bridge->config.min_severity) {
        return NIMCP_SUCCESS;
    }

    /* Check category filter */
    if (!(bridge->config.enabled_categories & WM_LOG_CAT_MASK(category))) {
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Create entry */
    wm_general_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    entry.entry_id = bridge->next_entry_id++;
    entry.timestamp_ns = get_current_time_ns();
    entry.category = category;
    entry.severity = severity;
    snprintf(entry.source, sizeof(entry.source), "world_model");
    if (message) {
        strncpy(entry.message, message, sizeof(entry.message) - 1);
    }

    /* Write to outputs */
    write_entry_to_outputs(bridge, &entry);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_logging_bridge_logf(
    omni_wm_logging_bridge_t* bridge,
    wm_log_category_t category,
    wm_log_severity_t severity,
    const char* format,
    ...) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_logf", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(format, NIMCP_ERROR_NULL_POINTER, "format is NULL");

    char message[WM_LOG_MAX_MESSAGE_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    return omni_wm_logging_bridge_log(bridge, category, severity, message);
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

const omni_wm_to_logging_effects_t* omni_wm_logging_bridge_get_wm_effects(
    const omni_wm_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_get_wm_effects", 0.0f);


    return &bridge->wm_to_logging;
}

const logging_to_omni_wm_effects_t* omni_wm_logging_bridge_get_logging_effects(
    const omni_wm_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_get_logging_effects", 0.0f);


    return &bridge->logging_to_wm;
}

nimcp_error_t omni_wm_logging_bridge_get_stats(
    const omni_wm_logging_bridge_t* bridge,
    omni_wm_logging_bridge_stats_t* stats) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_get_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_mutex_lock(((omni_wm_logging_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((omni_wm_logging_bridge_t*)bridge)->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_logging_bridge_reset_stats(
    omni_wm_logging_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

const wm_training_metrics_t* omni_wm_logging_bridge_get_training_metrics(
    const omni_wm_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_get_training_metrics", 0.0f);


    return &bridge->training_metrics;
}

const wm_calibration_metrics_t* omni_wm_logging_bridge_get_calibration(
    const omni_wm_logging_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_get_calibration", 0.0f);


    return &bridge->calibration;
}

/* ============================================================================
 * Bio-Async API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_logging_bridge_connect_bio_async(
    omni_wm_logging_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    return bridge_base_connect_bio_async(&bridge->base);
}

nimcp_error_t omni_wm_logging_bridge_disconnect_bio_async(
    omni_wm_logging_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    return bridge_base_disconnect_bio_async(&bridge->base);
}

bool omni_wm_logging_bridge_is_bio_async_connected(
    const omni_wm_logging_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_is_bio_async_connect", 0.0f);


    return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL);
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* omni_wm_logging_msg_type_to_string(omni_wm_logging_msg_type_t msg_type) {
    /* Map message type to string - simplified */
    uint32_t base_idx = 0;

    if (msg_type >= BIO_MSG_WM_LOG_PREDICTION && msg_type <= BIO_MSG_WM_LOG_COUNTERFACTUAL) {
        base_idx = msg_type - BIO_MSG_WM_LOG_PREDICTION;
    } else if (msg_type >= BIO_MSG_WM_LOG_TRAINING && msg_type <= BIO_MSG_WM_LOG_WEIGHT_UPDATE) {
        base_idx = 4 + (msg_type - BIO_MSG_WM_LOG_TRAINING);
    } else if (msg_type >= BIO_MSG_WM_LOG_ANOMALY && msg_type <= BIO_MSG_WM_LOG_INSTABILITY) {
        base_idx = 8 + (msg_type - BIO_MSG_WM_LOG_ANOMALY);
    } else if (msg_type >= BIO_MSG_WM_LOG_CONFIDENCE && msg_type <= BIO_MSG_WM_LOG_UNCERTAINTY) {
        base_idx = 12 + (msg_type - BIO_MSG_WM_LOG_CONFIDENCE);
    } else if (msg_type >= BIO_MSG_WM_LOG_REPLAY_ADD && msg_type <= BIO_MSG_WM_LOG_DREAM_EPISODE) {
        base_idx = 15 + (msg_type - BIO_MSG_WM_LOG_REPLAY_ADD);
    } else if (msg_type >= BIO_MSG_WM_LOG_BRIDGE_STATUS && msg_type <= BIO_MSG_WM_LOG_FLUSH) {
        base_idx = 19 + (msg_type - BIO_MSG_WM_LOG_BRIDGE_STATUS);
    } else {
        return "UNKNOWN";
    }

    if (base_idx < sizeof(s_msg_type_names) / sizeof(s_msg_type_names[0])) {
        return s_msg_type_names[base_idx];
    }
    return "UNKNOWN";
}

const char* wm_log_category_to_string(wm_log_category_t category) {
    if ((uint32_t)category < WM_LOG_CAT_COUNT) {
        return s_category_names[category];
    }
    return "UNKNOWN";
}

const char* wm_log_severity_to_string(wm_log_severity_t severity) {
    if ((uint32_t)severity < WM_LOG_SEV_COUNT) {
        return s_severity_names[severity];
    }
    return "UNKNOWN";
}

const char* wm_anomaly_type_to_string(wm_anomaly_type_t type) {
    if ((uint32_t)type < WM_ANOMALY_COUNT) {
        return s_anomaly_names[type];
    }
    return "UNKNOWN";
}

nimcp_error_t omni_wm_logging_bridge_validate_config(
    const omni_wm_logging_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_logging_bridge_heartbeat("omni_wm_logg_validate_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Validate sensitivity range */
    if (config->sensitivity < 0.5f || config->sensitivity > 2.0f) {
        return NIMCP_ERROR_INVALID_PARAMETER;
    }

    /* Validate sampling rate */
    if (config->prediction_sample_rate < 0.0f || config->prediction_sample_rate > 1.0f) {
        return NIMCP_ERROR_INVALID_PARAMETER;
    }

    /* Validate buffer size */
    if (config->buffer_size == 0 || config->buffer_size > 1000000) {
        return NIMCP_ERROR_INVALID_PARAMETER;
    }

    return NIMCP_SUCCESS;
}
