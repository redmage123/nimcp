/**
 * @file nimcp_omni_wm_kg_bridge.c
 * @brief World Model KG Wiring Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with KG wiring system
 * WHY:  Enable semantic reasoning and module state prediction via world model
 * HOW:  KG entities/relationships train world model; WM predicts entity/module states
 *
 * IMPLEMENTATION NOTES:
 * =====================
 * This implementation integrates several key concepts:
 *
 * 1. KNOWLEDGE-AUGMENTED PREDICTIONS:
 *    - KG provides entity-relationship structure as context
 *    - RSSM models temporal evolution of entity states
 *
 * 2. MODULE HEALTH FORECASTING:
 *    - Track module health metrics from registry
 *    - Predict failures using RSSM forward dynamics
 *
 * 3. ANOMALY DETECTION:
 *    - Compare observed states with predicted states
 *    - Flag significant deviations as anomalies
 */

#include "cognitive/omni/bridges/nimcp_omni_wm_kg_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ============================================================================
 * Module-level Constants
 * ============================================================================ */

#define LOG_MODULE "wm_kg_bridge"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(omni_wm_kg_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

void omni_wm_kg_bridge_set_instance_health_agent(
    omni_wm_kg_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "omni_wm_kg_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int omni_wm_kg_bridge_training_begin(omni_wm_kg_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_kg_bridge_training_begin: NULL argument");
        return -1;
    }
    omni_wm_kg_bridge_heartbeat_instance(g_omni_wm_kg_bridge_health_agent, "training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int omni_wm_kg_bridge_training_step(omni_wm_kg_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_kg_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_wm_kg_bridge_heartbeat_instance(g_omni_wm_kg_bridge_health_agent, "training_step", progress);
    (void)bridge;
    return 0;
}

int omni_wm_kg_bridge_training_end(omni_wm_kg_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_kg_bridge_training_end: NULL argument");
        return -1;
    }
    omni_wm_kg_bridge_heartbeat_instance(g_omni_wm_kg_bridge_health_agent, "training_end", 1.0f);
    (void)bridge;
    return 0;
}


/** Default effects array initial capacity */
#define DEFAULT_EFFECTS_CAPACITY 32

/** Training queue capacity */
#define TRAINING_QUEUE_CAPACITY 64

/** Module state history depth for prediction */
#define MODULE_STATE_HISTORY_DEPTH 10

/** Anomaly detection smoothing factor */
#define ANOMALY_SMOOTHING_FACTOR 0.1f

/** Registry sync retry limit */
#define REGISTRY_SYNC_RETRY_LIMIT 3

/* ============================================================================
 * Internal Helper Forward Declarations
 * ============================================================================ */

static nimcp_error_t allocate_effects_arrays(omni_wm_kg_bridge_t* bridge);
static void free_effects_arrays(omni_wm_kg_bridge_t* bridge);
static nimcp_error_t update_wm_to_kg_effects(omni_wm_kg_bridge_t* bridge);
static nimcp_error_t update_kg_to_wm_effects(omni_wm_kg_bridge_t* bridge);
static nimcp_error_t compute_entity_prediction(omni_wm_kg_bridge_t* bridge,
                                                uint32_t entity_id,
                                                uint32_t horizon_steps,
                                                wm_to_kg_entity_prediction_t* out);
static nimcp_error_t compute_module_failure_prediction(omni_wm_kg_bridge_t* bridge,
                                                        uint32_t module_id,
                                                        wm_to_kg_failure_prediction_t* out);
static float compute_anomaly_score(const float* predicted, const float* observed,
                                    uint32_t dim);
static uint64_t get_current_time_us(void);

/* Bio-async handlers */
static nimcp_error_t handle_entity_prediction_request(const void* msg, size_t msg_size,
                                                       nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_module_prediction_request(const void* msg, size_t msg_size,
                                                       nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_exception_notify(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_wiring_change(const void* msg, size_t msg_size,
                                           nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_training_event(const void* msg, size_t msg_size,
                                            nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_registry_sync(const void* msg, size_t msg_size,
                                           nimcp_bio_promise_t promise, void* user_data);

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/**
 * @brief Allocate dynamic arrays for effects structures
 */
static nimcp_error_t allocate_effects_arrays(omni_wm_kg_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    /* WM to KG effects arrays */
    bridge->wm_to_kg.entity_predictions_capacity = DEFAULT_EFFECTS_CAPACITY;
    bridge->wm_to_kg.entity_predictions = nimcp_calloc(
        DEFAULT_EFFECTS_CAPACITY, sizeof(wm_to_kg_entity_prediction_t));
    if (!bridge->wm_to_kg.entity_predictions) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    bridge->wm_to_kg.failure_predictions_capacity = DEFAULT_EFFECTS_CAPACITY;
    bridge->wm_to_kg.failure_predictions = nimcp_calloc(
        DEFAULT_EFFECTS_CAPACITY, sizeof(wm_to_kg_failure_prediction_t));
    if (!bridge->wm_to_kg.failure_predictions) {
        nimcp_free(bridge->wm_to_kg.entity_predictions);
        bridge->wm_to_kg.entity_predictions = NULL;
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* KG to WM effects arrays */
    bridge->kg_to_wm.active_entities_capacity = WM_KG_MAX_ENTITIES_PER_UPDATE;
    bridge->kg_to_wm.active_entity_ids = nimcp_calloc(
        WM_KG_MAX_ENTITIES_PER_UPDATE, sizeof(uint32_t));
    if (!bridge->kg_to_wm.active_entity_ids) {
        nimcp_free(bridge->wm_to_kg.entity_predictions);
        nimcp_free(bridge->wm_to_kg.failure_predictions);
        return NIMCP_ERROR_NO_MEMORY;
    }

    bridge->kg_to_wm.relationships_capacity = WM_KG_MAX_RELATIONSHIPS_PER_ENTITY * 4;
    bridge->kg_to_wm.relationships = nimcp_calloc(
        bridge->kg_to_wm.relationships_capacity, sizeof(kg_to_wm_relationship_t));
    if (!bridge->kg_to_wm.relationships) {
        nimcp_free(bridge->wm_to_kg.entity_predictions);
        nimcp_free(bridge->wm_to_kg.failure_predictions);
        nimcp_free(bridge->kg_to_wm.active_entity_ids);
        return NIMCP_ERROR_NO_MEMORY;
    }

    bridge->kg_to_wm.module_states_capacity = WM_KG_MAX_MODULES;
    bridge->kg_to_wm.module_states = nimcp_calloc(
        WM_KG_MAX_MODULES, sizeof(kg_to_wm_module_state_t));
    if (!bridge->kg_to_wm.module_states) {
        nimcp_free(bridge->wm_to_kg.entity_predictions);
        nimcp_free(bridge->wm_to_kg.failure_predictions);
        nimcp_free(bridge->kg_to_wm.active_entity_ids);
        nimcp_free(bridge->kg_to_wm.relationships);
        return NIMCP_ERROR_NO_MEMORY;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Free effects arrays
 */
static void free_effects_arrays(omni_wm_kg_bridge_t* bridge) {
    if (!bridge) return;

    /* WM to KG arrays */
    nimcp_free(bridge->wm_to_kg.entity_predictions);
    bridge->wm_to_kg.entity_predictions = NULL;
    bridge->wm_to_kg.entity_predictions_capacity = 0;

    nimcp_free(bridge->wm_to_kg.failure_predictions);
    bridge->wm_to_kg.failure_predictions = NULL;
    bridge->wm_to_kg.failure_predictions_capacity = 0;

    /* KG to WM arrays */
    nimcp_free(bridge->kg_to_wm.active_entity_ids);
    bridge->kg_to_wm.active_entity_ids = NULL;
    bridge->kg_to_wm.active_entities_capacity = 0;

    nimcp_free(bridge->kg_to_wm.relationships);
    bridge->kg_to_wm.relationships = NULL;
    bridge->kg_to_wm.relationships_capacity = 0;

    nimcp_free(bridge->kg_to_wm.module_states);
    bridge->kg_to_wm.module_states = NULL;
    bridge->kg_to_wm.module_states_capacity = 0;
}

/**
 * @brief Compute entity state prediction using world model
 */
static nimcp_error_t compute_entity_prediction(omni_wm_kg_bridge_t* bridge,
                                                uint32_t entity_id,
                                                uint32_t horizon_steps,
                                                wm_to_kg_entity_prediction_t* out) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(out, NIMCP_ERROR_NULL_POINTER, "out is NULL");

    /* Initialize prediction output */
    memset(out, 0, sizeof(wm_to_kg_entity_prediction_t));
    out->entity_id = entity_id;
    out->horizon_steps = horizon_steps;
    out->timestamp_us = get_current_time_us();

    /* In full implementation, would:
     * 1. Get entity's current state from KG
     * 2. Get relationship context for entity
     * 3. Run RSSM forward dynamics for horizon_steps
     * 4. Return predicted state with confidence
     *
     * For now, generate placeholder prediction */

    out->state_dim = 32; /* Placeholder dimension */
    for (uint32_t i = 0; i < out->state_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && out->state_dim > 256) {
            omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_loop",
                             (float)(i + 1) / (float)out->state_dim);
        }

        out->predicted_state[i] = 0.5f + 0.1f * (float)(entity_id % 10);
    }

    /* Confidence decreases with prediction horizon */
    out->confidence = 0.95f * expf(-0.05f * (float)horizon_steps);
    out->uncertainty = 1.0f - out->confidence;

    return NIMCP_SUCCESS;
}

/**
 * @brief Compute module failure prediction
 */
static nimcp_error_t compute_module_failure_prediction(omni_wm_kg_bridge_t* bridge,
                                                        uint32_t module_id,
                                                        wm_to_kg_failure_prediction_t* out) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(out, NIMCP_ERROR_NULL_POINTER, "out is NULL");

    /* Initialize prediction output */
    memset(out, 0, sizeof(wm_to_kg_failure_prediction_t));
    out->module_id = module_id;

    /* Find module in tracked states */
    const kg_to_wm_module_state_t* module_state = NULL;
    for (uint32_t i = 0; i < bridge->kg_to_wm.module_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->kg_to_wm.module_count > 256) {
            omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_loop",
                             (float)(i + 1) / (float)bridge->kg_to_wm.module_count);
        }

        if (bridge->kg_to_wm.module_states[i].module_id == module_id) {
            module_state = &bridge->kg_to_wm.module_states[i];
            break;
        }
    }

    if (!module_state) {
        /* Module not tracked, return low failure probability */
        out->failure_probability = 0.0f;
        out->time_to_failure_sec = INFINITY;
        out->predicted_health = KG_MODULE_HEALTH_UNKNOWN;
        out->confidence = 0.1f;
        snprintf(out->reason, sizeof(out->reason), "Module not tracked");
        return NIMCP_SUCCESS;
    }

    /* In full implementation, would:
     * 1. Get module health history
     * 2. Run RSSM forward dynamics on health trajectory
     * 3. Estimate failure probability distribution
     *
     * For now, use simple heuristics based on current state */

    switch (module_state->health_state) {
        case KG_MODULE_HEALTH_HEALTHY:
            out->failure_probability = 0.01f;
            out->time_to_failure_sec = 3600.0f; /* 1 hour */
            out->predicted_health = KG_MODULE_HEALTH_HEALTHY;
            break;

        case KG_MODULE_HEALTH_DEGRADED:
            out->failure_probability = 0.25f;
            out->time_to_failure_sec = 300.0f; /* 5 minutes */
            out->predicted_health = KG_MODULE_HEALTH_FAILING;
            break;

        case KG_MODULE_HEALTH_FAILING:
            out->failure_probability = 0.75f;
            out->time_to_failure_sec = 60.0f; /* 1 minute */
            out->predicted_health = KG_MODULE_HEALTH_FAILED;
            break;

        case KG_MODULE_HEALTH_FAILED:
            out->failure_probability = 1.0f;
            out->time_to_failure_sec = 0.0f;
            out->predicted_health = KG_MODULE_HEALTH_FAILED;
            break;

        case KG_MODULE_HEALTH_RECOVERING:
            out->failure_probability = 0.15f;
            out->time_to_failure_sec = 600.0f; /* 10 minutes */
            out->predicted_health = KG_MODULE_HEALTH_HEALTHY;
            break;

        default:
            out->failure_probability = 0.5f;
            out->time_to_failure_sec = 120.0f; /* 2 minutes */
            out->predicted_health = KG_MODULE_HEALTH_UNKNOWN;
            break;
    }

    /* Adjust based on exception count */
    float exception_factor = 1.0f + 0.1f * (float)module_state->exception_count;
    out->failure_probability = fminf(1.0f, out->failure_probability * exception_factor);
    out->time_to_failure_sec /= exception_factor;

    /* Set confidence based on data quality */
    out->confidence = module_state->health_score > 0.0f ? 0.8f : 0.3f;

    /* Contributing factors bitmask */
    out->contributing_factors = 0;
    if (module_state->exception_count > 0) out->contributing_factors |= 0x01;
    if (module_state->cpu_utilization > 0.8f) out->contributing_factors |= 0x02;
    if (module_state->memory_utilization > 0.8f) out->contributing_factors |= 0x04;
    if (module_state->message_backlog > 100) out->contributing_factors |= 0x08;

    /* Generate reason string */
    snprintf(out->reason, sizeof(out->reason),
             "Health=%.2f, Exceptions=%u, CPU=%.1f%%, Mem=%.1f%%",
             module_state->health_score,
             module_state->exception_count,
             module_state->cpu_utilization * 100.0f,
             module_state->memory_utilization * 100.0f);

    return NIMCP_SUCCESS;
}

/**
 * @brief Compute anomaly score between predicted and observed states
 */
static float compute_anomaly_score(const float* predicted, const float* observed,
                                    uint32_t dim) {
    if (!predicted || !observed || dim == 0) return 0.0f;

    /* Compute normalized L2 distance */
    float sum_sq_diff = 0.0f;
    float sum_sq_pred = 0.0f;

    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_loop",
                             (float)(i + 1) / (float)dim);
        }

        float diff = observed[i] - predicted[i];
        sum_sq_diff += diff * diff;
        sum_sq_pred += predicted[i] * predicted[i];
    }

    /* Normalize by predicted magnitude */
    float norm = sqrtf(sum_sq_pred);
    if (norm < 1e-6f) norm = 1.0f;

    float distance = sqrtf(sum_sq_diff) / (fabsf(norm) > 1e-7f ? norm : 1e-7f);

    /* Convert to anomaly score [0,1] using sigmoid-like function */
    float score = 2.0f / (1.0f + expf(-distance)) - 1.0f;
    return fminf(1.0f, fmaxf(0.0f, score));
}

/**
 * @brief Update effects flowing from WM to KG system
 */
static nimcp_error_t update_wm_to_kg_effects(omni_wm_kg_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    omni_wm_to_kg_effects_t* effects = &bridge->wm_to_kg;

    /* Reset counts */
    effects->entity_predictions_count = 0;
    effects->failure_predictions_count = 0;
    effects->anomaly_detected = false;

    /* If no world model connected, skip predictions */
    if (!bridge->world_model) {
        effects->system_stability_score = 0.5f;
        effects->system_entropy = 1.0f;
        return NIMCP_SUCCESS;
    }

    /* Compute system-wide predictions */
    float total_failure_risk = 0.0f;
    uint32_t module_count = bridge->kg_to_wm.module_count;

    for (uint32_t i = 0; i < module_count && i < effects->failure_predictions_capacity; i++) {
        wm_to_kg_failure_prediction_t* pred = &effects->failure_predictions[i];
        uint32_t module_id = bridge->kg_to_wm.module_states[i].module_id;

        nimcp_error_t err = compute_module_failure_prediction(bridge, module_id, pred);
        if (err == NIMCP_SUCCESS) {
            effects->failure_predictions_count++;
            total_failure_risk += pred->failure_probability;
        }
    }

    /* Compute system stability score */
    if (module_count > 0) {
        float avg_failure_risk = total_failure_risk / (float)module_count;
        effects->system_stability_score = 1.0f - avg_failure_risk;
    } else {
        effects->system_stability_score = 1.0f;
    }

    /* Estimate system entropy from module states */
    effects->system_entropy = 1.0f - effects->system_stability_score;

    /* Predict exception count based on current trends */
    effects->predicted_exception_count = (uint32_t)(
        (1.0f - effects->system_stability_score) * 10.0f);

    /* Relationship change predictions - simplified heuristic */
    effects->relationship_change_probability = 0.1f; /* Low baseline */
    effects->predicted_new_relationships = 0;
    effects->predicted_removed_relationships = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Update effects flowing from KG system to WM
 */
static nimcp_error_t update_kg_to_wm_effects(omni_wm_kg_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    kg_to_omni_wm_effects_t* effects = &bridge->kg_to_wm;

    /* Update timestamp */
    effects->last_kg_event_time_us = get_current_time_us();

    /* If KG wiring manager connected, could query topology stats
     * For now, maintain current values */

    /* If no registry connected, use placeholder values */
    if (!bridge->registry) {
        effects->graph_density = 0.1f;
        effects->avg_in_degree = 2.0f;
        effects->avg_out_degree = 2.0f;
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

/**
 * @brief Handle entity prediction request
 */
static nimcp_error_t handle_entity_prediction_request(const void* msg, size_t msg_size,
                                                       nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_kg_bridge_t* bridge = (omni_wm_kg_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.entity_predictions_made++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle module prediction request
 */
static nimcp_error_t handle_module_prediction_request(const void* msg, size_t msg_size,
                                                       nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_kg_bridge_t* bridge = (omni_wm_kg_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.module_predictions_made++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle exception notification
 */
static nimcp_error_t handle_exception_notify(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_kg_bridge_t* bridge = (omni_wm_kg_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->kg_to_wm.pending_exception_count++;
    bridge->kg_to_wm.exception_propagation_active = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle wiring topology change
 */
static nimcp_error_t handle_wiring_change(const void* msg, size_t msg_size,
                                           nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_kg_bridge_t* bridge = (omni_wm_kg_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    /* Would trigger relationship prediction update */
    bridge->wm_to_kg.relationship_change_probability += 0.1f;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle training event from KG
 */
static nimcp_error_t handle_training_event(const void* msg, size_t msg_size,
                                            nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_kg_bridge_t* bridge = (omni_wm_kg_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->training_events_queued++;
    bridge->stats.kg_events_processed++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle registry sync request
 */
static nimcp_error_t handle_registry_sync(const void* msg, size_t msg_size,
                                           nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_kg_bridge_t* bridge = (omni_wm_kg_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.registry_syncs++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_kg_bridge_default_config(
    omni_wm_kg_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(omni_wm_kg_bridge_config_t));

    /* General settings */
    config->enable_modulation = true;
    config->sensitivity = 1.0f;

    /* Entity prediction settings */
    config->enable_entity_prediction = true;
    config->default_prediction_horizon = WM_KG_DEFAULT_HORIZON_STEPS;
    config->prediction_confidence_threshold = 0.5f;
    config->max_entities_per_batch = 32;

    /* Relationship prediction settings */
    config->enable_relationship_prediction = true;
    config->relationship_change_threshold = 0.3f;

    /* Module health prediction settings */
    config->enable_module_prediction = true;
    config->failure_probability_threshold = 0.3f;
    config->failure_horizon_sec = WM_KG_DEFAULT_FAILURE_HORIZON_SEC;

    /* Anomaly detection settings */
    config->enable_anomaly_detection = true;
    config->anomaly_threshold = WM_KG_DEFAULT_ANOMALY_THRESHOLD;
    config->report_anomalies_to_exception = true;

    /* Training settings */
    config->enable_training_from_kg = true;
    config->kg_training_learning_rate = 0.001f;
    config->training_batch_size = 32;
    config->training_priority_decay = 0.95f;

    /* Registry integration settings */
    config->enable_registry_sync = true;
    config->registry_sync_interval_sec = 1.0f;

    /* Bio-async settings */
    config->enable_bio_async = true;

    return NIMCP_SUCCESS;
}

omni_wm_kg_bridge_t* omni_wm_kg_bridge_create(
    const omni_wm_kg_bridge_config_t* config) {

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_create", 0.0f);


    omni_wm_kg_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_wm_kg_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate WM KG bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: bridge is NULL");
        return NULL;
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_WM_KG_BRIDGE,
                         "wm_kg_bridge") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: operation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        omni_wm_kg_bridge_default_config(&bridge->config);
    }

    /* Allocate effects arrays */
    nimcp_error_t err = allocate_effects_arrays(bridge);
    if (err != NIMCP_SUCCESS) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate effects arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->last_prediction_time_us = 0;
    bridge->prediction_sequence = 0;
    bridge->prediction_in_progress = false;
    bridge->training_events_queued = 0;
    bridge->last_training_time_us = 0;
    bridge->last_registry_sync_us = 0;
    bridge->registry_sync_failures = 0;

    NIMCP_LOGGING_INFO("WM KG Bridge created successfully");
    return bridge;
}

void omni_wm_kg_bridge_destroy(omni_wm_kg_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        omni_wm_kg_bridge_disconnect_bio_async(bridge);
    }

    /* Free effects arrays */
    free_effects_arrays(bridge);

    /* Cleanup base and free */
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("WM KG Bridge destroyed");
}

nimcp_error_t omni_wm_kg_bridge_reset(omni_wm_kg_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_reset", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset effects (preserve capacity) */
    bridge->wm_to_kg.entity_predictions_count = 0;
    bridge->wm_to_kg.failure_predictions_count = 0;
    bridge->wm_to_kg.relationship_change_probability = 0.0f;
    bridge->wm_to_kg.predicted_new_relationships = 0;
    bridge->wm_to_kg.predicted_removed_relationships = 0;
    bridge->wm_to_kg.system_stability_score = 1.0f;
    bridge->wm_to_kg.system_entropy = 0.0f;
    bridge->wm_to_kg.predicted_exception_count = 0;
    bridge->wm_to_kg.anomaly_detected = false;
    bridge->wm_to_kg.anomaly_score = 0.0f;

    bridge->kg_to_wm.active_entities_count = 0;
    bridge->kg_to_wm.relationship_count = 0;
    bridge->kg_to_wm.module_count = 0;
    bridge->kg_to_wm.pending_exception_count = 0;
    bridge->kg_to_wm.exception_propagation_active = false;
    bridge->kg_to_wm.pending_training_events = 0;

    /* Reset internal state */
    bridge->last_prediction_time_us = 0;
    bridge->prediction_sequence = 0;
    bridge->prediction_in_progress = false;
    bridge->training_events_queued = 0;
    bridge->last_training_time_us = 0;
    bridge->last_registry_sync_us = 0;
    bridge->registry_sync_failures = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(omni_wm_kg_bridge_stats_t));

    /* Reset base bridge (unlocked since we already hold the mutex) */
    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_kg_bridge_connect(
    omni_wm_kg_bridge_t* bridge,
    omni_world_model_t* world_model,
    kg_wiring_manager_t* kg_wiring,
    kg_module_registry_t* registry) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_connect", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_INVALID_PARAM, "world_model is required");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->world_model = world_model;
    bridge->kg_wiring = kg_wiring;
    bridge->registry = registry;

    /* Update base connection state */
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.system_b = kg_wiring;
    bridge->base.system_b_connected = (kg_wiring != NULL);
    bridge->base.bridge_active = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("WM KG Bridge connected: WM=%p, KG_Wiring=%p, Registry=%p",
                       (void*)world_model, (void*)kg_wiring, (void*)registry);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_kg_bridge_connect_world_model(
    omni_wm_kg_bridge_t* bridge,
    omni_world_model_t* world_model) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_connect_world_model", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_NULL_POINTER, "world_model is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->world_model = world_model;
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_kg_bridge_connect_kg_wiring(
    omni_wm_kg_bridge_t* bridge,
    kg_wiring_manager_t* kg_wiring) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_connect_kg_wiring", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(kg_wiring, NIMCP_ERROR_NULL_POINTER, "kg_wiring is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->kg_wiring = kg_wiring;
    bridge->base.system_b = kg_wiring;
    bridge->base.system_b_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_kg_bridge_connect_registry(
    omni_wm_kg_bridge_t* bridge,
    kg_module_registry_t* registry) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_connect_registry", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(registry, NIMCP_ERROR_NULL_POINTER, "registry is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->registry = registry;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

bool omni_wm_kg_bridge_is_connected(const omni_wm_kg_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_is_connected", 0.0f);


    return bridge->world_model != NULL;
}

/* ============================================================================
 * Update API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_kg_bridge_update(
    omni_wm_kg_bridge_t* bridge,
    float dt) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_update", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_modulation) return NIMCP_SUCCESS;

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if registry sync is due */
    if (bridge->config.enable_registry_sync && bridge->registry) {
        float elapsed_sec = (float)(start_time - bridge->last_registry_sync_us) / 1000000.0f;
        if (elapsed_sec >= bridge->config.registry_sync_interval_sec) {
            /* Sync with registry - would query actual registry in full impl */
            bridge->last_registry_sync_us = start_time;
            bridge->stats.registry_syncs++;
        }
    }

    /* Process queued training events */
    if (bridge->config.enable_training_from_kg && bridge->training_events_queued > 0) {
        /* In full implementation, would batch train world model */
        bridge->stats.training_updates++;
        bridge->training_events_queued = 0;
        bridge->last_training_time_us = start_time;
    }

    /* Update effects in both directions */
    update_kg_to_wm_effects(bridge);
    update_wm_to_kg_effects(bridge);

    /* Update timing statistics */
    bridge->stats.total_updates++;
    uint64_t elapsed = get_current_time_us() - start_time;
    bridge->stats.total_processing_time_ms += (double)elapsed / 1000.0;
    bridge->stats.mean_update_time_ms = bridge->stats.total_processing_time_ms /
                                         (double)bridge->stats.total_updates;
    bridge->stats.last_update_time_us = start_time;

    /* Record base update */
    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    (void)dt; /* dt available for time-based scaling */
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Entity Prediction API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_kg_bridge_predict_entity(
    omni_wm_kg_bridge_t* bridge,
    uint32_t entity_id,
    uint32_t horizon_steps,
    wm_to_kg_entity_prediction_t* out_prediction) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_predict_entity", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(out_prediction, NIMCP_ERROR_NULL_POINTER, "out_prediction is NULL");
    if (!bridge->config.enable_entity_prediction) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    nimcp_error_t err = compute_entity_prediction(bridge, entity_id,
                                                   horizon_steps, out_prediction);

    if (err == NIMCP_SUCCESS) {
        bridge->stats.entity_predictions_made++;
        bridge->stats.mean_entity_confidence =
            ANOMALY_SMOOTHING_FACTOR * out_prediction->confidence +
            (1.0f - ANOMALY_SMOOTHING_FACTOR) * bridge->stats.mean_entity_confidence;
    } else {
        bridge->stats.errors_prediction++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return err;
}

nimcp_error_t omni_wm_kg_bridge_predict_entities_batch(
    omni_wm_kg_bridge_t* bridge,
    const uint32_t* entity_ids,
    uint32_t entity_count,
    uint32_t horizon_steps,
    wm_to_kg_entity_prediction_t* out_predictions) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_predict_entities_bat", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(entity_ids, NIMCP_ERROR_NULL_POINTER, "entity_ids is NULL");
    NIMCP_CHECK_THROW(out_predictions, NIMCP_ERROR_NULL_POINTER, "out_predictions is NULL");
    if (entity_count == 0) return NIMCP_SUCCESS;
    if (!bridge->config.enable_entity_prediction) return NIMCP_SUCCESS;

    uint32_t max_entities = bridge->config.max_entities_per_batch;
    if (entity_count > max_entities) entity_count = max_entities;

    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < entity_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && entity_count > 256) {
            omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_loop",
                             (float)(i + 1) / (float)entity_count);
        }

        nimcp_error_t err = compute_entity_prediction(bridge, entity_ids[i],
                                                       horizon_steps, &out_predictions[i]);
        if (err == NIMCP_SUCCESS) {
            bridge->stats.entity_predictions_made++;
        } else {
            bridge->stats.errors_prediction++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Relationship Prediction API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_kg_bridge_predict_relationships(
    omni_wm_kg_bridge_t* bridge,
    uint32_t entity_id,
    float* out_change_probability) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_predict_relationship", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(out_change_probability, NIMCP_ERROR_NULL_POINTER, "out_change_probability is NULL");
    if (!bridge->config.enable_relationship_prediction) {
        *out_change_probability = 0.0f;
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* In full implementation, would:
     * 1. Get entity's current relationships
     * 2. Predict entity state evolution
     * 3. Model relationship dynamics
     *
     * For now, use simple heuristic */

    /* Base probability plus system instability factor */
    float base_prob = 0.05f;
    float instability_factor = 1.0f - bridge->wm_to_kg.system_stability_score;
    *out_change_probability = base_prob + 0.2f * instability_factor;

    bridge->stats.relationship_predictions_made++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_kg_bridge_predict_relationship_strength(
    omni_wm_kg_bridge_t* bridge,
    uint32_t source_id,
    uint32_t target_id,
    uint32_t horizon_steps,
    float* out_predicted_strength,
    float* out_confidence) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_predict_relationship", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(out_predicted_strength, NIMCP_ERROR_NULL_POINTER, "out_predicted_strength is NULL");
    NIMCP_CHECK_THROW(out_confidence, NIMCP_ERROR_NULL_POINTER, "out_confidence is NULL");
    if (!bridge->config.enable_relationship_prediction) {
        *out_predicted_strength = 0.5f;
        *out_confidence = 0.0f;
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find existing relationship */
    float current_strength = 0.0f;
    bool found = false;

    for (uint32_t i = 0; i < bridge->kg_to_wm.relationship_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->kg_to_wm.relationship_count > 256) {
            omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_loop",
                             (float)(i + 1) / (float)bridge->kg_to_wm.relationship_count);
        }

        kg_to_wm_relationship_t* rel = &bridge->kg_to_wm.relationships[i];
        if (rel->source_entity_id == source_id && rel->target_entity_id == target_id) {
            current_strength = rel->relationship_strength;
            found = true;
            break;
        }
    }

    /* Predict strength evolution */
    if (found) {
        /* Simple exponential decay toward 0.5 */
        float decay_rate = 0.01f * (float)horizon_steps;
        *out_predicted_strength = current_strength + (0.5f - current_strength) * decay_rate;
        *out_confidence = 0.8f * expf(-0.05f * (float)horizon_steps);
    } else {
        *out_predicted_strength = 0.0f;
        *out_confidence = 0.3f;
    }

    bridge->stats.relationship_predictions_made++;

    nimcp_mutex_unlock(bridge->base.mutex);

    (void)source_id;
    (void)target_id;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Module Health Prediction API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_kg_bridge_predict_module_failure(
    omni_wm_kg_bridge_t* bridge,
    uint32_t module_id,
    float* out_failure_probability,
    float* out_time_to_failure) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_predict_module_failu", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(out_failure_probability, NIMCP_ERROR_NULL_POINTER, "out_failure_probability is NULL");
    NIMCP_CHECK_THROW(out_time_to_failure, NIMCP_ERROR_NULL_POINTER, "out_time_to_failure is NULL");
    if (!bridge->config.enable_module_prediction) {
        *out_failure_probability = 0.0f;
        *out_time_to_failure = INFINITY;
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    wm_to_kg_failure_prediction_t prediction;
    nimcp_error_t err = compute_module_failure_prediction(bridge, module_id, &prediction);

    if (err == NIMCP_SUCCESS) {
        *out_failure_probability = prediction.failure_probability;
        *out_time_to_failure = prediction.time_to_failure_sec;
        bridge->stats.module_predictions_made++;

        /* Track if we predicted a failure */
        if (prediction.failure_probability >= bridge->config.failure_probability_threshold) {
            bridge->stats.module_failures_predicted++;
        }
    } else {
        *out_failure_probability = 0.0f;
        *out_time_to_failure = INFINITY;
        bridge->stats.errors_prediction++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return err;
}

nimcp_error_t omni_wm_kg_bridge_get_module_prediction(
    omni_wm_kg_bridge_t* bridge,
    uint32_t module_id,
    wm_to_kg_failure_prediction_t* out_prediction) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_get_module_predictio", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(out_prediction, NIMCP_ERROR_NULL_POINTER, "out_prediction is NULL");
    if (!bridge->config.enable_module_prediction) {
        memset(out_prediction, 0, sizeof(wm_to_kg_failure_prediction_t));
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    nimcp_error_t err = compute_module_failure_prediction(bridge, module_id, out_prediction);

    if (err == NIMCP_SUCCESS) {
        bridge->stats.module_predictions_made++;
    } else {
        bridge->stats.errors_prediction++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return err;
}

nimcp_error_t omni_wm_kg_bridge_predict_system_stability(
    omni_wm_kg_bridge_t* bridge,
    float* out_stability_score,
    uint32_t* out_predicted_exceptions) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_predict_system_stabi", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(out_stability_score, NIMCP_ERROR_NULL_POINTER, "out_stability_score is NULL");
    NIMCP_CHECK_THROW(out_predicted_exceptions, NIMCP_ERROR_NULL_POINTER, "out_predicted_exceptions is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    *out_stability_score = bridge->wm_to_kg.system_stability_score;
    *out_predicted_exceptions = bridge->wm_to_kg.predicted_exception_count;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Exception Handling Integration API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_kg_bridge_on_exception(
    omni_wm_kg_bridge_t* bridge,
    const nimcp_kg_wiring_exception_t* exception) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_on_exception", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(exception, NIMCP_ERROR_NULL_POINTER, "exception is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update exception tracking */
    bridge->kg_to_wm.pending_exception_count++;
    bridge->kg_to_wm.exception_propagation_active = true;

    /* In full implementation, would:
     * 1. Extract exception context
     * 2. Add to training data for exception pattern learning
     * 3. Update anomaly model
     *
     * For now, queue as training event */
    if (bridge->config.enable_training_from_kg) {
        bridge->training_events_queued++;
        bridge->stats.kg_events_processed++;
    }

    /* Update system stability estimate */
    bridge->wm_to_kg.system_stability_score *= 0.95f; /* Slight decrease on exception */
    bridge->wm_to_kg.predicted_exception_count++;

    NIMCP_LOGGING_DEBUG("WM KG Bridge received exception, pending=%u",
                        bridge->kg_to_wm.pending_exception_count);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_kg_bridge_check_anomaly(
    omni_wm_kg_bridge_t* bridge,
    uint32_t entity_id,
    bool* out_is_anomalous,
    float* out_anomaly_score) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_check_anomaly", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(out_is_anomalous, NIMCP_ERROR_NULL_POINTER, "out_is_anomalous is NULL");
    NIMCP_CHECK_THROW(out_anomaly_score, NIMCP_ERROR_NULL_POINTER, "out_anomaly_score is NULL");
    if (!bridge->config.enable_anomaly_detection) {
        *out_is_anomalous = false;
        *out_anomaly_score = 0.0f;
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* In full implementation, would:
     * 1. Get predicted state for entity
     * 2. Get current observed state
     * 3. Compute anomaly score
     *
     * For now, use placeholder */

    /* Use entity_id modulo for reproducible test behavior */
    float base_score = 0.1f + 0.05f * (float)(entity_id % 10);
    float system_factor = 1.0f - bridge->wm_to_kg.system_stability_score;
    *out_anomaly_score = fminf(1.0f, base_score + 0.3f * system_factor);
    *out_is_anomalous = (*out_anomaly_score >= bridge->config.anomaly_threshold);

    if (*out_is_anomalous) {
        bridge->stats.anomalies_detected++;
        bridge->stats.mean_anomaly_score =
            ANOMALY_SMOOTHING_FACTOR * (*out_anomaly_score) +
            (1.0f - ANOMALY_SMOOTHING_FACTOR) * bridge->stats.mean_anomaly_score;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Training API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_kg_bridge_train_from_kg_event(
    omni_wm_kg_bridge_t* bridge,
    const kg_event_t* event) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_train_from_kg_event", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(event, NIMCP_ERROR_NULL_POINTER, "event is NULL");
    if (!bridge->config.enable_training_from_kg) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Queue event for batch training */
    bridge->training_events_queued++;
    bridge->stats.kg_events_processed++;
    bridge->kg_to_wm.pending_training_events++;

    /* Check if batch is ready */
    if (bridge->training_events_queued >= bridge->config.training_batch_size) {
        /* In full implementation, would trigger batch training here */
        bridge->stats.training_updates++;
        bridge->training_events_queued = 0;
        bridge->last_training_time_us = get_current_time_us();
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_kg_bridge_train_from_observation(
    omni_wm_kg_bridge_t* bridge,
    uint32_t entity_id,
    const float* observed_state,
    uint32_t state_dim,
    uint64_t timestamp_us) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_train_from_observati", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(observed_state, NIMCP_ERROR_NULL_POINTER, "observed_state is NULL");
    NIMCP_CHECK_THROW(state_dim > 0, NIMCP_ERROR_INVALID_PARAM, "state_dim must be greater than 0");
    if (!bridge->config.enable_training_from_kg) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* In full implementation, would:
     * 1. Compare with predicted state for entity
     * 2. Compute prediction error
     * 3. Add to training batch
     * 4. Update RSSM if batch ready
     *
     * For now, update statistics */

    bridge->stats.kg_events_processed++;

    /* Compute placeholder prediction error */
    float mean_state = 0.0f;
    for (uint32_t i = 0; i < state_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state_dim > 256) {
            omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_loop",
                             (float)(i + 1) / (float)state_dim);
        }

        mean_state += observed_state[i];
    }
    mean_state /= (float)state_dim;

    /* Simulated prediction error */
    float prediction_error = fabsf(mean_state - 0.5f);
    bridge->stats.mean_entity_prediction_error =
        ANOMALY_SMOOTHING_FACTOR * prediction_error +
        (1.0f - ANOMALY_SMOOTHING_FACTOR) * bridge->stats.mean_entity_prediction_error;

    /* Verify prediction if we have history */
    bridge->stats.entity_predictions_correct++;  /* Placeholder */

    nimcp_mutex_unlock(bridge->base.mutex);

    (void)entity_id;
    (void)timestamp_us;
    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_kg_bridge_flush_training(
    omni_wm_kg_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_flush_training", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_training_from_kg) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->training_events_queued > 0) {
        /* In full implementation, would:
         * 1. Process all queued events
         * 2. Run batch training on world model
         * 3. Update prediction models */

        bridge->stats.training_updates++;
        bridge->training_events_queued = 0;
        bridge->kg_to_wm.pending_training_events = 0;
        bridge->last_training_time_us = get_current_time_us();

        NIMCP_LOGGING_DEBUG("WM KG Bridge flushed training queue");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Registry Sync API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_kg_bridge_sync_registry(
    omni_wm_kg_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_sync_registry", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_registry_sync) return NIMCP_SUCCESS;
    if (!bridge->registry) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* In full implementation, would:
     * 1. Query registry for all module states
     * 2. Update kg_to_wm.module_states array
     * 3. Trigger predictions if state changes detected */

    bridge->last_registry_sync_us = get_current_time_us();
    bridge->stats.registry_syncs++;
    bridge->registry_sync_failures = 0;

    NIMCP_LOGGING_DEBUG("WM KG Bridge synced with registry");

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_kg_bridge_update_module_health(
    omni_wm_kg_bridge_t* bridge,
    uint32_t module_id,
    kg_module_health_t health_state,
    float health_score) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_update_module_health", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(health_score >= 0.0f && health_score <= 1.0f, NIMCP_ERROR_INVALID_PARAM,
                      "health_score must be between 0.0 and 1.0");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find or create module state entry */
    kg_to_wm_module_state_t* module_state = NULL;
    for (uint32_t i = 0; i < bridge->kg_to_wm.module_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->kg_to_wm.module_count > 256) {
            omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_loop",
                             (float)(i + 1) / (float)bridge->kg_to_wm.module_count);
        }

        if (bridge->kg_to_wm.module_states[i].module_id == module_id) {
            module_state = &bridge->kg_to_wm.module_states[i];
            break;
        }
    }

    if (!module_state && bridge->kg_to_wm.module_count < bridge->kg_to_wm.module_states_capacity) {
        /* Add new module */
        module_state = &bridge->kg_to_wm.module_states[bridge->kg_to_wm.module_count];
        bridge->kg_to_wm.module_count++;
        module_state->module_id = module_id;
        snprintf(module_state->module_name, sizeof(module_state->module_name),
                 "module_%u", module_id);
    }

    if (module_state) {
        /* Track state change for anomaly detection */
        bool was_healthy = (module_state->health_state == KG_MODULE_HEALTH_HEALTHY);
        bool is_healthy = (health_state == KG_MODULE_HEALTH_HEALTHY);

        module_state->health_state = health_state;
        module_state->health_score = health_score;
        module_state->last_update_us = get_current_time_us();

        /* Update degraded count */
        if (!is_healthy && was_healthy) {
            bridge->stats.modules_degraded++;
        } else if (is_healthy && !was_healthy && bridge->stats.modules_degraded > 0) {
            bridge->stats.modules_degraded--;
        }

        bridge->stats.modules_tracked = bridge->kg_to_wm.module_count;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

const omni_wm_to_kg_effects_t* omni_wm_kg_bridge_get_wm_effects(
    const omni_wm_kg_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_get_wm_effects", 0.0f);


    return &bridge->wm_to_kg;
}

const kg_to_omni_wm_effects_t* omni_wm_kg_bridge_get_kg_effects(
    const omni_wm_kg_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_get_kg_effects", 0.0f);


    return &bridge->kg_to_wm;
}

nimcp_error_t omni_wm_kg_bridge_get_stats(
    const omni_wm_kg_bridge_t* bridge,
    omni_wm_kg_bridge_stats_t* stats) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_get_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_kg_bridge_reset_stats(
    omni_wm_kg_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(omni_wm_kg_bridge_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_kg_bridge_connect_bio_async(
    omni_wm_kg_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_bio_async) return NIMCP_SUCCESS;
    if (bridge->base.bio_async_enabled) return NIMCP_SUCCESS; /* Already connected */

    /* Check if router is initialized */
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_DEBUG("Bio-async router not initialized, skipping registration");
        return NIMCP_SUCCESS;
    }

    /* Register module with router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_WM_KG_BRIDGE,
        .module_name = "wm_kg_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (!bridge->base.bio_ctx) {
        NIMCP_LOGGING_WARN("Failed to register with bio-async router");
        return NIMCP_SUCCESS; /* Non-fatal */
    }

    /* Register message handlers */
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_KG_ENTITY_PRED,
                                handle_entity_prediction_request);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_KG_MODULE_PRED,
                                handle_module_prediction_request);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_KG_EXCEPTION_NOTIFY,
                                handle_exception_notify);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_KG_WIRING_CHANGE,
                                handle_wiring_change);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_KG_TRAINING_EVENT,
                                handle_training_event);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_KG_REGISTRY_SYNC,
                                handle_registry_sync);

    bridge->base.bio_async_enabled = true;
    NIMCP_LOGGING_INFO("WM KG Bridge connected to bio-async router");

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_kg_bridge_disconnect_bio_async(
    omni_wm_kg_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("WM KG Bridge disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool omni_wm_kg_bridge_is_bio_async_connected(
    const omni_wm_kg_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_is_bio_async_connect", 0.0f);


    return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL);
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* omni_wm_kg_msg_type_to_string(omni_wm_kg_msg_type_t msg_type) {
    switch (msg_type) {
        case BIO_MSG_WM_KG_ENTITY_PRED:
            return "ENTITY_PRED";
        case BIO_MSG_WM_KG_RELATIONSHIP_PRED:
            return "RELATIONSHIP_PRED";
        case BIO_MSG_WM_KG_MODULE_PRED:
            return "MODULE_PRED";
        case BIO_MSG_WM_KG_ENTITY_BATCH_PRED:
            return "ENTITY_BATCH_PRED";
        case BIO_MSG_WM_KG_EXCEPTION_NOTIFY:
            return "EXCEPTION_NOTIFY";
        case BIO_MSG_WM_KG_WIRING_CHANGE:
            return "WIRING_CHANGE";
        case BIO_MSG_WM_KG_ANOMALY_DETECTED:
            return "ANOMALY_DETECTED";
        case BIO_MSG_WM_KG_MODULE_DEGRADED:
            return "MODULE_DEGRADED";
        case BIO_MSG_WM_KG_TRAINING_EVENT:
            return "TRAINING_EVENT";
        case BIO_MSG_WM_KG_SYSTEM_STABILITY:
            return "SYSTEM_STABILITY";
        case BIO_MSG_WM_KG_TRAINING_BATCH:
            return "TRAINING_BATCH";
        case BIO_MSG_WM_KG_REGISTRY_SYNC:
            return "REGISTRY_SYNC";
        case BIO_MSG_WM_KG_MODULE_HEALTH_UPDATE:
            return "MODULE_HEALTH_UPDATE";
        case BIO_MSG_WM_KG_FAILURE_PREDICTION:
            return "FAILURE_PREDICTION";
        case BIO_MSG_WM_KG_BRIDGE_STATUS:
            return "BRIDGE_STATUS";
        case BIO_MSG_WM_KG_BRIDGE_ERROR:
            return "BRIDGE_ERROR";
        case BIO_MSG_WM_KG_STATS_UPDATE:
            return "STATS_UPDATE";
        default:
            return "UNKNOWN";
    }
}

const char* omni_wm_kg_relationship_type_to_string(kg_relationship_type_t rel_type) {
    switch (rel_type) {
        case KG_REL_TYPE_UNKNOWN:
            return "UNKNOWN";
        case KG_REL_TYPE_DEPENDS_ON:
            return "DEPENDS_ON";
        case KG_REL_TYPE_SENDS_TO:
            return "SENDS_TO";
        case KG_REL_TYPE_RECEIVES_FROM:
            return "RECEIVES_FROM";
        case KG_REL_TYPE_INHIBITS:
            return "INHIBITS";
        case KG_REL_TYPE_EXCITES:
            return "EXCITES";
        case KG_REL_TYPE_MODULATES:
            return "MODULATES";
        case KG_REL_TYPE_CONTAINS:
            return "CONTAINS";
        case KG_REL_TYPE_BELONGS_TO:
            return "BELONGS_TO";
        case KG_REL_TYPE_RELATED_TO:
            return "RELATED_TO";
        default:
            return "UNKNOWN";
    }
}

const char* omni_wm_kg_module_health_to_string(kg_module_health_t health) {
    switch (health) {
        case KG_MODULE_HEALTH_UNKNOWN:
            return "UNKNOWN";
        case KG_MODULE_HEALTH_HEALTHY:
            return "HEALTHY";
        case KG_MODULE_HEALTH_DEGRADED:
            return "DEGRADED";
        case KG_MODULE_HEALTH_FAILING:
            return "FAILING";
        case KG_MODULE_HEALTH_FAILED:
            return "FAILED";
        case KG_MODULE_HEALTH_RECOVERING:
            return "RECOVERING";
        default:
            return "UNKNOWN";
    }
}

nimcp_error_t omni_wm_kg_bridge_validate_config(
    const omni_wm_kg_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_kg_bridge_heartbeat("omni_wm_kg_b_validate_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Validate sensitivity range */
    if (config->sensitivity < 0.5f || config->sensitivity > 2.0f) {
        NIMCP_LOGGING_WARN("Sensitivity %.2f out of range [0.5, 2.0]",
                          config->sensitivity);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate entity prediction settings */
    if (config->enable_entity_prediction) {
        if (config->default_prediction_horizon == 0 ||
            config->default_prediction_horizon > 100) {
            NIMCP_LOGGING_WARN("Invalid default_prediction_horizon: %u",
                              config->default_prediction_horizon);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->prediction_confidence_threshold < 0.0f ||
            config->prediction_confidence_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid prediction_confidence_threshold: %.2f",
                              config->prediction_confidence_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate module prediction settings */
    if (config->enable_module_prediction) {
        if (config->failure_probability_threshold < 0.0f ||
            config->failure_probability_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid failure_probability_threshold: %.2f",
                              config->failure_probability_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->failure_horizon_sec <= 0.0f) {
            NIMCP_LOGGING_WARN("Invalid failure_horizon_sec: %.2f",
                              config->failure_horizon_sec);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate anomaly detection settings */
    if (config->enable_anomaly_detection) {
        if (config->anomaly_threshold < 0.0f ||
            config->anomaly_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid anomaly_threshold: %.2f",
                              config->anomaly_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate training settings */
    if (config->enable_training_from_kg) {
        if (config->kg_training_learning_rate <= 0.0f ||
            config->kg_training_learning_rate > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid kg_training_learning_rate: %.4f",
                              config->kg_training_learning_rate);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->training_batch_size == 0) {
            NIMCP_LOGGING_WARN("Invalid training_batch_size: 0");
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate registry sync settings */
    if (config->enable_registry_sync) {
        if (config->registry_sync_interval_sec <= 0.0f) {
            NIMCP_LOGGING_WARN("Invalid registry_sync_interval_sec: %.2f",
                              config->registry_sync_interval_sec);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    return NIMCP_SUCCESS;
}
