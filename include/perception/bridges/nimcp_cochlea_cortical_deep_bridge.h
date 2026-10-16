/**
 * @file nimcp_cochlea_cortical_deep_bridge.h
 * @brief Deep bidirectional Cochlea-Cortical Columns integration
 *
 * WHAT: Full tonotopic cortical column organization with bidirectional plasticity
 * WHY:  Enable learning, adaptation, and top-down modulation
 * HOW:  Hypercolumns per critical band, minicolumns per frequency, STDP
 *
 * BIDIRECTIONAL DATA FLOWS:
 * - OUTBOUND: Cochlea -> Columns: Frequency activations, onset events
 * - INBOUND:  Columns -> Cochlea: Attention modulation, learned expectations
 *
 * CORTICAL PLASTICITY:
 * - STDP: Tonotopic map refinement
 * - Lateral inhibition: Mexican hat sharpening
 * - Top-down: Attention-based gain modulation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_CORTICAL_DEEP_BRIDGE_H
#define NIMCP_COCHLEA_CORTICAL_DEEP_BRIDGE_H

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

typedef struct cortical_column_pool cortical_column_pool_t;
typedef struct hypercolumn hypercolumn_t;

//=============================================================================
// Constants
//=============================================================================

#define COCHLEA_CORTICAL_MAX_HYPERCOLUMNS   128
#define COCHLEA_CORTICAL_MINICOLUMNS_PER    100

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief STDP configuration for tonotopic plasticity
 */
typedef struct {
    float tau_plus_ms;                /**< LTP time constant */
    float tau_minus_ms;               /**< LTD time constant */
    float a_plus;                     /**< LTP amplitude */
    float a_minus;                    /**< LTD amplitude */
    float w_max;                      /**< Maximum weight */
    float w_min;                      /**< Minimum weight */
} cochlea_stdp_config_t;

/**
 * @brief Top-down modulation state
 */
typedef struct {
    float* attention_gain;            /**< Per-column gain */
    float* expected_activation;       /**< Predictive coding */
    uint32_t num_columns;             /**< Number of columns */
} cochlea_topdown_state_t;

/**
 * @brief Bottom-up output state
 */
typedef struct {
    float* column_activations;        /**< Column activation levels */
    float* prediction_errors;         /**< Prediction errors */
    uint32_t num_columns;             /**< Number of columns */
} cochlea_bottomup_state_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Hypercolumn organization */
    uint32_t num_hypercolumns;        /**< Number of hypercolumns */
    uint32_t minicolumns_per_hypercolumn; /**< Minicolumns per hypercolumn */

    /* Lateral connectivity (Mexican hat) */
    float excitatory_radius_octaves;  /**< Excitatory radius */
    float inhibitory_radius_octaves;  /**< Inhibitory radius */
    float excitatory_strength;        /**< Excitatory weight */
    float inhibitory_strength;        /**< Inhibitory weight */

    /* Plasticity */
    bool enable_plasticity;           /**< Enable STDP */
    cochlea_stdp_config_t stdp_config;/**< STDP parameters */

    /* Top-down modulation */
    bool enable_top_down;             /**< Enable top-down modulation */
    float top_down_gain_range;        /**< Range of gain modulation */
} cochlea_cortical_deep_config_t;

/**
 * @brief Bridge instance (opaque)
 */
typedef struct cochlea_cortical_deep_bridge cochlea_cortical_deep_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

cochlea_cortical_deep_config_t cochlea_cortical_deep_config_default(void);

//=============================================================================
// Core API
//=============================================================================

cochlea_cortical_deep_bridge_t* cochlea_cortical_deep_bridge_create(
    cochlea_t* cochlea,
    cortical_column_pool_t* pool,
    const cochlea_cortical_deep_config_t* config
);

void cochlea_cortical_deep_bridge_destroy(cochlea_cortical_deep_bridge_t* bridge);

nimcp_error_t cochlea_cortical_deep_bridge_update(
    cochlea_cortical_deep_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
);

nimcp_error_t cochlea_cortical_deep_bridge_reset(cochlea_cortical_deep_bridge_t* bridge);

//=============================================================================
// Bottom-Up Processing (Outbound)
//=============================================================================

/**
 * @brief Process cochlea -> columns (bottom-up)
 */
nimcp_error_t cochlea_cortical_process_bottom_up(
    cochlea_cortical_deep_bridge_t* bridge,
    const cochlea_output_t* cochlea_output
);

/**
 * @brief Get bottom-up state
 */
nimcp_error_t cochlea_cortical_get_bottom_up(
    const cochlea_cortical_deep_bridge_t* bridge,
    cochlea_bottomup_state_t* state
);

//=============================================================================
// Top-Down Modulation (Inbound)
//=============================================================================

/**
 * @brief Apply columns -> cochlea modulation (top-down)
 */
nimcp_error_t cochlea_cortical_apply_top_down(
    cochlea_cortical_deep_bridge_t* bridge,
    const float* attention_pattern,
    uint32_t pattern_size
);

/**
 * @brief Get top-down state
 */
nimcp_error_t cochlea_cortical_get_top_down(
    const cochlea_cortical_deep_bridge_t* bridge,
    cochlea_topdown_state_t* state
);

//=============================================================================
// Prediction Error
//=============================================================================

/**
 * @brief Compute prediction error
 */
nimcp_error_t cochlea_cortical_compute_prediction_error(
    cochlea_cortical_deep_bridge_t* bridge,
    float* prediction_error,
    uint32_t* error_size
);

//=============================================================================
// Plasticity
//=============================================================================

/**
 * @brief Apply STDP learning
 */
nimcp_error_t cochlea_cortical_apply_stdp(
    cochlea_cortical_deep_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Get current synaptic weights
 */
nimcp_error_t cochlea_cortical_get_weights(
    const cochlea_cortical_deep_bridge_t* bridge,
    float** weights,
    uint32_t* num_weights
);

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_cortical_deep_verify_bidirectional(const cochlea_cortical_deep_bridge_t* bridge);
uint64_t cochlea_cortical_deep_get_last_outbound(const cochlea_cortical_deep_bridge_t* bridge);
uint64_t cochlea_cortical_deep_get_last_inbound(const cochlea_cortical_deep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_CORTICAL_DEEP_BRIDGE_H */
