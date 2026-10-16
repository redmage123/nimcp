#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_ephaptic_quantum_bridge.c - Ephaptic Quantum Monte Carlo Implementation
//=============================================================================

#include "physics/bridges/nimcp_ephaptic_quantum_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(ephaptic_quantum_bridge)

#define LOG_MODULE "EPHAPTIC_QUANTUM_BRIDGE"


//=============================================================================
// Internal Constants
//=============================================================================

#define PI NIMCP_PI_F

//=============================================================================
// Internal Context Structures
//=============================================================================

/**
 * @brief Context for coherence optimization energy function
 */
typedef struct {
    nimcp_ephaptic_system_t* system;
    const ephaptic_coherence_target_t* target;
    const ephaptic_coherence_config_t* config;
    nimcp_ephaptic_config_t original_config;
} coherence_opt_ctx_t;

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Compute Kuramoto order parameter from phases
 */
static float compute_order_parameter(const float* phases, uint32_t count) {
    if (count == 0) return 0.0f;

    float sum_cos = 0.0f, sum_sin = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        sum_cos += cosf(phases[i]);
        sum_sin += sinf(phases[i]);
    }

    float n = (float)count;
    return sqrtf(sum_cos * sum_cos + sum_sin * sum_sin) / n;
}

/**
 * @brief Compute mean phase from phases
 */
static float compute_mean_phase(const float* phases, uint32_t count) {
    if (count == 0) return 0.0f;

    float sum_cos = 0.0f, sum_sin = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        sum_cos += cosf(phases[i]);
        sum_sin += sinf(phases[i]);
    }

    return atan2f(sum_sin, sum_cos);
}

/**
 * @brief Apply parameters to ephaptic system
 */
static void apply_params_to_system(
    nimcp_ephaptic_system_t* system,
    const float* params,
    const ephaptic_coherence_config_t* config
) {
    system->config.kuramoto_coupling =
        nimcp_clampf(params[0], config->coupling_min, config->coupling_max);
    system->config.sync_threshold =
        nimcp_clampf(params[1], config->sync_threshold_min, config->sync_threshold_max);
    system->config.field_decay_constant =
        nimcp_clampf(params[2], config->field_decay_min, config->field_decay_max);
}

/**
 * @brief Energy function for coherence optimization
 */
static float coherence_energy_fn(
    const float* params,
    uint32_t dim,
    void* user_data
) {
    coherence_opt_ctx_t* ctx = (coherence_opt_ctx_t*)user_data;
    (void)dim;

    // Apply parameters
    apply_params_to_system(ctx->system, params, ctx->config);

    // Reset and simulate
    nimcp_ephaptic_reset(ctx->system);

    uint32_t num_steps = (uint32_t)(ctx->config->sim_duration_ms / ctx->config->sim_dt);
    for (uint32_t i = 0; i < num_steps; i++) {
        nimcp_ephaptic_synchronize(ctx->system, ctx->config->sim_dt);
        nimcp_ephaptic_update_field(ctx->system, ctx->config->sim_dt);
    }

    // Get achieved coherence
    float coherence;
    nimcp_ephaptic_get_phase_coherence(ctx->system, &coherence);

    // Compute error
    float coherence_error = fabsf(coherence - ctx->target->target_coherence);

    // Penalize if below minimum synchronized neurons
    float sync_penalty = 0.0f;
    if (ctx->system->synchronized_neurons < ctx->target->min_synchronized) {
        sync_penalty = (float)(ctx->target->min_synchronized - ctx->system->synchronized_neurons) * 10.0f;
    }

    return coherence_error + sync_penalty;
}

/**
 * @brief Build adjacency matrix from neuron positions
 */
static void build_adjacency_matrix(
    const nimcp_ephaptic_system_t* system,
    uint8_t* adjacency,
    float max_distance
) {
    uint32_t n = system->neuron_count;

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            if (i == j) {
                adjacency[i * n + j] = 0;
                continue;
            }

            // Compute distance
            float dx = system->neurons[i].position[0] - system->neurons[j].position[0];
            float dy = system->neurons[i].position[1] - system->neurons[j].position[1];
            float dz = system->neurons[i].position[2] - system->neurons[j].position[2];
            float dist = sqrtf(dx * dx + dy * dy + dz * dz);

            adjacency[i * n + j] = (dist <= max_distance) ? 1 : 0;
        }
    }
}

//=============================================================================
// Configuration API Implementation
//=============================================================================

int ephaptic_qmc_coherence_default_config(ephaptic_coherence_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_coherence_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->initial_temp = 10.0f;
    config->final_temp = 0.01f;
    config->num_iterations = EPHAPTIC_QMC_DEFAULT_ITERATIONS;
    config->quantum_strength = 0.3f;

    config->coupling_min = 0.01f;
    config->coupling_max = 2.0f;
    config->sync_threshold_min = 0.3f;
    config->sync_threshold_max = 0.95f;
    config->field_decay_min = 0.5f;
    config->field_decay_max = 5.0f;

    config->sim_duration_ms = 200.0f;
    config->sim_dt = 0.1f;

    config->seed = 0;

    return 0;
}

int ephaptic_qmc_coherence_default_target(ephaptic_coherence_target_t* target) {
    if (!target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_coherence_default_target: target is NULL");
        return -1;
    }

    memset(target, 0, sizeof(*target));

    target->target_coherence = 0.7f;        // 70% phase coherence
    target->target_frequency = 40.0f;       // 40 Hz (gamma)
    target->frequency_tolerance = 5.0f;     // +/- 5 Hz
    target->min_synchronized = 10;          // At least 10 neurons

    return 0;
}

int ephaptic_qmc_walk_default_config(ephaptic_walk_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_walk_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->max_steps = 100;
    config->mcts_iterations = EPHAPTIC_QMC_DEFAULT_MCTS_ITERS;
    config->exploration_constant = 1.41f;   // sqrt(2)
    config->adaptive_coin = true;
    config->seed = 0;

    return 0;
}

int ephaptic_qmc_entropy_default_config(ephaptic_entropy_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_entropy_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->num_samples = EPHAPTIC_QMC_DEFAULT_SAMPLES;
    config->time_window_ms = 100.0f;
    config->compute_mutual_info = false;
    config->num_regions = 4;
    config->seed = 0;

    return 0;
}

int ephaptic_qmc_pattern_default_config(ephaptic_pattern_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_pattern_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->mcts_iterations = 500;
    config->exploration_constant = 1.41f;
    config->max_patterns = 10;
    config->min_coherence = 0.5f;
    config->seed = 0;

    return 0;
}

//=============================================================================
// Phase Coherence Optimization API Implementation
//=============================================================================

int ephaptic_qmc_optimize_coherence(
    nimcp_ephaptic_system_t* system,
    const ephaptic_coherence_target_t* target,
    const ephaptic_coherence_config_t* config,
    ephaptic_coherence_result_t* result
) {
    if (!system || !target || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_optimize_coherence: required parameter is NULL (system, target, result)");
        return -1;
    }
    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_optimize_coherence: system->initialized is NULL");
        return -1;
    }

    ephaptic_coherence_config_t default_cfg;
    if (!config) {
        ephaptic_qmc_coherence_default_config(&default_cfg);
        config = &default_cfg;
    }

    memset(result, 0, sizeof(*result));

    // Set up context
    coherence_opt_ctx_t ctx;
    ctx.system = system;
    ctx.target = target;
    ctx.config = config;
    ctx.original_config = system->config;

    // Initial parameters: [coupling, sync_threshold, field_decay]
    float initial_params[3];
    initial_params[0] = system->config.kuramoto_coupling;
    initial_params[1] = system->config.sync_threshold;
    initial_params[2] = system->config.field_decay_constant;

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
        coherence_energy_fn,
        initial_params,
        3,
        &anneal_cfg,
        &ctx,
        &anneal_result
    );

    if (err != QMC_OK) {
        result->converged = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ephaptic_qmc_optimize_coherence: validation failed");
        return -1;
    }

    // Extract results
    result->opt_coupling = anneal_result.best_state[0];
    result->opt_sync_threshold = anneal_result.best_state[1];
    result->opt_field_decay = anneal_result.best_state[2];

    // Apply and measure
    apply_params_to_system(system, anneal_result.best_state, config);
    nimcp_ephaptic_reset(system);

    uint32_t num_steps = (uint32_t)(config->sim_duration_ms / config->sim_dt);
    for (uint32_t i = 0; i < num_steps; i++) {
        nimcp_ephaptic_synchronize(system, config->sim_dt);
    }

    nimcp_ephaptic_get_phase_coherence(system, &result->achieved_coherence);
    result->synced_neurons = system->synchronized_neurons;

    // Estimate frequency from phase evolution (simplified)
    result->achieved_frequency = 40.0f;  // Would need phase tracking for accurate estimate

    // Stats
    result->final_energy = anneal_result.final_energy;
    result->acceptance_rate = anneal_result.acceptance_rate;
    result->iterations_run = anneal_result.iterations_run;
    result->converged = (result->final_energy < 0.1f);

    qmc_anneal_result_free(&anneal_result);

    return 0;
}

int ephaptic_qmc_apply_coherence_result(
    nimcp_ephaptic_system_t* system,
    const ephaptic_coherence_result_t* result
) {
    if (!system || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_apply_coherence_result: required parameter is NULL (system, result)");
        return -1;
    }
    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_apply_coherence_result: system->initialized is NULL");
        return -1;
    }

    system->config.kuramoto_coupling = result->opt_coupling;
    system->config.sync_threshold = result->opt_sync_threshold;
    system->config.field_decay_constant = result->opt_field_decay;

    return 0;
}

int ephaptic_qmc_current_coherence(
    const nimcp_ephaptic_system_t* system,
    float* coherence
) {
    if (!system || !coherence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_current_coherence: required parameter is NULL (system, coherence)");
        return -1;
    }
    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_current_coherence: system->initialized is NULL");
        return -1;
    }

    return nimcp_ephaptic_get_phase_coherence(system, coherence);
}

//=============================================================================
// Quantum Walk API Implementation
//=============================================================================

int ephaptic_qmc_field_walk(
    const nimcp_ephaptic_system_t* system,
    uint32_t start_idx,
    uint32_t target_idx,
    const ephaptic_walk_config_t* config,
    ephaptic_walk_result_t* result
) {
    if (!system || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_field_walk: required parameter is NULL (system, result)");
        return -1;
    }
    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_field_walk: system->initialized is NULL");
        return -1;
    }
    if (start_idx >= system->neuron_count || target_idx >= system->neuron_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "ephaptic_qmc_field_walk: capacity exceeded");
        return -1;
    }

    ephaptic_walk_config_t default_cfg;
    if (!config) {
        ephaptic_qmc_walk_default_config(&default_cfg);
        config = &default_cfg;
    }

    memset(result, 0, sizeof(*result));

    // Build adjacency matrix from positions
    uint32_t n = system->neuron_count;
    uint8_t* adjacency = nimcp_calloc(n * n, sizeof(uint8_t));
    if (!adjacency) { LOG_ERROR("Failed to allocate adjacency matrix for quantum walk"); return -1; }

    // Connect neurons within field decay distance
    float max_dist = 3.0f / system->config.field_decay_constant;  // 3 decay lengths
    build_adjacency_matrix(system, adjacency, max_dist);

    // Configure quantum walk
    qmc_walk_config_t walk_cfg;
    walk_cfg.max_steps = config->max_steps;
    walk_cfg.mcts_iterations = config->mcts_iterations;
    walk_cfg.exploration_constant = config->exploration_constant;
    walk_cfg.adaptive_coin = config->adaptive_coin;
    walk_cfg.seed = config->seed;

    // Run quantum walk
    qmc_walk_result_t walk_result;
    qmc_result_t err = qmc_walk_mcts(
        adjacency,
        n,
        start_idx,
        target_idx,
        &walk_cfg,
        &walk_result
    );

    nimcp_free(adjacency);

    if (err != QMC_OK) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ephaptic_qmc_field_walk: validation failed");
        return -1;
    }

    // Transfer results
    result->target_reached = walk_result.target_reached;
    result->steps_taken = walk_result.steps_taken;
    result->target_probability = walk_result.target_probability;
    result->mean_hitting_time = walk_result.mean_hitting_time;
    result->num_nodes = n;

    // Compute propagation speed (mm per step)
    if (walk_result.steps_taken > 0) {
        float dx = system->neurons[target_idx].position[0] - system->neurons[start_idx].position[0];
        float dy = system->neurons[target_idx].position[1] - system->neurons[start_idx].position[1];
        float dz = system->neurons[target_idx].position[2] - system->neurons[start_idx].position[2];
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        result->propagation_speed = dist / (float)walk_result.steps_taken;
    }

    qmc_walk_result_free(&walk_result);

    return 0;
}

void ephaptic_qmc_walk_result_free(ephaptic_walk_result_t* result) {
    if (!result) return;

    if (result->amplitude_distribution) {
        nimcp_free(result->amplitude_distribution);
        result->amplitude_distribution = NULL;
    }
}

int ephaptic_qmc_propagation_speed(
    const nimcp_ephaptic_system_t* system,
    float* speed
) {
    if (!system || !speed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_propagation_speed: required parameter is NULL (system, speed)");
        return -1;
    }
    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_propagation_speed: system->initialized is NULL");
        return -1;
    }

    // Estimate from field decay and typical frequencies
    // Speed ~ wavelength / period
    // wavelength ~ 1 / field_decay_constant
    // period ~ 1 / 40 Hz (gamma)

    float wavelength = 1.0f / system->config.field_decay_constant;  // mm
    float freq = 40.0f;  // Hz (typical gamma)
    *speed = wavelength * freq / 1000.0f;  // mm/ms

    return 0;
}

//=============================================================================
// Collective Entropy API Implementation
//=============================================================================

int ephaptic_qmc_collective_entropy(
    const nimcp_ephaptic_system_t* system,
    const ephaptic_entropy_config_t* config,
    ephaptic_entropy_result_t* result
) {
    if (!system || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_collective_entropy: required parameter is NULL (system, result)");
        return -1;
    }
    if (!system->initialized || system->neuron_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_collective_entropy: system->initialized is NULL");
        return -1;
    }

    ephaptic_entropy_config_t default_cfg;
    if (!config) {
        ephaptic_qmc_entropy_default_config(&default_cfg);
        config = &default_cfg;
    }

    memset(result, 0, sizeof(*result));
    result->num_regions = config->num_regions;

    // Extract phases for entropy estimation
    float* phases = nimcp_calloc(system->neuron_count, sizeof(float));
    if (!phases) { LOG_ERROR("Failed to allocate phases array for entropy estimation"); return -1; }

    for (uint32_t i = 0; i < system->neuron_count; i++) {
        phases[i] = system->neurons[i].phase;
    }

    // Build phase histogram (discretize to bins)
    uint32_t num_bins = 36;  // 10-degree bins
    float* phase_probs = nimcp_calloc(num_bins, sizeof(float));
    if (!phase_probs) {
        LOG_ERROR("Failed to allocate phase probabilities array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate phase probabilities");
        nimcp_free(phases);
        return -1;
    }

    for (uint32_t i = 0; i < system->neuron_count; i++) {
        float normalized = fmodf(phases[i] + PI, 2.0f * PI);
        if (normalized < 0.0f) normalized += 2.0f * PI;
        uint32_t bin = (uint32_t)(normalized / (2.0f * PI) * (float)num_bins);
        if (bin >= num_bins) bin = num_bins - 1;
        phase_probs[bin] += 1.0f;
    }

    // Normalize
    float n = (float)system->neuron_count;
    for (uint32_t i = 0; i < num_bins; i++) {
        phase_probs[i] /= n;
    }

    // Estimate entropy using QMC
    qmc_entropy_config_t qmc_cfg;
    qmc_cfg.num_samples = config->num_samples;
    qmc_cfg.use_stratified = true;
    qmc_cfg.num_strata = 6;
    qmc_cfg.seed = config->seed;

    qmc_entropy_result_t entropy_result;
    qmc_result_t err = qmc_estimate_entropy(
        phase_probs,
        num_bins,
        &qmc_cfg,
        &entropy_result
    );

    if (err == QMC_OK) {
        result->phase_entropy = entropy_result.shannon_entropy;
    }

    // Field entropy from field magnitude distribution
    // (simplified: use field_potential histogram)
    result->field_entropy = result->phase_entropy * 0.8f;  // Correlated estimate

    // Spatial entropy based on field uniformity
    float r = sqrtf(
        system->order_parameter_real * system->order_parameter_real +
        system->order_parameter_imag * system->order_parameter_imag
    );
    // High r = low spatial entropy (ordered), low r = high entropy (disordered)
    result->spatial_entropy = (1.0f - r) * log2f((float)num_bins);

    // Collective information
    result->collective_info = result->phase_entropy + result->spatial_entropy;

    // Per-region entropies
    if (config->num_regions > 0) {
        result->region_entropies = nimcp_calloc(config->num_regions, sizeof(float));
        if (result->region_entropies) {
            // Divide neurons into regions and compute per-region entropy
            uint32_t per_region = system->neuron_count / config->num_regions;
            for (uint32_t r_idx = 0; r_idx < config->num_regions; r_idx++) {
                uint32_t start = r_idx * per_region;
                uint32_t end = (r_idx == config->num_regions - 1) ?
                               system->neuron_count : (r_idx + 1) * per_region;

                float region_coherence = compute_order_parameter(&phases[start], end - start);
                result->region_entropies[r_idx] = (1.0f - region_coherence) * log2f((float)num_bins);
            }
        }
    }

    nimcp_free(phases);
    nimcp_free(phase_probs);

    return 0;
}

void ephaptic_qmc_entropy_result_free(ephaptic_entropy_result_t* result) {
    if (!result) return;

    if (result->region_entropies) {
        nimcp_free(result->region_entropies);
        result->region_entropies = NULL;
    }
    if (result->mutual_info_matrix) {
        nimcp_free(result->mutual_info_matrix);
        result->mutual_info_matrix = NULL;
    }
}

int ephaptic_qmc_region_mutual_info(
    const nimcp_ephaptic_system_t* system,
    const uint32_t* region1,
    uint32_t count1,
    const uint32_t* region2,
    uint32_t count2,
    float* mi
) {
    if (!system || !region1 || !region2 || !mi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_region_mutual_info: required parameter is NULL (system, region1, region2, mi)");
        return -1;
    }
    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_region_mutual_info: system->initialized is NULL");
        return -1;
    }
    if (count1 == 0 || count2 == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ephaptic_qmc_region_mutual_info: count1 is zero");
        return -1;
    }

    // Extract phases for both regions
    float* phases1 = nimcp_calloc(count1, sizeof(float));
    float* phases2 = nimcp_calloc(count2, sizeof(float));
    if (!phases1 || !phases2) {
        LOG_ERROR("Failed to allocate phase arrays for mutual information");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate phase arrays");
        if (phases1) nimcp_free(phases1);
        if (phases2) nimcp_free(phases2);
        return -1;
    }

    for (uint32_t i = 0; i < count1; i++) {
        if (region1[i] < system->neuron_count) {
            phases1[i] = system->neurons[region1[i]].phase;
        }
    }
    for (uint32_t i = 0; i < count2; i++) {
        if (region2[i] < system->neuron_count) {
            phases2[i] = system->neurons[region2[i]].phase;
        }
    }

    // Compute coherences
    float r1 = compute_order_parameter(phases1, count1);
    float r2 = compute_order_parameter(phases2, count2);

    // Combine phases and compute joint coherence
    float* combined = nimcp_calloc(count1 + count2, sizeof(float));
    if (!combined) {
        LOG_ERROR("Failed to allocate combined phases array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate combined phases");
        nimcp_free(phases1);
        nimcp_free(phases2);
        return -1;
    }
    memcpy(combined, phases1, count1 * sizeof(float));
    memcpy(combined + count1, phases2, count2 * sizeof(float));

    float r_joint = compute_order_parameter(combined, count1 + count2);

    // Simplified MI estimate based on phase coherence
    // Higher joint coherence than individual -> mutual information
    *mi = (r1 + r2 - r_joint) * log2f(36.0f);  // Scale to bits
    if (*mi < 0.0f) *mi = 0.0f;

    nimcp_free(phases1);
    nimcp_free(phases2);
    nimcp_free(combined);

    return 0;
}

//=============================================================================
// Synchronization Pattern API Implementation
//=============================================================================

int ephaptic_qmc_discover_patterns(
    const nimcp_ephaptic_system_t* system,
    const ephaptic_pattern_config_t* config,
    ephaptic_pattern_result_t* result
) {
    if (!system || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_discover_patterns: required parameter is NULL (system, result)");
        return -1;
    }
    if (!system->initialized || system->neuron_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_discover_patterns: system->initialized is NULL");
        return -1;
    }

    ephaptic_pattern_config_t default_cfg;
    if (!config) {
        ephaptic_qmc_pattern_default_config(&default_cfg);
        config = &default_cfg;
    }

    memset(result, 0, sizeof(*result));

    // Simple pattern discovery: group by phase similarity
    // (Full MCTS would be more sophisticated)

    uint32_t n = system->neuron_count;
    bool* assigned = nimcp_calloc(n, sizeof(bool));
    ephaptic_sync_pattern_t* patterns = nimcp_calloc(config->max_patterns, sizeof(ephaptic_sync_pattern_t));
    if (!assigned || !patterns) {
        LOG_ERROR("Failed to allocate pattern discovery arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate pattern discovery arrays");
        if (assigned) nimcp_free(assigned);
        if (patterns) nimcp_free(patterns);
        return -1;
    }

    uint32_t num_patterns = 0;
    float phase_threshold = 0.5f;  // ~30 degrees

    for (uint32_t seed_idx = 0; seed_idx < n && num_patterns < config->max_patterns; seed_idx++) {
        if (assigned[seed_idx]) continue;

        // Start new pattern with this neuron
        uint32_t* pattern_neurons = nimcp_calloc(n, sizeof(uint32_t));
        if (!pattern_neurons) continue;

        uint32_t pattern_count = 0;
        float seed_phase = system->neurons[seed_idx].phase;

        // Find all neurons with similar phase
        for (uint32_t j = 0; j < n; j++) {
            if (assigned[j]) continue;

            float phase_diff = fabsf(system->neurons[j].phase - seed_phase);
            // Wrap around
            if (phase_diff > PI) phase_diff = 2.0f * PI - phase_diff;

            if (phase_diff < phase_threshold) {
                pattern_neurons[pattern_count++] = j;
            }
        }

        // Check if pattern meets minimum coherence
        if (pattern_count >= 2) {
            float* pattern_phases = nimcp_calloc(pattern_count, sizeof(float));
            if (pattern_phases) {
                for (uint32_t k = 0; k < pattern_count; k++) {
                    pattern_phases[k] = system->neurons[pattern_neurons[k]].phase;
                }

                float coherence = compute_order_parameter(pattern_phases, pattern_count);

                if (coherence >= config->min_coherence) {
                    // Accept pattern
                    patterns[num_patterns].neuron_ids = nimcp_calloc(pattern_count, sizeof(uint32_t));
                    if (patterns[num_patterns].neuron_ids) {
                        memcpy(patterns[num_patterns].neuron_ids, pattern_neurons, pattern_count * sizeof(uint32_t));
                        patterns[num_patterns].neuron_count = pattern_count;
                        patterns[num_patterns].coherence = coherence;
                        patterns[num_patterns].frequency = 40.0f;  // Estimated
                        patterns[num_patterns].phase_offset = compute_mean_phase(pattern_phases, pattern_count);

                        // Mark neurons as assigned
                        for (uint32_t k = 0; k < pattern_count; k++) {
                            assigned[pattern_neurons[k]] = true;
                        }

                        num_patterns++;
                    }
                }
                nimcp_free(pattern_phases);
            }
        }

        nimcp_free(pattern_neurons);
    }

    // Fill result
    result->patterns = patterns;
    result->num_patterns = num_patterns;

    // Compute total coherence
    float coherence;
    nimcp_ephaptic_get_phase_coherence(system, &coherence);
    result->total_coherence = coherence;

    // Coverage
    uint32_t covered = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (assigned[i]) covered++;
    }
    result->pattern_coverage = (float)covered / (float)n;

    nimcp_free(assigned);

    return 0;
}

void ephaptic_qmc_pattern_result_free(ephaptic_pattern_result_t* result) {
    if (!result) return;

    if (result->patterns) {
        for (uint32_t i = 0; i < result->num_patterns; i++) {
            if (result->patterns[i].neuron_ids) {
                nimcp_free(result->patterns[i].neuron_ids);
            }
        }
        nimcp_free(result->patterns);
        result->patterns = NULL;
    }
}

int ephaptic_qmc_classify_neuron(
    const nimcp_ephaptic_system_t* system,
    uint32_t neuron_idx,
    const ephaptic_sync_pattern_t* patterns,
    uint32_t num_patterns,
    int32_t* best_pattern,
    float* match_score
) {
    if (!system || !patterns || !best_pattern || !match_score) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_classify_neuron: required parameter is NULL (system, patterns, best_pattern, match_score)");
        return -1;
    }
    if (!system->initialized || neuron_idx >= system->neuron_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_classify_neuron: system->initialized is NULL");
        return -1;
    }

    float neuron_phase = system->neurons[neuron_idx].phase;
    *best_pattern = -1;
    *match_score = 0.0f;

    for (uint32_t p = 0; p < num_patterns; p++) {
        // Compute phase difference to pattern mean phase
        float phase_diff = fabsf(neuron_phase - patterns[p].phase_offset);
        if (phase_diff > PI) phase_diff = 2.0f * PI - phase_diff;

        // Score based on phase proximity (1 = perfect match, 0 = opposite phase)
        float score = 1.0f - phase_diff / PI;

        if (score > *match_score) {
            *match_score = score;
            *best_pattern = (int32_t)p;
        }
    }

    return 0;
}

//=============================================================================
// Field Analysis API Implementation
//=============================================================================

int ephaptic_qmc_correlation_length(
    const nimcp_ephaptic_system_t* system,
    float* corr_length
) {
    if (!system || !corr_length) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_correlation_length: required parameter is NULL (system, corr_length)");
        return -1;
    }
    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_correlation_length: system->initialized is NULL");
        return -1;
    }

    // Correlation length ~ 1 / field_decay_constant
    // Modified by coherence (higher coherence = longer correlation)
    float r = sqrtf(
        system->order_parameter_real * system->order_parameter_real +
        system->order_parameter_imag * system->order_parameter_imag
    );

    float base_length = 1.0f / system->config.field_decay_constant;
    *corr_length = base_length * (1.0f + r);  // Coherence extends correlation

    return 0;
}

int ephaptic_qmc_field_capacity(
    const nimcp_ephaptic_system_t* system,
    float* capacity
) {
    if (!system || !capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_field_capacity: required parameter is NULL (system, capacity)");
        return -1;
    }
    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_qmc_field_capacity: system->initialized is NULL");
        return -1;
    }

    // Information capacity ~ N * log2(resolution) * coherence
    // where resolution depends on field strength discretization

    uint32_t n = system->neuron_count;
    float r = sqrtf(
        system->order_parameter_real * system->order_parameter_real +
        system->order_parameter_imag * system->order_parameter_imag
    );

    // Assume 36 phase bins (10 degrees each) and field can carry ~log2(36) bits per neuron
    float bits_per_neuron = log2f(36.0f);

    // Effective capacity reduced by lack of coherence (noise)
    *capacity = (float)n * bits_per_neuron * r;

    return 0;
}
