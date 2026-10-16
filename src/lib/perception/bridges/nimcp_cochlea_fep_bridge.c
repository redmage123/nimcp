/**
 * @file nimcp_cochlea_fep_bridge.c
 * @brief Cochlea-FEP integration implementation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/bridges/nimcp_cochlea_fep_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_fep_bridge)

#define LOG_MODULE "COCHLEA_FEP_BRIDGE"

//=============================================================================
// Internal Structure
//=============================================================================

/** Default number of FEP channels */
#define COCHLEA_FEP_DEFAULT_CHANNELS 64

/** Bridge internal structure */
struct cochlea_fep_bridge {
    bridge_base_t base;                         /**< MUST be first */
    cochlea_t* cochlea;                         /**< Cochlea instance */
    fep_orchestrator_t* fep;                    /**< FEP orchestrator */
    cochlea_fep_config_t config;               /**< Configuration */

    /* Prediction state */
    float* frequency_prediction;                /**< Predicted frequency activations */
    float* temporal_prediction;                 /**< Predicted temporal pattern */
    float* precision_weights;                   /**< Per-channel precision weights */
    float global_precision;                     /**< Overall precision */
    uint32_t num_channels;                      /**< Number of channels */
    uint64_t prediction_timestamp;              /**< When prediction was made */

    /* Error state */
    float* frequency_error;                     /**< Per-channel frequency error */
    float* temporal_error;                      /**< Temporal prediction error */
    float total_error;                          /**< Summed weighted error */
    float free_energy;                          /**< Current free energy estimate */
    bool surprise_detected;                     /**< High prediction error flag */
    float surprise_magnitude;                   /**< How surprising */

    /* Bidirectional timestamps */
    uint64_t last_outbound_ts;
    uint64_t last_inbound_ts;
};

//=============================================================================
// Helper: Current time in ms
//=============================================================================

static uint64_t cochlea_fep_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

//=============================================================================
// Configuration
//=============================================================================

cochlea_fep_config_t cochlea_fep_config_default(void) {
    cochlea_fep_config_t config;
    memset(&config, 0, sizeof(config));
    config.default_precision = COCHLEA_FEP_DEFAULT_PRECISION;
    config.precision_decay = 0.01f;
    config.error_threshold = COCHLEA_FEP_ERROR_THRESHOLD;
    config.error_gain = 1.0f;
    config.enable_learning = true;
    config.learning_rate = COCHLEA_FEP_LEARNING_RATE;
    config.enable_active_inference = false;
    config.action_threshold = 0.5f;
    return config;
}

//=============================================================================
// Helper: Allocate/free arrays
//=============================================================================

static int cochlea_fep_alloc(cochlea_fep_bridge_t* bridge, uint32_t n) {
    bridge->frequency_prediction = (float*)nimcp_calloc(n, sizeof(float));
    bridge->temporal_prediction = (float*)nimcp_calloc(n, sizeof(float));
    bridge->precision_weights = (float*)nimcp_calloc(n, sizeof(float));
    bridge->frequency_error = (float*)nimcp_calloc(n, sizeof(float));
    bridge->temporal_error = (float*)nimcp_calloc(n, sizeof(float));
    bridge->num_channels = n;

    if (!bridge->frequency_prediction || !bridge->temporal_prediction ||
        !bridge->precision_weights || !bridge->frequency_error ||
        !bridge->temporal_error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_fep_alloc: operation failed");
        return -1;
    }

    /* Initialize precision weights to default */
    for (uint32_t i = 0; i < n; i++) {
        bridge->precision_weights[i] = bridge->config.default_precision;
    }

    return 0;
}

static void cochlea_fep_free_arrays(cochlea_fep_bridge_t* bridge) {
    if (bridge->frequency_prediction) { nimcp_free(bridge->frequency_prediction); bridge->frequency_prediction = NULL; }
    if (bridge->temporal_prediction) { nimcp_free(bridge->temporal_prediction); bridge->temporal_prediction = NULL; }
    if (bridge->precision_weights) { nimcp_free(bridge->precision_weights); bridge->precision_weights = NULL; }
    if (bridge->frequency_error) { nimcp_free(bridge->frequency_error); bridge->frequency_error = NULL; }
    if (bridge->temporal_error) { nimcp_free(bridge->temporal_error); bridge->temporal_error = NULL; }
}

//=============================================================================
// Core API
//=============================================================================

cochlea_fep_bridge_t* cochlea_fep_bridge_create(
    cochlea_t* cochlea,
    fep_orchestrator_t* fep,
    const cochlea_fep_config_t* config)
{
    cochlea_fep_bridge_t* bridge = (cochlea_fep_bridge_t*)nimcp_calloc(1, sizeof(cochlea_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_fep_bridge_create: alloc failed");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cochlea_fep_config_default();
    }

    if (bridge_base_init(&bridge->base, 0, "cochlea_fep_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cochlea_fep_bridge_create: validation failed");
        return NULL;
    }

    bridge->cochlea = cochlea;
    bridge->fep = fep;

    if (cochlea) {
        bridge_base_connect_a_unlocked(&bridge->base, cochlea);
    }
    if (fep) {
        bridge_base_connect_b_unlocked(&bridge->base, fep);
    }

    if (cochlea_fep_alloc(bridge, COCHLEA_FEP_DEFAULT_CHANNELS) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_fep_bridge_create: array alloc failed");
        bridge_base_cleanup(&bridge->base);
        cochlea_fep_free_arrays(bridge);
        nimcp_free(bridge);
        return NULL;
    }

    bridge->global_precision = bridge->config.default_precision;
    bridge->free_energy = 0.0f;
    bridge->total_error = 0.0f;
    bridge->surprise_detected = false;
    bridge->surprise_magnitude = 0.0f;

    cochlea_fep_bridge_heartbeat("create", 1.0f);
    return bridge;
}

void cochlea_fep_bridge_destroy(cochlea_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cochlea_fep");
    cochlea_fep_bridge_heartbeat("destroy", 0.0f);
    cochlea_fep_free_arrays(bridge);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

nimcp_error_t cochlea_fep_bridge_update(
    cochlea_fep_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_fep_bridge_update: bridge is NULL");
        return -1;
    }
    if (!cochlea_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_fep_bridge_update: cochlea_output is NULL");
        return -1;
    }

    cochlea_fep_bridge_heartbeat("update", 0.1f);

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t n = bridge->num_channels;
    float total_weighted_error = 0.0f;
    float total_precision = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        /* Compute prediction error (sensory - prediction) */
        float sensory = 0.0f; /* Placeholder: actual cochlea output mapping */
        float error = sensory - bridge->frequency_prediction[i];
        bridge->frequency_error[i] = error * bridge->config.error_gain;

        /* Precision-weighted error */
        float weighted_error = bridge->frequency_error[i] * bridge->precision_weights[i];
        total_weighted_error += fabsf(weighted_error);
        total_precision += bridge->precision_weights[i];

        /* Update prediction if learning is enabled */
        if (bridge->config.enable_learning && fabsf(error) > bridge->config.error_threshold) {
            float new_pred = bridge->frequency_prediction[i] + bridge->config.learning_rate * error;
            if (isfinite(new_pred)) {
                bridge->frequency_prediction[i] = new_pred;
            }
        }

        /* Decay precision */
        if (bridge->config.precision_decay > 0.0f) {
            float new_precision = bridge->precision_weights[i] * (1.0f - bridge->config.precision_decay * dt_ms * 0.001f);
            if (isfinite(new_precision)) {
                bridge->precision_weights[i] = (new_precision < 0.01f) ? 0.01f : new_precision;
            }
        }
    }

    /* Compute free energy: weighted sum of prediction errors */
    if (isfinite(total_weighted_error)) {
        bridge->total_error = total_weighted_error;
    }
    if (total_precision > 0.0f) {
        float fe = total_weighted_error / total_precision;
        if (isfinite(fe)) {
            bridge->free_energy = fe;
        }
    } else {
        if (isfinite(total_weighted_error)) {
            bridge->free_energy = total_weighted_error;
        }
    }

    /* Detect surprise: free energy exceeds threshold */
    float surprise_threshold = 0.5f;
    bridge->surprise_magnitude = bridge->free_energy;
    bridge->surprise_detected = (bridge->free_energy > surprise_threshold);

    bridge->prediction_timestamp = cochlea_fep_time_ms();
    bridge->last_outbound_ts = cochlea_fep_time_ms();

    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_fep_bridge_heartbeat("update", 1.0f);
    return 0;
}

nimcp_error_t cochlea_fep_bridge_reset(cochlea_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_fep_bridge_reset: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t n = bridge->num_channels;
    memset(bridge->frequency_prediction, 0, n * sizeof(float));
    memset(bridge->temporal_prediction, 0, n * sizeof(float));
    memset(bridge->frequency_error, 0, n * sizeof(float));
    memset(bridge->temporal_error, 0, n * sizeof(float));

    for (uint32_t i = 0; i < n; i++) {
        bridge->precision_weights[i] = bridge->config.default_precision;
    }

    bridge->global_precision = bridge->config.default_precision;
    bridge->total_error = 0.0f;
    bridge->free_energy = 0.0f;
    bridge->surprise_detected = false;
    bridge->surprise_magnitude = 0.0f;
    bridge->prediction_timestamp = 0;
    bridge->last_outbound_ts = 0;
    bridge->last_inbound_ts = 0;

    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_fep_bridge_heartbeat("reset", 1.0f);
    return 0;
}

//=============================================================================
// Prediction API
//=============================================================================

nimcp_error_t cochlea_fep_set_prediction(
    cochlea_fep_bridge_t* bridge,
    const float* frequency_pred,
    uint32_t num_channels)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_fep_set_prediction: bridge is NULL");
        return -1;
    }
    if (!frequency_pred) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_fep_set_prediction: frequency_pred is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t copy_n = (num_channels < bridge->num_channels) ? num_channels : bridge->num_channels;
    memcpy(bridge->frequency_prediction, frequency_pred, copy_n * sizeof(float));
    bridge->prediction_timestamp = cochlea_fep_time_ms();
    bridge->last_inbound_ts = cochlea_fep_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_fep_bridge_heartbeat("set_prediction", 1.0f);
    return 0;
}

nimcp_error_t cochlea_fep_get_prediction(
    const cochlea_fep_bridge_t* bridge,
    auditory_prediction_t* prediction)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_fep_get_prediction: bridge is NULL");
        return -1;
    }
    if (!prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_fep_get_prediction: prediction is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    prediction->frequency_prediction = bridge->frequency_prediction;
    prediction->temporal_prediction = bridge->temporal_prediction;
    prediction->num_channels = bridge->num_channels;
    prediction->precision_weights = bridge->precision_weights;
    prediction->global_precision = bridge->global_precision;
    prediction->prediction_timestamp = bridge->prediction_timestamp;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

nimcp_error_t cochlea_fep_get_error(
    const cochlea_fep_bridge_t* bridge,
    prediction_error_t* error)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_fep_get_error: bridge is NULL");
        return -1;
    }
    if (!error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_fep_get_error: error is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    error->frequency_error = bridge->frequency_error;
    error->temporal_error = bridge->temporal_error;
    error->num_channels = bridge->num_channels;
    error->total_error = bridge->total_error;
    error->free_energy = bridge->free_energy;
    error->surprise_detected = bridge->surprise_detected;
    error->surprise_magnitude = bridge->surprise_magnitude;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Precision Control
//=============================================================================

nimcp_error_t cochlea_fep_set_precision(
    cochlea_fep_bridge_t* bridge,
    float global_precision)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_fep_set_precision: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->global_precision = global_precision;
    /* Also update all per-channel weights to match */
    for (uint32_t i = 0; i < bridge->num_channels; i++) {
        bridge->precision_weights[i] = global_precision;
    }
    bridge->last_inbound_ts = cochlea_fep_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_fep_bridge_heartbeat("set_precision", 1.0f);
    return 0;
}

nimcp_error_t cochlea_fep_set_channel_precision(
    cochlea_fep_bridge_t* bridge,
    uint32_t channel,
    float precision)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_fep_set_channel_precision: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (channel >= bridge->num_channels) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    bridge->precision_weights[channel] = precision;
    bridge->last_inbound_ts = cochlea_fep_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Free Energy Access
//=============================================================================

float cochlea_fep_get_free_energy(const cochlea_fep_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    nimcp_mutex_lock(bridge->base.mutex);
    float fe = bridge->free_energy;
    nimcp_mutex_unlock(bridge->base.mutex);
    return fe;
}

bool cochlea_fep_is_surprised(const cochlea_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->surprise_detected;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_fep_verify_bidirectional(const cochlea_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_fep_verify_bidirectional: bridge is NULL");
        return false;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bool result = (bridge->last_outbound_ts > 0 && bridge->last_inbound_ts > 0);
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

uint64_t cochlea_fep_get_last_outbound(const cochlea_fep_bridge_t* bridge) {
    if (!bridge) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_outbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}

uint64_t cochlea_fep_get_last_inbound(const cochlea_fep_bridge_t* bridge) {
    if (!bridge) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_inbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}
