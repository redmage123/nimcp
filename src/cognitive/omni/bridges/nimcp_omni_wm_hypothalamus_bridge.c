/**
 * @file nimcp_omni_wm_hypothalamus_bridge.c
 * @brief World Model Hypothalamus Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with Hypothalamus systems
 * WHY:  Enable homeostatic-informed world modeling and prediction-driven resource planning
 * HOW:  Drive states modulate predictions; WM predicts resource availability
 *
 * IMPLEMENTATION NOTES:
 * =====================
 * This implementation integrates several key concepts:
 *
 * 1. HOMEOSTATIC PREDICTIVE PROCESSING (Seth & Friston, 2016):
 *    - Homeostatic setpoints are predictions about optimal internal states
 *    - WM enhances control by predicting future resource availability
 *
 * 2. BYRNES' STEERING SUBSYSTEM:
 *    - Drive states bias prediction priorities (alignment-safe)
 *    - Reward predictions guide learning (subject to alignment constraints)
 *
 * 3. CIRCADIAN INTEGRATION:
 *    - Time-of-day affects prediction confidence and horizon
 *    - Sleep pressure tracks and modulates WM behavior
 */

#include "cognitive/omni/bridges/nimcp_omni_wm_hypothalamus_bridge.h"
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

/* ============================================================================
 * Module-level Constants
 * ============================================================================ */

#define LOG_MODULE "wm_hypothalamus_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(omni_wm_hypothalamus_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_omni_wm_hypothalamus_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_omni_wm_hypothalamus_bridge_mesh_registry = NULL;

nimcp_error_t omni_wm_hypothalamus_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_omni_wm_hypothalamus_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "omni_wm_hypothalamus_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "omni_wm_hypothalamus_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_omni_wm_hypothalamus_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_omni_wm_hypothalamus_bridge_mesh_registry = registry;
    return err;
}

void omni_wm_hypothalamus_bridge_mesh_unregister(void) {
    if (g_omni_wm_hypothalamus_bridge_mesh_registry && g_omni_wm_hypothalamus_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_omni_wm_hypothalamus_bridge_mesh_registry, g_omni_wm_hypothalamus_bridge_mesh_id);
        g_omni_wm_hypothalamus_bridge_mesh_id = 0;
        g_omni_wm_hypothalamus_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from omni_wm_hypothalamus_bridge module (instance-level) */
static inline void omni_wm_hypothalamus_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_omni_wm_hypothalamus_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_omni_wm_hypothalamus_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_omni_wm_hypothalamus_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

void omni_wm_hypothalamus_bridge_set_instance_health_agent(
    omni_wm_hypothalamus_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "omni_wm_hypothalamus_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int omni_wm_hypothalamus_bridge_training_begin(omni_wm_hypothalamus_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_hypothalamus_bridge_training_begin: NULL argument");
        return -1;
    }
    omni_wm_hypothalamus_bridge_heartbeat_instance(g_omni_wm_hypothalamus_bridge_health_agent, "training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int omni_wm_hypothalamus_bridge_training_step(omni_wm_hypothalamus_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_hypothalamus_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_wm_hypothalamus_bridge_heartbeat_instance(g_omni_wm_hypothalamus_bridge_health_agent, "training_step", progress);
    (void)bridge;
    return 0;
}

int omni_wm_hypothalamus_bridge_training_end(omni_wm_hypothalamus_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_hypothalamus_bridge_training_end: NULL argument");
        return -1;
    }
    omni_wm_hypothalamus_bridge_heartbeat_instance(g_omni_wm_hypothalamus_bridge_health_agent, "training_end", 1.0f);
    (void)bridge;
    return 0;
}


/** Smoothing factor for stress/arousal (exponential moving average) */
#define STRESS_AROUSAL_SMOOTHING 0.1f

/** Minimum confidence for predictions to be useful */
#define MIN_PREDICTION_CONFIDENCE 0.3f

/** Conservative mode confidence scale factor */
#define CONSERVATIVE_CONFIDENCE_SCALE 0.7f

/** Conservative mode horizon scale factor */
#define CONSERVATIVE_HORIZON_SCALE 0.5f

/** Resource prediction decay per step */
#define RESOURCE_PREDICTION_DECAY NIMCP_ELIGIBILITY_DECAY_DEFAULT

/** Reward prediction error smoothing */
#define REWARD_ERROR_SMOOTHING 0.1f

/* ============================================================================
 * Internal Helper Forward Declarations
 * ============================================================================ */

static uint64_t get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static nimcp_error_t update_hypo_to_wm_effects(omni_wm_hypothalamus_bridge_t* bridge);
static nimcp_error_t update_wm_to_hypo_effects(omni_wm_hypothalamus_bridge_t* bridge);
static nimcp_error_t update_drive_states(omni_wm_hypothalamus_bridge_t* bridge);
static nimcp_error_t update_circadian_state(omni_wm_hypothalamus_bridge_t* bridge);
static nimcp_error_t check_conservative_mode(omni_wm_hypothalamus_bridge_t* bridge);
static nimcp_error_t predict_resources_internal(omni_wm_hypothalamus_bridge_t* bridge);
static float compute_drive_priority_boost(float urgency, float threshold, float strength);

/**
 * @brief Compute priority boost from drive urgency
 *
 * WHAT: Map urgency to prediction priority boost
 * WHY:  Urgent drives should increase related prediction priority
 * HOW:  Nonlinear scaling above threshold
 */
static float compute_drive_priority_boost(float urgency, float threshold, float strength) {
    if (urgency < threshold) return 1.0f;

    /* Superlinear boost above threshold */
    float excess = urgency - threshold;
    float boost = 1.0f + strength * excess * excess;
    return nimcp_clampf(boost, 1.0f, 2.0f);
}

/**
 * @brief Update drive states from connected drive system
 */
static nimcp_error_t update_drive_states(omni_wm_hypothalamus_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    hypothalamus_to_omni_wm_effects_t* effects = &bridge->hypo_to_wm;
    wm_hypothal_drive_vector_t* dv = &effects->drive_vector;

    /* If drive system connected, extract drive states */
    if (bridge->drive_system) {
        /* In full implementation, would call hypo_drive_get_system_state()
         * For now, use placeholder data */
        dv->drive_count = 9; /* HYPO_DRIVE_COUNT from drives.h */
        dv->global_arousal = 0.5f;
        dv->global_stress = bridge->current_stress_smoothed;

        /* Extract urgencies and compute priority boosts */
        for (uint32_t i = 0; i < dv->drive_count && i < WM_HYPO_MAX_DRIVES; i++) {
            dv->drives[i].drive_type = i;
            dv->drives[i].level = 0.3f + 0.1f * (float)i;
            dv->drives[i].urgency = 0.2f + 0.05f * (float)i;
            dv->drives[i].deviation_from_setpoint = 0.1f;
            dv->drives[i].time_since_satisfied = 0.0f;
            dv->drives[i].is_active = (dv->drives[i].urgency > 0.3f);

            /* Compute priority boost */
            effects->drive_priority_boost[i] = compute_drive_priority_boost(
                dv->drives[i].urgency,
                bridge->config.drive_urgency_threshold,
                bridge->config.drive_modulation_strength);
        }

        /* Determine priority drive (highest urgency) */
        float max_urgency = 0.0f;
        uint32_t max_idx = 0;
        for (uint32_t i = 0; i < dv->drive_count && i < WM_HYPO_MAX_DRIVES; i++) {
            if (dv->drives[i].urgency > max_urgency) {
                max_urgency = dv->drives[i].urgency;
                max_idx = i;
            }
        }
        dv->priority_drive = max_idx;
        effects->prediction_focus_drive = max_idx;
    } else {
        /* No drive system - use defaults */
        dv->drive_count = 0;
        dv->global_arousal = 0.5f;
        dv->global_stress = 0.3f;
    }

    dv->timestamp_us = get_current_time_us();
    bridge->stats.drive_updates_received++;

    return NIMCP_SUCCESS;
}

/**
 * @brief Update circadian state from connected circadian system
 */
static nimcp_error_t update_circadian_state(omni_wm_hypothalamus_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    hypothalamus_to_omni_wm_effects_t* effects = &bridge->hypo_to_wm;
    wm_circadian_state_t* cs = &effects->circadian_state;

    /* If circadian system connected, extract state */
    if (bridge->circadian) {
        /* In full implementation, would call circadian_get_* functions
         * For now, use placeholder data */
        cs->phase = bridge->current_circadian_phase;
        cs->cycle_position = 0.5f; /* Noon */
        cs->arousal_modulation = 0.8f;
        cs->learning_rate_modulation = 0.9f;
        cs->consolidation_modulation = 0.3f;
        cs->metabolism_modulation = 0.85f;
        cs->sleep_pressure = 0.2f;
        cs->is_sleep_period = false;
    } else {
        /* No circadian system - use defaults (daytime) */
        cs->phase = 4; /* CIRCADIAN_PHASE_MIDDAY */
        cs->cycle_position = 0.5f;
        cs->arousal_modulation = 0.8f;
        cs->learning_rate_modulation = 0.8f;
        cs->consolidation_modulation = 0.3f;
        cs->metabolism_modulation = 0.8f;
        cs->sleep_pressure = 0.3f;
        cs->is_sleep_period = false;
    }

    cs->timestamp_us = get_current_time_us();

    /* Update modifiers for WM */
    if (bridge->config.enable_circadian_modulation) {
        effects->learning_rate_modifier = cs->learning_rate_modulation *
                                          bridge->config.circadian_modulation_strength +
                                          (1.0f - bridge->config.circadian_modulation_strength);
        effects->exploration_modifier = cs->arousal_modulation *
                                        bridge->config.circadian_modulation_strength +
                                        (1.0f - bridge->config.circadian_modulation_strength);
    } else {
        effects->learning_rate_modifier = 1.0f;
        effects->exploration_modifier = 1.0f;
    }

    bridge->stats.circadian_updates_received++;

    return NIMCP_SUCCESS;
}

/**
 * @brief Check and update conservative prediction mode
 */
static nimcp_error_t check_conservative_mode(omni_wm_hypothalamus_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    hypothalamus_to_omni_wm_effects_t* effects = &bridge->hypo_to_wm;
    bool was_conservative = bridge->conservative_mode;

    /* Check stress threshold */
    if (bridge->config.enable_stress_modulation) {
        bridge->conservative_mode = (bridge->current_stress_smoothed >
                                     bridge->config.stress_threshold);
    }

    effects->conservative_mode_active = bridge->conservative_mode;

    if (bridge->conservative_mode) {
        /* Apply conservative modifiers */
        effects->prediction_confidence_modifier = bridge->config.conservative_confidence_scale;
        effects->prediction_horizon_modifier = bridge->config.conservative_horizon_scale;
    } else {
        effects->prediction_confidence_modifier = 1.0f;
        effects->prediction_horizon_modifier = 1.0f;
    }

    /* Track transitions */
    if (bridge->conservative_mode && !was_conservative) {
        bridge->stats.conservative_mode_entries++;
        NIMCP_LOGGING_DEBUG("Entering conservative prediction mode (stress=%.2f)",
                           bridge->current_stress_smoothed);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Predict resource availability internally
 */
static nimcp_error_t predict_resources_internal(omni_wm_hypothalamus_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_resource_prediction) return NIMCP_SUCCESS;

    omni_wm_to_hypothalamus_effects_t* effects = &bridge->wm_to_hypo;
    wm_resource_forecast_t* forecast = &effects->resource_forecast;

    /* Generate resource predictions for each type */
    forecast->resource_count = WM_RESOURCE_COUNT;
    float overall_score = 0.0f;
    float overall_confidence = 0.0f;

    for (uint32_t r = 0; r < WM_RESOURCE_COUNT; r++) {
        /* Phase 8: Loop progress heartbeat */
        if ((r & 0xFF) == 0 && WM_RESOURCE_COUNT > 256) {
            omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_loop",
                             (float)(r + 1) / (float)WM_RESOURCE_COUNT);
        }

        wm_resource_prediction_t* pred = &forecast->resources[r];
        pred->type = (wm_resource_type_t)r;
        pred->current_availability = bridge->resource_levels[r];

        /* Simple decay-based prediction */
        uint32_t horizon = bridge->config.resource_prediction_horizon;
        if (horizon > WM_HYPO_MAX_PREDICTION_HORIZON) {
            horizon = WM_HYPO_MAX_PREDICTION_HORIZON;
        }
        pred->horizon_steps = horizon;

        float level = pred->current_availability;
        float min_pred = level;
        float sum_pred = level;

        for (uint32_t h = 0; h < horizon; h++) {
            /* Phase 8: Loop progress heartbeat */
            if ((h & 0xFF) == 0 && horizon > 256) {
                omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_loop",
                                 (float)(h + 1) / (float)horizon);
            }

            /* Apply resource-specific dynamics */
            level *= RESOURCE_PREDICTION_DECAY;
            bridge->resource_predictions[r][h] = level;

            if (level < min_pred) min_pred = level;
            sum_pred += level;
        }

        pred->mean_predicted = sum_pred / (float)(horizon + 1);
        pred->min_predicted = min_pred;
        pred->prediction_confidence = 0.7f; /* Placeholder */

        /* Estimate time to depletion */
        if (pred->current_availability < 0.3f && pred->min_predicted < 0.2f) {
            pred->time_to_depletion = 10.0f * pred->current_availability;
        } else {
            pred->time_to_depletion = -1.0f; /* Not depleting */
        }

        overall_score += pred->mean_predicted;
        overall_confidence += pred->prediction_confidence;
    }

    forecast->overall_resource_score = overall_score / (float)WM_RESOURCE_COUNT;
    forecast->forecast_confidence = overall_confidence / (float)WM_RESOURCE_COUNT;
    forecast->forecast_timestamp_us = get_current_time_us();

    /* Check for predicted scarcity */
    effects->resource_scarcity_predicted = (forecast->overall_resource_score < 0.3f);
    if (effects->resource_scarcity_predicted) {
        effects->time_to_scarcity = 60.0f; /* Placeholder estimate */
        bridge->stats.scarcity_predictions++;
    }

    bridge->stats.resource_predictions_generated++;
    bridge->stats.mean_resource_confidence =
        STRESS_AROUSAL_SMOOTHING * forecast->forecast_confidence +
        (1.0f - STRESS_AROUSAL_SMOOTHING) * bridge->stats.mean_resource_confidence;

    return NIMCP_SUCCESS;
}

/**
 * @brief Update effects flowing from Hypothalamus to WM
 */
static nimcp_error_t update_hypo_to_wm_effects(omni_wm_hypothalamus_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    hypothalamus_to_omni_wm_effects_t* effects = &bridge->hypo_to_wm;

    /* Update drive states */
    update_drive_states(bridge);

    /* Update circadian state */
    update_circadian_state(bridge);

    /* Update arousal and stress from smoothed values */
    effects->arousal_level = bridge->current_arousal_smoothed;
    effects->stress_level = bridge->current_stress_smoothed;

    /* Check conservative mode */
    check_conservative_mode(bridge);

    /* Get reward from drive system if available */
    if (bridge->drive_system) {
        /* Would call hypo_drive_get_reward() */
        effects->current_reward = 0.0f;
    } else {
        effects->current_reward = 0.0f;
    }

    /* Get alignment score if alignment checks enabled */
    if (bridge->config.enable_alignment_checks && bridge->drive_system) {
        /* Would call hypo_drive_check_alignment() */
        effects->alignment_score = 0.95f; /* Placeholder high score */
    } else {
        effects->alignment_score = 1.0f;
    }

    /* Get homeostatic errors if homeostasis connected */
    if (bridge->homeostasis) {
        /* Would call hypo_homeostasis_get_all_outputs() */
        effects->homeostatic_error_total = 0.1f; /* Placeholder */
    } else {
        effects->homeostatic_error_total = 0.0f;
    }

    /* Update statistics */
    bridge->stats.mean_stress_level =
        STRESS_AROUSAL_SMOOTHING * effects->stress_level +
        (1.0f - STRESS_AROUSAL_SMOOTHING) * bridge->stats.mean_stress_level;
    bridge->stats.mean_arousal_level =
        STRESS_AROUSAL_SMOOTHING * effects->arousal_level +
        (1.0f - STRESS_AROUSAL_SMOOTHING) * bridge->stats.mean_arousal_level;

    return NIMCP_SUCCESS;
}

/**
 * @brief Update effects flowing from WM to Hypothalamus
 */
static nimcp_error_t update_wm_to_hypo_effects(omni_wm_hypothalamus_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    omni_wm_to_hypothalamus_effects_t* effects = &bridge->wm_to_hypo;

    /* Predict resources */
    predict_resources_internal(bridge);

    /* If world model connected, get state prediction */
    if (bridge->world_model) {
        /* In full implementation, would call omni_wm_predict_forward()
         * For now, use placeholder data */
        effects->state_prediction_confidence = 0.8f;
        effects->prediction_horizon = bridge->config.resource_prediction_horizon;

        /* Placeholder predicted reward */
        if (bridge->config.enable_reward_prediction) {
            effects->predicted_reward = 0.0f;
            effects->reward_prediction_confidence = 0.7f;
            bridge->stats.reward_predictions_made++;
        }
    }

    /* Clear anomaly detection (placeholder) */
    effects->anomaly_detected = false;
    effects->anomaly_magnitude = 0.0f;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

/**
 * @brief Handle incoming drive state message
 */
static nimcp_error_t handle_drive_state(const void* msg, size_t msg_size,
                                         nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    if (!user_data) return NIMCP_ERROR_NULL_POINTER;

    omni_wm_hypothalamus_bridge_t* bridge = (omni_wm_hypothalamus_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.drive_updates_received++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle circadian update message
 */
static nimcp_error_t handle_circadian(const void* msg, size_t msg_size,
                                       nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    if (!user_data) return NIMCP_ERROR_NULL_POINTER;

    omni_wm_hypothalamus_bridge_t* bridge = (omni_wm_hypothalamus_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.circadian_updates_received++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle stress state message
 */
static nimcp_error_t handle_stress_state(const void* msg, size_t msg_size,
                                          nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    if (!user_data) return NIMCP_ERROR_NULL_POINTER;

    /* Would extract stress level from message and update bridge */
    return NIMCP_SUCCESS;
}

/**
 * @brief Handle resource update message
 */
static nimcp_error_t handle_resource_update(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    if (!user_data) return NIMCP_ERROR_NULL_POINTER;

    /* Would extract resource data from message and update tracking */
    return NIMCP_SUCCESS;
}

/**
 * @brief Handle reward signal message
 */
static nimcp_error_t handle_reward_signal(const void* msg, size_t msg_size,
                                           nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    if (!user_data) return NIMCP_ERROR_NULL_POINTER;

    omni_wm_hypothalamus_bridge_t* bridge = (omni_wm_hypothalamus_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.reward_predictions_made++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle homeostasis message
 */
static nimcp_error_t handle_homeostasis(const void* msg, size_t msg_size,
                                         nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    if (!user_data) return NIMCP_ERROR_NULL_POINTER;

    /* Would extract homeostatic state from message */
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_hypothalamus_bridge_default_config(
    omni_wm_hypothalamus_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(omni_wm_hypothalamus_bridge_config_t));

    /* General settings */
    config->enable_modulation = true;
    config->sensitivity = 1.0f;

    /* Drive modulation settings */
    config->enable_drive_modulation = true;
    config->drive_urgency_threshold = WM_HYPO_DEFAULT_URGENCY_THRESHOLD;
    config->drive_modulation_strength = 1.0f;
    config->enable_reward_prediction = true;

    /* Stress and conservative mode settings */
    config->enable_stress_modulation = true;
    config->stress_threshold = WM_HYPO_DEFAULT_STRESS_THRESHOLD;
    config->conservative_confidence_scale = CONSERVATIVE_CONFIDENCE_SCALE;
    config->conservative_horizon_scale = CONSERVATIVE_HORIZON_SCALE;

    /* Circadian integration settings */
    config->enable_circadian_modulation = true;
    config->circadian_modulation_strength = WM_HYPO_DEFAULT_CIRCADIAN_STRENGTH;
    config->enable_phase_prediction = true;
    config->enable_sleep_pressure_tracking = true;

    /* Resource prediction settings */
    config->enable_resource_prediction = true;
    config->resource_prediction_horizon = 16;
    config->resource_confidence_threshold = MIN_PREDICTION_CONFIDENCE;

    /* Homeostasis integration settings */
    config->enable_homeostasis_feedback = true;
    config->homeostasis_learning_rate = 0.001f;
    config->enable_setpoint_prediction = false;

    /* Alignment settings */
    config->enable_alignment_checks = true;
    config->alignment_weight = 1.0f;

    /* Bio-async settings */
    config->enable_bio_async = true;

    return NIMCP_SUCCESS;
}

omni_wm_hypothalamus_bridge_t* omni_wm_hypothalamus_bridge_create(
    const omni_wm_hypothalamus_bridge_config_t* config) {

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_create", 0.0f);


    omni_wm_hypothalamus_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_wm_hypothalamus_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate WM hypothalamus bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: bridge is NULL");
        return NULL;
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_WM_HYPOTHALAMUS_BRIDGE,
                         "wm_hypothalamus_bridge") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "omni_wm_hypothalamus_bridge_create: bridge_base_init failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        omni_wm_hypothalamus_bridge_default_config(&bridge->config);
    }

    /* Allocate dynamic effect arrays */
    bridge->hypo_to_wm.controller_outputs = nimcp_calloc(16, sizeof(float));
    if (!bridge->hypo_to_wm.controller_outputs) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate controller outputs");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: bridge->hypo_to_wm is NULL");
        return NULL;
    }
    bridge->hypo_to_wm.controller_count = 0;

    bridge->wm_to_hypo.predicted_world_state = nimcp_calloc(WM_HYPO_MAX_STATE_DIM, sizeof(float));
    if (!bridge->wm_to_hypo.predicted_world_state) {
        nimcp_free(bridge->hypo_to_wm.controller_outputs);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate predicted state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: bridge->wm_to_hypo is NULL");
        return NULL;
    }
    bridge->wm_to_hypo.predicted_state_dim = 0;

    bridge->wm_to_hypo.suggested_setpoints = nimcp_calloc(16, sizeof(float));
    if (!bridge->wm_to_hypo.suggested_setpoints) {
        nimcp_free(bridge->wm_to_hypo.predicted_world_state);
        nimcp_free(bridge->hypo_to_wm.controller_outputs);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate suggested setpoints");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: bridge->wm_to_hypo is NULL");
        return NULL;
    }

    bridge->last_predicted_state = nimcp_calloc(WM_HYPO_MAX_STATE_DIM, sizeof(float));
    if (!bridge->last_predicted_state) {
        nimcp_free(bridge->wm_to_hypo.suggested_setpoints);
        nimcp_free(bridge->wm_to_hypo.predicted_world_state);
        nimcp_free(bridge->hypo_to_wm.controller_outputs);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate last predicted state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: bridge->last_predicted_state is NULL");
        return NULL;
    }

    /* Allocate resource prediction arrays in forecast */
    for (uint32_t r = 0; r < WM_HYPO_MAX_RESOURCE_TYPES; r++) {
        /* Phase 8: Loop progress heartbeat */
        if ((r & 0xFF) == 0 && WM_HYPO_MAX_RESOURCE_TYPES > 256) {
            omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_loop",
                             (float)(r + 1) / (float)WM_HYPO_MAX_RESOURCE_TYPES);
        }

        bridge->wm_to_hypo.resource_forecast.resources[r].predicted_availability =
            nimcp_calloc(WM_HYPO_MAX_PREDICTION_HORIZON, sizeof(float));
        if (!bridge->wm_to_hypo.resource_forecast.resources[r].predicted_availability) {
            /* Cleanup already allocated */
            for (uint32_t i = 0; i < r; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && r > 256) {
                    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_loop",
                                     (float)(i + 1) / (float)r);
                }

                nimcp_free(bridge->wm_to_hypo.resource_forecast.resources[i].predicted_availability);
            }
            nimcp_free(bridge->last_predicted_state);
            nimcp_free(bridge->wm_to_hypo.suggested_setpoints);
            nimcp_free(bridge->wm_to_hypo.predicted_world_state);
            nimcp_free(bridge->hypo_to_wm.controller_outputs);
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_LOGGING_ERROR("Failed to allocate resource predictions");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_hypothalamus_bridge_create: resource prediction allocation failed");
            return NULL;
        }
    }

    /* Initialize state */
    bridge->conservative_mode = false;
    bridge->current_stress_smoothed = 0.3f;
    bridge->current_arousal_smoothed = 0.5f;
    bridge->current_circadian_phase = 4; /* MIDDAY */

    /* Initialize resource levels to moderate */
    for (uint32_t r = 0; r < WM_HYPO_MAX_RESOURCE_TYPES; r++) {
        /* Phase 8: Loop progress heartbeat */
        if ((r & 0xFF) == 0 && WM_HYPO_MAX_RESOURCE_TYPES > 256) {
            omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_loop",
                             (float)(r + 1) / (float)WM_HYPO_MAX_RESOURCE_TYPES);
        }

        bridge->resource_levels[r] = 0.7f;
    }

    bridge->reward_prediction_running_error = 0.0f;
    bridge->reward_prediction_count = 0;

    NIMCP_LOGGING_INFO("WM Hypothalamus Bridge created successfully");
    return bridge;
}

void omni_wm_hypothalamus_bridge_destroy(omni_wm_hypothalamus_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        omni_wm_hypothalamus_bridge_disconnect_bio_async(bridge);
    }

    /* Free hypo_to_wm effects dynamic arrays */
    nimcp_free(bridge->hypo_to_wm.controller_outputs);

    /* Free wm_to_hypo effects dynamic arrays */
    nimcp_free(bridge->wm_to_hypo.predicted_world_state);
    nimcp_free(bridge->wm_to_hypo.suggested_setpoints);

    /* Free resource prediction arrays */
    for (uint32_t r = 0; r < WM_HYPO_MAX_RESOURCE_TYPES; r++) {
        /* Phase 8: Loop progress heartbeat */
        if ((r & 0xFF) == 0 && WM_HYPO_MAX_RESOURCE_TYPES > 256) {
            omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_loop",
                             (float)(r + 1) / (float)WM_HYPO_MAX_RESOURCE_TYPES);
        }

        nimcp_free(bridge->wm_to_hypo.resource_forecast.resources[r].predicted_availability);
    }

    /* Free internal buffers */
    nimcp_free(bridge->last_predicted_state);

    /* Cleanup base and free */
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("WM Hypothalamus Bridge destroyed");
}

nimcp_error_t omni_wm_hypothalamus_bridge_reset(omni_wm_hypothalamus_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_reset", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset effects (preserving allocated arrays) */
    memset(&bridge->hypo_to_wm.drive_vector, 0, sizeof(wm_hypothal_drive_vector_t));
    memset(&bridge->hypo_to_wm.circadian_state, 0, sizeof(wm_circadian_state_t));
    bridge->hypo_to_wm.arousal_level = 0.5f;
    bridge->hypo_to_wm.stress_level = 0.3f;
    bridge->hypo_to_wm.conservative_mode_active = false;
    bridge->hypo_to_wm.prediction_confidence_modifier = 1.0f;
    bridge->hypo_to_wm.prediction_horizon_modifier = 1.0f;
    bridge->hypo_to_wm.learning_rate_modifier = 1.0f;
    bridge->hypo_to_wm.exploration_modifier = 1.0f;

    /* Reset wm_to_hypo effects — preserve allocated predicted_availability ptrs */
    /* Save resource prediction array pointers before clearing */
    float* saved_ptrs[WM_HYPO_MAX_RESOURCE_TYPES];
    for (uint32_t r = 0; r < WM_HYPO_MAX_RESOURCE_TYPES; r++) {
        saved_ptrs[r] = bridge->wm_to_hypo.resource_forecast.resources[r].predicted_availability;
    }
    memset(&bridge->wm_to_hypo.resource_forecast, 0, sizeof(wm_resource_forecast_t));
    /* Restore the heap-allocated prediction array pointers */
    for (uint32_t r = 0; r < WM_HYPO_MAX_RESOURCE_TYPES; r++) {
        bridge->wm_to_hypo.resource_forecast.resources[r].predicted_availability = saved_ptrs[r];
        if (saved_ptrs[r]) {
            memset(saved_ptrs[r], 0, WM_HYPO_MAX_PREDICTION_HORIZON * sizeof(float));
        }
    }
    bridge->wm_to_hypo.predicted_reward = 0.0f;
    bridge->wm_to_hypo.resource_scarcity_predicted = false;
    bridge->wm_to_hypo.anomaly_detected = false;

    /* Reset internal state */
    bridge->conservative_mode = false;
    bridge->current_stress_smoothed = 0.3f;
    bridge->current_arousal_smoothed = 0.5f;
    bridge->current_circadian_phase = 4;

    /* Reset resource levels */
    for (uint32_t r = 0; r < WM_HYPO_MAX_RESOURCE_TYPES; r++) {
        /* Phase 8: Loop progress heartbeat */
        if ((r & 0xFF) == 0 && WM_HYPO_MAX_RESOURCE_TYPES > 256) {
            omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_loop",
                             (float)(r + 1) / (float)WM_HYPO_MAX_RESOURCE_TYPES);
        }

        bridge->resource_levels[r] = 0.7f;
    }

    /* Reset reward tracking */
    bridge->reward_prediction_running_error = 0.0f;
    bridge->reward_prediction_count = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(omni_wm_hypothalamus_bridge_stats_t));

    /* Reset base bridge (unlocked since we already hold the mutex) */
    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_hypothalamus_bridge_connect(
    omni_wm_hypothalamus_bridge_t* bridge,
    omni_world_model_t* world_model,
    hypo_drive_system_handle_t* drive_system,
    hypo_homeostasis_handle_t* homeostasis,
    circadian_rhythm_t* circadian) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_connect", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_INVALID_PARAM, "world_model is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->world_model = world_model;
    bridge->drive_system = drive_system;
    bridge->homeostasis = homeostasis;
    bridge->circadian = circadian;

    /* Update base connection state */
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("WM Hypothalamus Bridge connected: WM=%p, Drives=%p, Homeo=%p, Circ=%p",
                       (void*)world_model, (void*)drive_system,
                       (void*)homeostasis, (void*)circadian);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_hypothalamus_bridge_connect_world_model(
    omni_wm_hypothalamus_bridge_t* bridge,
    omni_world_model_t* world_model) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_connect_world_model", 0.0f);


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

nimcp_error_t omni_wm_hypothalamus_bridge_connect_drives(
    omni_wm_hypothalamus_bridge_t* bridge,
    hypo_drive_system_handle_t* drive_system) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_connect_drives", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(drive_system, NIMCP_ERROR_NULL_POINTER, "drive_system is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->drive_system = drive_system;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_hypothalamus_bridge_connect_homeostasis(
    omni_wm_hypothalamus_bridge_t* bridge,
    hypo_homeostasis_handle_t* homeostasis) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_connect_homeostasis", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(homeostasis, NIMCP_ERROR_NULL_POINTER, "homeostasis is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->homeostasis = homeostasis;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_hypothalamus_bridge_connect_circadian(
    omni_wm_hypothalamus_bridge_t* bridge,
    circadian_rhythm_t* circadian) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_connect_circadian", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(circadian, NIMCP_ERROR_NULL_POINTER, "circadian is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->circadian = circadian;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

bool omni_wm_hypothalamus_bridge_is_connected(const omni_wm_hypothalamus_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_is_connected", 0.0f);


    return bridge->world_model != NULL;
}

/* ============================================================================
 * Update API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_hypothalamus_bridge_update(
    omni_wm_hypothalamus_bridge_t* bridge,
    float dt) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_update", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_modulation) return NIMCP_SUCCESS;

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update effects in both directions */
    nimcp_error_t err = update_hypo_to_wm_effects(bridge);
    if (err != NIMCP_SUCCESS) {
        bridge->stats.errors_drive++;
    }

    err = update_wm_to_hypo_effects(bridge);
    if (err != NIMCP_SUCCESS) {
        bridge->stats.errors_prediction++;
    }

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

    (void)dt;
    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_hypothalamus_bridge_on_drive_change(
    omni_wm_hypothalamus_bridge_t* bridge,
    uint32_t drive_type,
    float new_level,
    float new_urgency) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_on_drive_change", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(drive_type < WM_HYPO_MAX_DRIVES, NIMCP_ERROR_INVALID_PARAM, "drive_type out of range");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update the specific drive in our vector */
    wm_hypothal_drive_vector_t* dv = &bridge->hypo_to_wm.drive_vector;
    if (drive_type < dv->drive_count) {
        dv->drives[drive_type].level = nimcp_clampf(new_level, 0.0f, 1.0f);
        dv->drives[drive_type].urgency = nimcp_clampf(new_urgency, 0.0f, 1.0f);
        dv->drives[drive_type].is_active = (new_urgency > 0.3f);

        /* Recalculate priority boost */
        bridge->hypo_to_wm.drive_priority_boost[drive_type] = compute_drive_priority_boost(
            new_urgency,
            bridge->config.drive_urgency_threshold,
            bridge->config.drive_modulation_strength);

        /* Check if this is now the priority drive */
        if (new_urgency > dv->drives[dv->priority_drive].urgency) {
            uint32_t old_priority = dv->priority_drive;
            dv->priority_drive = drive_type;
            bridge->hypo_to_wm.prediction_focus_drive = drive_type;

            if (old_priority != drive_type) {
                bridge->stats.priority_changes++;
            }
        }
    }

    dv->timestamp_us = get_current_time_us();
    bridge->stats.drive_updates_received++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_hypothalamus_bridge_on_phase_change(
    omni_wm_hypothalamus_bridge_t* bridge,
    uint32_t new_phase,
    float cycle_position) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_on_phase_change", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t old_phase = bridge->current_circadian_phase;
    bridge->current_circadian_phase = new_phase;

    /* Update circadian state */
    wm_circadian_state_t* cs = &bridge->hypo_to_wm.circadian_state;
    cs->phase = new_phase;
    cs->cycle_position = nimcp_clampf(cycle_position, 0.0f, 1.0f);
    cs->timestamp_us = get_current_time_us();

    /* Update modulation factors based on phase */
    /* These would normally come from the circadian system */
    switch (new_phase) {
        case 0: /* NIGHT_DEEP */
        case 1: /* NIGHT_LATE */
            cs->arousal_modulation = 0.2f;
            cs->learning_rate_modulation = 0.3f;
            cs->consolidation_modulation = 0.9f;
            cs->metabolism_modulation = 0.6f;
            cs->is_sleep_period = true;
            break;
        case 2: /* DAWN */
            cs->arousal_modulation = 0.6f;
            cs->learning_rate_modulation = 0.7f;
            cs->consolidation_modulation = 0.5f;
            cs->metabolism_modulation = 0.8f;
            cs->is_sleep_period = false;
            break;
        case 3: /* MORNING */
            cs->arousal_modulation = 1.0f;
            cs->learning_rate_modulation = 0.9f;
            cs->consolidation_modulation = 0.3f;
            cs->metabolism_modulation = 1.0f;
            cs->is_sleep_period = false;
            break;
        case 4: /* MIDDAY */
            cs->arousal_modulation = 0.7f;
            cs->learning_rate_modulation = 0.7f;
            cs->consolidation_modulation = 0.3f;
            cs->metabolism_modulation = 0.9f;
            cs->is_sleep_period = false;
            break;
        case 5: /* AFTERNOON */
            cs->arousal_modulation = 0.8f;
            cs->learning_rate_modulation = 0.8f;
            cs->consolidation_modulation = 0.4f;
            cs->metabolism_modulation = 0.85f;
            cs->is_sleep_period = false;
            break;
        case 6: /* EVENING */
            cs->arousal_modulation = 0.5f;
            cs->learning_rate_modulation = 0.6f;
            cs->consolidation_modulation = 0.6f;
            cs->metabolism_modulation = 0.7f;
            cs->is_sleep_period = false;
            break;
        case 7: /* DUSK */
            cs->arousal_modulation = 0.3f;
            cs->learning_rate_modulation = 0.4f;
            cs->consolidation_modulation = 0.7f;
            cs->metabolism_modulation = 0.65f;
            cs->is_sleep_period = false;
            break;
        default:
            cs->arousal_modulation = 0.5f;
            cs->learning_rate_modulation = 0.5f;
            cs->consolidation_modulation = 0.5f;
            cs->metabolism_modulation = 0.5f;
            cs->is_sleep_period = false;
    }

    /* Recalculate WM modifiers */
    if (bridge->config.enable_circadian_modulation) {
        float strength = bridge->config.circadian_modulation_strength;
        bridge->hypo_to_wm.learning_rate_modifier =
            cs->learning_rate_modulation * strength + (1.0f - strength);
        bridge->hypo_to_wm.exploration_modifier =
            cs->arousal_modulation * strength + (1.0f - strength);
    }

    if (old_phase != new_phase) {
        bridge->stats.phase_transitions++;
        NIMCP_LOGGING_DEBUG("Circadian phase changed: %u -> %u", old_phase, new_phase);
    }

    bridge->stats.circadian_updates_received++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Drive Modulation API Implementation
 * ============================================================================ */

float omni_wm_hypothalamus_bridge_get_priority_boost(
    const omni_wm_hypothalamus_bridge_t* bridge,
    uint32_t drive_type) {

    if (!bridge || drive_type >= WM_HYPO_MAX_DRIVES) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_get_priority_boost", 0.0f);


    return bridge->hypo_to_wm.drive_priority_boost[drive_type];
}

float omni_wm_hypothalamus_bridge_get_drive_modifier(
    const omni_wm_hypothalamus_bridge_t* bridge) {

    if (!bridge) return 1.0f;
    if (!bridge->config.enable_drive_modulation) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_get_drive_modifier", 0.0f);


    const wm_hypothal_drive_vector_t* dv = &bridge->hypo_to_wm.drive_vector;

    /* Weighted average of active drive priority boosts */
    float total_urgency = 0.0f;
    float weighted_boost = 0.0f;

    for (uint32_t i = 0; i < dv->drive_count && i < WM_HYPO_MAX_DRIVES; i++) {
        if (dv->drives[i].is_active) {
            total_urgency += dv->drives[i].urgency;
            weighted_boost += dv->drives[i].urgency *
                              bridge->hypo_to_wm.drive_priority_boost[i];
        }
    }

    if (total_urgency > 0.0f) {
        return weighted_boost / total_urgency;
    }

    return 1.0f;
}

bool omni_wm_hypothalamus_bridge_is_conservative(
    const omni_wm_hypothalamus_bridge_t* bridge) {

    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_is_conservative", 0.0f);


    return bridge->conservative_mode;
}

/* ============================================================================
 * Stress and Arousal API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_hypothalamus_bridge_set_stress(
    omni_wm_hypothalamus_bridge_t* bridge,
    float stress_level) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_set_stress", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    stress_level = nimcp_clampf(stress_level, 0.0f, 1.0f);

    /* Direct assignment for explicit API call - smoothing is applied
     * internally during update cycles, not on explicit set */
    bridge->current_stress_smoothed = stress_level;

    /* Update effects */
    bridge->hypo_to_wm.stress_level = bridge->current_stress_smoothed;

    /* Check conservative mode */
    check_conservative_mode(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_hypothalamus_bridge_set_arousal(
    omni_wm_hypothalamus_bridge_t* bridge,
    float arousal_level) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_set_arousal", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    arousal_level = nimcp_clampf(arousal_level, 0.0f, 1.0f);

    /* Exponential moving average smoothing */
    bridge->current_arousal_smoothed =
        STRESS_AROUSAL_SMOOTHING * arousal_level +
        (1.0f - STRESS_AROUSAL_SMOOTHING) * bridge->current_arousal_smoothed;

    /* Update effects */
    bridge->hypo_to_wm.arousal_level = bridge->current_arousal_smoothed;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

float omni_wm_hypothalamus_bridge_get_stress(
    const omni_wm_hypothalamus_bridge_t* bridge) {

    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_get_stress", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float stress = bridge->current_stress_smoothed;
    nimcp_mutex_unlock(bridge->base.mutex);
    return stress;
}

float omni_wm_hypothalamus_bridge_get_arousal(
    const omni_wm_hypothalamus_bridge_t* bridge) {

    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_get_arousal", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float arousal = bridge->current_arousal_smoothed;
    nimcp_mutex_unlock(bridge->base.mutex);
    return arousal;
}

/* ============================================================================
 * Circadian API Implementation
 * ============================================================================ */

float omni_wm_hypothalamus_bridge_get_circadian_modulation(
    const omni_wm_hypothalamus_bridge_t* bridge,
    uint32_t modulation_type) {

    if (!bridge) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_get_circadian_modula", 0.0f);


    const wm_circadian_state_t* cs = &bridge->hypo_to_wm.circadian_state;

    switch (modulation_type) {
        case 0: return cs->arousal_modulation;
        case 1: return cs->learning_rate_modulation;
        case 2: return cs->consolidation_modulation;
        case 3: return cs->metabolism_modulation;
        default: return 1.0f;
    }
}

uint32_t omni_wm_hypothalamus_bridge_get_circadian_phase(
    const omni_wm_hypothalamus_bridge_t* bridge) {

    if (!bridge) return 0;
    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_get_circadian_phase", 0.0f);


    return bridge->current_circadian_phase;
}

float omni_wm_hypothalamus_bridge_get_sleep_pressure(
    const omni_wm_hypothalamus_bridge_t* bridge) {

    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_get_sleep_pressure", 0.0f);


    return bridge->hypo_to_wm.circadian_state.sleep_pressure;
}

/* ============================================================================
 * Resource Prediction API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_hypothalamus_bridge_predict_resource(
    omni_wm_hypothalamus_bridge_t* bridge,
    wm_resource_type_t resource_type,
    uint32_t horizon_steps,
    wm_resource_prediction_t* prediction_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_predict_resource", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(prediction_out, NIMCP_ERROR_NULL_POINTER, "prediction_out is NULL");
    NIMCP_CHECK_THROW(resource_type < WM_RESOURCE_COUNT, NIMCP_ERROR_INVALID_PARAM, "resource_type out of range");
    if (!bridge->config.enable_resource_prediction) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    if (horizon_steps > WM_HYPO_MAX_PREDICTION_HORIZON) {
        horizon_steps = WM_HYPO_MAX_PREDICTION_HORIZON;
    }

    prediction_out->type = resource_type;
    prediction_out->current_availability = bridge->resource_levels[resource_type];
    prediction_out->horizon_steps = horizon_steps;

    /* Simple decay-based prediction */
    float level = prediction_out->current_availability;
    float min_pred = level;
    float sum_pred = level;

    for (uint32_t h = 0; h < horizon_steps; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && horizon_steps > 256) {
            omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_loop",
                             (float)(h + 1) / (float)horizon_steps);
        }

        level *= RESOURCE_PREDICTION_DECAY;
        if (prediction_out->predicted_availability) {
            prediction_out->predicted_availability[h] = level;
        }
        if (level < min_pred) min_pred = level;
        sum_pred += level;
    }

    prediction_out->mean_predicted = sum_pred / (float)(horizon_steps + 1);
    prediction_out->min_predicted = min_pred;
    prediction_out->prediction_confidence = 0.7f;

    if (prediction_out->current_availability < 0.3f && min_pred < 0.2f) {
        prediction_out->time_to_depletion = 10.0f * prediction_out->current_availability;
    } else {
        prediction_out->time_to_depletion = -1.0f;
    }

    bridge->stats.resource_predictions_generated++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_hypothalamus_bridge_forecast_resources(
    omni_wm_hypothalamus_bridge_t* bridge,
    wm_resource_forecast_t* forecast_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_forecast_resources", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(forecast_out, NIMCP_ERROR_NULL_POINTER, "forecast_out is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Copy the internally maintained forecast */
    *forecast_out = bridge->wm_to_hypo.resource_forecast;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_hypothalamus_bridge_update_resource(
    omni_wm_hypothalamus_bridge_t* bridge,
    wm_resource_type_t resource_type,
    float level) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_update_resource", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(resource_type < WM_RESOURCE_COUNT, NIMCP_ERROR_INVALID_PARAM, "resource_type out of range");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->resource_levels[resource_type] = nimcp_clampf(level, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Reward Prediction API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_hypothalamus_bridge_predict_reward(
    omni_wm_hypothalamus_bridge_t* bridge,
    const float* action,
    uint32_t action_dim,
    float* reward_out,
    float* confidence_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_predict_reward", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(action, NIMCP_ERROR_INVALID_PARAM, "action is NULL");
    NIMCP_CHECK_THROW(action_dim > 0, NIMCP_ERROR_INVALID_PARAM, "action_dim must be greater than 0");
    NIMCP_CHECK_THROW(reward_out, NIMCP_ERROR_NULL_POINTER, "reward_out is NULL");
    if (!bridge->config.enable_reward_prediction) {
        *reward_out = 0.0f;
        if (confidence_out) *confidence_out = 0.0f;
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* In full implementation, would use world model to predict reward.
     * For now, simple heuristic based on drives */
    float predicted = 0.0f;
    float confidence = 0.7f;

    const wm_hypothal_drive_vector_t* dv = &bridge->hypo_to_wm.drive_vector;

    /* Reward is positive if action aligns with urgent drives */
    if (dv->drive_count > 0 && dv->priority_drive < dv->drive_count) {
        float urgency = dv->drives[dv->priority_drive].urgency;

        /* Simple heuristic: action magnitude correlates with expected reward */
        float action_magnitude = 0.0f;
        for (uint32_t i = 0; i < action_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && action_dim > 256) {
                omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_loop",
                                 (float)(i + 1) / (float)action_dim);
            }

            action_magnitude += action[i] * action[i];
        }
        action_magnitude = sqrtf(action_magnitude);

        predicted = urgency * action_magnitude * 0.1f;
    }

    /* Apply alignment modifier if enabled */
    if (bridge->config.enable_alignment_checks) {
        predicted *= bridge->hypo_to_wm.alignment_score;
    }

    *reward_out = nimcp_clampf(predicted, -1.0f, 1.0f);
    if (confidence_out) *confidence_out = confidence;

    bridge->stats.reward_predictions_made++;
    bridge->wm_to_hypo.predicted_reward = *reward_out;
    bridge->wm_to_hypo.reward_prediction_confidence = confidence;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_hypothalamus_bridge_update_reward_prediction(
    omni_wm_hypothalamus_bridge_t* bridge,
    float predicted_reward,
    float actual_reward) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_update_reward_predic", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    float error = fabsf(actual_reward - predicted_reward);

    bridge->reward_prediction_running_error =
        REWARD_ERROR_SMOOTHING * error +
        (1.0f - REWARD_ERROR_SMOOTHING) * bridge->reward_prediction_running_error;

    bridge->reward_prediction_count++;
    bridge->stats.mean_reward_prediction_error = bridge->reward_prediction_running_error;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

const hypothalamus_to_omni_wm_effects_t* omni_wm_hypothalamus_bridge_get_hypo_effects(
    const omni_wm_hypothalamus_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_get_hypo_effects", 0.0f);


    return &bridge->hypo_to_wm;
}

const omni_wm_to_hypothalamus_effects_t* omni_wm_hypothalamus_bridge_get_wm_effects(
    const omni_wm_hypothalamus_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_get_wm_effects", 0.0f);


    return &bridge->wm_to_hypo;
}

nimcp_error_t omni_wm_hypothalamus_bridge_get_stats(
    const omni_wm_hypothalamus_bridge_t* bridge,
    omni_wm_hypothalamus_bridge_stats_t* stats) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_get_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_hypothalamus_bridge_reset_stats(
    omni_wm_hypothalamus_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(omni_wm_hypothalamus_bridge_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_hypothalamus_bridge_connect_bio_async(
    omni_wm_hypothalamus_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_bio_async) return NIMCP_SUCCESS;
    if (bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    /* Check if router is initialized */
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_DEBUG("Bio-async router not initialized, skipping registration");
        return NIMCP_SUCCESS;
    }

    /* Register module with router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_WM_HYPOTHALAMUS_BRIDGE,
        .module_name = "wm_hypothalamus_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (!bridge->base.bio_ctx) {
        NIMCP_LOGGING_WARN("Failed to register with bio-async router");
        return NIMCP_SUCCESS;
    }

    /* Register message handlers */
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_HYPOTHAL_DRIVE_STATE,
                                handle_drive_state);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_HYPOTHAL_CIRCADIAN,
                                handle_circadian);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_HYPOTHAL_STRESS_STATE,
                                handle_stress_state);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_HYPOTHAL_RESOURCE_AVAIL,
                                handle_resource_update);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_HYPOTHAL_REWARD_SIGNAL,
                                handle_reward_signal);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_HYPOTHAL_HOMEOSTASIS,
                                handle_homeostasis);

    bridge->base.bio_async_enabled = true;
    NIMCP_LOGGING_INFO("WM Hypothalamus Bridge connected to bio-async router");

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_hypothalamus_bridge_disconnect_bio_async(
    omni_wm_hypothalamus_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("WM Hypothalamus Bridge disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool omni_wm_hypothalamus_bridge_is_bio_async_connected(
    const omni_wm_hypothalamus_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_is_bio_async_connect", 0.0f);


    return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL);
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* omni_wm_hypothalamus_msg_type_to_string(omni_wm_hypothalamus_msg_type_t msg_type) {
    switch (msg_type) {
        case BIO_MSG_WM_HYPOTHAL_DRIVE_STATE:
            return "DRIVE_STATE";
        case BIO_MSG_WM_HYPOTHAL_DRIVE_URGENCY:
            return "DRIVE_URGENCY";
        case BIO_MSG_WM_HYPOTHAL_DRIVE_PRIORITY:
            return "DRIVE_PRIORITY";
        case BIO_MSG_WM_HYPOTHAL_DRIVE_SATISFIED:
            return "DRIVE_SATISFIED";
        case BIO_MSG_WM_HYPOTHAL_CIRCADIAN:
            return "CIRCADIAN";
        case BIO_MSG_WM_HYPOTHAL_CIRCADIAN_MOD:
            return "CIRCADIAN_MOD";
        case BIO_MSG_WM_HYPOTHAL_TIME_OF_DAY:
            return "TIME_OF_DAY";
        case BIO_MSG_WM_HYPOTHAL_RESOURCE_PRED:
            return "RESOURCE_PRED";
        case BIO_MSG_WM_HYPOTHAL_RESOURCE_AVAIL:
            return "RESOURCE_AVAIL";
        case BIO_MSG_WM_HYPOTHAL_RESOURCE_FORECAST:
            return "RESOURCE_FORECAST";
        case BIO_MSG_WM_HYPOTHAL_STRESS_STATE:
            return "STRESS_STATE";
        case BIO_MSG_WM_HYPOTHAL_AROUSAL_STATE:
            return "AROUSAL_STATE";
        case BIO_MSG_WM_HYPOTHAL_CONSERVATIVE_MODE:
            return "CONSERVATIVE_MODE";
        case BIO_MSG_WM_HYPOTHAL_REWARD_PRED:
            return "REWARD_PRED";
        case BIO_MSG_WM_HYPOTHAL_ALIGNMENT_CHECK:
            return "ALIGNMENT_CHECK";
        case BIO_MSG_WM_HYPOTHAL_REWARD_SIGNAL:
            return "REWARD_SIGNAL";
        case BIO_MSG_WM_HYPOTHAL_HOMEOSTASIS:
            return "HOMEOSTASIS";
        case BIO_MSG_WM_HYPOTHAL_SETPOINT_ERROR:
            return "SETPOINT_ERROR";
        case BIO_MSG_WM_HYPOTHAL_CONTROLLER_OUT:
            return "CONTROLLER_OUT";
        case BIO_MSG_WM_HYPOTHAL_BRIDGE_STATUS:
            return "BRIDGE_STATUS";
        case BIO_MSG_WM_HYPOTHAL_BRIDGE_ERROR:
            return "BRIDGE_ERROR";
        case BIO_MSG_WM_HYPOTHAL_STATS_UPDATE:
            return "STATS_UPDATE";
        default:
            return "UNKNOWN";
    }
}

const char* wm_resource_type_to_string(wm_resource_type_t resource_type) {
    switch (resource_type) {
        case WM_RESOURCE_ENERGY:
            return "ENERGY";
        case WM_RESOURCE_WATER:
            return "WATER";
        case WM_RESOURCE_SAFETY:
            return "SAFETY";
        case WM_RESOURCE_SOCIAL:
            return "SOCIAL";
        case WM_RESOURCE_INFORMATION:
            return "INFORMATION";
        case WM_RESOURCE_REST:
            return "REST";
        case WM_RESOURCE_COMPUTATION:
            return "COMPUTATION";
        case WM_RESOURCE_MEMORY:
            return "MEMORY";
        default:
            return "UNKNOWN";
    }
}

nimcp_error_t omni_wm_hypothalamus_bridge_validate_config(
    const omni_wm_hypothalamus_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_hypothalamus_bridge_heartbeat("omni_wm_hypo_validate_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Validate sensitivity range */
    if (config->sensitivity < 0.5f || config->sensitivity > 2.0f) {
        NIMCP_LOGGING_WARN("Sensitivity %.2f out of range [0.5, 2.0]",
                          config->sensitivity);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate drive modulation settings */
    if (config->enable_drive_modulation) {
        if (config->drive_urgency_threshold < 0.0f ||
            config->drive_urgency_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid drive_urgency_threshold: %.2f",
                              config->drive_urgency_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->drive_modulation_strength < 0.0f ||
            config->drive_modulation_strength > 5.0f) {
            NIMCP_LOGGING_WARN("Invalid drive_modulation_strength: %.2f",
                              config->drive_modulation_strength);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate stress settings */
    if (config->enable_stress_modulation) {
        if (config->stress_threshold < 0.0f || config->stress_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid stress_threshold: %.2f",
                              config->stress_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->conservative_confidence_scale < 0.0f ||
            config->conservative_confidence_scale > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid conservative_confidence_scale: %.2f",
                              config->conservative_confidence_scale);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->conservative_horizon_scale < 0.0f ||
            config->conservative_horizon_scale > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid conservative_horizon_scale: %.2f",
                              config->conservative_horizon_scale);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate circadian settings */
    if (config->enable_circadian_modulation) {
        if (config->circadian_modulation_strength < 0.0f ||
            config->circadian_modulation_strength > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid circadian_modulation_strength: %.2f",
                              config->circadian_modulation_strength);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate resource prediction settings */
    if (config->enable_resource_prediction) {
        if (config->resource_prediction_horizon == 0 ||
            config->resource_prediction_horizon > WM_HYPO_MAX_PREDICTION_HORIZON) {
            NIMCP_LOGGING_WARN("Invalid resource_prediction_horizon: %u",
                              config->resource_prediction_horizon);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->resource_confidence_threshold < 0.0f ||
            config->resource_confidence_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid resource_confidence_threshold: %.2f",
                              config->resource_confidence_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate homeostasis settings */
    if (config->enable_homeostasis_feedback) {
        if (config->homeostasis_learning_rate <= 0.0f ||
            config->homeostasis_learning_rate > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid homeostasis_learning_rate: %.4f",
                              config->homeostasis_learning_rate);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate alignment settings */
    if (config->enable_alignment_checks) {
        if (config->alignment_weight < 0.0f || config->alignment_weight > 2.0f) {
            NIMCP_LOGGING_WARN("Invalid alignment_weight: %.2f",
                              config->alignment_weight);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    return NIMCP_SUCCESS;
}
