/**
 * @file nimcp_predictive_fep_bridge.h
 * @brief Free Energy Principle - Predictive Regions Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and hierarchical predictive coding
 * WHY:  Predictive coding IS the neural implementation of FEP - this bridge makes
 *       the theoretical connection explicit.
 * HOW:  Direct mapping: predictions = generative model, errors = variational gradients,
 *       precision = attention, minimizing errors = minimizing free energy.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PREDICTIVE CODING AS FEP IMPLEMENTATION:
 * ----------------------------------------
 * - Rao & Ballard (1999): Predictive coding in visual cortex
 * - Friston (2005): "A theory of cortical responses"
 * - Clark (2013): "Whatever next? Predictive brains"
 * - Predictive coding = message passing on FEP free energy functional
 *
 * HIERARCHICAL MESSAGE PASSING:
 * ----------------------------
 * Level N+1: μ_{n+1} (beliefs about causes)
 *      ↓ Top-down predictions
 * Level N: ε_n = x_n - g(μ_{n+1}) (prediction errors)
 *      ↓ Bottom-up error signals
 * Level N-1: ...
 *
 * FEP → PREDICTIVE PATHWAYS:
 * --------------------------
 * 1. Belief Updates = Prediction Updates:
 *    - ∂F/∂μ = 0 gives prediction updates
 *    - Gradient descent on free energy
 *    - Precision-weighted error signals
 *
 * 2. Precision = Gain Control:
 *    - High precision → amplify errors
 *    - Low precision → suppress errors
 *    - Attention as precision optimization
 *
 * 3. Free Energy = Prediction Error:
 *    - F ≈ ∑ Π * ||ε||²
 *    - Minimizing F = minimizing weighted PE
 *    - Direct equivalence in linear Gaussian case
 *
 * PREDICTIVE → FEP PATHWAYS:
 * --------------------------
 * 1. Prediction Errors = Variational Gradients:
 *    - PE signals used to update beliefs
 *    - Hierarchical error propagation
 *    - Evidence accumulation
 *
 * 2. Predictions Provide Generative Model:
 *    - Top-down predictions = g(μ)
 *    - Hierarchical structure = structured prior
 *    - Layer connectivity = conditional densities
 *
 * 3. Precision Weighting = Kalman Gains:
 *    - Optimal prediction = precision-weighted averaging
 *    - Balances prior and likelihood
 *    - Implements Bayesian inference
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PREDICTIVE_FEP_BRIDGE_H
#define NIMCP_PREDICTIVE_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_predictive.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Error-free energy mapping */
#define PRED_FEP_ERROR_FE_SCALING          1.0f    /**< PE to FE conversion */
#define PRED_FEP_PRECISION_DEFAULT         1.0f    /**< Default precision */

/* Hierarchy synchronization */
#define PRED_FEP_MAX_HIERARCHY_SYNC        8       /**< Max levels to sync */
#define PRED_FEP_LEVEL_MATCHING_STRICT     true    /**< Strict level matching */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct predictive_fep_bridge predictive_fep_bridge_t;
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for Predictive-FEP bridge
 */
typedef struct {
    /* FEP → Predictive */
    bool enable_belief_prediction_sync;   /**< Sync FEP beliefs with predictions */
    bool enable_precision_gain_control;   /**< Use FEP precision as gain */
    bool enable_fe_error_mapping;         /**< Map free energy to prediction error */
    float belief_sync_rate;               /**< Rate of belief synchronization */

    /* Predictive → FEP */
    bool enable_error_gradient_flow;      /**< PE as variational gradient */
    bool enable_prediction_generative_model; /**< Predictions as generative model */
    bool enable_precision_kalman_gains;   /**< Precision as Kalman gains */
    float error_gradient_scaling;         /**< PE to gradient scaling */

    /* Hierarchy mapping */
    bool match_hierarchy_levels;          /**< Match FEP and pred levels */
    uint32_t hierarchy_offset;            /**< Level offset (if needed) */

    /* Sensitivity factors */
    float precision_sensitivity;          /**< Precision effect scaling */
    float prediction_sensitivity;         /**< Prediction effect scaling */
} predictive_fep_config_t;

/**
 * @brief FEP effects on predictive system
 */
typedef struct {
    /* Belief synchronization */
    float* synchronized_beliefs;          /**< FEP beliefs → predictions */
    uint32_t num_levels;                  /**< Number of synced levels */

    /* Precision as gain */
    float* precision_gains;               /**< Precision per level */
    float avg_precision;                  /**< Average precision */

    /* Free energy mapping */
    float total_free_energy;              /**< Current FE */
    float* fe_per_level;                  /**< FE decomposition */
} predictive_fep_effects_t;

/**
 * @brief Predictive effects on FEP
 */
typedef struct {
    /* Prediction errors as gradients */
    float* error_gradients;               /**< PE → variational gradients */
    uint32_t gradient_dim;                /**< Gradient dimensionality */

    /* Predictions as generative model */
    float* generative_predictions;        /**< Predictions for FEP */
    uint32_t prediction_dim;              /**< Prediction dimensionality */

    /* Precision weighting */
    float* kalman_gains;                  /**< Precision-based gains */
    uint32_t num_gains;                   /**< Number of gain values */
} fep_predictive_effects_t;

/**
 * @brief Current state of Predictive-FEP interaction
 */
typedef struct {
    /* Current values */
    float current_free_energy;            /**< Current FE */
    float current_prediction_error;       /**< Current PE magnitude */
    uint32_t num_active_levels;           /**< Active hierarchy levels */

    /* Synchronization state */
    bool beliefs_synchronized;            /**< Beliefs in sync */
    bool hierarchy_aligned;               /**< Hierarchies aligned */
    uint64_t last_sync_time;              /**< Last sync timestamp */

    /* Convergence tracking */
    float fe_convergence_rate;            /**< FE reduction rate */
    float pe_convergence_rate;            /**< PE reduction rate */
    bool converged;                       /**< System converged */
} predictive_fep_state_t;

/**
 * @brief Statistics for Predictive-FEP bridge
 */
typedef struct {
    /* FEP → Predictive */
    uint64_t belief_syncs;                /**< Belief synchronizations */
    uint64_t precision_updates;           /**< Precision gain updates */
    uint64_t fe_error_mappings;           /**< FE-PE mappings */
    float avg_free_energy;                /**< Average FE */

    /* Predictive → FEP */
    uint64_t gradient_flows;              /**< Gradient flow events */
    uint64_t generative_updates;          /**< Generative model updates */
    uint64_t kalman_updates;              /**< Kalman gain updates */
    float avg_prediction_error;           /**< Average PE */

    /* Convergence */
    float avg_convergence_steps;          /**< Avg steps to converge */
    uint64_t convergence_count;           /**< Times converged */
} predictive_fep_stats_t;

/**
 * @brief Predictive-FEP bridge state
 */
struct predictive_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    predictive_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;             /**< FEP system */
    predictive_network_t predictive;      /**< Predictive network */

    /* Current effects */
    predictive_fep_effects_t fep_effects; /**< FEP → Predictive */
    fep_predictive_effects_t pred_effects; /**< Predictive → FEP */
    predictive_fep_state_t state;

    /* Statistics */
    predictive_fep_stats_t stats;

    /* Phase 8: Instance health agent (B24 upgrade) */
    nimcp_health_agent_t* health_agent;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default Predictive-FEP configuration
 */
int predictive_fep_bridge_default_config(predictive_fep_config_t* config);

/**
 * @brief Create Predictive-FEP bridge
 */
predictive_fep_bridge_t* predictive_fep_bridge_create(
    const predictive_fep_config_t* config
);

/**
 * @brief Destroy Predictive-FEP bridge
 */
void predictive_fep_bridge_destroy(predictive_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int predictive_fep_bridge_connect_fep(
    predictive_fep_bridge_t* bridge,
    fep_system_t* fep
);

int predictive_fep_bridge_connect_predictive(
    predictive_fep_bridge_t* bridge,
    predictive_network_t predictive
);

int predictive_fep_bridge_disconnect(predictive_fep_bridge_t* bridge);

/* ============================================================================
 * FEP → Predictive Direction
 * ============================================================================ */

/**
 * @brief Synchronize FEP beliefs with predictions
 *
 * WHAT: Copy FEP belief states to predictive network predictions
 * WHY:  FEP beliefs = expected states = predictions
 * HOW:  Direct mapping μ_FEP → prediction_predictive
 */
int predictive_fep_sync_beliefs_to_predictions(predictive_fep_bridge_t* bridge);

/**
 * @brief Apply FEP precision as predictive gain control
 *
 * WHAT: Use FEP precision to modulate prediction error gains
 * WHY:  Precision = attention = gain on errors
 * HOW:  Π_FEP → gain_multiplier_predictive
 */
int predictive_fep_apply_precision_gain_control(predictive_fep_bridge_t* bridge);

/**
 * @brief Map free energy to prediction error
 *
 * WHAT: Convert FEP free energy to predictive error magnitude
 * WHY:  F ≈ ∑Π||ε||² (equivalence in linear Gaussian case)
 * HOW:  F_FEP → ||ε||_predictive (approximate mapping)
 */
int predictive_fep_map_fe_to_error(predictive_fep_bridge_t* bridge);

/* ============================================================================
 * Predictive → FEP Direction
 * ============================================================================ */

/**
 * @brief Flow prediction errors as variational gradients
 *
 * WHAT: Use prediction errors to compute FEP belief gradients
 * WHY:  PE = ∂F/∂μ (prediction errors are variational gradients)
 * HOW:  ε_predictive → ∂F/∂μ_FEP
 */
int predictive_fep_flow_error_gradients(predictive_fep_bridge_t* bridge);

/**
 * @brief Provide predictions as generative model
 *
 * WHAT: Use predictive network predictions in FEP generative model
 * WHY:  Predictions = g(μ) (generative model output)
 * HOW:  prediction_predictive → g(μ)_FEP
 */
int predictive_fep_provide_generative_predictions(predictive_fep_bridge_t* bridge);

/**
 * @brief Compute precision as Kalman gains
 *
 * WHAT: Calculate optimal precision weighting from prediction statistics
 * WHY:  Precision = Kalman gain (optimal Bayesian weighting)
 * HOW:  error_statistics → Π_optimal
 */
int predictive_fep_compute_kalman_gains(predictive_fep_bridge_t* bridge);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

int predictive_fep_bridge_update(
    predictive_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

int predictive_fep_bridge_get_state(
    const predictive_fep_bridge_t* bridge,
    predictive_fep_state_t* state
);

int predictive_fep_bridge_get_stats(
    const predictive_fep_bridge_t* bridge,
    predictive_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int predictive_fep_bridge_connect_bio_async(predictive_fep_bridge_t* bridge);
int predictive_fep_bridge_disconnect_bio_async(predictive_fep_bridge_t* bridge);
bool predictive_fep_bridge_is_bio_async_connected(
    const predictive_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_FEP_BRIDGE_H */
