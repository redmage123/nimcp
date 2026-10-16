//=============================================================================
// nimcp_neuromod_pink_noise.h - Pink Noise Modulated Neuromodulation
//=============================================================================
/**
 * @file nimcp_neuromod_pink_noise.h
 * @brief Integration of pink noise with neuromodulator system for exploration
 *
 * WHAT: Pink noise-modulated dopamine/serotonin for contextual learning
 * WHY:
 *   - Pink noise provides multi-timescale exploration
 *   - Neuromodulators gate learning at synapses
 *   - Together: biologically realistic exploration-exploitation balance
 *
 * HOW:
 *   dopamine_current = baseline + reward_signal + pink_noise × amplitude
 *
 * BIOLOGICAL MOTIVATION:
 *   - Dopamine neurons exhibit 1/f noise in firing (Montague et al., 2004)
 *   - Serotonin fluctuations follow pink spectrum (Cools et al., 2008)
 *   - This enables long-range correlations in learning
 *
 * USE CASES:
 *   - NLP: Context-dependent learning (attend to relevant words)
 *   - RL: Exploration in high-dimensional spaces
 *   - Attention: Dynamic re-weighting of inputs
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 * @version 2.7.0 Phase 3
 */

#ifndef NIMCP_NEUROMOD_PINK_NOISE_H
#define NIMCP_NEUROMOD_PINK_NOISE_H

#include <stdint.h>
#include <stdbool.h>
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/noise/nimcp_pink_noise.h"  // Includes stdio.h for FILE

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Pink Noise Neuromodulator State
//=============================================================================

/**
 * @brief Pink noise-modulated neuromodulator state
 *
 * WHAT: Extends neuromodulator with pink noise generator
 * WHY: Adds multi-timescale exploration to learning
 * HOW: Combines baseline + external signal + pink noise
 */
typedef struct {
    // Baseline levels
    float dopamine_baseline;
    float serotonin_baseline;
    float acetylcholine_baseline;
    float norepinephrine_baseline;

    // Current levels (baseline + signal + noise)
    float dopamine_current;
    float serotonin_current;
    float acetylcholine_current;
    float norepinephrine_current;

    // Pink noise generators (one per neurotransmitter)
    pink_noise_generator_t dopamine_noise;
    pink_noise_generator_t serotonin_noise;
    pink_noise_generator_t acetylcholine_noise;
    pink_noise_generator_t norepinephrine_noise;

    // Noise amplitudes
    float dopamine_noise_amplitude;
    float serotonin_noise_amplitude;
    float acetylcholine_noise_amplitude;
    float norepinephrine_noise_amplitude;

    // Statistics
    uint64_t update_count;
    float avg_dopamine;
    float avg_serotonin;

} neuromod_pink_noise_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for pink noise neuromodulation
 */
typedef struct {
    // Baselines (when no external signal)
    float dopamine_baseline;      /**< Default: 0.2 */
    float serotonin_baseline;     /**< Default: 0.3 */
    float acetylcholine_baseline; /**< Default: 0.4 */
    float norepinephrine_baseline;/**< Default: 0.1 */

    // Pink noise amplitudes
    float dopamine_noise_amplitude;      /**< Default: 0.1 (10% noise) */
    float serotonin_noise_amplitude;     /**< Default: 0.05 (5% noise) */
    float acetylcholine_noise_amplitude; /**< Default: 0.15 (15% noise, fast) */
    float norepinephrine_noise_amplitude;/**< Default: 0.08 (8% noise) */

    // Pink noise parameters
    float alpha;          /**< Spectral exponent (default: 1.0 = pink) */
    float min_frequency;  /**< Min freq Hz (default: 0.1) */
    float max_frequency;  /**< Max freq Hz (default: 10.0) */
    float sample_rate;    /**< Sample rate Hz (default: 1000.0) */

} neuromod_pink_config_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create pink noise-modulated neuromodulator system
 *
 * WHAT: Initialize system with pink noise generators
 * WHY: Enable exploration in learning
 * HOW: Create one pink noise generator per neuromodulator
 *
 * @param config Configuration
 * @return System handle, or NULL on error
 */
neuromod_pink_noise_t* neuromod_pink_create(const neuromod_pink_config_t* config);

/**
 * @brief Destroy pink noise neuromodulator system
 *
 * @param mod System to destroy
 */
void neuromod_pink_destroy(neuromod_pink_noise_t* mod);

/**
 * @brief Get default configuration
 *
 * @return Default configuration with sensible values
 */
neuromod_pink_config_t neuromod_pink_default_config(void);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update neuromodulators with external signals + pink noise
 *
 * WHAT: Update dopamine/serotonin based on reward/punishment + exploration noise
 * WHY: Combines task-driven signals with exploration
 * HOW:
 *   1. Generate pink noise samples
 *   2. Combine: baseline + external_signal + noise
 *   3. Clamp to [0, 1]
 *
 * ALGORITHM:
 * ```
 * dopamine = baseline + reward * gain + pink_noise * amplitude
 * serotonin = baseline + punishment * gain + pink_noise * amplitude
 * acetylcholine = baseline + salience * gain + pink_noise * amplitude
 * norepinephrine = baseline + arousal * gain + pink_noise * amplitude
 * ```
 *
 * @param mod Neuromodulator system
 * @param reward_signal External reward signal [-1, 1]
 * @param punishment_signal External punishment signal [0, 1]
 * @param salience_signal Salience/attention signal [0, 1]
 * @param arousal_signal Arousal/stress signal [0, 1]
 */
void neuromod_pink_update(
    neuromod_pink_noise_t* mod,
    float reward_signal,
    float punishment_signal,
    float salience_signal,
    float arousal_signal
);

/**
 * @brief Update with simple reward signal only
 *
 * WHAT: Simplified update with just reward
 * WHY: Common case for RL tasks
 * HOW: Sets reward, leaves others at baseline
 *
 * @param mod Neuromodulator system
 * @param reward Reward signal [-1, 1]
 */
void neuromod_pink_update_reward(neuromod_pink_noise_t* mod, float reward);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current dopamine level
 *
 * @param mod Neuromodulator system
 * @return Current dopamine [0, 1]
 */
float neuromod_pink_get_dopamine(const neuromod_pink_noise_t* mod);

/**
 * @brief Get current serotonin level
 *
 * @param mod Neuromodulator system
 * @return Current serotonin [0, 1]
 */
float neuromod_pink_get_serotonin(const neuromod_pink_noise_t* mod);

/**
 * @brief Get current acetylcholine level
 *
 * @param mod Neuromodulator system
 * @return Current acetylcholine [0, 1]
 */
float neuromod_pink_get_acetylcholine(const neuromod_pink_noise_t* mod);

/**
 * @brief Get current norepinephrine level
 *
 * @param mod Neuromodulator system
 * @return Current norepinephrine [0, 1]
 */
float neuromod_pink_get_norepinephrine(const neuromod_pink_noise_t* mod);

/**
 * @brief Get all current levels
 *
 * @param mod Neuromodulator system
 * @param dopamine Output dopamine
 * @param serotonin Output serotonin
 * @param acetylcholine Output acetylcholine
 * @param norepinephrine Output norepinephrine
 */
void neuromod_pink_get_all(
    const neuromod_pink_noise_t* mod,
    float* dopamine,
    float* serotonin,
    float* acetylcholine,
    float* norepinephrine
);

//=============================================================================
// Integration with Synapse Learning
//=============================================================================

/**
 * @brief Compute learning rate modulation from neuromodulators
 *
 * WHAT: Scale synapse learning rate by dopamine
 * WHY: Dopamine gates plasticity (Schultz et al., 1997)
 * HOW: learning_rate_effective = base_rate × dopamine
 *
 * @param mod Neuromodulator system
 * @param base_learning_rate Base learning rate
 * @return Modulated learning rate
 */
float neuromod_pink_compute_learning_rate(
    const neuromod_pink_noise_t* mod,
    float base_learning_rate
);

/**
 * @brief Compute attention weight from acetylcholine
 *
 * WHAT: Scale attention by acetylcholine
 * WHY: ACh enhances attention and salience
 * HOW: attention_weight = 0.5 + 0.5 × acetylcholine
 *
 * @param mod Neuromodulator system
 * @return Attention weight [0.5, 1.0]
 */
float neuromod_pink_compute_attention(const neuromod_pink_noise_t* mod);

//=============================================================================
// Persistence API (Save/Load) - Phase 10.x
//=============================================================================

/**
 * @brief Save pink noise neuromodulator state to file
 *
 * WHAT: Serialize pink noise neuromodulator system to binary file
 * WHY:  Enable persistence of neuromodulator levels and pink noise generators
 * HOW:  Write version, baselines, current levels, noise amplitudes, and generators
 *
 * @param mod Pink noise neuromodulator system
 * @param file Open file handle for writing
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must ensure exclusive access)
 */
bool neuromod_pink_save(neuromod_pink_noise_t* mod, FILE* file);

/**
 * @brief Load pink noise neuromodulator state from file
 *
 * WHAT: Deserialize pink noise neuromodulator system from binary file
 * WHY:  Restore saved neuromodulator levels and pink noise generators
 * HOW:  Read version, validate, reconstruct state
 *
 * @param file Open file handle for reading
 * @return Pink noise neuromodulator handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (creates new instance)
 */
neuromod_pink_noise_t* neuromod_pink_load(FILE* file);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEUROMOD_PINK_NOISE_H
