/**
 * @file nimcp_hypo_tom_fep_bridge.c
 * @brief Implementation of Hypothalamus Theory of Mind FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hypothalamic modulation of Theory of Mind
 * WHY:  SOCIAL drive modulates ToM processing; model accuracy maps to FEP
 * HOW:  Map drive to complexity, prediction accuracy to free energy
 */

#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_tom_fep_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hypo_tom_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static void compute_model_complexity(hypo_tom_fep_bridge_t* bridge);

static float compute_fe_from_tom(const hypo_tom_fep_bridge_t* bridge);

static hypo_tom_fep_level_t classify_tom_level(
    float complexity,
    const hypo_tom_fep_config_t* config);

static hypo_tom_fep_response_t determine_response(
    hypo_tom_fep_level_t level,
    float prediction_error);

static void update_running_averages(hypo_tom_fep_bridge_t* bridge,
                                    float free_energy,
                                    float surprise,
                                    float pred_error);

static void update_tom_tracking(hypo_tom_fep_bridge_t* bridge);

static hypo_tom_agent_model_t* find_agent_model(hypo_tom_fep_bridge_t* bridge,
                                                 uint32_t agent_id);

static hypo_tom_agent_model_t* allocate_agent_slot(hypo_tom_fep_bridge_t* bridge);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * WHAT: Get default configuration for hypothalamus ToM FEP bridge
 * WHY:  Provide sensible starting point for social cognition
 * HOW:  Set biologically-plausible defaults for ToM parameters
 */
int hypo_tom_fep_default_config(hypo_tom_fep_config_t* config) {
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

    /* ToM computation */
    config->social_drive_to_complexity_scale = 0.8f;
    config->base_model_complexity = 0.2f;
    config->complexity_decay_rate = 0.05f;

    /* Relationship effects - precision boost per relation type */
    config->relation_precision_scale[HYPO_TOM_RELATION_STRANGER] = 0.5f;
    config->relation_precision_scale[HYPO_TOM_RELATION_ACQUAINTANCE] = 0.7f;
    config->relation_precision_scale[HYPO_TOM_RELATION_COLLEAGUE] = 0.9f;
    config->relation_precision_scale[HYPO_TOM_RELATION_FRIEND] = 1.2f;
    config->relation_precision_scale[HYPO_TOM_RELATION_FAMILY] = 1.4f;
    config->relation_precision_scale[HYPO_TOM_RELATION_INTIMATE] = 1.8f;
    config->enable_relation_modulation = true;

    /* Prediction learning */
    config->model_update_rate = 0.1f;
    config->prediction_learning_rate = 0.05f;
    config->enable_prediction_learning = true;

    /* Detection parameters */
    config->free_energy_threshold = HYPO_TOM_FEP_HIGH_THRESHOLD;
    config->surprise_threshold = 8.0f;
    config->precision_learning_rate = 0.05f;

    /* Recursive depth */
    config->max_recursion_depth = 3;  /* "I think you think I think" */
    config->recursion_cost_per_level = 2.0f;  /* FE cost per level */

    /* Learning */
    config->enable_online_learning = true;
    config->learning_rate = 0.01f;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Create hypothalamus ToM FEP bridge
 * WHY:  Initialize FEP integration for social cognition
 * HOW:  Allocate structure, initialize base, apply configuration
 */
hypo_tom_fep_bridge_t* hypo_tom_fep_create(
    const hypo_tom_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
) {
    /* Validate required parameters */
    if (!drive_system || !fep_system) {
        NIMCP_LOGGING_ERROR("Hypo ToM FEP bridge: NULL system pointers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_tom_fep_create: required parameter is NULL (drive_system, fep_system)");
        return NULL;
    }

    /* Allocate bridge structure */
    hypo_tom_fep_bridge_t* bridge = (hypo_tom_fep_bridge_t*)nimcp_malloc(
        sizeof(hypo_tom_fep_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Hypo ToM FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_tom_fep_create: bridge is NULL");
        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(hypo_tom_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hypo_tom_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->drive_system = drive_system;
    bridge->fep_system = fep_system;

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "hypo_tom_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Hypo ToM FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_tom_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = HYPO_TOM_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_level = HYPO_TOM_LEVEL_MINIMAL;
    bridge->state.agent_count = 0;

    /* Initialize agent models to inactive */
    for (int i = 0; i < HYPO_TOM_MAX_AGENTS; i++) {
        bridge->state.agent_models[i].active = false;
    }

    /* Initialize FEP effects */
    bridge->fep_effects.tom_level = HYPO_TOM_LEVEL_MINIMAL;
    bridge->fep_effects.precision = HYPO_TOM_FEP_DEFAULT_PRECISION;
    bridge->fep_effects.avg_model_complexity = bridge->config.base_model_complexity;
    bridge->fep_effects.avg_model_accuracy = 0.5f;

    /* Initialize ToM effects */
    bridge->tom_effects.social_importance = 0.5f;
    bridge->tom_effects.social_uncertainty = 0.5f;

    /* Bio-async not yet connected */
    bridge->base.bio_async_enabled = false;
    bridge->base.module_id = BIO_MODULE_HYPO_TOM_FEP;
    bridge->base.module_name = "hypo_tom_fep_bridge";

    NIMCP_LOGGING_INFO("Hypo ToM FEP bridge created");
    return bridge;
}

/**
 * WHAT: Destroy hypothalamus ToM FEP bridge
 * WHY:  Clean up all resources
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void hypo_tom_fep_destroy(hypo_tom_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        hypo_tom_fep_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Hypo ToM FEP bridge destroyed");
}

/**
 * WHAT: Reset bridge to initial state
 * WHY:  Allow reuse without full recreation
 * HOW:  Clear state and statistics, preserve connections
 */
int hypo_tom_fep_reset(hypo_tom_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->state.update_count = 0;
    bridge->state.prediction_count = 0;
    bridge->state.current_precision = HYPO_TOM_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_level = HYPO_TOM_LEVEL_MINIMAL;
    bridge->state.last_detection_time_ms = 0;
    bridge->state.agent_count = 0;

    /* Reset agent models */
    for (int i = 0; i < HYPO_TOM_MAX_AGENTS; i++) {
        bridge->state.agent_models[i].active = false;
    }

    /* Reset ToM tracking */
    memset(&bridge->state.tom_tracking, 0, sizeof(hypo_tom_tracking_t));

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(hypo_tom_fep_effects_t));
    bridge->fep_effects.precision = HYPO_TOM_FEP_DEFAULT_PRECISION;
    bridge->fep_effects.avg_model_complexity = bridge->config.base_model_complexity;
    bridge->fep_effects.avg_model_accuracy = 0.5f;

    memset(&bridge->tom_effects, 0, sizeof(tom_to_fep_effects_t));
    bridge->tom_effects.social_importance = 0.5f;
    bridge->tom_effects.social_uncertainty = 0.5f;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(hypo_tom_fep_stats_t));
    bridge->stats.current_precision = HYPO_TOM_FEP_DEFAULT_PRECISION;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Hypo ToM FEP bridge reset");
    return 0;
}

/**
 * WHAT: Update bridge state
 * WHY:  Main update loop for bridge synchronization
 * HOW:  Compute model complexity, update FEP metrics
 */
int hypo_tom_fep_update(hypo_tom_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute model complexity from SOCIAL drive */
    compute_model_complexity(bridge);

    /* Compute free energy from ToM state */
    float current_fe = compute_fe_from_tom(bridge);

    /* Get FEP system metrics */
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    /* Update running averages */
    update_running_averages(bridge, current_fe, surprise, pred_error);

    /* Store in FEP effects */
    bridge->fep_effects.free_energy = current_fe;
    bridge->fep_effects.prediction_error = pred_error;
    bridge->fep_effects.precision = bridge->state.current_precision;

    /* Classify ToM level based on average complexity */
    bridge->fep_effects.tom_level = classify_tom_level(
        bridge->fep_effects.avg_model_complexity,
        &bridge->config
    );

    /* Determine response based on prediction error */
    bridge->fep_effects.recommended_response = determine_response(
        bridge->fep_effects.tom_level,
        bridge->fep_effects.avg_model_accuracy
    );

    /* Processing confidence from precision */
    bridge->fep_effects.processing_confidence =
        bridge->state.current_precision / HYPO_TOM_FEP_MAX_PRECISION;
    if (bridge->fep_effects.processing_confidence > 1.0f) {
        bridge->fep_effects.processing_confidence = 1.0f;
    }

    /* Response urgency from FE */
    float urgency = current_fe / HYPO_TOM_FEP_CRITICAL_THRESHOLD;
    if (urgency > 1.0f) urgency = 1.0f;
    bridge->fep_effects.response_urgency = urgency;

    /* Count active models */
    uint32_t active_count = 0;
    for (int i = 0; i < HYPO_TOM_MAX_AGENTS; i++) {
        if (bridge->state.agent_models[i].active) {
            active_count++;
        }
    }
    bridge->fep_effects.active_model_count = active_count;

    /* Check if focus model needs update */
    if (bridge->fep_effects.focus_agent_id > 0) {
        hypo_tom_agent_model_t* focus = find_agent_model(bridge,
            bridge->fep_effects.focus_agent_id);
        if (focus) {
            bridge->fep_effects.focus_prediction_error = focus->prediction_error_ema;
            bridge->fep_effects.focus_model_needs_update =
                (focus->prediction_error_ema > 0.3f);
        }
    }

    /* Update ToM tracking */
    update_tom_tracking(bridge);

    /* Update statistics */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    bridge->stats.current_precision = bridge->state.current_precision;

    if (current_fe > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = current_fe;
    }

    /* Calculate overall accuracy */
    if (bridge->tom_effects.total_predictions > 0) {
        bridge->stats.overall_accuracy_rate =
            (float)bridge->tom_effects.accurate_predictions /
            (float)bridge->tom_effects.total_predictions;
    }

    bridge->stats.avg_model_complexity = bridge->fep_effects.avg_model_complexity;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Core Operations Implementation
 * ============================================================================ */

/**
 * WHAT: Compute free energy from ToM state
 * WHY:  Core FEP computation for social cognition
 * HOW:  Map prediction errors to free energy
 */
int hypo_tom_fep_compute_fe(
    hypo_tom_fep_bridge_t* bridge,
    const hypo_drive_system_t* drives
) {
    if (!bridge || !drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_tom_fep_compute_fe: bridge or drives is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Store drive urgencies */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->tom_effects.drive_urgencies[i] = drives->drives[i].urgency;
    }
    bridge->tom_effects.social_drive_urgency = drives->drives[HYPO_DRIVE_SOCIAL].urgency;

    /* Compute model complexity from social drive */
    compute_model_complexity(bridge);

    /* Compute free energy */
    float total_fe = compute_fe_from_tom(bridge);
    bridge->fep_effects.free_energy = total_fe;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Modulate precision based on relationship
 * WHY:  More important relationships warrant higher precision
 * HOW:  Scale precision by relationship type
 */
int hypo_tom_fep_modulate_precision(
    hypo_tom_fep_bridge_t* bridge,
    uint32_t agent_id,
    hypo_tom_relation_t relationship
) {
    if (!bridge || relationship > HYPO_TOM_RELATION_INTIMATE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_tom_fep_modulate_precision: bridge is NULL or invalid relationship");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Find or create agent model */
    hypo_tom_agent_model_t* model = find_agent_model(bridge, agent_id);
    if (!model) {
        model = allocate_agent_slot(bridge);
        if (!model) {
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_tom_fep_modulate_precision: model is NULL");
            return -1;
        }
        model->agent_id = agent_id;
        model->active = true;
    }

    model->relationship = relationship;

    /* Update precision based on relationship */
    if (bridge->config.enable_relation_modulation) {
        float precision_scale = bridge->config.relation_precision_scale[relationship];
        float new_precision = HYPO_TOM_FEP_DEFAULT_PRECISION * precision_scale;

        /* Smooth adaptation */
        float alpha = bridge->config.precision_learning_rate;
        bridge->state.current_precision =
            (1.0f - alpha) * bridge->state.current_precision + alpha * new_precision;

        /* Clamp */
        if (bridge->state.current_precision < HYPO_TOM_FEP_MIN_PRECISION) {
            bridge->state.current_precision = HYPO_TOM_FEP_MIN_PRECISION;
        }
        if (bridge->state.current_precision > HYPO_TOM_FEP_MAX_PRECISION) {
            bridge->state.current_precision = HYPO_TOM_FEP_MAX_PRECISION;
        }

        bridge->fep_effects.precision = bridge->state.current_precision;
        bridge->stats.precision_adaptations++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Get current FEP effects
 * WHY:  Allow inspection of current effects
 * HOW:  Copy effects structure
 */
int hypo_tom_fep_get_effects(
    const hypo_tom_fep_bridge_t* bridge,
    hypo_tom_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_tom_fep_get_effects: bridge or effects is NULL");
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
int hypo_tom_fep_get_stats(
    const hypo_tom_fep_bridge_t* bridge,
    hypo_tom_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_tom_fep_get_stats: bridge or stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Agent Model API Implementation
 * ============================================================================ */

/**
 * WHAT: Register or update an agent model
 * WHY:  Enable ToM tracking for social prediction
 * HOW:  Find or create model slot
 */
int hypo_tom_fep_register_agent(
    hypo_tom_fep_bridge_t* bridge,
    uint32_t agent_id,
    hypo_tom_relation_t relationship
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Find existing or allocate new */
    hypo_tom_agent_model_t* model = find_agent_model(bridge, agent_id);
    if (!model) {
        model = allocate_agent_slot(bridge);
        if (!model) {
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_tom_fep_register_agent: model is NULL");
            return -1;
        }
        bridge->state.agent_count++;
    }

    /* Initialize/update model */
    model->agent_id = agent_id;
    model->active = true;
    model->relationship = relationship;
    model->familiarity = 0.1f;
    model->trust = 0.5f;
    model->model_complexity = bridge->config.base_model_complexity;
    model->model_accuracy = 0.5f;
    model->model_confidence = 0.3f;
    model->predictions_made = 0;
    model->predictions_correct = 0;
    model->prediction_error_ema = 0.5f;
    model->last_interaction_ms = nimcp_platform_time_monotonic_ms();
    model->last_update_ms = model->last_interaction_ms;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Get model for an agent
 * WHY:  Access current state of other-model
 * HOW:  Find and copy model
 */
int hypo_tom_fep_get_agent_model(
    const hypo_tom_fep_bridge_t* bridge,
    uint32_t agent_id,
    hypo_tom_agent_model_t* model
) {
    if (!bridge || !model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_tom_fep_get_agent_model: bridge or model is NULL");
        return -1;
    }

    for (int i = 0; i < HYPO_TOM_MAX_AGENTS; i++) {
        if (bridge->state.agent_models[i].active &&
            bridge->state.agent_models[i].agent_id == agent_id) {
            *model = bridge->state.agent_models[i];
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_tom_fep_get_agent_model: operation failed");
    return -1;  /* Agent not found */
}

/**
 * WHAT: Report prediction outcome for an agent
 * WHY:  Learn from prediction outcomes
 * HOW:  Update model accuracy and prediction error EMA
 */
int hypo_tom_fep_report_prediction(
    hypo_tom_fep_bridge_t* bridge,
    uint32_t agent_id,
    float prediction_error
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (prediction_error < 0.0f) prediction_error = 0.0f;
    if (prediction_error > 1.0f) prediction_error = 1.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    hypo_tom_agent_model_t* model = find_agent_model(bridge, agent_id);
    if (!model) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_tom_fep_report_prediction: model is NULL");
        return -1;
    }

    /* Update prediction tracking */
    model->predictions_made++;
    bridge->tom_effects.total_predictions++;
    bridge->stats.fep_predictions++;

    /* Update prediction error EMA */
    float alpha = bridge->config.prediction_learning_rate;
    model->prediction_error_ema =
        (1.0f - alpha) * model->prediction_error_ema + alpha * prediction_error;

    /* Track accuracy (low error = accurate) */
    if (prediction_error < 0.3f) {
        model->predictions_correct++;
        bridge->tom_effects.accurate_predictions++;
        bridge->stats.accurate_predictions++;
    }

    /* Update model accuracy */
    if (model->predictions_made > 0) {
        model->model_accuracy = (float)model->predictions_correct /
                                (float)model->predictions_made;
    }

    /* Update model confidence based on accuracy */
    model->model_confidence = model->model_accuracy * 0.8f +
                              model->familiarity * 0.2f;

    /* Update max PE stat */
    if (prediction_error > bridge->stats.max_prediction_error) {
        bridge->stats.max_prediction_error = prediction_error;
    }

    /* Track by relation */
    if (model->relationship <= HYPO_TOM_RELATION_INTIMATE) {
        bridge->stats.predictions_by_relation[model->relationship]++;
        if (prediction_error < 0.3f) {
            bridge->stats.accurate_by_relation[model->relationship]++;
        }
    }

    model->last_update_ms = nimcp_platform_time_monotonic_ms();
    bridge->stats.model_updates++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Focus ToM processing on an agent
 * WHY:  Allocate modeling capacity where needed
 * HOW:  Set focus agent ID
 */
int hypo_tom_fep_focus_agent(
    hypo_tom_fep_bridge_t* bridge,
    uint32_t agent_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    hypo_tom_agent_model_t* model = find_agent_model(bridge, agent_id);
    if (!model) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_tom_fep_focus_agent: model is NULL");
        return -1;
    }

    bridge->fep_effects.focus_agent_id = agent_id;
    bridge->fep_effects.focus_prediction_error = model->prediction_error_ema;
    bridge->fep_effects.focus_model_needs_update = (model->prediction_error_ema > 0.3f);

    /* Increase complexity for focused agent */
    float social_drive = bridge->tom_effects.social_drive_urgency;
    model->model_complexity = bridge->config.base_model_complexity +
        social_drive * bridge->config.social_drive_to_complexity_scale * 1.5f;
    if (model->model_complexity > HYPO_TOM_MAX_COMPLEXITY) {
        model->model_complexity = HYPO_TOM_MAX_COMPLEXITY;
    }

    model->last_interaction_ms = nimcp_platform_time_monotonic_ms();

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int hypo_tom_fep_connect_bio_async(
    hypo_tom_fep_bridge_t* bridge,
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
        .module_id = BIO_MODULE_HYPO_TOM_FEP,
        .module_name = "hypo_tom_fep_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hypo ToM FEP bridge connected to bio-async");
    }

    return 0;
}

int hypo_tom_fep_disconnect_bio_async(hypo_tom_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Hypo ToM FEP bridge disconnected from bio-async");
    return 0;
}

int hypo_tom_fep_process_messages(
    hypo_tom_fep_bridge_t* bridge,
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

const char* hypo_tom_fep_level_name(hypo_tom_fep_level_t level) {
    switch (level) {
        case HYPO_TOM_LEVEL_MINIMAL:
            return "Minimal";
        case HYPO_TOM_LEVEL_BASIC:
            return "Basic";
        case HYPO_TOM_LEVEL_MODERATE:
            return "Moderate";
        case HYPO_TOM_LEVEL_ADVANCED:
            return "Advanced";
        case HYPO_TOM_LEVEL_EXPERT:
            return "Expert";
        default:
            return "Unknown";
    }
}

const char* hypo_tom_fep_relation_name(hypo_tom_relation_t relation) {
    switch (relation) {
        case HYPO_TOM_RELATION_STRANGER:
            return "Stranger";
        case HYPO_TOM_RELATION_ACQUAINTANCE:
            return "Acquaintance";
        case HYPO_TOM_RELATION_COLLEAGUE:
            return "Colleague";
        case HYPO_TOM_RELATION_FRIEND:
            return "Friend";
        case HYPO_TOM_RELATION_FAMILY:
            return "Family";
        case HYPO_TOM_RELATION_INTIMATE:
            return "Intimate";
        default:
            return "Unknown";
    }
}

const char* hypo_tom_fep_response_name(hypo_tom_fep_response_t response) {
    switch (response) {
        case HYPO_TOM_FEP_RESPONSE_OBSERVE:
            return "Observe";
        case HYPO_TOM_FEP_RESPONSE_UPDATE_MODEL:
            return "Update Model";
        case HYPO_TOM_FEP_RESPONSE_SEEK_INFO:
            return "Seek Info";
        case HYPO_TOM_FEP_RESPONSE_COMMUNICATE:
            return "Communicate";
        case HYPO_TOM_FEP_RESPONSE_COORDINATE:
            return "Coordinate";
        case HYPO_TOM_FEP_RESPONSE_REPAIR:
            return "Repair";
        default:
            return "Unknown";
    }
}

void hypo_tom_fep_print_summary(const hypo_tom_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Hypo ToM FEP Bridge: NULL\n");
        return;
    }

    printf("=== Hypothalamus ToM FEP Bridge Summary ===\n");
    printf("State:\n");
    printf("  Active: %s\n", bridge->state.active ? "yes" : "no");
    printf("  Updates: %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Precision: %.3f\n", bridge->state.current_precision);
    printf("  Active Agents: %u\n", bridge->fep_effects.active_model_count);
    printf("\n");
    printf("ToM Processing:\n");
    printf("  Level: %s\n", hypo_tom_fep_level_name(bridge->fep_effects.tom_level));
    printf("  Avg Complexity: %.3f\n", bridge->fep_effects.avg_model_complexity);
    printf("  Avg Accuracy: %.3f\n", bridge->fep_effects.avg_model_accuracy);
    printf("  Processing Confidence: %.3f\n", bridge->fep_effects.processing_confidence);
    printf("\n");
    printf("FEP Effects:\n");
    printf("  Free Energy: %.3f\n", bridge->fep_effects.free_energy);
    printf("  Prediction Error: %.3f\n", bridge->fep_effects.prediction_error);
    printf("  Response Urgency: %.3f\n", bridge->fep_effects.response_urgency);
    printf("  Recommended Response: %s\n",
           hypo_tom_fep_response_name(bridge->fep_effects.recommended_response));
    printf("\n");
    printf("Focus Agent:\n");
    if (bridge->fep_effects.focus_agent_id > 0) {
        printf("  ID: %u\n", bridge->fep_effects.focus_agent_id);
        printf("  Prediction Error: %.3f\n", bridge->fep_effects.focus_prediction_error);
        printf("  Needs Update: %s\n",
               bridge->fep_effects.focus_model_needs_update ? "yes" : "no");
    } else {
        printf("  None\n");
    }
    printf("\n");
    printf("Statistics:\n");
    printf("  Total Predictions: %lu\n", (unsigned long)bridge->stats.fep_predictions);
    printf("  Accurate Predictions: %lu\n", (unsigned long)bridge->stats.accurate_predictions);
    printf("  Accuracy Rate: %.1f%%\n", bridge->stats.overall_accuracy_rate * 100.0f);
    printf("  Model Updates: %lu\n", (unsigned long)bridge->stats.model_updates);
    printf("=============================================\n");
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

/**
 * WHAT: Find agent model by ID
 * WHY:  Lookup existing models
 * HOW:  Linear search (small array)
 */
static hypo_tom_agent_model_t* find_agent_model(hypo_tom_fep_bridge_t* bridge,
                                                 uint32_t agent_id) {
    for (int i = 0; i < HYPO_TOM_MAX_AGENTS; i++) {
        if (bridge->state.agent_models[i].active &&
            bridge->state.agent_models[i].agent_id == agent_id) {
            return &bridge->state.agent_models[i];
        }
    }
    /* Not found is a normal condition (agent not yet registered) - no THROW */
    return NULL;
}

/**
 * WHAT: Allocate a new agent model slot
 * WHY:  Add new agents to tracking
 * HOW:  Find first inactive slot
 */
static hypo_tom_agent_model_t* allocate_agent_slot(hypo_tom_fep_bridge_t* bridge) {
    for (int i = 0; i < HYPO_TOM_MAX_AGENTS; i++) {
        if (!bridge->state.agent_models[i].active) {
            return &bridge->state.agent_models[i];
        }
    }
    /* No free slots is a capacity limit, not a critical error */
    return NULL;
}

/**
 * WHAT: Compute model complexity from SOCIAL drive
 * WHY:  SOCIAL drive increases ToM processing
 * HOW:  Scale complexity by drive urgency
 */
static void compute_model_complexity(hypo_tom_fep_bridge_t* bridge) {
    float social_drive = bridge->tom_effects.social_drive_urgency;

    /* Base complexity plus drive-scaled component */
    float base = bridge->config.base_model_complexity;
    float drive_component = social_drive * bridge->config.social_drive_to_complexity_scale;

    float total_complexity = 0.0f;
    uint32_t active_count = 0;

    /* Update complexity for each active model */
    for (int i = 0; i < HYPO_TOM_MAX_AGENTS; i++) {
        if (bridge->state.agent_models[i].active) {
            /* Scale by relationship importance */
            float relation_scale = 1.0f;
            if (bridge->config.enable_relation_modulation) {
                hypo_tom_relation_t rel = bridge->state.agent_models[i].relationship;
                relation_scale = bridge->config.relation_precision_scale[rel];
            }

            float model_complexity = base + drive_component * relation_scale;
            if (model_complexity > HYPO_TOM_MAX_COMPLEXITY) {
                model_complexity = HYPO_TOM_MAX_COMPLEXITY;
            }
            if (model_complexity < HYPO_TOM_MIN_COMPLEXITY) {
                model_complexity = HYPO_TOM_MIN_COMPLEXITY;
            }

            bridge->state.agent_models[i].model_complexity = model_complexity;
            total_complexity += model_complexity;
            active_count++;
        }
    }

    /* Compute average */
    if (active_count > 0) {
        bridge->fep_effects.avg_model_complexity = total_complexity / (float)active_count;
    } else {
        bridge->fep_effects.avg_model_complexity = base;
    }

    /* Compute average accuracy */
    float total_accuracy = 0.0f;
    for (int i = 0; i < HYPO_TOM_MAX_AGENTS; i++) {
        if (bridge->state.agent_models[i].active) {
            total_accuracy += bridge->state.agent_models[i].model_accuracy;
        }
    }
    if (active_count > 0) {
        bridge->fep_effects.avg_model_accuracy = total_accuracy / (float)active_count;
    } else {
        bridge->fep_effects.avg_model_accuracy = 0.5f;
    }
}

/**
 * WHAT: Compute free energy from ToM state
 * WHY:  Map prediction errors to FEP domain
 * HOW:  Sum of precision-weighted prediction errors across models
 */
static float compute_fe_from_tom(const hypo_tom_fep_bridge_t* bridge) {
    float fe = 0.0f;

    /* FE from prediction errors across all models */
    for (int i = 0; i < HYPO_TOM_MAX_AGENTS; i++) {
        if (bridge->state.agent_models[i].active) {
            float pe = bridge->state.agent_models[i].prediction_error_ema;
            float complexity = bridge->state.agent_models[i].model_complexity;

            /* Higher complexity means more processing, so errors cost more */
            fe += pe * pe * complexity * bridge->config.drive_fe_weight;
        }
    }

    /* Add social uncertainty as FE component */
    fe += bridge->tom_effects.social_uncertainty * 2.0f;

    /* Scale by precision */
    fe *= bridge->state.current_precision;

    return fe;
}

/**
 * WHAT: Classify ToM level from complexity
 * WHY:  Map continuous complexity to discrete categories
 * HOW:  Threshold-based classification
 */
static hypo_tom_fep_level_t classify_tom_level(
    float complexity,
    const hypo_tom_fep_config_t* config
) {
    (void)config;  /* May use config thresholds in future */

    if (complexity >= 0.8f) {
        return HYPO_TOM_LEVEL_EXPERT;
    } else if (complexity >= 0.6f) {
        return HYPO_TOM_LEVEL_ADVANCED;
    } else if (complexity >= 0.4f) {
        return HYPO_TOM_LEVEL_MODERATE;
    } else if (complexity >= 0.2f) {
        return HYPO_TOM_LEVEL_BASIC;
    } else {
        return HYPO_TOM_LEVEL_MINIMAL;
    }
}

/**
 * WHAT: Determine appropriate response
 * WHY:  Active inference selects social action
 * HOW:  Map ToM level and accuracy to response type
 */
static hypo_tom_fep_response_t determine_response(
    hypo_tom_fep_level_t level,
    float accuracy
) {
    /* Low accuracy = need to update/repair */
    if (accuracy < 0.3f) {
        if (level >= HYPO_TOM_LEVEL_MODERATE) {
            return HYPO_TOM_FEP_RESPONSE_REPAIR;
        }
        return HYPO_TOM_FEP_RESPONSE_SEEK_INFO;
    }

    /* Medium accuracy = communicate or update */
    if (accuracy < 0.6f) {
        if (level >= HYPO_TOM_LEVEL_ADVANCED) {
            return HYPO_TOM_FEP_RESPONSE_COMMUNICATE;
        }
        return HYPO_TOM_FEP_RESPONSE_UPDATE_MODEL;
    }

    /* High accuracy = coordinate or continue observing */
    switch (level) {
        case HYPO_TOM_LEVEL_EXPERT:
            return HYPO_TOM_FEP_RESPONSE_COORDINATE;
        case HYPO_TOM_LEVEL_ADVANCED:
            return HYPO_TOM_FEP_RESPONSE_COMMUNICATE;
        case HYPO_TOM_LEVEL_MODERATE:
            return HYPO_TOM_FEP_RESPONSE_UPDATE_MODEL;
        default:
            return HYPO_TOM_FEP_RESPONSE_OBSERVE;
    }
}

/**
 * WHAT: Update running averages for metrics
 * WHY:  Smooth metrics over time for stability
 * HOW:  Exponential moving average
 */
static void update_running_averages(
    hypo_tom_fep_bridge_t* bridge,
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
 * WHAT: Update ToM tracking
 * WHY:  Track ToM evolution for predictions
 * HOW:  Store history, update predictions
 */
static void update_tom_tracking(hypo_tom_fep_bridge_t* bridge) {
    hypo_tom_tracking_t* tracking = &bridge->state.tom_tracking;

    /* Store complexity in history */
    uint32_t idx = tracking->history_idx;
    tracking->complexity_history[idx] = bridge->fep_effects.avg_model_complexity;

    /* Update prediction using EMA */
    float alpha = 0.1f;
    tracking->predicted_complexity =
        (1.0f - alpha) * tracking->predicted_complexity +
        alpha * bridge->fep_effects.avg_model_complexity;

    /* Compute velocity */
    if (idx > 0) {
        float prev = tracking->complexity_history[(idx - 1) % 16];
        tracking->complexity_velocity = bridge->fep_effects.avg_model_complexity - prev;
    }

    tracking->history_idx = (idx + 1) % 16;
}
