#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_hh_quantum_bridge.c - Hodgkin-Huxley Quantum Monte Carlo Implementation
//=============================================================================

#include "physics/bridges/nimcp_hh_quantum_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hh_quantum_bridge)

#define LOG_MODULE "HH_QUANTUM_BRIDGE"


//=============================================================================
// Internal Context Structures
//=============================================================================

/**
 * @brief Context for HH parameter optimization energy function
 */
typedef struct {
    nimcp_hh_neuron_t* neuron;
    nimcp_hh_config_t original_config;
    const hh_qmc_target_t* target;
    const hh_qmc_config_t* qmc_config;
} hh_optimization_ctx_t;

/**
 * @brief Context for f-I curve fitting
 */
typedef struct {
    nimcp_hh_neuron_t* neuron;
    nimcp_hh_config_t original_config;
    const float* currents;
    const float* target_rates;
    uint32_t num_points;
    float eval_duration_ms;
    float eval_dt;
} hh_fi_fit_ctx_t;

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Simulate neuron and measure firing rate
 */
static float measure_firing_rate(
    nimcp_hh_neuron_t* neuron,
    float I_ext,
    float duration_ms,
    float dt
) {
    nimcp_hh_neuron_reset(neuron);

    uint32_t spike_count = 0;
    uint32_t num_steps = (uint32_t)(duration_ms / dt);

    for (uint32_t i = 0; i < num_steps; i++) {
        nimcp_hh_neuron_update(neuron, I_ext, dt);
        if (neuron->spiked) {
            spike_count++;
        }
    }

    // Convert to Hz: spikes / seconds
    return (float)spike_count * 1000.0f / duration_ms;
}

/**
 * @brief Apply parameters from optimization state to neuron
 */
static void apply_params_to_neuron(
    nimcp_hh_neuron_t* neuron,
    const float* params,
    const hh_qmc_config_t* config
) {
    // Basic parameters: g_Na, g_K, E_Na, E_K
    neuron->channels[NIMCP_ION_CHANNEL_NA].g_max =
        nimcp_clampf(params[0], config->g_Na_min, config->g_Na_max);
    neuron->channels[NIMCP_ION_CHANNEL_K].g_max =
        nimcp_clampf(params[1], config->g_K_min, config->g_K_max);
    neuron->channels[NIMCP_ION_CHANNEL_NA].E_rev =
        nimcp_clampf(params[2], config->E_Na_min, config->E_Na_max);
    neuron->channels[NIMCP_ION_CHANNEL_K].E_rev =
        nimcp_clampf(params[3], config->E_K_min, config->E_K_max);

    // Extended parameters if applicable
    if (config->optimize_extended) {
        neuron->channels[NIMCP_ION_CHANNEL_CA_L].g_max =
            nimcp_clampf(params[4], config->g_Ca_min, config->g_Ca_max);
        neuron->channels[NIMCP_ION_CHANNEL_CA_L].E_rev =
            nimcp_clampf(params[5], config->E_Ca_min, config->E_Ca_max);
        neuron->channels[NIMCP_ION_CHANNEL_LEAK].g_max =
            nimcp_clampf(params[6], config->g_L_min, config->g_L_max);
        neuron->channels[NIMCP_ION_CHANNEL_LEAK].E_rev =
            nimcp_clampf(params[7], config->E_L_min, config->E_L_max);
    }
}

/**
 * @brief Energy function for HH parameter optimization
 */
static float hh_param_energy(
    const float* params,
    uint32_t dim,
    void* user_data
) {
    hh_optimization_ctx_t* ctx = (hh_optimization_ctx_t*)user_data;
    (void)dim;

    // Apply parameters
    apply_params_to_neuron(ctx->neuron, params, ctx->qmc_config);

    // Measure firing rate
    float rate = measure_firing_rate(
        ctx->neuron,
        ctx->qmc_config->eval_current,
        ctx->qmc_config->eval_duration_ms,
        ctx->qmc_config->eval_dt
    );

    // Compute error
    float rate_error = fabsf(rate - ctx->target->target_firing_rate);

    // Compute rheobase (simplified: binary search for threshold)
    float rheobase_error = 0.0f;
    if (ctx->target->weight_rheobase > 0.0f) {
        float rheobase = 0.0f;
        nimcp_hh_compute_rheobase(ctx->neuron, &rheobase);
        rheobase_error = fabsf(rheobase - ctx->target->target_rheobase);
    }

    // Combined error (weighted sum)
    float energy = ctx->target->weight_rate * rate_error
                 + ctx->target->weight_rheobase * rheobase_error;

    return energy;
}

/**
 * @brief Energy function for f-I curve fitting
 */
static float hh_fi_energy(
    const float* params,
    uint32_t dim,
    void* user_data
) {
    hh_fi_fit_ctx_t* ctx = (hh_fi_fit_ctx_t*)user_data;
    (void)dim;

    // Restore original config
    ctx->neuron->config = ctx->original_config;

    // Apply test parameters (basic: g_Na, g_K, E_Na, E_K)
    ctx->neuron->channels[NIMCP_ION_CHANNEL_NA].g_max = params[0];
    ctx->neuron->channels[NIMCP_ION_CHANNEL_K].g_max = params[1];
    ctx->neuron->channels[NIMCP_ION_CHANNEL_NA].E_rev = params[2];
    ctx->neuron->channels[NIMCP_ION_CHANNEL_K].E_rev = params[3];

    // Compute f-I curve and compare to target
    float total_error = 0.0f;
    for (uint32_t i = 0; i < ctx->num_points; i++) {
        float rate = measure_firing_rate(
            ctx->neuron,
            ctx->currents[i],
            ctx->eval_duration_ms,
            ctx->eval_dt
        );
        float error = rate - ctx->target_rates[i];
        total_error += error * error;  // MSE
    }

    return total_error / (float)ctx->num_points;
}

//=============================================================================
// Configuration API Implementation
//=============================================================================

int hh_qmc_default_config(hh_qmc_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    // Annealing parameters
    config->initial_temp = HH_QMC_DEFAULT_INITIAL_TEMP;
    config->final_temp = HH_QMC_DEFAULT_FINAL_TEMP;
    config->num_iterations = HH_QMC_DEFAULT_ITERATIONS;
    config->quantum_strength = HH_QMC_DEFAULT_QUANTUM_STRENGTH;

    // Basic parameter bounds (physiological ranges)
    config->g_Na_min = 50.0f;   config->g_Na_max = 200.0f;
    config->g_K_min = 10.0f;    config->g_K_max = 80.0f;
    config->E_Na_min = 40.0f;   config->E_Na_max = 70.0f;
    config->E_K_min = -90.0f;   config->E_K_max = -60.0f;

    // Extended parameter bounds
    config->optimize_extended = false;
    config->g_Ca_min = 0.0f;    config->g_Ca_max = 10.0f;
    config->E_Ca_min = 100.0f;  config->E_Ca_max = 140.0f;
    config->g_L_min = 0.1f;     config->g_L_max = 1.0f;
    config->E_L_min = -70.0f;   config->E_L_max = -50.0f;

    // Evaluation parameters
    config->eval_duration_ms = 500.0f;
    config->eval_dt = 0.025f;
    config->eval_current = 10.0f;

    config->seed = 0;

    return 0;
}

int hh_qmc_default_target(hh_qmc_target_t* target) {
    if (!target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_default_target: target is NULL");
        return -1;
    }

    memset(target, 0, sizeof(*target));

    target->target_firing_rate = 20.0f;     // 20 Hz
    target->target_threshold = 10.0f;       // 10 uA/cm^2
    target->target_spike_width = 1.0f;      // 1 ms
    target->target_rheobase = 5.0f;         // 5 uA/cm^2

    // Default weights
    target->weight_rate = 1.0f;
    target->weight_threshold = 0.5f;
    target->weight_spike_width = 0.3f;
    target->weight_rheobase = 0.5f;

    return 0;
}

int hh_entropy_default_config(hh_entropy_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_entropy_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->num_samples = 10000;
    config->bin_width_ms = 1.0f;
    config->num_bins = 100;
    config->use_stratified = true;
    config->seed = 0;

    return 0;
}

int hh_stochastic_default_config(hh_stochastic_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_stochastic_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->num_channels = 1000;            // 1000 channels per type
    config->channel_conductance = 20.0f;    // 20 pS single channel
    config->simulate_na = true;
    config->simulate_k = true;
    config->num_trajectories = 100;
    config->seed = 0;

    return 0;
}

//=============================================================================
// Parameter Optimization API Implementation
//=============================================================================

int hh_qmc_optimize_parameters(
    nimcp_hh_neuron_t* neuron,
    const hh_qmc_target_t* target,
    const hh_qmc_config_t* config,
    hh_qmc_result_t* result
) {
    if (!neuron || !target || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_optimize_parameters: required parameter is NULL (neuron, target, result)");
        return -1;
    }
    if (!neuron->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_optimize_parameters: neuron->initialized is NULL");
        return -1;
    }

    // Use default config if not provided
    hh_qmc_config_t default_cfg;
    if (!config) {
        hh_qmc_default_config(&default_cfg);
        config = &default_cfg;
    }

    // Clear result
    memset(result, 0, sizeof(*result));

    // Set up optimization context
    hh_optimization_ctx_t ctx;
    ctx.neuron = neuron;
    ctx.original_config = neuron->config;
    ctx.target = target;
    ctx.qmc_config = config;

    // Determine dimensionality
    uint32_t dim = config->optimize_extended ?
                   HH_QMC_PARAM_DIM_EXTENDED : HH_QMC_PARAM_DIM_BASIC;

    // Initial state from current neuron parameters
    float* initial_state = nimcp_calloc(dim, sizeof(float));
    if (!initial_state) { LOG_ERROR("Failed to allocate HH optimization initial state"); return -1; }

    initial_state[0] = neuron->channels[NIMCP_ION_CHANNEL_NA].g_max;
    initial_state[1] = neuron->channels[NIMCP_ION_CHANNEL_K].g_max;
    initial_state[2] = neuron->channels[NIMCP_ION_CHANNEL_NA].E_rev;
    initial_state[3] = neuron->channels[NIMCP_ION_CHANNEL_K].E_rev;

    if (config->optimize_extended) {
        initial_state[4] = neuron->channels[NIMCP_ION_CHANNEL_CA_L].g_max;
        initial_state[5] = neuron->channels[NIMCP_ION_CHANNEL_CA_L].E_rev;
        initial_state[6] = neuron->channels[NIMCP_ION_CHANNEL_LEAK].g_max;
        initial_state[7] = neuron->channels[NIMCP_ION_CHANNEL_LEAK].E_rev;
    }

    // Configure annealing
    qmc_anneal_config_t anneal_cfg = qmc_anneal_default_config();
    anneal_cfg.initial_temp = config->initial_temp;
    anneal_cfg.final_temp = config->final_temp;
    anneal_cfg.num_iterations = config->num_iterations;
    anneal_cfg.quantum_strength = config->quantum_strength;
    anneal_cfg.strategy = QMC_PROPOSAL_ADAPTIVE;
    anneal_cfg.target_acceptance = 0.234f;
    anneal_cfg.seed = config->seed;

    // Run adaptive annealing
    qmc_anneal_result_t anneal_result;
    qmc_result_t qmc_err = qmc_adaptive_anneal(
        hh_param_energy,
        initial_state,
        dim,
        &anneal_cfg,
        &ctx,
        &anneal_result
    );

    nimcp_free(initial_state);

    if (qmc_err != QMC_OK) {
        result->converged = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hh_qmc_optimize_parameters: validation failed");
        return -1;
    }

    // Extract optimized parameters
    result->opt_g_Na = anneal_result.best_state[0];
    result->opt_g_K = anneal_result.best_state[1];
    result->opt_E_Na = anneal_result.best_state[2];
    result->opt_E_K = anneal_result.best_state[3];

    if (config->optimize_extended && dim == HH_QMC_PARAM_DIM_EXTENDED) {
        result->opt_g_Ca = anneal_result.best_state[4];
        result->opt_E_Ca = anneal_result.best_state[5];
        result->opt_g_L = anneal_result.best_state[6];
        result->opt_E_L = anneal_result.best_state[7];
    }

    // Apply optimized parameters
    apply_params_to_neuron(neuron, anneal_result.best_state, config);

    // Measure achieved behavior
    result->achieved_firing_rate = measure_firing_rate(
        neuron,
        config->eval_current,
        config->eval_duration_ms,
        config->eval_dt
    );

    nimcp_hh_compute_rheobase(neuron, &result->achieved_rheobase);

    // Compute errors
    result->rate_error = fabsf(result->achieved_firing_rate - target->target_firing_rate);
    result->rheobase_error = fabsf(result->achieved_rheobase - target->target_rheobase);
    result->total_error = anneal_result.final_energy;

    // Statistics
    result->final_energy = anneal_result.final_energy;
    result->acceptance_rate = anneal_result.acceptance_rate;
    result->iterations_run = anneal_result.iterations_run;
    result->tunneling_events = anneal_result.tunneling_events;

    // Check convergence (error below threshold)
    result->converged = (result->total_error < 5.0f);

    // Free annealing result
    qmc_anneal_result_free(&anneal_result);

    return 0;
}

int hh_qmc_fit_fi_curve(
    nimcp_hh_neuron_t* neuron,
    const float* currents,
    const float* target_rates,
    uint32_t num_points,
    const hh_qmc_config_t* config,
    hh_qmc_result_t* result
) {
    if (!neuron || !currents || !target_rates || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_fit_fi_curve: required parameter is NULL (neuron, currents, target_rates, result)");
        return -1;
    }
    if (!neuron->initialized || num_points == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_fit_fi_curve: neuron->initialized is NULL");
        return -1;
    }

    // Use default config if not provided
    hh_qmc_config_t default_cfg;
    if (!config) {
        hh_qmc_default_config(&default_cfg);
        config = &default_cfg;
    }

    memset(result, 0, sizeof(*result));

    // Set up context
    hh_fi_fit_ctx_t ctx;
    ctx.neuron = neuron;
    ctx.original_config = neuron->config;
    ctx.currents = currents;
    ctx.target_rates = target_rates;
    ctx.num_points = num_points;
    ctx.eval_duration_ms = config->eval_duration_ms;
    ctx.eval_dt = config->eval_dt;

    // Initial state
    float initial_state[HH_QMC_PARAM_DIM_BASIC];
    initial_state[0] = neuron->channels[NIMCP_ION_CHANNEL_NA].g_max;
    initial_state[1] = neuron->channels[NIMCP_ION_CHANNEL_K].g_max;
    initial_state[2] = neuron->channels[NIMCP_ION_CHANNEL_NA].E_rev;
    initial_state[3] = neuron->channels[NIMCP_ION_CHANNEL_K].E_rev;

    // Configure annealing
    qmc_anneal_config_t anneal_cfg = qmc_anneal_default_config();
    anneal_cfg.initial_temp = config->initial_temp;
    anneal_cfg.final_temp = config->final_temp;
    anneal_cfg.num_iterations = config->num_iterations;
    anneal_cfg.quantum_strength = config->quantum_strength;
    anneal_cfg.seed = config->seed;

    // Run annealing
    qmc_anneal_result_t anneal_result;
    qmc_result_t err = qmc_adaptive_anneal(
        hh_fi_energy,
        initial_state,
        HH_QMC_PARAM_DIM_BASIC,
        &anneal_cfg,
        &ctx,
        &anneal_result
    );

    if (err != QMC_OK) {
        result->converged = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hh_qmc_fit_fi_curve: validation failed");
        return -1;
    }

    // Extract results
    result->opt_g_Na = anneal_result.best_state[0];
    result->opt_g_K = anneal_result.best_state[1];
    result->opt_E_Na = anneal_result.best_state[2];
    result->opt_E_K = anneal_result.best_state[3];

    result->final_energy = anneal_result.final_energy;
    result->acceptance_rate = anneal_result.acceptance_rate;
    result->iterations_run = anneal_result.iterations_run;
    result->total_error = sqrtf(anneal_result.final_energy);  // RMSE

    result->converged = (result->total_error < 5.0f);

    qmc_anneal_result_free(&anneal_result);

    return 0;
}

int hh_qmc_apply_result(
    nimcp_hh_neuron_t* neuron,
    const hh_qmc_result_t* result
) {
    if (!neuron || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_apply_result: required parameter is NULL (neuron, result)");
        return -1;
    }
    if (!neuron->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_apply_result: neuron->initialized is NULL");
        return -1;
    }

    neuron->channels[NIMCP_ION_CHANNEL_NA].g_max = result->opt_g_Na;
    neuron->channels[NIMCP_ION_CHANNEL_K].g_max = result->opt_g_K;
    neuron->channels[NIMCP_ION_CHANNEL_NA].E_rev = result->opt_E_Na;
    neuron->channels[NIMCP_ION_CHANNEL_K].E_rev = result->opt_E_K;

    if (result->opt_g_Ca > 0.0f) {
        neuron->channels[NIMCP_ION_CHANNEL_CA_L].g_max = result->opt_g_Ca;
        neuron->channels[NIMCP_ION_CHANNEL_CA_L].E_rev = result->opt_E_Ca;
    }
    if (result->opt_g_L > 0.0f) {
        neuron->channels[NIMCP_ION_CHANNEL_LEAK].g_max = result->opt_g_L;
        neuron->channels[NIMCP_ION_CHANNEL_LEAK].E_rev = result->opt_E_L;
    }

    return 0;
}

//=============================================================================
// Entropy Analysis API Implementation
//=============================================================================

int hh_qmc_spike_train_entropy(
    nimcp_hh_neuron_t* neuron,
    const float* stimulus,
    uint32_t stimulus_len,
    float dt,
    const hh_entropy_config_t* config,
    hh_entropy_result_t* result
) {
    if (!neuron || !stimulus || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_spike_train_entropy: required parameter is NULL (neuron, stimulus, result)");
        return -1;
    }
    if (!neuron->initialized || stimulus_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_spike_train_entropy: neuron->initialized is NULL");
        return -1;
    }

    hh_entropy_config_t default_cfg;
    if (!config) {
        hh_entropy_default_config(&default_cfg);
        config = &default_cfg;
    }

    memset(result, 0, sizeof(*result));

    // Collect spike times
    nimcp_hh_neuron_reset(neuron);
    float* spike_times = nimcp_calloc(stimulus_len, sizeof(float));
    if (!spike_times) { LOG_ERROR("Failed to allocate spike times array for entropy analysis"); return -1; }

    uint32_t num_spikes = 0;
    float time = 0.0f;

    for (uint32_t i = 0; i < stimulus_len; i++) {
        nimcp_hh_neuron_update(neuron, stimulus[i], dt);
        if (neuron->spiked && num_spikes < stimulus_len) {
            spike_times[num_spikes++] = time;
        }
        time += dt;
    }

    if (num_spikes < 2) {
        // Not enough spikes for ISI analysis
        nimcp_free(spike_times);
        result->isi_entropy = 0.0f;
        result->spike_count_entropy = 0.0f;
        return 0;
    }

    // Compute ISI histogram
    float* isis = nimcp_calloc(num_spikes - 1, sizeof(float));
    float* isi_probs = nimcp_calloc(config->num_bins, sizeof(float));
    if (!isis || !isi_probs) {
        LOG_ERROR("Failed to allocate ISI arrays for entropy analysis");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ISI arrays");
        nimcp_free(spike_times);
        if (isis) nimcp_free(isis);
        if (isi_probs) nimcp_free(isi_probs);
        return -1;
    }

    for (uint32_t i = 0; i < num_spikes - 1; i++) {
        isis[i] = spike_times[i + 1] - spike_times[i];
    }

    // Build histogram
    float max_isi = config->bin_width_ms * config->num_bins;
    for (uint32_t i = 0; i < num_spikes - 1; i++) {
        uint32_t bin = (uint32_t)(isis[i] / config->bin_width_ms);
        if (bin >= config->num_bins) bin = config->num_bins - 1;
        isi_probs[bin] += 1.0f;
    }

    // Normalize
    float total = (float)(num_spikes - 1);
    for (uint32_t i = 0; i < config->num_bins; i++) {
        isi_probs[i] /= total;
    }

    // Estimate entropy using QMC
    qmc_entropy_config_t qmc_cfg;
    qmc_cfg.num_samples = config->num_samples;
    qmc_cfg.use_stratified = config->use_stratified;
    qmc_cfg.num_strata = 10;
    qmc_cfg.seed = config->seed;

    qmc_entropy_result_t entropy_result;
    qmc_result_t err = qmc_estimate_entropy(
        isi_probs,
        config->num_bins,
        &qmc_cfg,
        &entropy_result
    );

    if (err == QMC_OK) {
        result->isi_entropy = entropy_result.shannon_entropy;
        result->variance = entropy_result.variance;
        result->std_error = entropy_result.std_error;
    }

    // Coding efficiency: bits per spike
    float total_time_s = time / 1000.0f;
    float firing_rate = (float)num_spikes / total_time_s;
    if (firing_rate > 0.0f) {
        result->coding_efficiency = result->isi_entropy / firing_rate;
    }

    nimcp_free(spike_times);
    nimcp_free(isis);
    nimcp_free(isi_probs);

    return 0;
}

int hh_qmc_mutual_information(
    nimcp_hh_neuron_t* neuron,
    const float** stimuli,
    uint32_t num_stimuli,
    uint32_t stim_len,
    uint32_t num_trials,
    float dt,
    const hh_entropy_config_t* config,
    float* mutual_info
) {
    if (!neuron || !stimuli || !mutual_info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_mutual_information: required parameter is NULL (neuron, stimuli, mutual_info)");
        return -1;
    }
    if (!neuron->initialized || num_stimuli == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_mutual_information: neuron->initialized is NULL");
        return -1;
    }

    (void)config;  // Not used in simplified implementation

    // Simplified mutual information estimate:
    // I(S;R) = H(R) - H(R|S)
    // where R is response, S is stimulus

    // Collect response distributions for each stimulus
    float total_response_entropy = 0.0f;
    float* response_counts = nimcp_calloc(num_stimuli, sizeof(float));
    if (!response_counts) { LOG_ERROR("Failed to allocate response counts for mutual information"); return -1; }

    for (uint32_t s = 0; s < num_stimuli; s++) {
        uint32_t total_spikes = 0;
        for (uint32_t t = 0; t < num_trials; t++) {
            nimcp_hh_neuron_reset(neuron);
            uint32_t spike_count = 0;
            for (uint32_t i = 0; i < stim_len; i++) {
                nimcp_hh_neuron_update(neuron, stimuli[s][i], dt);
                if (neuron->spiked) spike_count++;
            }
            total_spikes += spike_count;
        }
        response_counts[s] = (float)total_spikes / (float)num_trials;
    }

    // Compute marginal response entropy
    float total_count = 0.0f;
    for (uint32_t s = 0; s < num_stimuli; s++) {
        total_count += response_counts[s];
    }

    float marginal_entropy = 0.0f;
    if (total_count > 0.0f) {
        for (uint32_t s = 0; s < num_stimuli; s++) {
            float p = response_counts[s] / total_count;
            if (p > 0.0f) {
                marginal_entropy -= p * log2f(p);
            }
        }
    }

    *mutual_info = marginal_entropy;  // Simplified estimate

    nimcp_free(response_counts);
    return 0;
}

//=============================================================================
// Stochastic Channel Simulation API Implementation
//=============================================================================

int hh_qmc_stochastic_simulation(
    nimcp_hh_neuron_t* neuron,
    float I_ext,
    float duration_ms,
    float dt,
    const hh_stochastic_config_t* config,
    hh_stochastic_result_t* result
) {
    if (!neuron || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_stochastic_simulation: required parameter is NULL (neuron, result)");
        return -1;
    }
    if (!neuron->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_stochastic_simulation: neuron->initialized is NULL");
        return -1;
    }

    hh_stochastic_config_t default_cfg;
    if (!config) {
        hh_stochastic_default_config(&default_cfg);
        config = &default_cfg;
    }

    memset(result, 0, sizeof(*result));

    uint32_t num_steps = (uint32_t)(duration_ms / dt);

    // Run multiple trajectories and compute variance
    float* voltages = nimcp_calloc(config->num_trajectories, sizeof(float));
    float* spike_times = nimcp_calloc(config->num_trajectories, sizeof(float));
    if (!voltages || !spike_times) {
        LOG_ERROR("Failed to allocate trajectory arrays for stochastic simulation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate trajectory arrays");
        if (voltages) nimcp_free(voltages);
        if (spike_times) nimcp_free(spike_times);
        return -1;
    }

    for (uint32_t traj = 0; traj < config->num_trajectories; traj++) {
        nimcp_hh_neuron_reset(neuron);

        float first_spike_time = -1.0f;
        float time = 0.0f;

        for (uint32_t i = 0; i < num_steps; i++) {
            // Add stochastic noise to conductances
            float noise_na = 0.0f, noise_k = 0.0f;
            if (config->simulate_na && config->num_channels > 0) {
                // Simplified channel noise: sqrt(g * (1-open) * open / N)
                float g_na = neuron->channels[NIMCP_ION_CHANNEL_NA].g_current;
                float g_max = neuron->channels[NIMCP_ION_CHANNEL_NA].g_max;
                if (g_max > 0.0f) {
                    float open_frac = g_na / g_max;
                    float var = open_frac * (1.0f - open_frac) / (float)config->num_channels;
                    // Simple random (would use proper RNG in production)
                    noise_na = sqrtf(var) * ((float)(nimcp_tl_rand() % 1000) / 500.0f - 1.0f);
                }
            }

            // Temporarily modulate conductance
            float orig_g_na = neuron->channels[NIMCP_ION_CHANNEL_NA].g_max;
            neuron->channels[NIMCP_ION_CHANNEL_NA].g_max *= (1.0f + noise_na);

            nimcp_hh_neuron_update(neuron, I_ext, dt);

            neuron->channels[NIMCP_ION_CHANNEL_NA].g_max = orig_g_na;

            if (neuron->spiked && first_spike_time < 0.0f) {
                first_spike_time = time;
            }
            time += dt;
        }

        voltages[traj] = neuron->V;
        spike_times[traj] = first_spike_time;
    }

    // Compute statistics
    float v_sum = 0.0f, v_sum2 = 0.0f;
    float st_sum = 0.0f, st_sum2 = 0.0f;
    uint32_t st_count = 0;

    for (uint32_t i = 0; i < config->num_trajectories; i++) {
        v_sum += voltages[i];
        v_sum2 += voltages[i] * voltages[i];
        if (spike_times[i] >= 0.0f) {
            st_sum += spike_times[i];
            st_sum2 += spike_times[i] * spike_times[i];
            st_count++;
        }
    }

    float n = (float)config->num_trajectories;
    result->voltage_variance = (v_sum2 / n) - (v_sum / n) * (v_sum / n);

    if (st_count > 1) {
        float st_n = (float)st_count;
        float st_mean = st_sum / st_n;
        result->spike_time_jitter = sqrtf((st_sum2 / st_n) - st_mean * st_mean);
    }

    nimcp_free(voltages);
    nimcp_free(spike_times);

    return 0;
}

int hh_qmc_channel_noise_variance(
    nimcp_hh_neuron_t* neuron,
    float I_ext,
    const hh_stochastic_config_t* config,
    float* variance
) {
    if (!neuron || !variance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_channel_noise_variance: required parameter is NULL (neuron, variance)");
        return -1;
    }

    hh_stochastic_result_t result;
    int err = hh_qmc_stochastic_simulation(
        neuron, I_ext, 100.0f, 0.025f, config, &result
    );

    if (err == 0) {
        *variance = result.voltage_variance;
    }

    return err;
}

//=============================================================================
// Population Analysis API Implementation
//=============================================================================

int hh_qmc_population_coherence(
    const nimcp_hh_population_t* population,
    float* coherence,
    float* entropy
) {
    if (!population || !coherence || !entropy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_population_coherence: required parameter is NULL (population, coherence, entropy)");
        return -1;
    }
    if (!population->initialized || population->count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hh_qmc_population_coherence: population->initialized is NULL");
        return -1;
    }

    // Compute phase coherence from spike times (Kuramoto-like order parameter)
    float sum_cos = 0.0f, sum_sin = 0.0f;

    for (uint32_t i = 0; i < population->count; i++) {
        // Use last spike time as phase proxy
        float phase = population->neurons[i].last_spike_time * 0.01f;  // Scale
        sum_cos += cosf(phase);
        sum_sin += sinf(phase);
    }

    float n = (float)population->count;
    float r = sqrtf(sum_cos * sum_cos + sum_sin * sum_sin) / n;
    *coherence = r;

    // Compute voltage distribution entropy
    float* v_probs = nimcp_calloc(100, sizeof(float));
    if (!v_probs) { LOG_ERROR("Failed to allocate voltage probability array"); return -1; }

    float v_min = -100.0f, v_max = 50.0f;
    float bin_width = (v_max - v_min) / 100.0f;

    for (uint32_t i = 0; i < population->count; i++) {
        int bin = (int)((population->neurons[i].V - v_min) / bin_width);
        if (bin < 0) bin = 0;
        if (bin >= 100) bin = 99;
        v_probs[bin] += 1.0f;
    }

    // Normalize and compute entropy
    for (int i = 0; i < 100; i++) {
        v_probs[i] /= n;
    }

    float H = 0.0f;
    for (int i = 0; i < 100; i++) {
        if (v_probs[i] > 0.0f) {
            H -= v_probs[i] * log2f(v_probs[i]);
        }
    }

    *entropy = H;

    nimcp_free(v_probs);
    return 0;
}
