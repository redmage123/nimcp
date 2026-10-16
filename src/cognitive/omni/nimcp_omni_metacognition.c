/**
 * @file nimcp_omni_metacognition.c
 * @brief Phase 10: Self-Organizing Inference (Metacognition) Implementation
 * @version 1.0.0
 * @date 2026-01-04
 */

#include "cognitive/omni/nimcp_omni_metacognition.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(omni_metacognition, MESH_ADAPTER_CATEGORY_COGNITIVE)

void omni_metacognition_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_omni_metacognition_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int omni_metacognition_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_metacognition_training_begin: NULL argument");
        return -1;
    }
    omni_metacognition_heartbeat_instance(g_omni_metacognition_health_agent, "training_begin", 0.0f);
    (void)ctx;
    return 0;
}

int omni_metacognition_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_metacognition_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_metacognition_heartbeat_instance(g_omni_metacognition_health_agent, "training_step", progress);
    (void)ctx;
    return 0;
}

int omni_metacognition_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_metacognition_training_end: NULL argument");
        return -1;
    }
    omni_metacognition_heartbeat_instance(g_omni_metacognition_health_agent, "training_end", 1.0f);
    (void)ctx;
    return 0;
}


/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

/**
 * @brief Get current time in seconds
 */
static double get_current_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/**
 * @brief Sigmoid function
 */
static float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

/**
 * @brief Softmax over array
 */
static void softmax(const float* input, float* output, uint32_t n) {
    float max_val = input[0];
    for (uint32_t i = 1; i < n; i++) {
        if (input[i] > max_val) max_val = input[i];
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            omni_metacognition_heartbeat("omni_metacog_loop",
                             (float)(i + 1) / (float)n);
        }

        output[i] = expf(input[i] - max_val);
        sum += output[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                omni_metacognition_heartbeat("omni_metacog_loop",
                                 (float)(i + 1) / (float)n);
            }

            output[i] /= sum;
        }
    }
}

/**
 * @brief Compute exponential moving average
 */
static float ema_update(float old_val, float new_val, float alpha) {
    return alpha * new_val + (1.0f - alpha) * old_val;
}

/**
 * @brief Simple hash function for context
 */
static uint32_t simple_hash(const void* data, size_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t hash = 5381;
    for (size_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            omni_metacognition_heartbeat("omni_metacog_loop",
                             (float)(i + 1) / (float)len);
        }

        hash = ((hash << 5) + hash) + bytes[i];
    }
    return hash;
}

/* ============================================================================
 * LIFECYCLE API IMPLEMENTATION
 * ============================================================================ */

nimcp_error_t omni_metacog_get_default_config(omni_metacog_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_get_def", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAMETER, "config is NULL");

    memset(config, 0, sizeof(*config));

    /* Monitoring parameters */
    config->monitoring_frequency = 10.0f;    /* 10 Hz */
    config->coherence_threshold = OMNI_METACOG_DEFAULT_COHERENCE_THRESHOLD;
    config->anomaly_threshold = 0.8f;

    /* Control parameters */
    config->intervention_threshold = 0.3f;
    config->mode_switch_cost = 0.1f;
    config->resource_tradeoff_lambda = 0.5f;

    /* Learning parameters */
    config->meta_learning_rate = 0.01f;
    config->exploration_rate = 0.1f;
    config->enable_online_learning = true;

    /* Safety constraints */
    config->min_coherence = 0.3f;
    config->max_resource_usage = 0.95f;
    config->max_retries = 3;

    return NIMCP_SUCCESS;
}

omni_metacog_ctx_t* omni_metacog_create(void) {
    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_create", 0.0f);


    omni_metacog_config_t config;
    omni_metacog_get_default_config(&config);
    return omni_metacog_create_with_config(&config);
}

omni_metacog_ctx_t* omni_metacog_create_with_config(
    const omni_metacog_config_t* config) {

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");


        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_create_", 0.0f);


    omni_metacog_ctx_t* ctx = (omni_metacog_ctx_t*)nimcp_malloc(
        sizeof(omni_metacog_ctx_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ctx");

        return NULL;
    }

    memset(ctx, 0, sizeof(*ctx));

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(omni_metacog_config_t));

    /* Allocate self-model */
    ctx->self_model = (omni_self_model_t*)nimcp_malloc(sizeof(omni_self_model_t));
    if (!ctx->self_model) {
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_metacog_create: ctx->self_model is NULL");
        return NULL;
    }

    /* Initialize self-model */
    if (omni_metacog_init_self_model(ctx) != NIMCP_SUCCESS) {
        nimcp_free(ctx->self_model);
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "omni_metacog_create: validation failed");
        return NULL;
    }

    /* Create mutex */
    ctx->mutex = nimcp_mutex_create(NULL);
    if (!ctx->mutex) {
        nimcp_free(ctx->self_model);
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_metacog_create: ctx->mutex is NULL");
        return NULL;
    }

    /* Initialize state */
    ctx->state = OMNI_METACOG_STATE_IDLE;
    ctx->current_mode = OMNI_METACOG_MODE_FORWARD;
    ctx->creation_time = get_current_time();
    ctx->last_update_time = ctx->creation_time;

    return ctx;
}

void omni_metacog_destroy(omni_metacog_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_destroy", 0.0f);


    if (ctx->mutex) {
        nimcp_mutex_destroy(ctx->mutex);
    }

    if (ctx->self_model) {
        nimcp_free(ctx->self_model);
    }

    nimcp_free(ctx);
    ctx = NULL;
}

nimcp_error_t omni_metacog_reset(omni_metacog_ctx_t* ctx) {
    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_reset", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");

    nimcp_mutex_lock(ctx->mutex);

    /* Reset state */
    ctx->state = OMNI_METACOG_STATE_IDLE;
    ctx->current_mode = OMNI_METACOG_MODE_FORWARD;

    /* Reset statistics */
    ctx->total_inferences = 0;
    ctx->successful_inferences = 0;
    ctx->interventions_made = 0;
    ctx->mode_switches = 0;

    /* Reset self-model */
    omni_metacog_init_self_model(ctx);

    /* Reset timing */
    ctx->last_update_time = get_current_time();

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * SELF-MODEL API IMPLEMENTATION
 * ============================================================================ */

nimcp_error_t omni_metacog_init_self_model(omni_metacog_ctx_t* ctx) {
    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_init_se", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(ctx->self_model, NIMCP_ERROR_INVALID_PARAMETER, "self_model is NULL");

    omni_self_model_t* model = ctx->self_model;
    memset(model, 0, sizeof(*model));

    /* Initialize capabilities for each mode */
    for (int i = 0; i < OMNI_METACOG_MODE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OMNI_METACOG_MODE_COUNT > 256) {
            omni_metacognition_heartbeat("omni_metacog_loop",
                             (float)(i + 1) / (float)OMNI_METACOG_MODE_COUNT);
        }

        model->capabilities[i].mode = (omni_metacog_mode_t)i;
        model->capabilities[i].proficiency = 0.5f;  /* Start neutral */
        model->capabilities[i].typical_cost = 0.5f;
        model->capabilities[i].typical_accuracy = 0.5f;
        model->capabilities[i].typical_latency = 0.1f;
        model->capabilities[i].usage_count = 0;
        model->capabilities[i].success_count = 0;
        model->capabilities[i].last_used = 0.0;
    }

    /* Initialize modalities */
    model->num_modalities = 0;
    for (int i = 0; i < OMNI_METACOG_MAX_MODALITIES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OMNI_METACOG_MAX_MODALITIES > 256) {
            omni_metacognition_heartbeat("omni_metacog_loop",
                             (float)(i + 1) / (float)OMNI_METACOG_MAX_MODALITIES);
        }

        model->modality_proficiency[i] = 0.5f;
    }

    /* Initialize resources */
    for (int i = 0; i < OMNI_RESOURCE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OMNI_RESOURCE_COUNT > 256) {
            omni_metacognition_heartbeat("omni_metacog_loop",
                             (float)(i + 1) / (float)OMNI_RESOURCE_COUNT);
        }

        model->resources.available[i] = 1.0f;
        model->resources.allocated[i] = 0.0f;
        model->resources.budget[i] = OMNI_METACOG_DEFAULT_RESOURCE_BUDGET;
        model->resources.consumption_rate[i] = 0.1f;
        model->resources.replenishment_rate[i] = 0.05f;
    }

    /* Initialize history */
    model->history_count = 0;
    model->history_head = 0;

    /* Initialize aggregate statistics */
    model->overall_accuracy = 0.5f;
    model->overall_efficiency = 0.5f;
    model->overall_coherence = 1.0f;

    /* Initialize learning parameters */
    model->self_model_confidence = 0.5f;
    model->learning_rate = 0.1f;
    model->discount_factor = 0.95f;

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_update_self_model(
    omni_metacog_ctx_t* ctx,
    omni_metacog_mode_t mode,
    float accuracy,
    float cost,
    float latency,
    bool success) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_update_", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(ctx->self_model, NIMCP_ERROR_INVALID_PARAMETER, "self_model is NULL");
    NIMCP_CHECK_THROW(mode < OMNI_METACOG_MODE_COUNT, NIMCP_ERROR_INVALID_PARAMETER, "mode out of range");

    nimcp_mutex_lock(ctx->mutex);

    omni_self_model_t* model = ctx->self_model;
    omni_capability_t* cap = &model->capabilities[mode];

    /* Update capability profile with EMA */
    float lr = model->learning_rate;
    cap->typical_accuracy = ema_update(cap->typical_accuracy, accuracy, lr);
    cap->typical_cost = ema_update(cap->typical_cost, cost, lr);
    cap->typical_latency = ema_update(cap->typical_latency, latency, lr);

    cap->usage_count++;
    if (success) {
        cap->success_count++;
    }

    /* Update proficiency based on success rate */
    if (cap->usage_count > 0) {
        float success_rate = (float)cap->success_count / (float)cap->usage_count;
        cap->proficiency = ema_update(cap->proficiency, success_rate, lr);
    }

    cap->last_used = get_current_time();

    /* Add to history */
    uint32_t idx = model->history_head;
    model->history[idx].mode = mode;
    model->history[idx].resource_used = cost;
    model->history[idx].accuracy = accuracy;
    model->history[idx].latency = latency;
    model->history[idx].coherence = 1.0f;  /* Will be updated by coherence check */
    model->history[idx].success = success;
    model->history[idx].timestamp = get_current_time();
    model->history[idx].context_hash = 0;

    model->history_head = (model->history_head + 1) % OMNI_METACOG_MAX_HISTORY;
    if (model->history_count < OMNI_METACOG_MAX_HISTORY) {
        model->history_count++;
    }

    /* Update aggregate statistics */
    model->overall_accuracy = ema_update(model->overall_accuracy, accuracy, lr);

    float efficiency = (accuracy > 0.0f) ? accuracy / (cost + 0.01f) : 0.0f;
    model->overall_efficiency = ema_update(model->overall_efficiency, efficiency, lr);

    /* Update global counters */
    ctx->total_inferences++;
    if (success) {
        ctx->successful_inferences++;
    }

    ctx->last_update_time = get_current_time();

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_get_capability(
    omni_metacog_ctx_t* ctx,
    omni_metacog_mode_t mode,
    omni_capability_t* capability) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_get_cap", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(ctx->self_model, NIMCP_ERROR_INVALID_PARAMETER, "self_model is NULL");
    NIMCP_CHECK_THROW(capability, NIMCP_ERROR_INVALID_PARAMETER, "capability is NULL");
    NIMCP_CHECK_THROW(mode < OMNI_METACOG_MODE_COUNT, NIMCP_ERROR_INVALID_PARAMETER, "mode out of range");

    nimcp_mutex_lock(ctx->mutex);
    memcpy(capability, &ctx->self_model->capabilities[mode], sizeof(*capability));
    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_get_resources(
    omni_metacog_ctx_t* ctx,
    omni_resource_state_t* resources) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_get_res", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(ctx->self_model, NIMCP_ERROR_INVALID_PARAMETER, "self_model is NULL");
    NIMCP_CHECK_THROW(resources, NIMCP_ERROR_INVALID_PARAMETER, "resources is NULL");

    nimcp_mutex_lock(ctx->mutex);
    memcpy(resources, &ctx->self_model->resources, sizeof(*resources));
    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_set_resource_budget(
    omni_metacog_ctx_t* ctx,
    omni_resource_type_t resource,
    float budget) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_set_res", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(ctx->self_model, NIMCP_ERROR_INVALID_PARAMETER, "self_model is NULL");
    NIMCP_CHECK_THROW(resource < OMNI_RESOURCE_COUNT, NIMCP_ERROR_INVALID_PARAMETER, "resource out of range");

    nimcp_mutex_lock(ctx->mutex);
    ctx->self_model->resources.budget[resource] = nimcp_clamp01(budget);
    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * MONITORING API IMPLEMENTATION
 * ============================================================================ */

nimcp_error_t omni_metacog_monitor(
    omni_metacog_ctx_t* ctx,
    omni_monitoring_snapshot_t* snapshot) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_monitor", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(snapshot, NIMCP_ERROR_INVALID_PARAMETER, "snapshot is NULL");

    nimcp_mutex_lock(ctx->mutex);

    memset(snapshot, 0, sizeof(*snapshot));

    snapshot->timestamp = get_current_time();
    snapshot->current_mode = ctx->current_mode;
    snapshot->meta_state = ctx->state;

    /* Performance metrics from self-model */
    if (ctx->self_model) {
        const omni_capability_t* cap = &ctx->self_model->capabilities[ctx->current_mode];
        snapshot->current_accuracy = cap->typical_accuracy;
        snapshot->current_confidence = cap->proficiency;
        snapshot->current_progress = 0.5f;  /* Placeholder */

        /* Resource usage */
        for (int i = 0; i < OMNI_RESOURCE_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && OMNI_RESOURCE_COUNT > 256) {
                omni_metacognition_heartbeat("omni_metacog_loop",
                                 (float)(i + 1) / (float)OMNI_RESOURCE_COUNT);
            }

            snapshot->resource_usage[i] = ctx->self_model->resources.allocated[i];
        }

        /* Coherence from latest data */
        snapshot->coherence.status = OMNI_COHERENCE_OK;
        snapshot->coherence.coherence_score = ctx->self_model->overall_coherence;
        snapshot->coherence.plausibility_score = 1.0f;
        snapshot->coherence.temporal_consistency = 1.0f;
        snapshot->coherence.modal_agreement = 1.0f;
    }

    /* Anomaly detection placeholder */
    snapshot->anomaly_detected = false;
    snapshot->anomaly_score = 0.0f;

    /* Store as latest snapshot */
    memcpy(&ctx->latest_snapshot, snapshot, sizeof(*snapshot));

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_check_coherence(
    omni_metacog_ctx_t* ctx,
    const void* inferences,
    uint32_t num_inferences,
    omni_coherence_result_t* result) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_check_c", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_INVALID_PARAMETER, "result is NULL");

    nimcp_mutex_lock(ctx->mutex);

    memset(result, 0, sizeof(*result));

    /* Basic coherence check */
    result->status = OMNI_COHERENCE_OK;
    result->coherence_score = 1.0f;
    result->contradiction_level = 0.0f;
    result->plausibility_score = 1.0f;
    result->temporal_consistency = 1.0f;
    result->modal_agreement = 1.0f;
    result->num_violations = 0;

    if (num_inferences == 0 || !inferences) {
        /* No inferences to check - fully coherent by default */
        snprintf(result->violation_description, sizeof(result->violation_description),
                 "No inferences to check");
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_SUCCESS;
    }

    /* Simulate coherence checking based on history */
    if (ctx->self_model && ctx->self_model->history_count > 0) {
        /* Check for recent failures indicating potential incoherence */
        uint32_t recent_failures = 0;
        uint32_t check_count = (ctx->self_model->history_count < 10) ?
                               ctx->self_model->history_count : 10;

        for (uint32_t i = 0; i < check_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && check_count > 256) {
                omni_metacognition_heartbeat("omni_metacog_loop",
                                 (float)(i + 1) / (float)check_count);
            }

            uint32_t idx = (ctx->self_model->history_head + OMNI_METACOG_MAX_HISTORY - 1 - i) %
                           OMNI_METACOG_MAX_HISTORY;
            if (!ctx->self_model->history[idx].success) {
                recent_failures++;
            }
        }

        if (recent_failures > 0) {
            result->coherence_score = 1.0f - (float)recent_failures / (float)check_count * 0.5f;

            if (result->coherence_score < ctx->config.coherence_threshold) {
                result->status = OMNI_COHERENCE_IMPLAUSIBILITY;
                result->num_violations = recent_failures;
                snprintf(result->violation_description, sizeof(result->violation_description),
                         "High failure rate indicates potential incoherence");
            }
        }

        /* Update self-model coherence */
        ctx->self_model->overall_coherence = ema_update(
            ctx->self_model->overall_coherence, result->coherence_score, 0.1f);
    }

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_detect_anomaly(
    omni_metacog_ctx_t* ctx,
    const omni_monitoring_snapshot_t* snapshot,
    float* anomaly_score,
    char* description) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_detect_", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(snapshot, NIMCP_ERROR_INVALID_PARAMETER, "snapshot is NULL");
    NIMCP_CHECK_THROW(anomaly_score, NIMCP_ERROR_INVALID_PARAMETER, "anomaly_score is NULL");

    nimcp_mutex_lock(ctx->mutex);

    *anomaly_score = 0.0f;

    /* Check for anomalies based on self-model */
    if (ctx->self_model) {
        const omni_capability_t* cap = &ctx->self_model->capabilities[snapshot->current_mode];

        /* Anomaly: accuracy much lower than typical */
        if (cap->usage_count > 10) {
            float accuracy_deviation = fabsf(snapshot->current_accuracy - cap->typical_accuracy);
            if (accuracy_deviation > 0.3f) {
                *anomaly_score = fmaxf(*anomaly_score, accuracy_deviation);
            }
        }

        /* Anomaly: resource usage exceeding budget */
        for (int i = 0; i < OMNI_RESOURCE_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && OMNI_RESOURCE_COUNT > 256) {
                omni_metacognition_heartbeat("omni_metacog_loop",
                                 (float)(i + 1) / (float)OMNI_RESOURCE_COUNT);
            }

            if (snapshot->resource_usage[i] > ctx->self_model->resources.budget[i]) {
                float excess = snapshot->resource_usage[i] - ctx->self_model->resources.budget[i];
                *anomaly_score = fmaxf(*anomaly_score, excess);
            }
        }

        /* Anomaly: coherence below threshold */
        if (snapshot->coherence.coherence_score < ctx->config.min_coherence) {
            float coherence_deficit = ctx->config.min_coherence - snapshot->coherence.coherence_score;
            *anomaly_score = fmaxf(*anomaly_score, coherence_deficit);
        }
    }

    /* Generate description */
    if (description) {
        if (*anomaly_score >= ctx->config.anomaly_threshold) {
            snprintf(description, 256,
                     "High anomaly score (%.2f): potential system instability",
                     *anomaly_score);
        } else if (*anomaly_score > 0.0f) {
            snprintf(description, 256,
                     "Minor anomaly detected (%.2f)", *anomaly_score);
        } else {
            snprintf(description, 256, "No anomalies detected");
        }
    }

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * EVALUATION API IMPLEMENTATION
 * ============================================================================ */

nimcp_error_t omni_metacog_evaluate_performance(
    omni_metacog_ctx_t* ctx,
    float* accuracy,
    float* confidence) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_evaluat", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(accuracy, NIMCP_ERROR_INVALID_PARAMETER, "accuracy is NULL");
    NIMCP_CHECK_THROW(confidence, NIMCP_ERROR_INVALID_PARAMETER, "confidence is NULL");

    nimcp_mutex_lock(ctx->mutex);

    if (ctx->self_model) {
        const omni_capability_t* cap = &ctx->self_model->capabilities[ctx->current_mode];
        *accuracy = cap->typical_accuracy;
        *confidence = cap->proficiency;

        /* Adjust confidence based on usage */
        if (cap->usage_count < 10) {
            *confidence *= 0.5f;  /* Low confidence with little experience */
        }
    } else {
        *accuracy = 0.5f;
        *confidence = 0.0f;
    }

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_predict_performance(
    omni_metacog_ctx_t* ctx,
    omni_metacog_mode_t mode,
    uint32_t context_hash,
    float* expected_accuracy,
    float* expected_cost) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_predict", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(expected_accuracy, NIMCP_ERROR_INVALID_PARAMETER, "expected_accuracy is NULL");
    NIMCP_CHECK_THROW(expected_cost, NIMCP_ERROR_INVALID_PARAMETER, "expected_cost is NULL");
    NIMCP_CHECK_THROW(mode < OMNI_METACOG_MODE_COUNT, NIMCP_ERROR_INVALID_PARAMETER, "mode out of range");

    nimcp_mutex_lock(ctx->mutex);

    if (ctx->self_model) {
        const omni_capability_t* cap = &ctx->self_model->capabilities[mode];
        *expected_accuracy = cap->typical_accuracy;
        *expected_cost = cap->typical_cost;

        /* Adjust based on similar contexts in history */
        if (context_hash != 0 && ctx->self_model->history_count > 0) {
            float total_accuracy = 0.0f;
            float total_cost = 0.0f;
            uint32_t matches = 0;

            for (uint32_t i = 0; i < ctx->self_model->history_count; i++) {
                if (ctx->self_model->history[i].mode == mode) {
                    /* Weight by context similarity (simplified) */
                    total_accuracy += ctx->self_model->history[i].accuracy;
                    total_cost += ctx->self_model->history[i].resource_used;
                    matches++;
                }
            }

            if (matches > 0) {
                *expected_accuracy = total_accuracy / matches;
                *expected_cost = total_cost / matches;
            }
        }
    } else {
        *expected_accuracy = 0.5f;
        *expected_cost = 0.5f;
    }

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

float omni_metacog_get_confidence(omni_metacog_ctx_t* ctx) {
    if (!ctx || !ctx->self_model) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_get_con", 0.0f);


    nimcp_mutex_lock(ctx->mutex);
    float conf = ctx->self_model->capabilities[ctx->current_mode].proficiency;
    nimcp_mutex_unlock(ctx->mutex);

    return conf;
}

/* ============================================================================
 * CONTROL API IMPLEMENTATION
 * ============================================================================ */

nimcp_error_t omni_metacog_select_mode(
    omni_metacog_ctx_t* ctx,
    uint32_t context_hash,
    omni_mode_recommendation_t* recommendation) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_select_", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(recommendation, NIMCP_ERROR_INVALID_PARAMETER, "recommendation is NULL");

    nimcp_mutex_lock(ctx->mutex);

    memset(recommendation, 0, sizeof(*recommendation));

    /* Calculate expected value for each mode */
    float mode_values[OMNI_METACOG_MODE_COUNT];
    float mode_probs[OMNI_METACOG_MODE_COUNT];

    for (int i = 0; i < OMNI_METACOG_MODE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OMNI_METACOG_MODE_COUNT > 256) {
            omni_metacognition_heartbeat("omni_metacog_loop",
                             (float)(i + 1) / (float)OMNI_METACOG_MODE_COUNT);
        }

        float expected_accuracy, expected_cost;
        nimcp_mutex_unlock(ctx->mutex);
        omni_metacog_predict_performance(ctx, (omni_metacog_mode_t)i,
                                         context_hash, &expected_accuracy, &expected_cost);
        nimcp_mutex_lock(ctx->mutex);

        /* Value = Accuracy - lambda * Cost */
        mode_values[i] = expected_accuracy - ctx->config.resource_tradeoff_lambda * expected_cost;

        /* Subtract mode switch cost if switching */
        if (i != ctx->current_mode) {
            mode_values[i] -= ctx->config.mode_switch_cost;
        }
    }

    /* Apply softmax to get probabilities */
    softmax(mode_values, mode_probs, OMNI_METACOG_MODE_COUNT);

    /* Find best mode */
    int best_mode = 0;
    float best_value = mode_values[0];
    for (int i = 1; i < OMNI_METACOG_MODE_COUNT; i++) {
        if (mode_values[i] > best_value) {
            best_value = mode_values[i];
            best_mode = i;
        }
    }

    recommendation->recommended_mode = (omni_metacog_mode_t)best_mode;
    recommendation->confidence = mode_probs[best_mode];

    if (ctx->self_model) {
        const omni_capability_t* cap = &ctx->self_model->capabilities[best_mode];
        recommendation->expected_accuracy = cap->typical_accuracy;
        recommendation->expected_cost = cap->typical_cost;
        recommendation->expected_latency = cap->typical_latency;
    }

    /* Generate rationale */
    snprintf(recommendation->rationale, sizeof(recommendation->rationale),
             "Mode %s selected with expected value %.2f",
             omni_metacog_mode_to_string((omni_metacog_mode_t)best_mode),
             best_value);

    /* Find alternatives */
    float sorted_values[OMNI_METACOG_MODE_COUNT];
    int sorted_indices[OMNI_METACOG_MODE_COUNT];
    memcpy(sorted_values, mode_values, sizeof(sorted_values));
    for (int i = 0; i < OMNI_METACOG_MODE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OMNI_METACOG_MODE_COUNT > 256) {
            omni_metacognition_heartbeat("omni_metacog_loop",
                             (float)(i + 1) / (float)OMNI_METACOG_MODE_COUNT);
        }

        sorted_indices[i] = i;
    }

    /* Simple bubble sort for small array */
    for (int i = 0; i < OMNI_METACOG_MODE_COUNT - 1; i++) {
        for (int j = 0; j < OMNI_METACOG_MODE_COUNT - 1 - i; j++) {
            if (sorted_values[j] < sorted_values[j + 1]) {
                float tv = sorted_values[j];
                sorted_values[j] = sorted_values[j + 1];
                sorted_values[j + 1] = tv;
                int ti = sorted_indices[j];
                sorted_indices[j] = sorted_indices[j + 1];
                sorted_indices[j + 1] = ti;
            }
        }
    }

    int alt_idx = 0;
    for (int i = 0; i < OMNI_METACOG_MODE_COUNT && alt_idx < 3; i++) {
        if (sorted_indices[i] != best_mode) {
            recommendation->alternatives[alt_idx] = (omni_metacog_mode_t)sorted_indices[i];
            recommendation->alternative_scores[alt_idx] = sorted_values[i];
            alt_idx++;
        }
    }

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_plan_resources(
    omni_metacog_ctx_t* ctx,
    omni_metacog_mode_t mode,
    float accuracy_target,
    omni_resource_plan_t* plan) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_plan_re", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(plan, NIMCP_ERROR_INVALID_PARAMETER, "plan is NULL");
    NIMCP_CHECK_THROW(mode < OMNI_METACOG_MODE_COUNT, NIMCP_ERROR_INVALID_PARAMETER, "mode out of range");

    nimcp_mutex_lock(ctx->mutex);

    memset(plan, 0, sizeof(*plan));

    if (ctx->self_model) {
        const omni_capability_t* cap = &ctx->self_model->capabilities[mode];

        /* Estimate resources needed based on accuracy target */
        float accuracy_ratio = accuracy_target / (cap->typical_accuracy + 0.01f);
        float base_cost = cap->typical_cost;

        /* Resources scale with accuracy demand */
        for (int i = 0; i < OMNI_RESOURCE_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && OMNI_RESOURCE_COUNT > 256) {
                omni_metacognition_heartbeat("omni_metacog_loop",
                                 (float)(i + 1) / (float)OMNI_RESOURCE_COUNT);
            }

            plan->allocations[i] = nimcp_clamp01(base_cost * accuracy_ratio);
            plan->total_budget += plan->allocations[i];

            /* Check against budget */
            if (plan->allocations[i] > ctx->self_model->resources.budget[i]) {
                plan->within_budget = false;
            }
        }

        plan->total_budget /= OMNI_RESOURCE_COUNT;
        if (plan->total_budget <= ctx->config.max_resource_usage) {
            plan->within_budget = true;
        }

        /* Efficiency score */
        plan->efficiency_score = accuracy_target / (plan->total_budget + 0.01f);

        snprintf(plan->rationale, sizeof(plan->rationale),
                 "Allocating %.1f%% resources for %.1f%% accuracy target",
                 plan->total_budget * 100.0f, accuracy_target * 100.0f);
    }

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_decide_intervention(
    omni_metacog_ctx_t* ctx,
    const omni_monitoring_snapshot_t* snapshot,
    omni_intervention_t* intervention) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_decide_", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(snapshot, NIMCP_ERROR_INVALID_PARAMETER, "snapshot is NULL");
    NIMCP_CHECK_THROW(intervention, NIMCP_ERROR_INVALID_PARAMETER, "intervention is NULL");

    nimcp_mutex_lock(ctx->mutex);

    memset(intervention, 0, sizeof(*intervention));

    intervention->should_intervene = false;
    intervention->should_abort = false;
    intervention->should_retry = false;

    /* Check intervention conditions */

    /* 1. Coherence violation */
    if (snapshot->coherence.coherence_score < ctx->config.min_coherence) {
        intervention->should_intervene = true;
        intervention->should_retry = true;
        snprintf(intervention->intervention_reason, sizeof(intervention->intervention_reason),
                 "Coherence (%.2f) below minimum (%.2f)",
                 snapshot->coherence.coherence_score, ctx->config.min_coherence);
    }

    /* 2. Anomaly detected */
    if (snapshot->anomaly_detected && snapshot->anomaly_score >= ctx->config.anomaly_threshold) {
        intervention->should_intervene = true;
        snprintf(intervention->intervention_reason, sizeof(intervention->intervention_reason),
                 "Anomaly detected with score %.2f", snapshot->anomaly_score);
    }

    /* 3. Resource exhaustion */
    for (int i = 0; i < OMNI_RESOURCE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && OMNI_RESOURCE_COUNT > 256) {
            omni_metacognition_heartbeat("omni_metacog_loop",
                             (float)(i + 1) / (float)OMNI_RESOURCE_COUNT);
        }

        if (ctx->self_model &&
            snapshot->resource_usage[i] > ctx->self_model->resources.budget[i]) {
            intervention->should_intervene = true;
            intervention->precision_adjustment = -0.2f;  /* Reduce precision to save resources */
            snprintf(intervention->intervention_reason, sizeof(intervention->intervention_reason),
                     "Resource %s exceeded budget", omni_resource_type_to_string((omni_resource_type_t)i));
            break;
        }
    }

    /* 4. Progress stalled */
    if (snapshot->current_progress < 0.1f && snapshot->current_confidence < 0.3f) {
        intervention->should_intervene = true;

        /* Recommend mode switch */
        omni_mode_recommendation_t rec;
        nimcp_mutex_unlock(ctx->mutex);
        omni_metacog_select_mode(ctx, 0, &rec);
        nimcp_mutex_lock(ctx->mutex);

        if (rec.recommended_mode != snapshot->current_mode) {
            intervention->new_mode = rec.recommended_mode;
        }

        snprintf(intervention->intervention_reason, sizeof(intervention->intervention_reason),
                 "Progress stalled (%.1f%%) with low confidence (%.1f%%)",
                 snapshot->current_progress * 100.0f, snapshot->current_confidence * 100.0f);
    }

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_execute_intervention(
    omni_metacog_ctx_t* ctx,
    const omni_intervention_t* intervention) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_execute", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(intervention, NIMCP_ERROR_INVALID_PARAMETER, "intervention is NULL");

    if (!intervention->should_intervene) {
        return NIMCP_SUCCESS;  /* Nothing to do */
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Switch mode if recommended */
    if (intervention->new_mode != ctx->current_mode &&
        intervention->new_mode < OMNI_METACOG_MODE_COUNT) {
        ctx->current_mode = intervention->new_mode;
        ctx->mode_switches++;
    }

    /* Apply precision adjustment */
    if (fabsf(intervention->precision_adjustment) > 0.001f) {
        /* This would integrate with Phase 6 precision system */
    }

    /* Apply resource plan */
    if (intervention->resource_plan.total_budget > 0.0f && ctx->self_model) {
        for (int i = 0; i < OMNI_RESOURCE_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && OMNI_RESOURCE_COUNT > 256) {
                omni_metacognition_heartbeat("omni_metacog_loop",
                                 (float)(i + 1) / (float)OMNI_RESOURCE_COUNT);
            }

            ctx->self_model->resources.allocated[i] = intervention->resource_plan.allocations[i];
        }
    }

    ctx->interventions_made++;
    ctx->state = OMNI_METACOG_STATE_INTERVENING;

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_switch_mode(
    omni_metacog_ctx_t* ctx,
    omni_metacog_mode_t new_mode) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_switch_", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(new_mode < OMNI_METACOG_MODE_COUNT, NIMCP_ERROR_INVALID_PARAMETER, "new_mode out of range");

    nimcp_mutex_lock(ctx->mutex);

    if (ctx->current_mode != new_mode) {
        ctx->current_mode = new_mode;
        ctx->mode_switches++;
    }

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_adjust_precision(
    omni_metacog_ctx_t* ctx,
    float adjustment) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_adjust_", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");

    /* Clamp adjustment */
    if (adjustment < -1.0f) adjustment = -1.0f;
    if (adjustment > 1.0f) adjustment = 1.0f;

    nimcp_mutex_lock(ctx->mutex);

    /* Adjust precision budget for all resources proportionally */
    if (ctx->self_model) {
        for (int i = 0; i < OMNI_RESOURCE_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && OMNI_RESOURCE_COUNT > 256) {
                omni_metacognition_heartbeat("omni_metacog_loop",
                                 (float)(i + 1) / (float)OMNI_RESOURCE_COUNT);
            }

            float new_budget = ctx->self_model->resources.budget[i] + adjustment * 0.1f;
            ctx->self_model->resources.budget[i] = nimcp_clamp01(new_budget);
        }
    }

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * META-LEARNING API IMPLEMENTATION
 * ============================================================================ */

nimcp_error_t omni_metacog_learn(
    omni_metacog_ctx_t* ctx,
    uint32_t num_entries) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_learn", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(ctx->self_model, NIMCP_ERROR_INVALID_PARAMETER, "self_model is NULL");

    if (num_entries == 0) {
        num_entries = ctx->self_model->history_count;
    }

    if (num_entries > ctx->self_model->history_count) {
        num_entries = ctx->self_model->history_count;
    }

    nimcp_mutex_lock(ctx->mutex);

    ctx->state = OMNI_METACOG_STATE_LEARNING;

    /* Learn from history */
    float lr = ctx->config.meta_learning_rate;
    float discount = ctx->self_model->discount_factor;

    for (uint32_t i = 0; i < num_entries; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_entries > 256) {
            omni_metacognition_heartbeat("omni_metacog_loop",
                             (float)(i + 1) / (float)num_entries);
        }

        /* Get entry from ring buffer (most recent first) */
        uint32_t idx = (ctx->self_model->history_head + OMNI_METACOG_MAX_HISTORY - 1 - i) %
                       OMNI_METACOG_MAX_HISTORY;

        omni_metacog_history_t* entry = &ctx->self_model->history[idx];
        omni_capability_t* cap = &ctx->self_model->capabilities[entry->mode];

        /* Apply discounted learning */
        float weight = powf(discount, (float)i) * lr;

        /* Update capability estimates */
        cap->typical_accuracy = ema_update(cap->typical_accuracy, entry->accuracy, weight);
        cap->typical_cost = ema_update(cap->typical_cost, entry->resource_used, weight);
        cap->typical_latency = ema_update(cap->typical_latency, entry->latency, weight);

        /* Update proficiency based on success */
        float target_prof = entry->success ? 1.0f : 0.0f;
        cap->proficiency = ema_update(cap->proficiency, target_prof, weight * 0.1f);
    }

    /* Update self-model confidence based on prediction accuracy */
    ctx->self_model->self_model_confidence = nimcp_clamp01(
        ctx->self_model->self_model_confidence + lr * 0.01f);

    ctx->state = OMNI_METACOG_STATE_MONITORING;

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_update_policy(
    omni_metacog_ctx_t* ctx,
    float reward) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_update_", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");

    nimcp_mutex_lock(ctx->mutex);

    /* Update exploration rate based on reward
     * Positive reward: decrease exploration (reward is positive, subtract)
     * Negative reward: increase exploration (reward is negative, -negative = add)
     * Both cases use the same formula: rate - lr * reward * 0.1f
     */
    float lr = ctx->config.meta_learning_rate;
    ctx->config.exploration_rate = nimcp_clamp01(
        ctx->config.exploration_rate - lr * reward * 0.1f);

    /* Update intervention threshold based on reward */
    if (ctx->interventions_made > 0) {
        float intervention_effectiveness = reward;

        if (intervention_effectiveness > 0.0f) {
            /* Interventions are helping: lower threshold (intervene more) */
            ctx->config.intervention_threshold = nimcp_clamp01(
                ctx->config.intervention_threshold - lr * 0.01f);
        } else {
            /* Interventions not helping: raise threshold (intervene less) */
            ctx->config.intervention_threshold = nimcp_clamp01(
                ctx->config.intervention_threshold + lr * 0.01f);
        }
    }

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_get_learning_stats(
    omni_metacog_ctx_t* ctx,
    float* improvement,
    float* convergence) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_get_lea", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(ctx->self_model, NIMCP_ERROR_INVALID_PARAMETER, "self_model is NULL");
    NIMCP_CHECK_THROW(improvement, NIMCP_ERROR_INVALID_PARAMETER, "improvement is NULL");
    NIMCP_CHECK_THROW(convergence, NIMCP_ERROR_INVALID_PARAMETER, "convergence is NULL");

    nimcp_mutex_lock(ctx->mutex);

    /* Calculate improvement from history */
    *improvement = 0.0f;

    if (ctx->self_model->history_count >= 20) {
        /* Compare first half vs second half accuracy */
        float first_half_acc = 0.0f;
        float second_half_acc = 0.0f;
        uint32_t half = ctx->self_model->history_count / 2;

        for (uint32_t i = 0; i < half; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && half > 256) {
                omni_metacognition_heartbeat("omni_metacog_loop",
                                 (float)(i + 1) / (float)half);
            }

            first_half_acc += ctx->self_model->history[i].accuracy;
        }
        for (uint32_t i = half; i < ctx->self_model->history_count; i++) {
            second_half_acc += ctx->self_model->history[i].accuracy;
        }

        first_half_acc /= half;
        second_half_acc /= (ctx->self_model->history_count - half);

        *improvement = second_half_acc - first_half_acc;
    }

    /* Calculate convergence as stability of recent performance */
    *convergence = 0.0f;

    if (ctx->self_model->history_count >= 10) {
        float mean = 0.0f;
        float variance = 0.0f;
        uint32_t n = 10;

        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                omni_metacognition_heartbeat("omni_metacog_loop",
                                 (float)(i + 1) / (float)n);
            }

            uint32_t idx = (ctx->self_model->history_head + OMNI_METACOG_MAX_HISTORY - 1 - i) %
                           OMNI_METACOG_MAX_HISTORY;
            mean += ctx->self_model->history[idx].accuracy;
        }
        mean /= n;

        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                omni_metacognition_heartbeat("omni_metacog_loop",
                                 (float)(i + 1) / (float)n);
            }

            uint32_t idx = (ctx->self_model->history_head + OMNI_METACOG_MAX_HISTORY - 1 - i) %
                           OMNI_METACOG_MAX_HISTORY;
            float diff = ctx->self_model->history[idx].accuracy - mean;
            variance += diff * diff;
        }
        variance /= n;

        /* Low variance = high convergence */
        *convergence = nimcp_clamp01(1.0f - sqrtf(variance) * 2.0f);
    }

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * INTEGRATION API IMPLEMENTATION
 * ============================================================================ */

nimcp_error_t omni_metacog_connect_world_model(
    omni_metacog_ctx_t* ctx,
    void* world_model) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_connect", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");

    nimcp_mutex_lock(ctx->mutex);
    ctx->world_model = world_model;
    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_connect_active_inference(
    omni_metacog_ctx_t* ctx,
    void* active_inference) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_connect", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");

    nimcp_mutex_lock(ctx->mutex);
    ctx->active_inference = active_inference;
    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_connect_precision(
    omni_metacog_ctx_t* ctx,
    void* precision_system) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_connect", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");

    nimcp_mutex_lock(ctx->mutex);
    ctx->precision_system = precision_system;
    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_metacog_step(omni_metacog_ctx_t* ctx) {
    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_step", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");

    nimcp_mutex_lock(ctx->mutex);

    /* 1. Monitor current state */
    omni_monitoring_snapshot_t snapshot;
    nimcp_mutex_unlock(ctx->mutex);
    omni_metacog_monitor(ctx, &snapshot);
    nimcp_mutex_lock(ctx->mutex);

    /* 2. Detect anomalies */
    float anomaly_score = 0.0f;
    char anomaly_desc[NIMCP_ERROR_BUFFER_SIZE];
    nimcp_mutex_unlock(ctx->mutex);
    omni_metacog_detect_anomaly(ctx, &snapshot, &anomaly_score, anomaly_desc);
    nimcp_mutex_lock(ctx->mutex);

    snapshot.anomaly_detected = (anomaly_score >= ctx->config.anomaly_threshold);
    snapshot.anomaly_score = anomaly_score;

    /* 3. Decide on intervention */
    omni_intervention_t intervention;
    nimcp_mutex_unlock(ctx->mutex);
    omni_metacog_decide_intervention(ctx, &snapshot, &intervention);
    nimcp_mutex_lock(ctx->mutex);

    /* 4. Execute intervention if needed */
    if (intervention.should_intervene) {
        nimcp_mutex_unlock(ctx->mutex);
        omni_metacog_execute_intervention(ctx, &intervention);
        nimcp_mutex_lock(ctx->mutex);
    }

    /* 5. Online learning if enabled */
    if (ctx->config.enable_online_learning && ctx->self_model->history_count > 0) {
        nimcp_mutex_unlock(ctx->mutex);
        omni_metacog_learn(ctx, 5);  /* Learn from last 5 entries */
        nimcp_mutex_lock(ctx->mutex);
    }

    /* Update resource availability (replenishment) */
    if (ctx->self_model) {
        for (int i = 0; i < OMNI_RESOURCE_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && OMNI_RESOURCE_COUNT > 256) {
                omni_metacognition_heartbeat("omni_metacog_loop",
                                 (float)(i + 1) / (float)OMNI_RESOURCE_COUNT);
            }

            float replenish = ctx->self_model->resources.replenishment_rate[i];
            ctx->self_model->resources.available[i] = nimcp_clamp01(
                ctx->self_model->resources.available[i] + replenish);
        }
    }

    ctx->last_update_time = get_current_time();

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * STATISTICS AND DEBUGGING IMPLEMENTATION
 * ============================================================================ */

nimcp_error_t omni_metacog_get_statistics(
    omni_metacog_ctx_t* ctx,
    uint64_t* total_inferences,
    float* success_rate,
    float* avg_efficiency) {

    /* Phase 8: Heartbeat at operation start */
    omni_metacognition_heartbeat("omni_metacog_omni_metacog_get_sta", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAMETER, "context is NULL");
    NIMCP_CHECK_THROW(total_inferences, NIMCP_ERROR_INVALID_PARAMETER, "total_inferences is NULL");
    NIMCP_CHECK_THROW(success_rate, NIMCP_ERROR_INVALID_PARAMETER, "success_rate is NULL");
    NIMCP_CHECK_THROW(avg_efficiency, NIMCP_ERROR_INVALID_PARAMETER, "avg_efficiency is NULL");

    nimcp_mutex_lock(ctx->mutex);

    *total_inferences = ctx->total_inferences;

    if (ctx->total_inferences > 0) {
        *success_rate = (float)ctx->successful_inferences / (float)ctx->total_inferences;
    } else {
        *success_rate = 0.0f;
    }

    if (ctx->self_model) {
        *avg_efficiency = ctx->self_model->overall_efficiency;
    } else {
        *avg_efficiency = 0.0f;
    }

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

const char* omni_metacog_mode_to_string(omni_metacog_mode_t mode) {
    switch (mode) {
        case OMNI_METACOG_MODE_FORWARD:      return "FORWARD";
        case OMNI_METACOG_MODE_BACKWARD:     return "BACKWARD";
        case OMNI_METACOG_MODE_LATERAL:      return "LATERAL";
        case OMNI_METACOG_MODE_HIERARCHICAL: return "HIERARCHICAL";
        case OMNI_METACOG_MODE_EXPLORATORY:  return "EXPLORATORY";
        case OMNI_METACOG_MODE_EXPLOITATIVE: return "EXPLOITATIVE";
        case OMNI_METACOG_MODE_DREAMING:     return "DREAMING";
        case OMNI_METACOG_MODE_CONSOLIDATING: return "CONSOLIDATING";
        default:                             return "UNKNOWN";
    }
}

const char* omni_metacog_state_to_string(omni_metacog_state_t state) {
    switch (state) {
        case OMNI_METACOG_STATE_IDLE:        return "IDLE";
        case OMNI_METACOG_STATE_MONITORING:  return "MONITORING";
        case OMNI_METACOG_STATE_EVALUATING:  return "EVALUATING";
        case OMNI_METACOG_STATE_INTERVENING: return "INTERVENING";
        case OMNI_METACOG_STATE_LEARNING:    return "LEARNING";
        default:                             return "UNKNOWN";
    }
}

const char* omni_resource_type_to_string(omni_resource_type_t resource) {
    switch (resource) {
        case OMNI_RESOURCE_COMPUTE:   return "COMPUTE";
        case OMNI_RESOURCE_MEMORY:    return "MEMORY";
        case OMNI_RESOURCE_TIME:      return "TIME";
        case OMNI_RESOURCE_ATTENTION: return "ATTENTION";
        case OMNI_RESOURCE_PRECISION: return "PRECISION";
        default:                      return "UNKNOWN";
    }
}

const char* omni_coherence_status_to_string(omni_coherence_status_t status) {
    switch (status) {
        case OMNI_COHERENCE_OK:            return "OK";
        case OMNI_COHERENCE_CONTRADICTION: return "CONTRADICTION";
        case OMNI_COHERENCE_IMPLAUSIBILITY: return "IMPLAUSIBILITY";
        case OMNI_COHERENCE_TEMPORAL:      return "TEMPORAL";
        case OMNI_COHERENCE_MODAL:         return "MODAL";
        default:                           return "UNKNOWN";
    }
}
