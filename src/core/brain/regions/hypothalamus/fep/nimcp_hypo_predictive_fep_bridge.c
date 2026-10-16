/**
 * @file nimcp_hypo_predictive_fep_bridge.c
 * @brief Implementation of Hypothalamus Predictive FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hypothalamic modulation of predictive processing
 * WHY:  Drives prioritize predictions; accuracy reduces free energy
 * HOW:  Map drive urgency to priority, prediction accuracy to FE reduction
 */

#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_predictive_fep_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hypo_predictive_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static void compute_channel_priorities(hypo_pred_fep_bridge_t* bridge);

static float compute_fe_from_predictions(const hypo_pred_fep_bridge_t* bridge);

static hypo_pred_fep_level_t classify_priority_level(
    float total_priority,
    const hypo_pred_fep_config_t* config);

static hypo_pred_fep_response_t determine_response(
    hypo_pred_fep_level_t level,
    float avg_accuracy,
    float free_energy);

static void update_running_averages(hypo_pred_fep_bridge_t* bridge,
                                    float free_energy,
                                    float surprise,
                                    float pred_error);

static void update_pred_tracking(hypo_pred_fep_bridge_t* bridge);

static hypo_pred_channel_t map_drive_to_channel(hypo_drive_type_t drive);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * WHAT: Get default configuration for hypothalamus predictive FEP bridge
 * WHY:  Provide sensible starting point for prediction prioritization
 * HOW:  Set biologically-plausible defaults for predictive parameters
 */
int hypo_pred_fep_default_config(hypo_pred_fep_config_t* config) {
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

    /* Priority computation - drive to priority mapping */
    config->drive_to_priority_scale[HYPO_DRIVE_HUNGER] = 1.0f;
    config->drive_to_priority_scale[HYPO_DRIVE_THIRST] = 1.0f;
    config->drive_to_priority_scale[HYPO_DRIVE_SAFETY] = 1.5f;  /* Safety is high priority */
    config->drive_to_priority_scale[HYPO_DRIVE_TEMPERATURE] = 0.6f;
    config->drive_to_priority_scale[HYPO_DRIVE_FATIGUE] = 0.5f;
    config->drive_to_priority_scale[HYPO_DRIVE_SOCIAL] = 0.8f;
    config->drive_to_priority_scale[HYPO_DRIVE_AUTONOMY] = 0.7f;
    config->drive_to_priority_scale[HYPO_DRIVE_CURIOSITY] = 0.6f;

    config->base_priority = 0.1f;
    config->priority_decay_rate = 0.05f;

    /* Accuracy tracking */
    config->accuracy_learning_rate = 0.1f;
    config->accuracy_threshold = 0.7f;  /* 70% = "accurate" */
    config->enable_accuracy_learning = true;

    /* FE reduction parameters */
    config->fe_reduction_per_accuracy = 2.0f;
    config->surprise_amplification = 1.5f;

    /* Detection parameters */
    config->free_energy_threshold = HYPO_PRED_FEP_HIGH_THRESHOLD;
    config->surprise_threshold = 8.0f;
    config->precision_learning_rate = 0.05f;

    /* Active inference */
    config->action_threshold = 15.0f;
    config->exploration_threshold = 8.0f;

    /* Learning */
    config->enable_online_learning = true;
    config->learning_rate = 0.01f;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Create hypothalamus predictive FEP bridge
 * WHY:  Initialize FEP integration for prediction prioritization
 * HOW:  Allocate structure, initialize base, apply configuration
 */
hypo_pred_fep_bridge_t* hypo_pred_fep_create(
    const hypo_pred_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
) {
    /* Validate required parameters */
    if (!drive_system || !fep_system) {
        NIMCP_LOGGING_ERROR("Hypo Predictive FEP bridge: NULL system pointers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_pred_fep_create: required parameter is NULL (drive_system, fep_system)");
        return NULL;
    }

    /* Allocate bridge structure */
    hypo_pred_fep_bridge_t* bridge = (hypo_pred_fep_bridge_t*)nimcp_malloc(
        sizeof(hypo_pred_fep_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Hypo Predictive FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_pred_fep_create: bridge is NULL");
        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(hypo_pred_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hypo_pred_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->drive_system = drive_system;
    bridge->fep_system = fep_system;

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "hypo_predictive_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Hypo Predictive FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_pred_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = HYPO_PRED_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_level = HYPO_PRED_FEP_LEVEL_BACKGROUND;

    /* Initialize channels */
    for (int i = 0; i < HYPO_PRED_CHANNEL_COUNT; i++) {
        bridge->state.channels[i].channel = (hypo_pred_channel_t)i;
        bridge->state.channels[i].active = true;
        bridge->state.channels[i].priority = bridge->config.base_priority;
        bridge->state.channels[i].precision = HYPO_PRED_FEP_DEFAULT_PRECISION;
        bridge->state.channels[i].accuracy_rate = 0.5f;
        bridge->state.channels[i].prediction_error_ema = 0.5f;

        bridge->fep_effects.channel_priorities[i] = bridge->config.base_priority;
    }

    /* Initialize FEP effects */
    bridge->fep_effects.priority_level = HYPO_PRED_FEP_LEVEL_BACKGROUND;
    bridge->fep_effects.precision = HYPO_PRED_FEP_DEFAULT_PRECISION;
    bridge->fep_effects.avg_accuracy = 0.5f;

    /* Bio-async not yet connected */
    bridge->base.bio_async_enabled = false;
    bridge->base.module_id = BIO_MODULE_HYPO_PREDICTIVE_FEP;
    bridge->base.module_name = "hypo_predictive_fep_bridge";

    NIMCP_LOGGING_INFO("Hypo Predictive FEP bridge created");
    return bridge;
}

/**
 * WHAT: Destroy hypothalamus predictive FEP bridge
 * WHY:  Clean up all resources
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void hypo_pred_fep_destroy(hypo_pred_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.bio_async_enabled) {
        hypo_pred_fep_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Hypo Predictive FEP bridge destroyed");
}

/**
 * WHAT: Reset bridge to initial state
 * WHY:  Allow reuse without full recreation
 * HOW:  Clear state and statistics, preserve connections
 */
int hypo_pred_fep_reset(hypo_pred_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->state.update_count = 0;
    bridge->state.prediction_count = 0;
    bridge->state.current_precision = HYPO_PRED_FEP_DEFAULT_PRECISION;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.avg_prediction_error = 0.0f;
    bridge->state.last_level = HYPO_PRED_FEP_LEVEL_BACKGROUND;
    bridge->state.last_detection_time_ms = 0;

    /* Reset channels */
    for (int i = 0; i < HYPO_PRED_CHANNEL_COUNT; i++) {
        bridge->state.channels[i].priority = bridge->config.base_priority;
        bridge->state.channels[i].precision = HYPO_PRED_FEP_DEFAULT_PRECISION;
        bridge->state.channels[i].predictions_made = 0;
        bridge->state.channels[i].predictions_accurate = 0;
        bridge->state.channels[i].accuracy_rate = 0.5f;
        bridge->state.channels[i].prediction_error_ema = 0.5f;
        bridge->state.channels[i].channel_free_energy = 0.0f;
        bridge->state.channels[i].fe_reduction_cumulative = 0.0f;

        bridge->fep_effects.channel_priorities[i] = bridge->config.base_priority;
    }

    /* Reset prediction tracking */
    memset(&bridge->state.pred_tracking, 0, sizeof(hypo_pred_tracking_t));

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(hypo_pred_fep_effects_t));
    bridge->fep_effects.precision = HYPO_PRED_FEP_DEFAULT_PRECISION;
    bridge->fep_effects.avg_accuracy = 0.5f;
    for (int i = 0; i < HYPO_PRED_CHANNEL_COUNT; i++) {
        bridge->fep_effects.channel_priorities[i] = bridge->config.base_priority;
    }

    memset(&bridge->pred_effects, 0, sizeof(pred_to_fep_effects_t));

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(hypo_pred_fep_stats_t));
    bridge->stats.current_precision = HYPO_PRED_FEP_DEFAULT_PRECISION;
    bridge->stats.min_free_energy = 1000.0f;  /* High initial value */

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Hypo Predictive FEP bridge reset");
    return 0;
}

/**
 * WHAT: Update bridge state
 * WHY:  Main update loop for bridge synchronization
 * HOW:  Compute priorities from drives, update FEP metrics
 */
int hypo_pred_fep_update(hypo_pred_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute channel priorities from drives */
    compute_channel_priorities(bridge);

    /* Compute free energy from predictions */
    float current_fe = compute_fe_from_predictions(bridge);

    /* Get FEP system metrics */
    float surprise = fep_compute_surprise(bridge->fep_system);
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);

    /* Update running averages */
    update_running_averages(bridge, current_fe, surprise, pred_error);

    /* Store in FEP effects */
    bridge->fep_effects.free_energy = current_fe;
    bridge->fep_effects.prediction_error = pred_error;
    bridge->fep_effects.precision = bridge->state.current_precision;
    bridge->fep_effects.surprise_level = surprise;

    /* Weighted surprise by dominant drive */
    float drive_weight = 1.0f;
    if (bridge->pred_effects.dominant_drive < HYPO_DRIVE_COUNT) {
        drive_weight = bridge->config.drive_to_priority_scale[bridge->pred_effects.dominant_drive];
    }
    bridge->fep_effects.weighted_surprise = surprise * drive_weight *
                                            bridge->config.surprise_amplification;

    /* Classify priority level */
    bridge->fep_effects.priority_level = classify_priority_level(
        bridge->fep_effects.total_priority,
        &bridge->config
    );

    /* Compute average accuracy */
    float total_accuracy = 0.0f;
    uint32_t active_channels = 0;
    for (int i = 0; i < HYPO_PRED_CHANNEL_COUNT; i++) {
        if (bridge->state.channels[i].active) {
            total_accuracy += bridge->state.channels[i].accuracy_rate;
            active_channels++;
        }
    }
    if (active_channels > 0) {
        bridge->fep_effects.avg_accuracy = total_accuracy / (float)active_channels;
    }
    bridge->pred_effects.active_channel_count = active_channels;

    /* FE reduction from accuracy */
    bridge->fep_effects.fe_reduction = bridge->fep_effects.avg_accuracy *
                                       bridge->config.fe_reduction_per_accuracy;

    /* Model update needed? */
    bridge->fep_effects.model_update_needed =
        (bridge->fep_effects.avg_accuracy < bridge->config.accuracy_threshold);

    /* Determine response */
    bridge->fep_effects.recommended_response = determine_response(
        bridge->fep_effects.priority_level,
        bridge->fep_effects.avg_accuracy,
        current_fe
    );

    /* Priority confidence from consistency */
    bridge->fep_effects.priority_confidence =
        bridge->state.current_precision / HYPO_PRED_FEP_MAX_PRECISION;
    if (bridge->fep_effects.priority_confidence > 1.0f) {
        bridge->fep_effects.priority_confidence = 1.0f;
    }

    /* Response urgency from FE */
    float urgency = current_fe / HYPO_PRED_FEP_CRITICAL_THRESHOLD;
    if (urgency > 1.0f) urgency = 1.0f;
    bridge->fep_effects.response_urgency = urgency;

    /* Update prediction tracking */
    update_pred_tracking(bridge);

    /* Update statistics */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    bridge->stats.current_precision = bridge->state.current_precision;

    if (current_fe > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = current_fe;
    }
    if (current_fe < bridge->stats.min_free_energy) {
        bridge->stats.min_free_energy = current_fe;
    }

    /* Track FE reduction */
    bridge->stats.total_fe_reduction += bridge->fep_effects.fe_reduction;
    if (bridge->fep_effects.fe_reduction > bridge->stats.max_fe_reduction) {
        bridge->stats.max_fe_reduction = bridge->fep_effects.fe_reduction;
    }

    /* Overall accuracy */
    if (bridge->pred_effects.total_predictions > 0) {
        bridge->stats.overall_accuracy_rate =
            (float)bridge->pred_effects.accurate_predictions /
            (float)bridge->pred_effects.total_predictions;
    }

    /* Track actions triggered */
    if (bridge->fep_effects.recommended_response >= HYPO_PRED_FEP_RESPONSE_ACT) {
        bridge->stats.actions_triggered++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Core Operations Implementation
 * ============================================================================ */

/**
 * WHAT: Compute free energy from prediction state
 * WHY:  Core FEP computation for predictive processing
 * HOW:  Map prediction errors to free energy
 */
int hypo_pred_fep_compute_fe(
    hypo_pred_fep_bridge_t* bridge,
    const hypo_drive_system_t* drives
) {
    if (!bridge || !drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_pred_fep_compute_fe: bridge or drives is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Store drive urgencies */
    float max_urgency = 0.0f;
    hypo_drive_type_t dominant = HYPO_DRIVE_HUNGER;
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->pred_effects.drive_urgencies[i] = drives->drives[i].urgency;
        if (drives->drives[i].urgency > max_urgency) {
            max_urgency = drives->drives[i].urgency;
            dominant = (hypo_drive_type_t)i;
        }
    }
    bridge->pred_effects.dominant_drive = dominant;
    bridge->pred_effects.dominant_urgency = max_urgency;

    /* Compute channel priorities from drives */
    compute_channel_priorities(bridge);

    /* Compute free energy */
    float total_fe = compute_fe_from_predictions(bridge);
    bridge->fep_effects.free_energy = total_fe;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Modulate precision for a channel
 * WHY:  Urgent drives increase precision for relevant channels
 * HOW:  Scale precision by factor
 */
int hypo_pred_fep_modulate_precision(
    hypo_pred_fep_bridge_t* bridge,
    hypo_pred_channel_t channel,
    float precision_factor
) {
    if (!bridge || channel >= HYPO_PRED_CHANNEL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_pred_fep_modulate_precision: bridge is NULL or invalid channel");
        return -1;
    }

    if (precision_factor < 0.1f) precision_factor = 0.1f;
    if (precision_factor > 10.0f) precision_factor = 10.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    float new_precision = HYPO_PRED_FEP_DEFAULT_PRECISION * precision_factor;

    /* Smooth adaptation */
    float alpha = bridge->config.precision_learning_rate;
    bridge->state.channels[channel].precision =
        (1.0f - alpha) * bridge->state.channels[channel].precision + alpha * new_precision;

    /* Clamp */
    if (bridge->state.channels[channel].precision < HYPO_PRED_FEP_MIN_PRECISION) {
        bridge->state.channels[channel].precision = HYPO_PRED_FEP_MIN_PRECISION;
    }
    if (bridge->state.channels[channel].precision > HYPO_PRED_FEP_MAX_PRECISION) {
        bridge->state.channels[channel].precision = HYPO_PRED_FEP_MAX_PRECISION;
    }

    bridge->stats.precision_adaptations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Get current FEP effects
 * WHY:  Allow inspection of current effects
 * HOW:  Copy effects structure
 */
int hypo_pred_fep_get_effects(
    const hypo_pred_fep_bridge_t* bridge,
    hypo_pred_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_pred_fep_get_effects: bridge or effects is NULL");
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
int hypo_pred_fep_get_stats(
    const hypo_pred_fep_bridge_t* bridge,
    hypo_pred_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_pred_fep_get_stats: bridge or stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Prediction Channel API Implementation
 * ============================================================================ */

/**
 * WHAT: Register a prediction for a channel
 * WHY:  Enable accuracy tracking and FE computation
 * HOW:  Store prediction value and uncertainty
 */
int hypo_pred_fep_register_prediction(
    hypo_pred_fep_bridge_t* bridge,
    hypo_pred_channel_t channel,
    float prediction,
    float variance
) {
    if (!bridge || channel >= HYPO_PRED_CHANNEL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_pred_fep_register_prediction: bridge is NULL or invalid channel");
        return -1;
    }

    if (variance < 0.0f) variance = 0.0f;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    hypo_pred_channel_state_t* ch = &bridge->state.channels[channel];

    ch->current_prediction = prediction;
    ch->prediction_variance = variance;
    ch->last_prediction_ms = nimcp_platform_time_monotonic_ms();
    ch->predictions_made++;

    bridge->pred_effects.total_predictions++;
    bridge->stats.fep_predictions++;
    bridge->stats.predictions_by_channel[channel]++;

    bridge->state.prediction_count++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report prediction outcome
 * WHY:  Learn from prediction outcomes, reduce FE
 * HOW:  Update accuracy and prediction error
 */
int hypo_pred_fep_report_outcome(
    hypo_pred_fep_bridge_t* bridge,
    hypo_pred_channel_t channel,
    float actual_value
) {
    if (!bridge || channel >= HYPO_PRED_CHANNEL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_pred_fep_report_outcome: bridge is NULL or invalid channel");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    hypo_pred_channel_state_t* ch = &bridge->state.channels[channel];

    /* Compute prediction error */
    float error = fabsf(actual_value - ch->current_prediction);

    /* Normalize error by prediction variance (if available) */
    float normalized_error = error;
    if (ch->prediction_variance > 0.01f) {
        normalized_error = error / sqrtf(ch->prediction_variance);
    }
    if (normalized_error > 1.0f) normalized_error = 1.0f;

    /* Update prediction error EMA */
    float alpha = bridge->config.accuracy_learning_rate;
    ch->prediction_error_ema =
        (1.0f - alpha) * ch->prediction_error_ema + alpha * normalized_error;

    /* Track accuracy (low error = accurate) */
    bool accurate = (normalized_error < (1.0f - bridge->config.accuracy_threshold));
    if (accurate) {
        ch->predictions_accurate++;
        bridge->pred_effects.accurate_predictions++;
        bridge->stats.accurate_predictions++;
        bridge->stats.accurate_by_channel[channel]++;

        /* FE reduction from accurate prediction */
        float fe_reduction = (1.0f - normalized_error) *
                            bridge->config.fe_reduction_per_accuracy *
                            ch->priority;
        ch->fe_reduction_cumulative += fe_reduction;
        bridge->pred_effects.total_fe_reduction += fe_reduction;

        bridge->state.pred_tracking.last_accurate_time_ms =
            nimcp_platform_time_monotonic_ms();
        bridge->state.pred_tracking.last_accurate_channel = channel;
    }

    /* Update accuracy rate */
    if (ch->predictions_made > 0) {
        ch->accuracy_rate = (float)ch->predictions_accurate / (float)ch->predictions_made;
    }

    /* Compute channel free energy */
    ch->channel_free_energy = ch->prediction_error_ema * ch->prediction_error_ema *
                              ch->priority * ch->precision *
                              bridge->config.drive_fe_weight;

    /* Overall accuracy */
    if (bridge->pred_effects.total_predictions > 0) {
        bridge->pred_effects.overall_accuracy =
            (float)bridge->pred_effects.accurate_predictions /
            (float)bridge->pred_effects.total_predictions;
    }

    ch->last_update_ms = nimcp_platform_time_monotonic_ms();
    bridge->stats.model_updates++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Get channel priority
 * WHY:  Query drive-modulated priority
 * HOW:  Return cached priority
 */
float hypo_pred_fep_get_channel_priority(
    const hypo_pred_fep_bridge_t* bridge,
    hypo_pred_channel_t channel
) {
    if (!bridge || channel >= HYPO_PRED_CHANNEL_COUNT) {
        return -1.0f;
    }

    return bridge->fep_effects.channel_priorities[channel];
}

/**
 * WHAT: Get all channel priorities
 * WHY:  Batch priority query
 * HOW:  Copy priority array
 */
int hypo_pred_fep_get_all_priorities(
    const hypo_pred_fep_bridge_t* bridge,
    float* priorities
) {
    if (!bridge || !priorities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_pred_fep_get_all_priorities: bridge or priorities is NULL");
        return -1;
    }

    for (int i = 0; i < HYPO_PRED_CHANNEL_COUNT; i++) {
        priorities[i] = bridge->fep_effects.channel_priorities[i];
    }

    return 0;
}

/**
 * WHAT: Get channel accuracy
 * WHY:  Monitor prediction performance
 * HOW:  Return cached accuracy
 */
float hypo_pred_fep_get_channel_accuracy(
    const hypo_pred_fep_bridge_t* bridge,
    hypo_pred_channel_t channel
) {
    if (!bridge || channel >= HYPO_PRED_CHANNEL_COUNT) {
        return -1.0f;
    }

    return bridge->state.channels[channel].accuracy_rate;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int hypo_pred_fep_connect_bio_async(
    hypo_pred_fep_bridge_t* bridge,
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
        .module_id = BIO_MODULE_HYPO_PREDICTIVE_FEP,
        .module_name = "hypo_predictive_fep_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hypo Predictive FEP bridge connected to bio-async");
    }

    return 0;
}

int hypo_pred_fep_disconnect_bio_async(hypo_pred_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Hypo Predictive FEP bridge disconnected from bio-async");
    return 0;
}

int hypo_pred_fep_process_messages(
    hypo_pred_fep_bridge_t* bridge,
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

const char* hypo_pred_fep_channel_name(hypo_pred_channel_t channel) {
    switch (channel) {
        case HYPO_PRED_CHANNEL_FOOD:
            return "Food";
        case HYPO_PRED_CHANNEL_WATER:
            return "Water";
        case HYPO_PRED_CHANNEL_THREAT:
            return "Threat";
        case HYPO_PRED_CHANNEL_TEMPERATURE:
            return "Temperature";
        case HYPO_PRED_CHANNEL_SOCIAL:
            return "Social";
        case HYPO_PRED_CHANNEL_MATE:
            return "Mate";
        case HYPO_PRED_CHANNEL_NOVELTY:
            return "Novelty";
        case HYPO_PRED_CHANNEL_REST:
            return "Rest";
        case HYPO_PRED_CHANNEL_GENERAL:
            return "General";
        default:
            return "Unknown";
    }
}

const char* hypo_pred_fep_level_name(hypo_pred_fep_level_t level) {
    switch (level) {
        case HYPO_PRED_FEP_LEVEL_BACKGROUND:
            return "Background";
        case HYPO_PRED_FEP_LEVEL_ROUTINE:
            return "Routine";
        case HYPO_PRED_FEP_LEVEL_ELEVATED:
            return "Elevated";
        case HYPO_PRED_FEP_LEVEL_HIGH:
            return "High";
        case HYPO_PRED_FEP_LEVEL_CRITICAL:
            return "Critical";
        default:
            return "Unknown";
    }
}

const char* hypo_pred_fep_response_name(hypo_pred_fep_response_t response) {
    switch (response) {
        case HYPO_PRED_FEP_RESPONSE_CONTINUE:
            return "Continue";
        case HYPO_PRED_FEP_RESPONSE_UPDATE:
            return "Update";
        case HYPO_PRED_FEP_RESPONSE_ATTEND:
            return "Attend";
        case HYPO_PRED_FEP_RESPONSE_ACT:
            return "Act";
        case HYPO_PRED_FEP_RESPONSE_EXPLORE:
            return "Explore";
        case HYPO_PRED_FEP_RESPONSE_EMERGENCY:
            return "Emergency";
        default:
            return "Unknown";
    }
}

void hypo_pred_fep_print_summary(const hypo_pred_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Hypo Predictive FEP Bridge: NULL\n");
        return;
    }

    printf("=== Hypothalamus Predictive FEP Bridge Summary ===\n");
    printf("State:\n");
    printf("  Active: %s\n", bridge->state.active ? "yes" : "no");
    printf("  Updates: %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Predictions: %lu\n", (unsigned long)bridge->state.prediction_count);
    printf("  Precision: %.3f\n", bridge->state.current_precision);
    printf("\n");
    printf("Channel Priorities:\n");
    for (int i = 0; i < HYPO_PRED_CHANNEL_COUNT; i++) {
        printf("  %s: %.3f (acc: %.1f%%)\n",
               hypo_pred_fep_channel_name((hypo_pred_channel_t)i),
               bridge->fep_effects.channel_priorities[i],
               bridge->state.channels[i].accuracy_rate * 100.0f);
    }
    printf("  Total: %.3f\n", bridge->fep_effects.total_priority);
    printf("  Dominant: %s (%.3f)\n",
           hypo_pred_fep_channel_name(bridge->fep_effects.dominant_channel),
           bridge->fep_effects.dominant_priority);
    printf("\n");
    printf("FEP Effects:\n");
    printf("  Free Energy: %.3f\n", bridge->fep_effects.free_energy);
    printf("  Priority Level: %s\n",
           hypo_pred_fep_level_name(bridge->fep_effects.priority_level));
    printf("  Avg Accuracy: %.1f%%\n", bridge->fep_effects.avg_accuracy * 100.0f);
    printf("  FE Reduction: %.3f\n", bridge->fep_effects.fe_reduction);
    printf("  Model Update Needed: %s\n",
           bridge->fep_effects.model_update_needed ? "yes" : "no");
    printf("  Recommended Response: %s\n",
           hypo_pred_fep_response_name(bridge->fep_effects.recommended_response));
    printf("\n");
    printf("Statistics:\n");
    printf("  Total Predictions: %lu\n", (unsigned long)bridge->stats.fep_predictions);
    printf("  Accurate: %lu\n", (unsigned long)bridge->stats.accurate_predictions);
    printf("  Accuracy Rate: %.1f%%\n", bridge->stats.overall_accuracy_rate * 100.0f);
    printf("  Total FE Reduction: %.3f\n", bridge->stats.total_fe_reduction);
    printf("  Actions Triggered: %lu\n", (unsigned long)bridge->stats.actions_triggered);
    printf("=================================================\n");
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

/**
 * WHAT: Map drive type to prediction channel
 * WHY:  Each drive prioritizes specific predictions
 * HOW:  Direct mapping based on biological basis
 */
static hypo_pred_channel_t map_drive_to_channel(hypo_drive_type_t drive) {
    switch (drive) {
        case HYPO_DRIVE_HUNGER:
            return HYPO_PRED_CHANNEL_FOOD;
        case HYPO_DRIVE_THIRST:
            return HYPO_PRED_CHANNEL_WATER;
        case HYPO_DRIVE_SAFETY:
            return HYPO_PRED_CHANNEL_THREAT;
        case HYPO_DRIVE_TEMPERATURE:
            return HYPO_PRED_CHANNEL_TEMPERATURE;
        case HYPO_DRIVE_SOCIAL:
            return HYPO_PRED_CHANNEL_SOCIAL;
        case HYPO_DRIVE_AUTONOMY:
            return HYPO_PRED_CHANNEL_MATE;
        case HYPO_DRIVE_CURIOSITY:
            return HYPO_PRED_CHANNEL_NOVELTY;
        case HYPO_DRIVE_FATIGUE:
            return HYPO_PRED_CHANNEL_REST;
        default:
            return HYPO_PRED_CHANNEL_GENERAL;
    }
}

/**
 * WHAT: Compute channel priorities from drive urgencies
 * WHY:  Drives prioritize related prediction channels
 * HOW:  Map drive urgency to channel priority via scaling
 */
static void compute_channel_priorities(hypo_pred_fep_bridge_t* bridge) {
    /* Reset priorities to base */
    for (int i = 0; i < HYPO_PRED_CHANNEL_COUNT; i++) {
        bridge->state.channels[i].priority = bridge->config.base_priority;
    }

    /* Map each drive to its channel */
    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        float urgency = bridge->pred_effects.drive_urgencies[d];
        float scale = bridge->config.drive_to_priority_scale[d];
        hypo_pred_channel_t channel = map_drive_to_channel((hypo_drive_type_t)d);

        /* Accumulate priority */
        float contribution = urgency * scale;
        bridge->state.channels[channel].priority += contribution;
    }

    /* Normalize and find dominant */
    float total = 0.0f;
    float max_priority = 0.0f;
    hypo_pred_channel_t dominant = HYPO_PRED_CHANNEL_FOOD;

    for (int i = 0; i < HYPO_PRED_CHANNEL_COUNT; i++) {
        /* Clamp */
        if (bridge->state.channels[i].priority > HYPO_PRED_MAX_PRIORITY) {
            bridge->state.channels[i].priority = HYPO_PRED_MAX_PRIORITY;
        }
        if (bridge->state.channels[i].priority < HYPO_PRED_MIN_PRIORITY) {
            bridge->state.channels[i].priority = HYPO_PRED_MIN_PRIORITY;
        }

        bridge->fep_effects.channel_priorities[i] = bridge->state.channels[i].priority;
        total += bridge->state.channels[i].priority;

        if (bridge->state.channels[i].priority > max_priority) {
            max_priority = bridge->state.channels[i].priority;
            dominant = (hypo_pred_channel_t)i;
        }
    }

    bridge->fep_effects.total_priority = total;
    bridge->fep_effects.dominant_channel = dominant;
    bridge->fep_effects.dominant_priority = max_priority;
}

/**
 * WHAT: Compute free energy from prediction state
 * WHY:  Map prediction errors to FEP domain
 * HOW:  Priority-weighted sum of prediction errors
 */
static float compute_fe_from_predictions(const hypo_pred_fep_bridge_t* bridge) {
    float fe = 0.0f;

    /* FE from each channel */
    for (int i = 0; i < HYPO_PRED_CHANNEL_COUNT; i++) {
        if (bridge->state.channels[i].active) {
            /* FE = priority * precision * error^2 */
            float error = bridge->state.channels[i].prediction_error_ema;
            float priority = bridge->state.channels[i].priority;
            float precision = bridge->state.channels[i].precision;

            fe += priority * precision * error * error * bridge->config.drive_fe_weight;
        }
    }

    /* Subtract FE reduction from accuracy (accurate predictions reduce FE) */
    float reduction = bridge->fep_effects.avg_accuracy *
                     bridge->config.fe_reduction_per_accuracy;
    fe -= reduction;

    if (fe < 0.0f) fe = 0.0f;

    return fe;
}

/**
 * WHAT: Classify priority level from total priority
 * WHY:  Map continuous priority to discrete categories
 * HOW:  Threshold-based classification
 */
static hypo_pred_fep_level_t classify_priority_level(
    float total_priority,
    const hypo_pred_fep_config_t* config
) {
    (void)config;

    if (total_priority >= 4.0f) {
        return HYPO_PRED_FEP_LEVEL_CRITICAL;
    } else if (total_priority >= 3.0f) {
        return HYPO_PRED_FEP_LEVEL_HIGH;
    } else if (total_priority >= 2.0f) {
        return HYPO_PRED_FEP_LEVEL_ELEVATED;
    } else if (total_priority >= 1.0f) {
        return HYPO_PRED_FEP_LEVEL_ROUTINE;
    } else {
        return HYPO_PRED_FEP_LEVEL_BACKGROUND;
    }
}

/**
 * WHAT: Determine appropriate response
 * WHY:  Active inference selects prediction action
 * HOW:  Map priority level, accuracy, and FE to response type
 */
static hypo_pred_fep_response_t determine_response(
    hypo_pred_fep_level_t level,
    float avg_accuracy,
    float free_energy
) {
    /* Emergency for critical situations */
    if (level == HYPO_PRED_FEP_LEVEL_CRITICAL && free_energy > 15.0f) {
        return HYPO_PRED_FEP_RESPONSE_EMERGENCY;
    }

    /* Low accuracy = update model or explore */
    if (avg_accuracy < 0.4f) {
        if (level >= HYPO_PRED_FEP_LEVEL_HIGH) {
            return HYPO_PRED_FEP_RESPONSE_EXPLORE;
        }
        return HYPO_PRED_FEP_RESPONSE_UPDATE;
    }

    /* High FE = take action */
    if (free_energy > 10.0f) {
        return HYPO_PRED_FEP_RESPONSE_ACT;
    }

    /* Moderate accuracy or elevated priority = attend */
    if (avg_accuracy < 0.7f || level >= HYPO_PRED_FEP_LEVEL_ELEVATED) {
        return HYPO_PRED_FEP_RESPONSE_ATTEND;
    }

    /* Good accuracy and low priority = continue */
    return HYPO_PRED_FEP_RESPONSE_CONTINUE;
}

/**
 * WHAT: Update running averages for metrics
 * WHY:  Smooth metrics over time for stability
 * HOW:  Exponential moving average
 */
static void update_running_averages(
    hypo_pred_fep_bridge_t* bridge,
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
 * WHAT: Update prediction tracking
 * WHY:  Track prediction evolution for adaptation
 * HOW:  Store history, compute velocity
 */
static void update_pred_tracking(hypo_pred_fep_bridge_t* bridge) {
    hypo_pred_tracking_t* tracking = &bridge->state.pred_tracking;

    /* Store in history */
    uint32_t idx = tracking->history_idx;

    /* Store priorities history */
    for (int i = 0; i < HYPO_PRED_CHANNEL_COUNT; i++) {
        tracking->priority_history[i][idx] = bridge->fep_effects.channel_priorities[i];
    }

    /* Store FE history */
    tracking->fe_history[idx] = bridge->fep_effects.free_energy;

    /* Update FE prediction using EMA */
    float alpha = 0.1f;
    tracking->predicted_fe =
        (1.0f - alpha) * tracking->predicted_fe +
        alpha * bridge->fep_effects.free_energy;

    /* Compute FE velocity */
    if (idx > 0) {
        float prev_fe = tracking->fe_history[(idx - 1) % 16];
        tracking->fe_velocity = bridge->fep_effects.free_energy - prev_fe;
    }

    tracking->history_idx = (idx + 1) % 16;
}
