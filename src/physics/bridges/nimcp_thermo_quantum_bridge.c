#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_thermo_quantum_bridge.c - Thermodynamics Quantum Monte Carlo Impl
//=============================================================================

#include "physics/bridges/nimcp_thermo_quantum_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(thermo_quantum_bridge)

#define LOG_MODULE "THERMO_QUANTUM_BRIDGE"


//=============================================================================
// Internal Context Structures
//=============================================================================

/**
 * @brief Context for Landauer optimization energy function
 */
typedef struct {
    nimcp_thermodynamic_state_t* state;
    const thermo_landauer_config_t* config;
    double original_temp;
} landauer_opt_ctx_t;

/**
 * @brief Context for partition function estimation
 */
typedef struct {
    const nimcp_thermodynamic_state_t* state;
    double temperature;
} partition_ctx_t;

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Clamp value to range
 */
static double clamp_d(double v, double min_val, double max_val) {
    if (v < min_val) return min_val;
    if (v > max_val) return max_val;
    return v;
}

/**
 * @brief Compute Landauer limit energy per bit
 */
static double compute_landauer_limit(double temperature_k) {
    // E_min = kT * ln(2)
    return THERMO_QMC_KB * temperature_k * 0.693147f;  // ln(2)
}

/**
 * @brief Energy function for Landauer optimization
 * Minimizes: energy_per_bit while maintaining computation rate
 */
static float landauer_energy_fn(
    const float* params,
    uint32_t dim,
    void* user_data
) {
    landauer_opt_ctx_t* ctx = (landauer_opt_ctx_t*)user_data;
    (void)dim;

    // params[0] = temperature deviation from baseline
    // params[1] = bit rate scaling factor
    double temp = ctx->original_temp + (double)params[0];
    double rate_scale = (double)params[1];

    // Clamp to allowed range
    temp = clamp_d(temp, ctx->config->min_temperature, ctx->config->max_temperature);
    rate_scale = clamp_d(rate_scale, 0.1, 10.0);

    // Compute energy per bit at this temperature
    double landauer = compute_landauer_limit(temp);

    // Estimate actual energy per bit (some multiple of Landauer)
    // Real systems are ~10^6 times less efficient than Landauer
    double efficiency_factor = 1e-3;  // Assume we can approach 0.1% efficiency
    double energy_per_bit = landauer / efficiency_factor;

    // Compute bit rate at this scaling
    double bit_rate = ctx->config->min_computation_rate * rate_scale;

    // Compute total power consumption
    double power = energy_per_bit * bit_rate;

    // Penalty if exceeding heat budget
    double penalty = 0.0;
    if (power > ctx->config->max_heat_dissipation) {
        penalty = (power - ctx->config->max_heat_dissipation) * 1000.0;
    }

    // Penalty if rate below minimum
    if (bit_rate < ctx->config->min_computation_rate) {
        penalty += (ctx->config->min_computation_rate - bit_rate) * 100.0;
    }

    // Objective: minimize energy per bit + penalties
    return (float)(energy_per_bit * 1e18 + penalty);  // Scale to reasonable range
}

/**
 * @brief Energy function for partition function estimation
 */
static float thermo_state_energy(
    const float* state,
    uint32_t dim,
    void* user_data
) {
    partition_ctx_t* ctx = (partition_ctx_t*)user_data;
    (void)dim;
    (void)state;

    // Return current energy consumption as "energy level"
    return (float)ctx->state->total_energy_consumed;
}

//=============================================================================
// Configuration API Implementation
//=============================================================================

int thermo_qmc_partition_default_config(thermo_partition_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(*config));
    config->temperature = THERMO_QMC_DEFAULT_TEMP;
    config->num_samples = THERMO_QMC_DEFAULT_SAMPLES;
    config->burnin = THERMO_QMC_DEFAULT_BURNIN;
    config->thinning = 1;
    config->seed = 0;

    return 0;
}

int thermo_qmc_landauer_default_config(thermo_landauer_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(*config));

    config->initial_temp = 10.0f;
    config->final_temp = 0.01f;
    config->num_iterations = 1000;
    config->quantum_strength = 0.3f;

    config->min_computation_rate = 1e6f;   // 1 Mbps
    config->max_heat_dissipation = 1e-9f;  // 1 nW
    config->atp_budget = 1e-12f;           // 1 pmol

    config->min_temperature = 295.0f;      // 22°C
    config->max_temperature = 315.0f;      // 42°C

    config->seed = 0;

    return 0;
}

int thermo_qmc_atp_default_config(thermo_atp_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(*config));
    config->target_efficiency = 0.3f;      // 30% target efficiency
    config->min_reserve = 0.1f;            // 10% minimum reserve
    config->production_rate = 1e-12f;      // ATP synthesis rate
    config->consumption_rate = 1e-12f;     // ATP consumption rate
    config->num_iterations = 500;
    config->seed = 0;

    return 0;
}

int thermo_qmc_entropy_default_config(thermo_entropy_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(*config));
    config->time_window_ms = 100.0f;
    config->num_samples = 1000;
    config->include_channel_entropy = true;
    config->include_transport_entropy = true;
    config->seed = 0;

    return 0;
}

//=============================================================================
// Partition Function API Implementation
//=============================================================================

int thermo_qmc_estimate_partition(
    const nimcp_thermodynamic_state_t* state,
    const thermo_partition_config_t* config,
    thermo_partition_result_t* result
) {
    if (!state || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_qmc_estimate_partition: required parameter is NULL (state, result)");
        return -1;
    }
    if (!state->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_qmc_estimate_partition: state->initialized is NULL");
        return -1;
    }

    thermo_partition_config_t default_cfg;
    if (!config) {
        thermo_qmc_partition_default_config(&default_cfg);
        config = &default_cfg;
    }

    memset(result, 0, sizeof(*result));

    // Set up context
    partition_ctx_t ctx;
    ctx.state = state;
    ctx.temperature = config->temperature;

    // Initial state for MCMC
    float initial_state[1] = { (float)state->total_energy_consumed };

    // Configure partition estimation
    qmc_partition_config_t qmc_cfg;
    qmc_cfg.num_samples = config->num_samples;
    qmc_cfg.burnin = config->burnin;
    qmc_cfg.thinning = config->thinning;
    qmc_cfg.temperature = config->temperature;
    qmc_cfg.seed = config->seed;

    qmc_partition_result_t qmc_result;
    qmc_result_t err = qmc_estimate_partition(
        thermo_state_energy,
        initial_state,
        1,
        &qmc_cfg,
        &ctx,
        &qmc_result
    );

    if (err != QMC_OK) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "thermo_qmc_estimate_partition: validation failed");
        return -1;
    }

    // Transfer results
    result->log_Z = qmc_result.log_Z;
    result->free_energy = qmc_result.free_energy;
    result->entropy = qmc_result.entropy;
    result->internal_energy = qmc_result.mean_energy;
    result->heat_capacity = qmc_result.heat_capacity;
    result->energy_variance = qmc_result.energy_variance;
    result->std_error = qmc_result.std_error;

    return 0;
}

int thermo_qmc_free_energy_landscape(
    const nimcp_thermodynamic_state_t* state,
    float temp_min,
    float temp_max,
    uint32_t num_points,
    const thermo_partition_config_t* config,
    float* temperatures,
    float* free_energies
) {
    if (!state || !temperatures || !free_energies) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_qmc_free_energy_landscape: required parameter is NULL (state, temperatures, free_energies)");
        return -1;
    }
    if (!state->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "thermo_qmc_free_energy_landscape: state not initialized");
        return -1;
    }
    if (num_points < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "thermo_qmc_free_energy_landscape: num_points must be >= 2");
        return -1;
    }

    thermo_partition_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        thermo_qmc_partition_default_config(&cfg);
    }

    float temp_step = (temp_max - temp_min) / (float)(num_points - 1);

    for (uint32_t i = 0; i < num_points; i++) {
        float temp = temp_min + temp_step * (float)i;
        temperatures[i] = temp;

        cfg.temperature = temp;

        thermo_partition_result_t result;
        int err = thermo_qmc_estimate_partition(state, &cfg, &result);

        if (err == 0) {
            free_energies[i] = result.free_energy;
        } else {
            free_energies[i] = 0.0f;  // Error case
        }
    }

    return 0;
}

//=============================================================================
// Landauer Optimization API Implementation
//=============================================================================

int thermo_qmc_optimize_landauer(
    nimcp_thermodynamic_state_t* state,
    const thermo_landauer_config_t* config,
    thermo_landauer_result_t* result
) {
    if (!state || !config || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_qmc_optimize_landauer: required parameter is NULL (state, config, result)");
        return -1;
    }
    if (!state->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_qmc_optimize_landauer: state->initialized is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    // Set up context
    landauer_opt_ctx_t ctx;
    ctx.state = state;
    ctx.config = config;
    ctx.original_temp = THERMO_QMC_DEFAULT_TEMP;

    // Initial parameters: [temp_offset, rate_scale]
    float initial_params[2] = { 0.0f, 1.0f };

    // Configure annealing
    qmc_anneal_config_t anneal_cfg = qmc_anneal_default_config();
    anneal_cfg.initial_temp = config->initial_temp;
    anneal_cfg.final_temp = config->final_temp;
    anneal_cfg.num_iterations = config->num_iterations;
    anneal_cfg.quantum_strength = config->quantum_strength;
    anneal_cfg.seed = config->seed;

    // Run optimization
    qmc_anneal_result_t anneal_result;
    qmc_result_t err = qmc_adaptive_anneal(
        landauer_energy_fn,
        initial_params,
        2,
        &anneal_cfg,
        &ctx,
        &anneal_result
    );

    if (err != QMC_OK) {
        result->converged = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "thermo_qmc_optimize_landauer: validation failed");
        return -1;
    }

    // Extract results
    double opt_temp_offset = (double)anneal_result.best_state[0];
    double opt_rate_scale = (double)anneal_result.best_state[1];

    result->optimal_temperature = (float)(ctx.original_temp + opt_temp_offset);
    result->optimal_temperature = (float)clamp_d(
        result->optimal_temperature,
        config->min_temperature,
        config->max_temperature
    );

    result->optimal_bit_rate = (float)(config->min_computation_rate * opt_rate_scale);

    // Compute achieved Landauer efficiency
    double landauer_limit = compute_landauer_limit(result->optimal_temperature);
    double efficiency_factor = 1e-3;
    result->min_energy_per_bit = (float)(landauer_limit / efficiency_factor);
    result->landauer_efficiency = (float)(landauer_limit / result->min_energy_per_bit);

    // Compute ATP consumption
    // ATP hydrolysis ~50 kJ/mol = 8.3e-20 J/molecule
    double energy_per_atp = 8.3e-20;
    double power = (double)result->min_energy_per_bit * (double)result->optimal_bit_rate;
    result->heat_production = (float)power;
    result->atp_consumption_rate = (float)(power / energy_per_atp);

    // Statistics
    result->final_energy = anneal_result.final_energy;
    result->acceptance_rate = anneal_result.acceptance_rate;
    result->iterations_run = anneal_result.iterations_run;
    result->converged = (result->landauer_efficiency > 0.0001f);

    qmc_anneal_result_free(&anneal_result);

    return 0;
}

int thermo_qmc_landauer_efficiency(
    const nimcp_thermodynamic_state_t* state,
    float bit_rate,
    float* efficiency
) {
    if (!state || !efficiency) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_qmc_landauer_efficiency: required parameter is NULL (state, efficiency)");
        return -1;
    }
    if (!state->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_qmc_landauer_efficiency: state->initialized is NULL");
        return -1;
    }

    // Get current Landauer limit
    double landauer_limit = compute_landauer_limit(THERMO_QMC_DEFAULT_TEMP);

    // Get actual energy per bit from state
    double actual_per_bit = state->energy_per_bit;
    if (actual_per_bit <= 0.0) {
        // Estimate from total energy and assumed bit count
        actual_per_bit = state->power_consumption / (double)bit_rate;
    }

    // Efficiency = limit / actual
    if (actual_per_bit > 0.0) {
        *efficiency = (float)(landauer_limit / actual_per_bit);
        if (*efficiency > 1.0f) *efficiency = 1.0f;
    } else {
        *efficiency = 0.0f;
    }

    return 0;
}

int thermo_qmc_landauer_limit(float temperature, float* energy_per_bit) {
    if (!energy_per_bit) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "energy_per_bit is NULL");

        return -1;

    }

    *energy_per_bit = (float)compute_landauer_limit((double)temperature);
    return 0;
}

//=============================================================================
// ATP Pool Optimization API Implementation
//=============================================================================

int thermo_qmc_optimize_atp(
    nimcp_thermodynamic_state_t* state,
    const thermo_atp_config_t* config,
    thermo_atp_result_t* result
) {
    if (!state || !config || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_qmc_optimize_atp: required parameter is NULL (state, config, result)");
        return -1;
    }
    if (!state->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_qmc_optimize_atp: state->initialized is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    // Simple optimization: find steady-state ATP pool
    // dATP/dt = production - consumption = 0 at steady state

    double production = config->production_rate;
    double consumption = config->consumption_rate;

    // Sustainable rate is limited by production
    result->sustainable_rate = (float)production;

    // Optimal pool maintains reserve during consumption spikes
    double spike_factor = 2.0;  // Assume 2x consumption spikes
    double recovery_time_s = 1.0;  // 1 second recovery window

    result->optimal_pool_size = (float)(consumption * spike_factor * recovery_time_s);
    result->optimal_pool_size /= (1.0f - config->min_reserve);

    result->optimal_turnover = (float)(consumption / result->optimal_pool_size);

    // Efficiency is production/consumption under load
    if (consumption > 0.0) {
        result->achieved_efficiency = (float)(production / consumption);
        if (result->achieved_efficiency > 1.0f) {
            result->achieved_efficiency = 1.0f;
        }
    }

    result->recovery_time = (float)recovery_time_s;
    result->stable = (production >= consumption);

    return 0;
}

int thermo_qmc_atp_sustainability(
    const nimcp_thermodynamic_state_t* state,
    float load_factor,
    float* sustainability
) {
    if (!state || !sustainability) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_qmc_atp_sustainability: required parameter is NULL (state, sustainability)");
        return -1;
    }

    // Sustainability based on ATP availability vs consumption
    double atp_available = state->atp_available;
    double consumption_rate = state->atp_consumption_rate * (double)load_factor;

    // Time until depletion at this load
    double time_to_depletion = 1e10;  // Very long
    if (consumption_rate > 0.0) {
        time_to_depletion = atp_available / consumption_rate;
    }

    // Normalize: >1 hour = fully sustainable
    *sustainability = (float)(time_to_depletion / 3600.0);
    if (*sustainability > 1.0f) *sustainability = 1.0f;

    return 0;
}

//=============================================================================
// Entropy Production API Implementation
//=============================================================================

int thermo_qmc_entropy_production(
    const nimcp_thermodynamic_state_t* state,
    const thermo_entropy_config_t* config,
    thermo_entropy_result_t* result
) {
    if (!state || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_qmc_entropy_production: required parameter is NULL (state, result)");
        return -1;
    }
    if (!state->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_qmc_entropy_production: state->initialized is NULL");
        return -1;
    }

    (void)config;  // Use state's current values

    memset(result, 0, sizeof(*result));

    // Total entropy production rate from state
    result->total_entropy_rate = (float)state->entropy_production_rate;

    // Estimate component contributions
    // Ion channels: ~50% of neural energy
    if (config && config->include_channel_entropy) {
        result->channel_entropy = result->total_entropy_rate * 0.5f;
    }

    // Active transport: ~30% of energy
    if (config && config->include_transport_entropy) {
        result->transport_entropy = result->total_entropy_rate * 0.3f;
    }

    // Dissipation rate = entropy * temperature
    result->dissipation_rate = (float)(state->entropy_production_rate * THERMO_QMC_DEFAULT_TEMP);

    // Information cost: bits per joule dissipated
    // Using Landauer: 1 bit costs kT ln 2
    double landauer = compute_landauer_limit(THERMO_QMC_DEFAULT_TEMP);
    if (result->dissipation_rate > 0.0f) {
        result->information_cost = (float)(1.0 / landauer);  // Max bits per joule
    }

    // Thermodynamic efficiency
    result->thermodynamic_efficiency = (float)state->thermodynamic_efficiency;

    return 0;
}

int thermo_qmc_information_cost(
    const nimcp_thermodynamic_state_t* state,
    float bits,
    float* energy_cost,
    float* entropy_cost
) {
    if (!state || !energy_cost || !entropy_cost) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_qmc_information_cost: required parameter is NULL (state, energy_cost, entropy_cost)");
        return -1;
    }

    // Landauer minimum
    double landauer = compute_landauer_limit(THERMO_QMC_DEFAULT_TEMP);

    // Actual cost based on state efficiency
    double efficiency = state->landauer_efficiency;
    if (efficiency <= 0.0 || efficiency > 1.0) {
        efficiency = 1e-6;  // Very inefficient default
    }

    double actual_per_bit = landauer / efficiency;
    *energy_cost = (float)(actual_per_bit * (double)bits);

    // Entropy cost: dS = dQ / T
    *entropy_cost = (float)(*energy_cost / THERMO_QMC_DEFAULT_TEMP);

    return 0;
}

//=============================================================================
// Temperature Optimization API Implementation
//=============================================================================

int thermo_qmc_optimal_temperature(
    nimcp_thermodynamic_state_t* state,
    float temp_min,
    float temp_max,
    float* optimal_temp
) {
    if (!state || !optimal_temp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_qmc_optimal_temperature: required parameter is NULL (state, optimal_temp)");
        return -1;
    }
    if (!state->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "thermo_qmc_optimal_temperature: state->initialized is NULL");
        return -1;
    }

    // For neural systems, optimal temperature balances:
    // - Enzymatic rates (increase with temp, Arrhenius)
    // - Protein stability (decreases at high temp)
    // - Landauer cost (increases with temp)

    // Simplified: assume optimum near body temperature
    float body_temp = 310.15f;  // 37°C

    // Clamp to allowed range
    *optimal_temp = body_temp;
    if (*optimal_temp < temp_min) *optimal_temp = temp_min;
    if (*optimal_temp > temp_max) *optimal_temp = temp_max;

    return 0;
}
