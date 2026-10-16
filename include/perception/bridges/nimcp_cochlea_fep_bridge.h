/**
 * @file nimcp_cochlea_fep_bridge.h
 * @brief Cochlea-FEP (Free Energy Principle) integration bridge
 *
 * WHAT: Connect cochlear processing to predictive coding framework
 * WHY:  Enable active inference and auditory prediction error minimization
 * HOW:  Bidirectional prediction/error signals between cochlea and FEP
 *
 * BIOLOGICAL BASIS:
 * - Auditory system as predictive processing hierarchy
 * - Top-down predictions modulate peripheral sensitivity
 * - Bottom-up prediction errors drive learning
 * - Active inference: movement to sample expected sounds
 *
 * FREE ENERGY MINIMIZATION:
 * - Prediction: Expected auditory input based on context
 * - Sensory: Actual cochlear output
 * - Error: Mismatch drives adaptation
 * - Precision: Confidence weighting of errors
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_FEP_BRIDGE_H
#define NIMCP_COCHLEA_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_cochlea.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct fep_orchestrator fep_orchestrator_t;

//=============================================================================
// Constants
//=============================================================================

#define COCHLEA_FEP_DEFAULT_PRECISION       1.0f    /**< Default precision weight */
#define COCHLEA_FEP_ERROR_THRESHOLD         0.1f    /**< Min error for update */
#define COCHLEA_FEP_LEARNING_RATE           0.01f   /**< FEP learning rate */

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Auditory prediction state
 */
typedef struct {
    float* frequency_prediction;    /**< Predicted frequency activation */
    float* temporal_prediction;     /**< Predicted temporal pattern */
    uint32_t num_channels;

    float* precision_weights;       /**< Per-channel precision */
    float global_precision;         /**< Overall confidence */

    uint64_t prediction_timestamp;  /**< When prediction was made */
} auditory_prediction_t;

/**
 * @brief Prediction error signal
 */
typedef struct {
    float* frequency_error;         /**< Per-channel frequency error */
    float* temporal_error;          /**< Temporal prediction error */
    uint32_t num_channels;

    float total_error;              /**< Summed weighted error */
    float free_energy;              /**< Current free energy estimate */

    bool surprise_detected;         /**< High prediction error */
    float surprise_magnitude;       /**< How surprising */
} prediction_error_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Prediction parameters */
    float default_precision;        /**< Default precision weight */
    float precision_decay;          /**< Precision decay rate */

    /* Error parameters */
    float error_threshold;          /**< Minimum error to report */
    float error_gain;               /**< Error amplification */

    /* Learning */
    bool enable_learning;           /**< Update predictions */
    float learning_rate;            /**< Prediction update rate */

    /* Active inference */
    bool enable_active_inference;   /**< Allow motor predictions */
    float action_threshold;         /**< Threshold for action */
} cochlea_fep_config_t;

/**
 * @brief Bridge instance (opaque)
 */
typedef struct cochlea_fep_bridge cochlea_fep_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

cochlea_fep_config_t cochlea_fep_config_default(void);

//=============================================================================
// Core API
//=============================================================================

cochlea_fep_bridge_t* cochlea_fep_bridge_create(
    cochlea_t* cochlea,
    fep_orchestrator_t* fep,
    const cochlea_fep_config_t* config
);

void cochlea_fep_bridge_destroy(cochlea_fep_bridge_t* bridge);

nimcp_error_t cochlea_fep_bridge_update(
    cochlea_fep_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
);

nimcp_error_t cochlea_fep_bridge_reset(cochlea_fep_bridge_t* bridge);

//=============================================================================
// Prediction API
//=============================================================================

nimcp_error_t cochlea_fep_set_prediction(
    cochlea_fep_bridge_t* bridge,
    const float* frequency_pred,
    uint32_t num_channels
);

nimcp_error_t cochlea_fep_get_prediction(
    const cochlea_fep_bridge_t* bridge,
    auditory_prediction_t* prediction
);

nimcp_error_t cochlea_fep_get_error(
    const cochlea_fep_bridge_t* bridge,
    prediction_error_t* error
);

//=============================================================================
// Precision Control
//=============================================================================

nimcp_error_t cochlea_fep_set_precision(
    cochlea_fep_bridge_t* bridge,
    float global_precision
);

nimcp_error_t cochlea_fep_set_channel_precision(
    cochlea_fep_bridge_t* bridge,
    uint32_t channel,
    float precision
);

//=============================================================================
// Free Energy Access
//=============================================================================

float cochlea_fep_get_free_energy(const cochlea_fep_bridge_t* bridge);
bool cochlea_fep_is_surprised(const cochlea_fep_bridge_t* bridge);

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_fep_verify_bidirectional(const cochlea_fep_bridge_t* bridge);
uint64_t cochlea_fep_get_last_outbound(const cochlea_fep_bridge_t* bridge);
uint64_t cochlea_fep_get_last_inbound(const cochlea_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_FEP_BRIDGE_H */
