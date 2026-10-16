/**
 * @file nimcp_quantum_annealing.c
 * @brief Quantum-inspired annealing implementation
 *
 * WHAT: Core implementation of quantum annealing optimization
 * WHY:  Escape local minima in weight space for better learning
 * HOW:  Combine simulated annealing with quantum tunneling probability
 *
 * COMPLEXITY: O(N * D) per iteration where N = iterations, D = dimensions
 *
 * @author NIMCP Development Team
 * @date 2025-11-12
 * @version 2.7.0 Phase 11 Enhancement C1.1
 */

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "security/nimcp_security.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "OPTIMIZATION"

/* Named constants for quantum annealing parameters (P3: replace magic numbers) */
#define QA_TUNNELING_ALPHA          0.5F   /**< Tunable exponent for quantum tunneling */
#define QA_TUNNELING_TEMP_MIN       1e-10F /**< Minimum scaled temperature to avoid div-by-zero */
#define QA_TUNNELING_EXP_CLAMP      -20.0F /**< Clamp exponent to prevent underflow */
#define QA_NEIGHBOR_STEP_SCALE      0.5F   /**< Neighbor perturbation scale factor */
#define QA_LOG_COOLING_MIN_DENOM    1e-10F /**< Minimum denominator for logarithmic cooling */

#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/algorithms/nimcp_monte_carlo.h"
#include "utils/quantum/nimcp_quantum_monte_carlo.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(qa)

//=============================================================================
// Monte Carlo Integration - Thread-local seed for reproducibility
//=============================================================================

/** Thread-local fallback seed for Monte Carlo operations when annealer has no rng_state */
static __thread uint32_t g_qa_mc_seed = 0;

//=============================================================================
// Bio-Async State
//=============================================================================

static bio_module_context_t g_quantum_bio_ctx = NULL;
static bool g_quantum_initialized = false;

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal annealer state
 *
 * WHAT: Private data structure for quantum annealer
 * WHY:  Encapsulate implementation details
 * HOW:  Opaque handle pattern
 */
struct quantum_annealer_struct {
    quantum_annealing_config_t config;  /**< Configuration copy */
    uint32_t* rng_state;                /**< RNG state for reproducibility */
    bio_module_context_t bio_ctx;       /**< Bio-async context */
    void* sec_ctx;                      /**< Security context */
};

//=============================================================================
// Helper Functions (All < 50 lines, early returns, WHAT-WHY-HOW docs)
//=============================================================================

/**
 * @brief Initialize random number generator
 *
 * WHAT: Set up RNG with seed from config
 * WHY:  Provide reproducible randomness for annealing
 * HOW:  Use seed if non-zero, otherwise use time
 *
 * @param annealer Annealer instance
 *
 * COMPLEXITY: O(1)
 */
static void init_rng(quantum_annealer_t annealer) {
    LOG_DEBUG("Initializing RNG for quantum annealer");

    if (!annealer) {
        LOG_ERROR("init_rng: null annealer");
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "init_rng: null annealer");
        return;
    }

    uint32_t seed = annealer->config.seed;
    if (seed == 0) {
        seed = (uint32_t)time(NULL);
        LOG_DEBUG("Using time-based seed: %u", seed);
    } else {
        LOG_DEBUG("Using configured seed: %u", seed);
    }

    // BUGFIX: Use nimcp_malloc for consistency with memory tracking
    annealer->rng_state = nimcp_malloc(sizeof(uint32_t));
    if (!annealer->rng_state) {
        LOG_ERROR("Failed to allocate RNG state");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(uint32_t),
                          "Failed to allocate RNG state for quantum annealer");
        return;
    }

    *annealer->rng_state = seed;
    LOG_DEBUG("RNG initialized with seed %u", seed);
}

/**
 * @brief Generate random float in [0, 1)
 *
 * WHAT: Fast random number generation
 * WHY:  Need many random numbers during annealing
 * HOW:  Numerical Recipes LCG for deterministic per-seed sequences
 *
 * @param annealer Annealer instance
 * @return Random float in [0, 1)
 *
 * COMPLEXITY: O(1)
 */
static float random_float(quantum_annealer_t annealer) {
    if (!annealer || !annealer->rng_state) return 0.5F;

    /* Numerical Recipes LCG */
    *annealer->rng_state = (*annealer->rng_state * 1664525U + 1013904223U);
    return (*annealer->rng_state) / 4294967296.0F;
}

/**
 * @brief Generate random Gaussian value
 *
 * WHAT: Box-Muller transform for normal distribution
 * WHY:  Need Gaussian perturbations for state transitions
 * HOW:  Inline Box-Muller using random_float() for deterministic sequences
 *
 * @param annealer Annealer instance
 * @param mean Mean of distribution
 * @param stddev Standard deviation
 * @return Random value from N(mean, stddev^2)
 *
 * COMPLEXITY: O(1)
 */
static float random_gaussian(quantum_annealer_t annealer, float mean, float stddev) {
    if (!annealer) return mean;

    /* Box-Muller transform */
    float u1 = random_float(annealer);
    float u2 = random_float(annealer);

    /* Avoid log(0) */
    u1 = (u1 < 1e-10F) ? 1e-10F : u1;

    float z = sqrtf(-2.0F * logf(u1)) * cosf(2.0F * (float)M_PI * u2);
    return mean + stddev * z;
}

/**
 * @brief Validate configuration parameters
 *
 * WHAT: Check config values are in valid ranges
 * WHY:  Prevent invalid configurations causing undefined behavior
 * HOW:  Early return pattern for each validation
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * VALIDATION RULES:
 * - initial_temperature > 0
 * - final_temperature > 0
 * - final_temperature < initial_temperature
 * - num_iterations > 0
 * - quantum_strength in [0, 1]
 *
 * COMPLEXITY: O(1)
 */
static bool validate_config(const quantum_annealing_config_t* config) {
    LOG_DEBUG("Validating quantum annealing configuration");

    if (!config) {
        LOG_ERROR("validate_config: null config");
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "validate_config: null config");
        return false;
    }
    if (config->initial_temperature <= 0.0F) {
        LOG_ERROR("Invalid initial_temperature: %f (must be > 0)", config->initial_temperature);
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                   "Invalid initial_temperature: %f (must be > 0)", config->initial_temperature);
        return false;
    }
    if (config->final_temperature <= 0.0F) {
        LOG_ERROR("Invalid final_temperature: %f (must be > 0)", config->final_temperature);
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                   "Invalid final_temperature: %f (must be > 0)", config->final_temperature);
        return false;
    }
    if (config->final_temperature >= config->initial_temperature) {
        LOG_ERROR("Invalid temperature range: final (%f) >= initial (%f)",
                  config->final_temperature, config->initial_temperature);
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                   "Invalid temperature range: final (%f) >= initial (%f)",
                   config->final_temperature, config->initial_temperature);
        return false;
    }
    if (config->num_iterations == 0) {
        LOG_ERROR("Invalid num_iterations: 0 (must be > 0)");
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Invalid num_iterations: 0 (must be > 0)");
        return false;
    }
    if (config->quantum_strength < 0.0F || config->quantum_strength > 1.0F) {
        LOG_ERROR("Invalid quantum_strength: %f (must be in [0, 1])", config->quantum_strength);
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                   "Invalid quantum_strength: %f (must be in [0, 1])", config->quantum_strength);
        return false;
    }

    LOG_DEBUG("Configuration validated successfully");
    return true;
}

/**
 * @brief Calculate temperature at specific iteration
 *
 * WHAT: Apply cooling schedule formula
 * WHY:  Temperature controls exploration vs exploitation
 * HOW:  Use formula based on config.cooling_schedule
 *
 * @param annealer Annealer instance
 * @param iteration Current iteration (0 to N-1)
 * @return Temperature value
 *
 * FORMULAS:
 * - EXPONENTIAL: T(t) = T_init * exp(-t/τ)
 * - LINEAR: T(t) = T_init - (T_init - T_final) * t/N
 * - LOGARITHMIC: T(t) = T_init / log(1 + t)
 *
 * COMPLEXITY: O(1)
 */
static float calculate_temperature(quantum_annealer_t annealer, uint32_t iteration) {
    if (!annealer) return 0.0F;

    float T_init = annealer->config.initial_temperature;
    float T_final = annealer->config.final_temperature;
    uint32_t N = annealer->config.num_iterations;

    // Avoid division by zero
    if (N == 0) return T_init;

    /* P1-OPT-1: When N==1, (N-1)=0 causes division by zero in progress,
     * lambda, lambda_adapt. Return T_init since there's only one iteration. */
    if (N <= 1) return T_init;

    float progress = (float)iteration / (float)(N - 1);

    switch (annealer->config.cooling_schedule) {
        case COOLING_EXPONENTIAL: {
            // T(t) = T_init * exp(-λ * t) where λ chosen s.t. T(N-1) = T_final
            float lambda = -logf(T_final / T_init) / (float)(N - 1);
            return T_init * expf(-lambda * (float)iteration);
        }

        case COOLING_LINEAR:
            return T_init - (T_init - T_final) * progress;

        case COOLING_LOGARITHMIC: {
            /* T(t) = T_init / (1 + c * log(1 + t))
             * where c = (T_init/T_final - 1) / log(N)
             * This gives T(0) = T_init and T(N-1) ≈ T_final */
            float log_N = logf((float)N);
            float c = (log_N > QA_LOG_COOLING_MIN_DENOM)
                     ? (T_init / T_final - 1.0F) / log_N
                     : 1.0F;
            return T_init / (1.0F + c * logf(1.0F + (float)iteration));
        }

        case COOLING_ADAPTIVE: {
            // Simplified adaptive: use exponential as baseline
            float lambda_adapt = -logf(T_final / T_init) / (float)(N - 1);
            return T_init * expf(-lambda_adapt * (float)iteration);
        }

        default:
            return T_init;
    }
}

/**
 * @brief Calculate Metropolis acceptance probability
 *
 * WHAT: Classical simulated annealing acceptance criterion
 * WHY:  Allow uphill moves with probability decreasing with ΔE and T
 * HOW:  P = exp(-ΔE / T) for ΔE > 0
 *
 * @param delta_energy Energy difference (new - old)
 * @param temperature Current temperature
 * @return Acceptance probability in [0, 1]
 *
 * MATH:
 * - If ΔE <= 0: always accept (return 1.0)
 * - If ΔE > 0: P = exp(-ΔE / T)
 *
 * COMPLEXITY: O(1)
 */
static float metropolis_probability(float delta_energy, float temperature) {
    if (delta_energy <= 0.0F) return 1.0F;
    if (temperature <= 0.0F) return 0.0F;

    return expf(-delta_energy / temperature);
}

/**
 * @brief Calculate quantum tunneling probability
 *
 * WHAT: Quantum-inspired tunneling through energy barriers
 * WHY:  Escape local minima classical annealing cannot escape
 * HOW:  P_tunnel = Γ * exp(-B/T^α) where Γ is quantum strength
 *
 * @param delta_energy Energy barrier height
 * @param temperature Current temperature
 * @param quantum_strength Tunneling probability multiplier
 * @return Tunneling probability in [0, 1]
 *
 * MATH:
 * - B = |ΔE| (barrier height)
 * - α = 0.5 (tunable exponent)
 * - P = Γ * exp(-B / T^α)
 *
 * BIOLOGY: Analogous to spontaneous activity helping escape metastable states
 * COMPLEXITY: O(1)
 */
static float quantum_tunneling_probability(
    float delta_energy,
    float temperature,
    float quantum_strength
) {
    if (!quantum_strength) return 0.0F;
    if (delta_energy <= 0.0F) return 0.0F;
    if (temperature <= 0.0F) return 0.0F;

    float barrier_height = fabsf(delta_energy);
    float alpha = QA_TUNNELING_ALPHA;
    float temp_scaled = powf(temperature, alpha);

    // Avoid division by zero or overflow
    if (temp_scaled < QA_TUNNELING_TEMP_MIN) return 0.0F;

    float exponent = -barrier_height / temp_scaled;
    // Clamp to prevent underflow
    exponent = (exponent < QA_TUNNELING_EXP_CLAMP) ? QA_TUNNELING_EXP_CLAMP : exponent;

    return quantum_strength * expf(exponent);
}

/**
 * @brief Generate neighbor state via perturbation
 *
 * WHAT: Create nearby state in solution space
 * WHY:  Explore neighborhood of current state
 * HOW:  Add Gaussian noise scaled by temperature
 *
 * @param annealer Annealer instance
 * @param current Current state
 * @param neighbor [OUT] Neighbor state
 * @param dim Dimensionality
 * @param temperature Current temperature
 *
 * PERTURBATION: neighbor[i] = current[i] + N(0, σ)
 * where σ = 0.1 * sqrt(T) (exploration decreases with T)
 *
 * COMPLEXITY: O(D) where D = dim
 */
static void generate_neighbor(
    quantum_annealer_t annealer,
    const float* current,
    float* neighbor,
    uint32_t dim,
    float temperature
) {
    if (!annealer || !current || !neighbor) return;

    // Larger step size for better exploration: 0.5 * sqrt(T) instead of 0.1 * sqrt(T)
    // This allows faster convergence while still decreasing step size as T decreases
    float stddev = QA_NEIGHBOR_STEP_SCALE * sqrtf(temperature);

    for (uint32_t i = 0; i < dim; ++i) {
        neighbor[i] = current[i] + random_gaussian(annealer, 0.0F, stddev);
    }
}

/**
 * @brief Copy state vector
 *
 * WHAT: Copy state from source to destination
 * WHY:  Maintain current and best states
 * HOW:  Simple memcpy
 *
 * @param dst Destination
 * @param src Source
 * @param dim Dimensionality
 *
 * COMPLEXITY: O(D)
 */
static void copy_state(float* dst, const float* src, uint32_t dim) {
    if (!dst || !src) return;
    memcpy(dst, src, dim * sizeof(float));
}

//=============================================================================
// Public API Implementation
//=============================================================================

quantum_annealing_config_t quantum_annealing_default_config(void) {
    /**
     * WHAT: Return sensible default configuration
     * WHY:  Quick setup for common use cases
     * HOW:  Preset values based on typical scenarios
     */
    LOG_DEBUG("Creating default quantum annealing configuration");

    quantum_annealing_config_t config = {
        .initial_temperature = 1.0F,
        .final_temperature = 0.01F,
        .num_iterations = 1000,
        .cooling_schedule = COOLING_EXPONENTIAL,
        .quantum_strength = 0.5F,
        .enable_tunneling = true,
        .seed = 0  // Use time-based seed
    };

    LOG_DEBUG("Default config: T_init=%f, T_final=%f, iters=%u, quantum_strength=%f",
              config.initial_temperature, config.final_temperature,
              config.num_iterations, config.quantum_strength);

    return config;
}

quantum_annealer_t quantum_annealer_create(const quantum_annealing_config_t* config) {
    /**
     * WHAT: Create and initialize quantum annealer
     * WHY:  Set up optimizer with specified parameters
     * HOW:  Allocate, validate, initialize RNG
     *
     * GUARD CLAUSES: Early returns for invalid inputs
     */

    // Guard: null config
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_annealer_create: null config");
        return NULL;
    }

    // Guard: invalid configuration
    if (!validate_config(config)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "quantum_annealer_create: invalid config");
        return NULL;
    }

    // Allocate annealer
    // BUGFIX: Use nimcp_malloc for consistency with memory tracking
    quantum_annealer_t annealer = nimcp_malloc(sizeof(struct quantum_annealer_struct));
    if (!annealer) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(struct quantum_annealer_struct),
                          "Failed to allocate quantum annealer structure");
        return NULL;
    }

    // Copy configuration
    memcpy(&annealer->config, config, sizeof(quantum_annealing_config_t));

    // Initialize RNG
    init_rng(annealer);
    if (!annealer->rng_state) {
        nimcp_free(annealer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "quantum_annealer_create: failed to allocate RNG state");
        return NULL;
    }

    return annealer;
}

void quantum_annealer_destroy(quantum_annealer_t annealer) {
    /**
     * WHAT: Free annealer resources
     * WHY:  Prevent memory leaks
     * HOW:  Free RNG state, then annealer
     */

    if (!annealer) return;

    if (annealer->rng_state) {
        // BUGFIX: Use nimcp_free to match nimcp_malloc
        nimcp_free(annealer->rng_state);
        annealer->rng_state = NULL;
    }

    nimcp_free(annealer);
}

float quantum_annealer_get_temperature(quantum_annealer_t annealer, uint32_t iteration) {
    /**
     * WHAT: Get temperature at specific iteration
     * WHY:  Inspect cooling schedule, debug behavior
     * HOW:  Delegate to calculate_temperature()
     */

    if (!annealer) return -1.0F;
    if (iteration >= annealer->config.num_iterations) return -1.0F;

    return calculate_temperature(annealer, iteration);
}

float quantum_anneal(
    quantum_annealer_t annealer,
    energy_function_t energy_func,
    const float* initial_state,
    float* optimized_state,
    uint32_t dim,
    void* user_data
) {
    /**
     * WHAT: Run quantum annealing optimization (main algorithm)
     * WHY:  Find global minimum of energy function
     * HOW:  Iterative cooling with Metropolis + quantum tunneling
     *
     * ALGORITHM:
     * 1. Initialize current = initial, best = initial
     * 2. For each iteration:
     *    a. Calculate temperature T(t)
     *    b. Generate neighbor state
     *    c. Evaluate energies
     *    d. Accept/reject via combined classical + quantum probability
     *    e. Track best state found
     * 3. Return best state and energy
     *
     * COMPLEXITY: O(N * D * E) where E = energy_func cost
     */

    // Guard clauses with exception handling
    if (!annealer) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_anneal: null annealer");
        return INFINITY;
    }
    if (!energy_func) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_anneal: null energy_func");
        return INFINITY;
    }
    if (!initial_state || !optimized_state) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_anneal: null state pointer");
        return INFINITY;
    }
    if (dim == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "quantum_anneal: dim must be > 0");
        return INFINITY;
    }

    // Allocate temporary state vectors
    // BUGFIX: Use nimcp_malloc instead of malloc for consistency with memory tracking
    // WHY: Regular malloc/free can cause heap corruption detected by nimcp memory tracker
    /* P2-OPT-1: Check for integer overflow in state allocation size */
    if (dim > SIZE_MAX / sizeof(float)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "quantum_anneal: dim too large, allocation overflow");
        return INFINITY;
    }
    size_t state_alloc_size = dim * sizeof(float);
    float* current_state = nimcp_malloc(state_alloc_size);
    float* best_state = nimcp_malloc(state_alloc_size);
    float* neighbor_state = nimcp_malloc(state_alloc_size);

    if (!current_state || !best_state || !neighbor_state) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, state_alloc_size * 3,
                          "Failed to allocate state vectors for quantum annealing (dim=%u)", dim);
        nimcp_free(current_state);
        nimcp_free(best_state);
        nimcp_free(neighbor_state);
        return INFINITY;
    }

    // Initialize states
    copy_state(current_state, initial_state, dim);
    copy_state(best_state, initial_state, dim);

    float current_energy = energy_func(current_state, dim, user_data);
    float best_energy = current_energy;

    // Main annealing loop
    for (uint32_t iter = 0; iter < annealer->config.num_iterations; ++iter) {
        float temperature = calculate_temperature(annealer, iter);

        // Generate neighbor
        generate_neighbor(annealer, current_state, neighbor_state, dim, temperature);

        // Evaluate neighbor energy
        float neighbor_energy = energy_func(neighbor_state, dim, user_data);
        float delta_energy = neighbor_energy - current_energy;

        // Accept/reject decision
        bool accept = false;

        if (delta_energy <= 0.0F) {
            // Always accept improvements
            accept = true;
        } else {
            // Classical Metropolis
            float p_metropolis = metropolis_probability(delta_energy, temperature);

            // Quantum tunneling (if enabled)
            float p_quantum = 0.0F;
            if (annealer->config.enable_tunneling) {
                p_quantum = quantum_tunneling_probability(
                    delta_energy, temperature, annealer->config.quantum_strength);
            }

            // Combined probability
            float p_total = p_metropolis + p_quantum;
            p_total = (p_total > 1.0F) ? 1.0F : p_total;  // Clamp to [0,1]

            // Accept with probability p_total
            accept = (random_float(annealer) < p_total);
        }

        // Update current state if accepted
        if (accept) {
            copy_state(current_state, neighbor_state, dim);
            current_energy = neighbor_energy;

            // Update best if improved
            if (current_energy < best_energy) {
                copy_state(best_state, current_state, dim);
                best_energy = current_energy;
            }
        }
    }

    // Copy best state to output
    copy_state(optimized_state, best_state, dim);

    // Cleanup
    // BUGFIX: Use nimcp_free to match nimcp_malloc
    nimcp_free(current_state);
    nimcp_free(best_state);
    nimcp_free(neighbor_state);

    return best_energy;
}

//=============================================================================
// Monte Carlo Enhanced Functions
//=============================================================================

/**
 * @brief Adaptive annealing using MCMC proposal learning
 *
 * WHAT: Quantum annealing with adaptive proposal distribution
 * WHY:  Learn optimal step sizes from acceptance history
 * HOW:  Use qmc_adaptive_anneal() with learned proposal covariance
 *
 * @param annealer Annealer instance
 * @param energy_func Energy function to minimize
 * @param initial_state Starting state
 * @param optimized_state Output: best state found
 * @param dim State dimensionality
 * @param user_data Passed to energy function
 * @param result Output: detailed optimization result
 * @return Best energy found
 */
float quantum_annealer_optimize_adaptive_mc(
    quantum_annealer_t annealer,
    energy_function_t energy_func,
    const float* initial_state,
    float* optimized_state,
    uint32_t dim,
    void* user_data,
    qmc_anneal_result_t* result
) {
    if (!annealer) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_annealer_optimize_adaptive_mc: null annealer");
        return INFINITY;
    }
    if (!energy_func) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_annealer_optimize_adaptive_mc: null energy_func");
        return INFINITY;
    }
    if (!initial_state || !optimized_state) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_annealer_optimize_adaptive_mc: null state pointer");
        return INFINITY;
    }
    if (!result) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_annealer_optimize_adaptive_mc: null result");
        return INFINITY;
    }

    /* Set up QMC adaptive annealing config */
    qmc_anneal_config_t config = {
        .initial_temp = annealer->config.initial_temperature,
        .final_temp = annealer->config.final_temperature,
        .num_iterations = annealer->config.num_iterations,
        .quantum_strength = annealer->config.quantum_strength,
        .strategy = QMC_PROPOSAL_ADAPTIVE,  /* Use adaptive proposal */
        .target_acceptance = 0.234f,  /* Optimal for random walk MH */
        .adaptation_interval = 100,
        .seed = annealer->config.seed
    };

    /* Initialize result (qmc_adaptive_anneal allocates best_state) */
    memset(result, 0, sizeof(*result));

    /* Run adaptive annealing */
    qmc_result_t err = qmc_adaptive_anneal(
        (qmc_energy_fn)energy_func,
        initial_state,
        dim,
        &config,
        user_data,
        result
    );

    if (err != QMC_OK) {
        qmc_anneal_result_free(result);
        return INFINITY;
    }

    /* Copy best state to output */
    copy_state(optimized_state, result->best_state, dim);

    return result->final_energy;
}

/**
 * @brief Importance sampling for Boltzmann distribution
 *
 * WHAT: Sample states proportional to Boltzmann weights
 * WHY:  Efficient sampling of low-energy configurations
 * HOW:  Use MC importance sampling with exp(-E/T) weights
 *
 * @param annealer Annealer instance
 * @param energy_func Energy function
 * @param temperature Current temperature
 * @param states Array of candidate states (dim x num_states)
 * @param num_states Number of candidate states
 * @param dim State dimensionality
 * @param user_data Passed to energy function
 * @return Index of sampled state
 */
uint32_t quantum_annealer_sample_boltzmann_mc(
    quantum_annealer_t annealer,
    energy_function_t energy_func,
    float temperature,
    const float* states,
    uint32_t num_states,
    uint32_t dim,
    void* user_data
) {
    if (!annealer) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_annealer_sample_boltzmann_mc: null annealer");
        return 0;
    }
    if (!energy_func) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_annealer_sample_boltzmann_mc: null energy_func");
        return 0;
    }
    if (!states) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_annealer_sample_boltzmann_mc: null states");
        return 0;
    }
    if (num_states == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "quantum_annealer_sample_boltzmann_mc: num_states must be > 0");
        return 0;
    }

    /* P1-OPT-2: Guard against non-positive temperature */
    if (temperature <= 0.0f) {
        /* At zero temperature, always return the lowest-energy state */
        return 0;
    }

    /* Compute Boltzmann weights */
    size_t weights_size = num_states * sizeof(float);
    float* weights = nimcp_malloc(weights_size);
    if (!weights) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, weights_size,
                          "Failed to allocate weights array for Boltzmann sampling (num_states=%u)", num_states);
        return 0;
    }

    float max_energy = -INFINITY;
    for (uint32_t i = 0; i < num_states; i++) {
        float E = energy_func(&states[i * dim], dim, user_data);
        weights[i] = E;
        if (E > max_energy) max_energy = E;
    }

    /* Convert to Boltzmann probabilities (shifted for numerical stability) */
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_states; i++) {
        weights[i] = expf(-(weights[i] - max_energy) / temperature);
        sum += weights[i];
    }

    /* P1-OPT-2: If all weights underflow to zero, fall back to uniform */
    if (sum == 0.0f) {
        for (uint32_t i = 0; i < num_states; i++) {
            weights[i] = 1.0f / (float)num_states;
        }
        sum = 1.0f;
    }

    /* Normalize */
    for (uint32_t i = 0; i < num_states; i++) {
        weights[i] /= sum;
    }

    /* Sample using MC importance sampling */
    uint32_t* seed = annealer->rng_state ? annealer->rng_state : &g_qa_mc_seed;
    if (*seed == 0) {
        *seed = mc_seed_from_time();
    }

    uint32_t sampled = qmc_measure_importance(weights, num_states, NULL, seed);

    nimcp_free(weights);
    return sampled;
}

/**
 * @brief Estimate partition function via Monte Carlo
 *
 * WHAT: Estimate Z = Σ exp(-E_i/T) for normalization
 * WHY:  Needed for free energy calculations
 * HOW:  MC sampling with variance estimation
 *
 * @param annealer Annealer instance
 * @param energy_func Energy function
 * @param temperature Current temperature
 * @param sample_states Sampled states for estimation
 * @param num_samples Number of samples
 * @param dim State dimensionality
 * @param user_data Passed to energy function
 * @param variance_out Output: variance of estimate
 * @return Estimated partition function
 */
float quantum_annealer_estimate_partition_mc(
    quantum_annealer_t annealer,
    energy_function_t energy_func,
    float temperature,
    const float* sample_states,
    uint32_t num_samples,
    uint32_t dim,
    void* user_data,
    float* variance_out
) {
    if (!annealer) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_annealer_estimate_partition_mc: null annealer");
        if (variance_out) *variance_out = INFINITY;
        return 0.0f;
    }
    if (!energy_func) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_annealer_estimate_partition_mc: null energy_func");
        if (variance_out) *variance_out = INFINITY;
        return 0.0f;
    }
    if (!sample_states) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_annealer_estimate_partition_mc: null sample_states");
        if (variance_out) *variance_out = INFINITY;
        return 0.0f;
    }
    if (num_samples == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "quantum_annealer_estimate_partition_mc: num_samples must be > 0");
        if (variance_out) *variance_out = INFINITY;
        return 0.0f;
    }

    /* P1-OPT-3: Validate temperature to avoid division by zero or negative */
    if (temperature <= 0.0f) {
        if (variance_out) *variance_out = INFINITY;
        return 0.0f;
    }

    /* P2 fix: Store energies in array to avoid double evaluation of energy_func.
     * A stochastic energy function evaluated twice returns different results. */
    float* energies = nimcp_malloc(num_samples * sizeof(float));
    if (!energies) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_samples * sizeof(float),
                          "Failed to allocate energy cache for partition estimation (num_samples=%u)", num_samples);
        if (variance_out) *variance_out = INFINITY;
        return 0.0f;
    }

    /* Compute Boltzmann factors */
    float sum = 0.0f;
    float sum_sq = 0.0f;
    float max_energy = -INFINITY;

    /* First pass: compute and store energies, find max for numerical stability */
    for (uint32_t i = 0; i < num_samples; i++) {
        energies[i] = energy_func(&sample_states[i * dim], dim, user_data);
        if (energies[i] > max_energy) max_energy = energies[i];
    }

    /* Second pass: compute shifted Boltzmann factors using stored energies */
    for (uint32_t i = 0; i < num_samples; i++) {
        float w = expf(-(energies[i] - max_energy) / temperature);
        sum += w;
        sum_sq += w * w;
    }

    nimcp_free(energies);

    /* Estimate and variance */
    float mean = sum / num_samples;
    float var = (sum_sq / num_samples) - (mean * mean);
    var = var / num_samples;  /* Variance of mean */

    if (variance_out) {
        *variance_out = var;
    }

    /* Scale back by shift factor */
    return mean * expf(-max_energy / temperature);
}

/**
 * @brief Get thread-local MC seed for quantum annealing
 *
 * @return Pointer to thread-local seed
 */
uint32_t* quantum_annealer_get_mc_seed(void) {
    if (g_qa_mc_seed == 0) {
        g_qa_mc_seed = mc_seed_from_time();
    }
    return &g_qa_mc_seed;
}
