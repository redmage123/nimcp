/**
 * @file nimcp_insula_quantum_bridge.c
 * @brief Quantum Insula Bridge Implementation
 *
 * Integrates quantum algorithms with Insula for optimized
 * interoceptive integration, emotional evaluation, and somatic marker search.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/insula/nimcp_insula_quantum_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(insula_quantum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "INSULA_QUANTUM_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURE
 *===========================================================================*/

struct insula_quantum_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* insula;                            /**< Insula adapter handle */
    insula_quantum_config_t config;          /**< Configuration */
    qreason_t quantum_reasoner;              /**< Quantum reasoning engine */
    insula_quantum_stats_t stats;            /**< Statistics */

    /* Interoceptive state */
    quantum_intero_state_t* intero_states;
    uint32_t num_intero_channels;

    /* Emotion hypotheses */
    quantum_emotion_hypothesis_t* emotion_hypotheses;
    uint32_t num_emotion_hypotheses;

    /* Somatic marker candidates */
    quantum_somatic_marker_t* somatic_candidates;
    uint32_t max_somatic_candidates;

    /* RNG state */
    uint32_t rng_state;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

static uint32_t quantum_rand(uint32_t* state) {
    *state = *state * 1103515245 + 12345;
    return (*state >> 16) & 0x7FFF;
}

static float quantum_randf(uint32_t* state) {
    return (float)quantum_rand(state) / 32767.0f;
}

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

insula_quantum_config_t insula_quantum_default_config(void) {
    return (insula_quantum_config_t){
        .enabled = true,
        .intero_channels = 16,
        .emotion_superposition_size = 8,
        .max_grover_iterations = 10,
        .min_confidence_threshold = NIMCP_CONFIDENCE_MIN,
        .enable_interference = true,
        .use_superposition = true,
        .noise_tolerance = 0.1f,
        .seed = 42
    };
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

insula_quantum_bridge_t* insula_quantum_bridge_create(
    void* insula,
    const insula_quantum_config_t* config
) {
    insula_quantum_bridge_t* bridge = nimcp_calloc(1, sizeof(insula_quantum_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    bridge->insula = insula;
    bridge->config = config ? *config : insula_quantum_default_config();

    /* Create quantum reasoner */
    qreason_config_t qconfig = qreason_default_config();
    qconfig.max_grover_iterations = bridge->config.max_grover_iterations;
    qconfig.min_confidence = bridge->config.min_confidence_threshold;
    qconfig.enable_interference = bridge->config.enable_interference;
    qconfig.seed = bridge->config.seed;

    bridge->quantum_reasoner = qreason_create(&qconfig);
    if (!bridge->quantum_reasoner) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "insula_quantum_bridge_create: bridge->quantum_reasoner is NULL");
        return NULL;
    }

    /* Allocate interoceptive states */
    bridge->num_intero_channels = bridge->config.intero_channels;
    bridge->intero_states = nimcp_calloc(
        bridge->num_intero_channels, sizeof(quantum_intero_state_t));
    if (!bridge->intero_states) {
        insula_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "insula_quantum_bridge_create: bridge->intero_states is NULL");
        return NULL;
    }

    /* Allocate emotion hypotheses */
    bridge->num_emotion_hypotheses = bridge->config.emotion_superposition_size;
    bridge->emotion_hypotheses = nimcp_calloc(
        bridge->num_emotion_hypotheses, sizeof(quantum_emotion_hypothesis_t));
    if (!bridge->emotion_hypotheses) {
        insula_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "insula_quantum_bridge_create: bridge->emotion_hypotheses is NULL");
        return NULL;
    }

    /* Initialize emotion hypothesis labels */
    const char* emotion_labels[] = {
        "joy", "sadness", "fear", "anger",
        "disgust", "surprise", "contempt", "neutral"
    };
    for (uint32_t i = 0; i < bridge->num_emotion_hypotheses && i < 8; i++) {
        bridge->emotion_hypotheses[i].hypothesis_id = i;
        strncpy(bridge->emotion_hypotheses[i].emotion_label,
                emotion_labels[i], sizeof(bridge->emotion_hypotheses[i].emotion_label) - 1);
    }

    /* Allocate somatic marker candidates */
    bridge->max_somatic_candidates = 32;
    bridge->somatic_candidates = nimcp_calloc(
        bridge->max_somatic_candidates, sizeof(quantum_somatic_marker_t));
    if (!bridge->somatic_candidates) {
        insula_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "insula_quantum_bridge_create: bridge->somatic_candidates is NULL");
        return NULL;
    }

    bridge->rng_state = bridge->config.seed;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void insula_quantum_bridge_destroy(insula_quantum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "insula_quantum");

    if (bridge->intero_states) nimcp_free(bridge->intero_states);
    if (bridge->emotion_hypotheses) nimcp_free(bridge->emotion_hypotheses);
    if (bridge->somatic_candidates) nimcp_free(bridge->somatic_candidates);

    if (bridge->quantum_reasoner) {
        qreason_destroy(bridge->quantum_reasoner);
    }

    nimcp_free(bridge);
}

bool insula_quantum_bridge_is_enabled(const insula_quantum_bridge_t* bridge) {
    return bridge && bridge->config.enabled;
}

void insula_quantum_bridge_set_enabled(insula_quantum_bridge_t* bridge, bool enabled) {
    if (bridge) bridge->config.enabled = enabled;
}

/*=============================================================================
 * INTEROCEPTIVE INTEGRATION API
 *===========================================================================*/

int insula_quantum_init_interoception(
    insula_quantum_bridge_t* bridge,
    const float* channels,
    uint32_t num_channels
) {
    if (!bridge || !channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_init_interoception: required parameter is NULL (bridge, channels)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_init_interoception: bridge->config is NULL");
        return -1;
    }

    uint32_t n = (num_channels < bridge->num_intero_channels) ?
                  num_channels : bridge->num_intero_channels;

    /* Initialize quantum states with equal superposition */
    float norm = 1.0f / sqrtf((float)n);

    for (uint32_t i = 0; i < n; i++) {
        bridge->intero_states[i].channel_id = i;
        bridge->intero_states[i].amplitude_real = norm;
        bridge->intero_states[i].amplitude_imag = 0.0f;
        bridge->intero_states[i].signal_value = channels[i];
        bridge->intero_states[i].confidence = 0.8f;
        bridge->intero_states[i].collapsed = false;
    }

    return 0;
}

int insula_quantum_integrate_interoception(
    insula_quantum_bridge_t* bridge,
    quantum_intero_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_integrate_interoception: required parameter is NULL (bridge, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_integrate_interoception: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Build CNF for interoceptive integration */
    qreason_cnf_t intero_cnf = {0};
    intero_cnf.n_variables = (bridge->num_intero_channels < QREASON_MAX_VARIABLES) ?
                              bridge->num_intero_channels : QREASON_MAX_VARIABLES;

    /* Single clause: at least one channel must be active */
    intero_cnf.n_clauses = 1;
    intero_cnf.clauses[0].n_literals = intero_cnf.n_variables;

    for (uint32_t i = 0; i < intero_cnf.n_variables; i++) {
        intero_cnf.clauses[0].literals[i].variable = i;
        intero_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &intero_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "insula_quantum_integrate_interoception: validation failed");
        return -1;
    }

    /* Integrate signals based on quantum amplitudes */
    float integrated_value = 0.0f;
    float total_amplitude = 0.0f;
    uint32_t channels_fused = 0;

    for (uint32_t i = 0; i < intero_cnf.n_variables; i++) {
        float amp = qresult.confidences[i];
        float signal = bridge->intero_states[i].signal_value;

        /* Weight signal by quantum amplitude */
        integrated_value += signal * amp;
        total_amplitude += amp;

        if (amp > 0.0f) {
            channels_fused++;
        }

        /* Update intero state */
        bridge->intero_states[i].amplitude_real = amp;
        bridge->intero_states[i].confidence = amp * amp;
        bridge->intero_states[i].collapsed = true;
    }

    /* Normalize */
    if (total_amplitude > 0.0f) {
        integrated_value /= total_amplitude;
    }

    /* Build result */
    result->state_dim = 1;
    result->integrated_state = NULL;  /* Caller should allocate if needed */
    result->integration_quality = qresult.satisfaction_prob;
    result->coherence = total_amplitude / (float)intero_cnf.n_variables;
    result->channels_fused = channels_fused;

    /* Estimate speedup (quantum provides sqrt(N) advantage) */
    float classical_cost = (float)bridge->num_intero_channels;
    float quantum_cost = sqrtf(classical_cost);
    result->speedup = classical_cost / (quantum_cost > 0.0f ? quantum_cost : 1.0f);

    /* Update statistics */
    bridge->stats.intero_integrations++;
    bridge->stats.avg_intero_speedup =
        (bridge->stats.avg_intero_speedup * (bridge->stats.intero_integrations - 1) +
         result->speedup) / bridge->stats.intero_integrations;

    return 0;
}

int insula_quantum_correct_intero_noise(
    insula_quantum_bridge_t* bridge,
    const float* noisy_signals,
    float* corrected_signals,
    uint32_t num_signals
) {
    if (!bridge || !noisy_signals || !corrected_signals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_correct_intero_noise: required parameter is NULL (bridge, noisy_signals, corrected_signals)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_correct_intero_noise: bridge->config is NULL");
        return -1;
    }

    /* Simple noise correction using averaging and thresholding */
    /* In a full implementation, this would use quantum error correction */

    float tolerance = bridge->config.noise_tolerance;

    for (uint32_t i = 0; i < num_signals; i++) {
        float signal = noisy_signals[i];

        /* Clamp extreme values */
        signal = nimcp_clampf(signal, 0.0f, 1.0f);

        /* Apply soft thresholding for noise reduction */
        if (fabsf(signal - 0.5f) < tolerance) {
            /* Near neutral - reduce noise */
            signal = 0.5f + (signal - 0.5f) * 0.5f;
        }

        corrected_signals[i] = signal;
    }

    return 0;
}

/*=============================================================================
 * EMOTIONAL EVALUATION API
 *===========================================================================*/

int insula_quantum_evaluate_emotion(
    insula_quantum_bridge_t* bridge,
    const float* body_state,
    uint32_t body_state_dim,
    quantum_emotion_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_evaluate_emotion: required parameter is NULL (bridge, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_evaluate_emotion: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Build CNF for emotion evaluation */
    qreason_cnf_t emotion_cnf = {0};
    emotion_cnf.n_variables = (bridge->num_emotion_hypotheses < QREASON_MAX_VARIABLES) ?
                               bridge->num_emotion_hypotheses : QREASON_MAX_VARIABLES;

    /* Single clause: at least one emotion must be active */
    emotion_cnf.n_clauses = 1;
    emotion_cnf.clauses[0].n_literals = emotion_cnf.n_variables;

    for (uint32_t i = 0; i < emotion_cnf.n_variables; i++) {
        emotion_cnf.clauses[0].literals[i].variable = i;
        emotion_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &emotion_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "insula_quantum_evaluate_emotion: validation failed");
        return -1;
    }

    /* Map body state to emotional dimensions */
    float avg_body = 0.0f;
    if (body_state && body_state_dim > 0) {
        for (uint32_t i = 0; i < body_state_dim; i++) {
            avg_body += body_state[i];
        }
        avg_body /= (float)body_state_dim;
    }

    /* Update emotion hypotheses based on quantum result and body state */
    quantum_emotion_hypothesis_t* best = NULL;
    float best_prob = -1.0f;

    /* Predefined valence/arousal for each emotion */
    float emotion_valence[] = {0.8f, -0.8f, -0.6f, -0.4f, -0.7f, 0.2f, -0.5f, 0.0f};
    float emotion_arousal[] = {0.6f, -0.4f, 0.7f, 0.8f, 0.3f, 0.5f, 0.2f, 0.0f};

    for (uint32_t i = 0; i < emotion_cnf.n_variables && i < bridge->num_emotion_hypotheses; i++) {
        quantum_emotion_hypothesis_t* hyp = &bridge->emotion_hypotheses[i];

        hyp->amplitude = qresult.confidences[i];
        hyp->probability = hyp->amplitude * hyp->amplitude;

        /* Set valence/arousal from predefined values */
        hyp->valence = emotion_valence[i];
        hyp->arousal = emotion_arousal[i];

        /* Modulate by body state */
        if (body_state_dim > 0) {
            hyp->probability *= (0.5f + avg_body * 0.5f);
        }

        if (hyp->probability > best_prob) {
            best_prob = hyp->probability;
            best = hyp;
        }
    }

    /* Fill result */
    result->best_emotion = best;
    result->hypotheses_evaluated = emotion_cnf.n_variables;
    result->satisfaction_probability = qresult.satisfaction_prob;
    result->emotional_clarity = best_prob;
    result->grover_iterations_used = qresult.grover_iterations;

    /* Update statistics */
    bridge->stats.emotion_evaluations++;
    bridge->stats.avg_emotion_clarity =
        (bridge->stats.avg_emotion_clarity * (bridge->stats.emotion_evaluations - 1) +
         result->emotional_clarity) / bridge->stats.emotion_evaluations;

    if (result->emotional_clarity >= bridge->config.min_confidence_threshold) {
        bridge->stats.successful_collapses++;
    }

    return 0;
}

int insula_quantum_apply_emotional_interference(
    insula_quantum_bridge_t* bridge,
    quantum_emotion_hypothesis_t* hypotheses,
    uint32_t num_hypotheses,
    const float* consistency_matrix
) {
    if (!bridge || !hypotheses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_apply_emotional_interference: required parameter is NULL (bridge, hypotheses)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_apply_emotional_interference: bridge->config is NULL");
        return -1;
    }
    if (!bridge->config.enable_interference) return 0;

    /* Apply interference based on consistency */
    for (uint32_t i = 0; i < num_hypotheses; i++) {
        float interference = 0.0f;

        if (consistency_matrix) {
            for (uint32_t j = 0; j < num_hypotheses; j++) {
                if (i != j) {
                    float consistency = consistency_matrix[i * num_hypotheses + j];
                    float other_amp = hypotheses[j].amplitude;

                    /* Constructive interference for consistent, destructive for inconsistent */
                    interference += other_amp * (consistency - 0.5f) * 2.0f;
                }
            }
        }

        /* Apply interference to amplitude */
        hypotheses[i].amplitude += interference * 0.1f;
        hypotheses[i].amplitude = nimcp_clampf(hypotheses[i].amplitude, 0.0f, 1.0f);

        /* Recalculate probability */
        hypotheses[i].probability = hypotheses[i].amplitude * hypotheses[i].amplitude;
    }

    return 0;
}

/*=============================================================================
 * SOMATIC MARKER SEARCH API
 *===========================================================================*/

int insula_quantum_search_somatic_marker(
    insula_quantum_bridge_t* bridge,
    const float* context_features,
    uint32_t context_dim,
    uint32_t marker_database,
    quantum_somatic_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_search_somatic_marker: required parameter is NULL (bridge, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_search_somatic_marker: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Build CNF for somatic marker search */
    qreason_cnf_t search_cnf = {0};
    search_cnf.n_variables = (marker_database < QREASON_MAX_VARIABLES) ?
                              marker_database : QREASON_MAX_VARIABLES;

    /* Single clause: at least one marker must match */
    search_cnf.n_clauses = 1;
    search_cnf.clauses[0].n_literals = (search_cnf.n_variables < QREASON_MAX_LITERALS) ?
                                       search_cnf.n_variables : QREASON_MAX_LITERALS;

    for (uint32_t i = 0; i < search_cnf.clauses[0].n_literals; i++) {
        search_cnf.clauses[0].literals[i].variable = i;
        search_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &search_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "insula_quantum_search_somatic_marker: validation failed");
        return -1;
    }

    /* Generate somatic marker candidates */
    uint32_t num_candidates = (bridge->max_somatic_candidates < search_cnf.n_variables) ?
                               bridge->max_somatic_candidates : search_cnf.n_variables;

    quantum_somatic_marker_t* best = NULL;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < num_candidates; i++) {
        quantum_somatic_marker_t* marker = &bridge->somatic_candidates[i];

        marker->context_id = i;
        marker->amplitude = qresult.confidences[i];

        /* Generate marker properties based on context features */
        float context_sum = 0.0f;
        if (context_features && context_dim > 0) {
            for (uint32_t j = 0; j < context_dim; j++) {
                context_sum += context_features[j];
            }
            context_sum /= (float)context_dim;
        } else {
            context_sum = quantum_randf(&bridge->rng_state);
        }

        /* Map context to valence */
        marker->valence = (context_sum - 0.5f) * 2.0f;  /* [-1, 1] */
        marker->confidence = marker->amplitude;
        marker->retrieval_time = 1.0f / (marker->amplitude + 0.1f);

        float score = marker->amplitude * marker->confidence;
        if (score > best_score) {
            best_score = score;
            best = marker;
        }
    }

    /* Fill result */
    result->best_marker = best;
    result->markers_evaluated = num_candidates;
    result->satisfaction_probability = qresult.satisfaction_prob;

    /* Estimate speedup */
    float classical_cost = (float)marker_database;
    float quantum_cost = sqrtf(classical_cost);
    result->search_speedup = classical_cost / (quantum_cost > 0.0f ? quantum_cost : 1.0f);

    /* Update statistics */
    bridge->stats.somatic_searches++;
    bridge->stats.avg_somatic_speedup =
        (bridge->stats.avg_somatic_speedup * (bridge->stats.somatic_searches - 1) +
         result->search_speedup) / bridge->stats.somatic_searches;

    return 0;
}

/*=============================================================================
 * SOCIAL EMOTION API
 *===========================================================================*/

int insula_quantum_evaluate_social(
    insula_quantum_bridge_t* bridge,
    const float* social_cues,
    uint32_t num_cues,
    float* trust_estimate,
    float* fairness_estimate
) {
    if (!bridge || !trust_estimate || !fairness_estimate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_evaluate_social: required parameter is NULL (bridge, trust_estimate, fairness_estimate)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_evaluate_social: bridge->config is NULL");
        return -1;
    }

    /* Aggregate social cues */
    float avg_cue = 0.5f;
    if (social_cues && num_cues > 0) {
        for (uint32_t i = 0; i < num_cues; i++) {
            avg_cue += social_cues[i];
        }
        avg_cue /= (float)num_cues;
    }

    /* Use quantum randomness for uncertainty */
    float noise = (quantum_randf(&bridge->rng_state) - 0.5f) * 0.1f;

    /* Estimate trust based on cues */
    *trust_estimate = nimcp_clampf(avg_cue + noise, 0.0f, 1.0f);

    /* Estimate fairness */
    *fairness_estimate = nimcp_clampf(avg_cue + noise * 0.5f, 0.0f, 1.0f);

    return 0;
}

/*=============================================================================
 * HOMEOSTATIC OPTIMIZATION API
 *===========================================================================*/

int insula_quantum_optimize_homeostasis(
    insula_quantum_bridge_t* bridge,
    const float* current_state,
    const float* target_state,
    uint32_t state_dim,
    float* optimal_action,
    uint32_t action_dim
) {
    if (!bridge || !current_state || !target_state || !optimal_action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_optimize_homeostasis: required parameter is NULL (bridge, current_state, target_state, optimal_action)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_optimize_homeostasis: bridge->config is NULL");
        return -1;
    }

    /* Calculate error between current and target */
    float total_error = 0.0f;
    for (uint32_t i = 0; i < state_dim; i++) {
        float error = target_state[i] - current_state[i];
        total_error += error * error;
    }
    total_error = sqrtf(total_error);

    /* Generate optimal action to minimize error */
    for (uint32_t i = 0; i < action_dim && i < state_dim; i++) {
        /* Simple proportional control */
        float error = target_state[i] - current_state[i];
        optimal_action[i] = nimcp_clampf(error * 0.5f, -1.0f, 1.0f);
    }

    /* Fill remaining action dimensions with zeros */
    for (uint32_t i = state_dim; i < action_dim; i++) {
        optimal_action[i] = 0.0f;
    }

    return 0;
}

/*=============================================================================
 * STATISTICS API
 *===========================================================================*/

int insula_quantum_get_stats(
    const insula_quantum_bridge_t* bridge,
    insula_quantum_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void insula_quantum_reset_stats(insula_quantum_bridge_t* bridge) {
    if (bridge) {
        memset(&bridge->stats, 0, sizeof(bridge->stats));
    }
}

int insula_quantum_get_config(
    const insula_quantum_bridge_t* bridge,
    insula_quantum_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "insula_quantum_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    *config = bridge->config;
    return 0;
}
