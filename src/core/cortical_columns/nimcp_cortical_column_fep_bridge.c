/**
 * @file nimcp_cortical_column_fep_bridge.c
 * @brief FEP Bridge for Cortical Columns - Implementation
 */

#include "core/cortical_columns/nimcp_cortical_column_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_column_fep_bridge)

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int cortical_column_fep_default_config(cortical_column_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(*config));
    config->belief_to_activation_gain = 1.0f;
    config->activation_to_belief_gain = 0.1f;
    config->precision_to_inhibition_gain = 0.5f;
    config->enable_precision_learning = true;
    config->prediction_error_threshold = 0.1f;
    config->enable_error_backprop = true;
    config->hierarchy_level = 0;
    config->level_precision_scaling = 1.0f;
    config->belief_learning_rate = FEP_DEFAULT_BELIEF_LR;
    config->precision_learning_rate = FEP_DEFAULT_PRECISION_LR;
    config->surprise_threshold = FEP_SURPRISE_THRESHOLD;
    config->convergence_threshold = FEP_CONVERGENCE_THRESHOLD;

    return 0;
}

cortical_column_fep_bridge_t* cortical_column_fep_create(
    const cortical_column_fep_config_t* config,
    hypercolumn_t* hypercolumn,
    fep_system_t* fep_system)
{
    if (!hypercolumn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypercolumn is NULL");
        return NULL;
    }
    if (!fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_system is NULL");
        return NULL;
    }

    cortical_column_fep_bridge_t* bridge = (cortical_column_fep_bridge_t*)
        nimcp_malloc(sizeof(cortical_column_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate cortical column FEP bridge");

        return NULL;

    }

    memset(bridge, 0, sizeof(*bridge));

    /* Set configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(*config));
    } else {
        cortical_column_fep_default_config(&bridge->config);
    }

    /* Connect systems */
    bridge->hypercolumn = hypercolumn;
    bridge->fep_system = fep_system;

    /* Get number of minicolumns */
    cc_hypercolumn_stats_t hc_stats;
    hypercolumn_get_stats(hypercolumn, &hc_stats);
    bridge->state.num_minicolumns = hc_stats.num_minicolumns;

    /* Allocate per-minicolumn states */
    if (bridge->state.num_minicolumns > 0) {
        bridge->state.minicolumn_states = (minicolumn_fep_state_t*)
            nimcp_malloc(sizeof(minicolumn_fep_state_t) * bridge->state.num_minicolumns);
        bridge->state.belief_distribution = (float*)
            nimcp_malloc(sizeof(float) * bridge->state.num_minicolumns);
        bridge->state.prior_distribution = (float*)
            nimcp_malloc(sizeof(float) * bridge->state.num_minicolumns);
        bridge->state.prediction_errors = (float*)
            nimcp_malloc(sizeof(float) * bridge->state.num_minicolumns);

        if (!bridge->state.minicolumn_states || !bridge->state.belief_distribution ||
            !bridge->state.prior_distribution || !bridge->state.prediction_errors) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate minicolumn states");
            cortical_column_fep_destroy(bridge);
            return NULL;
        }

        /* Initialize uniform prior */
        float uniform_prior = 1.0f / bridge->state.num_minicolumns;
        for (uint32_t i = 0; i < bridge->state.num_minicolumns; i++) {
            memset(&bridge->state.minicolumn_states[i], 0, sizeof(minicolumn_fep_state_t));
            bridge->state.minicolumn_states[i].belief_precision = FEP_DEFAULT_PRECISION;
            bridge->state.belief_distribution[i] = uniform_prior;
            bridge->state.prior_distribution[i] = uniform_prior;
            bridge->state.prediction_errors[i] = 0.0f;
        }
    }

    /* Initialize precision */
    bridge->state.sensory_precision = FEP_DEFAULT_PRECISION;
    bridge->state.prior_precision = FEP_DEFAULT_PRECISION;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "cortical_column_fep") != 0) {
        if (bridge->state.minicolumn_states) nimcp_free(bridge->state.minicolumn_states);
        if (bridge->state.belief_distribution) nimcp_free(bridge->state.belief_distribution);
        if (bridge->state.prior_distribution) nimcp_free(bridge->state.prior_distribution);
        if (bridge->state.prediction_errors) nimcp_free(bridge->state.prediction_errors);
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        cortical_column_fep_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_column_fep_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created cortical column FEP bridge");
    return bridge;
}

void cortical_column_fep_destroy(cortical_column_fep_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        cortical_column_fep_disconnect_bio_async(bridge);
    }

    if (bridge->state.minicolumn_states) nimcp_free(bridge->state.minicolumn_states);
    if (bridge->state.belief_distribution) nimcp_free(bridge->state.belief_distribution);
    if (bridge->state.prior_distribution) nimcp_free(bridge->state.prior_distribution);
    if (bridge->state.prediction_errors) nimcp_free(bridge->state.prediction_errors);

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed cortical column FEP bridge");
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int cortical_column_fep_update(cortical_column_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get hypercolumn distribution */
    float* activations = (float*)nimcp_malloc(sizeof(float) * bridge->state.num_minicolumns);
    if (!activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate activations array");
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    hypercolumn_get_distribution(bridge->hypercolumn, activations, bridge->state.num_minicolumns);

    /* Update belief distribution from activations */
    for (uint32_t i = 0; i < bridge->state.num_minicolumns; i++) {
        float activation = activations[i];
        float current_belief = bridge->state.belief_distribution[i];

        /* Weighted update */
        float new_belief = current_belief +
            bridge->config.activation_to_belief_gain * (activation - current_belief);

        bridge->state.belief_distribution[i] = fmaxf(0.0f, new_belief);
    }

    /* Normalize beliefs */
    float belief_sum = 0.0f;
    for (uint32_t i = 0; i < bridge->state.num_minicolumns; i++) {
        belief_sum += bridge->state.belief_distribution[i];
    }
    if (belief_sum > 0.0f) {
        for (uint32_t i = 0; i < bridge->state.num_minicolumns; i++) {
            bridge->state.belief_distribution[i] /= belief_sum;
        }
    }

    nimcp_free(activations);

    bridge->stats.total_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int cortical_column_fep_process_observation(
    cortical_column_fep_bridge_t* bridge,
    const float* observation,
    uint32_t observation_dim)
{
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(observation, NIMCP_ERROR_NULL_POINTER, "observation is NULL");

    /* Update beliefs */
    cortical_column_fep_update_beliefs(bridge, observation, observation_dim);

    /* Apply precision-weighted inhibition */
    cortical_column_fep_apply_lateral_inhibition(bridge);

    /* Run FEP-guided competition */
    cortical_column_fep_select_hypothesis(bridge);

    return 0;
}

uint32_t cortical_column_fep_compute_prediction(
    const cortical_column_fep_bridge_t* bridge,
    float* prediction,
    uint32_t prediction_dim)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return 0;
    }
    if (!prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prediction is NULL");
        return 0;
    }

    /* Compute weighted prediction based on beliefs */
    uint32_t winner = hypercolumn_get_winner(bridge->hypercolumn);

    /* For simplicity, return winner index as prediction */
    if (winner != UINT32_MAX && prediction_dim > 0) {
        prediction[0] = (float)winner;
        return 1;
    }

    return 0;
}

int cortical_column_fep_compute_error(
    const cortical_column_fep_bridge_t* bridge,
    const float* observation,
    uint32_t observation_dim,
    float* error)
{
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(observation, NIMCP_ERROR_NULL_POINTER, "observation is NULL");
    NIMCP_FEP_CHECK_THROW(error, NIMCP_ERROR_NULL_POINTER, "error is NULL");

    /* Compute prediction */
    float prediction[16];
    uint32_t pred_dim = cortical_column_fep_compute_prediction(bridge, prediction, 16);

    /* Compute error magnitude */
    *error = 0.0f;
    uint32_t min_dim = (observation_dim < pred_dim) ? observation_dim : pred_dim;
    for (uint32_t i = 0; i < min_dim; i++) {
        float diff = observation[i] - prediction[i];
        *error += diff * diff;
    }
    *error = sqrtf(*error);

    return 0;
}

/* ============================================================================
 * Belief and Precision API
 * ============================================================================ */

int cortical_column_fep_update_beliefs(
    cortical_column_fep_bridge_t* bridge,
    const float* observation,
    uint32_t observation_dim)
{
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(observation, NIMCP_ERROR_NULL_POINTER, "observation is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Simple belief update: increase belief for active minicolumns */
    for (uint32_t i = 0; i < bridge->state.num_minicolumns && i < observation_dim; i++) {
        float obs = observation[i];
        float current_belief = bridge->state.belief_distribution[i];

        /* Gradient step */
        float update = bridge->config.belief_learning_rate *
                      (obs - current_belief) * bridge->state.sensory_precision;

        bridge->state.belief_distribution[i] = fmaxf(0.0f, current_belief + update);
    }

    /* Normalize */
    float sum = 0.0f;
    for (uint32_t i = 0; i < bridge->state.num_minicolumns; i++) {
        sum += bridge->state.belief_distribution[i];
    }
    if (sum > 0.0f) {
        for (uint32_t i = 0; i < bridge->state.num_minicolumns; i++) {
            bridge->state.belief_distribution[i] /= sum;
        }
    }

    bridge->stats.belief_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int cortical_column_fep_update_precision(cortical_column_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_precision_learning) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Adapt precision based on prediction error variance */
    float error_variance = 0.0f;
    for (uint32_t i = 0; i < bridge->state.num_minicolumns; i++) {
        float err = bridge->state.prediction_errors[i];
        error_variance += err * err;
    }
    if (bridge->state.num_minicolumns == 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    error_variance /= bridge->state.num_minicolumns;

    /* Update precision (inverse variance) */
    if (error_variance > 0.0f) {
        float new_precision = 1.0f / error_variance;
        float current_precision = bridge->state.sensory_precision;

        bridge->state.sensory_precision = current_precision +
            bridge->config.precision_learning_rate * (new_precision - current_precision);

        /* Clamp */
        if (bridge->state.sensory_precision < FEP_MIN_PRECISION) {
            bridge->state.sensory_precision = FEP_MIN_PRECISION;
        }
        if (bridge->state.sensory_precision > FEP_MAX_PRECISION) {
            bridge->state.sensory_precision = FEP_MAX_PRECISION;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int cortical_column_fep_set_precision(
    cortical_column_fep_bridge_t* bridge,
    float precision)
{
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.sensory_precision = fmaxf(FEP_MIN_PRECISION,
                                            fminf(FEP_MAX_PRECISION, precision));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Competition and Selection API
 * ============================================================================ */

uint32_t cortical_column_fep_select_hypothesis(
    cortical_column_fep_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return UINT32_MAX;
    }

    /* Find minicolumn with highest belief */
    uint32_t winner = 0;
    float max_belief = bridge->state.belief_distribution[0];

    for (uint32_t i = 1; i < bridge->state.num_minicolumns; i++) {
        if (bridge->state.belief_distribution[i] > max_belief) {
            max_belief = bridge->state.belief_distribution[i];
            winner = i;
        }
    }

    return winner;
}

int cortical_column_fep_apply_lateral_inhibition(
    cortical_column_fep_bridge_t* bridge)
{
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Precision modulates inhibition strength */
    float inhibition_strength = bridge->state.sensory_precision *
                               bridge->config.precision_to_inhibition_gain;

    /* Apply to hypercolumn competition */
    hypercolumn_run_competition(bridge->hypercolumn, CC_COMPETITION_SOFTMAX,
                                1.0f / inhibition_strength);

    return 0;
}

/* ============================================================================
 * Free Energy and Surprise API
 * ============================================================================ */

int cortical_column_fep_compute_free_energy(
    const cortical_column_fep_bridge_t* bridge,
    float* free_energy)
{
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(free_energy, NIMCP_ERROR_NULL_POINTER, "free_energy is NULL");

    /* Compute entropy of belief distribution */
    float entropy = 0.0f;
    for (uint32_t i = 0; i < bridge->state.num_minicolumns; i++) {
        float p = bridge->state.belief_distribution[i];
        if (p > 0.0f) {
            entropy -= p * logf(p);
        }
    }

    *free_energy = entropy; /* Simplified free energy */
    return 0;
}

float cortical_column_fep_compute_surprise(
    const cortical_column_fep_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return 0.0f;
    }

    float fe;
    cortical_column_fep_compute_free_energy(bridge, &fe);
    return fe; /* Free energy upper bounds surprise */
}

/* ============================================================================
 * Query API
 * ============================================================================ */

uint32_t cortical_column_fep_get_beliefs(
    const cortical_column_fep_bridge_t* bridge,
    float* beliefs,
    uint32_t size)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return 0;
    }
    if (!beliefs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "beliefs is NULL");
        return 0;
    }

    uint32_t count = (size < bridge->state.num_minicolumns) ? size : bridge->state.num_minicolumns;
    memcpy(beliefs, bridge->state.belief_distribution, sizeof(float) * count);
    return count;
}

int cortical_column_fep_get_effects(
    const cortical_column_fep_bridge_t* bridge,
    cortical_column_fep_effects_t* effects)
{
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");
    memcpy(effects, &bridge->fep_effects, sizeof(*effects));
    return 0;
}

int cortical_column_fep_get_column_effects(
    const cortical_column_fep_bridge_t* bridge,
    fep_cortical_column_effects_t* effects)
{
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");
    memcpy(effects, &bridge->column_effects, sizeof(*effects));
    return 0;
}

int cortical_column_fep_get_stats(
    const cortical_column_fep_bridge_t* bridge,
    cortical_column_fep_stats_t* stats)
{
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");
    memcpy(stats, &bridge->stats, sizeof(*stats));
    return 0;
}

/* ============================================================================
 * Bio-async API
 * ============================================================================ */

int cortical_column_fep_connect_bio_async(cortical_column_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Note: BIO_MODULE_FEP_CORTICAL_COLUMN would need to be defined */
    /* This is a stub implementation */
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Bio-async not available for cortical column FEP bridge");

    return 0;
}

int cortical_column_fep_disconnect_bio_async(cortical_column_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool cortical_column_fep_is_bio_async_connected(
    const cortical_column_fep_bridge_t* bridge)
{
    return bridge ? bridge->base.bio_async_enabled : false;
}
