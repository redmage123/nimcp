/**
 * @file nimcp_sleep_gpu.h
 * @brief GPU-accelerated Sleep and Memory Consolidation Kernels
 *
 * WHAT: CUDA kernels for sleep-dependent memory consolidation
 * WHY:  GPU acceleration for offline learning and memory transfer
 * HOW:  Custom kernels for replay, consolidation, synaptic homeostasis
 *
 * ARCHITECTURE:
 * - NREM Sleep: Slow-wave replay, systems consolidation
 * - REM Sleep: Memory integration, emotional processing
 * - Synaptic Homeostasis: Synaptic downscaling during sleep
 * - Memory Replay: Hippocampal-cortical memory transfer
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_SLEEP_GPU_H
#define NIMCP_SLEEP_GPU_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Sleep Stage Types
//=============================================================================

/**
 * @brief Sleep stage enumeration
 */
typedef enum {
    NIMCP_SLEEP_WAKE = 0,
    NIMCP_SLEEP_N1 = 1,       /**< Light sleep */
    NIMCP_SLEEP_N2 = 2,       /**< Spindle sleep */
    NIMCP_SLEEP_N3 = 3,       /**< Slow-wave sleep (SWS) */
    NIMCP_SLEEP_REM = 4,      /**< REM/dreaming */
    NIMCP_SLEEP_COUNT = 5
} nimcp_sleep_stage_t;

//=============================================================================
// Sleep Parameters
//=============================================================================

/**
 * @brief NREM consolidation parameters
 */
typedef struct {
    float slow_wave_freq;         /**< Slow oscillation frequency (Hz) */
    float spindle_freq;           /**< Sleep spindle frequency (Hz) */
    float sharp_wave_rate;        /**< Hippocampal SWR rate */
    float consolidation_rate;     /**< Memory consolidation rate */
    float replay_speed;           /**< Replay compression factor */
    float cortical_gain;          /**< Cortical plasticity gain */
    int replay_iterations;        /**< Number of replay iterations */
    float noise_level;            /**< Background noise level */
} nimcp_gpu_nrem_params_t;

/**
 * @brief REM processing parameters
 */
typedef struct {
    float theta_freq;             /**< Theta oscillation frequency (Hz) */
    float pgo_rate;               /**< PGO wave rate */
    float emotional_bias;         /**< Emotional memory bias */
    float integration_rate;       /**< Memory integration rate */
    float dream_generation_rate;  /**< Dream content generation */
    float creativity_factor;      /**< Novel association factor */
    float acetylcholine_level;    /**< ACh modulation (high in REM) */
} nimcp_gpu_rem_params_t;

/**
 * @brief Synaptic homeostasis parameters
 */
typedef struct {
    float downscaling_rate;       /**< Synaptic downscaling rate */
    float threshold;              /**< Potentiation threshold */
    float preservation_factor;    /**< Strong synapse preservation */
    float global_factor;          /**< Global downscaling factor */
    bool selective_pruning;       /**< Enable selective pruning */
    float min_weight;             /**< Minimum weight after scaling */
} nimcp_gpu_homeostasis_params_t;

/**
 * @brief Memory replay parameters
 */
typedef struct {
    float compression_ratio;      /**< Temporal compression ratio */
    float sequence_fidelity;      /**< Replay sequence fidelity */
    float reverse_replay_prob;    /**< Probability of reverse replay */
    float priority_weight;        /**< Priority experience replay */
    int buffer_size;              /**< Replay buffer size */
    float decay_rate;             /**< Buffer decay rate */
} nimcp_gpu_replay_params_t;

//=============================================================================
// Sleep State Structures
//=============================================================================

/**
 * @brief Sleep stage state
 */
typedef struct {
    nimcp_sleep_stage_t current_stage;  /**< Current sleep stage */
    float stage_duration;               /**< Time in current stage */
    float total_sleep_time;             /**< Total sleep time */
    float sleep_pressure;               /**< Homeostatic sleep pressure */
    nimcp_gpu_tensor_t* stage_probabilities; /**< Transition probabilities */
} nimcp_gpu_sleep_stage_state_t;

/**
 * @brief Memory consolidation state
 */
typedef struct {
    nimcp_gpu_tensor_t* hippocampal_buffer;  /**< Hippocampal memory buffer */
    nimcp_gpu_tensor_t* cortical_weights;    /**< Cortical synaptic weights */
    nimcp_gpu_tensor_t* replay_buffer;       /**< Replay experience buffer */
    nimcp_gpu_tensor_t* consolidation_mask;  /**< Which memories to consolidate */
    nimcp_gpu_tensor_t* priority_scores;     /**< Memory priority scores */
    nimcp_gpu_tensor_t* slow_oscillation;    /**< Current slow oscillation phase */
    size_t buffer_size;                      /**< Buffer size */
    size_t memory_dim;                       /**< Memory dimension */
} nimcp_gpu_consolidation_state_t;

/**
 * @brief Synaptic state for homeostasis
 */
typedef struct {
    nimcp_gpu_tensor_t* synaptic_weights;    /**< All synaptic weights */
    nimcp_gpu_tensor_t* weight_history;      /**< Recent weight changes */
    nimcp_gpu_tensor_t* potentiation_tags;   /**< Synaptic tagging */
    nimcp_gpu_tensor_t* activity_integral;   /**< Integrated activity */
    size_t n_synapses;                       /**< Number of synapses */
} nimcp_gpu_synaptic_state_t;

//=============================================================================
// Default Parameter Functions
//=============================================================================

NIMCP_EXPORT nimcp_gpu_nrem_params_t nimcp_gpu_nrem_params_default(void);
NIMCP_EXPORT nimcp_gpu_rem_params_t nimcp_gpu_rem_params_default(void);
NIMCP_EXPORT nimcp_gpu_homeostasis_params_t nimcp_gpu_homeostasis_params_default(void);
NIMCP_EXPORT nimcp_gpu_replay_params_t nimcp_gpu_replay_params_default(void);

//=============================================================================
// Sleep Stage Functions
//=============================================================================

/**
 * @brief Update sleep stage based on homeostatic pressure and circadian
 */
NIMCP_EXPORT bool nimcp_gpu_sleep_stage_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_sleep_stage_state_t* state,
    float circadian_phase,
    float arousal_signal,
    float dt);

/**
 * @brief Compute sleep transition probabilities
 */
NIMCP_EXPORT bool nimcp_gpu_sleep_transitions(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_sleep_stage_state_t* state,
    nimcp_gpu_tensor_t* transition_probs);

//=============================================================================
// NREM Consolidation Functions
//=============================================================================

/**
 * @brief Generate slow oscillation pattern
 */
NIMCP_EXPORT bool nimcp_gpu_nrem_slow_oscillation(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    float time,
    const nimcp_gpu_nrem_params_t* params);

/**
 * @brief Perform memory replay during NREM
 */
NIMCP_EXPORT bool nimcp_gpu_nrem_replay(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    const nimcp_gpu_nrem_params_t* params);

/**
 * @brief Transfer memories from hippocampus to cortex
 */
NIMCP_EXPORT bool nimcp_gpu_nrem_systems_consolidation(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    float dt,
    const nimcp_gpu_nrem_params_t* params);

/**
 * @brief Simulate sharp-wave ripple events
 */
NIMCP_EXPORT bool nimcp_gpu_nrem_sharp_wave_ripple(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    nimcp_gpu_tensor_t* ripple_content,
    const nimcp_gpu_nrem_params_t* params);

//=============================================================================
// REM Processing Functions
//=============================================================================

/**
 * @brief Process memories during REM
 */
NIMCP_EXPORT bool nimcp_gpu_rem_processing(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    float dt,
    const nimcp_gpu_rem_params_t* params);

/**
 * @brief Integrate memories with existing knowledge
 */
NIMCP_EXPORT bool nimcp_gpu_rem_memory_integration(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    const nimcp_gpu_tensor_t* semantic_memory,
    const nimcp_gpu_rem_params_t* params);

/**
 * @brief Process emotional memories in REM
 */
NIMCP_EXPORT bool nimcp_gpu_rem_emotional_processing(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    const nimcp_gpu_tensor_t* emotional_tags,
    const nimcp_gpu_rem_params_t* params);

/**
 * @brief Generate dream content (creative recombination)
 */
NIMCP_EXPORT bool nimcp_gpu_rem_dream_generation(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_consolidation_state_t* state,
    nimcp_gpu_tensor_t* dream_content,
    const nimcp_gpu_rem_params_t* params);

//=============================================================================
// Synaptic Homeostasis Functions
//=============================================================================

/**
 * @brief Apply global synaptic downscaling
 */
NIMCP_EXPORT bool nimcp_gpu_synaptic_downscaling(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synaptic_state_t* state,
    const nimcp_gpu_homeostasis_params_t* params);

/**
 * @brief Selective synapse preservation
 */
NIMCP_EXPORT bool nimcp_gpu_synapse_preservation(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synaptic_state_t* state,
    const nimcp_gpu_tensor_t* importance_scores,
    const nimcp_gpu_homeostasis_params_t* params);

/**
 * @brief Prune weak synapses
 */
NIMCP_EXPORT bool nimcp_gpu_synapse_pruning(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synaptic_state_t* state,
    const nimcp_gpu_homeostasis_params_t* params);

/**
 * @brief Update synaptic tags for consolidation
 */
NIMCP_EXPORT bool nimcp_gpu_synaptic_tagging(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synaptic_state_t* state,
    const nimcp_gpu_tensor_t* recent_activity,
    float dt);

//=============================================================================
// Memory Replay Functions
//=============================================================================

/**
 * @brief Sample experiences from replay buffer
 */
NIMCP_EXPORT bool nimcp_gpu_replay_sample(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_consolidation_state_t* state,
    nimcp_gpu_tensor_t* sampled_memories,
    int n_samples,
    const nimcp_gpu_replay_params_t* params);

/**
 * @brief Add new experience to replay buffer
 */
NIMCP_EXPORT bool nimcp_gpu_replay_store(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    const nimcp_gpu_tensor_t* experience,
    float priority,
    const nimcp_gpu_replay_params_t* params);

/**
 * @brief Update priorities after learning
 */
NIMCP_EXPORT bool nimcp_gpu_replay_update_priorities(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    const nimcp_gpu_tensor_t* td_errors,
    const nimcp_gpu_replay_params_t* params);

/**
 * @brief Compress replay sequence temporally
 */
NIMCP_EXPORT bool nimcp_gpu_replay_compress(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* sequence,
    nimcp_gpu_tensor_t* compressed,
    const nimcp_gpu_replay_params_t* params);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SLEEP_GPU_H
