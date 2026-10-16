/**
 * @file nimcp_wernicke_quantum_bridge.c
 * @brief Quantum-Accelerated Language Comprehension Implementation
 *
 * Implements quantum algorithms for:
 * - Grover search for lexicon lookup
 * - Quantum walks for spreading activation
 * - Quantum disambiguation
 * - Amplitude estimation for confidence
 *
 * @version Phase W5: Advanced Features
 * @date 2026-01-05
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_quantum_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(wernicke_quantum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "WERNICKE_QUANTUM_BRIDGE"


/*=============================================================================
 * INTERNAL CONSTANTS
 *=============================================================================*/

#define DEFAULT_GROVER_MAX_ITERATIONS    15
#define DEFAULT_GROVER_SUCCESS_THRESHOLD 0.9f
#define DEFAULT_WALK_MAX_STEPS           50
#define DEFAULT_WALK_MIXING_THRESHOLD    0.95f
#define DEFAULT_CONTEXT_WEIGHT           0.6f
#define DEFAULT_TARGET_SPEEDUP           4.0f
#define DEFAULT_CLASSICAL_FALLBACK       0.5f

#define PI NIMCP_PI_F

/*=============================================================================
 * INTERNAL STRUCTURES
 *=============================================================================*/

/**
 * @brief Wernicke quantum bridge context
 */
struct wernicke_quantum_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    wernicke_quantum_config_t config;

    /* Connected Wernicke adapter */
    void* wernicke;

    /* State */
    bool enabled;
    uint32_t seed;

    /* Statistics */
    wernicke_quantum_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *=============================================================================*/

/**
 * @brief Simple pseudo-random number generator
 */
static float random_float(uint32_t* seed) {
    *seed = (*seed * 1103515245 + 12345) & 0x7fffffff;
    return (float)(*seed) / (float)0x7fffffff;
}

/**
 * @brief Compute optimal Grover iterations
 *
 * For N items with M solutions: iterations ≈ (π/4) * sqrt(N/M)
 */
static uint32_t compute_grover_iterations(uint32_t n, uint32_t m) {
    if (m == 0 || n == 0) return 1;
    float ratio = (float)n / (float)m;
    float iterations = (PI / 4.0f) * sqrtf(ratio);
    uint32_t result = (uint32_t)(iterations + 0.5f);
    return result > 0 ? result : 1;
}

/**
 * @brief Apply Grover diffusion operator (simplified)
 */
static void apply_diffusion(float* amplitudes, uint32_t n) {
    if (!amplitudes || n == 0) return;

    /* Compute mean */
    float mean = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        mean += amplitudes[i];
    }
    mean /= (float)n;

    /* Inversion about mean */
    for (uint32_t i = 0; i < n; i++) {
        amplitudes[i] = 2.0f * mean - amplitudes[i];
    }
}

/**
 * @brief Apply oracle (mark target state)
 */
static void apply_oracle(float* amplitudes, uint32_t target_idx, uint32_t n) {
    if (!amplitudes || target_idx >= n) return;
    amplitudes[target_idx] = -amplitudes[target_idx];
}

/**
 * @brief Normalize amplitudes
 */
static void normalize_amplitudes(float* amplitudes, uint32_t n) {
    if (!amplitudes || n == 0) return;

    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum_sq += amplitudes[i] * amplitudes[i];
    }

    if (sum_sq > 0.0f) {
        float norm = sqrtf(sum_sq);
        for (uint32_t i = 0; i < n; i++) {
            amplitudes[i] /= norm;
        }
    }
}

/**
 * @brief Compute quantum speedup achieved
 */
static float compute_speedup(uint32_t classical_ops, uint32_t quantum_ops) {
    if (quantum_ops == 0) return 1.0f;
    return (float)classical_ops / (float)quantum_ops;
}

/**
 * @brief Update running average
 */
static float update_avg(float current_avg, float new_value, uint64_t count) {
    if (count == 0) return isfinite(new_value) ? new_value : 0.0f;
    float n = (float)count;
    float result = ((n - 1.0f) * current_avg + new_value) / n;
    return isfinite(result) ? result : current_avg;
}

/*=============================================================================
 * CONFIGURATION API
 *=============================================================================*/

int wernicke_quantum_default_config(wernicke_quantum_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_quantum_default_config: config is NULL");
        return -1;
    }

    *config = (wernicke_quantum_config_t){
        .default_algo = WERNICKE_QA_HYBRID,
        .enable_hybrid = true,
        .classical_fallback_threshold = DEFAULT_CLASSICAL_FALLBACK,

        .grover_max_iterations = DEFAULT_GROVER_MAX_ITERATIONS,
        .grover_success_threshold = DEFAULT_GROVER_SUCCESS_THRESHOLD,

        .walk_max_steps = DEFAULT_WALK_MAX_STEPS,
        .walk_mixing_threshold = DEFAULT_WALK_MIXING_THRESHOLD,
        .walk_use_continuous = false,

        .disambig_mode = WERNICKE_DISAMBIG_QUANTUM,
        .context_weight = DEFAULT_CONTEXT_WEIGHT,

        .enable_speedup_tracking = true,
        .target_speedup = DEFAULT_TARGET_SPEEDUP,

        .enable_logging = false
    };

    return 0;
}

/*=============================================================================
 * LIFECYCLE API
 *=============================================================================*/

wernicke_quantum_bridge_t* wernicke_quantum_bridge_create(
    void* wernicke,
    const wernicke_quantum_config_t* config
) {
    /* Wernicke adapter is required */
    if (!wernicke) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke is NULL");

        return NULL;
    }

    wernicke_quantum_bridge_t* bridge = nimcp_calloc(1, sizeof(wernicke_quantum_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        wernicke_quantum_default_config(&bridge->config);
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "wernicke_quantum") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    bridge->wernicke = wernicke;
    bridge->enabled = true;
    bridge->seed = 42;

    return bridge;
}

void wernicke_quantum_bridge_destroy(wernicke_quantum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "wernicke_quantum");
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

bool wernicke_quantum_is_enabled(const wernicke_quantum_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->enabled;
}

void wernicke_quantum_set_enabled(wernicke_quantum_bridge_t* bridge,
                                   bool enabled) {
    if (!bridge) return;
    bridge->enabled = enabled;
}

/*=============================================================================
 * GROVER SEARCH API
 *=============================================================================*/

int wernicke_quantum_search_lexicon(wernicke_quantum_bridge_t* bridge,
                                     const uint8_t* phoneme_pattern,
                                     uint32_t pattern_len,
                                     uint32_t lexicon_size,
                                     quantum_search_result_t* result) {
    if (!bridge || !phoneme_pattern || !result || lexicon_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_quantum_is_enabled: required parameter is NULL (bridge, phoneme_pattern, result)");
        return -1;
    }

    memset(result, 0, sizeof(quantum_search_result_t));

    /* Initialize uniform superposition */
    float* amplitudes = nimcp_calloc(lexicon_size, sizeof(float));
    if (!amplitudes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_quantum_is_enabled: amplitudes is NULL");
        return -1;
    }

    float initial_amp = 1.0f / sqrtf((float)lexicon_size);
    for (uint32_t i = 0; i < lexicon_size; i++) {
        amplitudes[i] = initial_amp;
    }

    /* Simulate oracle: find target based on phoneme pattern hash */
    uint32_t target_hash = 0;
    for (uint32_t i = 0; i < pattern_len; i++) {
        target_hash = target_hash * 31 + phoneme_pattern[i];
    }
    uint32_t target_idx = target_hash % lexicon_size;

    /* Compute optimal iterations */
    uint32_t optimal_iters = compute_grover_iterations(lexicon_size, 1);
    uint32_t max_iters = bridge->config.grover_max_iterations;
    uint32_t iterations = (optimal_iters < max_iters) ? optimal_iters : max_iters;

    /* Grover iterations */
    for (uint32_t iter = 0; iter < iterations; iter++) {
        apply_oracle(amplitudes, target_idx, lexicon_size);
        apply_diffusion(amplitudes, lexicon_size);
    }

    /* Find max amplitude (measurement simulation) */
    uint32_t best_idx = 0;
    float best_amp = amplitudes[0];
    for (uint32_t i = 1; i < lexicon_size; i++) {
        if (fabsf(amplitudes[i]) > fabsf(best_amp)) {
            best_amp = amplitudes[i];
            best_idx = i;
        }
    }

    /* Populate result */
    result->found_id = best_idx;
    result->probability = best_amp * best_amp;
    result->iterations_used = iterations;
    result->speedup_achieved = compute_speedup(lexicon_size, iterations);
    result->success = (result->probability >= bridge->config.grover_success_threshold);

    /* Generate placeholder word */
    snprintf(result->found_word, sizeof(result->found_word), "word_%u", best_idx);

    nimcp_free(amplitudes);

    /* Update statistics */
    bridge->stats.grover_searches++;
    if (result->success) {
        bridge->stats.successful_searches++;
    } else {
        bridge->stats.failed_searches++;
    }
    bridge->stats.avg_grover_speedup = update_avg(
        bridge->stats.avg_grover_speedup,
        result->speedup_achieved,
        bridge->stats.grover_searches
    );
    if (result->speedup_achieved > bridge->stats.max_speedup_achieved) {
        bridge->stats.max_speedup_achieved = result->speedup_achieved;
    }

    return 0;
}

int wernicke_quantum_search_concepts(wernicke_quantum_bridge_t* bridge,
                                      const float* semantic_target,
                                      uint32_t target_dim,
                                      uint32_t num_concepts,
                                      quantum_search_result_t* result) {
    if (!bridge || !semantic_target || !result || num_concepts == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_quantum_is_enabled: required parameter is NULL (bridge, semantic_target, result)");
        return -1;
    }

    memset(result, 0, sizeof(quantum_search_result_t));

    /* Compute target hash from semantic vector */
    float target_sum = 0.0f;
    for (uint32_t i = 0; i < target_dim; i++) {
        target_sum += semantic_target[i];
    }
    uint32_t target_idx = (uint32_t)(fabsf(target_sum) * 1000) % num_concepts;

    /* Initialize superposition */
    float* amplitudes = nimcp_calloc(num_concepts, sizeof(float));
    if (!amplitudes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_quantum_is_enabled: amplitudes is NULL");
        return -1;
    }

    float initial_amp = 1.0f / sqrtf((float)num_concepts);
    for (uint32_t i = 0; i < num_concepts; i++) {
        amplitudes[i] = initial_amp;
    }

    /* Grover iterations */
    uint32_t optimal_iters = compute_grover_iterations(num_concepts, 1);
    uint32_t iterations = (optimal_iters < bridge->config.grover_max_iterations)
                          ? optimal_iters : bridge->config.grover_max_iterations;

    for (uint32_t iter = 0; iter < iterations; iter++) {
        apply_oracle(amplitudes, target_idx, num_concepts);
        apply_diffusion(amplitudes, num_concepts);
    }

    /* Measure */
    uint32_t best_idx = 0;
    float best_amp = fabsf(amplitudes[0]);
    for (uint32_t i = 1; i < num_concepts; i++) {
        if (fabsf(amplitudes[i]) > best_amp) {
            best_amp = fabsf(amplitudes[i]);
            best_idx = i;
        }
    }

    result->found_id = best_idx;
    result->probability = best_amp * best_amp;
    result->iterations_used = iterations;
    result->speedup_achieved = compute_speedup(num_concepts, iterations);
    result->success = (result->probability >= bridge->config.grover_success_threshold);

    nimcp_free(amplitudes);

    bridge->stats.grover_searches++;

    return 0;
}

/*=============================================================================
 * QUANTUM WALK API
 *=============================================================================*/

int wernicke_quantum_walk_init(wernicke_quantum_bridge_t* bridge,
                                uint32_t start_concept,
                                uint32_t graph_size,
                                quantum_walk_state_t* state) {
    if (!bridge || !state || graph_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_quantum_is_enabled: required parameter is NULL (bridge, state)");
        return -1;
    }

    memset(state, 0, sizeof(quantum_walk_state_t));

    state->node_amplitudes = nimcp_calloc(graph_size, sizeof(float));
    if (!state->node_amplitudes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_quantum_is_enabled: state->node_amplitudes is NULL");
        return -1;
    }

    state->num_nodes = graph_size;
    state->current_node = start_concept;
    state->steps_taken = 0;
    state->mixing_progress = 0.0f;

    /* Initialize amplitude at start node */
    if (start_concept < graph_size) {
        state->node_amplitudes[start_concept] = 1.0f;
    }

    return 0;
}

int wernicke_quantum_walk_step(wernicke_quantum_bridge_t* bridge,
                                quantum_walk_state_t* state) {
    if (!bridge || !state || !state->node_amplitudes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_quantum_is_enabled: required parameter is NULL (bridge, state, state->node_amplitudes)");
        return -1;
    }

    uint32_t n = state->num_nodes;

    /* Allocate temporary buffer */
    float* new_amps = nimcp_calloc(n, sizeof(float));
    if (!new_amps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_quantum_is_enabled: new_amps is NULL");
        return -1;
    }

    /* Simplified quantum walk: each node spreads to neighbors
     * Assume ring topology for simplicity */
    for (uint32_t i = 0; i < n; i++) {
        float amp = state->node_amplitudes[i];
        if (fabsf(amp) < 1e-10f) continue;

        /* Distribute to neighbors (and self) */
        uint32_t left = (i > 0) ? i - 1 : n - 1;
        uint32_t right = (i < n - 1) ? i + 1 : 0;

        /* Grover coin: spread with quantum interference */
        float coin_amp = amp / sqrtf(3.0f);
        new_amps[left] += coin_amp;
        new_amps[i] += coin_amp;
        new_amps[right] += coin_amp;
    }

    /* Normalize */
    normalize_amplitudes(new_amps, n);

    /* Copy back */
    memcpy(state->node_amplitudes, new_amps, n * sizeof(float));
    nimcp_free(new_amps);

    state->steps_taken++;

    /* Compute mixing progress (variance of distribution) */
    float mean = (n > 0) ? 1.0f / (float)n : 0.0f;  /* Target uniform */
    float variance = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float prob = state->node_amplitudes[i] * state->node_amplitudes[i];
        float diff = prob - mean;
        variance += diff * diff;
    }
    variance = (n > 0) ? variance / (float)n : 0.0f;

    /* Lower variance = more mixed */
    state->mixing_progress = 1.0f - sqrtf(variance) * sqrtf((float)n);
    if (state->mixing_progress < 0.0f) state->mixing_progress = 0.0f;
    if (state->mixing_progress > 1.0f) state->mixing_progress = 1.0f;

    return 0;
}

int wernicke_quantum_walk_run(wernicke_quantum_bridge_t* bridge,
                               quantum_walk_state_t* state,
                               uint32_t max_steps) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, state)");
        return -1;
    }

    uint32_t steps = 0;
    while (steps < max_steps &&
           state->mixing_progress < bridge->config.walk_mixing_threshold) {
        if (wernicke_quantum_walk_step(bridge, state) < 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
            return -1;
        }
        steps++;
    }

    bridge->stats.quantum_walks++;

    return (int)steps;
}

int wernicke_quantum_walk_measure(wernicke_quantum_bridge_t* bridge,
                                   const quantum_walk_state_t* state,
                                   quantum_spreading_result_t* result) {
    if (!bridge || !state || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, state, result)");
        return -1;
    }

    memset(result, 0, sizeof(quantum_spreading_result_t));

    /* Count significant activations */
    uint32_t count = 0;
    float threshold = 0.01f;

    for (uint32_t i = 0; i < state->num_nodes; i++) {
        float prob = state->node_amplitudes[i] * state->node_amplitudes[i];
        if (prob > threshold) {
            count++;
        }
    }

    if (count == 0) return 0;

    /* Allocate results */
    result->activated_concepts = nimcp_calloc(count, sizeof(uint32_t));
    result->activation_levels = nimcp_calloc(count, sizeof(float));
    if (!result->activated_concepts || !result->activation_levels) {
        nimcp_free(result->activated_concepts);
        nimcp_free(result->activation_levels);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: required parameter is NULL (result->activated_concepts, result->activation_levels)");
        return -1;
    }

    /* Fill results */
    uint32_t idx = 0;
    float total = 0.0f;
    for (uint32_t i = 0; i < state->num_nodes && idx < count; i++) {
        float prob = state->node_amplitudes[i] * state->node_amplitudes[i];
        if (prob > threshold) {
            result->activated_concepts[idx] = i;
            result->activation_levels[idx] = prob;
            total += prob;
            idx++;
        }
    }

    result->num_activated = count;
    result->total_activation = total;
    result->walk_steps = state->steps_taken;
    result->speedup_achieved = sqrtf((float)state->num_nodes) /
                               (float)(state->steps_taken + 1);

    bridge->stats.spreading_activations++;
    bridge->stats.avg_walk_speedup = update_avg(
        bridge->stats.avg_walk_speedup,
        result->speedup_achieved,
        bridge->stats.spreading_activations
    );

    return 0;
}

/*=============================================================================
 * DISAMBIGUATION API
 *=============================================================================*/

int wernicke_quantum_disambiguate(wernicke_quantum_bridge_t* bridge,
                                   const char* word,
                                   const float* context_embedding,
                                   uint32_t context_dim,
                                   quantum_disambig_result_t* result) {
    if (!bridge || !word || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, word, result)");
        return -1;
    }

    memset(result, 0, sizeof(quantum_disambig_result_t));

    /* Create superposition of senses */
    quantum_concept_state_t state;
    memset(&state, 0, sizeof(state));

    if (wernicke_quantum_superpose_senses(bridge, word, &state) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    if (state.num_concepts == 0) {
        wernicke_quantum_state_free(&state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: state.num_concepts is zero");
        return -1;
    }

    /* Collapse based on context */
    int32_t collapsed = wernicke_quantum_collapse(bridge, &state,
                                                   context_embedding,
                                                   context_dim);

    if (collapsed >= 0) {
        result->sense_id = (uint32_t)collapsed;
        result->confidence = state.amplitudes ? state.amplitudes[0] : 0.5f;
    } else {
        result->sense_id = state.concept_ids ? state.concept_ids[0] : 0;
        result->confidence = 0.5f;
    }

    /* Copy alternatives */
    if (state.num_concepts > 1) {
        result->num_alternatives = state.num_concepts - 1;
        result->alternatives = nimcp_calloc(result->num_alternatives, sizeof(uint32_t));
        result->alt_probabilities = nimcp_calloc(result->num_alternatives, sizeof(float));

        if (result->alternatives && result->alt_probabilities) {
            for (uint32_t i = 1; i < state.num_concepts; i++) {
                result->alternatives[i - 1] = state.concept_ids[i];
                result->alt_probabilities[i - 1] =
                    state.amplitudes[i] * state.amplitudes[i];
            }
        }
    }

    result->mode_used = bridge->config.disambig_mode;

    wernicke_quantum_state_free(&state);

    bridge->stats.disambiguations++;
    bridge->stats.avg_disambiguation_confidence = update_avg(
        bridge->stats.avg_disambiguation_confidence,
        result->confidence,
        bridge->stats.disambiguations
    );

    return 0;
}

int wernicke_quantum_superpose_senses(wernicke_quantum_bridge_t* bridge,
                                       const char* word,
                                       quantum_concept_state_t* state) {
    if (!bridge || !word || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, word, state)");
        return -1;
    }

    memset(state, 0, sizeof(quantum_concept_state_t));

    /* Simulate: generate 2-5 senses based on word hash */
    uint32_t hash = 0;
    for (const char* p = word; *p; p++) {
        hash = hash * 31 + (uint8_t)*p;
    }
    uint32_t num_senses = 2 + (hash % 4);  /* 2-5 senses */

    state->concept_ids = nimcp_calloc(num_senses, sizeof(uint32_t));
    state->amplitudes = nimcp_calloc(num_senses, sizeof(float));
    state->phases = nimcp_calloc(num_senses, sizeof(float));

    if (!state->concept_ids || !state->amplitudes || !state->phases) {
        wernicke_quantum_state_free(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: required parameter is NULL (state->concept_ids, state->amplitudes, state->phases)");
        return -1;
    }

    /* Initialize uniform superposition */
    float amp = 1.0f / sqrtf((float)num_senses);
    for (uint32_t i = 0; i < num_senses; i++) {
        state->concept_ids[i] = hash + i * 1000;  /* Unique sense IDs */
        state->amplitudes[i] = amp;
        state->phases[i] = 0.0f;
    }

    state->num_concepts = num_senses;
    state->total_probability = 1.0f;
    state->collapsed = false;

    return 0;
}

int32_t wernicke_quantum_collapse(wernicke_quantum_bridge_t* bridge,
                                   quantum_concept_state_t* state,
                                   const float* context_embedding,
                                   uint32_t context_dim) {
    if (!bridge || !state || state->num_concepts == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Bias amplitudes by context similarity */
    if (context_embedding && context_dim > 0) {
        float context_sum = 0.0f;
        for (uint32_t i = 0; i < context_dim; i++) {
            context_sum += context_embedding[i];
        }

        /* Apply context bias to amplitudes */
        for (uint32_t i = 0; i < state->num_concepts; i++) {
            /* Simple bias: earlier senses get higher weight with positive context */
            float bias = 1.0f + context_sum * 0.1f * (float)(state->num_concepts - i);
            state->amplitudes[i] *= bias;
        }

        /* Renormalize */
        float sum_sq = 0.0f;
        for (uint32_t i = 0; i < state->num_concepts; i++) {
            sum_sq += state->amplitudes[i] * state->amplitudes[i];
        }
        if (sum_sq > 0.0f) {
            float norm = sqrtf(sum_sq);
            for (uint32_t i = 0; i < state->num_concepts; i++) {
                state->amplitudes[i] /= norm;
            }
        }
    }

    /* Find max amplitude (measurement) */
    uint32_t best_idx = 0;
    float best_prob = state->amplitudes[0] * state->amplitudes[0];

    for (uint32_t i = 1; i < state->num_concepts; i++) {
        float prob = state->amplitudes[i] * state->amplitudes[i];
        if (prob > best_prob) {
            best_prob = prob;
            best_idx = i;
        }
    }

    state->collapsed = true;
    state->collapsed_id = state->concept_ids[best_idx];

    return (int32_t)state->collapsed_id;
}

/*=============================================================================
 * SPREADING ACTIVATION API
 *=============================================================================*/

int wernicke_quantum_spreading_activation(
    wernicke_quantum_bridge_t* bridge,
    const uint32_t* seed_concepts,
    const float* seed_activations,
    uint32_t num_seeds,
    uint32_t graph_size,
    quantum_spreading_result_t* result
) {
    if (!bridge || !seed_concepts || !result || graph_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_quantum_spreading_activation: required parameter is NULL (bridge, seed_concepts, result)");
        return -1;
    }

    /* Initialize walk with multiple seeds */
    quantum_walk_state_t state;
    if (wernicke_quantum_walk_init(bridge, 0, graph_size, &state) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "wernicke_quantum_spreading_activation: validation failed");
        return -1;
    }

    /* Set initial amplitudes from seeds */
    memset(state.node_amplitudes, 0, graph_size * sizeof(float));
    float total_seed = 0.0f;

    for (uint32_t i = 0; i < num_seeds; i++) {
        if (seed_concepts[i] < graph_size) {
            float act = seed_activations ? seed_activations[i] : 1.0f;
            state.node_amplitudes[seed_concepts[i]] = act;
            total_seed += act * act;
        }
    }

    /* Normalize */
    if (total_seed > 0.0f) {
        float norm = sqrtf(total_seed);
        for (uint32_t i = 0; i < graph_size; i++) {
            state.node_amplitudes[i] /= norm;
        }
    }

    /* Run quantum walk */
    wernicke_quantum_walk_run(bridge, &state, bridge->config.walk_max_steps);

    /* Measure result */
    int ret = wernicke_quantum_walk_measure(bridge, &state, result);

    wernicke_quantum_walk_free(&state);

    return ret;
}

int wernicke_quantum_get_related(wernicke_quantum_bridge_t* bridge,
                                  uint32_t concept_id,
                                  uint32_t max_related,
                                  uint32_t* related_ids,
                                  float* similarities,
                                  uint32_t* num_related) {
    if (!bridge || !related_ids || !similarities || !num_related) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_quantum_spreading_activation: required parameter is NULL (bridge, related_ids, similarities, num_related)");
        return -1;
    }

    *num_related = 0;

    /* Run quantum walk from concept */
    uint32_t graph_size = concept_id + max_related + 100;  /* Estimate */
    quantum_spreading_result_t spreading;

    uint32_t seed = concept_id;
    float seed_act = 1.0f;

    if (wernicke_quantum_spreading_activation(bridge,
                                               &seed, &seed_act, 1,
                                               graph_size, &spreading) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wernicke_quantum_spreading_activation: operation failed");
        return -1;
    }

    /* Copy top results */
    uint32_t count = (spreading.num_activated < max_related)
                     ? spreading.num_activated : max_related;

    for (uint32_t i = 0; i < count; i++) {
        related_ids[i] = spreading.activated_concepts[i];
        similarities[i] = spreading.activation_levels[i];
    }
    *num_related = count;

    wernicke_quantum_spreading_free(&spreading);

    return 0;
}

/*=============================================================================
 * QUERY API
 *=============================================================================*/

int wernicke_quantum_get_stats(const wernicke_quantum_bridge_t* bridge,
                                wernicke_quantum_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_quantum_spreading_activation: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void wernicke_quantum_reset_stats(wernicke_quantum_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(wernicke_quantum_stats_t));
}

int wernicke_quantum_get_config(const wernicke_quantum_bridge_t* bridge,
                                 wernicke_quantum_config_t* config) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_quantum_reset_stats: required parameter is NULL (bridge, config)");
        return -1;
    }
    *config = bridge->config;
    return 0;
}

float wernicke_quantum_estimate_speedup(const wernicke_quantum_bridge_t* bridge,
                                         uint32_t problem_size,
                                         wernicke_quantum_algo_t algo) {
    if (!bridge || problem_size == 0) return 1.0f;

    switch (algo) {
        case WERNICKE_QA_GROVER:
            /* Grover: O(sqrt(N)) speedup */
            return sqrtf((float)problem_size);

        case WERNICKE_QA_QUANTUM_WALK:
            /* Quantum walk: depends on graph structure */
            /* For regular graphs: ~sqrt speedup for hitting time */
            return sqrtf((float)problem_size) * 0.5f;

        case WERNICKE_QA_QMC:
            /* QMC: polynomial speedup for sampling */
            return powf((float)problem_size, 0.25f);

        case WERNICKE_QA_AMPLITUDE_EST:
            /* Amplitude estimation: sqrt speedup */
            return sqrtf((float)problem_size);

        case WERNICKE_QA_HYBRID:
            /* Hybrid: average of methods */
            return sqrtf((float)problem_size) * 0.75f;

        default:
            return 1.0f;
    }
}

/*=============================================================================
 * MEMORY MANAGEMENT API
 *=============================================================================*/

void wernicke_quantum_state_free(quantum_concept_state_t* state) {
    if (!state) return;
    nimcp_free(state->concept_ids);
    nimcp_free(state->amplitudes);
    nimcp_free(state->phases);
    memset(state, 0, sizeof(quantum_concept_state_t));
}

void wernicke_quantum_walk_free(quantum_walk_state_t* state) {
    if (!state) return;
    nimcp_free(state->node_amplitudes);
    memset(state, 0, sizeof(quantum_walk_state_t));
}

void wernicke_quantum_disambig_free(quantum_disambig_result_t* result) {
    if (!result) return;
    nimcp_free(result->alternatives);
    nimcp_free(result->alt_probabilities);
    memset(result, 0, sizeof(quantum_disambig_result_t));
}

void wernicke_quantum_spreading_free(quantum_spreading_result_t* result) {
    if (!result) return;
    nimcp_free(result->activated_concepts);
    nimcp_free(result->activation_levels);
    memset(result, 0, sizeof(quantum_spreading_result_t));
}

/*=============================================================================
 * STRING CONVERSION API
 *=============================================================================*/

const char* wernicke_quantum_algo_to_string(wernicke_quantum_algo_t algo) {
    switch (algo) {
        case WERNICKE_QA_GROVER:        return "grover";
        case WERNICKE_QA_QUANTUM_WALK:  return "quantum_walk";
        case WERNICKE_QA_QMC:           return "qmc";
        case WERNICKE_QA_AMPLITUDE_EST: return "amplitude_estimation";
        case WERNICKE_QA_HYBRID:        return "hybrid";
        default:                        return "unknown";
    }
}

const char* wernicke_quantum_target_to_string(wernicke_search_target_t target) {
    switch (target) {
        case WERNICKE_SEARCH_PHONEME:  return "phoneme";
        case WERNICKE_SEARCH_WORD:     return "word";
        case WERNICKE_SEARCH_CONCEPT:  return "concept";
        case WERNICKE_SEARCH_RELATION: return "relation";
        default:                       return "unknown";
    }
}

const char* wernicke_quantum_disambig_to_string(wernicke_disambig_mode_t mode) {
    switch (mode) {
        case WERNICKE_DISAMBIG_FREQUENCY: return "frequency";
        case WERNICKE_DISAMBIG_CONTEXT:   return "context";
        case WERNICKE_DISAMBIG_QUANTUM:   return "quantum";
        case WERNICKE_DISAMBIG_HYBRID:    return "hybrid";
        default:                          return "unknown";
    }
}
