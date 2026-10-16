//=============================================================================
// nimcp_thermodynamics.h - Non-Equilibrium Thermodynamics Module (AC-9)
//=============================================================================
/**
 * @file nimcp_thermodynamics.h
 * @brief Non-equilibrium thermodynamics for neural computation
 * @version 1.0.0
 * @date 2026-01-12
 *
 * BIOLOGICAL: Neural computation has thermodynamic costs that fundamentally
 * constrain information processing. The brain consumes ~20W (20% of body's
 * energy) while comprising only 2% of body mass. This module models:
 *
 * 1. Energy Budget: ATP consumption for ion pumping, synaptic transmission,
 *    and housekeeping processes
 *
 * 2. Entropy Production: Neural systems operate far from equilibrium,
 *    continuously dissipating heat. dS/dt = Q/T + sigma_irr
 *
 * 3. Landauer's Principle: Information erasure has minimum energy cost
 *    E_min = kT * ln(2) per bit (~2.8 zJ at 37C / 310K)
 *
 * 4. Free Energy Dissipation: Tracks thermodynamic efficiency of computation
 *
 * WHAT: Models thermodynamic constraints on neural computation
 * WHY:  Enables biologically realistic energy-aware processing
 * HOW:  Tracks ATP, entropy production, Landauer costs, and efficiency
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_THERMODYNAMICS_H
#define NIMCP_THERMODYNAMICS_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Physical Constants
//=============================================================================

/** Boltzmann constant (J/K) */
#define NIMCP_THERMO_KB              1.380649e-23

/** Landauer limit at 310K (J/bit) - body temperature */
#define NIMCP_THERMO_LANDAUER_310K   2.966e-21

/** ATP hydrolysis energy (J) - approximately 50 kJ/mol */
#define NIMCP_THERMO_ATP_ENERGY      8.3e-20

/** Avogadro's number */
#define NIMCP_THERMO_AVOGADRO        6.02214076e23

/** Standard body temperature (K) */
#define NIMCP_THERMO_BODY_TEMP_K     310.0

/** Joules per calorie */
#define NIMCP_THERMO_J_PER_CAL       4.184

//=============================================================================
// Default Configuration Values
//=============================================================================

/** Default temperature (Kelvin) - body temperature */
#define NIMCP_THERMO_DEFAULT_TEMP_K         310.0

/** Default ATP pool size (moles) - approximate neuronal level */
#define NIMCP_THERMO_DEFAULT_ATP_POOL       1.0e-12

/** Default metabolic rate (W) - baseline neuronal metabolism */
#define NIMCP_THERMO_DEFAULT_METABOLIC_RATE 1.0e-9

/** Maximum entropy production rate (W/K) */
#define NIMCP_THERMO_MAX_ENTROPY_RATE       1.0e-6

//=============================================================================
// Thermodynamic State Structure
//=============================================================================

/**
 * @brief Complete thermodynamic state of a computational module
 *
 * BIOLOGICAL: Tracks all thermodynamic variables relevant to neural
 * computation including energy consumption, entropy production, and
 * metabolic resources.
 */
typedef struct nimcp_thermodynamic_state_s {
    //-------------------------------------------------------------------------
    // Energy Tracking (Joules, Watts)
    //-------------------------------------------------------------------------

    /** Total energy consumed since initialization (J) */
    double total_energy_consumed;

    /** Current power consumption rate (W) */
    double power_consumption;

    /** Heat dissipation rate (W) - thermal output */
    double heat_dissipation;

    /** Free energy dissipated in irreversible processes (J) */
    double free_energy_dissipated;

    //-------------------------------------------------------------------------
    // Entropy Production
    //-------------------------------------------------------------------------

    /** Entropy production rate dS/dt (W/K)
     *  Components: Q/T (heat flow) + sigma_irr (irreversible processes)
     */
    double entropy_production_rate;

    /** Cumulative entropy produced (J/K) */
    double total_entropy_produced;

    //-------------------------------------------------------------------------
    // ATP/Metabolic Resources
    //-------------------------------------------------------------------------

    /** Available ATP pool (moles) */
    double atp_available;

    /** ATP consumption rate (moles/s) */
    double atp_consumption_rate;

    /** Oxygen delivery rate (moles O2/s) */
    double oxygen_delivery_rate;

    /** Glucose consumption rate (moles/s) */
    double glucose_consumption_rate;

    //-------------------------------------------------------------------------
    // Efficiency Metrics
    //-------------------------------------------------------------------------

    /** Computational efficiency: useful work / total energy [0, 1] */
    double computational_efficiency;

    /** Thermodynamic efficiency: actual / Carnot limit [0, 1] */
    double thermodynamic_efficiency;

    /** Energy cost per bit operation (J/bit) */
    double energy_per_bit;

    /** Landauer efficiency: Landauer_limit / actual_cost [0, 1] */
    double landauer_efficiency;

    //-------------------------------------------------------------------------
    // Module Identification
    //-------------------------------------------------------------------------

    /** Module identifier for tracking */
    uint32_t module_id;

    /** Timestamp of last update (microseconds) */
    uint64_t last_update_us;

    /** Total simulation time (seconds) */
    double simulation_time;

    /** Initialization flag */
    bool initialized;

} nimcp_thermodynamic_state_t;

//=============================================================================
// Configuration Structure
//=============================================================================

/**
 * @brief Configuration for thermodynamic module
 */
typedef struct nimcp_thermo_config_s {
    /** System temperature (Kelvin) */
    double temperature_k;

    /** Initial ATP pool size (moles) */
    double atp_pool_size;

    /** Baseline metabolic rate (W) */
    double metabolic_rate;

    /** Enable Landauer cost tracking for bit erasure */
    bool enable_landauer_cost;

    /** Enable detailed entropy accounting */
    bool enable_entropy_tracking;

    /** Enable ATP depletion warnings */
    bool enable_atp_warnings;

    /** ATP warning threshold (fraction of initial pool) */
    double atp_warning_threshold;

    /** Maximum power budget (W) - 0 for unlimited */
    double max_power_budget;

    /** Heat dissipation coefficient (W/K) */
    double heat_dissipation_coeff;

    /** External temperature for heat flow (K) */
    double external_temp_k;

    /** Module ID for identification */
    uint32_t module_id;

} nimcp_thermo_config_t;

//=============================================================================
// Energy Budget Structure
//=============================================================================

/**
 * @brief Energy budget breakdown by process type
 *
 * BIOLOGICAL: Neural energy is consumed by:
 * - Ion pumping: Na+/K+-ATPase restoring gradients (~50% of neural energy)
 * - Synaptic: vesicle cycling, receptor binding (~25%)
 * - Housekeeping: protein synthesis, maintenance (~25%)
 */
typedef struct nimcp_energy_budget_s {
    /** Energy for ion pump operation (J) */
    double ion_pumping;

    /** Energy for synaptic processes (J) */
    double synaptic;

    /** Energy for housekeeping/maintenance (J) */
    double housekeeping;

    /** Energy for computation/signaling (J) */
    double computation;

    /** Waste heat / dissipation (J) */
    double waste_heat;

    /** Total energy in budget (J) */
    double total;

    /** Time period for this budget (s) */
    double time_period;

} nimcp_energy_budget_t;

//=============================================================================
// Landauer Cost Structure
//=============================================================================

/**
 * @brief Landauer cost tracking for information erasure
 *
 * BIOLOGICAL: Landauer's principle sets the minimum energy for bit erasure:
 * E_min = kT * ln(2). Real neural operations are far less efficient,
 * but this provides a fundamental lower bound.
 */
typedef struct nimcp_landauer_cost_s {
    /** Number of bits erased */
    uint64_t bits_erased;

    /** Minimum theoretical cost (J) - Landauer limit */
    double minimum_cost;

    /** Actual energy consumed (J) */
    double actual_cost;

    /** Efficiency ratio: min/actual [0, 1] */
    double efficiency;

    /** Temperature at which cost was computed (K) */
    double temperature_k;

} nimcp_landauer_cost_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default thermodynamic configuration
 *
 * WHAT: Returns configuration with sensible defaults
 * WHY:  Simplifies initialization for common use cases
 * HOW:  Sets body temperature, typical ATP pool, etc.
 *
 * @return Default configuration structure
 */
NIMCP_EXPORT nimcp_thermo_config_t nimcp_thermo_default_config(void);

/**
 * @brief Initialize thermodynamic state
 *
 * WHAT: Initializes thermodynamic tracking for a module
 * WHY:  Set up energy/entropy accounting before simulation
 * HOW:  Zero counters, set initial ATP pool, configure parameters
 *
 * @param state State structure to initialize
 * @param config Configuration (NULL for defaults)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
NIMCP_EXPORT nimcp_error_t nimcp_thermo_init(
    nimcp_thermodynamic_state_t* state,
    const nimcp_thermo_config_t* config
);

/**
 * @brief Reset thermodynamic state to initial conditions
 *
 * @param state State to reset
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_thermo_reset(
    nimcp_thermodynamic_state_t* state
);

/**
 * @brief Destroy thermodynamic state
 *
 * @param state State to destroy (NULL-safe)
 */
NIMCP_EXPORT void nimcp_thermo_destroy(nimcp_thermodynamic_state_t* state);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update thermodynamic state for a time step
 *
 * WHAT: Advances thermodynamic simulation by dt seconds
 * WHY:  Track cumulative energy consumption and entropy production
 * HOW:  Integrates power consumption, updates ATP, computes entropy
 *
 * @param state Thermodynamic state to update
 * @param dt Time step (seconds)
 * @param power_consumed Power consumed this step (W)
 * @param bits_erased Bits erased this step (for Landauer tracking)
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_thermo_update(
    nimcp_thermodynamic_state_t* state,
    double dt,
    double power_consumed,
    uint64_t bits_erased
);

/**
 * @brief Record energy consumption by category
 *
 * WHAT: Records energy spent on specific process type
 * WHY:  Enables detailed energy budgeting
 * HOW:  Updates state counters for specific energy category
 *
 * @param state Thermodynamic state
 * @param ion_pump Energy for ion pumping (J)
 * @param synaptic Energy for synaptic processes (J)
 * @param housekeeping Energy for housekeeping (J)
 * @param computation Energy for computation (J)
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_thermo_record_energy(
    nimcp_thermodynamic_state_t* state,
    double ion_pump,
    double synaptic,
    double housekeeping,
    double computation
);

/**
 * @brief Replenish ATP pool
 *
 * WHAT: Adds ATP to the available pool
 * WHY:  Model metabolic ATP production
 * HOW:  Increases atp_available up to maximum capacity
 *
 * @param state Thermodynamic state
 * @param atp_moles ATP to add (moles)
 * @param max_capacity Maximum pool capacity (moles), 0 for no limit
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_thermo_replenish_atp(
    nimcp_thermodynamic_state_t* state,
    double atp_moles,
    double max_capacity
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current energy budget breakdown
 *
 * WHAT: Returns detailed energy budget by category
 * WHY:  Analyze where energy is being consumed
 * HOW:  Copies internal tracking to output structure
 *
 * @param state Thermodynamic state
 * @param budget Output energy budget structure
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_thermo_get_energy_budget(
    const nimcp_thermodynamic_state_t* state,
    nimcp_energy_budget_t* budget
);

/**
 * @brief Compute current entropy production rate
 *
 * WHAT: Calculates dS/dt from current state
 * WHY:  Monitor thermodynamic efficiency
 * HOW:  dS/dt = Q/T + sigma_irr (heat flow + irreversible processes)
 *
 * BIOLOGICAL: Neural systems produce entropy through:
 * - Heat dissipation to surroundings (Q/T term)
 * - Irreversible chemical reactions (sigma_irr term)
 * - Information processing (computational dissipation)
 *
 * @param state Thermodynamic state
 * @param temperature_k System temperature (K)
 * @param entropy_rate_out Output: entropy production rate (W/K)
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_thermo_compute_entropy_rate(
    const nimcp_thermodynamic_state_t* state,
    double temperature_k,
    double* entropy_rate_out
);

/**
 * @brief Compute Landauer cost for bit erasure
 *
 * WHAT: Calculates minimum thermodynamic cost of erasing bits
 * WHY:  Provides fundamental lower bound on computation energy
 * HOW:  E_min = n_bits * kT * ln(2)
 *
 * BIOLOGICAL: Landauer's principle (1961) proves that erasing one bit
 * requires at least kT*ln(2) energy dissipation. At body temperature
 * (310K), this is ~2.97 zeptojoules per bit.
 *
 * @param temperature_k Temperature (Kelvin)
 * @param num_bits Number of bits to erase
 * @param cost_out Output: Landauer cost structure
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_thermo_compute_landauer_cost(
    double temperature_k,
    uint64_t num_bits,
    nimcp_landauer_cost_t* cost_out
);

/**
 * @brief Get thermodynamic efficiency metrics
 *
 * WHAT: Returns efficiency metrics for the system
 * WHY:  Evaluate how close to theoretical limits
 * HOW:  Compares actual energy use to theoretical minimum
 *
 * @param state Thermodynamic state
 * @param computational_eff Output: computational efficiency [0,1]
 * @param thermo_eff Output: thermodynamic efficiency [0,1]
 * @param landauer_eff Output: Landauer efficiency [0,1]
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_thermo_get_efficiency(
    const nimcp_thermodynamic_state_t* state,
    double* computational_eff,
    double* thermo_eff,
    double* landauer_eff
);

/**
 * @brief Check if ATP is critically low
 *
 * @param state Thermodynamic state
 * @param threshold Critical threshold (fraction of initial pool)
 * @return true if ATP below threshold
 */
NIMCP_EXPORT bool nimcp_thermo_is_atp_critical(
    const nimcp_thermodynamic_state_t* state,
    double threshold
);

/**
 * @brief Get ATP availability ratio
 *
 * @param state Thermodynamic state
 * @return Current ATP as fraction of maximum [0, 1]
 */
NIMCP_EXPORT double nimcp_thermo_get_atp_ratio(
    const nimcp_thermodynamic_state_t* state
);

//=============================================================================
// Computation Cost API
//=============================================================================

/**
 * @brief Estimate energy cost of neural operation
 *
 * WHAT: Estimates energy for common neural operations
 * WHY:  Pre-computation energy budgeting
 * HOW:  Uses empirical models of neural energy costs
 *
 * @param num_synapses Number of synaptic operations
 * @param num_spikes Number of action potentials
 * @param temperature_k Temperature (K)
 * @param energy_out Output: estimated energy (J)
 * @return NIMCP_SUCCESS on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_thermo_estimate_operation_cost(
    uint64_t num_synapses,
    uint64_t num_spikes,
    double temperature_k,
    double* energy_out
);

/**
 * @brief Convert ATP consumption to energy
 *
 * @param atp_moles ATP consumed (moles)
 * @return Energy in Joules
 */
NIMCP_EXPORT double nimcp_thermo_atp_to_energy(double atp_moles);

/**
 * @brief Convert energy to ATP requirement
 *
 * @param energy_j Energy requirement (J)
 * @return ATP required (moles)
 */
NIMCP_EXPORT double nimcp_thermo_energy_to_atp(double energy_j);

//=============================================================================
// Wave W15: Runtime aggregated dissipation / entropy summary events
//
// Emit only on ATP-critical transitions, entropy spikes, or every N>=1000
// update calls. Never per-update.
//=============================================================================

#ifndef NIMCP_BRAIN_T_DEFINED
#define NIMCP_BRAIN_T_DEFINED
typedef struct brain_struct* brain_t;
#endif

struct brain_kg;

/**
 * @brief Register brain handle for thermodynamic runtime KG emission.
 */
NIMCP_EXPORT void nimcp_thermo_kg_register_brain(brain_t brain);

/**
 * @brief Emit an aggregated thermodynamic dissipation/entropy event.
 *
 * @param brain                     Brain handle (falls back to registered)
 * @param kind                      "atp_critical", "entropy_spike",
 *                                  "dissipation_summary", "landauer_eff_drop"
 * @param entropy_rate              Recent dS/dt (W/K)
 * @param atp_ratio                 ATP ratio [0,1]
 * @param power_consumption_w       Power (W) since last emit
 * @param ts_us                     Timestamp (microseconds)
 */
NIMCP_EXPORT void nimcp_thermo_kg_trigger_dissipation_event(
    brain_t brain,
    const char* kind,
    double entropy_rate,
    double atp_ratio,
    double power_consumption_w,
    uint64_t ts_us
);

/**
 * @brief Read-path: count thermodynamic events.
 */
NIMCP_EXPORT uint32_t nimcp_thermo_kg_count_events(
    struct brain_kg* kg,
    const char* kind_substr
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THERMODYNAMICS_H */
