/**
 * @file nimcp_omni_cortical_columns_bridge.c
 * @brief Implementation of Omnidirectional Inference to Cortical Columns Bridge
 */

#include "core/cortical_columns/nimcp_omni_cortical_columns_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "cognitive/jepa/nimcp_jepa_bidirectional.h"
#include "cognitive/predictive/nimcp_predictive_hierarchy.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(omni_cortical_columns_bridge)

#define LOG_MODULE "OMNI_CORTICAL_COLUMNS_BRIDGE"


/* ============================================================================
 * Static Helpers
 * ============================================================================ */

static float compute_entropy(const float* distribution, uint32_t size) {
    if (!distribution || size == 0) return 0.0f;

    float entropy = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        if (distribution[i] > 1e-10f) {
            entropy -= distribution[i] * logf(distribution[i]);
        }
    }
    return entropy;
}

static float compute_sparsity(const float* activations, uint32_t size,
                               float threshold) {
    if (!activations || size == 0) return 0.0f;

    uint32_t active_count = 0;
    for (uint32_t i = 0; i < size; i++) {
        if (activations[i] > threshold) {
            active_count++;
        }
    }
    return 1.0f - ((float)active_count / (float)size);
}

static void apply_bias_additive(float* activations, const float* bias,
                                 uint32_t size, float strength) {
    for (uint32_t i = 0; i < size; i++) {
        activations[i] += strength * bias[i];
    }
}

static void apply_bias_multiplicative(float* activations, const float* bias,
                                       uint32_t size, float strength) {
    for (uint32_t i = 0; i < size; i++) {
        float factor = 1.0f + strength * (bias[i] - 0.5f) * 2.0f;
        activations[i] *= fmaxf(0.0f, factor);
    }
}

static void apply_bias_gating(float* activations, const float* bias,
                               uint32_t size, float strength) {
    for (uint32_t i = 0; i < size; i++) {
        float gate = (1.0f - strength) + strength * bias[i];
        activations[i] *= gate;
    }
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int omni_cc_default_config(omni_cc_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    memset(config, 0, sizeof(omni_cc_config_t));

    config->default_bias_strength = OMNI_CC_DEFAULT_BIAS_STRENGTH;
    config->bias_mode = OMNI_CC_BIAS_ADDITIVE;
    config->enable_adaptive_bias = true;

    config->competition_mode = OMNI_CC_COMP_WTA;
    config->competition_temperature = OMNI_CC_DEFAULT_TEMPERATURE;
    config->k_winners = 1;

    config->enable_layer_modulation = true;
    config->layer_2_3_weight = 1.0f;
    config->layer_4_weight = 1.0f;
    config->layer_5_weight = 1.0f;
    config->layer_6_weight = 1.0f;

    config->target_sparsity = 0.95f;
    config->enable_homeostatic = true;

    config->enable_bio_async = true;
    config->enable_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

omni_cortical_columns_bridge_t* omni_cc_bridge_create(
    const omni_cc_config_t* config) {

    omni_cortical_columns_bridge_t* bridge =
        nimcp_calloc(1, sizeof(omni_cortical_columns_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate omni cortical columns bridge");

        return NULL;

    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(omni_cc_config_t));
    } else {
        omni_cc_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "omni_cortical_columns") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_cc_default_config: bridge->base is NULL");
        return NULL;
    }

    /* Initialize layer modulation */
    bridge->omni_effects.layer_modulation = nimcp_calloc(6, sizeof(float));
    if (!bridge->omni_effects.layer_modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_cc_bridge_create: layer_modulation alloc failed");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }
    bridge->omni_effects.layer_modulation[2] = bridge->config.layer_2_3_weight;
    bridge->omni_effects.layer_modulation[3] = bridge->config.layer_2_3_weight;
    bridge->omni_effects.layer_modulation[4] = bridge->config.layer_4_weight;
    bridge->omni_effects.layer_modulation[5] = bridge->config.layer_5_weight;

    memset(&bridge->stats, 0, sizeof(omni_cc_stats_t));

    return bridge;
}

void omni_cc_bridge_destroy(omni_cortical_columns_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "omni_cortical_columns");

    if (bridge->omni_effects.prediction_bias) {
        nimcp_free(bridge->omni_effects.prediction_bias);
    }
    if (bridge->omni_effects.layer_modulation) {
        nimcp_free(bridge->omni_effects.layer_modulation);
    }
    if (bridge->column_effects.winner_activations) {
        nimcp_free(bridge->column_effects.winner_activations);
    }
    if (bridge->column_effects.winner_indices) {
        nimcp_free(bridge->column_effects.winner_indices);
    }
    if (bridge->column_effects.activation_distribution) {
        nimcp_free(bridge->column_effects.activation_distribution);
    }
    if (bridge->column_effects.prediction_errors) {
        nimcp_free(bridge->column_effects.prediction_errors);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int omni_cc_connect_jepa(omni_cortical_columns_bridge_t* bridge,
                          jepa_bidirectional_t* jepa) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->jepa = jepa;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_cc_connect_pred_hier(omni_cortical_columns_bridge_t* bridge,
                               predictive_hierarchy_t* pred_hier) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->pred_hier = pred_hier;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_cc_connect_column_pool(omni_cortical_columns_bridge_t* bridge,
                                 cortical_column_pool_t* pool) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->column_pool = pool;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int omni_cc_update(omni_cortical_columns_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update omni effects */
    bridge->omni_effects.bias_strength = bridge->config.default_bias_strength;
    bridge->omni_effects.bias_mode = bridge->config.bias_mode;
    bridge->omni_effects.competition_temp = bridge->config.competition_temperature;
    bridge->omni_effects.comp_mode = bridge->config.competition_mode;

    /* Compute sparsity and entropy if we have activation distribution */
    if (bridge->column_effects.activation_distribution &&
        bridge->column_effects.distribution_size > 0) {

        bridge->column_effects.competition_entropy = compute_entropy(
            bridge->column_effects.activation_distribution,
            bridge->column_effects.distribution_size);

        bridge->column_effects.sparsity = compute_sparsity(
            bridge->column_effects.activation_distribution,
            bridge->column_effects.distribution_size,
            0.1f);
    }

    /* Adaptive bias based on PE */
    if (bridge->config.enable_adaptive_bias && bridge->pred_hier) {
        float fe = pred_hier_compute_free_energy(bridge->pred_hier);
        if (!isnan(fe)) {
            /* Higher FE -> stronger bias to guide competition */
            bridge->omni_effects.bias_strength =
                bridge->config.default_bias_strength * (1.0f + 0.5f * fe);
        }
    }

    /* Update statistics */
    bridge->stats.total_updates++;
    float n = (float)bridge->stats.total_updates;
    if (n > 0.0f) {
        float new_avg_sparsity =
            (bridge->stats.avg_sparsity * (n - 1) + bridge->column_effects.sparsity) / n;
        if (isfinite(new_avg_sparsity)) bridge->stats.avg_sparsity = new_avg_sparsity;

        float new_avg_entropy =
            (bridge->stats.avg_competition_entropy * (n - 1) +
             bridge->column_effects.competition_entropy) / n;
        if (isfinite(new_avg_entropy)) bridge->stats.avg_competition_entropy = new_avg_entropy;

        float new_avg_bias =
            (bridge->stats.avg_bias_strength * (n - 1) +
             bridge->omni_effects.bias_strength) / n;
        if (isfinite(new_avg_bias)) bridge->stats.avg_bias_strength = new_avg_bias;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_cc_apply_to_columns(omni_cortical_columns_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    return NIMCP_SUCCESS;
}

int omni_cc_apply_to_omni(omni_cortical_columns_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bias API
 * ============================================================================ */

int omni_cc_set_hypercolumn_bias(omni_cortical_columns_bridge_t* bridge,
                                  uint32_t hypercolumn_id,
                                  const float* bias,
                                  uint32_t num_minicolumns) {
    NIMCP_CHECK_THROW(bridge && bias && num_minicolumns > 0, NIMCP_ERROR_INVALID_PARAM, "bridge or bias is NULL, or num_minicolumns is 0");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Allocate or reallocate bias buffer */
    if (!bridge->omni_effects.prediction_bias ||
        bridge->omni_effects.num_minicolumns != num_minicolumns) {

        if (bridge->omni_effects.prediction_bias) {
            nimcp_free(bridge->omni_effects.prediction_bias);
        }
        bridge->omni_effects.prediction_bias =
            nimcp_calloc(num_minicolumns, sizeof(float));
        bridge->omni_effects.num_minicolumns = num_minicolumns;
    }

    if (bridge->omni_effects.prediction_bias) {
        memcpy(bridge->omni_effects.prediction_bias, bias,
               num_minicolumns * sizeof(float));
    }

    bridge->stats.bias_applications++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_cc_get_hypercolumn_bias(const omni_cortical_columns_bridge_t* bridge,
                                  uint32_t hypercolumn_id,
                                  float* bias,
                                  uint32_t* num_minicolumns) {
    NIMCP_CHECK_THROW(bridge && bias && num_minicolumns, NIMCP_ERROR_INVALID_PARAM, "bridge, bias, or num_minicolumns is NULL");

    nimcp_mutex_lock(((omni_cortical_columns_bridge_t*)bridge)->base.mutex);

    if (bridge->omni_effects.prediction_bias) {
        *num_minicolumns = bridge->omni_effects.num_minicolumns;
        memcpy(bias, bridge->omni_effects.prediction_bias,
               bridge->omni_effects.num_minicolumns * sizeof(float));
    } else {
        *num_minicolumns = 0;
    }

    nimcp_mutex_unlock(((omni_cortical_columns_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_cc_apply_prediction_bias(omni_cortical_columns_bridge_t* bridge,
                                   uint32_t hypercolumn_id) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->omni_effects.prediction_bias ||
        !bridge->column_effects.activation_distribution) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_SUCCESS;
    }

    uint32_t size = bridge->omni_effects.num_minicolumns;
    float strength = bridge->omni_effects.bias_strength;

    switch (bridge->omni_effects.bias_mode) {
        case OMNI_CC_BIAS_ADDITIVE:
            apply_bias_additive(bridge->column_effects.activation_distribution,
                                bridge->omni_effects.prediction_bias,
                                size, strength);
            break;
        case OMNI_CC_BIAS_MULTIPLICATIVE:
            apply_bias_multiplicative(bridge->column_effects.activation_distribution,
                                      bridge->omni_effects.prediction_bias,
                                      size, strength);
            break;
        case OMNI_CC_BIAS_GATING:
            apply_bias_gating(bridge->column_effects.activation_distribution,
                              bridge->omni_effects.prediction_bias,
                              size, strength);
            break;
        default:
            break;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Competition API
 * ============================================================================ */

int omni_cc_run_competition(omni_cortical_columns_bridge_t* bridge,
                             uint32_t hypercolumn_id) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.competition_events++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int omni_cc_get_winners(const omni_cortical_columns_bridge_t* bridge,
                         uint32_t hypercolumn_id,
                         uint32_t* winner_indices,
                         float* winner_activations,
                         uint32_t* num_winners) {
    NIMCP_CHECK_THROW(bridge && winner_indices && winner_activations && num_winners, NIMCP_ERROR_INVALID_PARAM, "bridge, winner_indices, winner_activations, or num_winners is NULL");

    nimcp_mutex_lock(((omni_cortical_columns_bridge_t*)bridge)->base.mutex);

    if (bridge->column_effects.winner_indices &&
        bridge->column_effects.winner_activations) {
        *num_winners = bridge->column_effects.num_winners;
        for (uint32_t i = 0; i < *num_winners; i++) {
            winner_indices[i] = bridge->column_effects.winner_indices[i];
            winner_activations[i] = bridge->column_effects.winner_activations[i];
        }
    } else {
        *num_winners = 0;
    }

    nimcp_mutex_unlock(((omni_cortical_columns_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_cc_set_competition_mode(omni_cortical_columns_bridge_t* bridge,
                                  uint32_t hypercolumn_id,
                                  omni_cc_competition_mode_t mode) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->omni_effects.comp_mode = mode;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Layer API
 * ============================================================================ */

int omni_cc_set_layer_modulation(omni_cortical_columns_bridge_t* bridge,
                                  omni_cc_layer_mode_t layer,
                                  float modulation) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->omni_effects.layer_modulation) {
        switch (layer) {
            case OMNI_CC_LAYER_2_3:
                bridge->omni_effects.layer_modulation[2] = modulation;
                bridge->omni_effects.layer_modulation[3] = modulation;
                break;
            case OMNI_CC_LAYER_4:
                bridge->omni_effects.layer_modulation[4] = modulation;
                break;
            case OMNI_CC_LAYER_5:
                bridge->omni_effects.layer_modulation[5] = modulation;
                break;
            case OMNI_CC_LAYER_6:
                /* Would be index 6 but our array is size 6 (0-5) */
                break;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_cc_get_layer_pe(const omni_cortical_columns_bridge_t* bridge,
                          omni_cc_layer_mode_t layer,
                          float* pe) {
    NIMCP_CHECK_THROW(bridge && pe, NIMCP_ERROR_INVALID_PARAM, "bridge or pe is NULL");

    nimcp_mutex_lock(((omni_cortical_columns_bridge_t*)bridge)->base.mutex);

    /* Placeholder - would need to compute layer-specific PE */
    *pe = 0.0f;

    nimcp_mutex_unlock(((omni_cortical_columns_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int omni_cc_get_omni_effects(const omni_cortical_columns_bridge_t* bridge,
                              omni_to_columns_effects_t* effects) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_INVALID_PARAM, "bridge or effects is NULL");
    nimcp_mutex_lock(((omni_cortical_columns_bridge_t*)bridge)->base.mutex);
    memcpy(effects, &bridge->omni_effects, sizeof(omni_to_columns_effects_t));
    nimcp_mutex_unlock(((omni_cortical_columns_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_cc_get_column_effects(const omni_cortical_columns_bridge_t* bridge,
                                columns_to_omni_effects_t* effects) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_INVALID_PARAM, "bridge or effects is NULL");
    nimcp_mutex_lock(((omni_cortical_columns_bridge_t*)bridge)->base.mutex);
    memcpy(effects, &bridge->column_effects, sizeof(columns_to_omni_effects_t));
    nimcp_mutex_unlock(((omni_cortical_columns_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_cc_get_stats(const omni_cortical_columns_bridge_t* bridge,
                       omni_cc_stats_t* stats) {
    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_INVALID_PARAM, "bridge or stats is NULL");
    nimcp_mutex_lock(((omni_cortical_columns_bridge_t*)bridge)->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(omni_cc_stats_t));
    nimcp_mutex_unlock(((omni_cortical_columns_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_cc_reset_stats(omni_cortical_columns_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(omni_cc_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

float omni_cc_get_sparsity(const omni_cortical_columns_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->column_effects.sparsity;
}

float omni_cc_get_entropy(const omni_cortical_columns_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->column_effects.competition_entropy;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

static nimcp_error_t handle_cc_predict_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_cortical_columns_bridge_t* bridge = (omni_cortical_columns_bridge_t*)user_data;
    NIMCP_CHECK_THROW(bridge && msg, NIMCP_ERROR_INVALID_PARAM, "bridge or msg is NULL");

    omni_cc_update(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_cc_precision_update(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_cortical_columns_bridge_t* bridge = (omni_cortical_columns_bridge_t*)user_data;
    NIMCP_CHECK_THROW(bridge && msg, NIMCP_ERROR_INVALID_PARAM, "bridge or msg is NULL");

    /* Apply precision update to competition temperature */
    omni_cc_update(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int omni_cc_connect_bio_async(omni_cortical_columns_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    if (bridge->bio_async_connected) return NIMCP_SUCCESS;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_OMNI_CORTICAL_COLUMNS_BRIDGE,
        .module_name = "omni_cortical_columns_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bio_module_context_t ctx = bio_router_register_module(&info);
    if (!ctx) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    bridge->bio_context = ctx;

    bio_router_register_handler(ctx, BIO_MSG_OMNI_PREDICT_REQUEST,
                                 handle_cc_predict_request);
    bio_router_register_handler(ctx, BIO_MSG_OMNI_PRECISION_UPDATE,
                                 handle_cc_precision_update);

    bridge->bio_async_connected = true;
    return NIMCP_SUCCESS;
}

int omni_cc_disconnect_bio_async(omni_cortical_columns_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    if (!bridge->bio_async_connected) return NIMCP_SUCCESS;

    if (bridge->bio_context) {
        bio_router_unregister_module(bridge->bio_context);
        bridge->bio_context = NULL;
    }

    bridge->bio_async_connected = false;
    return NIMCP_SUCCESS;
}

bool omni_cc_is_bio_async_connected(const omni_cortical_columns_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_cc_competition_to_string(omni_cc_competition_mode_t mode) {
    switch (mode) {
        case OMNI_CC_COMP_WTA: return "WTA";
        case OMNI_CC_COMP_K_WINNERS: return "K_WINNERS";
        case OMNI_CC_COMP_SOFTMAX: return "SOFTMAX";
        case OMNI_CC_COMP_HEBBIAN: return "HEBBIAN";
        default: return "UNKNOWN";
    }
}

const char* omni_cc_bias_to_string(omni_cc_bias_mode_t mode) {
    switch (mode) {
        case OMNI_CC_BIAS_NONE: return "NONE";
        case OMNI_CC_BIAS_ADDITIVE: return "ADDITIVE";
        case OMNI_CC_BIAS_MULTIPLICATIVE: return "MULTIPLICATIVE";
        case OMNI_CC_BIAS_GATING: return "GATING";
        default: return "UNKNOWN";
    }
}

const char* omni_cc_layer_to_string(omni_cc_layer_mode_t layer) {
    switch (layer) {
        case OMNI_CC_LAYER_2_3: return "LAYER_2_3";
        case OMNI_CC_LAYER_4: return "LAYER_4";
        case OMNI_CC_LAYER_5: return "LAYER_5";
        case OMNI_CC_LAYER_6: return "LAYER_6";
        default: return "UNKNOWN";
    }
}
