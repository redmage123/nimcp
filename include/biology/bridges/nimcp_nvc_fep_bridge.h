/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_nvc_fep_bridge.h - Neurovascular Coupling to Free Energy Principle Bridge
//=============================================================================
/**
 * @file nimcp_nvc_fep_bridge.h
 * @brief Bridge between Neurovascular Coupling and Free Energy Principle
 *
 * WHAT: Connects hemodynamic responses with predictive processing and
 *       free energy minimization, enabling metabolic costs to enter
 *       the precision-weighted prediction error framework.
 *
 * WHY:  The brain's predictive processing must account for metabolic reality:
 *       - Prediction error computation requires energy
 *       - Precision weighting should reflect metabolic capacity
 *       - Active inference actions have metabolic costs
 *       - BOLD signal reflects prediction error accumulation
 *
 * HOW:  Integration pathways:
 *       1. Metabolic state → Precision weighting
 *       2. Blood flow → Action cost estimation
 *       3. BOLD signal → Prediction error proxy
 *       4. Free energy → Hemodynamic demand signal
 *
 * BIOLOGICAL BASIS:
 * ```
 * NEUROVASCULAR                           FREE ENERGY PRINCIPLE
 * ─────────────────────────────────────────────────────────────────
 * CBF/metabolic capacity            → Precision (inverse variance)
 * Oxygen availability               → Prediction error computation
 * Energy expenditure                → Action cost in active inference
 * BOLD signal                       ← Precision-weighted PE integral
 * Hemodynamic response              ← Free energy gradient
 * Metabolic efficiency              → Model complexity penalty
 * ```
 *
 * THEORETICAL CONNECTIONS:
 * - Precision reflects confidence, requires metabolic resources
 * - High precision = high metabolic cost
 * - Free energy minimization optimizes metabolic efficiency
 * - BOLD undershoot may reflect prediction error resolution
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NVC_FEP_BRIDGE_H
#define NIMCP_NVC_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define NVC_FEP_MODULE_NAME             "nvc_fep_bridge"

/** Maximum FEP regions tracked */
#define NVC_FEP_MAX_REGIONS             64

/** Maximum hierarchical levels */
#define NVC_FEP_MAX_LEVELS              8

/** Default precision scaling from metabolism */
#define NVC_FEP_PRECISION_SCALE         1.0f

/** Metabolic cost per unit precision */
#define NVC_FEP_PRECISION_COST          0.01f

/** BOLD integration time constant (ms) */
#define NVC_FEP_BOLD_TAU                2000.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Precision modulation mode
 */
typedef enum {
    NVC_FEP_PRECISION_LINEAR = 0,        /**< Linear metabolic scaling */
    NVC_FEP_PRECISION_SIGMOID,           /**< Sigmoid with threshold */
    NVC_FEP_PRECISION_ADAPTIVE           /**< History-dependent adaptation */
} nvc_fep_precision_mode_t;

/**
 * @brief Free energy computation level
 */
typedef enum {
    NVC_FEP_LEVEL_SENSORY = 0,           /**< Sensory prediction errors */
    NVC_FEP_LEVEL_PERCEPTUAL,            /**< Perceptual inference */
    NVC_FEP_LEVEL_COGNITIVE,             /**< Higher cognitive */
    NVC_FEP_LEVEL_EXECUTIVE              /**< Executive/goal level */
} nvc_fep_level_t;

/**
 * @brief Metabolic optimization target
 */
typedef enum {
    NVC_FEP_OPT_ACCURACY = 0,            /**< Minimize prediction error */
    NVC_FEP_OPT_EFFICIENCY,              /**< Minimize metabolic cost */
    NVC_FEP_OPT_BALANCED                 /**< Balance accuracy and cost */
} nvc_fep_optimization_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for NVC-FEP bridge
 */
typedef struct {
    /** Precision modulation */
    nvc_fep_precision_mode_t precision_mode;
    float precision_metabolic_scale;     /**< Metabolic effect on precision */
    float precision_floor;               /**< Minimum precision */
    float precision_ceiling;             /**< Maximum precision */

    /** Action cost integration */
    bool enable_action_costs;            /**< Include metabolic action costs */
    float action_cost_scale;             /**< Action cost scaling */
    float action_cost_threshold;         /**< Threshold for costly actions */

    /** BOLD-PE correlation */
    bool track_bold_pe_correlation;      /**< Correlate BOLD with PE */
    float bold_integration_tau_ms;       /**< BOLD integration time constant */
    float pe_smoothing_tau_ms;           /**< PE smoothing time constant */

    /** Optimization */
    nvc_fep_optimization_t optimization_target;
    float accuracy_weight;               /**< Weight for accuracy in balance */
    float efficiency_weight;             /**< Weight for efficiency in balance */

    /** Model complexity penalty */
    bool enable_complexity_penalty;      /**< Metabolic penalty for complexity */
    float complexity_cost_scale;         /**< Complexity cost factor */

    /** Update parameters */
    float update_interval_ms;            /**< Bridge update interval */
} nvc_fep_config_t;

/**
 * @brief Precision state for FEP region
 */
typedef struct {
    uint32_t region_id;                  /**< FEP region ID */
    uint32_t nvc_unit_id;                /**< Mapped NVC unit */
    nvc_fep_level_t level;               /**< Hierarchical level */

    /** Metabolic state */
    float metabolic_capacity;            /**< Current metabolic capacity (0-1) */
    float oxygen_availability;           /**< O2 for computation (0-1) */
    float energy_expenditure;            /**< Current energy use (0-1) */

    /** Precision values */
    float sensory_precision;             /**< Precision on sensory input */
    float prior_precision;               /**< Precision on prior beliefs */
    float effective_precision;           /**< Net precision (metabolically-scaled) */

    /** Free energy components */
    float prediction_error;              /**< Current prediction error */
    float precision_weighted_pe;         /**< Precision-weighted PE */
    float complexity;                    /**< Model complexity estimate */
    float free_energy;                   /**< Total free energy */

    /** Action costs */
    float action_metabolic_cost;         /**< Metabolic cost of actions */
    float expected_free_energy;          /**< Expected FE for active inference */
} nvc_fep_precision_state_t;

/**
 * @brief BOLD-Prediction Error correlation
 */
typedef struct {
    uint32_t region_id;                  /**< Region ID */
    float bold_signal;                   /**< Current BOLD (% change) */
    float integrated_pe;                 /**< Integrated prediction error */
    float correlation;                   /**< BOLD-PE correlation */
    float correlation_window_ms;         /**< Integration window */
} nvc_fep_bold_pe_t;

/**
 * @brief Metabolic action cost
 */
typedef struct {
    uint32_t action_id;                  /**< Action identifier */
    float expected_metabolic_cost;       /**< Expected metabolic cost */
    float actual_metabolic_cost;         /**< Actual cost (after execution) */
    float cbf_increase_required;         /**< CBF increase needed */
    float action_affordability;          /**< Can current metabolism support */
} nvc_fep_action_cost_t;

/**
 * @brief Free energy gradient for hemodynamics
 */
typedef struct {
    uint32_t region_id;                  /**< Region ID */
    float fe_gradient;                   /**< Free energy gradient */
    float requested_cbf_change;          /**< Requested CBF change (%) */
    float urgency;                       /**< Urgency of metabolic demand */
} nvc_fep_demand_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t updates;                    /**< Total update calls */
    uint64_t precision_modulations;      /**< Precision adjustments made */
    uint64_t action_cost_evaluations;    /**< Action costs computed */

    /** Averages */
    float mean_effective_precision;      /**< Mean metabolic-scaled precision */
    float mean_free_energy;              /**< Mean free energy */
    float mean_bold_pe_correlation;      /**< Mean BOLD-PE correlation */
    float mean_metabolic_efficiency;     /**< Mean efficiency */

    /** Extremes */
    float max_free_energy;               /**< Peak free energy */
    float max_action_cost;               /**< Highest action cost */
    float min_precision;                 /**< Lowest precision reached */

    float last_update_ms;                /**< Last update timestamp */
} nvc_fep_stats_t;

/** Opaque bridge handle */
typedef struct nvc_fep_bridge_struct nvc_fep_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_fep_default_config(nvc_fep_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create NVC-FEP bridge
 *
 * WHAT: Allocates and initializes the bridge structure
 * WHY:  Establishes metabolic-predictive coupling
 * HOW:  Creates internal state for precision and FE tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT nvc_fep_bridge_t* nvc_fep_bridge_create(
    const nvc_fep_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void nvc_fep_bridge_destroy(nvc_fep_bridge_t* bridge);

//=============================================================================
// Region Management API
//=============================================================================

/**
 * @brief Register FEP region with NVC unit
 *
 * WHAT: Maps FEP region to vascular territory
 * WHY:  Enables local metabolic modulation of precision
 * HOW:  Creates region entry with NVC mapping
 *
 * @param bridge Bridge handle
 * @param region_id FEP region ID
 * @param nvc_unit_id NVC unit ID
 * @param level Hierarchical level
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_fep_register_region(
    nvc_fep_bridge_t* bridge,
    uint32_t region_id,
    uint32_t nvc_unit_id,
    nvc_fep_level_t level
);

/**
 * @brief Unregister FEP region
 *
 * @param bridge Bridge handle
 * @param region_id Region to unregister
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_fep_unregister_region(
    nvc_fep_bridge_t* bridge,
    uint32_t region_id
);

//=============================================================================
// Metabolic State API
//=============================================================================

/**
 * @brief Update metabolic state from NVC
 *
 * WHAT: Receives blood flow state for precision calculation
 * WHY:  Metabolic capacity affects precision weighting
 * HOW:  Converts CBF/OEF to metabolic capacity estimate
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id NVC unit providing state
 * @param cbf Cerebral blood flow
 * @param cbf_baseline Baseline CBF
 * @param oef Oxygen extraction fraction
 * @param bold BOLD signal (% change)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_fep_update_from_nvc(
    nvc_fep_bridge_t* bridge,
    uint32_t nvc_unit_id,
    float cbf,
    float cbf_baseline,
    float oef,
    float bold
);

//=============================================================================
// Precision API
//=============================================================================

/**
 * @brief Compute metabolically-scaled precision
 *
 * WHAT: Calculates effective precision given metabolic state
 * WHY:  Precision requires metabolic resources
 * HOW:  Scales raw precision by metabolic capacity
 *
 * @param bridge Bridge handle
 * @param region_id FEP region
 * @param raw_precision Requested precision
 * @return Effective precision (metabolically-scaled)
 */
NIMCP_EXPORT float nvc_fep_compute_precision(
    nvc_fep_bridge_t* bridge,
    uint32_t region_id,
    float raw_precision
);

/**
 * @brief Get precision state for region
 *
 * @param bridge Bridge handle
 * @param region_id Region to query
 * @param state Output precision state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_fep_get_precision_state(
    const nvc_fep_bridge_t* bridge,
    uint32_t region_id,
    nvc_fep_precision_state_t* state
);

/**
 * @brief Set sensory and prior precisions
 *
 * WHAT: Updates precision values from FEP system
 * WHY:  Precisions are inputs to metabolic scaling
 * HOW:  Stores values, computes effective precision
 *
 * @param bridge Bridge handle
 * @param region_id FEP region
 * @param sensory_precision Precision on sensory input
 * @param prior_precision Precision on prior beliefs
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_fep_set_precisions(
    nvc_fep_bridge_t* bridge,
    uint32_t region_id,
    float sensory_precision,
    float prior_precision
);

//=============================================================================
// Free Energy API
//=============================================================================

/**
 * @brief Update prediction error from FEP
 *
 * WHAT: Receives prediction error for integration
 * WHY:  PE drives metabolic demand signal
 * HOW:  Integrates PE, correlates with BOLD
 *
 * @param bridge Bridge handle
 * @param region_id FEP region
 * @param prediction_error Current prediction error
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_fep_update_prediction_error(
    nvc_fep_bridge_t* bridge,
    uint32_t region_id,
    float prediction_error
);

/**
 * @brief Get free energy for region
 *
 * WHAT: Returns current free energy estimate
 * WHY:  Free energy drives system dynamics
 * HOW:  Combines precision-weighted PE and complexity
 *
 * @param bridge Bridge handle
 * @param region_id FEP region
 * @return Free energy value
 */
NIMCP_EXPORT float nvc_fep_get_free_energy(
    const nvc_fep_bridge_t* bridge,
    uint32_t region_id
);

/**
 * @brief Get BOLD-PE correlation
 *
 * @param bridge Bridge handle
 * @param region_id FEP region
 * @param bold_pe Output BOLD-PE correlation data
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_fep_get_bold_pe_correlation(
    const nvc_fep_bridge_t* bridge,
    uint32_t region_id,
    nvc_fep_bold_pe_t* bold_pe
);

//=============================================================================
// Action Cost API
//=============================================================================

/**
 * @brief Evaluate metabolic cost of action
 *
 * WHAT: Computes metabolic cost for active inference action
 * WHY:  Actions have metabolic consequences
 * HOW:  Estimates CBF increase needed, checks affordability
 *
 * @param bridge Bridge handle
 * @param region_id FEP region
 * @param action_id Action identifier
 * @param expected_activity Expected neural activity increase
 * @param cost Output action cost structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_fep_evaluate_action_cost(
    nvc_fep_bridge_t* bridge,
    uint32_t region_id,
    uint32_t action_id,
    float expected_activity,
    nvc_fep_action_cost_t* cost
);

/**
 * @brief Check if action is metabolically affordable
 *
 * @param bridge Bridge handle
 * @param region_id FEP region
 * @param expected_cost Expected metabolic cost
 * @return true if action is affordable
 */
NIMCP_EXPORT bool nvc_fep_action_affordable(
    const nvc_fep_bridge_t* bridge,
    uint32_t region_id,
    float expected_cost
);

//=============================================================================
// Hemodynamic Demand API
//=============================================================================

/**
 * @brief Get hemodynamic demand from free energy
 *
 * WHAT: Converts FE gradient to CBF demand
 * WHY:  Free energy drives metabolic needs
 * HOW:  Higher PE/FE requests more blood flow
 *
 * @param bridge Bridge handle
 * @param region_id FEP region
 * @param demand Output demand structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_fep_get_hemodynamic_demand(
    const nvc_fep_bridge_t* bridge,
    uint32_t region_id,
    nvc_fep_demand_t* demand
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of precision and FE dynamics
 * WHY:  Integrate PE, update correlations, decay states
 * HOW:  Called during simulation timestep
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_fep_update(
    nvc_fep_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_fep_reset(nvc_fep_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_fep_get_stats(
    const nvc_fep_bridge_t* bridge,
    nvc_fep_stats_t* stats
);

/**
 * @brief Get metabolic efficiency for region
 *
 * WHAT: Returns efficiency metric
 * WHY:  Measures FE per unit metabolic cost
 * HOW:  Ratio of FE reduction to energy spent
 *
 * @param bridge Bridge handle
 * @param region_id FEP region
 * @return Metabolic efficiency (higher is better)
 */
NIMCP_EXPORT float nvc_fep_get_metabolic_efficiency(
    const nvc_fep_bridge_t* bridge,
    uint32_t region_id
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NVC_FEP_BRIDGE_H */