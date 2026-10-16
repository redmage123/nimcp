/**
 * @file nimcp_quantum_monte_carlo.c
 * @brief Quantum Monte Carlo Integration Module Implementation
 */

#include "utils/quantum/nimcp_quantum_monte_carlo.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(quantum_monte_carlo)

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define QMC_EPSILON 1e-10f
#define QMC_LOG_EPSILON -23.0259f  /* log(1e-10) */

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

void qmc_amplitudes_to_probabilities(
    const float* amplitudes,
    float* probabilities,
    uint32_t num_states
) {
    if (!amplitudes || !probabilities || num_states == 0) return;

    for (uint32_t i = 0; i < num_states; i++) {
        probabilities[i] = amplitudes[i] * amplitudes[i];
    }
}

float qmc_normalize_distribution(
    float* probabilities,
    uint32_t num_states
) {
    if (!probabilities || num_states == 0) return 0.0f;

    float sum = 0.0f;
    for (uint32_t i = 0; i < num_states; i++) {
        sum += probabilities[i];
    }

    if (sum > QMC_EPSILON) {
        float inv_sum = 1.0f / sum;
        for (uint32_t i = 0; i < num_states; i++) {
            probabilities[i] *= inv_sum;
        }
    }

    return sum;
}

float qmc_kl_divergence(
    const float* p,
    const float* q,
    uint32_t num_states
) {
    if (!p || !q || num_states == 0) return 0.0f;

    float kl = 0.0f;
    for (uint32_t i = 0; i < num_states; i++) {
        if (p[i] > QMC_EPSILON && q[i] > QMC_EPSILON) {
            kl += p[i] * logf(p[i] / q[i]);
        }
    }

    return kl;
}

float qmc_fidelity(
    const float* amplitudes1,
    const float* amplitudes2,
    uint32_t num_states
) {
    if (!amplitudes1 || !amplitudes2 || num_states == 0) return 0.0f;

    float dot = 0.0f;
    for (uint32_t i = 0; i < num_states; i++) {
        dot += amplitudes1[i] * amplitudes2[i];
    }

    return dot * dot;  /* |<psi1|psi2>|^2 */
}

uint32_t qmc_binary_sample(
    const float* cumulative,
    uint32_t num_states,
    float target
) {
    if (!cumulative || num_states == 0) return 0;

    uint32_t low = 0;
    uint32_t high = num_states;

    while (low < high) {
        uint32_t mid = low + (high - low) / 2;
        if (cumulative[mid] <= target) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    return (low < num_states) ? low : num_states - 1;
}

void qmc_build_cumulative(
    const float* probabilities,
    float* cumulative,
    uint32_t num_states
) {
    if (!probabilities || !cumulative || num_states == 0) return;

    float sum = 0.0f;
    for (uint32_t i = 0; i < num_states; i++) {
        sum += probabilities[i];
        cumulative[i] = sum;
    }
}

/* ============================================================================
 * Amplitude Estimation
 * ============================================================================ */

qmc_result_t qmc_estimate_amplitude(
    const float* amplitudes,
    uint32_t num_states,
    uint32_t target_state,
    const qmc_amplitude_config_t* config,
    qmc_amplitude_result_t* result
) {
    if (!amplitudes || !config || !result) {
        return QMC_ERROR_NULL;
    }
    if (num_states == 0 || target_state >= num_states) {
        return QMC_ERROR_INVALID;
    }

    memset(result, 0, sizeof(qmc_amplitude_result_t));

    uint32_t seed = config->seed ? config->seed : mc_seed_from_time();
    uint32_t num_samples = config->num_samples > 0 ?
                           config->num_samples : QMC_DEFAULT_AMPLITUDE_SAMPLES;

    /* For small state spaces, compute directly */
    if (num_states <= QMC_MAX_DIRECT_STATES && !config->use_importance) {
        float prob = amplitudes[target_state] * amplitudes[target_state];
        result->probability = prob;
        result->amplitude = fabsf(amplitudes[target_state]);
        result->variance = 0.0f;
        result->std_error = 0.0f;
        result->effective_samples = (float)num_samples;
        result->samples_used = 0;  /* Direct computation */
        return QMC_OK;
    }

    /* Build probability distribution */
    float* probs = nimcp_malloc(num_states * sizeof(float));
    float* cumulative = nimcp_malloc(num_states * sizeof(float));
    if (!probs || !cumulative) {
        nimcp_free(probs);
        nimcp_free(cumulative);
        return QMC_ERROR_MEMORY;
    }

    qmc_amplitudes_to_probabilities(amplitudes, probs, num_states);
    float norm = qmc_normalize_distribution(probs, num_states);

    if (norm < QMC_EPSILON) {
        nimcp_free(probs);
        nimcp_free(cumulative);
        result->probability = 0.0f;
        result->amplitude = 0.0f;
        return QMC_OK;
    }

    /* Use proposal distribution if provided, otherwise use |psi|^2 */
    const float* proposal = config->proposal_dist ? config->proposal_dist : probs;

    /* Build cumulative for sampling */
    qmc_build_cumulative(proposal, cumulative, num_states);

    /* Importance sampling estimation */
    float sum_weights = 0.0f;
    float sum_weighted_indicator = 0.0f;
    float sum_weights_sq = 0.0f;

    for (uint32_t i = 0; i < num_samples; i++) {
        float r = mc_random_uniform(&seed);
        uint32_t sampled = qmc_binary_sample(cumulative, num_states, r);

        /* Importance weight: target / proposal */
        float weight = 1.0f;
        if (config->use_importance && config->proposal_dist) {
            if (proposal[sampled] > QMC_EPSILON) {
                weight = probs[sampled] / proposal[sampled];
            }
        }

        /* Indicator: 1 if sampled == target, 0 otherwise */
        float indicator = (sampled == target_state) ? 1.0f : 0.0f;

        sum_weights += weight;
        sum_weighted_indicator += weight * indicator;
        sum_weights_sq += weight * weight;
    }

    /* Compute estimate and variance */
    if (sum_weights > QMC_EPSILON) {
        result->probability = sum_weighted_indicator / sum_weights;
        result->amplitude = sqrtf(result->probability);

        /* Effective sample size */
        result->effective_samples = (sum_weights * sum_weights) / sum_weights_sq;

        /* Variance estimate using weighted samples */
        float p = result->probability;
        result->variance = p * (1.0f - p) / result->effective_samples;
        result->std_error = sqrtf(result->variance);
    }

    result->samples_used = num_samples;

    nimcp_free(probs);
    nimcp_free(cumulative);

    return QMC_OK;
}

qmc_result_t qmc_estimate_amplitudes_batch(
    const float* amplitudes,
    uint32_t num_states,
    const uint32_t* target_states,
    uint32_t num_targets,
    const qmc_amplitude_config_t* config,
    qmc_amplitude_result_t* results
) {
    if (!amplitudes || !target_states || !config || !results) {
        return QMC_ERROR_NULL;
    }
    if (num_states == 0 || num_targets == 0) {
        return QMC_ERROR_INVALID;
    }

    /* Process each target */
    for (uint32_t t = 0; t < num_targets; t++) {
        qmc_result_t err = qmc_estimate_amplitude(
            amplitudes, num_states, target_states[t], config, &results[t]);
        if (err != QMC_OK) {
            return err;
        }
    }

    return QMC_OK;
}

/* ============================================================================
 * Quantum Measurement
 * ============================================================================ */

qmc_result_t qmc_finite_shots(
    const float* amplitudes,
    uint32_t num_states,
    const qmc_measurement_config_t* config,
    qmc_measurement_result_t* result
) {
    if (!amplitudes || !config || !result) {
        return QMC_ERROR_NULL;
    }
    if (num_states == 0) {
        return QMC_ERROR_INVALID;
    }

    memset(result, 0, sizeof(qmc_measurement_result_t));

    uint32_t num_shots = config->num_shots > 0 ?
                         config->num_shots : QMC_DEFAULT_SHOTS;
    uint32_t seed = config->seed ? config->seed : mc_seed_from_time();

    /* Allocate result arrays */
    result->counts = nimcp_calloc(num_states, sizeof(uint32_t));
    result->frequencies = nimcp_malloc(num_states * sizeof(float));
    if (config->compute_uncertainty) {
        result->uncertainties = nimcp_malloc(num_states * sizeof(float));
    }

    if (!result->counts || !result->frequencies ||
        (config->compute_uncertainty && !result->uncertainties)) {
        qmc_measurement_result_free(result);
        return QMC_ERROR_MEMORY;
    }

    /* Build probability distribution and cumulative */
    float* probs = nimcp_malloc(num_states * sizeof(float));
    float* cumulative = nimcp_malloc(num_states * sizeof(float));
    if (!probs || !cumulative) {
        nimcp_free(probs);
        nimcp_free(cumulative);
        qmc_measurement_result_free(result);
        return QMC_ERROR_MEMORY;
    }

    qmc_amplitudes_to_probabilities(amplitudes, probs, num_states);
    qmc_normalize_distribution(probs, num_states);
    qmc_build_cumulative(cumulative, probs, num_states);

    /* Perform measurements */
    uint32_t max_count = 0;
    uint32_t max_state = 0;

    for (uint32_t shot = 0; shot < num_shots; shot++) {
        float r = mc_random_uniform(&seed);
        uint32_t measured = qmc_binary_sample(cumulative, num_states, r);
        result->counts[measured]++;

        if (result->counts[measured] > max_count) {
            max_count = result->counts[measured];
            max_state = measured;
        }
    }

    /* Compute frequencies and uncertainties */
    float inv_shots = 1.0f / (float)num_shots;
    for (uint32_t i = 0; i < num_states; i++) {
        result->frequencies[i] = (float)result->counts[i] * inv_shots;

        if (config->compute_uncertainty) {
            /* Binomial standard error: sqrt(p(1-p)/n) */
            float p = result->frequencies[i];
            result->uncertainties[i] = sqrtf(p * (1.0f - p) * inv_shots);
        }
    }

    result->num_states = num_states;
    result->total_shots = num_shots;
    result->most_frequent = max_state;

    nimcp_free(probs);
    nimcp_free(cumulative);

    return QMC_OK;
}

void qmc_measurement_result_free(qmc_measurement_result_t* result) {
    if (!result) return;
    nimcp_free(result->counts);
    nimcp_free(result->frequencies);
    nimcp_free(result->uncertainties);
    memset(result, 0, sizeof(qmc_measurement_result_t));
}

uint32_t qmc_measure_importance(
    const float* amplitudes,
    uint32_t num_states,
    const float* proposal,
    uint32_t* seed
) {
    if (!amplitudes || num_states == 0 || !seed) return 0;

    /* If no proposal, use amplitudes directly with binary search */
    if (!proposal) {
        /* Build cumulative on stack for small states, heap for large */
        float cumulative_stack[256];
        float* cumulative = (num_states <= 256) ?
                            cumulative_stack :
                            nimcp_malloc(num_states * sizeof(float));

        if (!cumulative) return 0;

        float sum = 0.0f;
        for (uint32_t i = 0; i < num_states; i++) {
            sum += amplitudes[i] * amplitudes[i];
            cumulative[i] = sum;
        }

        float r = mc_random_uniform(seed) * sum;
        uint32_t result = qmc_binary_sample(cumulative, num_states, r);

        if (num_states > 256) nimcp_free(cumulative);

        return result;
    }

    /* Rejection sampling with proposal */
    float max_ratio = 0.0f;
    for (uint32_t i = 0; i < num_states; i++) {
        if (proposal[i] > QMC_EPSILON) {
            float ratio = (amplitudes[i] * amplitudes[i]) / proposal[i];
            if (ratio > max_ratio) max_ratio = ratio;
        }
    }

    if (max_ratio < QMC_EPSILON) return 0;

    /* Rejection sampling loop */
    for (uint32_t attempt = 0; attempt < num_states * 10; attempt++) {
        /* Sample from proposal */
        float r = mc_random_uniform(seed);
        float cumsum = 0.0f;
        uint32_t sampled = 0;
        for (uint32_t i = 0; i < num_states; i++) {
            cumsum += proposal[i];
            if (r <= cumsum) {
                sampled = i;
                break;
            }
        }

        /* Accept/reject */
        float target_prob = amplitudes[sampled] * amplitudes[sampled];
        float accept_prob = target_prob / (proposal[sampled] * max_ratio);

        if (mc_random_uniform(seed) < accept_prob) {
            return sampled;
        }
    }

    /* Fallback to uniform random */
    return mc_random_int(seed, num_states);
}

/* ============================================================================
 * Adaptive Quantum Annealing
 * ============================================================================ */

qmc_anneal_config_t qmc_anneal_default_config(void) {
    return (qmc_anneal_config_t){
        .initial_temp = 1.0f,
        .final_temp = 0.01f,
        .num_iterations = 1000,
        .quantum_strength = 0.5f,
        .strategy = QMC_PROPOSAL_ADAPTIVE,
        .target_acceptance = QMC_TARGET_ACCEPTANCE_RATE,
        .adaptation_interval = 50,
        .seed = 0
    };
}

qmc_result_t qmc_adaptive_anneal(
    qmc_energy_fn energy_fn,
    const float* initial_state,
    uint32_t dim,
    const qmc_anneal_config_t* config,
    void* user_data,
    qmc_anneal_result_t* result
) {
    if (!energy_fn || !initial_state || !config || !result) {
        return QMC_ERROR_NULL;
    }
    if (dim == 0) {
        return QMC_ERROR_INVALID;
    }

    memset(result, 0, sizeof(qmc_anneal_result_t));

    uint32_t seed = config->seed ? config->seed : mc_seed_from_time();

    /* Allocate state arrays */
    float* current = nimcp_malloc(dim * sizeof(float));
    float* proposed = nimcp_malloc(dim * sizeof(float));
    float* best = nimcp_malloc(dim * sizeof(float));
    float* step_sizes = nimcp_malloc(dim * sizeof(float));

    if (!current || !proposed || !best || !step_sizes) {
        nimcp_free(current);
        nimcp_free(proposed);
        nimcp_free(best);
        nimcp_free(step_sizes);
        return QMC_ERROR_MEMORY;
    }

    /* Initialize */
    memcpy(current, initial_state, dim * sizeof(float));
    memcpy(best, initial_state, dim * sizeof(float));

    for (uint32_t i = 0; i < dim; i++) {
        step_sizes[i] = 0.1f;  /* Initial step size */
    }

    float current_energy = energy_fn(current, dim, user_data);
    float best_energy = current_energy;

    uint32_t accepted = 0;
    uint32_t tunneling_events = 0;
    uint32_t window_accepted = 0;

    /* Annealing loop */
    for (uint32_t iter = 0; iter < config->num_iterations; iter++) {
        /* Compute temperature */
        float progress = (float)iter / (float)config->num_iterations;
        float temp = config->initial_temp *
                     powf(config->final_temp / config->initial_temp, progress);

        /* Propose new state */
        for (uint32_t i = 0; i < dim; i++) {
            proposed[i] = current[i] +
                          mc_random_normal(&seed, 0.0f, step_sizes[i]);
        }

        float proposed_energy = energy_fn(proposed, dim, user_data);
        float delta_energy = proposed_energy - current_energy;

        /* Metropolis acceptance */
        bool accept = false;
        if (delta_energy < 0.0f) {
            accept = true;  /* Always accept improvements */
        } else {
            float p_metro = expf(-delta_energy / temp);
            if (mc_random_uniform(&seed) < p_metro) {
                accept = true;
            }
        }

        /* Quantum tunneling */
        if (!accept && config->quantum_strength > 0.0f) {
            /* Tunneling probability decreases with barrier height and temperature */
            float barrier = fabsf(delta_energy);
            float p_tunnel = config->quantum_strength *
                             expf(-barrier / (temp * temp));
            if (mc_random_uniform(&seed) < p_tunnel) {
                accept = true;
                tunneling_events++;
            }
        }

        if (accept) {
            memcpy(current, proposed, dim * sizeof(float));
            current_energy = proposed_energy;
            accepted++;
            window_accepted++;

            if (current_energy < best_energy) {
                memcpy(best, current, dim * sizeof(float));
                best_energy = current_energy;
            }
        }

        /* Adapt step sizes */
        if (config->strategy == QMC_PROPOSAL_ADAPTIVE &&
            (iter + 1) % config->adaptation_interval == 0) {

            float acceptance_rate = (float)window_accepted /
                                    (float)config->adaptation_interval;

            /* Adjust step sizes to approach target acceptance rate */
            float adjustment = 1.0f;
            if (acceptance_rate > config->target_acceptance + 0.05f) {
                adjustment = 1.1f;  /* Increase step size */
            } else if (acceptance_rate < config->target_acceptance - 0.05f) {
                adjustment = 0.9f;  /* Decrease step size */
            }

            for (uint32_t i = 0; i < dim; i++) {
                step_sizes[i] *= adjustment;
                /* Clamp step sizes */
                if (step_sizes[i] > 10.0f) step_sizes[i] = 10.0f;
                if (step_sizes[i] < 1e-6f) step_sizes[i] = 1e-6f;
            }

            window_accepted = 0;
        }
    }

    /* Fill result */
    result->final_energy = best_energy;
    result->best_state = best;
    result->dim = dim;
    result->acceptance_rate = (float)accepted / (float)config->num_iterations;
    result->step_sizes = step_sizes;
    result->iterations_run = config->num_iterations;
    result->tunneling_events = tunneling_events;

    nimcp_free(current);
    nimcp_free(proposed);

    return QMC_OK;
}

void qmc_anneal_result_free(qmc_anneal_result_t* result) {
    if (!result) return;
    nimcp_free(result->best_state);
    nimcp_free(result->step_sizes);
    memset(result, 0, sizeof(qmc_anneal_result_t));
}

/* ============================================================================
 * Partition Function Estimation
 * ============================================================================ */

qmc_result_t qmc_estimate_partition(
    qmc_energy_fn energy_fn,
    const float* initial_state,
    uint32_t dim,
    const qmc_partition_config_t* config,
    void* user_data,
    qmc_partition_result_t* result
) {
    if (!energy_fn || !initial_state || !config || !result) {
        return QMC_ERROR_NULL;
    }
    if (dim == 0) {
        return QMC_ERROR_INVALID;
    }

    memset(result, 0, sizeof(qmc_partition_result_t));

    uint32_t seed = config->seed ? config->seed : mc_seed_from_time();
    float temp = config->temperature > 0.0f ? config->temperature : 1.0f;

    /* Allocate state arrays */
    float* current = nimcp_malloc(dim * sizeof(float));
    float* proposed = nimcp_malloc(dim * sizeof(float));

    if (!current || !proposed) {
        nimcp_free(current);
        nimcp_free(proposed);
        return QMC_ERROR_MEMORY;
    }

    memcpy(current, initial_state, dim * sizeof(float));
    float current_energy = energy_fn(current, dim, user_data);

    /* MCMC sampling */
    float sum_energy = 0.0f;
    float sum_energy_sq = 0.0f;
    uint32_t sample_count = 0;
    uint32_t accepted = 0;

    uint32_t total_steps = config->burnin +
                           config->num_samples * config->thinning;

    for (uint32_t step = 0; step < total_steps; step++) {
        /* Propose new state */
        for (uint32_t i = 0; i < dim; i++) {
            proposed[i] = current[i] + mc_random_normal(&seed, 0.0f, 0.1f);
        }

        float proposed_energy = energy_fn(proposed, dim, user_data);
        float delta_energy = proposed_energy - current_energy;

        /* Metropolis acceptance */
        bool accept = false;
        if (delta_energy < 0.0f) {
            accept = true;
        } else {
            float p = expf(-delta_energy / temp);
            if (mc_random_uniform(&seed) < p) {
                accept = true;
            }
        }

        if (accept) {
            memcpy(current, proposed, dim * sizeof(float));
            current_energy = proposed_energy;
            accepted++;
        }

        /* Collect samples after burn-in */
        if (step >= config->burnin &&
            (step - config->burnin) % config->thinning == 0) {
            sum_energy += current_energy;
            sum_energy_sq += current_energy * current_energy;
            sample_count++;
        }
    }

    /* Compute statistics */
    if (sample_count > 0) {
        result->mean_energy = sum_energy / (float)sample_count;
        result->energy_variance = (sum_energy_sq / (float)sample_count) -
                                  (result->mean_energy * result->mean_energy);
        result->heat_capacity = result->energy_variance / (temp * temp);

        /* Free energy approximation using mean energy */
        /* F = <E> - T*S, where S is estimated from variance */
        result->free_energy = result->mean_energy -
                              temp * logf(2.0f * M_PI * result->energy_variance + 1.0f) / 2.0f;
        result->log_Z = -result->free_energy / temp;
        result->entropy = (result->mean_energy - result->free_energy) / temp;

        /* Standard error */
        result->std_error = sqrtf(result->energy_variance / (float)sample_count);
    }

    nimcp_free(current);
    nimcp_free(proposed);

    return QMC_OK;
}

/* ============================================================================
 * MCTS-Guided Quantum Walk
 * ============================================================================ */

/* Internal state for walk MCTS */
typedef struct {
    float* amplitudes;
    uint32_t num_nodes;
    uint32_t current_step;
    uint32_t target_node;
    float target_probability;
} qmc_walk_state_t;

typedef struct {
    const uint8_t* adjacency;
    uint32_t num_nodes;
    uint32_t target_node;
    uint32_t max_steps;
} qmc_walk_user_data_t;

static uint32_t walk_get_action_count(const void* state, void* user_data) {
    (void)state;
    (void)user_data;
    return 4;  /* 4 coin types */
}

static uint32_t walk_get_action(const void* state, uint32_t idx, void* user_data) {
    (void)state;
    (void)user_data;
    return idx;
}

static void* walk_apply_action(const void* state, uint32_t action, void* user_data) {
    const qmc_walk_state_t* s = (const qmc_walk_state_t*)state;
    qmc_walk_user_data_t* ud = (qmc_walk_user_data_t*)user_data;

    qmc_walk_state_t* new_state = nimcp_malloc(sizeof(qmc_walk_state_t));
    if (!new_state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "new_state is NULL");

        return NULL;

    }

    new_state->num_nodes = s->num_nodes;
    new_state->current_step = s->current_step + 1;
    new_state->target_node = s->target_node;
    new_state->amplitudes = nimcp_malloc(s->num_nodes * sizeof(float));

    if (!new_state->amplitudes) {
        nimcp_free(new_state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "walk_apply_action: new_state->amplitudes is NULL");
        return NULL;
    }

    /* Apply coin operator based on action */
    float coin_factor = 1.0f;
    switch (action) {
        case QMC_COIN_HADAMARD:
            coin_factor = 0.7071f;  /* 1/sqrt(2) */
            break;
        case QMC_COIN_GROVER:
            coin_factor = 0.5f;
            break;
        case QMC_COIN_FOURIER:
            coin_factor = 0.6f;
            break;
        case QMC_COIN_IDENTITY:
        default:
            coin_factor = 1.0f;
            break;
    }

    /* Simplified walk step: spread amplitudes to neighbors */
    memset(new_state->amplitudes, 0, s->num_nodes * sizeof(float));

    for (uint32_t i = 0; i < s->num_nodes; i++) {
        if (fabsf(s->amplitudes[i]) < QMC_EPSILON) continue;

        /* Count neighbors */
        uint32_t degree = 0;
        for (uint32_t j = 0; j < s->num_nodes; j++) {
            if (ud->adjacency[i * s->num_nodes + j]) degree++;
        }

        if (degree == 0) {
            new_state->amplitudes[i] += s->amplitudes[i];
            continue;
        }

        /* Spread amplitude to neighbors */
        float spread = s->amplitudes[i] * coin_factor / sqrtf((float)degree);
        for (uint32_t j = 0; j < s->num_nodes; j++) {
            if (ud->adjacency[i * s->num_nodes + j]) {
                new_state->amplitudes[j] += spread;
            }
        }
    }

    /* Compute target probability */
    new_state->target_probability = new_state->amplitudes[ud->target_node] *
                                    new_state->amplitudes[ud->target_node];

    return new_state;
}

static float walk_evaluate(const void* state, void* user_data) {
    const qmc_walk_state_t* s = (const qmc_walk_state_t*)state;
    (void)user_data;
    return s->target_probability;
}

static bool walk_is_terminal(const void* state, void* user_data) {
    const qmc_walk_state_t* s = (const qmc_walk_state_t*)state;
    qmc_walk_user_data_t* ud = (qmc_walk_user_data_t*)user_data;

    if (s->target_probability > 0.5f) return true;
    if (s->current_step >= ud->max_steps) return true;
    return false;
}

static void walk_free_state(void* state, void* user_data) {
    (void)user_data;
    if (!state) return;
    qmc_walk_state_t* s = (qmc_walk_state_t*)state;
    nimcp_free(s->amplitudes);
    nimcp_free(s);
}

static void* walk_clone_state(const void* state, void* user_data) {
    (void)user_data;
    if (!state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state is NULL");

        return NULL;

    }

    const qmc_walk_state_t* s = (const qmc_walk_state_t*)state;
    qmc_walk_state_t* clone = nimcp_malloc(sizeof(qmc_walk_state_t));
    if (!clone) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "clone is NULL");

        return NULL;

    }

    *clone = *s;
    clone->amplitudes = nimcp_malloc(s->num_nodes * sizeof(float));
    if (!clone->amplitudes) {
        nimcp_free(clone);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "walk_clone_state: clone->amplitudes is NULL");
        return NULL;
    }
    memcpy(clone->amplitudes, s->amplitudes, s->num_nodes * sizeof(float));

    return clone;
}

qmc_result_t qmc_walk_mcts(
    const uint8_t* adjacency,
    uint32_t num_nodes,
    uint32_t start_node,
    uint32_t target_node,
    const qmc_walk_config_t* config,
    qmc_walk_result_t* result
) {
    if (!adjacency || !config || !result) {
        return QMC_ERROR_NULL;
    }
    if (num_nodes == 0 || start_node >= num_nodes || target_node >= num_nodes) {
        return QMC_ERROR_INVALID;
    }

    memset(result, 0, sizeof(qmc_walk_result_t));

    uint32_t seed = config->seed ? config->seed : mc_seed_from_time();

    /* Initialize walk state */
    qmc_walk_state_t initial_state;
    initial_state.num_nodes = num_nodes;
    initial_state.current_step = 0;
    initial_state.target_node = target_node;
    initial_state.amplitudes = nimcp_calloc(num_nodes, sizeof(float));

    if (!initial_state.amplitudes) {
        return QMC_ERROR_MEMORY;
    }

    initial_state.amplitudes[start_node] = 1.0f;  /* Start at start_node */
    initial_state.target_probability = (start_node == target_node) ? 1.0f : 0.0f;

    /* User data for callbacks */
    qmc_walk_user_data_t user_data = {
        .adjacency = adjacency,
        .num_nodes = num_nodes,
        .target_node = target_node,
        .max_steps = config->max_steps > 0 ? config->max_steps : 20
    };

    /* Configure MCTS */
    mcts_config_t mcts_config;
    mcts_config_init(&mcts_config);
    mcts_config.max_iterations = config->mcts_iterations > 0 ?
                                  config->mcts_iterations : 50;
    mcts_config.max_depth = user_data.max_steps;
    mcts_config.exploration_constant = config->exploration_constant > 0.0f ?
                                       config->exploration_constant : 1.4f;
    mcts_config.max_nodes = 256;

    mcts_config.get_action_count = walk_get_action_count;
    mcts_config.get_action = walk_get_action;
    mcts_config.apply_action = walk_apply_action;
    mcts_config.evaluate = walk_evaluate;
    mcts_config.is_terminal = walk_is_terminal;
    mcts_config.free_state = walk_free_state;
    mcts_config.clone_state = walk_clone_state;
    mcts_config.user_data = &user_data;
    mcts_config.seed = seed;

    /* Allocate coin sequence */
    result->coin_sequence = nimcp_malloc(user_data.max_steps * sizeof(qmc_coin_type_t));
    if (!result->coin_sequence) {
        nimcp_free(initial_state.amplitudes);
        return QMC_ERROR_MEMORY;
    }

    /* Run MCTS for each step */
    qmc_walk_state_t* current = nimcp_malloc(sizeof(qmc_walk_state_t));
    if (!current) {
        nimcp_free(initial_state.amplitudes);
        nimcp_free(result->coin_sequence);
        return QMC_ERROR_MEMORY;
    }

    *current = initial_state;
    current->amplitudes = nimcp_malloc(num_nodes * sizeof(float));
    if (!current->amplitudes) {
        nimcp_free(initial_state.amplitudes);
        nimcp_free(result->coin_sequence);
        nimcp_free(current);
        return QMC_ERROR_MEMORY;
    }
    memcpy(current->amplitudes, initial_state.amplitudes, num_nodes * sizeof(float));

    uint32_t step = 0;
    while (step < user_data.max_steps) {
        if (current->target_probability > 0.5f) {
            result->target_reached = 1;
            break;
        }

        mcts_result_t mcts_result;
        nimcp_mc_result_t err = mcts_search(&mcts_config, current, &mcts_result);

        if (err != NIMCP_MC_OK) {
            result->coin_sequence[step] = QMC_COIN_HADAMARD;  /* Default */
        } else {
            result->coin_sequence[step] = (qmc_coin_type_t)mcts_result.best_action;
            mcts_result_free(&mcts_result);
        }

        /* Apply selected coin */
        qmc_walk_state_t* next = walk_apply_action(current,
                                                   result->coin_sequence[step],
                                                   &user_data);
        if (!next) break;

        walk_free_state(current, &user_data);
        current = next;
        step++;
    }

    result->steps_taken = step;
    result->target_probability = current->target_probability;
    result->num_coins = step;
    result->mean_hitting_time = (float)step;

    walk_free_state(current, &user_data);
    nimcp_free(initial_state.amplitudes);

    return QMC_OK;
}

void qmc_walk_result_free(qmc_walk_result_t* result) {
    if (!result) return;
    nimcp_free(result->coin_sequence);
    memset(result, 0, sizeof(qmc_walk_result_t));
}

/* ============================================================================
 * Entropy Estimation
 * ============================================================================ */

qmc_result_t qmc_estimate_entropy(
    const float* probabilities,
    uint32_t num_states,
    const qmc_entropy_config_t* config,
    qmc_entropy_result_t* result
) {
    if (!probabilities || !config || !result) {
        return QMC_ERROR_NULL;
    }
    if (num_states == 0) {
        return QMC_ERROR_INVALID;
    }

    memset(result, 0, sizeof(qmc_entropy_result_t));

    uint32_t seed = config->seed ? config->seed : mc_seed_from_time();
    uint32_t num_samples = config->num_samples > 0 ?
                           config->num_samples : 10000;

    /* For small state spaces, compute directly */
    if (num_states <= 10000 && !config->use_stratified) {
        float shannon = 0.0f;
        float sum_p_sq = 0.0f;
        float max_p = 0.0f;

        for (uint32_t i = 0; i < num_states; i++) {
            if (probabilities[i] > QMC_EPSILON) {
                shannon -= probabilities[i] * logf(probabilities[i]);
                sum_p_sq += probabilities[i] * probabilities[i];
                if (probabilities[i] > max_p) max_p = probabilities[i];
            }
        }

        result->shannon_entropy = shannon / logf(2.0f);  /* Convert to bits */
        result->renyi_entropy_2 = -logf(sum_p_sq) / logf(2.0f);
        result->min_entropy = -logf(max_p) / logf(2.0f);
        result->variance = 0.0f;
        result->std_error = 0.0f;

        return QMC_OK;
    }

    /* Monte Carlo estimation for large state spaces */
    float* cumulative = nimcp_malloc(num_states * sizeof(float));
    if (!cumulative) {
        return QMC_ERROR_MEMORY;
    }

    qmc_build_cumulative(probabilities, cumulative, num_states);

    float sum_log_p = 0.0f;
    float sum_log_p_sq = 0.0f;
    float sum_p = 0.0f;
    float max_p = 0.0f;

    for (uint32_t i = 0; i < num_samples; i++) {
        float r = mc_random_uniform(&seed);
        uint32_t sampled = qmc_binary_sample(cumulative, num_states, r);

        float p = probabilities[sampled];
        if (p > QMC_EPSILON) {
            float log_p = logf(p);
            sum_log_p += log_p;
            sum_log_p_sq += log_p * log_p;
            sum_p += p;
            if (p > max_p) max_p = p;
        }
    }

    /* Shannon entropy: H = -E[log(p)] */
    float mean_log_p = sum_log_p / (float)num_samples;
    result->shannon_entropy = -mean_log_p / logf(2.0f);

    /* Variance of log(p) */
    float var_log_p = (sum_log_p_sq / (float)num_samples) - (mean_log_p * mean_log_p);
    result->variance = var_log_p / ((float)num_samples * logf(2.0f) * logf(2.0f));
    result->std_error = sqrtf(result->variance);

    /* Renyi-2 entropy approximation */
    result->renyi_entropy_2 = -logf(sum_p / (float)num_samples) / logf(2.0f);

    /* Min-entropy approximation */
    result->min_entropy = -logf(max_p) / logf(2.0f);

    nimcp_free(cumulative);

    return QMC_OK;
}

/* ============================================================================
 * SAT Solving with MCTS
 * ============================================================================ */

/* Internal state for SAT MCTS */
typedef struct {
    uint8_t* assignment;        /* 0=unassigned, 1=false, 2=true */
    uint32_t num_variables;
    uint32_t num_assigned;
    uint32_t num_satisfied;
    uint32_t num_unsatisfied;
} qmc_sat_state_t;

typedef struct {
    const qmc_cnf_t* cnf;
    bool use_unit_propagation;
} qmc_sat_user_data_t;

static bool clause_satisfied(const qmc_clause_t* clause, const uint8_t* assignment) {
    for (uint32_t i = 0; i < clause->num_literals; i++) {
        int32_t lit = clause->literals[i];
        uint32_t var = abs(lit) - 1;
        if (assignment[var] == 0) continue;  /* Unassigned */

        bool val = (assignment[var] == 2);
        bool positive = (lit > 0);
        if (val == positive) return true;
    }
    return false;
}

static bool clause_falsified(const qmc_clause_t* clause, const uint8_t* assignment) {
    for (uint32_t i = 0; i < clause->num_literals; i++) {
        int32_t lit = clause->literals[i];
        uint32_t var = abs(lit) - 1;
        if (assignment[var] == 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "clause_falsified: validation failed");
            return false;
        }

        bool val = (assignment[var] == 2);
        bool positive = (lit > 0);
        if (val == positive) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "clause_falsified: validation failed");
            return false;
        }
    }
    return true;  /* All literals are false */
}

static uint32_t sat_get_action_count(const void* state, void* user_data) {
    const qmc_sat_state_t* s = (const qmc_sat_state_t*)state;

    /* Count unassigned variables * 2 (true/false for each) */
    uint32_t count = 0;
    for (uint32_t i = 0; i < s->num_variables; i++) {
        if (s->assignment[i] == 0) count += 2;
    }
    return count;
}

static uint32_t sat_get_action(const void* state, uint32_t idx, void* user_data) {
    (void)user_data;
    const qmc_sat_state_t* s = (const qmc_sat_state_t*)state;

    uint32_t count = 0;
    for (uint32_t i = 0; i < s->num_variables; i++) {
        if (s->assignment[i] == 0) {
            if (count == idx) return i * 2;      /* Assign false */
            if (count + 1 == idx) return i * 2 + 1;  /* Assign true */
            count += 2;
        }
    }
    return 0;
}

static void* sat_apply_action(const void* state, uint32_t action, void* user_data) {
    const qmc_sat_state_t* s = (const qmc_sat_state_t*)state;
    qmc_sat_user_data_t* ud = (qmc_sat_user_data_t*)user_data;

    qmc_sat_state_t* new_state = nimcp_malloc(sizeof(qmc_sat_state_t));
    if (!new_state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "new_state is NULL");

        return NULL;

    }

    new_state->num_variables = s->num_variables;
    new_state->assignment = nimcp_malloc(s->num_variables);
    if (!new_state->assignment) {
        nimcp_free(new_state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sat_apply_action: new_state->assignment is NULL");
        return NULL;
    }
    memcpy(new_state->assignment, s->assignment, s->num_variables);

    /* Apply action: variable = action/2, value = action%2 + 1 */
    uint32_t var = action / 2;
    uint8_t val = (action % 2) + 1;  /* 1=false, 2=true */
    new_state->assignment[var] = val;
    new_state->num_assigned = s->num_assigned + 1;

    /* Count satisfied/unsatisfied clauses */
    new_state->num_satisfied = 0;
    new_state->num_unsatisfied = 0;

    for (uint32_t c = 0; c < ud->cnf->num_clauses; c++) {
        if (clause_satisfied(&ud->cnf->clauses[c], new_state->assignment)) {
            new_state->num_satisfied++;
        } else if (clause_falsified(&ud->cnf->clauses[c], new_state->assignment)) {
            new_state->num_unsatisfied++;
        }
    }

    return new_state;
}

static float sat_evaluate(const void* state, void* user_data) {
    const qmc_sat_state_t* s = (const qmc_sat_state_t*)state;
    qmc_sat_user_data_t* ud = (qmc_sat_user_data_t*)user_data;

    /* Reward based on satisfied clauses, penalize unsatisfied */
    float satisfied_ratio = (float)s->num_satisfied / (float)ud->cnf->num_clauses;
    float unsatisfied_penalty = (float)s->num_unsatisfied / (float)ud->cnf->num_clauses;

    return satisfied_ratio - unsatisfied_penalty;
}

static bool sat_is_terminal(const void* state, void* user_data) {
    const qmc_sat_state_t* s = (const qmc_sat_state_t*)state;
    qmc_sat_user_data_t* ud = (qmc_sat_user_data_t*)user_data;

    /* Terminal if all satisfied or any unsatisfied */
    if (s->num_satisfied == ud->cnf->num_clauses) return true;
    if (s->num_unsatisfied > 0) return true;
    if (s->num_assigned == s->num_variables) return true;

    return false;
}

static void sat_free_state(void* state, void* user_data) {
    (void)user_data;
    if (!state) return;
    qmc_sat_state_t* s = (qmc_sat_state_t*)state;
    nimcp_free(s->assignment);
    nimcp_free(s);
}

static void* sat_clone_state(const void* state, void* user_data) {
    (void)user_data;
    if (!state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state is NULL");

        return NULL;

    }

    const qmc_sat_state_t* s = (const qmc_sat_state_t*)state;
    qmc_sat_state_t* clone = nimcp_malloc(sizeof(qmc_sat_state_t));
    if (!clone) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "clone is NULL");

        return NULL;

    }

    *clone = *s;
    clone->assignment = nimcp_malloc(s->num_variables);
    if (!clone->assignment) {
        nimcp_free(clone);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sat_clone_state: clone->assignment is NULL");
        return NULL;
    }
    memcpy(clone->assignment, s->assignment, s->num_variables);

    return clone;
}

qmc_result_t qmc_solve_sat_mcts(
    const qmc_cnf_t* cnf,
    const qmc_sat_config_t* config,
    qmc_sat_result_t* result
) {
    if (!cnf || !config || !result) {
        return QMC_ERROR_NULL;
    }
    if (cnf->num_variables == 0 || cnf->num_clauses == 0) {
        return QMC_ERROR_INVALID;
    }

    memset(result, 0, sizeof(qmc_sat_result_t));

    uint32_t seed = config->seed ? config->seed : mc_seed_from_time();

    /* Initialize state */
    qmc_sat_state_t initial_state;
    initial_state.num_variables = cnf->num_variables;
    initial_state.assignment = nimcp_calloc(cnf->num_variables, 1);
    initial_state.num_assigned = 0;
    initial_state.num_satisfied = 0;
    initial_state.num_unsatisfied = 0;

    if (!initial_state.assignment) {
        return QMC_ERROR_MEMORY;
    }

    qmc_sat_user_data_t user_data = {
        .cnf = cnf,
        .use_unit_propagation = config->use_unit_propagation
    };

    /* Configure MCTS */
    mcts_config_t mcts_config;
    mcts_config_init(&mcts_config);
    mcts_config.max_iterations = config->mcts_iterations > 0 ?
                                  config->mcts_iterations : 100;
    mcts_config.max_depth = config->max_depth > 0 ?
                            config->max_depth : cnf->num_variables;
    mcts_config.exploration_constant = config->exploration_constant > 0.0f ?
                                       config->exploration_constant : 1.4f;
    mcts_config.max_nodes = 512;

    mcts_config.get_action_count = sat_get_action_count;
    mcts_config.get_action = sat_get_action;
    mcts_config.apply_action = sat_apply_action;
    mcts_config.evaluate = sat_evaluate;
    mcts_config.is_terminal = sat_is_terminal;
    mcts_config.free_state = sat_free_state;
    mcts_config.clone_state = sat_clone_state;
    mcts_config.user_data = &user_data;
    mcts_config.seed = seed;

    /* Search for solution */
    qmc_sat_state_t* current = sat_clone_state(&initial_state, &user_data);
    if (!current) {
        nimcp_free(initial_state.assignment);
        return QMC_ERROR_MEMORY;
    }

    uint32_t nodes_explored = 0;

    while (!sat_is_terminal(current, &user_data)) {
        mcts_result_t mcts_result;
        nimcp_mc_result_t err = mcts_search(&mcts_config, current, &mcts_result);

        if (err != NIMCP_MC_OK || mcts_result.num_actions == 0) {
            break;
        }

        nodes_explored += mcts_result.nodes_created;

        /* Apply best action */
        qmc_sat_state_t* next = sat_apply_action(current,
                                                 mcts_result.best_action,
                                                 &user_data);
        mcts_result_free(&mcts_result);

        if (!next) break;

        sat_free_state(current, &user_data);
        current = next;
    }

    /* Fill result */
    result->num_variables = cnf->num_variables;
    result->nodes_explored = nodes_explored;
    result->satisfiable = (current->num_satisfied == cnf->num_clauses);

    if (result->satisfiable) {
        result->assignment = nimcp_malloc(cnf->num_variables);
        if (result->assignment) {
            for (uint32_t i = 0; i < cnf->num_variables; i++) {
                result->assignment[i] = (current->assignment[i] == 2) ? 1 : 0;
            }
        }
        result->confidence = 1.0f;
    } else {
        result->confidence = (float)current->num_satisfied / (float)cnf->num_clauses;
    }

    sat_free_state(current, &user_data);
    nimcp_free(initial_state.assignment);

    return QMC_OK;
}

void qmc_sat_result_free(qmc_sat_result_t* result) {
    if (!result) return;
    nimcp_free(result->assignment);
    memset(result, 0, sizeof(qmc_sat_result_t));
}
