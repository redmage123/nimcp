#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_neuromod_pink_noise.c - Pink Noise Neuromodulation Implementation
//=============================================================================
/**
 * @file nimcp_neuromod_pink_noise.c
 * @brief Implementation of pink noise-modulated neuromodulation system
 *
 * WHAT: Neuromodulator system with 1/f noise for multi-timescale exploration
 * WHY:
 *   - Biological neuromodulators exhibit pink noise fluctuations
 *   - Multi-timescale exploration balances exploitation vs exploration
 *   - Long-range correlations enable contextual learning
 *
 * ARCHITECTURE:
 * ```
 * Neuromodulator Level = baseline + external_signal × gain + pink_noise × amplitude
 *
 * Four Neuromodulators:
 * 1. Dopamine: Reward-driven learning (baseline 0.2, noise 10%)
 * 2. Serotonin: Punishment/patience (baseline 0.3, noise 5%)
 * 3. Acetylcholine: Attention/salience (baseline 0.4, noise 15%)
 * 4. Norepinephrine: Arousal/stress (baseline 0.1, noise 8%)
 *
 * Pink Noise: Voss-McCartney algorithm, α=1.0 (pink spectrum)
 * Frequency Range: 0.1 - 10 Hz (matches biological timescales)
 * ```
 *
 * BIOLOGICAL BASIS:
 * - Dopamine neurons show 1/f noise in firing (Montague et al., 2004)
 * - Serotonin fluctuations follow pink spectrum (Cools et al., 2008)
 * - Long-range correlations enable credit assignment across time
 *
 * PERFORMANCE:
 * - Update: O(1) - single pink noise sample per neuromodulator
 * - Query: O(1) - direct field access
 * - Integration: O(1) - simple multiplication with learning rates
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 * @version 2.7.0 Phase 3
 */

#include "plasticity/neuromodulators/nimcp_neuromod_pink_noise.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "security/nimcp_security.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "plasticity_neuromod_pink_noise"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromod_pink_noise)

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Get default configuration for pink noise neuromodulation
 *
 * WHAT: Return sensible default values for all configuration parameters
 * WHY: Provide biologically plausible starting point
 * HOW: Static initialization with empirically chosen values
 *
 * BASELINE LEVELS:
 * - Dopamine: 0.2 (low baseline, driven by reward)
 * - Serotonin: 0.3 (moderate baseline for stability)
 * - Acetylcholine: 0.4 (higher baseline for attention)
 * - Norepinephrine: 0.1 (low baseline, activated by arousal)
 *
 * NOISE AMPLITUDES (percentage of baseline):
 * - Dopamine: 10% (moderate exploration)
 * - Serotonin: 5% (stable, less variable)
 * - Acetylcholine: 15% (high variability for attention shifts)
 * - Norepinephrine: 8% (moderate stress variability)
 *
 * PINK NOISE PARAMETERS:
 * - α = 1.0 (pink spectrum, 1/f)
 * - Frequency range: 0.1-10 Hz (biological timescales)
 * - Sample rate: 1000 Hz (1ms timesteps)
 *
 * TUNING: Values based on computational neuroscience literature
 *
 * @return Default configuration structure
 */
neuromod_pink_config_t neuromod_pink_default_config(void) {
    neuromod_pink_config_t config = {
        .dopamine_baseline = 0.2F,           // Low baseline, reward-driven
        .serotonin_baseline = 0.3F,          // Moderate baseline for stability
        .acetylcholine_baseline = 0.4F,      // Higher baseline for attention
        .norepinephrine_baseline = 0.1F,     // Low baseline, arousal-driven
        .dopamine_noise_amplitude = 0.1F,    // 10% exploration noise
        .serotonin_noise_amplitude = 0.05F,  // 5% stability noise
        .acetylcholine_noise_amplitude = 0.15F,  // 15% attention variability
        .norepinephrine_noise_amplitude = 0.08F, // 8% stress variability
        .alpha = 1.0F,                       // Pink spectrum (1/f)
        .min_frequency = 0.1F,               // 0.1 Hz (slow fluctuations)
        .max_frequency = 10.0F,              // 10 Hz (fast fluctuations)
        .sample_rate = 1000.0F               // 1 kHz (1ms timesteps)
    };
    return config;
}

/**
 * @brief Create pink noise-modulated neuromodulator system
 *
 * WHAT: Allocate and initialize neuromodulator system with pink noise generators
 * WHY: Enable multi-timescale exploration in learning
 * HOW: Create structure, initialize fields, create 4 pink noise generators
 *
 * ALGORITHM:
 * 1. Allocate neuromod_pink_noise_t structure (zero-initialized)
 * 2. Copy baseline and noise amplitude configuration
 * 3. Initialize current levels to baselines
 * 4. Create pink noise generator for each neuromodulator
 * 5. Verify all generators created successfully
 *
 * RESOURCE ALLOCATION:
 * - Main structure: nimcp_calloc(1, sizeof(neuromod_pink_noise_t))
 * - 4 pink noise generators: ~4KB each (Voss-McCartney state)
 * - Total: ~16KB per neuromodulator system
 *
 * ERROR HANDLING:
 * - Returns NULL if config is NULL
 * - Returns NULL if allocation fails
 * - Returns NULL if any pink noise generator fails to create
 * - Cleanup: neuromod_pink_destroy() called on partial initialization
 *
 * PERFORMANCE: O(1) - fixed-size allocations
 *
 * @param config Configuration (must not be NULL)
 * @return Neuromodulator system handle, or NULL on error
 */
neuromod_pink_noise_t* neuromod_pink_create(const neuromod_pink_config_t* config) {
    // Guard: NULL config
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    // WHAT: Allocate main structure (zero-initialized)
    // WHY: nimcp_calloc ensures all fields start at zero (safe defaults)
    // HOW: Single allocation for entire structure with memory tracking
    neuromod_pink_noise_t* mod = (neuromod_pink_noise_t*)nimcp_calloc(1, sizeof(neuromod_pink_noise_t));

    // Guard: Allocation failure
    if (!mod) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mod is NULL");

        return NULL;
    }

    // STEP 1: Copy baseline levels from configuration
    // WHAT: Set resting levels for each neuromodulator
    // WHY: Baselines provide stable reference point
    // HOW: Direct assignment from config
    mod->dopamine_baseline = config->dopamine_baseline;
    mod->serotonin_baseline = config->serotonin_baseline;
    mod->acetylcholine_baseline = config->acetylcholine_baseline;
    mod->norepinephrine_baseline = config->norepinephrine_baseline;

    // STEP 2: Initialize current levels to baselines
    // WHAT: Set initial neuromodulator levels
    // WHY: Start at resting state (no external signals yet)
    // HOW: Copy baseline to current
    mod->dopamine_current = mod->dopamine_baseline;
    mod->serotonin_current = mod->serotonin_baseline;
    mod->acetylcholine_current = mod->acetylcholine_baseline;
    mod->norepinephrine_current = mod->norepinephrine_baseline;

    // STEP 3: Copy noise amplitudes from configuration
    // WHAT: Set exploration noise level for each neuromodulator
    // WHY: Different neuromodulators have different variability
    // HOW: Direct assignment from config
    mod->dopamine_noise_amplitude = config->dopamine_noise_amplitude;
    mod->serotonin_noise_amplitude = config->serotonin_noise_amplitude;
    mod->acetylcholine_noise_amplitude = config->acetylcholine_noise_amplitude;
    mod->norepinephrine_noise_amplitude = config->norepinephrine_noise_amplitude;

    // STEP 4: Configure pink noise generators
    // WHAT: Create configuration for Voss-McCartney algorithm
    // WHY: All 4 generators share same spectral properties
    // HOW: Single config, amplitude = 1.0 (will scale by noise_amplitude)
    pink_noise_config_t noise_config = {
        .method = PINK_NOISE_VOSS,          // Voss-McCartney algorithm
        .alpha = config->alpha,              // Spectral exponent (1.0 = pink)
        .amplitude = 1.0F,                   // Unit amplitude (scaled later)
        .min_frequency = config->min_frequency,  // 0.1 Hz (slow fluctuations)
        .max_frequency = config->max_frequency,  // 10 Hz (fast fluctuations)
        .sample_rate = config->sample_rate,      // 1000 Hz (1ms timesteps)
        .seed = 0                            // Auto-seed from system time
    };

    // STEP 5: Create pink noise generators (one per neuromodulator)
    // WHAT: Instantiate 4 independent pink noise generators
    // WHY: Each neuromodulator needs independent noise source
    // HOW: Call pink_noise_create() 4 times with same config
    mod->dopamine_noise = pink_noise_create(&noise_config);
    mod->serotonin_noise = pink_noise_create(&noise_config);
    mod->acetylcholine_noise = pink_noise_create(&noise_config);
    mod->norepinephrine_noise = pink_noise_create(&noise_config);

    // Guard: Verify all generators created successfully
    // WHAT: Check if any generator creation failed
    // WHY: Partial initialization is unusable
    // HOW: Cleanup and return NULL on any failure
    if (!mod->dopamine_noise || !mod->serotonin_noise ||
        !mod->acetylcholine_noise || !mod->norepinephrine_noise) {
        neuromod_pink_destroy(mod);  // Cleanup partial initialization
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_pink_create: operation failed");
        return NULL;
    }

    return mod;
}

/**
 * @brief Destroy pink noise neuromodulator system
 *
 * WHAT: Free all resources associated with neuromodulator system
 * WHY: Prevent memory leaks
 * HOW: Destroy all pink noise generators, then free main structure
 *
 * ALGORITHM:
 * 1. Check if mod is NULL (safe to call on NULL)
 * 2. Destroy each pink noise generator (if exists)
 * 3. Free main structure
 *
 * RESOURCE CLEANUP:
 * - 4 pink noise generators (~16KB total)
 * - Main structure (~400 bytes)
 *
 * SAFETY:
 * - NULL-safe: Can call on NULL pointer
 * - Partial cleanup: Can call on partially initialized structure
 *
 * PERFORMANCE: O(1) - fixed cleanup
 *
 * @param mod Neuromodulator system to destroy (can be NULL)
 */
void neuromod_pink_destroy(neuromod_pink_noise_t* mod) {
    // Guard: NULL mod (safe to destroy NULL)
    if (!mod) {
        return;
    }

    // WHAT: Destroy pink noise generators
    // WHY: Free pink noise generator memory (~4KB each)
    // HOW: Call pink_noise_destroy() on each generator
    // SAFETY: pink_noise_destroy() is NULL-safe
    if (mod->dopamine_noise) pink_noise_destroy(mod->dopamine_noise);
    if (mod->serotonin_noise) pink_noise_destroy(mod->serotonin_noise);
    if (mod->acetylcholine_noise) pink_noise_destroy(mod->acetylcholine_noise);
    if (mod->norepinephrine_noise) pink_noise_destroy(mod->norepinephrine_noise);

    // WHAT: Free main structure
    // WHY: Release neuromod_pink_noise_t allocation
    // HOW: Use nimcp_free() for memory tracking and leak detection
    nimcp_free(mod);
}

//=============================================================================
// Update
//=============================================================================

/**
 * @brief Update all neuromodulator levels with external signals and pink noise
 *
 * WHAT: Compute new neuromodulator levels from signals and exploration noise
 * WHY: Combine task-driven signals with multi-timescale exploration
 * HOW: Generate pink noise samples, combine with baselines and signals, clamp
 *
 * ALGORITHM:
 * ```
 * For each neuromodulator:
 *   1. Generate pink noise sample from dedicated generator
 *   2. Combine: level = baseline + (signal × gain) + (noise × amplitude)
 *   3. Clamp to [0, 1] range
 *
 * Update statistics:
 *   - Increment update counter
 *   - Exponential moving average: avg = 0.99 × avg + 0.01 × current
 * ```
 *
 * NEUROMODULATOR FUNCTIONS:
 * - Dopamine: Reward-driven learning (gain=0.5, explores ±10%)
 * - Serotonin: Punishment/patience (gain=0.3, explores ±5%)
 * - Acetylcholine: Attention/salience (gain=0.4, explores ±15%)
 * - Norepinephrine: Arousal/stress (gain=0.6, explores ±8%)
 *
 * BIOLOGICAL BASIS:
 * - Pink noise provides long-range correlations (multi-timescale exploration)
 * - Gains tuned to match biological neuromodulator sensitivities
 * - Exploration enables discovery of novel strategies
 *
 * PERFORMANCE: O(1) - 4 pink noise samples + arithmetic
 * THREAD-SAFETY: Not thread-safe (caller must synchronize)
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
) {
    // Guard: NULL mod
    if (!mod) {
        return;
    }

    // STEP 1: Generate pink noise samples
    // WHAT: Sample from each neuromodulator's pink noise generator
    // WHY: Independent noise sources for each neuromodulator
    // HOW: Call pink_noise_generate() with count=1
    // NOTE: Each generator maintains independent 1/f state
    float dopamine_noise, serotonin_noise, acetylcholine_noise, norepinephrine_noise;
    pink_noise_generate(mod->dopamine_noise, &dopamine_noise, 1);
    pink_noise_generate(mod->serotonin_noise, &serotonin_noise, 1);
    pink_noise_generate(mod->acetylcholine_noise, &acetylcholine_noise, 1);
    pink_noise_generate(mod->norepinephrine_noise, &norepinephrine_noise, 1);

    // STEP 2: Update dopamine
    // WHAT: Combine baseline + reward signal + pink noise
    // WHY: Dopamine signals reward prediction errors (Schultz, 1997)
    // HOW: baseline + reward×0.5 + noise×amplitude
    // TUNING: gain=0.5 chosen for moderate sensitivity to reward
    mod->dopamine_current =
        mod->dopamine_baseline +
        reward_signal * 0.5F +
        dopamine_noise * mod->dopamine_noise_amplitude;

    // STEP 3: Update serotonin
    // WHAT: Combine baseline + punishment signal + pink noise
    // WHY: Serotonin modulates patience and aversion (Cools et al., 2008)
    // HOW: baseline + punishment×0.3 + noise×amplitude
    // TUNING: gain=0.3 for lower sensitivity (serotonin more stable)
    mod->serotonin_current =
        mod->serotonin_baseline +
        punishment_signal * 0.3F +
        serotonin_noise * mod->serotonin_noise_amplitude;

    // STEP 4: Update acetylcholine
    // WHAT: Combine baseline + salience signal + pink noise
    // WHY: Acetylcholine enhances attention to salient stimuli (Yu & Dayan, 2005)
    // HOW: baseline + salience×0.4 + noise×amplitude
    // TUNING: gain=0.4 for moderate attention modulation
    mod->acetylcholine_current =
        mod->acetylcholine_baseline +
        salience_signal * 0.4F +
        acetylcholine_noise * mod->acetylcholine_noise_amplitude;

    // STEP 5: Update norepinephrine
    // WHAT: Combine baseline + arousal signal + pink noise
    // WHY: Norepinephrine signals arousal/uncertainty (Aston-Jones & Cohen, 2005)
    // HOW: baseline + arousal×0.6 + noise×amplitude
    // TUNING: gain=0.6 for high sensitivity to arousal
    mod->norepinephrine_current =
        mod->norepinephrine_baseline +
        arousal_signal * 0.6F +
        norepinephrine_noise * mod->norepinephrine_noise_amplitude;

    // STEP 6: Clamp all levels to valid range [0, 1]
    // WHAT: Ensure neuromodulator levels stay in bounds
    // WHY: Biological levels are bounded (can't have negative dopamine!)
    // HOW: fminf(1.0, fmaxf(0.0, value))
    mod->dopamine_current = fminf(1.0F, fmaxf(0.0F, mod->dopamine_current));
    mod->serotonin_current = fminf(1.0F, fmaxf(0.0F, mod->serotonin_current));
    mod->acetylcholine_current = fminf(1.0F, fmaxf(0.0F, mod->acetylcholine_current));
    mod->norepinephrine_current = fminf(1.0F, fmaxf(0.0F, mod->norepinephrine_current));

    // STEP 7: Update statistics
    // WHAT: Track update count and exponential moving averages
    // WHY: Monitoring long-term trends in neuromodulator levels
    // HOW: EMA with α=0.01 (99% weight on history, 1% on current)
    mod->update_count++;
    mod->avg_dopamine = (mod->avg_dopamine * 0.99F) + (mod->dopamine_current * 0.01F);
    if (!isfinite(mod->avg_dopamine)) mod->avg_dopamine = mod->dopamine_current;
    mod->avg_serotonin = (mod->avg_serotonin * 0.99F) + (mod->serotonin_current * 0.01F);
    if (!isfinite(mod->avg_serotonin)) mod->avg_serotonin = mod->serotonin_current;
}

/**
 * @brief Simplified update with only reward signal
 *
 * WHAT: Update neuromodulators with just reward (common case)
 * WHY: Most RL applications only need reward signal
 * HOW: Call full update with other signals at zero
 *
 * ALGORITHM:
 * neuromod_pink_update(mod, reward, 0, 0, 0)
 *
 * USE CASE: Simple reinforcement learning tasks
 *
 * PERFORMANCE: O(1) - delegates to neuromod_pink_update()
 *
 * @param mod Neuromodulator system
 * @param reward Reward signal [-1, 1]
 */
void neuromod_pink_update_reward(neuromod_pink_noise_t* mod, float reward) {
    neuromod_pink_update(mod, reward, 0.0F, 0.0F, 0.0F);
}

//=============================================================================
// Query
//=============================================================================

/**
 * @brief Get current dopamine level
 *
 * WHAT: Return current dopamine concentration
 * WHY: Query reward-related neuromodulation state
 * HOW: Direct field access
 *
 * @param mod Neuromodulator system (can be NULL)
 * @return Current dopamine [0, 1], or 0.0 if mod is NULL
 */
float neuromod_pink_get_dopamine(const neuromod_pink_noise_t* mod) {
    return mod ? mod->dopamine_current : 0.0F;
}

/**
 * @brief Get current serotonin level
 *
 * WHAT: Return current serotonin concentration
 * WHY: Query punishment/patience state
 * HOW: Direct field access
 *
 * @param mod Neuromodulator system (can be NULL)
 * @return Current serotonin [0, 1], or 0.0 if mod is NULL
 */
float neuromod_pink_get_serotonin(const neuromod_pink_noise_t* mod) {
    return mod ? mod->serotonin_current : 0.0F;
}

/**
 * @brief Get current acetylcholine level
 *
 * WHAT: Return current acetylcholine concentration
 * WHY: Query attention/salience state
 * HOW: Direct field access
 *
 * @param mod Neuromodulator system (can be NULL)
 * @return Current acetylcholine [0, 1], or 0.0 if mod is NULL
 */
float neuromod_pink_get_acetylcholine(const neuromod_pink_noise_t* mod) {
    return mod ? mod->acetylcholine_current : 0.0F;
}

/**
 * @brief Get current norepinephrine level
 *
 * WHAT: Return current norepinephrine concentration
 * WHY: Query arousal/stress state
 * HOW: Direct field access
 *
 * @param mod Neuromodulator system (can be NULL)
 * @return Current norepinephrine [0, 1], or 0.0 if mod is NULL
 */
float neuromod_pink_get_norepinephrine(const neuromod_pink_noise_t* mod) {
    return mod ? mod->norepinephrine_current : 0.0F;
}

/**
 * @brief Get all neuromodulator levels at once
 *
 * WHAT: Query all four neuromodulator concentrations
 * WHY: Efficient batch query (one function call instead of four)
 * HOW: Direct field access for each requested output
 *
 * SAFETY: NULL output pointers are skipped (won't crash)
 *
 * @param mod Neuromodulator system
 * @param dopamine Output for dopamine (can be NULL)
 * @param serotonin Output for serotonin (can be NULL)
 * @param acetylcholine Output for acetylcholine (can be NULL)
 * @param norepinephrine Output for norepinephrine (can be NULL)
 */
void neuromod_pink_get_all(
    const neuromod_pink_noise_t* mod,
    float* dopamine,
    float* serotonin,
    float* acetylcholine,
    float* norepinephrine
) {
    // Guard: NULL mod
    if (!mod) {
        return;
    }

    // WHAT: Write outputs if pointers provided
    // WHY: Allow caller to request only subset of neuromodulators
    // HOW: NULL-check each output pointer before writing
    if (dopamine) *dopamine = mod->dopamine_current;
    if (serotonin) *serotonin = mod->serotonin_current;
    if (acetylcholine) *acetylcholine = mod->acetylcholine_current;
    if (norepinephrine) *norepinephrine = mod->norepinephrine_current;
}

//=============================================================================
// Integration
//=============================================================================

/**
 * @brief Compute dopamine-modulated learning rate
 *
 * WHAT: Scale synapse learning rate by dopamine level
 * WHY: Dopamine gates plasticity (Schultz et al., 1997)
 * HOW: learning_rate_effective = base_rate × dopamine
 *
 * BIOLOGICAL BASIS:
 * - Dopamine D1 receptors enhance LTP (long-term potentiation)
 * - High dopamine → stronger plasticity
 * - Low dopamine → weak/no plasticity
 *
 * USE CASE: Apply to synapse weight updates:
 * ```
 * float lr = neuromod_pink_compute_learning_rate(mod, 0.01f);
 * synapse->weight += lr * reward_signal * activity;
 * ```
 *
 * @param mod Neuromodulator system
 * @param base_learning_rate Base learning rate (unmodulated)
 * @return Modulated learning rate [0, base_learning_rate]
 */
float neuromod_pink_compute_learning_rate(
    const neuromod_pink_noise_t* mod,
    float base_learning_rate
) {
    // Guard: NULL mod (return unmodulated rate)
    if (!mod) {
        return base_learning_rate;
    }

    // WHAT: Scale learning rate by dopamine
    // WHY: Dopamine gates synaptic plasticity
    // HOW: Simple multiplication (dopamine ∈ [0, 1])
    return base_learning_rate * mod->dopamine_current;
}

/**
 * @brief Compute acetylcholine-modulated attention weight
 *
 * WHAT: Scale attention by acetylcholine level
 * WHY: Acetylcholine enhances attention and salience (Yu & Dayan, 2005)
 * HOW: attention = 0.5 + 0.5 × acetylcholine
 *
 * BIOLOGICAL BASIS:
 * - Acetylcholine enhances cortical processing of attended stimuli
 * - High ACh → sharp attention (weight → 1.0)
 * - Low ACh → diffuse attention (weight → 0.5)
 *
 * USE CASE: Apply to input gating:
 * ```
 * float attention = neuromod_pink_compute_attention(mod);
 * float gated_input = input * attention;
 * ```
 *
 * @param mod Neuromodulator system
 * @return Attention weight [0.5, 1.0]
 */
float neuromod_pink_compute_attention(const neuromod_pink_noise_t* mod) {
    // Guard: NULL mod (return neutral attention)
    if (!mod) {
        return 0.5F;
    }

    // WHAT: Scale attention by acetylcholine
    // WHY: ACh enhances sensory processing
    // HOW: Map [0, 1] → [0.5, 1.0] (never fully off)
    return 0.5F + 0.5F * mod->acetylcholine_current;
}

//=============================================================================
// Persistence API (Save/Load) - Phase 10.x
//=============================================================================

/**
 * @brief Save pink noise neuromodulator state to file
 *
 * WHAT: Serialize pink noise neuromodulator system to binary file
 * WHY:  Enable persistence of neuromodulator levels and pink noise generators
 * HOW:  Write version, baselines, current levels, noise amplitudes, and pink noise generators
 *
 * Binary format:
 *   uint32_t version (1)
 *   float dopamine_baseline
 *   float serotonin_baseline
 *   float acetylcholine_baseline
 *   float norepinephrine_baseline
 *   float dopamine_current
 *   float serotonin_current
 *   float acetylcholine_current
 *   float norepinephrine_current
 *   float dopamine_noise_amplitude
 *   float serotonin_noise_amplitude
 *   float acetylcholine_noise_amplitude
 *   float norepinephrine_noise_amplitude
 *   uint64_t update_count
 *   float avg_dopamine
 *   float avg_serotonin
 *   pink_noise_generator_t dopamine_noise (via pink_noise_save)
 *   pink_noise_generator_t serotonin_noise (via pink_noise_save)
 *   pink_noise_generator_t acetylcholine_noise (via pink_noise_save)
 *   pink_noise_generator_t norepinephrine_noise (via pink_noise_save)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must ensure exclusive access)
 *
 * @param mod Pink noise neuromodulator system
 * @param file Open file handle for writing
 * @return true on success, false on error
 */
bool neuromod_pink_save(neuromod_pink_noise_t* mod, FILE* file)
{
    // Guard: Validate parameters
    if (!mod || !file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_pink_save: required parameter is NULL (mod, file)");
        return false;
    }

    // WHAT: Write version marker for backward compatibility
    // WHY:  Enable future format changes while supporting old saves
    // HOW:  Write uint32_t version = 1
    uint32_t version = 1;
    if (fwrite(&version, sizeof(uint32_t), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }

    // WHAT: Write baseline levels
    // WHY:  Restore resting neuromodulator levels
    // HOW:  Binary write of 4 float baselines
    if (fwrite(&mod->dopamine_baseline, sizeof(float), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }
    if (fwrite(&mod->serotonin_baseline, sizeof(float), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }
    if (fwrite(&mod->acetylcholine_baseline, sizeof(float), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }
    if (fwrite(&mod->norepinephrine_baseline, sizeof(float), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }

    // WHAT: Write current levels
    // WHY:  Restore active neuromodulator concentrations
    // HOW:  Binary write of 4 float current levels
    if (fwrite(&mod->dopamine_current, sizeof(float), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }
    if (fwrite(&mod->serotonin_current, sizeof(float), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }
    if (fwrite(&mod->acetylcholine_current, sizeof(float), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }
    if (fwrite(&mod->norepinephrine_current, sizeof(float), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }

    // WHAT: Write noise amplitudes
    // WHY:  Restore exploration noise levels
    // HOW:  Binary write of 4 float amplitudes
    if (fwrite(&mod->dopamine_noise_amplitude, sizeof(float), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }
    if (fwrite(&mod->serotonin_noise_amplitude, sizeof(float), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }
    if (fwrite(&mod->acetylcholine_noise_amplitude, sizeof(float), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }
    if (fwrite(&mod->norepinephrine_noise_amplitude, sizeof(float), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }

    // WHAT: Write statistics
    // WHY:  Preserve historical tracking data
    // HOW:  Binary write of update_count, avg_dopamine, avg_serotonin
    if (fwrite(&mod->update_count, sizeof(uint64_t), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }
    if (fwrite(&mod->avg_dopamine, sizeof(float), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }
    if (fwrite(&mod->avg_serotonin, sizeof(float), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: validation failed");
        return false;
    }

    // WHAT: Write pink noise generators
    // WHY:  Restore exact noise generator state for reproducibility
    // HOW:  Use pink_noise_save API for each generator
    if (!pink_noise_save(mod->dopamine_noise, file)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: pink_noise_save is NULL");
        return false;
    }
    if (!pink_noise_save(mod->serotonin_noise, file)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: pink_noise_save is NULL");
        return false;
    }
    if (!pink_noise_save(mod->acetylcholine_noise, file)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: pink_noise_save is NULL");
        return false;
    }
    if (!pink_noise_save(mod->norepinephrine_noise, file)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_save: pink_noise_save is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Load pink noise neuromodulator state from file
 *
 * WHAT: Deserialize pink noise neuromodulator system from binary file
 * WHY:  Restore saved neuromodulator levels and pink noise generators
 * HOW:  Read version, validate, reconstruct state
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (creates new instance)
 *
 * @param file Open file handle for reading
 * @return Pink noise neuromodulator handle or NULL on error
 */
neuromod_pink_noise_t* neuromod_pink_load(FILE* file)
{
    // Guard: Validate parameter
    if (!file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "file is NULL");

        return NULL;
    }

    // WHAT: Read and validate version
    // WHY:  Ensure format compatibility
    // HOW:  Read version, check against current version
    uint32_t version = 0;
    if (fread(&version, sizeof(uint32_t), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_pink_load: validation failed");
        return NULL;
    }

    if (version != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_pink_load: validation failed");
        return NULL;
    }

    // WHAT: Allocate pink noise neuromodulator structure
    // WHY:  Need structure to hold loaded data
    // HOW:  Use nimcp_calloc for zero-initialization
    neuromod_pink_noise_t* mod = (neuromod_pink_noise_t*)nimcp_calloc(1, sizeof(neuromod_pink_noise_t));
    if (!mod) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mod is NULL");

        return NULL;
    }

    // WHAT: Read baseline levels
    // WHY:  Restore resting neuromodulator levels
    // HOW:  Binary read of 4 float baselines
    if (fread(&mod->dopamine_baseline, sizeof(float), 1, file) != 1) goto cleanup;
    if (fread(&mod->serotonin_baseline, sizeof(float), 1, file) != 1) goto cleanup;
    if (fread(&mod->acetylcholine_baseline, sizeof(float), 1, file) != 1) goto cleanup;
    if (fread(&mod->norepinephrine_baseline, sizeof(float), 1, file) != 1) goto cleanup;

    // WHAT: Read current levels
    // WHY:  Restore active neuromodulator concentrations
    // HOW:  Binary read of 4 float current levels
    if (fread(&mod->dopamine_current, sizeof(float), 1, file) != 1) goto cleanup;
    if (fread(&mod->serotonin_current, sizeof(float), 1, file) != 1) goto cleanup;
    if (fread(&mod->acetylcholine_current, sizeof(float), 1, file) != 1) goto cleanup;
    if (fread(&mod->norepinephrine_current, sizeof(float), 1, file) != 1) goto cleanup;

    // WHAT: Read noise amplitudes
    // WHY:  Restore exploration noise levels
    // HOW:  Binary read of 4 float amplitudes
    if (fread(&mod->dopamine_noise_amplitude, sizeof(float), 1, file) != 1) goto cleanup;
    if (fread(&mod->serotonin_noise_amplitude, sizeof(float), 1, file) != 1) goto cleanup;
    if (fread(&mod->acetylcholine_noise_amplitude, sizeof(float), 1, file) != 1) goto cleanup;
    if (fread(&mod->norepinephrine_noise_amplitude, sizeof(float), 1, file) != 1) goto cleanup;

    // WHAT: Read statistics
    // WHY:  Restore historical tracking data
    // HOW:  Binary read of update_count, avg_dopamine, avg_serotonin
    if (fread(&mod->update_count, sizeof(uint64_t), 1, file) != 1) goto cleanup;
    if (fread(&mod->avg_dopamine, sizeof(float), 1, file) != 1) goto cleanup;
    if (fread(&mod->avg_serotonin, sizeof(float), 1, file) != 1) goto cleanup;

    // WHAT: Read pink noise generators
    // WHY:  Restore exact noise generator state
    // HOW:  Use pink_noise_load API for each generator
    mod->dopamine_noise = pink_noise_load(file);
    if (!mod->dopamine_noise) goto cleanup;

    mod->serotonin_noise = pink_noise_load(file);
    if (!mod->serotonin_noise) goto cleanup;

    mod->acetylcholine_noise = pink_noise_load(file);
    if (!mod->acetylcholine_noise) goto cleanup;

    mod->norepinephrine_noise = pink_noise_load(file);
    if (!mod->norepinephrine_noise) goto cleanup;

    return mod;

cleanup:
    // WHAT: Cleanup on error
    // WHY:  Prevent memory leaks
    // HOW:  Free allocated resources
    if (mod) {
        neuromod_pink_destroy(mod);
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_pink_load: validation failed");
    return NULL;
}
