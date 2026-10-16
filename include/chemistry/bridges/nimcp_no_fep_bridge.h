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
// nimcp_no_fep_bridge.h - Nitric Oxide to Free Energy Principle Bridge
//=============================================================================
/**
 * @file nimcp_no_fep_bridge.h
 * @brief Bidirectional bridge between Nitric Oxide signaling and FEP systems
 *
 * WHAT: Connects NO gasotransmitter signaling with Free Energy Principle (FEP)
 *       for predictive processing and active inference.
 *
 * WHY:  Nitric oxide modulates neural computation in ways relevant to FEP:
 *       - NO release signals prediction errors (unexpected NMDA activation)
 *       - Volume transmission enables distributed precision weighting
 *       - Vasodilation adjusts metabolic allocation based on surprise
 *       - cGMP pathway modulates gain (precision) of neural responses
 *
 * HOW:  Bidirectional integration:
 *       1. FEP → NO: High prediction error triggers NO release
 *       2. NO → FEP: NO modulates precision of predictions
 *       3. Vasodilation: Metabolic resources track free energy
 *       4. Volume transmission: Spatial precision modulation
 *
 * THEORETICAL BASIS:
 * ```
 * FREE ENERGY PRINCIPLE                    NO SIGNALING
 * ─────────────────────────────────────────────────────────────────
 * Prediction error (surprise)           → NMDA activation → NO release
 * Precision weighting                   ← cGMP modulates neural gain
 * Active inference (action)             → NOS regulation
 * Expected free energy                  → Metabolic/vascular allocation
 * Markov blanket dynamics               ← Volume transmission defines boundaries
 * ```
 *
 * NO-FEP MAPPINGS:
 * - High NO: High precision, strong prediction error signaling
 * - Low NO: Low precision, flexible belief updating
 * - NO diffusion: Spatial extent of precision modulation
 * - Vasodilation: Metabolic free energy allocation
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NO_FEP_BRIDGE_H
#define NIMCP_NO_FEP_BRIDGE_H

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
#define NO_FEP_MODULE_NAME              "no_fep_bridge"

/** Maximum prediction units tracked */
#define NO_FEP_MAX_UNITS                256

/** Default NO-to-precision scaling factor */
#define NO_FEP_PRECISION_SCALE          0.1f

/** Default prediction error threshold for NO trigger */
#define NO_FEP_PE_THRESHOLD             0.5f

/** Default vasodilation-free energy coupling */
#define NO_FEP_VASO_FE_COUPLING         0.3f

/** Bio-async module ID */
#define BIO_MODULE_NO_FEP               0x0E03

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief NO-FEP coupling mode
 */
typedef enum {
    NO_FEP_MODE_PRECISION = 0,          /**< NO modulates precision only */
    NO_FEP_MODE_PREDICTION,             /**< NO modulates predictions */
    NO_FEP_MODE_METABOLIC,              /**< NO allocates metabolic resources */
    NO_FEP_MODE_FULL                    /**< Full bidirectional coupling */
} no_fep_mode_t;

/**
 * @brief Precision modulation target
 */
typedef enum {
    NO_FEP_PREC_SENSORY = 0,            /**< Sensory precision (bottom-up) */
    NO_FEP_PREC_PRIOR,                  /**< Prior precision (top-down) */
    NO_FEP_PREC_LIKELIHOOD,             /**< Likelihood precision */
    NO_FEP_PREC_ALL                     /**< All precision types */
} no_fep_precision_target_t;

/**
 * @brief Free energy computation level
 */
typedef enum {
    NO_FEP_LEVEL_LOCAL = 0,             /**< Local (single unit) */
    NO_FEP_LEVEL_REGIONAL,              /**< Regional (diffusion sphere) */
    NO_FEP_LEVEL_GLOBAL                 /**< Global (system-wide) */
} no_fep_level_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for NO-FEP bridge
 */
typedef struct {
    /** Coupling parameters */
    no_fep_mode_t mode;                      /**< Coupling mode */
    no_fep_precision_target_t prec_target;   /**< Precision target */
    no_fep_level_t fe_level;                 /**< Free energy level */

    /** NO → Precision mapping */
    float no_to_precision_scale;             /**< NO concentration to precision */
    float precision_floor;                   /**< Minimum precision value */
    float precision_ceiling;                 /**< Maximum precision value */
    float cgmp_precision_gain;               /**< cGMP enhancement of precision */

    /** Prediction error → NO mapping */
    float pe_threshold;                      /**< PE threshold for NO trigger */
    float pe_to_no_scale;                    /**< PE magnitude to NO rate */
    float pe_no_saturation;                  /**< NO production saturation */

    /** Metabolic/vascular coupling */
    bool enable_vascular_coupling;           /**< Enable vasodilation effects */
    float vaso_fe_coupling;                  /**< Vasodilation-FE coupling */
    float metabolic_weight;                  /**< Metabolic resource weight */

    /** Volume transmission */
    bool enable_spatial_precision;           /**< Spatial precision modulation */
    float spatial_radius_um;                 /**< Precision modulation radius */
    float spatial_decay_rate;                /**< Distance decay */

    /** Timing */
    float update_interval_ms;                /**< Update interval */

    /** Features */
    bool enable_bio_async;                   /**< Bio-async messaging */
} no_fep_config_t;

/**
 * @brief Prediction unit with NO modulation
 */
typedef struct {
    uint32_t unit_id;                        /**< Prediction unit ID */
    float position[3];                       /**< 3D position (um) */

    /** NO state */
    float local_no_nm;                       /**< Local NO concentration */
    float local_cgmp_um;                     /**< Local cGMP level */
    uint32_t no_source_id;                   /**< Associated NO source */

    /** FEP state */
    float prediction;                        /**< Current prediction */
    float sensory_input;                     /**< Sensory input */
    float prediction_error;                  /**< Current PE */
    float free_energy;                       /**< Local free energy */

    /** Precision */
    float sensory_precision;                 /**< Sensory (bottom-up) precision */
    float prior_precision;                   /**< Prior (top-down) precision */
    float no_precision_mod;                  /**< NO precision modifier */

    /** Metabolic */
    float metabolic_demand;                  /**< Metabolic requirement */
    float blood_flow_factor;                 /**< Vasodilation effect */

    bool active;
} no_fep_unit_t;

/**
 * @brief Free energy update event
 */
typedef struct {
    uint32_t unit_id;                        /**< Unit ID */
    float prediction_error;                  /**< Prediction error */
    float no_response;                       /**< NO response to PE */
    float precision_change;                  /**< Precision modification */
    float free_energy_delta;                 /**< Free energy change */
    float vascular_response;                 /**< Vasodilation change */
    float event_time_ms;
} no_fep_event_t;

/**
 * @brief Regional free energy summary
 */
typedef struct {
    float center[3];                         /**< Region center (um) */
    float radius_um;                         /**< Region radius */

    float mean_no_level;                     /**< Mean NO in region */
    float mean_precision;                    /**< Mean precision */
    float total_free_energy;                 /**< Total regional FE */
    float vasodilation_index;                /**< Regional blood flow */

    uint32_t num_units;                      /**< Units in region */
    uint32_t high_pe_units;                  /**< High prediction error units */
} no_fep_region_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;                  /**< Total updates */
    uint64_t pe_no_triggers;                 /**< PE-triggered NO release */
    uint64_t precision_modulations;          /**< Precision changes */
    uint64_t vascular_adjustments;           /**< Blood flow changes */

    float mean_prediction_error;             /**< System-wide mean PE */
    float mean_precision;                    /**< System-wide mean precision */
    float total_free_energy;                 /**< System total FE */
    float mean_no_level;                     /**< System mean NO */
    float mean_vasodilation;                 /**< Mean vasodilation */

    uint32_t active_units;                   /**< Active prediction units */
    float last_update_ms;
} no_fep_stats_t;

/** Opaque bridge handle */
typedef struct no_fep_bridge_struct no_fep_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_default_config(no_fep_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create NO-FEP bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT no_fep_bridge_t* no_fep_bridge_create(
    const no_fep_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void no_fep_bridge_destroy(no_fep_bridge_t* bridge);

//=============================================================================
// Unit Management API
//=============================================================================

/**
 * @brief Register prediction unit for NO-FEP coupling
 *
 * WHAT: Adds prediction unit to NO-FEP tracking
 * WHY:  Link FEP computations with NO signaling
 * HOW:  Creates unit entry with spatial position
 *
 * @param bridge Bridge handle
 * @param unit_id Prediction unit ID
 * @param position 3D position [x, y, z] in um
 * @param no_source_id Associated NO source (or 0)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_register_unit(
    no_fep_bridge_t* bridge,
    uint32_t unit_id,
    const float position[3],
    uint32_t no_source_id
);

/**
 * @brief Unregister prediction unit
 *
 * @param bridge Bridge handle
 * @param unit_id Unit to remove
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_unregister_unit(
    no_fep_bridge_t* bridge,
    uint32_t unit_id
);

//=============================================================================
// Prediction Error -> NO API
//=============================================================================

/**
 * @brief Report prediction error to trigger NO response
 *
 * WHAT: Sends prediction error to bridge for NO modulation
 * WHY:  High PE signals unexpected input -> NMDA activation -> NO
 * HOW:  If PE exceeds threshold, triggers NO production
 *
 * @param bridge Bridge handle
 * @param unit_id Prediction unit ID
 * @param prediction_error Prediction error magnitude
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_report_prediction_error(
    no_fep_bridge_t* bridge,
    uint32_t unit_id,
    float prediction_error
);

/**
 * @brief Set prediction and sensory input for unit
 *
 * WHAT: Updates unit's prediction/sensory state
 * WHY:  Required for PE computation
 * HOW:  Stores values, computes PE, triggers NO if needed
 *
 * @param bridge Bridge handle
 * @param unit_id Unit ID
 * @param prediction Current prediction
 * @param sensory_input Sensory observation
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_set_prediction_state(
    no_fep_bridge_t* bridge,
    uint32_t unit_id,
    float prediction,
    float sensory_input
);

//=============================================================================
// NO -> Precision API
//=============================================================================

/**
 * @brief Get NO-modulated precision for unit
 *
 * WHAT: Returns precision with NO modulation applied
 * WHY:  NO/cGMP modulates neural gain (precision)
 * HOW:  Scales precision based on local NO concentration
 *
 * @param bridge Bridge handle
 * @param unit_id Unit ID
 * @param[out] precision Modulated precision value
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_get_precision(
    const no_fep_bridge_t* bridge,
    uint32_t unit_id,
    float* precision
);

/**
 * @brief Get sensory precision (bottom-up)
 *
 * @param bridge Bridge handle
 * @param unit_id Unit ID
 * @param[out] sensory_precision Sensory precision
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_get_sensory_precision(
    const no_fep_bridge_t* bridge,
    uint32_t unit_id,
    float* sensory_precision
);

/**
 * @brief Get prior precision (top-down)
 *
 * @param bridge Bridge handle
 * @param unit_id Unit ID
 * @param[out] prior_precision Prior precision
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_get_prior_precision(
    const no_fep_bridge_t* bridge,
    uint32_t unit_id,
    float* prior_precision
);

/**
 * @brief Set base precision for unit
 *
 * @param bridge Bridge handle
 * @param unit_id Unit ID
 * @param sensory_precision Base sensory precision
 * @param prior_precision Base prior precision
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_set_base_precision(
    no_fep_bridge_t* bridge,
    uint32_t unit_id,
    float sensory_precision,
    float prior_precision
);

//=============================================================================
// Free Energy / Metabolic API
//=============================================================================

/**
 * @brief Compute local free energy for unit
 *
 * WHAT: Calculates variational free energy at unit
 * WHY:  FEP core quantity - organisms minimize FE
 * HOW:  Combines PE with precision weights
 *
 * @param bridge Bridge handle
 * @param unit_id Unit ID
 * @param[out] free_energy Computed free energy
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_compute_free_energy(
    no_fep_bridge_t* bridge,
    uint32_t unit_id,
    float* free_energy
);

/**
 * @brief Get vasodilation factor (metabolic allocation)
 *
 * WHAT: Returns NO-mediated blood flow factor
 * WHY:  FE should correlate with metabolic demand
 * HOW:  NO vasodilation tracks computational demand
 *
 * @param bridge Bridge handle
 * @param unit_id Unit ID
 * @param[out] vaso_factor Vasodilation factor (1.0 = baseline)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_get_vasodilation(
    const no_fep_bridge_t* bridge,
    uint32_t unit_id,
    float* vaso_factor
);

/**
 * @brief Get regional free energy summary
 *
 * WHAT: Aggregates FE across spatial region
 * WHY:  Volume transmission creates coherent regions
 * HOW:  Sums FE within NO diffusion sphere
 *
 * @param bridge Bridge handle
 * @param center Region center [x, y, z]
 * @param radius_um Region radius
 * @param[out] region Regional summary
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_get_regional_fe(
    const no_fep_bridge_t* bridge,
    const float center[3],
    float radius_um,
    no_fep_region_t* region
);

/**
 * @brief Get system-wide total free energy
 *
 * @param bridge Bridge handle
 * @param[out] total_fe System total free energy
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_get_total_free_energy(
    const no_fep_bridge_t* bridge,
    float* total_fe
);

//=============================================================================
// Spatial Precision API
//=============================================================================

/**
 * @brief Propagate precision modulation via volume transmission
 *
 * WHAT: Spreads precision effect through NO diffusion
 * WHY:  Volume transmission affects nearby units
 * HOW:  Distance-dependent precision modulation
 *
 * @param bridge Bridge handle
 * @param source_unit Source of NO release
 * @return Number of units affected
 */
NIMCP_EXPORT int no_fep_propagate_precision(
    no_fep_bridge_t* bridge,
    uint32_t source_unit
);

/**
 * @brief Get units within precision modulation radius
 *
 * @param bridge Bridge handle
 * @param unit_id Central unit
 * @param[out] neighbor_ids Array for neighbor IDs
 * @param max_neighbors Maximum neighbors to return
 * @return Number of neighbors found
 */
NIMCP_EXPORT int no_fep_get_precision_neighbors(
    const no_fep_bridge_t* bridge,
    uint32_t unit_id,
    uint32_t* neighbor_ids,
    uint32_t max_neighbors
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of NO-FEP integration
 * WHY:  Advance precision modulation, FE computation
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_update(
    no_fep_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_reset(no_fep_bridge_t* bridge);

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
NIMCP_EXPORT int no_fep_get_stats(
    const no_fep_bridge_t* bridge,
    no_fep_stats_t* stats
);

/**
 * @brief Get unit state
 *
 * @param bridge Bridge handle
 * @param unit_id Unit ID
 * @param[out] unit Unit state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_fep_get_unit(
    const no_fep_bridge_t* bridge,
    uint32_t unit_id,
    no_fep_unit_t* unit
);

/**
 * @brief Check if prediction error exceeds NO trigger threshold
 *
 * @param bridge Bridge handle
 * @param unit_id Unit ID
 * @return true if PE exceeds threshold
 */
NIMCP_EXPORT bool no_fep_is_pe_above_threshold(
    const no_fep_bridge_t* bridge,
    uint32_t unit_id
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NO_FEP_BRIDGE_H */