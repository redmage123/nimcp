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
// nimcp_thermo_fep_bridge.h - Thermodynamics to Free Energy Principle Bridge
//=============================================================================
/**
 * @file nimcp_thermo_fep_bridge.h
 * @brief Natural integration of thermodynamics with the Free Energy Principle
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bridges non-equilibrium thermodynamics to the Free Energy Principle,
 *       providing physical grounding for variational free energy minimization.
 *
 * WHY:  The FEP and thermodynamics share deep mathematical structure:
 *       - Variational free energy F = E - T*S (identical form to Helmholtz)
 *       - Surprise minimization analogous to entropy production minimization
 *       - Active inference as non-equilibrium steady state maintenance
 *       - Belief updating has thermodynamic costs (Landauer's principle)
 *       - Precision weighting maps to inverse temperature (beta = 1/kT)
 *
 * HOW:  - Computes thermodynamic bounds on inference
 *       - Maps temperature to precision/confidence
 *       - Tracks energy cost of belief updates
 *       - Provides entropy production rate for model comparison
 *       - Links active inference to heat dissipation
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * THERMODYNAMIC-FEP CORRESPONDENCE:
 * ---------------------------------
 * 1. Free Energy Formulation:
 *    - Thermodynamic: F = E - TS (Helmholtz free energy)
 *    - Variational: F = E[ln q(z)] - E[ln p(o,z)] (ELBO)
 *    - Both minimize F subject to constraints
 *
 * 2. Precision and Temperature:
 *    - FEP precision pi = 1/sigma^2 (inverse variance)
 *    - Thermodynamic beta = 1/kT (inverse temperature)
 *    - High precision = low temperature = sharp distributions
 *    - Low precision = high temperature = diffuse distributions
 *
 * 3. Belief Updating Costs:
 *    - Each bit of belief change costs >= kT*ln(2) energy
 *    - KL divergence has physical interpretation
 *    - Free energy decrease bounded by heat dissipation
 *
 * 4. Active Inference and NESS:
 *    - Active inference maintains non-equilibrium steady states
 *    - Entropy production rate reflects prediction error
 *    - Heat dissipation = active inference cost
 *
 * 5. Model Evidence and Thermodynamics:
 *    - Log model evidence = -F (negative free energy)
 *    - Partition function Z = exp(-F/kT)
 *    - Bayesian model comparison maps to thermodynamic stability
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_THERMO_FEP_BRIDGE_H
#define NIMCP_THERMO_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "common/nimcp_export.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Module Constants
//=============================================================================

/** Module name for logging */
#define THERMO_FEP_MODULE_NAME              "thermo_fep_bridge"

/** Boltzmann constant (J/K) */
#define THERMO_FEP_KB                       1.380649e-23

/** Reference temperature (Kelvin) - body temperature */
#define THERMO_FEP_TEMP_REF_K               310.15f

/** Landauer limit at body temperature (J/bit) */
#define THERMO_FEP_LANDAUER_310K            2.966e-21

/** Natural log of 2 */
#define THERMO_FEP_LN2                      0.693147f

/** Default precision at reference temperature */
#define THERMO_FEP_DEFAULT_PRECISION        1.0f

/** Default learning rate */
#define THERMO_FEP_DEFAULT_LEARNING_RATE    0.1f

/** Maximum belief update per step (nats) */
#define THERMO_FEP_MAX_KL_STEP              10.0f

/** Default update interval (ms) */
#define THERMO_FEP_DEFAULT_UPDATE_MS        10.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Free energy computation method
 */
typedef enum {
    THERMO_FEP_METHOD_VARIATIONAL = 0,      /**< Standard variational FE */
    THERMO_FEP_METHOD_BETHE,                /**< Bethe free energy */
    THERMO_FEP_METHOD_THERMODYNAMIC,        /**< Pure thermodynamic */
    THERMO_FEP_METHOD_HYBRID                /**< Combined approach */
} thermo_fep_method_t;

/**
 * @brief Temperature-precision mapping mode
 */
typedef enum {
    THERMO_FEP_PRECISION_LINEAR = 0,        /**< Linear: pi = c/T */
    THERMO_FEP_PRECISION_INVERSE,           /**< Inverse: pi = 1/(kT) */
    THERMO_FEP_PRECISION_SIGMOID,           /**< Sigmoid mapping */
    THERMO_FEP_PRECISION_ADAPTIVE           /**< Context-dependent */
} thermo_fep_precision_mode_t;

/**
 * @brief Active inference action type
 */
typedef enum {
    THERMO_FEP_ACTION_PERCEPTION = 0,       /**< Perceptual inference */
    THERMO_FEP_ACTION_LEARNING,             /**< Model parameter update */
    THERMO_FEP_ACTION_MOTOR,                /**< Active inference/action */
    THERMO_FEP_ACTION_ATTENTION             /**< Precision optimization */
} thermo_fep_action_t;

//=============================================================================
// Configuration Structure
//=============================================================================

/**
 * @brief Configuration for thermo-FEP bridge
 *
 * WHAT: All parameters controlling thermodynamic-FEP integration
 * WHY:  Allows tuning the physical grounding of inference
 * HOW:  Temperature mappings, energy costs, and feature flags
 */
typedef struct {
    /* Computation method */
    thermo_fep_method_t method;             /**< Free energy method */
    thermo_fep_precision_mode_t precision_mode; /**< Precision mapping */

    /* Temperature parameters */
    float reference_temp_k;                 /**< Reference temperature (K) */
    float precision_scale;                  /**< Scale factor for precision */
    float min_precision;                    /**< Minimum precision (stability) */
    float max_precision;                    /**< Maximum precision (cap) */

    /* Energy cost parameters */
    float energy_per_nat;                   /**< Energy per nat of information */
    float base_metabolic_rate;              /**< Baseline energy for inference */
    float action_energy_scale;              /**< Scale for action costs */

    /* Learning parameters */
    float base_learning_rate;               /**< Base learning rate */
    float temp_learning_modulation;         /**< Temperature effect on learning */
    float max_kl_step;                      /**< Maximum KL divergence per step */

    /* Entropy production */
    float entropy_weight;                   /**< Weight for entropy term */
    float min_entropy_rate;                 /**< Minimum entropy production */

    /* Active inference */
    float action_precision;                 /**< Precision for action selection */
    float exploration_temp;                 /**< Temperature for exploration */

    /* ATP gating */
    float atp_full_threshold;               /**< ATP for full inference */
    float atp_minimal_threshold;            /**< ATP for minimal inference */

    /* Feature flags */
    bool enable_landauer_tracking;          /**< Track Landauer costs */
    bool enable_precision_modulation;       /**< Modulate precision by temp */
    bool enable_energy_bounds;              /**< Enforce thermodynamic bounds */
    bool enable_entropy_tracking;           /**< Track entropy production */
    bool enable_atp_gating;                 /**< Gate inference by ATP */
    bool enable_action_costs;               /**< Track action energy costs */

    /* Update parameters */
    float update_interval_ms;               /**< Bridge update interval */
} thermo_fep_config_t;

//=============================================================================
// Free Energy State Structure
//=============================================================================

/**
 * @brief Current free energy state
 *
 * WHAT: Complete thermodynamic-FEP state
 * WHY:  Tracks all relevant quantities for inference
 * HOW:  Updated each timestep based on beliefs and observations
 */
typedef struct {
    /* Free energy components */
    float variational_fe;                   /**< Variational free energy (nats) */
    float expected_energy;                  /**< Expected log likelihood */
    float entropy_term;                     /**< -E[ln q] entropy term */
    float complexity_term;                  /**< KL divergence (complexity) */

    /* Thermodynamic quantities */
    float helmholtz_fe;                     /**< Thermodynamic F = E - TS */
    float entropy_production_rate;          /**< dS/dt (W/K) */
    float heat_dissipation;                 /**< Q dissipated this step (J) */
    float work_extracted;                   /**< Useful work (J) */

    /* Energy costs */
    float inference_energy_cost;            /**< Energy for belief update (J) */
    float action_energy_cost;               /**< Energy for actions (J) */
    float total_energy_consumed;            /**< Cumulative energy (J) */
    float landauer_cost;                    /**< Landauer limit cost (J) */
    float landauer_efficiency;              /**< Efficiency vs Landauer [0,1] */

    /* Precision state */
    float current_precision;                /**< Current precision */
    float temperature_precision;            /**< Temperature-derived precision */
    float effective_precision;              /**< Combined precision */

    /* Surprise tracking */
    float surprise;                         /**< -ln p(o) surprise (nats) */
    float avg_surprise;                     /**< Running average surprise */
    float surprise_change;                  /**< dS/dt surprise rate */

    /* KL divergence */
    float kl_divergence;                    /**< KL(q||p) this step */
    float cumulative_kl;                    /**< Total KL over time */

    /* Model evidence */
    float log_model_evidence;               /**< ln p(o|m) */
    float model_evidence_rate;              /**< Rate of evidence change */

    /* ATP state */
    float atp_level;                        /**< Current ATP [0,1] */
    float inference_gate;                   /**< ATP gating factor [0,1] */

    /* Timestamp */
    uint64_t last_update_us;                /**< Last update timestamp */
    float simulation_time_s;                /**< Total simulation time */
} thermo_fep_state_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Update counts */
    uint64_t updates_performed;             /**< Total bridge updates */
    uint64_t inference_steps;               /**< Inference updates */
    uint64_t action_steps;                  /**< Action selections */

    /* Free energy stats */
    float min_fe_observed;                  /**< Minimum FE seen */
    float max_fe_observed;                  /**< Maximum FE seen */
    float avg_fe;                           /**< Average free energy */
    float fe_variance;                      /**< FE variance */

    /* Energy consumption */
    double total_inference_energy;          /**< Total inference energy (J) */
    double total_action_energy;             /**< Total action energy (J) */
    double total_landauer_cost;             /**< Total Landauer minimum (J) */
    float avg_landauer_efficiency;          /**< Average efficiency */

    /* Entropy production */
    double total_entropy_produced;          /**< Cumulative entropy (J/K) */
    float avg_entropy_rate;                 /**< Average dS/dt */
    float peak_entropy_rate;                /**< Peak entropy production */

    /* Precision stats */
    float min_precision;                    /**< Minimum precision */
    float max_precision;                    /**< Maximum precision */
    float avg_precision;                    /**< Average precision */

    /* KL divergence stats */
    double total_kl;                        /**< Total KL divergence */
    float avg_kl_per_step;                  /**< Average KL per step */
    float max_kl_step;                      /**< Maximum KL step */

    /* Temperature stats */
    float min_temp_k;                       /**< Minimum temperature */
    float max_temp_k;                       /**< Maximum temperature */
    float avg_temp_k;                       /**< Average temperature */

    /* Timing */
    uint64_t start_time_us;                 /**< Bridge start time */
    uint64_t total_runtime_us;              /**< Total running time */
} thermo_fep_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

/** Opaque bridge handle */
typedef struct thermo_fep_bridge_struct thermo_fep_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Simplifies bridge creation
 * HOW:  Sets physically-grounded parameters
 *
 * @param config    Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_fep_default_config(thermo_fep_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create thermo-FEP bridge
 *
 * WHAT: Allocate and initialize bridge instance
 * WHY:  Enables thermodynamic grounding of FEP
 * HOW:  Creates internal state, initializes tracking
 *
 * @param config    Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT thermo_fep_bridge_t* thermo_fep_bridge_create(
    const thermo_fep_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge    Bridge to destroy (NULL-safe)
 */
NIMCP_EXPORT void thermo_fep_bridge_destroy(thermo_fep_bridge_t* bridge);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect bridge to thermodynamic state
 *
 * WHAT: Link bridge to thermodynamics module
 * WHY:  Enables real-time thermodynamic grounding
 * HOW:  Stores reference to thermodynamic state for polling
 *
 * @param bridge    Bridge handle
 * @param thermo    Thermodynamic state to monitor
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_fep_connect_thermo(
    thermo_fep_bridge_t* bridge,
    const nimcp_thermodynamic_state_t* thermo
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of thermodynamic-FEP state
 * WHY:  Recomputes precision, tracks energy costs
 * HOW:  Reads temperature, updates precision mapping
 *
 * @param bridge    Bridge handle
 * @param dt_ms     Time step (milliseconds)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_fep_update(
    thermo_fep_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Set temperature directly
 *
 * WHAT: Manually set operating temperature
 * WHY:  For use without connected thermodynamic state
 * HOW:  Updates internal temperature, recomputes precision
 *
 * @param bridge        Bridge handle
 * @param temperature_k Temperature in Kelvin
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_fep_set_temperature(
    thermo_fep_bridge_t* bridge,
    float temperature_k
);

/**
 * @brief Register belief update for energy tracking
 *
 * WHAT: Record belief update and compute energy cost
 * WHY:  Tracks thermodynamic cost of inference
 * HOW:  Computes KL divergence, Landauer cost
 *
 * @param bridge        Bridge handle
 * @param kl_divergence KL divergence of update (nats)
 * @param action_type   Type of inference action
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_fep_register_update(
    thermo_fep_bridge_t* bridge,
    float kl_divergence,
    thermo_fep_action_t action_type
);

/**
 * @brief Set current free energy value
 *
 * WHAT: Input variational free energy from FEP module
 * WHY:  Bridge needs FE for thermodynamic analysis
 * HOW:  Stores FE, computes derived quantities
 *
 * @param bridge            Bridge handle
 * @param free_energy       Variational free energy (nats)
 * @param expected_energy   Expected energy term
 * @param entropy           Entropy term
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_fep_set_free_energy(
    thermo_fep_bridge_t* bridge,
    float free_energy,
    float expected_energy,
    float entropy
);

/**
 * @brief Reset bridge state
 *
 * @param bridge    Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_fep_reset(thermo_fep_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current FEP state
 *
 * WHAT: Retrieve complete thermodynamic-FEP state
 * WHY:  For analysis and monitoring
 * HOW:  Copies current state to output
 *
 * @param bridge    Bridge handle
 * @param state     Output state structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_fep_get_state(
    const thermo_fep_bridge_t* bridge,
    thermo_fep_state_t* state
);

/**
 * @brief Get current precision
 *
 * WHAT: Get temperature-modulated precision
 * WHY:  For use in FEP calculations
 * HOW:  Returns combined precision value
 *
 * @param bridge    Bridge handle
 * @return Current precision, or 0 on error
 */
NIMCP_EXPORT float thermo_fep_get_precision(
    const thermo_fep_bridge_t* bridge
);

/**
 * @brief Compute thermodynamic bound on inference
 *
 * WHAT: Compute minimum energy for given belief change
 * WHY:  Physical constraint on inference efficiency
 * HOW:  Returns Landauer limit for KL divergence
 *
 * @param bridge        Bridge handle
 * @param kl_divergence KL divergence (nats)
 * @return Minimum energy cost (J), or -1 on error
 */
NIMCP_EXPORT float thermo_fep_compute_min_energy(
    const thermo_fep_bridge_t* bridge,
    float kl_divergence
);

/**
 * @brief Get entropy production rate
 *
 * @param bridge    Bridge handle
 * @return Entropy production rate (W/K), or 0 on error
 */
NIMCP_EXPORT float thermo_fep_get_entropy_rate(
    const thermo_fep_bridge_t* bridge
);

/**
 * @brief Check if inference is thermodynamically permitted
 *
 * WHAT: Verify ATP and temperature allow inference
 * WHY:  Physical constraints on computation
 * HOW:  Checks ATP level and temperature bounds
 *
 * @param bridge        Bridge handle
 * @param kl_required   KL divergence required (nats)
 * @return true if inference is permitted
 */
NIMCP_EXPORT bool thermo_fep_is_inference_permitted(
    const thermo_fep_bridge_t* bridge,
    float kl_required
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge    Bridge handle
 * @param stats     Output statistics structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_fep_get_stats(
    const thermo_fep_bridge_t* bridge,
    thermo_fep_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge    Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_fep_reset_stats(thermo_fep_bridge_t* bridge);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Convert nats to bits
 *
 * @param nats  Information in nats
 * @return Information in bits
 */
NIMCP_EXPORT float thermo_fep_nats_to_bits(float nats);

/**
 * @brief Convert bits to nats
 *
 * @param bits  Information in bits
 * @return Information in nats
 */
NIMCP_EXPORT float thermo_fep_bits_to_nats(float bits);

/**
 * @brief Compute Landauer energy for bit erasure
 *
 * @param temperature_k Temperature (K)
 * @param num_bits      Number of bits
 * @return Minimum energy (J)
 */
NIMCP_EXPORT float thermo_fep_landauer_energy(
    float temperature_k,
    float num_bits
);

/**
 * @brief Get action type name
 *
 * @param action    Action type
 * @return Action name string
 */
NIMCP_EXPORT const char* thermo_fep_action_name(thermo_fep_action_t action);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge    Bridge handle
 */
NIMCP_EXPORT void thermo_fep_print_summary(const thermo_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THERMO_FEP_BRIDGE_H */