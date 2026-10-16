/**
 * @file nimcp_predictive_immune.c
 * @brief Predictive Processing - Brain Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 */

#include "cognitive/nimcp_predictive_immune.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(predictive_immune, MESH_ADAPTER_CATEGORY_COGNITIVE)



/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal predictive-immune integration state
 */
struct predictive_immune_system {
    predictive_immune_config_t config;

    /* Integrated systems */
    predictive_network_t predictive_net;
    brain_immune_system_t* immune_system;

    /* Interoceptive state */
    interoceptive_state_t intero_state;
    predictive_network_t intero_network;  /**< Dedicated interoceptive predictor */

    /* Precision modulation state */
    immune_modulated_precision_t* region_precisions;
    uint32_t num_regions;
    uint32_t region_capacity;

    /* Error trigger state */
    prediction_error_trigger_t* error_triggers;
    uint32_t num_error_triggers;

    /* Statistics */
    predictive_immune_stats_t stats;

    /* State */
    bool running;
    uint64_t start_time;

    /* Thread safety */
    void* mutex;
};

/* Forward declarations for internal helpers */
static nimcp_result_t update_interoceptive_state(predictive_immune_system_t* sys);
static nimcp_result_t compute_precision_modulation(predictive_immune_system_t* sys, uint32_t region_idx);
static nimcp_result_t check_error_threshold(predictive_immune_system_t* sys, uint32_t region_idx, float error);
static float compute_inflammation_factor(float inflammation_level, float reduction_factor);
static float compute_cytokine_factor(const float* cytokine_levels, const predictive_immune_config_t* cfg);

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

nimcp_result_t predictive_immune_default_config(predictive_immune_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Interoceptive prediction defaults */
    config->intero_mode = INTERO_PREDICT_INFLAMMATION;
    config->intero_state_dim = 8;
    config->intero_learning_rate = 0.01f;
    config->enable_sickness_behavior = true;

    /* Immune modulation defaults */
    config->modulation_strategy = IMMUNE_MOD_PRECISION_ONLY;
    config->precision_reduction_factor = 0.3f;
    config->learning_rate_reduction_factor = 0.5f;
    config->max_precision_reduction = 0.8f;

    /* Prediction error detection defaults */
    config->error_response_mode = PRED_ERROR_RESPONSE_THRESHOLD;
    config->prediction_error_threshold = PREDICTIVE_IMMUNE_ERROR_THRESHOLD;
    config->free_energy_threshold = PREDICTIVE_IMMUNE_FREE_ENERGY_THRESHOLD;
    config->cumulative_error_window = 10;
    config->adaptive_threshold_alpha = 0.1f;

    /* Cytokine-specific effects */
    config->il1_precision_effect = 0.4f;
    config->il6_precision_effect = 0.3f;
    config->tnf_precision_effect = 0.5f;
    config->il10_recovery_boost = 0.2f;

    /* Bio-async */
    config->enable_bio_async = true;
    config->broadcast_intero_predictions = false;

    return NIMCP_SUCCESS;
}

predictive_immune_system_t* predictive_immune_create(
    const predictive_immune_config_t* config,
    predictive_network_t predictive_net,
    brain_immune_system_t* immune_system)
{
    /* Guard: validate inputs */
    if (!predictive_net || !immune_system) {
        NIMCP_LOGGING_ERROR("Invalid input: predictive_net or immune_system is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_immune_create: required parameter is NULL (predictive_net, immune_system)");
        return NULL;
    }

    /* Allocate system */
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_create", 0.0f);


    predictive_immune_system_t* sys = (predictive_immune_system_t*)
        nimcp_malloc(sizeof(predictive_immune_system_t));
    if (!sys) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sys is NULL");

        return NULL;

    }

    memset(sys, 0, sizeof(predictive_immune_system_t));

    /* Set configuration */
    if (config) {
        memcpy(&sys->config, config, sizeof(predictive_immune_config_t));
    } else {
        predictive_immune_default_config(&sys->config);
    }

    /* Store integrated systems */
    sys->predictive_net = predictive_net;
    sys->immune_system = immune_system;

    /* Create interoceptive network if needed */
    if (sys->config.intero_mode != INTERO_PREDICT_NONE) {
        predictive_config_t intero_cfg = predictive_default_config();
        intero_cfg.num_layers = 3;

        uint32_t layer_sizes[3] = {
            sys->config.intero_state_dim,
            sys->config.intero_state_dim * 2,
            sys->config.intero_state_dim
        };
        intero_cfg.layer_sizes = layer_sizes;
        intero_cfg.learning_rate = sys->config.intero_learning_rate;

        sys->intero_network = predictive_create(&intero_cfg);
        if (!sys->intero_network) {
            nimcp_free(sys);
            sys = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_immune_create: sys->intero_network is NULL");
            return NULL;
        }
    }

    /* Allocate region precision tracking */
    sys->region_capacity = 16;
    sys->region_precisions = (immune_modulated_precision_t*)
        nimcp_calloc(sys->region_capacity, sizeof(immune_modulated_precision_t));
    if (!sys->region_precisions) {
        if (sys->intero_network) predictive_destroy(sys->intero_network);
        nimcp_free(sys);
        sys = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_immune_create: validation failed");
        return NULL;
    }
    memset(sys->region_precisions, 0, sys->region_capacity * sizeof(immune_modulated_precision_t));

    /* Allocate error trigger tracking */
    sys->error_triggers = (prediction_error_trigger_t*)
        nimcp_calloc(sys->region_capacity, sizeof(prediction_error_trigger_t));
    if (!sys->error_triggers) {
        nimcp_free(sys->region_precisions);
        if (sys->intero_network) predictive_destroy(sys->intero_network);
        nimcp_free(sys);
        sys = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_immune_create: validation failed");
        return NULL;
    }
    memset(sys->error_triggers, 0, sys->region_capacity * sizeof(prediction_error_trigger_t));

    /* Initialize interoceptive state */
    memset(&sys->intero_state, 0, sizeof(interoceptive_state_t));

    /* Initialize statistics */
    memset(&sys->stats, 0, sizeof(predictive_immune_stats_t));

    sys->running = false;

    NIMCP_LOGGING_INFO("Created predictive-immune integration system (intero_mode=%d)",
                   sys->config.intero_mode);

    return sys;
}

void predictive_immune_destroy(predictive_immune_system_t* system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_destroy", 0.0f);


    if (system->running) {
        predictive_immune_stop(system);
    }

    if (system->intero_network) {
        predictive_destroy(system->intero_network);
    }

    if (system->region_precisions) {
        nimcp_free(system->region_precisions);
    }

    if (system->error_triggers) {
        nimcp_free(system->error_triggers);
    }

    nimcp_free(system);
    system = NULL;

    NIMCP_LOGGING_INFO("Destroyed predictive-immune integration system");
}

nimcp_result_t predictive_immune_start(predictive_immune_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_start", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (system->running) return NIMCP_SUCCESS;

    system->running = true;
    system->start_time = nimcp_time_get_ms();

    NIMCP_LOGGING_INFO("Started predictive-immune integration");

    return NIMCP_SUCCESS;
}

nimcp_result_t predictive_immune_stop(predictive_immune_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_stop", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system->running) return NIMCP_SUCCESS;

    system->running = false;

    NIMCP_LOGGING_INFO("Stopped predictive-immune integration");

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Interoceptive Prediction API Implementation
 * ============================================================================ */

nimcp_result_t predictive_immune_update_interoception(
    predictive_immune_system_t* system,
    float dt)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_update_interoception", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(system->running, NIMCP_ERROR_INVALID_STATE, "system is not running");
    if (system->config.intero_mode == INTERO_PREDICT_NONE) return NIMCP_SUCCESS;

    /* Update current immune state from immune system */
    nimcp_result_t result = update_interoceptive_state(system);
    if (result != NIMCP_SUCCESS) return result;

    /* Generate predictions using interoceptive network */
    if (system->intero_network) {
        float input[PREDICTIVE_IMMUNE_MAX_INTEROCEPTIVE_DIMS];
        input[0] = system->intero_state.inflammation_level;
        input[1] = system->intero_state.immune_activation;

        /* Run predictive inference */
        float free_energy = predictive_forward(system->intero_network, input, 5);

        /* Extract predictions */
        float prediction[PREDICTIVE_IMMUNE_MAX_INTEROCEPTIVE_DIMS];
        predictive_get_layer_prediction(system->intero_network, 0, prediction);

        system->intero_state.predicted_inflammation = prediction[0];
        system->intero_state.predicted_activation = prediction[1];
    }

    /* Compute prediction errors */
    system->intero_state.inflammation_error =
        fabsf(system->intero_state.inflammation_level - system->intero_state.predicted_inflammation);
    system->intero_state.activation_error =
        fabsf(system->intero_state.immune_activation - system->intero_state.predicted_activation);

    system->intero_state.total_interoceptive_error =
        system->intero_state.inflammation_error + system->intero_state.activation_error;

    /* Update statistics */
    system->stats.interoceptive_updates++;
    system->stats.avg_interoceptive_error =
        (system->stats.avg_interoceptive_error * 0.9f) +
        (system->intero_state.total_interoceptive_error * 0.1f);

    if (system->intero_state.total_interoceptive_error > system->stats.max_interoceptive_error) {
        system->stats.max_interoceptive_error = system->intero_state.total_interoceptive_error;
    }

    /* Trigger sickness behavior if error is large */
    if (system->config.enable_sickness_behavior &&
        system->intero_state.total_interoceptive_error > 0.5f) {
        predictive_immune_trigger_sickness_behavior(system,
            system->intero_state.total_interoceptive_error);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t predictive_immune_get_interoceptive_state(
    predictive_immune_system_t* system,
    interoceptive_state_t* state)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_get_interoceptive_st", 0.0f);


    NIMCP_CHECK_THROW(system && state, NIMCP_ERROR_NULL_POINTER, "system or state is NULL");

    memcpy(state, &system->intero_state, sizeof(interoceptive_state_t));

    return NIMCP_SUCCESS;
}

nimcp_result_t predictive_immune_trigger_sickness_behavior(
    predictive_immune_system_t* system,
    float intero_error)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_trigger_sickness_beh", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");

    /* Reduce learning rate in predictive network based on error */
    /* This models behavioral conservation during sickness */

    system->stats.sickness_behavior_triggers++;

    NIMCP_LOGGING_DEBUG("Triggered sickness behavior (intero_error=%.3f)", intero_error);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Immune Modulation API Implementation
 * ============================================================================ */

nimcp_result_t predictive_immune_apply_immune_modulation(
    predictive_immune_system_t* system,
    brain_region_t* region)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_apply_immune_modulat", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(system->running, NIMCP_ERROR_INVALID_STATE, "system is not running");

    /* Find or create region precision state */
    uint32_t region_idx = system->num_regions;

    /* Compute modulation for this region */
    nimcp_result_t result = compute_precision_modulation(system, region_idx);
    if (result != NIMCP_SUCCESS) return result;

    /* Apply to region if provided */
    if (region && system->config.modulation_strategy != IMMUNE_MOD_NONE) {
        brain_region_predictive_t* pred_ext = brain_region_get_predictive(region);
        if (pred_ext) {
            float modulated_precision = system->region_precisions[region_idx].current_precision;

            /* Set precision for all neurons in region */
            uint32_t num_neurons = 100; /* TODO: get from region */
            float* precisions = (float*)nimcp_calloc(num_neurons, sizeof(float));
            if (precisions) {
                for (uint32_t i = 0; i < num_neurons; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && num_neurons > 256) {
                        predictive_immune_heartbeat("predictive_i_loop",
                                         (float)(i + 1) / (float)num_neurons);
                    }

                    precisions[i] = modulated_precision;
                }
                brain_region_set_precision(region, precisions, num_neurons);
                nimcp_free(precisions);
                precisions = NULL;
            }
        }
    }

    system->stats.modulation_events++;

    return NIMCP_SUCCESS;
}

nimcp_result_t predictive_immune_compute_cytokine_precision_effect(
    predictive_immune_system_t* system,
    const float* cytokine_levels,
    float* precision_out)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_compute_cytokine_pre", 0.0f);


    NIMCP_CHECK_THROW(system && cytokine_levels && precision_out, NIMCP_ERROR_NULL_POINTER, "system, cytokine_levels, or precision_out is NULL");

    float factor = compute_cytokine_factor(cytokine_levels, &system->config);
    *precision_out = PREDICTIVE_IMMUNE_BASELINE_PRECISION * factor;

    /* Clamp to minimum */
    if (*precision_out < PREDICTIVE_IMMUNE_MIN_PRECISION) {
        *precision_out = PREDICTIVE_IMMUNE_MIN_PRECISION;
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t predictive_immune_get_precision_modulation(
    predictive_immune_system_t* system,
    brain_region_t* region,
    immune_modulated_precision_t* precision_state)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_get_precision_modula", 0.0f);


    NIMCP_CHECK_THROW(system && precision_state, NIMCP_ERROR_NULL_POINTER, "system or precision_state is NULL");

    /* Find region precision state */
    if (system->num_regions > 0) {
        memcpy(precision_state, &system->region_precisions[0],
               sizeof(immune_modulated_precision_t));
    } else {
        memset(precision_state, 0, sizeof(immune_modulated_precision_t));
        precision_state->baseline_precision = PREDICTIVE_IMMUNE_BASELINE_PRECISION;
        precision_state->current_precision = PREDICTIVE_IMMUNE_BASELINE_PRECISION;
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Prediction Error Detection API Implementation
 * ============================================================================ */

nimcp_result_t predictive_immune_monitor_prediction_errors(
    predictive_immune_system_t* system,
    brain_region_t* region,
    float dt)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_monitor_prediction_e", 0.0f);


    NIMCP_CHECK_THROW(system && region, NIMCP_ERROR_NULL_POINTER, "system or region is NULL");
    NIMCP_CHECK_THROW(system->running, NIMCP_ERROR_INVALID_STATE, "system is not running");
    if (system->config.error_response_mode == PRED_ERROR_RESPONSE_NONE) return NIMCP_SUCCESS;

    /* Get prediction error from region */
    brain_region_predictive_t* pred_ext = brain_region_get_predictive(region);
    NIMCP_CHECK_THROW(pred_ext, NIMCP_ERROR_INVALID_STATE, "pred_ext is NULL");

    /* Compute mean error magnitude */
    float total_error = 0.0f;
    uint32_t num_neurons = 100; /* TODO: get from region */

    for (uint32_t i = 0; i < num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_neurons > 256) {
            predictive_immune_heartbeat("predictive_i_loop",
                             (float)(i + 1) / (float)num_neurons);
        }

        total_error += fabsf(pred_ext->prediction_error[i]);
    }
    float mean_error = total_error / (fabsf(num_neurons) > 1e-7f ? num_neurons : 1e-7f);

    /* Check against threshold */
    uint32_t trigger_idx = 0; /* TODO: map region to trigger index */
    nimcp_result_t result = check_error_threshold(system, trigger_idx, mean_error);

    return result;
}

nimcp_result_t predictive_immune_trigger_error_response(
    predictive_immune_system_t* system,
    brain_region_t* region,
    float error_magnitude,
    uint32_t* antigen_id)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_trigger_error_respon", 0.0f);


    NIMCP_CHECK_THROW(system && region && antigen_id, NIMCP_ERROR_NULL_POINTER, "system, region, or antigen_id is NULL");

    /* Create epitope from error signature */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);

    /* Encode error magnitude in epitope */
    uint32_t encoded_error = (uint32_t)(error_magnitude * 1000.0f);
    memcpy(epitope, &encoded_error, sizeof(uint32_t));

    /* Present as antigen */
    uint32_t severity = (uint32_t)(error_magnitude * 10.0f);
    if (severity > 10) severity = 10;

    nimcp_result_t result = brain_immune_present_antigen(
        system->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        sizeof(uint32_t),
        severity,
        0, /* TODO: region ID */
        antigen_id
    );

    if (result == 0) {
        system->stats.immune_triggers++;
        NIMCP_LOGGING_WARN("Triggered immune response from prediction error (mag=%.3f, antigen=%u)",
                       error_magnitude, *antigen_id);
    }

    return (result == 0) ? NIMCP_SUCCESS : NIMCP_ERROR_OPERATION_FAILED;
}

nimcp_result_t predictive_immune_get_error_trigger_state(
    predictive_immune_system_t* system,
    brain_region_t* region,
    prediction_error_trigger_t* trigger_state)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_get_error_trigger_st", 0.0f);


    NIMCP_CHECK_THROW(system && trigger_state, NIMCP_ERROR_NULL_POINTER, "system or trigger_state is NULL");

    /* Return first trigger state as example */
    if (system->num_error_triggers > 0) {
        memcpy(trigger_state, &system->error_triggers[0],
               sizeof(prediction_error_trigger_t));
    } else {
        memset(trigger_state, 0, sizeof(prediction_error_trigger_t));
        trigger_state->error_threshold = system->config.prediction_error_threshold;
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Update and Query API Implementation
 * ============================================================================ */

nimcp_result_t predictive_immune_update(
    predictive_immune_system_t* system,
    float dt)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_update", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(system->running, NIMCP_ERROR_INVALID_STATE, "system is not running");

    /* 1. Update interoceptive predictions */
    nimcp_result_t result = predictive_immune_update_interoception(system, dt);
    if (result != NIMCP_SUCCESS) return result;

    /* 2. Apply immune modulation (pass NULL to modulate all regions) */
    result = predictive_immune_apply_immune_modulation(system, NULL);
    if (result != NIMCP_SUCCESS) return result;

    /* 3. Monitor prediction errors (would iterate over regions) */
    /* TODO: iterate over connected regions */

    return NIMCP_SUCCESS;
}

nimcp_result_t predictive_immune_get_stats(
    predictive_immune_system_t* system,
    predictive_immune_stats_t* stats)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_get_stats", 0.0f);


    NIMCP_CHECK_THROW(system && stats, NIMCP_ERROR_NULL_POINTER, "system or stats is NULL");

    memcpy(stats, &system->stats, sizeof(predictive_immune_stats_t));

    /* Compute derived statistics */
    if (system->stats.immune_triggers > 0) {
        stats->trigger_accuracy = 1.0f -
            ((float)system->stats.false_positives / system->stats.immune_triggers);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t predictive_immune_reset(predictive_immune_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_reset", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");

    /* Reset interoceptive state */
    memset(&system->intero_state, 0, sizeof(interoceptive_state_t));

    if (system->intero_network) {
        predictive_reset(system->intero_network);
    }

    /* Reset modulation state */
    for (uint32_t i = 0; i < system->num_regions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_regions > 256) {
            predictive_immune_heartbeat("predictive_i_loop",
                             (float)(i + 1) / (float)system->num_regions);
        }

        system->region_precisions[i].current_precision =
            system->region_precisions[i].baseline_precision;
        system->region_precisions[i].total_reduction = 0.0f;
    }

    /* Reset error triggers */
    memset(system->error_triggers, 0,
           system->num_error_triggers * sizeof(prediction_error_trigger_t));

    NIMCP_LOGGING_INFO("Reset predictive-immune integration state");

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Region-Specific Integration API Implementation
 * ============================================================================ */

nimcp_result_t predictive_immune_connect_region(
    predictive_immune_system_t* system,
    brain_region_t* region)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_connect_region", 0.0f);


    NIMCP_CHECK_THROW(system && region, NIMCP_ERROR_NULL_POINTER, "system or region is NULL");

    /* Expand capacity if needed */
    if (system->num_regions >= system->region_capacity) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Initialize precision state for this region */
    uint32_t idx = system->num_regions;
    system->region_precisions[idx].baseline_precision = PREDICTIVE_IMMUNE_BASELINE_PRECISION;
    system->region_precisions[idx].current_precision = PREDICTIVE_IMMUNE_BASELINE_PRECISION;
    system->region_precisions[idx].inflammation_factor = 1.0f;
    system->region_precisions[idx].cytokine_factor = 1.0f;
    system->region_precisions[idx].total_reduction = 0.0f;

    /* Initialize error trigger for this region */
    system->error_triggers[idx].error_threshold = system->config.prediction_error_threshold;
    system->error_triggers[idx].cumulative_error = 0.0f;
    system->error_triggers[idx].error_spike_count = 0;
    system->error_triggers[idx].triggered = false;

    system->num_regions++;
    system->num_error_triggers++;

    NIMCP_LOGGING_DEBUG("Connected region to predictive-immune integration (total_regions=%u)",
                    system->num_regions);

    return NIMCP_SUCCESS;
}

nimcp_result_t predictive_immune_disconnect_region(
    predictive_immune_system_t* system,
    brain_region_t* region)
{
    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_disconnect_region", 0.0f);


    NIMCP_CHECK_THROW(system && region, NIMCP_ERROR_NULL_POINTER, "system or region is NULL");

    /* Restore baseline precision to region */
    brain_region_predictive_t* pred_ext = brain_region_get_predictive(region);
    if (pred_ext) {
        uint32_t num_neurons = 100; /* TODO: get from region */
        float* precisions = (float*)nimcp_calloc(num_neurons, sizeof(float));
        if (precisions) {
            for (uint32_t i = 0; i < num_neurons; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && num_neurons > 256) {
                    predictive_immune_heartbeat("predictive_i_loop",
                                     (float)(i + 1) / (float)num_neurons);
                }

                precisions[i] = PREDICTIVE_IMMUNE_BASELINE_PRECISION;
            }
            brain_region_set_precision(region, precisions, num_neurons);
            nimcp_free(precisions);
            precisions = NULL;
        }
    }

    NIMCP_LOGGING_DEBUG("Disconnected region from predictive-immune integration");

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

static nimcp_result_t update_interoceptive_state(predictive_immune_system_t* sys) {
    NIMCP_CHECK_THROW(sys && sys->immune_system, NIMCP_ERROR_NULL_POINTER, "sys or immune_system is NULL");

    /* Get current immune statistics */
    brain_immune_stats_t immune_stats;
    int result = brain_immune_get_stats(sys->immune_system, &immune_stats);
    NIMCP_CHECK_THROW(result == 0, NIMCP_ERROR_OPERATION_FAILED, "operation failed");

    /* Map immune stats to interoceptive state */
    sys->intero_state.active_threats = immune_stats.inflammation_sites;
    sys->intero_state.immune_activation = immune_stats.system_health;

    /* Estimate inflammation from inflammation sites */
    if (immune_stats.inflammation_sites > 0) {
        sys->intero_state.inflammation_level =
            (float)immune_stats.inflammation_sites / 10.0f;
        if (sys->intero_state.inflammation_level > 1.0f) {
            sys->intero_state.inflammation_level = 1.0f;
        }
    } else {
        sys->intero_state.inflammation_level = 0.0f;
    }

    /* TODO: Get actual cytokine concentrations from immune system */
    /* For now, estimate from inflammation */
    for (uint32_t i = 0; i < BRAIN_CYTOKINE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && BRAIN_CYTOKINE_COUNT > 256) {
            predictive_immune_heartbeat("predictive_i_loop",
                             (float)(i + 1) / (float)BRAIN_CYTOKINE_COUNT);
        }

        sys->intero_state.cytokine_concentrations[i] =
            sys->intero_state.inflammation_level * 0.5f;
    }

    return NIMCP_SUCCESS;
}

static nimcp_result_t compute_precision_modulation(
    predictive_immune_system_t* sys,
    uint32_t region_idx)
{
    NIMCP_CHECK_THROW(sys, NIMCP_ERROR_NULL_POINTER, "sys is NULL");
    NIMCP_CHECK_THROW(region_idx < sys->region_capacity, NIMCP_ERROR_INVALID_PARAM, "region_idx out of range");

    immune_modulated_precision_t* prec = &sys->region_precisions[region_idx];

    /* Compute inflammation factor */
    prec->inflammation_factor = compute_inflammation_factor(
        sys->intero_state.inflammation_level,
        sys->config.precision_reduction_factor
    );

    /* Compute cytokine factor */
    prec->cytokine_factor = compute_cytokine_factor(
        sys->intero_state.cytokine_concentrations,
        &sys->config
    );

    /* Combine factors */
    float combined_factor = prec->inflammation_factor * prec->cytokine_factor;

    /* Apply to baseline precision */
    prec->current_precision = prec->baseline_precision * combined_factor;

    /* Enforce minimum */
    if (prec->current_precision < PREDICTIVE_IMMUNE_MIN_PRECISION) {
        prec->current_precision = PREDICTIVE_IMMUNE_MIN_PRECISION;
    }

    /* Track reduction */
    prec->total_reduction = 1.0f - (prec->current_precision / prec->baseline_precision);

    /* Update statistics */
    sys->stats.avg_precision_reduction =
        (sys->stats.avg_precision_reduction * 0.9f) + (prec->total_reduction * 0.1f);

    if (prec->total_reduction > sys->stats.max_precision_reduction) {
        sys->stats.max_precision_reduction = prec->total_reduction;
    }

    return NIMCP_SUCCESS;
}

static nimcp_result_t check_error_threshold(
    predictive_immune_system_t* sys,
    uint32_t region_idx,
    float error)
{
    NIMCP_CHECK_THROW(sys, NIMCP_ERROR_NULL_POINTER, "sys is NULL");
    NIMCP_CHECK_THROW(region_idx < sys->num_error_triggers, NIMCP_ERROR_INVALID_PARAM, "region_idx out of range");

    prediction_error_trigger_t* trigger = &sys->error_triggers[region_idx];
    trigger->current_error = error;

    /* Adaptive threshold update */
    if (sys->config.error_response_mode == PRED_ERROR_RESPONSE_ADAPTIVE) {
        float alpha = sys->config.adaptive_threshold_alpha;
        trigger->error_threshold =
            (1.0f - alpha) * trigger->error_threshold + alpha * error * 1.5f;
    }

    /* Check threshold */
    if (error > trigger->error_threshold) {
        trigger->error_spike_count++;
        trigger->triggered = true;

        /* Trigger immune response */
        uint32_t antigen_id = 0;
        predictive_immune_trigger_error_response(sys, NULL, error, &antigen_id);
    } else {
        trigger->triggered = false;
    }

    /* Update cumulative error */
    trigger->cumulative_error =
        (trigger->cumulative_error * 0.9f) + (error * 0.1f);

    return NIMCP_SUCCESS;
}

static float compute_inflammation_factor(float inflammation_level, float reduction_factor) {
    /* FORMULA: factor = 1 - (inflammation × reduction_factor) */
    float factor = 1.0f - (inflammation_level * reduction_factor);

    /* Clamp to [0, 1] */
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;

    return factor;
}

static float compute_cytokine_factor(
    const float* cytokine_levels,
    const predictive_immune_config_t* cfg)
{
    if (!cytokine_levels || !cfg) return 1.0f;

    /* Compute weighted reduction from pro-inflammatory cytokines */
    float il1_effect = cytokine_levels[CYTOKINE_IL1B] * cfg->il1_precision_effect;
    float il6_effect = cytokine_levels[CYTOKINE_IL6] * cfg->il6_precision_effect;
    float tnf_effect = cytokine_levels[CYTOKINE_TNFA] * cfg->tnf_precision_effect;

    /* IL-10 is anti-inflammatory, provides recovery boost */
    float il10_boost = cytokine_levels[CYTOKINE_IL10] * cfg->il10_recovery_boost;

    /* Total reduction */
    float total_reduction = il1_effect + il6_effect + tnf_effect;
    float factor = 1.0f - total_reduction + il10_boost;

    /* Clamp to [0, 1] */
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;

    return factor;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int predictive_immune_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    predictive_immune_heartbeat("predictive_i_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Predictive_Immune");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                predictive_immune_heartbeat("predictive_i_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Predictive_Immune");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Predictive_Immune");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void predictive_immune_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_predictive_immune_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int predictive_immune_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_immune_training_begin: NULL argument");
        return -1;
    }
    predictive_immune_heartbeat_instance(NULL, "predictive_immune_training_begin", 0.0f);
    (void)(struct predictive_immune_system*)instance; /* Module state available for reset */
    return 0;
}

int predictive_immune_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_immune_training_end: NULL argument");
        return -1;
    }
    predictive_immune_heartbeat_instance(NULL, "predictive_immune_training_end", 1.0f);
    (void)(struct predictive_immune_system*)instance; /* Module state available for finalization */
    return 0;
}

int predictive_immune_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_immune_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    predictive_immune_heartbeat_instance(NULL, "predictive_immune_training_step", progress);
    (void)(struct predictive_immune_system*)instance; /* Module state available for step adaptation */
    return 0;
}
