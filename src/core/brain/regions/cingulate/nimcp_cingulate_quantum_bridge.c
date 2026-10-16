/**
 * @file nimcp_cingulate_quantum_bridge.c
 * @brief Implementation of quantum bridge for Cingulate Cortex
 *
 * WHAT: Quantum-accelerated conflict resolution and error propagation
 * WHY:  Leverage quantum computing paradigms for faster decision making
 * HOW:  Implements Grover-like search and quantum interference
 *
 * @version Phase B4: Cingulate Cortex Integration
 * @date 2025-12-30
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/cingulate/nimcp_cingulate_quantum_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(cingulate_quantum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "CINGULATE_QUANTUM_BRIDGE"


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define CING_QUANTUM_LOG_MODULE "CING_QUANTUM"

/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define MAX_QUANTUM_STATES 256  /* 2^8 = 256 basis states max */
#define GROVER_OPTIMAL_ITERATIONS(n) ((int)(M_PI * sqrt((double)(n)) / 4.0))

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Internal quantum bridge structure
 */
struct cingulate_quantum_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    cingulate_quantum_config_t config;

    /* Connected cingulate adapter */
    cingulate_adapter_t* cingulate;

    /* Quantum state */
    float* amplitudes;          /**< Complex amplitudes (real part) */
    float* phases;              /**< Complex phases */
    uint32_t num_qubits;        /**< Current number of qubits */
    uint32_t num_states;        /**< 2^num_qubits */
    bool state_initialized;

    /* Option encoding */
    cingulate_quantum_option_t* encoded_options;
    uint32_t num_encoded_options;

    /* Control superposition */
    float* control_levels;
    uint32_t num_control_levels;
    float* control_amplitudes;

    /* Random state */
    uint64_t rng_state;

    /* Statistics */
    cingulate_quantum_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Simple xorshift64 PRNG
 */
static uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

/**
 * @brief Random float in [0, 1)
 */
static float random_float(uint64_t* state) {
    return (float)(xorshift64(state) & 0xFFFFFFFF) / 4294967296.0f;
}

/**
 * @brief Normalize quantum state (sum of |amplitude|^2 = 1)
 */
static void normalize_state(float* amplitudes, uint32_t num_states) {
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < num_states; i++) {
        sum_sq += amplitudes[i] * amplitudes[i];
    }
    if (sum_sq > 1e-10f) {
        float norm = 1.0f / sqrtf(sum_sq);
        for (uint32_t i = 0; i < num_states; i++) {
            amplitudes[i] *= norm;
        }
    }
}

/**
 * @brief Apply Hadamard-like diffusion operator
 *
 * The diffusion operator: D = 2|s><s| - I
 * where |s> is the uniform superposition
 */
static void apply_diffusion(float* amplitudes, uint32_t num_states) {
    /* Compute mean amplitude */
    float mean = 0.0f;
    for (uint32_t i = 0; i < num_states; i++) {
        mean += amplitudes[i];
    }
    mean /= (float)num_states;

    /* Apply inversion about the mean: 2*mean - amplitude */
    for (uint32_t i = 0; i < num_states; i++) {
        amplitudes[i] = 2.0f * mean - amplitudes[i];
    }
}

/**
 * @brief Apply oracle marking good states (phase flip)
 */
static void apply_oracle(float* amplitudes, const bool* marked, uint32_t num_states) {
    for (uint32_t i = 0; i < num_states; i++) {
        if (marked[i]) {
            amplitudes[i] = -amplitudes[i];  /* Phase flip */
        }
    }
}

/**
 * @brief Measure state (probabilistic collapse)
 */
static uint32_t measure_state(const float* amplitudes, uint32_t num_states, uint64_t* rng) {
    /* Compute cumulative probability */
    float* cumulative = (float*)nimcp_malloc(num_states * sizeof(float));
    if (!cumulative) return 0;

    float total = 0.0f;
    for (uint32_t i = 0; i < num_states; i++) {
        total += amplitudes[i] * amplitudes[i];
        cumulative[i] = total;
    }

    /* Sample */
    float r = random_float(rng) * total;
    uint32_t result = 0;
    for (uint32_t i = 0; i < num_states; i++) {
        if (r <= cumulative[i]) {
            result = i;
            break;
        }
    }

    nimcp_free(cumulative);
    return result;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

cingulate_quantum_config_t cingulate_quantum_default_config(void) {
    cingulate_quantum_config_t config;
    memset(&config, 0, sizeof(config));

    config.max_qubits = CINGULATE_QUANTUM_DEFAULT_MAX_QUBITS;
    config.max_iterations = CINGULATE_QUANTUM_DEFAULT_MAX_ITERATIONS;
    config.min_confidence = CINGULATE_QUANTUM_DEFAULT_MIN_CONFIDENCE;
    config.interference_gain = CINGULATE_QUANTUM_DEFAULT_INTERFERENCE_GAIN;

    config.enable_error_propagation = true;
    config.error_phase_shift = (float)M_PI;

    config.enable_classical_fallback = true;
    config.seed = 42;

    return config;
}

cingulate_quantum_bridge_t* cingulate_quantum_bridge_create(
    cingulate_adapter_t* cingulate,
    const cingulate_quantum_config_t* config) {

    LOG_INFO("[%s] Creating cingulate quantum bridge", CING_QUANTUM_LOG_MODULE);

    cingulate_quantum_bridge_t* bridge = (cingulate_quantum_bridge_t*)nimcp_calloc(
        1, sizeof(cingulate_quantum_bridge_t));
    if (!bridge) {
        LOG_ERROR("[%s] Failed to allocate bridge memory", CING_QUANTUM_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cingulate_quantum_default_config();
    }

    /* Limit qubits to prevent excessive memory usage */
    if (bridge->config.max_qubits > 8) {
        bridge->config.max_qubits = 8;
        LOG_WARNING("[%s] Limiting max_qubits to 8", CING_QUANTUM_LOG_MODULE);
    }

    /* Allocate quantum state buffers */
    uint32_t max_states = 1u << bridge->config.max_qubits;

    bridge->amplitudes = (float*)nimcp_calloc(max_states, sizeof(float));
    bridge->phases = (float*)nimcp_calloc(max_states, sizeof(float));
    bridge->encoded_options = (cingulate_quantum_option_t*)nimcp_calloc(
        max_states, sizeof(cingulate_quantum_option_t));

    if (!bridge->amplitudes || !bridge->phases || !bridge->encoded_options) {
        LOG_ERROR("[%s] Failed to allocate quantum state buffers", CING_QUANTUM_LOG_MODULE);
        cingulate_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cingulate_quantum_default_config: required parameter is NULL (bridge->amplitudes, bridge->phases, bridge->encoded_options)");
        return NULL;
    }

    /* Initialize RNG */
    bridge->rng_state = bridge->config.seed;

    /* Connect to cingulate adapter */
    bridge->cingulate = cingulate;

    LOG_INFO("[%s] Cingulate quantum bridge created (max_qubits=%u)",
             CING_QUANTUM_LOG_MODULE, bridge->config.max_qubits);

    return bridge;
}

void cingulate_quantum_bridge_destroy(cingulate_quantum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cingulate_quantum");

    LOG_INFO("[%s] Destroying cingulate quantum bridge", CING_QUANTUM_LOG_MODULE);

    if (bridge->amplitudes) nimcp_free(bridge->amplitudes);
    if (bridge->phases) nimcp_free(bridge->phases);
    if (bridge->encoded_options) nimcp_free(bridge->encoded_options);
    if (bridge->control_levels) nimcp_free(bridge->control_levels);
    if (bridge->control_amplitudes) nimcp_free(bridge->control_amplitudes);

    nimcp_free(bridge);
}

bool cingulate_quantum_bridge_reset(cingulate_quantum_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_quantum_bridge_reset: bridge is NULL");
        return false;
    }

    uint32_t max_states = 1u << bridge->config.max_qubits;

    memset(bridge->amplitudes, 0, max_states * sizeof(float));
    memset(bridge->phases, 0, max_states * sizeof(float));
    memset(bridge->encoded_options, 0, max_states * sizeof(cingulate_quantum_option_t));

    bridge->num_qubits = 0;
    bridge->num_states = 0;
    bridge->num_encoded_options = 0;
    bridge->state_initialized = false;

    return true;
}

/*=============================================================================
 * QUANTUM CONFLICT RESOLUTION
 *===========================================================================*/

bool cingulate_quantum_encode_options(
    cingulate_quantum_bridge_t* bridge,
    const cingulate_response_option_t* options,
    uint32_t num_options) {

    if (!bridge || !options || num_options == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_quantum_bridge_reset: required parameter is NULL (bridge, options)");
        return false;
    }

    /* Determine number of qubits needed */
    uint32_t qubits_needed = 1;
    while ((1u << qubits_needed) < num_options) {
        qubits_needed++;
    }

    if (qubits_needed > bridge->config.max_qubits) {
        LOG_WARNING("[%s] Too many options (%u), limiting to %u",
                    CING_QUANTUM_LOG_MODULE, num_options, 1u << bridge->config.max_qubits);
        num_options = 1u << bridge->config.max_qubits;
        qubits_needed = bridge->config.max_qubits;
    }

    bridge->num_qubits = qubits_needed;
    bridge->num_states = 1u << qubits_needed;
    bridge->num_encoded_options = num_options;

    /* Initialize uniform superposition */
    float uniform_amp = 1.0f / sqrtf((float)bridge->num_states);
    for (uint32_t i = 0; i < bridge->num_states; i++) {
        bridge->amplitudes[i] = uniform_amp;
        bridge->phases[i] = 0.0f;
    }

    /* Encode options */
    for (uint32_t i = 0; i < num_options; i++) {
        bridge->encoded_options[i].option_id = options[i].option_id;
        bridge->encoded_options[i].initial_amplitude = uniform_amp;
        bridge->encoded_options[i].activation = options[i].activation;
        bridge->encoded_options[i].constraint_satisfaction = 1.0f - options[i].activation * 0.5f;
        bridge->encoded_options[i].is_marked = false;
    }

    /* Weight amplitudes by activation (biased superposition) */
    float activation_sum = 0.0f;
    for (uint32_t i = 0; i < num_options; i++) {
        activation_sum += options[i].activation;
    }
    if (activation_sum > 0.0f) {
        for (uint32_t i = 0; i < num_options; i++) {
            /* Mix uniform and activation-weighted */
            float weighted = options[i].activation / activation_sum;
            bridge->amplitudes[i] = 0.5f * uniform_amp + 0.5f * sqrtf(weighted);
        }
        normalize_state(bridge->amplitudes, bridge->num_states);
    }

    bridge->state_initialized = true;

    LOG_DEBUG("[%s] Encoded %u options into %u-qubit state",
              CING_QUANTUM_LOG_MODULE, num_options, qubits_needed);

    return true;
}

bool cingulate_quantum_apply_constraints(
    cingulate_quantum_bridge_t* bridge,
    const cingulate_conflict_t* conflict) {

    if (!bridge || !conflict || !bridge->state_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_quantum_bridge_reset: required parameter is NULL (bridge, conflict, bridge->state_initialized)");
        return false;
    }

    /* Apply phase to conflicting options based on conflict level */
    uint32_t opt_a = conflict->option_a_id;
    uint32_t opt_b = conflict->option_b_id;

    if (opt_a < bridge->num_states && opt_b < bridge->num_states) {
        /* Apply destructive interference to conflicting pair */
        float interference = conflict->conflict_level * bridge->config.interference_gain;

        /* Reduce amplitude of less-activated option */
        if (bridge->encoded_options[opt_a].activation <
            bridge->encoded_options[opt_b].activation) {
            bridge->amplitudes[opt_a] *= (1.0f - interference);
        } else {
            bridge->amplitudes[opt_b] *= (1.0f - interference);
        }

        normalize_state(bridge->amplitudes, bridge->num_states);
    }

    LOG_DEBUG("[%s] Applied conflict constraints (conflict_level=%.3f)",
              CING_QUANTUM_LOG_MODULE, conflict->conflict_level);

    return true;
}

bool cingulate_quantum_resolve_conflict(
    cingulate_quantum_bridge_t* bridge,
    cingulate_quantum_result_t* result) {

    if (!bridge || !result || !bridge->state_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_quantum_bridge_reset: required parameter is NULL (bridge, result, bridge->state_initialized)");
        return false;
    }

    memset(result, 0, sizeof(cingulate_quantum_result_t));

    bridge->stats.resolutions_attempted++;

    /* Mark "good" options (those that resolve conflict) */
    bool* marked = (bool*)nimcp_calloc(bridge->num_states, sizeof(bool));
    if (!marked) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cingulate_quantum_bridge_reset: marked is NULL");
        return false;
    }

    /* Good options: high activation, low conflict contribution */
    uint32_t num_marked = 0;
    float max_activation = 0.0f;
    for (uint32_t i = 0; i < bridge->num_encoded_options; i++) {
        if (bridge->encoded_options[i].activation > max_activation) {
            max_activation = bridge->encoded_options[i].activation;
        }
    }

    /* Check if options have sufficient differentiation */
    float min_activation = max_activation;
    for (uint32_t i = 0; i < bridge->num_encoded_options; i++) {
        if (bridge->encoded_options[i].activation < min_activation) {
            min_activation = bridge->encoded_options[i].activation;
        }
    }
    float activation_range = max_activation - min_activation;

    for (uint32_t i = 0; i < bridge->num_encoded_options; i++) {
        /* Mark options that are good resolutions */
        float score = bridge->encoded_options[i].activation /
                      (max_activation + 0.01f);
        /* Require relative score above 0.5 AND sufficient differentiation
         * among options to identify clear winners */
        if (score > 0.5f && activation_range > 0.1f) {
            marked[i] = true;
            bridge->encoded_options[i].is_marked = true;
            num_marked++;
        }
    }

    if (num_marked == 0) {
        /* No clear winner - use classical fallback */
        if (bridge->config.enable_classical_fallback) {
            result->selected_option = 0;
            float best_activation = 0.0f;
            for (uint32_t i = 0; i < bridge->num_encoded_options; i++) {
                if (bridge->encoded_options[i].activation > best_activation) {
                    best_activation = bridge->encoded_options[i].activation;
                    result->selected_option = i;
                }
            }
            result->selection_confidence = best_activation;
            result->used_fallback = true;
            bridge->stats.fallbacks_used++;
        }
        nimcp_free(marked);
        return true;
    }

    /* Determine optimal number of Grover iterations */
    uint32_t optimal_iterations = GROVER_OPTIMAL_ITERATIONS(bridge->num_states);
    if (optimal_iterations > bridge->config.max_iterations) {
        optimal_iterations = bridge->config.max_iterations;
    }
    if (optimal_iterations < 1) optimal_iterations = 1;

    /* Apply Grover iterations */
    for (uint32_t iter = 0; iter < optimal_iterations; iter++) {
        apply_oracle(bridge->amplitudes, marked, bridge->num_states);
        apply_diffusion(bridge->amplitudes, bridge->num_states);
    }

    /* Measure the result */
    uint32_t measured = measure_state(bridge->amplitudes, bridge->num_states, &bridge->rng_state);

    /* Fill result */
    result->selected_option = (measured < bridge->num_encoded_options) ?
                              bridge->encoded_options[measured].option_id : 0;
    result->selection_confidence = bridge->amplitudes[measured] * bridge->amplitudes[measured];
    result->iterations_used = optimal_iterations;

    /* Estimate speedup */
    float classical_iterations = (float)bridge->num_states / 2.0f;
    result->speedup_achieved = classical_iterations / (float)optimal_iterations;

    /* Compute conflict resolution degree */
    result->conflict_resolution = fminf(1.0f, result->selection_confidence * 2.0f);

    result->used_fallback = false;

    /* Update stats */
    bridge->stats.resolutions_successful++;
    bridge->stats.avg_iterations =
        (bridge->stats.avg_iterations * (bridge->stats.resolutions_successful - 1) +
         (float)optimal_iterations) / bridge->stats.resolutions_successful;
    bridge->stats.avg_confidence =
        (bridge->stats.avg_confidence * (bridge->stats.resolutions_successful - 1) +
         result->selection_confidence) / bridge->stats.resolutions_successful;
    bridge->stats.avg_speedup =
        (bridge->stats.avg_speedup * (bridge->stats.resolutions_successful - 1) +
         result->speedup_achieved) / bridge->stats.resolutions_successful;

    nimcp_free(marked);

    LOG_DEBUG("[%s] Resolved conflict: option=%u, confidence=%.3f, iterations=%u",
              CING_QUANTUM_LOG_MODULE, result->selected_option,
              result->selection_confidence, optimal_iterations);

    return true;
}

bool cingulate_quantum_get_probabilities(
    const cingulate_quantum_bridge_t* bridge,
    float* probabilities,
    uint32_t num_options) {

    if (!bridge || !probabilities || !bridge->state_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, probabilities, bridge->state_initialized)");
        return false;
    }

    uint32_t count = (num_options < bridge->num_states) ? num_options : bridge->num_states;

    for (uint32_t i = 0; i < count; i++) {
        probabilities[i] = bridge->amplitudes[i] * bridge->amplitudes[i];
    }

    return true;
}

/*=============================================================================
 * QUANTUM ERROR PROPAGATION
 *===========================================================================*/

bool cingulate_quantum_encode_error(
    cingulate_quantum_bridge_t* bridge,
    const cingulate_error_event_t* error,
    cingulate_quantum_error_t* quantum_error) {

    if (!bridge || !error || !quantum_error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, error, quantum_error)");
        return false;
    }

    memset(quantum_error, 0, sizeof(cingulate_quantum_error_t));

    /* Encode error magnitude as phase */
    quantum_error->error_magnitude = error->error_magnitude;
    quantum_error->phase = bridge->config.error_phase_shift * error->error_magnitude;
    quantum_error->source_option = error->executed_option;

    /* Determine affected options (all except the intended one) */
    quantum_error->num_targets = 0;
    for (uint32_t i = 0; i < bridge->num_encoded_options && quantum_error->num_targets < 16; i++) {
        if (bridge->encoded_options[i].option_id != error->intended_option) {
            quantum_error->target_options[quantum_error->num_targets++] =
                bridge->encoded_options[i].option_id;
        }
    }

    return true;
}

bool cingulate_quantum_propagate_error(
    cingulate_quantum_bridge_t* bridge,
    const cingulate_quantum_error_t* quantum_error) {

    if (!bridge || !quantum_error || !bridge->state_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, quantum_error, bridge->state_initialized)");
        return false;
    }

    /* Apply phase rotation to error-causing option */
    uint32_t source = quantum_error->source_option;
    if (source < bridge->num_states) {
        /* Phase kick-back: rotate amplitude by error phase */
        float cos_phase = cosf(quantum_error->phase);
        float sin_phase = sinf(quantum_error->phase);

        float old_amp = bridge->amplitudes[source];
        float old_phase = bridge->phases[source];

        /* Rotate in complex plane (simplified to real) */
        bridge->amplitudes[source] = old_amp * cos_phase;
        bridge->phases[source] = old_phase + quantum_error->phase;

        /* Redistribute probability to other options */
        float lost_prob = old_amp * old_amp - bridge->amplitudes[source] * bridge->amplitudes[source];
        if (lost_prob > 0.0f && bridge->num_states > 1) {
            float boost = sqrtf(lost_prob / (float)(bridge->num_states - 1));
            for (uint32_t i = 0; i < bridge->num_states; i++) {
                if (i != source) {
                    bridge->amplitudes[i] += boost * 0.1f;  /* Small boost */
                }
            }
        }

        normalize_state(bridge->amplitudes, bridge->num_states);
    }

    bridge->stats.errors_propagated++;

    LOG_DEBUG("[%s] Propagated error: source=%u, phase=%.3f",
              CING_QUANTUM_LOG_MODULE, source, quantum_error->phase);

    return true;
}

bool cingulate_quantum_error_gradient(
    cingulate_quantum_bridge_t* bridge,
    const cingulate_quantum_error_t* quantum_error,
    float* gradients,
    uint32_t num_options) {

    if (!bridge || !quantum_error || !gradients) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, quantum_error, gradients)");
        return false;
    }

    uint32_t count = (num_options < bridge->num_encoded_options) ?
                      num_options : bridge->num_encoded_options;

    float error_mag = quantum_error->error_magnitude;

    for (uint32_t i = 0; i < count; i++) {
        if (bridge->encoded_options[i].option_id == quantum_error->source_option) {
            /* Error-causing option gets negative gradient */
            gradients[i] = -error_mag * 2.0f * bridge->amplitudes[i];
        } else {
            /* Other options get small positive gradient */
            gradients[i] = error_mag * 0.2f * bridge->amplitudes[i];
        }
    }

    /* Update stats */
    float total_grad = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        total_grad += fabsf(gradients[i]);
    }
    bridge->stats.avg_error_gradient =
        (bridge->stats.avg_error_gradient * bridge->stats.errors_propagated +
         total_grad) / (bridge->stats.errors_propagated + 1);

    return true;
}

/*=============================================================================
 * SUPERPOSITION EVALUATION
 *===========================================================================*/

bool cingulate_quantum_superpose_control(
    cingulate_quantum_bridge_t* bridge,
    float min_control,
    float max_control,
    uint32_t num_levels) {

    if (!bridge || num_levels == 0 || num_levels > MAX_QUANTUM_STATES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: bridge is NULL");
        return false;
    }

    /* Allocate control level buffers if needed */
    if (bridge->control_levels) nimcp_free(bridge->control_levels);
    if (bridge->control_amplitudes) nimcp_free(bridge->control_amplitudes);

    bridge->control_levels = (float*)nimcp_calloc(num_levels, sizeof(float));
    bridge->control_amplitudes = (float*)nimcp_calloc(num_levels, sizeof(float));

    if (!bridge->control_levels || !bridge->control_amplitudes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: required parameter is NULL (bridge->control_levels, bridge->control_amplitudes)");
        return false;
    }

    bridge->num_control_levels = num_levels;

    /* Initialize uniform superposition over control levels */
    float uniform_amp = 1.0f / sqrtf((float)num_levels);
    float step = (max_control - min_control) / (float)(num_levels - 1);

    for (uint32_t i = 0; i < num_levels; i++) {
        bridge->control_levels[i] = min_control + step * (float)i;
        bridge->control_amplitudes[i] = uniform_amp;
    }

    return true;
}

bool cingulate_quantum_evaluate_control(
    cingulate_quantum_bridge_t* bridge,
    const cingulate_conflict_t* conflict,
    const cingulate_error_event_t* error) {

    if (!bridge || !bridge->control_amplitudes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, bridge->control_amplitudes)");
        return false;
    }

    /* Evaluate each control level */
    for (uint32_t i = 0; i < bridge->num_control_levels; i++) {
        float control = bridge->control_levels[i];

        /* Compute score based on how well this control level addresses conflict/error */
        float score = 0.5f;  /* Baseline */

        if (conflict) {
            /* High conflict needs high control */
            float conflict_need = conflict->conflict_level;
            float match = 1.0f - fabsf(control - conflict_need);
            score += 0.25f * match;
        }

        if (error) {
            /* Errors need increased control */
            float error_need = fminf(1.0f, 0.5f + error->error_magnitude);
            float match = 1.0f - fabsf(control - error_need);
            score += 0.25f * match;
        }

        /* Modulate amplitude by score */
        bridge->control_amplitudes[i] *= sqrtf(score);
    }

    normalize_state(bridge->control_amplitudes, bridge->num_control_levels);

    return true;
}

bool cingulate_quantum_measure_control(
    cingulate_quantum_bridge_t* bridge,
    float* optimal_control,
    float* confidence) {

    if (!bridge || !optimal_control || !confidence || !bridge->control_amplitudes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, optimal_control, confidence, bridge->control_amplitudes)");
        return false;
    }

    /* Measure the control superposition */
    uint32_t measured = measure_state(bridge->control_amplitudes,
                                       bridge->num_control_levels,
                                       &bridge->rng_state);

    *optimal_control = bridge->control_levels[measured];
    *confidence = bridge->control_amplitudes[measured] * bridge->control_amplitudes[measured];

    return true;
}

/*=============================================================================
 * INTEGRATION WITH CINGULATE ADAPTER
 *===========================================================================*/

bool cingulate_quantum_bridge_update(cingulate_quantum_bridge_t* bridge) {
    if (!bridge || !bridge->cingulate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_quantum_bridge_update: required parameter is NULL (bridge, bridge->cingulate)");
        return false;
    }

    /* Could sync state from cingulate adapter here */
    /* For now, just return success */

    return true;
}

bool cingulate_quantum_apply_resolution(
    cingulate_quantum_bridge_t* bridge,
    const cingulate_quantum_result_t* result) {

    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_quantum_bridge_update: required parameter is NULL (bridge, result)");
        return false;
    }

    if (bridge->cingulate) {
        /* Could apply resolution to cingulate adapter */
        /* Generate control signal based on quantum result */
        cingulate_control_signal_t signal;
        memset(&signal, 0, sizeof(signal));

        signal.control_level = result->selection_confidence;
        signal.adjustment_magnitude = 1.0f - result->conflict_resolution;

        cingulate_generate_control_signal(bridge->cingulate, &signal);
    }

    return true;
}

bool cingulate_quantum_full_resolution(
    cingulate_quantum_bridge_t* bridge,
    cingulate_quantum_result_t* result) {

    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_quantum_bridge_update: required parameter is NULL (bridge, result)");
        return false;
    }

    /* This would be the complete pipeline:
     * 1. Get current state from cingulate
     * 2. Encode options
     * 3. Apply constraints
     * 4. Resolve
     * 5. Apply back
     *
     * For now, just resolve if state is initialized
     */

    if (!bridge->state_initialized) {
        LOG_WARNING("[%s] Full resolution called but state not initialized",
                    CING_QUANTUM_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_quantum_bridge_update: bridge->state_initialized is NULL");
        return false;
    }

    if (!cingulate_quantum_resolve_conflict(bridge, result)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cingulate_quantum_bridge_update: cingulate_quantum_resolve_conflict is NULL");
        return false;
    }

    if (!cingulate_quantum_apply_resolution(bridge, result)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cingulate_quantum_bridge_update: cingulate_quantum_apply_resolution is NULL");
        return false;
    }

    return true;
}

/*=============================================================================
 * STATISTICS AND DIAGNOSTICS
 *===========================================================================*/

bool cingulate_quantum_get_stats(
    const cingulate_quantum_bridge_t* bridge,
    cingulate_quantum_stats_t* stats) {

    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_quantum_bridge_update: required parameter is NULL (bridge, stats)");
        return false;
    }
    *stats = bridge->stats;
    return true;
}

bool cingulate_quantum_get_config(
    const cingulate_quantum_bridge_t* bridge,
    cingulate_quantum_config_t* config) {

    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_quantum_bridge_update: required parameter is NULL (bridge, config)");
        return false;
    }
    *config = bridge->config;
    return true;
}

bool cingulate_quantum_get_state(
    const cingulate_quantum_bridge_t* bridge,
    cingulate_quantum_state_t* state) {

    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cingulate_quantum_bridge_update: required parameter is NULL (bridge, state)");
        return false;
    }

    state->num_qubits = bridge->num_qubits;
    state->num_states = bridge->num_states;

    /* Compute total probability */
    float total = 0.0f;
    for (uint32_t i = 0; i < bridge->num_states; i++) {
        total += bridge->amplitudes[i] * bridge->amplitudes[i];
    }
    state->total_probability = total;

    /* Copy amplitudes if caller provided buffer */
    if (state->amplitudes && bridge->num_states > 0) {
        memcpy(state->amplitudes, bridge->amplitudes, bridge->num_states * sizeof(float));
    }

    return true;
}
