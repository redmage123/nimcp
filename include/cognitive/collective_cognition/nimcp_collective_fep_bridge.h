/**
 * @file nimcp_collective_fep_bridge.h
 * @brief FEP Orchestrator integration bridge for Collective Cognition module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Provides FEP (Free Energy Principle) orchestrator registration for
 *       the collective cognition module, enabling coordinated free energy
 *       minimization across distributed consciousness systems.
 *
 * WHY: The FEP orchestrator coordinates all bridges in NIMCP with proper
 *      timescales and free energy minimization. Collective cognition needs
 *      to participate in this coordinated update cycle to:
 *      - Minimize prediction error across the swarm/collective
 *      - Optimize collective coherence and synchronization
 *      - Enable emergent consciousness through integrated information
 *
 * HOW: The FEP bridge computes free energy inversely related to:
 *      - Phi (integrated information) - higher phi = lower free energy
 *      - Collective coherence across agents
 *      - Synchronization quality (hyperscanning metrics)
 *      - Shared intentionality and consensus levels
 *
 * THEORETICAL BASIS:
 * - Free Energy Principle (Friston): Minimize variational free energy
 * - Integrated Information Theory (Tononi): Phi as consciousness measure
 * - Collective Cognition (Couzin, Seeley): Swarm intelligence
 * - Active Inference: Action as prediction error minimization
 *
 * FEP-IIT MAPPING:
 * - Higher phi (integrated information) = lower free energy
 * - Collective coherence = precision weighting of predictions
 * - Synchronization = temporal alignment of belief updates
 * - Consensus = converged posterior distributions
 *
 * INTEGRATION ARCHITECTURE:
 *
 *   FEP Orchestrator (50ms cognitive timescale)
 *          |
 *          v
 *   +-----------------------------------+
 *   | Collective Cognition FEP Bridge   |
 *   +-----------------------------------+
 *          |
 *          +---> Phi (Integrated Information)
 *          |     - phi_total as integration measure
 *          |     - Lower free energy when phi is high
 *          |
 *          +---> Coherence (Swarm Alignment)
 *          |     - global_sync from hyperscanning
 *          |     - Higher coherence = better predictions
 *          |
 *          +---> Synchronization (Temporal Binding)
 *          |     - gamma_binding from hyperscanning
 *          |     - Precise temporal coordination
 *          |
 *          +---> Consensus (Shared Beliefs)
 *                - we_mode_strength from shared intentionality
 *                - Converged collective goals
 *
 * @see nimcp_collective_cognition.h
 * @see nimcp_fep_orchestrator.h
 * @see nimcp_collective_phi.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_COLLECTIVE_FEP_BRIDGE_H
#define NIMCP_COLLECTIVE_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Forward declarations */
typedef struct fep_orchestrator fep_orchestrator_t;
typedef struct collective_cognition collective_cognition_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for collective FEP bridge */
#define BIO_MODULE_COLLECTIVE_FEP_BRIDGE    0x1228

/** @brief Default bridge name for registration */
#define COLLECTIVE_FEP_BRIDGE_NAME          "collective_cognition_fep"

/** @brief Maximum history entries for metric tracking */
#define COLLECTIVE_FEP_MAX_HISTORY          64

/* ============================================================================
 * FEP Metrics Structure
 * ============================================================================ */

/**
 * @brief FEP metrics for collective cognition bridge
 *
 * WHAT: Tracks free energy and prediction error metrics for collective cognition
 * WHY:  FEP orchestrator uses these to coordinate updates and track system health
 * HOW:  Updated during each FEP update cycle based on collective state
 */
typedef struct {
    /* Core FEP metrics */
    float free_energy;              /**< Current free energy estimate [0-1] */
    float prediction_error;         /**< Current prediction error [0-1] */
    float surprise;                 /**< Bayesian surprise measure [0-1] */
    float entropy;                  /**< State uncertainty [0-1] */

    /* Collective-specific metrics */
    float phi_contribution;         /**< Phi-based free energy component */
    float coherence_contribution;   /**< Coherence-based component */
    float sync_contribution;        /**< Synchronization-based component */
    float consensus_contribution;   /**< Consensus-based component */

    /* Derived metrics */
    float integration_quality;      /**< Overall integration quality [0-1] */
    float collective_precision;     /**< Precision of collective predictions */

    /* Timing */
    uint64_t last_update_time;      /**< Timestamp of last update (ms) */
    uint32_t update_count;          /**< Total updates performed */
    float avg_update_time_us;       /**< Average update latency */
} collective_fep_metrics_t;

/**
 * @brief Extended statistics for collective FEP bridge
 */
typedef struct {
    /* Current metrics */
    collective_fep_metrics_t current;

    /* Historical statistics */
    float avg_free_energy;          /**< Average free energy over history */
    float min_free_energy;          /**< Minimum free energy achieved */
    float max_free_energy;          /**< Maximum free energy observed */
    float free_energy_variance;     /**< Variance of free energy */

    /* Prediction performance */
    float avg_prediction_error;     /**< Average prediction error */
    float prediction_accuracy;      /**< 1 - avg_prediction_error */
    uint64_t prediction_successes;  /**< Predictions within threshold */
    uint64_t prediction_failures;   /**< Predictions exceeding threshold */

    /* Collective health */
    uint32_t active_instances;      /**< Number of active collective instances */
    float collective_capacity;      /**< Total cognitive capacity */
    uint64_t convergence_events;    /**< Times consensus was reached */
    uint64_t divergence_events;     /**< Times collective fragmented */

    /* Update statistics */
    uint64_t total_updates;         /**< Total update cycles */
    uint64_t update_errors;         /**< Failed updates */
    float total_update_time_us;     /**< Cumulative update time */

    /* Registration info */
    uint32_t fep_bridge_id;         /**< Assigned FEP bridge ID */
    bool is_registered;             /**< Registration status */
    uint64_t registration_time;     /**< When registered (ms) */
} collective_fep_stats_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Configuration for collective FEP bridge
 */
typedef struct {
    /* Free energy computation weights */
    float phi_weight;               /**< Weight for phi contribution [0-1] */
    float coherence_weight;         /**< Weight for coherence [0-1] */
    float sync_weight;              /**< Weight for synchronization [0-1] */
    float consensus_weight;         /**< Weight for consensus [0-1] */

    /* Thresholds */
    float prediction_error_threshold; /**< Threshold for prediction success */
    float surprise_threshold;         /**< Threshold for surprise detection */
    float convergence_threshold;      /**< Phi threshold for convergence */
    float fragmentation_threshold;    /**< Phi threshold for fragmentation */

    /* Scaling parameters */
    float free_energy_scale;        /**< Scale factor for free energy */
    float precision_base;           /**< Base precision for predictions */

    /* Update behavior */
    bool enable_adaptive_weights;   /**< Auto-adjust weights based on history */
    bool enable_prediction_logging; /**< Log prediction performance */
    uint32_t history_window;        /**< Number of samples for statistics */
} collective_fep_config_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct collective_fep_bridge collective_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration for collective FEP bridge
 *
 * WHAT: Provides sensible defaults for FEP bridge configuration
 * WHY:  Easy initialization with biologically-plausible parameters
 * HOW:  Returns balanced weights emphasizing phi and coherence
 *
 * @return Default configuration
 */
collective_fep_config_t collective_fep_config_default(void);

/**
 * @brief Create collective FEP bridge
 *
 * WHAT: Allocates and initializes FEP bridge for collective cognition
 * WHY:  Bridge instance manages state between collective and FEP orchestrator
 * HOW:  Allocates internal structures, initializes metrics, sets config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
collective_fep_bridge_t* collective_fep_bridge_create(
    const collective_fep_config_t* config
);

/**
 * @brief Destroy collective FEP bridge
 *
 * WHAT: Frees bridge resources
 * WHY:  Clean up memory and state
 * HOW:  Unregisters if needed, frees internal structures
 *
 * NOTE: Automatically unregisters from FEP orchestrator if registered
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void collective_fep_bridge_destroy(collective_fep_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Resets all metrics and statistics to initial state
 * WHY:  Allow fresh start without recreating bridge
 * HOW:  Clears metrics, resets history, keeps configuration
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int collective_fep_bridge_reset(collective_fep_bridge_t* bridge);

/* ============================================================================
 * Registration API
 * ============================================================================ */

/**
 * @brief Register collective cognition with FEP orchestrator
 *
 * WHAT: Registers the collective cognition module with FEP orchestrator
 * WHY:  Enables coordinated free energy minimization across the system
 * HOW:  Creates bridge if needed, registers with orchestrator at cognitive timescale
 *
 * @param orchestrator FEP orchestrator instance
 * @param collective Collective cognition module instance
 * @param bridge_id_out Output: assigned FEP bridge ID (optional, can be NULL)
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if orchestrator is NULL
 * - Returns -1 if collective is NULL
 * - Returns -1 if registration fails
 *
 * THREAD-SAFE: Yes
 */
int collective_cognition_fep_bridge_register(
    fep_orchestrator_t* orchestrator,
    collective_cognition_t* collective,
    uint32_t* bridge_id_out
);

/**
 * @brief Unregister collective cognition from FEP orchestrator
 *
 * WHAT: Removes the collective cognition bridge from FEP orchestrator
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Unregisters bridge, cleans up internal state
 *
 * @param orchestrator FEP orchestrator instance
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int collective_cognition_fep_bridge_unregister(
    fep_orchestrator_t* orchestrator
);

/**
 * @brief Check if collective cognition is registered with FEP
 *
 * @return true if registered, false otherwise
 */
bool collective_cognition_fep_is_registered(void);

/**
 * @brief Get the assigned FEP bridge ID
 *
 * @return Bridge ID if registered, 0 if not registered
 */
uint32_t collective_cognition_fep_get_bridge_id(void);

/* ============================================================================
 * FEP Update Callback (Internal - called by FEP orchestrator)
 * ============================================================================ */

/**
 * @brief FEP update callback for collective cognition
 *
 * WHAT: Called by FEP orchestrator during cognitive update cycle (50ms)
 * WHY:  Update free energy based on current collective state
 * HOW:  Gets collective metrics, computes free energy, updates predictions
 *
 * FREE ENERGY COMPUTATION:
 * 1. Get phi (integrated information) from collective
 * 2. Get coherence metrics from hyperscanning
 * 3. Get synchronization metrics
 * 4. Get consensus metrics from shared intentionality
 * 5. Compute weighted free energy inversely related to integration
 * 6. Update prediction error based on collective consensus drift
 *
 * @param handle Opaque handle (collective_fep_bridge_t*)
 * @return 0 on success, -1 on error
 */
int collective_cognition_fep_update_callback(void* handle);

/**
 * @brief FEP destroy callback for collective cognition
 *
 * WHAT: Called by FEP orchestrator when bridge is destroyed
 * WHY:  Clean up bridge-specific resources
 * HOW:  Frees bridge handle internal state
 *
 * @param handle Opaque handle (collective_fep_bridge_t*)
 */
void collective_cognition_fep_destroy_callback(void* handle);

/* ============================================================================
 * Metrics API
 * ============================================================================ */

/**
 * @brief Get current FEP metrics for collective cognition
 *
 * @param metrics_out Output: current metrics
 * @return 0 on success, -1 on error
 */
int collective_cognition_fep_get_metrics(
    collective_fep_metrics_t* metrics_out
);

/**
 * @brief Get extended statistics for collective FEP bridge
 *
 * @param stats_out Output: extended statistics
 * @return 0 on success, -1 on error
 */
int collective_cognition_fep_get_stats(
    collective_fep_stats_t* stats_out
);

/**
 * @brief Reset metrics to initial state
 *
 * @return 0 on success, -1 on error
 */
int collective_cognition_fep_reset_metrics(void);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Update bridge configuration
 *
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int collective_cognition_fep_set_config(
    const collective_fep_config_t* config
);

/**
 * @brief Get current bridge configuration
 *
 * @param config_out Output: current configuration
 * @return 0 on success, -1 on error
 */
int collective_cognition_fep_get_config(
    collective_fep_config_t* config_out
);

/* ============================================================================
 * Advanced API
 * ============================================================================ */

/**
 * @brief Get the internal bridge handle
 *
 * For advanced use cases requiring direct bridge access.
 *
 * @return Bridge handle or NULL if not initialized
 */
collective_fep_bridge_t* collective_cognition_fep_get_bridge(void);

/**
 * @brief Force immediate FEP update
 *
 * Bypasses the orchestrator timing to immediately update FEP metrics.
 * Useful for testing or when synchronization is critical.
 *
 * @return 0 on success, -1 on error
 */
int collective_cognition_fep_force_update(void);

/**
 * @brief Get free energy contribution breakdown
 *
 * Returns the contribution of each component to the total free energy.
 *
 * @param phi_contrib Output: phi contribution
 * @param coherence_contrib Output: coherence contribution
 * @param sync_contrib Output: synchronization contribution
 * @param consensus_contrib Output: consensus contribution
 * @return 0 on success, -1 on error
 */
int collective_cognition_fep_get_contributions(
    float* phi_contrib,
    float* coherence_contrib,
    float* sync_contrib,
    float* consensus_contrib
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COLLECTIVE_FEP_BRIDGE_H */
