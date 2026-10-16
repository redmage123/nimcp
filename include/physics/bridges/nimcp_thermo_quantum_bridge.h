//=============================================================================
// nimcp_thermo_quantum_bridge.h - Thermodynamics Quantum Monte Carlo Bridge
//=============================================================================
/**
 * @file nimcp_thermo_quantum_bridge.h
 * @brief QMC integration for Thermodynamics module
 *
 * WHAT: Quantum Monte Carlo methods for thermodynamic calculations
 *
 * WHY:  QMC provides efficient computation of:
 *       - Partition functions Z = sum(exp(-E/kT))
 *       - Free energy F = -kT ln(Z)
 *       - Entropy S = (E - F) / T
 *       - Landauer limit optimization
 *       - Neural computation energy costs
 *
 * HOW:  - Uses qmc_estimate_partition() for thermodynamic quantities
 *       - Uses qmc_adaptive_anneal() for Landauer optimization
 *       - Integrates with thermodynamics module for ATP/heat calculations
 *
 * BIOLOGICAL: Neural computation is thermodynamically constrained by
 * Landauer's principle (kT ln 2 per bit erased). QMC helps optimize
 * energy efficiency while maintaining computational capability.
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 * @version 1.0.0
 */

#ifndef NIMCP_THERMO_QUANTUM_BRIDGE_H
#define NIMCP_THERMO_QUANTUM_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "utils/quantum/nimcp_quantum_monte_carlo.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Boltzmann constant (eV/K) */
#define THERMO_QMC_KB               8.617333e-5f

/** Default temperature (K) - body temperature */
#define THERMO_QMC_DEFAULT_TEMP     310.15f

/** Default number of MCMC samples */
#define THERMO_QMC_DEFAULT_SAMPLES  10000

/** Default burn-in period */
#define THERMO_QMC_DEFAULT_BURNIN   500

//=============================================================================
// Partition Function Types
//=============================================================================

/**
 * @brief Configuration for partition function estimation
 */
typedef struct {
    float temperature;              /**< System temperature (K) */
    uint32_t num_samples;           /**< MCMC samples */
    uint32_t burnin;                /**< Burn-in period */
    uint32_t thinning;              /**< Thinning interval */
    uint32_t seed;
} thermo_partition_config_t;

/**
 * @brief Result of partition function estimation
 */
typedef struct {
    float log_Z;                    /**< log(partition function) */
    float free_energy;              /**< Helmholtz free energy F = -kT ln(Z) */
    float entropy;                  /**< Entropy S */
    float internal_energy;          /**< Mean internal energy <E> */
    float heat_capacity;            /**< Heat capacity C_v */
    float energy_variance;          /**< Var(E) */
    float std_error;                /**< Standard error */
} thermo_partition_result_t;

//=============================================================================
// Landauer Optimization Types
//=============================================================================

/**
 * @brief Configuration for Landauer optimization
 */
typedef struct {
    /** Annealing parameters */
    float initial_temp;
    float final_temp;
    uint32_t num_iterations;
    float quantum_strength;

    /** Optimization constraints */
    float min_computation_rate;     /**< Min bits/s to maintain */
    float max_heat_dissipation;     /**< Max heat allowed (W) */
    float atp_budget;               /**< Available ATP pool */

    /** Physical bounds */
    float min_temperature;          /**< Min operating temp (K) */
    float max_temperature;          /**< Max operating temp (K) */

    uint32_t seed;
} thermo_landauer_config_t;

/**
 * @brief Result of Landauer optimization
 */
typedef struct {
    float optimal_temperature;      /**< Optimal operating temperature */
    float optimal_bit_rate;         /**< Optimal computation rate (bits/s) */
    float min_energy_per_bit;       /**< Achieved energy per bit (J) */
    float landauer_efficiency;      /**< Efficiency vs Landauer limit [0,1] */
    float atp_consumption_rate;     /**< ATP molecules/s */
    float heat_production;          /**< Heat production (W) */

    /** Optimization stats */
    float final_energy;
    float acceptance_rate;
    uint32_t iterations_run;
    bool converged;
} thermo_landauer_result_t;

//=============================================================================
// ATP Pool Optimization Types
//=============================================================================

/**
 * @brief Configuration for ATP pool optimization
 */
typedef struct {
    float target_efficiency;        /**< Target ATP efficiency [0,1] */
    float min_reserve;              /**< Minimum ATP reserve */
    float production_rate;          /**< ATP production rate */
    float consumption_rate;         /**< ATP consumption rate */
    uint32_t num_iterations;
    uint32_t seed;
} thermo_atp_config_t;

/**
 * @brief Result of ATP pool optimization
 */
typedef struct {
    float optimal_pool_size;        /**< Optimal ATP pool */
    float optimal_turnover;         /**< Optimal ATP turnover rate */
    float achieved_efficiency;      /**< Achieved efficiency */
    float sustainable_rate;         /**< Sustainable consumption rate */
    float recovery_time;            /**< Time to recover from depletion */
    bool stable;                    /**< Is the optimum stable */
} thermo_atp_result_t;

//=============================================================================
// Entropy Production Types
//=============================================================================

/**
 * @brief Configuration for entropy production analysis
 */
typedef struct {
    float time_window_ms;           /**< Analysis time window */
    uint32_t num_samples;           /**< Samples for estimation */
    bool include_channel_entropy;   /**< Include ion channel entropy */
    bool include_transport_entropy; /**< Include active transport entropy */
    uint32_t seed;
} thermo_entropy_config_t;

/**
 * @brief Result of entropy production analysis
 */
typedef struct {
    float total_entropy_rate;       /**< Total entropy production (J/K/s) */
    float channel_entropy;          /**< Ion channel contribution */
    float transport_entropy;        /**< Active transport contribution */
    float dissipation_rate;         /**< Energy dissipation rate (W) */
    float information_cost;         /**< Information processing cost (bits/J) */
    float thermodynamic_efficiency; /**< Overall efficiency */
} thermo_entropy_result_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default partition function configuration
 *
 * @param config    Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_qmc_partition_default_config(
    thermo_partition_config_t* config
);

/**
 * @brief Get default Landauer optimization configuration
 *
 * @param config    Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_qmc_landauer_default_config(
    thermo_landauer_config_t* config
);

/**
 * @brief Get default ATP optimization configuration
 *
 * @param config    Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_qmc_atp_default_config(
    thermo_atp_config_t* config
);

/**
 * @brief Get default entropy analysis configuration
 *
 * @param config    Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_qmc_entropy_default_config(
    thermo_entropy_config_t* config
);

//=============================================================================
// Partition Function API
//=============================================================================

/**
 * @brief Estimate partition function for neural system
 *
 * WHAT: Compute Z and derived thermodynamic quantities
 * WHY:  Required for free energy, entropy, heat capacity
 * HOW:  MCMC sampling with thermodynamic integration
 *
 * @param system    Thermodynamic system state
 * @param config    Estimation configuration (NULL for defaults)
 * @param result    Output partition result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_qmc_estimate_partition(
    const nimcp_thermodynamic_state_t* system,
    const thermo_partition_config_t* config,
    thermo_partition_result_t* result
);

/**
 * @brief Compute free energy landscape
 *
 * @param system        Thermodynamic system
 * @param temp_min      Minimum temperature to scan
 * @param temp_max      Maximum temperature to scan
 * @param num_points    Number of temperature points
 * @param config        Base configuration
 * @param temperatures  Output: temperature values
 * @param free_energies Output: free energy at each temp
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_qmc_free_energy_landscape(
    const nimcp_thermodynamic_state_t* system,
    float temp_min,
    float temp_max,
    uint32_t num_points,
    const thermo_partition_config_t* config,
    float* temperatures,
    float* free_energies
);

//=============================================================================
// Landauer Optimization API
//=============================================================================

/**
 * @brief Optimize computation efficiency vs Landauer limit
 *
 * WHAT: Find optimal parameters for minimal energy computation
 * WHY:  Approach theoretical limits of efficient computation
 * HOW:  QMC annealing to minimize energy per bit
 *
 * @param system    Thermodynamic system (modified in place)
 * @param config    Optimization configuration
 * @param result    Output optimization result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_qmc_optimize_landauer(
    nimcp_thermodynamic_state_t* system,
    const thermo_landauer_config_t* config,
    thermo_landauer_result_t* result
);

/**
 * @brief Compute Landauer efficiency at current state
 *
 * @param system        Thermodynamic system
 * @param bit_rate      Current computation rate (bits/s)
 * @param efficiency    Output: efficiency vs Landauer limit
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_qmc_landauer_efficiency(
    const nimcp_thermodynamic_state_t* system,
    float bit_rate,
    float* efficiency
);

/**
 * @brief Compute minimum energy per bit at temperature
 *
 * @param temperature   System temperature (K)
 * @param energy_per_bit Output: minimum energy (J/bit)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_qmc_landauer_limit(
    float temperature,
    float* energy_per_bit
);

//=============================================================================
// ATP Pool Optimization API
//=============================================================================

/**
 * @brief Optimize ATP pool management
 *
 * @param system    Thermodynamic system
 * @param config    ATP optimization configuration
 * @param result    Output optimization result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_qmc_optimize_atp(
    nimcp_thermodynamic_state_t* system,
    const thermo_atp_config_t* config,
    thermo_atp_result_t* result
);

/**
 * @brief Estimate ATP sustainability
 *
 * @param system        Thermodynamic system
 * @param load_factor   Computational load [0,1]
 * @param sustainability Output: sustainability score [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_qmc_atp_sustainability(
    const nimcp_thermodynamic_state_t* system,
    float load_factor,
    float* sustainability
);

//=============================================================================
// Entropy Production API
//=============================================================================

/**
 * @brief Analyze entropy production in neural computation
 *
 * WHAT: Compute entropy production rate and sources
 * WHY:  Understand thermodynamic costs of neural processing
 * HOW:  Sample entropy production over time window
 *
 * @param system    Thermodynamic system
 * @param config    Analysis configuration
 * @param result    Output entropy analysis result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_qmc_entropy_production(
    const nimcp_thermodynamic_state_t* system,
    const thermo_entropy_config_t* config,
    thermo_entropy_result_t* result
);

/**
 * @brief Compute thermodynamic cost of information
 *
 * @param system    Thermodynamic system
 * @param bits      Information processed (bits)
 * @param energy_cost Output: energy cost (J)
 * @param entropy_cost Output: entropy cost (J/K)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_qmc_information_cost(
    const nimcp_thermodynamic_state_t* system,
    float bits,
    float* energy_cost,
    float* entropy_cost
);

//=============================================================================
// Temperature Optimization API
//=============================================================================

/**
 * @brief Find optimal operating temperature
 *
 * @param system    Thermodynamic system
 * @param temp_min  Minimum allowable temperature (K)
 * @param temp_max  Maximum allowable temperature (K)
 * @param optimal_temp Output: optimal temperature
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_qmc_optimal_temperature(
    nimcp_thermodynamic_state_t* system,
    float temp_min,
    float temp_max,
    float* optimal_temp
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THERMO_QUANTUM_BRIDGE_H */
