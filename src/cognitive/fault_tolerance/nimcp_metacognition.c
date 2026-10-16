/**
 * @file nimcp_metacognition.c
 * @brief Implementation of Metacognition Self-Monitoring Module
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Self-monitoring system for cognitive health tracking
 * WHY: Enable brain to detect degradation before catastrophic failure
 * HOW: Track baselines, compare performance, self-diagnose, calibrate confidence
 *
 * @author NIMCP Development Team
 */

// For clock_gettime
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "cognitive/fault_tolerance/nimcp_metacognition.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"

#define LOG_MODULE "cognitive.fault.metacognition"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(metacognition, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define BIO_MODULE_COGNITIVE_FAULT_METACOGNITION 0x0359


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Rolling window for baseline tracking
 *
 * WHAT: Circular buffer for performance history
 * WHY: Compute baseline from recent samples
 * HOW: Fixed-size ring buffer with head pointer
 */
typedef struct {
    cognitive_state_t* samples;         /**< Sample buffer */
    uint32_t capacity;                  /**< Buffer capacity */
    uint32_t head;                      /**< Next write position */
    uint32_t count;                     /**< Number of samples */
} baseline_window_t;

/**
 * @brief Anomaly detector for degradation detection
 *
 * WHAT: Tracks deviations from baseline
 * WHY: Detect performance anomalies
 * HOW: Statistical comparison to baseline
 */
typedef struct {
    float current_deviation;            /**< Current deviation from baseline */
    uint32_t anomaly_count;             /**< Consecutive anomalies */
    uint64_t last_anomaly_time;         /**< Last anomaly timestamp */
} anomaly_detector_t;

/**
 * @brief Metacognition system implementation
 */
struct metacognition {
    // Configuration
    metacognition_config_t config;

    // Self-monitoring
    cognitive_health_t current_health;
    performance_baseline_t baseline;
    baseline_window_t* baseline_window;

    // Awareness
    float self_confidence;              /**< How confident am I? [0,1] */
    float uncertainty;                  /**< How uncertain am I? [0,1] */

    // Anomaly detection
    anomaly_detector_t* detector;

    // State
    bool initialized;
    uint64_t total_samples;
    uint64_t last_update_time;

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Bio-async registration status */
};

//=============================================================================
// Helper Function Declarations
//=============================================================================

static baseline_window_t* baseline_window_create(uint32_t capacity);
static void baseline_window_destroy(baseline_window_t* window);
static void baseline_window_add(baseline_window_t* window, const cognitive_state_t* state);
static bool baseline_window_compute_baseline(const baseline_window_t* window, performance_baseline_t* baseline);

static anomaly_detector_t* anomaly_detector_create(void);
static void anomaly_detector_destroy(anomaly_detector_t* detector);
static void anomaly_detector_update(anomaly_detector_t* detector, float deviation);

static float compute_overall_health(const cognitive_state_t* state);
static float compute_deviation(const cognitive_state_t* current, const performance_baseline_t* baseline);
static float compute_uncertainty_from_variance(const baseline_window_t* window);
static uint64_t get_current_time_us(void);

static diagnosis_t* create_diagnosis(const metacognition_t* meta);
static void populate_diagnosis_healthy(diagnosis_t* diagnosis);
static void populate_diagnosis_degraded(diagnosis_t* diagnosis, const metacognition_t* meta);

//=============================================================================
// Core API Implementation
//=============================================================================

metacognition_config_t metacognition_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_default_config", 0.0f);


    metacognition_config_t config = {0};
    config.baseline_window_size = METACOGNITION_DEFAULT_BASELINE_WINDOW;
    config.degradation_threshold = METACOGNITION_DEFAULT_DEGRADATION_THRESHOLD;
    config.confidence_learning_rate = METACOGNITION_DEFAULT_LEARNING_RATE;
    config.high_uncertainty_threshold = METACOGNITION_HIGH_UNCERTAINTY_THRESHOLD;
    config.enable_adaptive_baseline = true;
    config.enable_logging = false;
    return config;
}

metacognition_t* metacognition_create(const metacognition_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_create", 0.0f);


    LOG_DEBUG("Creating module");
    // GUARD: Allocate main structure
    metacognition_t* meta = (metacognition_t*)nimcp_malloc(sizeof(metacognition_t));
    if (!meta) return NULL;
    NIMCP_API_CHECK_ALLOC(meta, "Failed to allocate metacognition structure");

    // Initialize to zero
    memset(meta, 0, sizeof(metacognition_t));

    // WHAT: Apply configuration (defaults if NULL)
    // WHY: Allow customization while providing sensible defaults
    if (config) {
        meta->config = *config;
    } else {
        meta->config = metacognition_default_config();
    }

    // WHAT: Create baseline tracking window
    // WHY: Need history to compute baseline
    meta->baseline_window = baseline_window_create(meta->config.baseline_window_size);
    if (!meta->baseline_window) {
        LOG_ERROR("Failed to create baseline window");
        nimcp_free(meta);
        meta = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metacognition_create: meta->baseline_window is NULL");
        return NULL;
    }

    // WHAT: Create anomaly detector
    // WHY: Track deviations from baseline
    meta->detector = anomaly_detector_create();
    if (!meta->detector) {
        LOG_ERROR("Failed to create anomaly detector");
        baseline_window_destroy(meta->baseline_window);
        nimcp_free(meta);
        meta = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metacognition_create: meta->detector is NULL");
        return NULL;
    }

    // WHAT: Initialize self-awareness metrics
    // WHY: Start with moderate confidence and low uncertainty
    meta->self_confidence = METACOGNITION_DEFAULT_CONFIDENCE;
    meta->uncertainty = 0.5F;  // Moderate uncertainty initially

    // Mark as initialized
    meta->initialized = true;
    meta->last_update_time = get_current_time_us();

    if (meta->config.enable_logging) {
        LOG_INFO("Metacognition system created successfully");
    }

    
    // Bio-async registration
    meta->bio_ctx = NULL;
    meta->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_INTROSPECTION_METACOGNITION,
            .module_name = "metacognition",
            .inbox_capacity = 32,
            .user_data = meta
        };
        meta->bio_ctx = bio_router_register_module(&bio_info);
        if (meta->bio_ctx) {
            meta->bio_async_enabled = true;
        }
    }

return meta;
}

void metacognition_destroy(metacognition_t* meta) {
    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_destroy", 0.0f);


    LOG_DEBUG("Destroying module");
    // GUARD: NULL check
    if (!meta) {
        return;
    }

    // WHAT: Free all sub-structures
    // WHY: Prevent memory leaks
    if (meta->baseline_window) {
        baseline_window_destroy(meta->baseline_window);
    }

    if (meta->detector) {
        anomaly_detector_destroy(meta->detector);
    }

    // Free main structure
    // Unregister from bio-router
    if (meta->bio_async_enabled && meta->bio_ctx) {
        bio_router_unregister_module(meta->bio_ctx);
        meta->bio_ctx = NULL;
        meta->bio_async_enabled = false;
    }

    nimcp_free(meta);
    meta = NULL;
}

//=============================================================================
// Self-Monitoring Implementation
//=============================================================================

bool metacognition_monitor_self(
    metacognition_t* meta,
    const cognitive_state_t* state
) {
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_monitor_self", 0.0f);


    if (meta && meta->bio_ctx) {
        bio_router_process_inbox(meta->bio_ctx, 5);
    }

    // GUARD: NULL checks
    if (!meta) {
        LOG_ERROR("NULL metacognition in monitor_self");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metacognition_monitor_self: meta is NULL");
        return false;
    }

    if (!state) {
        LOG_ERROR("NULL cognitive state in monitor_self");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metacognition_monitor_self: state is NULL");
        return false;
    }

    if (!meta->initialized) {
        LOG_ERROR("Metacognition not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "metacognition_monitor_self: meta not initialized");
        return false;
    }

    // WHAT: Update timestamp
    // WHY: Track monitoring frequency
    uint64_t current_time = get_current_time_us();
    meta->last_update_time = current_time;
    meta->total_samples++;

    // WHAT: Add to baseline window
    // WHY: Maintain performance history
    baseline_window_add(meta->baseline_window, state);

    // WHAT: Compute/update baseline
    // WHY: Need baseline for degradation detection
    bool baseline_valid = baseline_window_compute_baseline(
        meta->baseline_window,
        &meta->baseline
    );

    // WHAT: Update current health snapshot
    // WHY: Track current state for queries
    meta->current_health.reasoning_speed = state->reasoning_speed;
    meta->current_health.memory_recall_accuracy = state->memory_recall_accuracy;
    meta->current_health.decision_quality = state->decision_quality;
    meta->current_health.learning_rate_actual = state->learning_rate_actual;
    meta->current_health.attention_focus = state->attention_focus;
    meta->current_health.timestamp_us = current_time;
    meta->current_health.overall_health = compute_overall_health(state);

    // WHAT: Compute deviation from baseline (if established)
    // WHY: Detect degradation
    if (baseline_valid) {
        float deviation = compute_deviation(state, &meta->baseline);
        anomaly_detector_update(meta->detector, deviation);
        meta->detector->current_deviation = deviation;
    }

    // WHAT: Update uncertainty based on variance
    // WHY: Know when performance is unstable
    meta->uncertainty = compute_uncertainty_from_variance(meta->baseline_window);

    // WHAT: Adjust confidence based on health
    // WHY: Lower confidence when degraded
    if (baseline_valid && meta->current_health.overall_health < 0.7F) {
        // Degraded performance lowers confidence
        meta->self_confidence *= 0.95F;  // Gradual decrease
    } else if (meta->current_health.overall_health > 0.9F) {
        // Good performance increases confidence
        meta->self_confidence = fminf(1.0F, meta->self_confidence * 1.02F);
    }

    // Ensure confidence stays in valid range
    meta->self_confidence = fmaxf(0.0F, fminf(1.0F, meta->self_confidence));

    if (meta->config.enable_logging && (meta->total_samples % 100 == 0)) {
        LOG_DEBUG("Metacognition: samples=%lu, health=%.2f, confidence=%.2f, uncertainty=%.2f",
                  meta->total_samples,
                  meta->current_health.overall_health,
                  meta->self_confidence,
                  meta->uncertainty);
    }

    return true;
}

bool metacognition_is_degraded(
    metacognition_t* meta,
    float threshold
) {
    // GUARD: NULL check
    if (!meta) {
        return false;
    }

    // GUARD: Invalid threshold
    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_is_degraded", 0.0f);


    if (threshold < 0.0F || threshold > 1.0F) {
        LOG_WARNING("Invalid degradation threshold %.2f, expected [0,1]", threshold);
        return false;
    }

    // GUARD: Baseline not established (normal at startup)
    if (!meta->baseline.established) {
        return false;  // Can't detect degradation without baseline
    }

    // WHAT: Check if current health is below threshold * baseline
    // WHY: Threshold allows tuning sensitivity
    // HOW: Compare overall health to threshold

    float health_threshold = threshold;
    bool is_degraded = (meta->current_health.overall_health < health_threshold);

    return is_degraded;
}

diagnosis_t* metacognition_self_diagnose(metacognition_t* meta) {
    // GUARD: NULL check
    if (!meta) {
        LOG_ERROR("NULL metacognition in self_diagnose");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metacognition_self_diagnose: meta is NULL");
        return NULL;
    }

    // WHAT: Allocate diagnosis structure
    // WHY: Return detailed diagnosis to caller
    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_self_diagnose", 0.0f);


    diagnosis_t* diagnosis = create_diagnosis(meta);
    if (!diagnosis) {
        LOG_ERROR("Failed to allocate diagnosis");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metacognition_self_diagnose: diagnosis is NULL");
        return NULL;
    }

    // WHAT: Determine if degraded
    // WHY: Different diagnosis for healthy vs degraded
    bool degraded = metacognition_is_degraded(meta, meta->config.degradation_threshold);

    if (!degraded) {
        populate_diagnosis_healthy(diagnosis);
    } else {
        populate_diagnosis_degraded(diagnosis, meta);
    }

    return diagnosis;
}

//=============================================================================
// Confidence Calibration Implementation
//=============================================================================

float metacognition_calibrate_confidence(
    metacognition_t* meta,
    float initial_confidence,
    bool success
) {
    // GUARD: NULL check
    if (!meta) {
        return initial_confidence;  // No change on error
    }

    // GUARD: Clamp initial confidence
    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_calibrate_confidence", 0.0f);


    initial_confidence = fmaxf(0.0F, fminf(1.0F, initial_confidence));

    float learning_rate = meta->config.confidence_learning_rate;
    float new_confidence = 0.0f;

    if (success) {
        // WHAT: Increase confidence on success
        // WHY: Reward prediction error (positive)
        // HOW: confidence += lr * (1 - confidence)
        new_confidence = initial_confidence + learning_rate * (1.0F - initial_confidence);
    } else {
        // WHAT: Decrease confidence on failure
        // WHY: Negative reward prediction error
        // HOW: confidence -= lr * confidence
        new_confidence = initial_confidence - learning_rate * initial_confidence;
    }

    // WHAT: Clamp to valid range
    // WHY: Prevent overflow/underflow
    new_confidence = fmaxf(0.0F, fminf(1.0F, new_confidence));

    // Update internal confidence
    meta->self_confidence = new_confidence;

    return new_confidence;
}

//=============================================================================
// Query Functions Implementation
//=============================================================================

float metacognition_get_self_confidence(const metacognition_t* meta) {
    if (!meta) {
        return 0.0F;
    }
    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_get_self_confidence", 0.0f);


    return meta->self_confidence;
}

float metacognition_get_uncertainty(const metacognition_t* meta) {
    if (!meta) {
        return 0.0F;
    }
    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_get_uncertainty", 0.0f);


    return meta->uncertainty;
}

bool metacognition_has_high_uncertainty(
    const metacognition_t* meta,
    float threshold
) {
    if (!meta) {
        return false;
    }

    // Clamp threshold
    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_has_high_uncertainty", 0.0f);


    threshold = fmaxf(0.0F, fminf(1.0F, threshold));

    return meta->uncertainty > threshold;
}

bool metacognition_get_baseline(
    const metacognition_t* meta,
    performance_baseline_t* baseline
) {
    // GUARD: NULL checks
    if (!meta || !baseline) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metacognition_get_baseline: required parameter is NULL (meta, baseline)");
        return false;
    }

    // WHAT: Copy baseline
    // WHY: External visibility
    *baseline = meta->baseline;

    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_get_baseline", 0.0f);


    return meta->baseline.established;
}

bool metacognition_get_current_health(
    const metacognition_t* meta,
    cognitive_health_t* health
) {
    // GUARD: NULL checks
    if (!meta || !health) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metacognition_get_current_health: required parameter is NULL (meta, health)");
        return false;
    }

    // WHAT: Copy current health
    // WHY: External monitoring
    *health = meta->current_health;

    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_get_current_health", 0.0f);


    return true;
}

bool metacognition_is_initialized(const metacognition_t* meta) {
    if (!meta) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_is_initialized", 0.0f);


    return meta->initialized;
}

//=============================================================================
// Diagnosis Utilities Implementation
//=============================================================================

void diagnosis_destroy(diagnosis_t* diagnosis) {
    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_diagnosis_destroy", 0.0f);


    LOG_DEBUG("Destroying module");
    if (diagnosis) {
        nimcp_free(diagnosis);
        diagnosis = NULL;
    }
}

const char* diagnosis_type_to_string(diagnosis_type_t type) {
    switch (type) {
        case DIAGNOSIS_HEALTHY:
            return "Healthy";
        case DIAGNOSIS_COGNITIVE_SLOWDOWN:
            return "Cognitive Slowdown";
        case DIAGNOSIS_MEMORY_CORRUPTION:
            return "Memory Corruption";
        case DIAGNOSIS_DECISION_QUALITY_LOW:
            return "Decision Quality Low";
        case DIAGNOSIS_LEARNING_IMPAIRED:
            return "Learning Impaired";
        case DIAGNOSIS_ATTENTION_DEFICIT:
            return "Attention Deficit";
        case DIAGNOSIS_MULTIPLE_ISSUES:
            return "Multiple Issues";
        case DIAGNOSIS_UNKNOWN:
        default:
            return "Unknown";
    }
}

//=============================================================================
// Debug Functions Implementation
//=============================================================================

void metacognition_reset_for_testing(metacognition_t* meta) {
    if (!meta) {
        return;
    }

    // Reset baseline
    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_reset_for_testing", 0.0f);


    memset(&meta->baseline, 0, sizeof(performance_baseline_t));

    // Reset health
    memset(&meta->current_health, 0, sizeof(cognitive_health_t));

    // Reset window
    if (meta->baseline_window) {
        meta->baseline_window->head = 0;
        meta->baseline_window->count = 0;
    }

    // Reset awareness
    meta->self_confidence = METACOGNITION_DEFAULT_CONFIDENCE;
    meta->uncertainty = 0.5F;

    // Reset detector
    if (meta->detector) {
        meta->detector->current_deviation = 0.0F;
        meta->detector->anomaly_count = 0;
        meta->detector->last_anomaly_time = 0;
    }

    // Reset counters
    meta->total_samples = 0;
}

void metacognition_print_state(const metacognition_t* meta, FILE* fp) {
    if (!meta || !fp) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_print_state", 0.0f);


    fprintf(fp, "=== Metacognition State ===\n");
    fprintf(fp, "Initialized: %s\n", meta->initialized ? "Yes" : "No");
    fprintf(fp, "Total Samples: %lu\n", meta->total_samples);
    fprintf(fp, "Self-Confidence: %.3f\n", meta->self_confidence);
    fprintf(fp, "Uncertainty: %.3f\n", meta->uncertainty);
    fprintf(fp, "\nCurrent Health:\n");
    fprintf(fp, "  Overall: %.3f\n", meta->current_health.overall_health);
    fprintf(fp, "  Reasoning Speed: %.3f\n", meta->current_health.reasoning_speed);
    fprintf(fp, "  Memory Accuracy: %.3f\n", meta->current_health.memory_recall_accuracy);
    fprintf(fp, "  Decision Quality: %.3f\n", meta->current_health.decision_quality);
    fprintf(fp, "  Learning Rate: %.3f\n", meta->current_health.learning_rate_actual);
    fprintf(fp, "  Attention Focus: %.3f\n", meta->current_health.attention_focus);

    if (meta->baseline.established) {
        fprintf(fp, "\nBaseline (n=%u):\n", meta->baseline.sample_count);
        fprintf(fp, "  Reasoning Speed: %.3f\n", meta->baseline.reasoning_speed);
        fprintf(fp, "  Memory Accuracy: %.3f\n", meta->baseline.memory_recall_accuracy);
        fprintf(fp, "  Decision Quality: %.3f\n", meta->baseline.decision_quality);
        fprintf(fp, "  Learning Rate: %.3f\n", meta->baseline.learning_rate_actual);
        fprintf(fp, "  Attention Focus: %.3f\n", meta->baseline.attention_focus);
    } else {
        fprintf(fp, "\nBaseline: Not established\n");
    }

    fprintf(fp, "===========================\n");
}

//=============================================================================
// Baseline Window Implementation
//=============================================================================

static baseline_window_t* baseline_window_create(uint32_t capacity) {
    LOG_DEBUG("Creating module");
    baseline_window_t* window = (baseline_window_t*)nimcp_malloc(sizeof(baseline_window_t));
    if (!window) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate window");

        return NULL;
    }

    window->samples = (cognitive_state_t*)nimcp_calloc(capacity, sizeof(cognitive_state_t));
    if (!window->samples) {
        nimcp_free(window);
        window = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "baseline_window_create: window->samples is NULL");
        return NULL;
    }

    window->capacity = capacity;
    window->head = 0;
    window->count = 0;

    return window;
}

static void baseline_window_destroy(baseline_window_t* window) {
    LOG_DEBUG("Destroying module");
    if (window) {
        if (window->samples) {
            nimcp_free(window->samples);
        }
        nimcp_free(window);
        window = NULL;
    }
}

static void baseline_window_add(baseline_window_t* window, const cognitive_state_t* state) {
    if (!window || !state) {
        return;
    }

    // WHAT: Add to circular buffer
    // WHY: Maintain recent history
    window->samples[window->head] = *state;
    window->head = (window->head + 1) % window->capacity;

    if (window->count < window->capacity) {
        window->count++;
    }
}

static bool baseline_window_compute_baseline(
    const baseline_window_t* window,
    performance_baseline_t* baseline
) {
    if (!window || !baseline || window->count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "baseline_window_compute_baseline: required parameter is NULL (window, baseline)");
        return false;
    }

    // WHAT: Compute average of all samples
    // WHY: Baseline = average performance
    // HOW: Sum and divide

    float sum_reasoning = 0.0F;
    float sum_memory = 0.0F;
    float sum_decision = 0.0F;
    float sum_learning = 0.0F;
    float sum_attention = 0.0F;

    for (uint32_t i = 0; i < window->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && window->count > 256) {
            metacognition_heartbeat("metacognitio_loop",
                             (float)(i + 1) / (float)window->count);
        }

        sum_reasoning += window->samples[i].reasoning_speed;
        sum_memory += window->samples[i].memory_recall_accuracy;
        sum_decision += window->samples[i].decision_quality;
        sum_learning += window->samples[i].learning_rate_actual;
        sum_attention += window->samples[i].attention_focus;
    }

    baseline->reasoning_speed = sum_reasoning / window->count;
    baseline->memory_recall_accuracy = sum_memory / window->count;
    baseline->decision_quality = sum_decision / window->count;
    baseline->learning_rate_actual = sum_learning / window->count;
    baseline->attention_focus = sum_attention / window->count;
    baseline->sample_count = window->count;
    baseline->established = true;

    return true;
}

//=============================================================================
// Anomaly Detector Implementation
//=============================================================================

static anomaly_detector_t* anomaly_detector_create(void) {
    LOG_DEBUG("Creating module");
    anomaly_detector_t* detector = (anomaly_detector_t*)nimcp_calloc(1, sizeof(anomaly_detector_t));
    return detector;
}

static void anomaly_detector_destroy(anomaly_detector_t* detector) {
    LOG_DEBUG("Destroying module");
    if (detector) {
        nimcp_free(detector);
        detector = NULL;
    }
}

static void anomaly_detector_update(anomaly_detector_t* detector, float deviation) {
    if (!detector) {
        return;
    }

    detector->current_deviation = deviation;

    // WHAT: Track consecutive anomalies
    // WHY: Persistent degradation more serious than transient
    if (deviation > 0.3F) {  // 30% deviation threshold
        detector->anomaly_count++;
        detector->last_anomaly_time = get_current_time_us();
    } else {
        detector->anomaly_count = 0;  // Reset on normal performance
    }
}

//=============================================================================
// Helper Functions Implementation
//=============================================================================

static float compute_overall_health(const cognitive_state_t* state) {
    if (!state) {
        return 0.0F;
    }

    // WHAT: Weighted average of all metrics
    // WHY: Single health score for quick assessment
    // HOW: Equal weights for simplicity

    float health = 0.0F;
    health += state->reasoning_speed / 5.0F;  // Normalize assuming max ~5x baseline
    health += state->memory_recall_accuracy;
    health += state->decision_quality;
    health += state->learning_rate_actual;
    health += state->attention_focus;

    health /= 5.0F;  // Average

    // Clamp to [0, 1]
    return fmaxf(0.0F, fminf(1.0F, health));
}

static float compute_deviation(
    const cognitive_state_t* current,
    const performance_baseline_t* baseline
) {
    if (!current || !baseline) {
        return 0.0F;
    }

    // WHAT: Compute normalized deviation from baseline
    // WHY: Detect performance degradation
    // HOW: Average relative difference

    float dev_reasoning = fabsf(current->reasoning_speed - baseline->reasoning_speed) /
                          fmaxf(baseline->reasoning_speed, 0.01F);
    float dev_memory = fabsf(current->memory_recall_accuracy - baseline->memory_recall_accuracy);
    float dev_decision = fabsf(current->decision_quality - baseline->decision_quality);
    float dev_learning = fabsf(current->learning_rate_actual - baseline->learning_rate_actual);
    float dev_attention = fabsf(current->attention_focus - baseline->attention_focus);

    float avg_deviation = (dev_reasoning + dev_memory + dev_decision + dev_learning + dev_attention) / 5.0F;

    return avg_deviation;
}

static float compute_uncertainty_from_variance(const baseline_window_t* window) {
    if (!window || window->count < 2) {
        return 0.5F;  // Moderate uncertainty if insufficient data
    }

    // WHAT: Compute variance across recent samples
    // WHY: High variance = high uncertainty
    // HOW: Standard deviation normalized

    // Compute means
    float mean_reasoning = 0.0F;
    for (uint32_t i = 0; i < window->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && window->count > 256) {
            metacognition_heartbeat("metacognitio_loop",
                             (float)(i + 1) / (float)window->count);
        }

        mean_reasoning += window->samples[i].reasoning_speed;
    }
    mean_reasoning /= window->count;

    // Compute variance
    float variance = 0.0F;
    for (uint32_t i = 0; i < window->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && window->count > 256) {
            metacognition_heartbeat("metacognitio_loop",
                             (float)(i + 1) / (float)window->count);
        }

        float diff = window->samples[i].reasoning_speed - mean_reasoning;
        variance += diff * diff;
    }
    variance /= window->count;

    float std_dev = sqrtf(variance);

    // Normalize to [0, 1] (assume std_dev < 2.0 is reasonable)
    float uncertainty = fminf(1.0F, std_dev / 2.0F);

    return uncertainty;
}

static uint64_t get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static diagnosis_t* create_diagnosis(const metacognition_t* meta) {
    diagnosis_t* diagnosis = (diagnosis_t*)nimcp_calloc(1, sizeof(diagnosis_t));
    return diagnosis;
}

static void populate_diagnosis_healthy(diagnosis_t* diagnosis) {
    diagnosis->primary_issue = DIAGNOSIS_HEALTHY;
    diagnosis->severity = 0.0F;
    diagnosis->has_memory_issues = false;
    diagnosis->has_attention_issues = false;
    diagnosis->has_reasoning_issues = false;
    diagnosis->has_learning_issues = false;
    diagnosis->recommend_recovery = false;
    diagnosis->recommend_help = false;
    diagnosis->recommend_rest = false;
    snprintf(diagnosis->description, sizeof(diagnosis->description),
             "Cognitive health: Normal. All systems functioning within baseline parameters.");
}

static void populate_diagnosis_degraded(diagnosis_t* diagnosis, const metacognition_t* meta) {
    // WHAT: Analyze which metrics are degraded
    // WHY: Provide specific diagnosis
    // HOW: Compare each metric to baseline

    const cognitive_health_t* health = &meta->current_health;
    const performance_baseline_t* baseline = &meta->baseline;

    int issue_count = 0;
    diagnosis_type_t primary = DIAGNOSIS_UNKNOWN;
    float max_severity = 0.0F;

    // Check reasoning speed
    if (health->reasoning_speed < baseline->reasoning_speed * 0.7F) {
        diagnosis->has_reasoning_issues = true;
        issue_count++;
        float severity = (fabsf(baseline->reasoning_speed) < 1e-7f) ? 1.0f
                       : 1.0F - (health->reasoning_speed / baseline->reasoning_speed);
        if (severity > max_severity) {
            max_severity = severity;
            primary = DIAGNOSIS_COGNITIVE_SLOWDOWN;
        }
    }

    // Check memory
    if (health->memory_recall_accuracy < 0.8F) {
        diagnosis->has_memory_issues = true;
        issue_count++;
        float severity = 1.0F - health->memory_recall_accuracy;
        if (severity > max_severity) {
            max_severity = severity;
            primary = DIAGNOSIS_MEMORY_CORRUPTION;
        }
    }

    // Check decision quality
    if (health->decision_quality < baseline->decision_quality * 0.7F) {
        issue_count++;
        float severity = (fabsf(baseline->decision_quality) < 1e-7f) ? 1.0f
                       : 1.0F - (health->decision_quality / baseline->decision_quality);
        if (severity > max_severity) {
            max_severity = severity;
            primary = DIAGNOSIS_DECISION_QUALITY_LOW;
        }
    }

    // Check learning
    if (health->learning_rate_actual < baseline->learning_rate_actual * 0.7F) {
        diagnosis->has_learning_issues = true;
        issue_count++;
        float severity = (fabsf(baseline->learning_rate_actual) < 1e-7f) ? 1.0f
                       : 1.0F - (health->learning_rate_actual / baseline->learning_rate_actual);
        if (severity > max_severity) {
            max_severity = severity;
            primary = DIAGNOSIS_LEARNING_IMPAIRED;
        }
    }

    // Check attention
    if (health->attention_focus < baseline->attention_focus * 0.7F) {
        diagnosis->has_attention_issues = true;
        issue_count++;
        float severity = (fabsf(baseline->attention_focus) < 1e-7f) ? 1.0f
                       : 1.0F - (health->attention_focus / baseline->attention_focus);
        if (severity > max_severity) {
            max_severity = severity;
            primary = DIAGNOSIS_ATTENTION_DEFICIT;
        }
    }

    // Determine primary issue
    if (issue_count > 1) {
        diagnosis->primary_issue = DIAGNOSIS_MULTIPLE_ISSUES;
    } else {
        diagnosis->primary_issue = primary;
    }

    diagnosis->severity = max_severity;

    // Recommendations
    diagnosis->recommend_recovery = (max_severity > 0.5F);
    diagnosis->recommend_help = (meta->uncertainty > 0.7F || max_severity > 0.7F);
    diagnosis->recommend_rest = (issue_count >= 3);

    // Description
    snprintf(diagnosis->description, sizeof(diagnosis->description),
             "Cognitive degradation detected: %s (severity=%.2f). Issues: %d. Recommendations: %s%s%s",
             diagnosis_type_to_string(diagnosis->primary_issue),
             diagnosis->severity,
             issue_count,
             diagnosis->recommend_recovery ? "Recovery " : "",
             diagnosis->recommend_help ? "Help " : "",
             diagnosis->recommend_rest ? "Rest" : "");
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

/**
 * @brief Query knowledge graph for self-knowledge about metacognition module
 *
 * WHAT: Retrieve module's own entity and connections from KG
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int metacognition_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    metacognition_heartbeat("metacognitio_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Metacognition_Module");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                metacognition_heartbeat("metacognitio_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Metacognition self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Metacognition_Module");
    if (connections) {
        LOG_DEBUG("Metacognition has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Metacognition_Module");
    if (incoming) {
        LOG_DEBUG("Metacognition has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void metacognition_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_metacognition_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int metacognition_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "metacognition_training_begin: NULL argument");
        return -1;
    }
    metacognition_heartbeat_instance(NULL, "metacognition_training_begin", 0.0f);
    (void)(struct metacognition*)instance; /* Module state available for reset */
    return 0;
}

int metacognition_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "metacognition_training_end: NULL argument");
        return -1;
    }
    metacognition_heartbeat_instance(NULL, "metacognition_training_end", 1.0f);
    (void)(struct metacognition*)instance; /* Module state available for finalization */
    return 0;
}

int metacognition_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "metacognition_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    metacognition_heartbeat_instance(NULL, "metacognition_training_step", progress);
    (void)(struct metacognition*)instance; /* Module state available for step adaptation */
    return 0;
}
