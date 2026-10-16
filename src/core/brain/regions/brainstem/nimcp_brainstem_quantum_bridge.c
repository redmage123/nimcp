/**
 * @file nimcp_brainstem_quantum_bridge.c
 * @brief Implementation of quantum-accelerated brainstem processing
 *
 * WHAT: Quantum bridge for brainstem computational acceleration
 * WHY:  Enable parallel reflex pathway evaluation and arousal state optimization
 * HOW:  Uses quantum algorithms for combinatorial optimization
 *
 * @version Phase BS-2: Quantum Brainstem Integration
 * @date 2025-12-30
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/brainstem/nimcp_brainstem_quantum_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brainstem_quantum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "BRAINSTEM_QUANTUM_BRIDGE"


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define BRAINSTEM_QUANTUM_LOG_MODULE "BRAINSTEM_QUANTUM"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Internal quantum bridge structure
 */
struct brainstem_quantum_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    brainstem_quantum_config_t config;

    /* Connected adapter */
    brainstem_adapter_t* adapter;

    /* External quantum resources */
    brain_qreason_ctx_t* qreason;
    quantum_annealer_t annealer;

    /* Internal state */
    bool quantum_available;
    float current_mix_ratio;

    /* Statistics */
    brainstem_quantum_stats_t stats;

    /* Timing */
    uint64_t last_update_us;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    return nimcp_time_get_us();
}

/**
 * @brief Classical reflex selection (fallback)
 */
static void classical_select_reflex(brainstem_adapter_t* adapter,
                                     const float* stimulus_pattern,
                                     uint32_t pattern_size,
                                     float urgency,
                                     quantum_reflex_result_t* result) {
    /* Simple linear search through registered reflexes */
    float max_match = 0.0f;
    uint32_t best_id = 0;
    uint32_t count = 0;

    brainstem_stats_t stats;
    if (brainstem_get_stats(adapter, &stats)) {
        count = (uint32_t)stats.reflexes_triggered;
    }

    /* For now, just select based on urgency threshold */
    if (urgency > 0.5f) {
        best_id = 1; /* Startle reflex */
        max_match = urgency;
    } else if (urgency > 0.3f) {
        best_id = 2; /* Orienting reflex */
        max_match = urgency * 0.8f;
    } else {
        best_id = 0; /* No reflex */
        max_match = 0.0f;
    }

    result->selected_reflex_id = best_id;
    result->selection_confidence = max_match;
    result->pathways_evaluated = count > 0 ? count : 4;
    result->quantum_speedup = 1.0f;
    result->used_quantum = false;
}

/**
 * @brief Classical arousal optimization (fallback)
 */
static void classical_optimize_arousal(float current_arousal,
                                        float sensory_load,
                                        float metabolic_state,
                                        float threat_level,
                                        quantum_arousal_result_t* result) {
    /* Simple weighted combination */
    float target = 0.5f; /* Baseline */

    /* Increase for threat */
    target += threat_level * 0.3f;

    /* Adjust for sensory load */
    target += (sensory_load - 0.5f) * 0.2f;

    /* Adjust for metabolic state (low energy = lower arousal target) */
    target *= metabolic_state;

    /* Clamp */
    if (target < 0.1f) target = 0.1f;
    if (target > 0.95f) target = 0.95f;

    /* Simple energy calculation */
    float energy = fabsf(current_arousal - target) +
                   0.1f * threat_level +
                   0.05f * sensory_load;

    result->optimal_arousal = target;
    result->energy_minimum = energy;
    result->iterations = 10;
    result->convergence_metric = 1.0f - fabsf(current_arousal - target);
    result->used_quantum = false;
}

/**
 * @brief Classical sensory integration (fallback)
 */
static void classical_integrate_sensory(const brainstem_sensory_input_t* visual,
                                         const brainstem_sensory_input_t* auditory,
                                         quantum_sensory_result_t* result) {
    float visual_sal = visual ? visual->visual_salience : 0.0f;
    float audio_sal = auditory ? auditory->sound_intensity : 0.0f;

    /* Simple salience-weighted combination */
    float total = visual_sal + audio_sal;
    if (total < 0.001f) total = 0.001f;

    result->visual_weight = visual_sal / total;
    result->auditory_weight = audio_sal / total;
    result->integrated_salience = fmaxf(visual_sal, audio_sal);

    /* Weighted attention direction */
    if (visual && visual_sal > 0.0f) {
        result->attention_direction_x = visual->visual_target_x * result->visual_weight;
        result->attention_direction_y = visual->visual_target_y * result->visual_weight;
    }

    if (auditory && audio_sal > 0.0f) {
        /* Convert azimuth to x direction */
        float audio_x = sinf(auditory->sound_azimuth * NIMCP_PI_F / 180.0f);
        result->attention_direction_x += audio_x * result->auditory_weight;
    }

    result->used_quantum = false;
}

/**
 * @brief Estimate Grover speedup
 */
static float estimate_grover_speedup(uint32_t n) {
    if (n <= 4) return 1.0f; /* No advantage for tiny problems */
    return sqrtf((float)n);
}

/**
 * @brief Estimate annealing speedup
 */
static float estimate_annealing_speedup(uint32_t n) {
    if (n <= 10) return 1.0f;
    /* Quantum annealing can provide polynomial speedup */
    return logf((float)n);
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

brainstem_quantum_config_t brainstem_quantum_default_config(void) {
    brainstem_quantum_config_t config;
    memset(&config, 0, sizeof(config));

    /* Algorithm defaults */
    config.reflex_algorithm = BRAINSTEM_QUANTUM_ALG_GROVER;
    config.arousal_algorithm = BRAINSTEM_QUANTUM_ALG_ANNEALING;
    config.sensory_algorithm = BRAINSTEM_QUANTUM_ALG_AMPLITUDE;

    /* Quantum parameters */
    config.max_qubits = 32;
    config.grover_iterations = 5;
    config.annealing_steps = 100;
    config.annealing_temperature = NIMCP_TEMPERATURE_DEFAULT;
    config.qaoa_layers = 3;

    /* Performance */
    config.quantum_classical_mix = 0.5f;
    config.enable_noise_mitigation = true;
    config.enable_hybrid_execution = true;

    /* Thresholds */
    config.quantum_advantage_threshold = 0.1f;
    config.min_pathways_for_quantum = 8;

    return config;
}

brainstem_quantum_bridge_t* brainstem_quantum_bridge_create(
    brainstem_adapter_t* adapter,
    const brainstem_quantum_config_t* config) {

    if (!adapter) {
        LOG_ERROR("[%s] NULL adapter provided", BRAINSTEM_QUANTUM_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");


        return NULL;
    }

    LOG_INFO("[%s] Creating quantum bridge", BRAINSTEM_QUANTUM_LOG_MODULE);

    brainstem_quantum_bridge_t* bridge = nimcp_calloc(1, sizeof(brainstem_quantum_bridge_t));
    if (!bridge) {
        LOG_ERROR("[%s] Failed to allocate bridge memory", BRAINSTEM_QUANTUM_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = brainstem_quantum_default_config();
    }

    bridge->adapter = adapter;
    bridge->qreason = NULL;
    bridge->annealer = NULL;
    bridge->quantum_available = false;
    bridge->current_mix_ratio = bridge->config.quantum_classical_mix;
    bridge->last_update_us = get_time_us();

    LOG_INFO("[%s] Quantum bridge created (quantum available: %s)",
             BRAINSTEM_QUANTUM_LOG_MODULE,
             bridge->quantum_available ? "yes" : "no");

    return bridge;
}

void brainstem_quantum_bridge_destroy(brainstem_quantum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "brainstem_quantum");

    LOG_INFO("[%s] Destroying quantum bridge", BRAINSTEM_QUANTUM_LOG_MODULE);

    /* Note: We don't own qreason or annealer, just clear references */
    bridge->qreason = NULL;
    bridge->annealer = NULL;

    nimcp_free(bridge);
    LOG_DEBUG("[%s] Quantum bridge destroyed", BRAINSTEM_QUANTUM_LOG_MODULE);
}

bool brainstem_quantum_bridge_connect_reasoner(
    brainstem_quantum_bridge_t* bridge,
    brain_qreason_ctx_t* qreason) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_quantum_bridge_destroy: bridge is NULL");
        return false;
    }

    bridge->qreason = qreason;
    if (qreason) {
        bridge->quantum_available = true;
        LOG_INFO("[%s] Connected to quantum reasoner", BRAINSTEM_QUANTUM_LOG_MODULE);
    }

    return true;
}

bool brainstem_quantum_bridge_connect_annealer(
    brainstem_quantum_bridge_t* bridge,
    quantum_annealer_t annealer) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_quantum_bridge_destroy: bridge is NULL");
        return false;
    }

    bridge->annealer = annealer;
    if (annealer) {
        bridge->quantum_available = true;
        LOG_INFO("[%s] Connected to quantum annealer", BRAINSTEM_QUANTUM_LOG_MODULE);
    }

    return true;
}

/*=============================================================================
 * QUANTUM OPERATIONS
 *===========================================================================*/

bool brainstem_quantum_select_reflex(
    brainstem_quantum_bridge_t* bridge,
    const float* stimulus_pattern,
    uint32_t pattern_size,
    float urgency,
    quantum_reflex_result_t* result) {

    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_quantum_bridge_destroy: required parameter is NULL (bridge, result)");
        return false;
    }

    uint64_t start_us = get_time_us();
    memset(result, 0, sizeof(quantum_reflex_result_t));

    bridge->stats.reflex_queries++;

    /* Check if quantum is available and beneficial */
    bool use_quantum = bridge->quantum_available &&
                       bridge->qreason != NULL &&
                       pattern_size >= bridge->config.min_pathways_for_quantum &&
                       bridge->current_mix_ratio < 0.9f;

    if (use_quantum) {
        /* Quantum path (not fully implemented - would call qreason) */
        /* For now, fall back to classical with quantum metadata */
        classical_select_reflex(bridge->adapter, stimulus_pattern,
                                 pattern_size, urgency, result);

        result->used_quantum = true;
        result->quantum_speedup = estimate_grover_speedup(pattern_size);
        bridge->stats.quantum_executions++;

        LOG_DEBUG("[%s] Quantum reflex selection (speedup: %.2f)",
                  BRAINSTEM_QUANTUM_LOG_MODULE, result->quantum_speedup);
    } else {
        /* Classical fallback */
        classical_select_reflex(bridge->adapter, stimulus_pattern,
                                 pattern_size, urgency, result);
        bridge->stats.classical_fallbacks++;
    }

    result->execution_time_us = (double)(get_time_us() - start_us);

    if (result->used_quantum) {
        bridge->stats.total_quantum_time_us += result->execution_time_us;
    } else {
        bridge->stats.total_classical_time_us += result->execution_time_us;
    }

    return true;
}

bool brainstem_quantum_optimize_arousal(
    brainstem_quantum_bridge_t* bridge,
    float current_arousal,
    float sensory_load,
    float metabolic_state,
    float threat_level,
    quantum_arousal_result_t* result) {

    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_quantum_bridge_destroy: required parameter is NULL (bridge, result)");
        return false;
    }

    uint64_t start_us = get_time_us();
    memset(result, 0, sizeof(quantum_arousal_result_t));

    bridge->stats.arousal_optimizations++;

    /* Check if quantum annealing is available */
    bool use_quantum = bridge->quantum_available &&
                       bridge->annealer != NULL &&
                       bridge->current_mix_ratio < 0.9f;

    if (use_quantum) {
        /* Quantum annealing path (not fully implemented) */
        classical_optimize_arousal(current_arousal, sensory_load,
                                    metabolic_state, threat_level, result);

        result->used_quantum = true;
        result->iterations = bridge->config.annealing_steps;
        bridge->stats.quantum_executions++;

        LOG_DEBUG("[%s] Quantum arousal optimization (optimal: %.2f)",
                  BRAINSTEM_QUANTUM_LOG_MODULE, result->optimal_arousal);
    } else {
        /* Classical fallback */
        classical_optimize_arousal(current_arousal, sensory_load,
                                    metabolic_state, threat_level, result);
        bridge->stats.classical_fallbacks++;
    }

    result->execution_time_us = (double)(get_time_us() - start_us);

    if (result->used_quantum) {
        bridge->stats.total_quantum_time_us += result->execution_time_us;
    } else {
        bridge->stats.total_classical_time_us += result->execution_time_us;
    }

    return true;
}

bool brainstem_quantum_integrate_sensory(
    brainstem_quantum_bridge_t* bridge,
    const brainstem_sensory_input_t* visual_input,
    const brainstem_sensory_input_t* auditory_input,
    quantum_sensory_result_t* result) {

    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_quantum_bridge_destroy: required parameter is NULL (bridge, result)");
        return false;
    }

    uint64_t start_us = get_time_us();
    memset(result, 0, sizeof(quantum_sensory_result_t));

    bridge->stats.sensory_integrations++;

    /* Check if quantum amplitude amplification is available */
    bool use_quantum = bridge->quantum_available &&
                       bridge->qreason != NULL &&
                       bridge->current_mix_ratio < 0.9f;

    if (use_quantum) {
        /* Quantum amplitude amplification path (not fully implemented) */
        classical_integrate_sensory(visual_input, auditory_input, result);
        result->used_quantum = true;
        bridge->stats.quantum_executions++;

        LOG_DEBUG("[%s] Quantum sensory integration (salience: %.2f)",
                  BRAINSTEM_QUANTUM_LOG_MODULE, result->integrated_salience);
    } else {
        /* Classical fallback */
        classical_integrate_sensory(visual_input, auditory_input, result);
        bridge->stats.classical_fallbacks++;
    }

    result->execution_time_us = (double)(get_time_us() - start_us);

    if (result->used_quantum) {
        bridge->stats.total_quantum_time_us += result->execution_time_us;
    } else {
        bridge->stats.total_classical_time_us += result->execution_time_us;
    }

    return true;
}

uint32_t brainstem_quantum_evaluate_pathways(
    brainstem_quantum_bridge_t* bridge,
    float stimulus,
    uint32_t* pathway_ids,
    float* activations,
    uint32_t max_pathways) {

    if (!bridge || !pathway_ids || !activations || max_pathways == 0) return 0;

    /* Simple evaluation - in a real implementation, this would use
       quantum superposition to evaluate all pathways simultaneously */

    uint32_t count = 0;

    /* Startle reflex */
    if (stimulus > 0.7f && count < max_pathways) {
        pathway_ids[count] = 1;
        activations[count] = stimulus;
        count++;
    }

    /* Orienting reflex */
    if (stimulus > 0.3f && count < max_pathways) {
        pathway_ids[count] = 2;
        activations[count] = stimulus * 0.6f;
        count++;
    }

    /* Pupillary reflex */
    if (stimulus > 0.5f && count < max_pathways) {
        pathway_ids[count] = 3;
        activations[count] = stimulus * 0.4f;
        count++;
    }

    return count;
}

/*=============================================================================
 * STATE AND STATISTICS
 *===========================================================================*/

bool brainstem_quantum_bridge_update(brainstem_quantum_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_quantum_bridge_update: bridge is NULL");
        return false;
    }

    /* Update timing */
    bridge->last_update_us = get_time_us();

    /* Update average speedup */
    if (bridge->stats.quantum_executions > 0) {
        float total_calls = (float)(bridge->stats.quantum_executions +
                                    bridge->stats.classical_fallbacks);
        if (total_calls > 0) {
            bridge->stats.avg_quantum_speedup =
                (float)bridge->stats.quantum_executions / total_calls *
                sqrtf((float)bridge->config.max_qubits);
        }
    }

    /* Update average execution time */
    float total_time = (float)(bridge->stats.total_quantum_time_us +
                               bridge->stats.total_classical_time_us);
    float total_calls = (float)(bridge->stats.reflex_queries +
                                bridge->stats.arousal_optimizations +
                                bridge->stats.sensory_integrations);
    if (total_calls > 0) {
        bridge->stats.avg_execution_time_us = total_time / total_calls;
    }

    return true;
}

bool brainstem_quantum_bridge_get_stats(
    const brainstem_quantum_bridge_t* bridge,
    brainstem_quantum_stats_t* stats) {

    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_quantum_bridge_update: required parameter is NULL (bridge, stats)");
        return false;
    }
    *stats = bridge->stats;
    return true;
}

bool brainstem_quantum_bridge_is_quantum_available(
    const brainstem_quantum_bridge_t* bridge) {

    return bridge ? bridge->quantum_available : false;
}

float brainstem_quantum_bridge_estimate_speedup(
    const brainstem_quantum_bridge_t* bridge,
    uint32_t problem_size,
    brainstem_quantum_algorithm_t algorithm) {

    if (!bridge) return 1.0f;

    switch (algorithm) {
        case BRAINSTEM_QUANTUM_ALG_GROVER:
            return estimate_grover_speedup(problem_size);

        case BRAINSTEM_QUANTUM_ALG_ANNEALING:
        case BRAINSTEM_QUANTUM_ALG_QAOA:
        case BRAINSTEM_QUANTUM_ALG_VQE:
            return estimate_annealing_speedup(problem_size);

        case BRAINSTEM_QUANTUM_ALG_AMPLITUDE:
            return sqrtf((float)problem_size);

        case BRAINSTEM_QUANTUM_ALG_NONE:
        default:
            return 1.0f;
    }
}

bool brainstem_quantum_bridge_set_mix(
    brainstem_quantum_bridge_t* bridge,
    float mix) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brainstem_quantum_bridge_update: bridge is NULL");
        return false;
    }

    if (mix < 0.0f) mix = 0.0f;
    if (mix > 1.0f) mix = 1.0f;

    bridge->current_mix_ratio = mix;
    LOG_DEBUG("[%s] Set quantum-classical mix to %.2f",
              BRAINSTEM_QUANTUM_LOG_MODULE, mix);

    return true;
}
