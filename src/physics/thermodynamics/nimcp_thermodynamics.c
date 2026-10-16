#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_thermodynamics.c - Non-Equilibrium Thermodynamics Implementation
//=============================================================================
/**
 * @file nimcp_thermodynamics.c
 * @brief Implementation of non-equilibrium thermodynamics module (AC-9)
 *
 * BIOLOGICAL: This module implements the thermodynamic constraints that
 * govern neural computation. Key principles:
 *
 * 1. Second Law: Total entropy always increases, dS/dt >= 0
 * 2. Landauer's Principle: Bit erasure costs at least kT*ln(2)
 * 3. ATP Economy: Neural operations consume ~10^8 ATP/neuron/second
 * 4. Efficiency: Brain operates at ~20% thermodynamic efficiency
 *
 * WHAT: Implements energy budgeting, entropy production, and efficiency metrics
 * WHY:  Enables biologically realistic energy-aware neural computation
 * HOW:  Tracks state variables and applies thermodynamic equations
 */

#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/memory/nimcp_memory_guards.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(thermodynamics)

//=============================================================================
// Internal Constants
//=============================================================================

/** Natural log of 2 */
#define LN_2 0.693147180559945309417

/** Energy per synaptic event (J) - approximately 1.6e-16 J */
#define ENERGY_PER_SYNAPSE 1.6e-16

/** Energy per action potential (J) - approximately 1.0e-13 J */
#define ENERGY_PER_SPIKE 1.0e-13

/** ATP molecules per synaptic event */
#define ATP_PER_SYNAPSE 1.2e4

/** ATP molecules per action potential */
#define ATP_PER_SPIKE 1.0e9

/** Minimum valid temperature (K) - must be above absolute zero */
#define MIN_VALID_TEMP_K 0.001

/** Maximum valid temperature (K) - upper bound for numerical stability */
#define MAX_VALID_TEMP_K 1000000.0

//=============================================================================
// Internal State
//=============================================================================

/**
 * @brief Internal extended state for detailed tracking
 */
typedef struct nimcp_thermo_internal_s {
    /** Configuration copy */
    nimcp_thermo_config_t config;

    /** Initial ATP pool for ratio calculations */
    double initial_atp_pool;

    /** Energy budget breakdown */
    nimcp_energy_budget_t budget;

    /** Cumulative Landauer tracking */
    nimcp_landauer_cost_t landauer_cumulative;

    /** Maximum ATP capacity */
    double max_atp_capacity;

} nimcp_thermo_internal_t;

/**
 * Shared internal state storage (NOT thread-local).
 *
 * DESIGN FIX: Previously used __thread, which caused state initialized on one
 * thread to be invisible from other threads. Since thermodynamic states are
 * shared resources (e.g., attached to brain subsystems accessed by multiple
 * threads), the storage must be global with mutex protection.
 */
static nimcp_thermo_internal_t* s_internal_states[256] = {0};
static uint32_t s_state_count = 0;
static nimcp_mutex_t* s_thermo_mutex = NULL;
static bool s_thermo_mutex_initialized = false;

/**
 * @brief Ensure the thermodynamics module mutex is initialized
 *
 * WHAT: Lazily initialize the global mutex for state array access
 * WHY:  Protect shared state from concurrent access after TLS removal
 * HOW:  One-time initialization with double-check pattern
 */
static void ensure_thermo_mutex(void) {
    if (s_thermo_mutex_initialized) return;
    s_thermo_mutex = nimcp_calloc(1, sizeof(nimcp_mutex_t));
    if (s_thermo_mutex) {
        nimcp_mutex_init(s_thermo_mutex, NULL);
        s_thermo_mutex_initialized = true;
    }
}

static inline void thermo_lock(void) {
    ensure_thermo_mutex();
    if (s_thermo_mutex) nimcp_mutex_lock(s_thermo_mutex);
}

static inline void thermo_unlock(void) {
    if (s_thermo_mutex) nimcp_mutex_unlock(s_thermo_mutex);
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get or create internal state for a thermodynamic state
 *
 * WHAT: Associates extended internal state with public state
 * WHY:  Store additional tracking data without exposing internals
 * HOW:  Uses module_id as index into thread-local storage
 */
static nimcp_thermo_internal_t* get_internal_state(
    const nimcp_thermodynamic_state_t* state
) {
    if (!state || state->module_id >= 256) {
        return NULL;
    }
    thermo_lock();
    nimcp_thermo_internal_t* result = s_internal_states[state->module_id];
    thermo_unlock();
    return result;
}

/**
 * @brief Set internal state for a thermodynamic state
 */
static void set_internal_state(
    uint32_t module_id,
    nimcp_thermo_internal_t* internal
) {
    if (module_id < 256) {
        thermo_lock();
        s_internal_states[module_id] = internal;
        thermo_unlock();
    }
}

/**
 * @brief Compute Landauer energy for given bits at temperature
 *
 * WHAT: Calculates minimum erasure energy
 * WHY:  Landauer's principle: E_min = n * kT * ln(2)
 * HOW:  Direct application of Landauer formula
 *
 * @param temp_k Temperature (Kelvin)
 * @param num_bits Number of bits
 * @return Minimum energy (Joules)
 */
static double compute_landauer_energy(double temp_k, uint64_t num_bits) {
    if (num_bits == 0) return 0.0;
    return (double)num_bits * NIMCP_THERMO_KB * temp_k * LN_2;
}

/**
 * @brief Clamp value to range
 */
static inline double clamp_double(double value, double min_val, double max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

//=============================================================================
// Lifecycle Implementation
//=============================================================================

nimcp_thermo_config_t nimcp_thermo_default_config(void) {
    /*
     * WHAT: Creates default configuration
     * WHY:  Provides sensible defaults for typical neural simulation
     * HOW:  Sets body temperature, typical ATP pool, etc.
     */
    nimcp_thermo_config_t config;
    memset(&config, 0, sizeof(config));

    config.temperature_k = NIMCP_THERMO_DEFAULT_TEMP_K;
    config.atp_pool_size = NIMCP_THERMO_DEFAULT_ATP_POOL;
    config.metabolic_rate = NIMCP_THERMO_DEFAULT_METABOLIC_RATE;
    config.enable_landauer_cost = true;
    config.enable_entropy_tracking = true;
    config.enable_atp_warnings = true;
    config.atp_warning_threshold = 0.1;  /* Warn at 10% ATP */
    config.max_power_budget = 0.0;       /* No limit */
    config.heat_dissipation_coeff = 1.0e-9;  /* W/K */
    config.external_temp_k = 298.0;      /* Room temperature */

    thermo_lock();
    config.module_id = s_state_count;
    thermo_unlock();

    return config;
}

nimcp_error_t nimcp_thermo_init(
    nimcp_thermodynamic_state_t* state,
    const nimcp_thermo_config_t* config
) {
    /*
     * WHAT: Initializes thermodynamic state structure
     * WHY:  Set up tracking before simulation begins
     * HOW:  Zero state, apply config, allocate internal storage
     *
     * BIOLOGICAL: Initializes the "energy metabolism" of a computational
     * module, analogous to establishing baseline metabolic rate.
     */

    /* Guard: validate input */
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL in nimcp_thermo_init");

    /* Initialize with defaults if no config provided */
    nimcp_thermo_config_t effective_config;
    if (config) {
        effective_config = *config;
    } else {
        effective_config = nimcp_thermo_default_config();
    }

    /* Validate temperature range */
    NIMCP_CHECK_THROW(effective_config.temperature_k >= MIN_VALID_TEMP_K &&
                      effective_config.temperature_k <= MAX_VALID_TEMP_K,
                      NIMCP_ERROR_OUT_OF_RANGE, "Temperature %.2f K out of valid range [%.0f, %.0f]",
                      effective_config.temperature_k, MIN_VALID_TEMP_K, MAX_VALID_TEMP_K);

    /* Clear state */
    memset(state, 0, sizeof(*state));

    /* Set initial values */
    state->atp_available = effective_config.atp_pool_size;
    state->module_id = effective_config.module_id;

    /* Set initial efficiency (will be updated during simulation) */
    state->computational_efficiency = 1.0;
    state->thermodynamic_efficiency = 1.0;
    state->landauer_efficiency = 0.0;  /* Unknown until bits erased */
    state->energy_per_bit = 0.0;

    /* Allocate internal state */
    nimcp_thermo_internal_t* internal = nimcp_calloc(1, sizeof(nimcp_thermo_internal_t));
    if (!internal) {
        LOG_ERROR("Failed to allocate thermodynamic internal state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate thermo internal state");
        return NIMCP_ERROR_NO_MEMORY;
    }

    internal->config = effective_config;
    internal->initial_atp_pool = effective_config.atp_pool_size;
    internal->max_atp_capacity = effective_config.atp_pool_size * 2.0;  /* Allow some accumulation */
    memset(&internal->budget, 0, sizeof(internal->budget));
    memset(&internal->landauer_cumulative, 0, sizeof(internal->landauer_cumulative));

    set_internal_state(state->module_id, internal);

    thermo_lock();
    s_state_count++;
    thermo_unlock();

    state->initialized = true;
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_thermo_reset(nimcp_thermodynamic_state_t* state) {
    /*
     * WHAT: Resets state to initial conditions
     * WHY:  Allow reuse without full reinitialization
     * HOW:  Zero counters, restore initial ATP
     */

    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL in nimcp_thermo_reset");
    NIMCP_CHECK_THROW(state->initialized, NIMCP_ERROR_NOT_INITIALIZED, "state not initialized in nimcp_thermo_reset");

    nimcp_thermo_internal_t* internal = get_internal_state(state);
    NIMCP_CHECK_THROW(internal, NIMCP_ERROR_INVALID_STATE, "internal state is NULL in nimcp_thermo_reset");

    /* Reset cumulative values */
    state->total_energy_consumed = 0.0;
    state->total_entropy_produced = 0.0;
    state->free_energy_dissipated = 0.0;
    state->simulation_time = 0.0;

    /* Reset current rates to zero */
    state->power_consumption = 0.0;
    state->heat_dissipation = 0.0;
    state->entropy_production_rate = 0.0;
    state->atp_consumption_rate = 0.0;

    /* Restore initial ATP */
    state->atp_available = internal->initial_atp_pool;

    /* Reset efficiencies */
    state->computational_efficiency = 1.0;
    state->thermodynamic_efficiency = 1.0;
    state->landauer_efficiency = 0.0;
    state->energy_per_bit = 0.0;

    /* Reset internal tracking */
    memset(&internal->budget, 0, sizeof(internal->budget));
    memset(&internal->landauer_cumulative, 0, sizeof(internal->landauer_cumulative));

    return NIMCP_SUCCESS;
}

void nimcp_thermo_destroy(nimcp_thermodynamic_state_t* state) {
    /*
     * WHAT: Cleans up thermodynamic state
     * WHY:  Free internal resources
     * HOW:  Free internal state, clear structure
     */

    if (!state) {
        return;
    }

    if (state->initialized) {
        nimcp_thermo_internal_t* internal = get_internal_state(state);
        if (internal) {
            set_internal_state(state->module_id, NULL);
            nimcp_free(internal);
        }
    }

    memset(state, 0, sizeof(*state));
}

//=============================================================================
// Update Implementation
//=============================================================================

nimcp_error_t nimcp_thermo_update(
    nimcp_thermodynamic_state_t* state,
    double dt,
    double power_consumed,
    uint64_t bits_erased
) {
    /*
     * WHAT: Advances thermodynamic simulation by time step
     * WHY:  Track energy consumption and entropy production over time
     * HOW:  Integrates power, updates ATP, computes entropy production
     *
     * BIOLOGICAL: Each time step represents ongoing metabolic processes:
     * - ATP hydrolysis for neural operations
     * - Heat dissipation to maintain temperature
     * - Entropy production from irreversible processes
     */

    /* Guard: validate inputs */
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL in nimcp_thermo_update");
    NIMCP_CHECK_THROW(state->initialized, NIMCP_ERROR_NOT_INITIALIZED, "state not initialized in nimcp_thermo_update");
    NIMCP_CHECK_THROW(dt > 0.0, NIMCP_ERROR_INVALID_PARAMETER, "dt must be positive, got %.6f", dt);

    nimcp_thermo_internal_t* internal = get_internal_state(state);
    NIMCP_CHECK_THROW(internal, NIMCP_ERROR_INVALID_STATE, "internal state is NULL in nimcp_thermo_update");

    /* Clamp power to reasonable range */
    power_consumed = clamp_double(power_consumed, 0.0, 1e6);

    /* Update power consumption rate */
    state->power_consumption = power_consumed;

    /* Compute energy consumed this step */
    double energy_this_step = power_consumed * dt;
    state->total_energy_consumed += energy_this_step;

    /* Update simulation time */
    state->simulation_time += dt;

    /* Compute ATP consumption
     * ATP_hydrolysis_energy ~ 50 kJ/mol = 8.3e-20 J/molecule
     * energy / ATP_energy = moles_ATP
     */
    double atp_consumed = energy_this_step / (NIMCP_THERMO_ATP_ENERGY * NIMCP_THERMO_AVOGADRO);
    state->atp_consumption_rate = atp_consumed / dt;
    state->atp_available -= atp_consumed;

    /* Clamp ATP to non-negative */
    if (state->atp_available < 0.0) {
        state->atp_available = 0.0;
    }

    /* Compute heat dissipation
     * Heat flow: Q = k * (T_system - T_external)
     */
    double temp_diff = internal->config.temperature_k - internal->config.external_temp_k;
    state->heat_dissipation = internal->config.heat_dissipation_coeff * temp_diff;

    /* For now, assume all consumed power becomes heat eventually */
    double heat_this_step = power_consumed;

    /* Compute entropy production rate
     * dS/dt = Q/T (heat flow entropy) + sigma_irr (irreversible processes)
     *
     * BIOLOGICAL: Entropy production has two components:
     * 1. Heat dissipation: dS_heat/dt = Q / T_system
     * 2. Irreversible chemical processes (ATP hydrolysis, ion transport)
     */
    double temperature = internal->config.temperature_k;
    if (temperature < MIN_VALID_TEMP_K) {
        temperature = MIN_VALID_TEMP_K;
    }
    double entropy_heat = heat_this_step / temperature;

    /* Irreversible entropy from chemical processes
     * Estimated as fraction of energy dissipated irreversibly
     */
    double irreversible_fraction = 0.3;  /* ~30% from irreversible processes */
    double entropy_irrev = (energy_this_step * irreversible_fraction) / temperature;

    state->entropy_production_rate = (entropy_heat + entropy_irrev) / dt;
    state->total_entropy_produced += (entropy_heat + entropy_irrev);

    /* Update free energy dissipation */
    state->free_energy_dissipated += energy_this_step;

    /* Landauer cost tracking */
    if (internal->config.enable_landauer_cost && bits_erased > 0) {
        double landauer_min = compute_landauer_energy(temperature, bits_erased);
        internal->landauer_cumulative.bits_erased += bits_erased;
        internal->landauer_cumulative.minimum_cost += landauer_min;
        internal->landauer_cumulative.actual_cost += energy_this_step;
        internal->landauer_cumulative.temperature_k = temperature;

        /* Update efficiency metrics */
        if (internal->landauer_cumulative.actual_cost > 0) {
            internal->landauer_cumulative.efficiency =
                internal->landauer_cumulative.minimum_cost /
                internal->landauer_cumulative.actual_cost;
            state->landauer_efficiency = internal->landauer_cumulative.efficiency;
        }

        /* Energy per bit */
        if (bits_erased > 0) {
            state->energy_per_bit = energy_this_step / (double)bits_erased;
        }
    }

    /* Update efficiency metrics */
    /* Computational efficiency: useful work / total energy
     * Estimate useful work as energy minus waste heat
     */
    double useful_work = energy_this_step * (1.0 - irreversible_fraction);
    if (energy_this_step > 0) {
        state->computational_efficiency = useful_work / energy_this_step;
    }

    /* Thermodynamic efficiency relative to Carnot limit
     * eta_carnot = 1 - T_cold / T_hot
     * For neural systems, this is limited by small temperature gradients
     */
    double t_hot = internal->config.temperature_k;
    double t_cold = internal->config.external_temp_k;
    double carnot_limit = (t_hot > t_cold) ? (1.0 - t_cold / t_hot) : 0.01;
    state->thermodynamic_efficiency = state->computational_efficiency / carnot_limit;
    state->thermodynamic_efficiency = clamp_double(state->thermodynamic_efficiency, 0.0, 1.0);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_thermo_record_energy(
    nimcp_thermodynamic_state_t* state,
    double ion_pump,
    double synaptic,
    double housekeeping,
    double computation
) {
    /*
     * WHAT: Records energy consumption by category
     * WHY:  Enables detailed energy budget analysis
     * HOW:  Updates internal budget tracking
     *
     * BIOLOGICAL: Neural energy budget breakdown:
     * - Ion pumping (Na+/K+-ATPase): ~50% of total
     * - Synaptic transmission: ~25%
     * - Housekeeping (protein synthesis, etc.): ~25%
     */

    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL in nimcp_thermo_record_energy");
    NIMCP_CHECK_THROW(state->initialized, NIMCP_ERROR_NOT_INITIALIZED, "state not initialized in nimcp_thermo_record_energy");

    nimcp_thermo_internal_t* internal = get_internal_state(state);
    NIMCP_CHECK_THROW(internal, NIMCP_ERROR_INVALID_STATE, "internal state is NULL in nimcp_thermo_record_energy");

    /* Update budget categories */
    internal->budget.ion_pumping += ion_pump;
    internal->budget.synaptic += synaptic;
    internal->budget.housekeeping += housekeeping;
    internal->budget.computation += computation;

    double total_recorded = ion_pump + synaptic + housekeeping + computation;
    internal->budget.total += total_recorded;

    /* Waste heat is energy not accounted for in categories */
    double useful_energy = total_recorded * 0.7;  /* ~30% waste */
    internal->budget.waste_heat += (total_recorded - useful_energy);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_thermo_replenish_atp(
    nimcp_thermodynamic_state_t* state,
    double atp_moles,
    double max_capacity
) {
    /*
     * WHAT: Adds ATP to available pool
     * WHY:  Model metabolic ATP production (glycolysis, oxidative phosphorylation)
     * HOW:  Increases atp_available with capacity limit
     *
     * BIOLOGICAL: ATP is continuously regenerated by:
     * - Glycolysis (anaerobic, fast, ~2 ATP/glucose)
     * - Oxidative phosphorylation (aerobic, ~36 ATP/glucose)
     */

    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL in nimcp_thermo_replenish_atp");
    NIMCP_CHECK_THROW(state->initialized, NIMCP_ERROR_NOT_INITIALIZED, "state not initialized in nimcp_thermo_replenish_atp");
    NIMCP_CHECK_THROW(atp_moles >= 0.0, NIMCP_ERROR_INVALID_PARAMETER, "atp_moles must be non-negative, got %.6f", atp_moles);

    nimcp_thermo_internal_t* internal = get_internal_state(state);
    NIMCP_CHECK_THROW(internal, NIMCP_ERROR_INVALID_STATE, "internal state is NULL in nimcp_thermo_replenish_atp");

    /* Determine effective capacity */
    double effective_capacity = max_capacity;
    if (effective_capacity <= 0.0) {
        effective_capacity = internal->max_atp_capacity;
    }

    /* Add ATP with capacity limit */
    state->atp_available += atp_moles;
    if (state->atp_available > effective_capacity) {
        state->atp_available = effective_capacity;
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Query Implementation
//=============================================================================

nimcp_error_t nimcp_thermo_get_energy_budget(
    const nimcp_thermodynamic_state_t* state,
    nimcp_energy_budget_t* budget
) {
    /*
     * WHAT: Returns detailed energy budget
     * WHY:  Analyze energy consumption patterns
     * HOW:  Copies internal tracking to output
     */

    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL in nimcp_thermo_get_energy_budget");
    NIMCP_CHECK_THROW(budget, NIMCP_ERROR_NULL_POINTER, "budget is NULL in nimcp_thermo_get_energy_budget");
    NIMCP_CHECK_THROW(state->initialized, NIMCP_ERROR_NOT_INITIALIZED, "state not initialized in nimcp_thermo_get_energy_budget");

    nimcp_thermo_internal_t* internal = get_internal_state(state);
    NIMCP_CHECK_THROW(internal, NIMCP_ERROR_INVALID_STATE, "internal state is NULL in nimcp_thermo_get_energy_budget");

    *budget = internal->budget;
    budget->time_period = state->simulation_time;

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_thermo_compute_entropy_rate(
    const nimcp_thermodynamic_state_t* state,
    double temperature_k,
    double* entropy_rate_out
) {
    /*
     * WHAT: Computes current entropy production rate
     * WHY:  Monitor thermodynamic state and efficiency
     * HOW:  dS/dt = Q/T + sigma_irreversible
     *
     * BIOLOGICAL: Entropy production is a measure of how far the system
     * operates from thermodynamic equilibrium. Living systems maintain
     * low internal entropy by exporting entropy to their environment.
     *
     * The entropy production rate has two components:
     * 1. Heat flow: dS_heat/dt = Q/T (Clausius inequality)
     * 2. Irreversible processes: sigma_irr >= 0 (Second Law)
     */

    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL in nimcp_thermo_compute_entropy_rate");
    NIMCP_CHECK_THROW(entropy_rate_out, NIMCP_ERROR_NULL_POINTER, "entropy_rate_out is NULL in nimcp_thermo_compute_entropy_rate");
    NIMCP_CHECK_THROW(state->initialized, NIMCP_ERROR_NOT_INITIALIZED, "state not initialized in nimcp_thermo_compute_entropy_rate");
    NIMCP_CHECK_THROW(temperature_k >= MIN_VALID_TEMP_K && temperature_k <= MAX_VALID_TEMP_K,
                      NIMCP_ERROR_OUT_OF_RANGE, "Temperature %.2f K out of valid range [%.0f, %.0f]",
                      temperature_k, MIN_VALID_TEMP_K, MAX_VALID_TEMP_K);

    /* Heat flow contribution: Q/T */
    double entropy_heat = state->heat_dissipation / temperature_k;

    /* Irreversible contribution from chemical processes
     * Approximated as fraction of power consumption
     */
    double irreversible_factor = 0.3;
    double entropy_irrev = (state->power_consumption * irreversible_factor) / temperature_k;

    *entropy_rate_out = entropy_heat + entropy_irrev;

    /* Clamp to reasonable range */
    if (*entropy_rate_out > NIMCP_THERMO_MAX_ENTROPY_RATE) {
        *entropy_rate_out = NIMCP_THERMO_MAX_ENTROPY_RATE;
    }
    if (*entropy_rate_out < 0.0) {
        *entropy_rate_out = 0.0;
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_thermo_compute_landauer_cost(
    double temperature_k,
    uint64_t num_bits,
    nimcp_landauer_cost_t* cost_out
) {
    /*
     * WHAT: Computes minimum energy for bit erasure
     * WHY:  Landauer's principle sets fundamental computation limit
     * HOW:  E_min = n * k_B * T * ln(2)
     *
     * BIOLOGICAL: Landauer's principle (1961) proves that erasing
     * information has a minimum thermodynamic cost. This is because
     * information has physical reality - reducing entropy of a system
     * requires energy dissipation.
     *
     * At body temperature (310K):
     * E_min = 1.38e-23 * 310 * 0.693 = 2.97e-21 J/bit = 2.97 zJ/bit
     */

    NIMCP_CHECK_THROW(cost_out, NIMCP_ERROR_NULL_POINTER, "cost_out is NULL in nimcp_thermo_compute_landauer_cost");
    NIMCP_CHECK_THROW(temperature_k >= MIN_VALID_TEMP_K && temperature_k <= MAX_VALID_TEMP_K,
                      NIMCP_ERROR_OUT_OF_RANGE, "Temperature %.2f K out of valid range [%.0f, %.0f]",
                      temperature_k, MIN_VALID_TEMP_K, MAX_VALID_TEMP_K);

    memset(cost_out, 0, sizeof(*cost_out));

    cost_out->bits_erased = num_bits;
    cost_out->temperature_k = temperature_k;
    cost_out->minimum_cost = compute_landauer_energy(temperature_k, num_bits);
    cost_out->actual_cost = 0.0;  /* Must be set by caller */
    cost_out->efficiency = 0.0;   /* Computed when actual_cost is known */

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_thermo_get_efficiency(
    const nimcp_thermodynamic_state_t* state,
    double* computational_eff,
    double* thermo_eff,
    double* landauer_eff
) {
    /*
     * WHAT: Returns efficiency metrics
     * WHY:  Evaluate system performance against limits
     * HOW:  Returns pre-computed efficiencies from state
     */

    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL in nimcp_thermo_get_efficiency");
    NIMCP_CHECK_THROW(state->initialized, NIMCP_ERROR_NOT_INITIALIZED, "state not initialized in nimcp_thermo_get_efficiency");

    if (computational_eff) {
        *computational_eff = state->computational_efficiency;
    }

    if (thermo_eff) {
        *thermo_eff = state->thermodynamic_efficiency;
    }

    if (landauer_eff) {
        *landauer_eff = state->landauer_efficiency;
    }

    return NIMCP_SUCCESS;
}

bool nimcp_thermo_is_atp_critical(
    const nimcp_thermodynamic_state_t* state,
    double threshold
) {
    /*
     * WHAT: Checks if ATP pool is critically low
     * WHY:  Enable metabolic emergency responses
     * HOW:  Compares current ATP to threshold fraction
     */

    if (!state || !state->initialized) {
        return true;  /* Fail-safe: assume critical if invalid */
    }

    nimcp_thermo_internal_t* internal = get_internal_state(state);
    if (!internal || internal->initial_atp_pool <= 0.0) {
        return true;
    }

    double ratio = state->atp_available / internal->initial_atp_pool;
    return ratio < threshold;
}

double nimcp_thermo_get_atp_ratio(const nimcp_thermodynamic_state_t* state) {
    /*
     * WHAT: Returns current ATP as fraction of initial pool
     * WHY:  Monitor metabolic state
     * HOW:  Simple ratio calculation
     */

    if (!state || !state->initialized) {
        return 0.0;
    }

    nimcp_thermo_internal_t* internal = get_internal_state(state);
    if (!internal || internal->initial_atp_pool <= 0.0) {
        return 0.0;
    }

    double ratio = state->atp_available / internal->initial_atp_pool;
    return clamp_double(ratio, 0.0, 2.0);  /* Allow up to 2x accumulation */
}

//=============================================================================
// Computation Cost Implementation
//=============================================================================

nimcp_error_t nimcp_thermo_estimate_operation_cost(
    uint64_t num_synapses,
    uint64_t num_spikes,
    double temperature_k,
    double* energy_out
) {
    /*
     * WHAT: Estimates energy for neural operations
     * WHY:  Pre-computation energy budgeting
     * HOW:  Uses empirical energy costs per operation
     *
     * BIOLOGICAL: Empirical estimates from neuroscience:
     * - Synaptic event: ~1.6e-16 J (vesicle release, receptor binding)
     * - Action potential: ~1.0e-13 J (ion flux, pump restoration)
     *
     * These costs scale with temperature due to Q10 effects on
     * ion channels and enzymatic reactions.
     */

    NIMCP_CHECK_THROW(energy_out, NIMCP_ERROR_NULL_POINTER, "energy_out is NULL in nimcp_thermo_estimate_operation_cost");
    NIMCP_CHECK_THROW(temperature_k >= MIN_VALID_TEMP_K && temperature_k <= MAX_VALID_TEMP_K,
                      NIMCP_ERROR_OUT_OF_RANGE, "Temperature %.2f K out of valid range [%.0f, %.0f]",
                      temperature_k, MIN_VALID_TEMP_K, MAX_VALID_TEMP_K);

    /* Temperature scaling (Q10 ~ 2-3 for biological processes) */
    double temp_factor = pow(2.5, (temperature_k - 310.0) / 10.0);

    /* Compute energy contributions */
    double synapse_energy = (double)num_synapses * ENERGY_PER_SYNAPSE * temp_factor;
    double spike_energy = (double)num_spikes * ENERGY_PER_SPIKE * temp_factor;

    *energy_out = synapse_energy + spike_energy;

    return NIMCP_SUCCESS;
}

double nimcp_thermo_atp_to_energy(double atp_moles) {
    /*
     * WHAT: Converts ATP consumption to energy
     * WHY:  Translate between metabolic and thermodynamic units
     * HOW:  E = n_ATP * E_hydrolysis * N_A
     *
     * BIOLOGICAL: ATP hydrolysis releases ~50 kJ/mol under
     * physiological conditions (varies with concentrations).
     */

    if (atp_moles <= 0.0) {
        return 0.0;
    }

    /* E = moles * Avogadro * energy_per_molecule */
    return atp_moles * NIMCP_THERMO_AVOGADRO * NIMCP_THERMO_ATP_ENERGY;
}

double nimcp_thermo_energy_to_atp(double energy_j) {
    /*
     * WHAT: Converts energy requirement to ATP
     * WHY:  Determine metabolic cost of operations
     * HOW:  n_ATP = E / (E_hydrolysis * N_A)
     */

    if (energy_j <= 0.0) {
        return 0.0;
    }

    return energy_j / (NIMCP_THERMO_AVOGADRO * NIMCP_THERMO_ATP_ENERGY);
}

/*=============================================================================
 * W15: Thermodynamic aggregated KG emission + read path
 *===========================================================================*/

#define THERMO_KG_ROOT_NAME "thermodynamics_runtime"

static brain_t s_thermo_kg_brain = NULL;

void nimcp_thermo_kg_register_brain(brain_t brain) {
    s_thermo_kg_brain = brain;
}

void nimcp_thermo_kg_trigger_dissipation_event(brain_t brain,
                                               const char* kind,
                                               double entropy_rate,
                                               double atp_ratio,
                                               double power_consumption_w,
                                               uint64_t ts_us) {
    if (!brain) brain = s_thermo_kg_brain;
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    if (!kind) return;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    brain_kg_node_id_t root = brain_kg_find_node(kg, THERMO_KG_ROOT_NAME);
    if (root == BRAIN_KG_INVALID_NODE) {
        root = brain_kg_add_node(kg, THERMO_KG_ROOT_NAME,
            BRAIN_KG_NODE_CORE,
            "Thermodynamics runtime aggregator - dissipation / entropy summaries");
        brain_kg_node_id_t phys = brain_kg_find_node(kg, "thermodynamics");
        if (root != BRAIN_KG_INVALID_NODE && phys != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, phys, root,
                BRAIN_KG_EDGE_INTEGRATES_WITH,
                "runtime aggregator for thermo subsystem", 1.0f);
        }
    }

    char ev_name[160];
    snprintf(ev_name, sizeof(ev_name),
             "thermodynamics_event_%s_%llu",
             kind, (unsigned long long)ts_us);
    char desc[240];
    snprintf(desc, sizeof(desc),
             "Thermo aggregated event: kind=%s dS/dt=%.4g W/K atp=%.3f power=%.3fW",
             kind, entropy_rate, atp_ratio, power_consumption_w);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, ev_name,
        BRAIN_KG_NODE_CORE, desc);
    if (ev != BRAIN_KG_INVALID_NODE && root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root, ev, BRAIN_KG_EDGE_SENDS_TO,
            "dissipation_event",
            power_consumption_w > 0.0 ? (float)power_consumption_w : 1.0f);
    }
    if (ev != BRAIN_KG_INVALID_NODE) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.6g W/K", entropy_rate);
        brain_kg_add_metadata(kg, ev, "entropy_rate", buf);
        snprintf(buf, sizeof(buf), "%.4f", atp_ratio);
        brain_kg_add_metadata(kg, ev, "atp_ratio", buf);
        snprintf(buf, sizeof(buf), "%.4f W", power_consumption_w);
        brain_kg_add_metadata(kg, ev, "power", buf);
        brain_kg_add_metadata(kg, ev, "kind", kind);
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)ts_us);
        brain_kg_add_metadata(kg, ev, "ts_us", buf);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

uint32_t nimcp_thermo_kg_count_events(struct brain_kg* kg_opaque,
                                      const char* kind_substr) {
    brain_kg_t* kg = (brain_kg_t*)kg_opaque;
    if (!kg) return 0;
    brain_kg_node_id_t root = brain_kg_find_node(kg, THERMO_KG_ROOT_NAME);
    if (root == BRAIN_KG_INVALID_NODE) return 0;
    brain_kg_edge_list_t* out = brain_kg_get_outgoing(kg, root);
    if (!out) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < out->count; ++i) {
        const brain_kg_edge_t* e = out->edges[i];
        if (!e) continue;
        const brain_kg_node_t* to = brain_kg_get_node(kg, e->to);
        if (!to) continue;
        if (!kind_substr || !*kind_substr) {
            count++;
        } else if ((to->name && strstr(to->name, kind_substr)) ||
                   (to->description && strstr(to->description, kind_substr))) {
            count++;
        }
    }
    brain_kg_edge_list_destroy(out);
    return count;
}
