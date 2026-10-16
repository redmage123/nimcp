/**
 * @file nimcp_systems_consolidation_pink_noise_bridge.h
 * @brief Pink Noise Integration Bridge for Sleep Replay
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Integrates sleep-stage specific pink noise modulation with memory replay
 * WHY:  Neural noise characteristics vary across sleep stages and affect replay quality
 * HOW:  Pink noise sleep bridge modulates replay strength and consolidation based on
 *       noise characteristics (amplitude, spectral exponent) for each sleep stage
 *
 * BIOLOGICAL BASIS:
 * =================
 * Neural noise profoundly affects memory consolidation during sleep:
 *
 * 1. SLOW-WAVE SLEEP (N3):
 *    - Redder noise spectrum (α≈1.5, higher amplitude)
 *    - Optimal for hippocampal-cortical replay
 *    - Strong replay modulation enhances consolidation
 *    - Models sharp-wave ripples coordination
 *
 * 2. REM SLEEP:
 *    - Whiter noise (α≈0.8, variable amplitude)
 *    - Integration and abstraction of memories
 *    - Moderate replay strength with creativity bursts
 *    - Models theta sequences and dream processing
 *
 * 3. LIGHT NREM (N2):
 *    - Intermediate noise (α≈0.9-1.0)
 *    - Spindle bursts enhance memory tagging
 *    - Moderate replay for memory sorting
 *
 * 4. WAKE/DROWSY:
 *    - Pink noise (α≈1.0, low amplitude)
 *    - Minimal replay, high precision
 *    - Spontaneous reactivation only
 *
 * INTEGRATION EFFECTS:
 * ====================
 * - Noise amplitude → Replay strength (higher amp = stronger consolidation)
 * - Spectral exponent (alpha) → Transfer quality (redder = more stable)
 * - Spindle bursts (N2) → Priority modulation (tagging for later consolidation)
 * - Arousal level → Replay frequency (deep sleep = high frequency)
 *
 * REFERENCES:
 * - Churchland, M.M. et al. (2010). "Stimulus onset quenches neural variability"
 * - Dehghani, N. et al. (2010). "Comparative power spectral analysis of sleep"
 * - Tononi, G. & Cirelli, C. (2014). "Sleep and synaptic homeostasis hypothesis"
 * - Rasch, B. & Born, J. (2013). "About sleep's role in memory"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SYSTEMS_CONSOLIDATION_PINK_NOISE_BRIDGE_H
#define NIMCP_SYSTEMS_CONSOLIDATION_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/memory/nimcp_systems_consolidation.h"
#include "plasticity/noise/nimcp_pink_noise_sleep.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/* Replay strength modulation by sleep stage (relative to base replay strength) */
#define REPLAY_STRENGTH_WAKE              0.1f   /**< Minimal spontaneous replay */
#define REPLAY_STRENGTH_DROWSY            0.2f   /**< Transition state */
#define REPLAY_STRENGTH_N1                0.4f   /**< Light sleep beginning */
#define REPLAY_STRENGTH_N2                0.7f   /**< Spindle-enhanced replay */
#define REPLAY_STRENGTH_N3                1.0f   /**< Optimal SWS consolidation */
#define REPLAY_STRENGTH_REM               0.6f   /**< Integration/abstraction */

/* Transfer quality modulation by spectral exponent (alpha) */
#define TRANSFER_QUALITY_ALPHA_MIN        0.5f   /**< White noise (poor quality) */
#define TRANSFER_QUALITY_ALPHA_OPTIMAL    1.5f   /**< Red noise (optimal in SWS) */
#define TRANSFER_QUALITY_ALPHA_MAX        2.0f   /**< Very red (saturation) */

/* Replay frequency modulation by arousal */
#define REPLAY_FREQ_DEEP_SLEEP            10.0f  /**< Hz in deep sleep */
#define REPLAY_FREQ_LIGHT_SLEEP           5.0f   /**< Hz in light sleep */
#define REPLAY_FREQ_AWAKE                 0.5f   /**< Hz spontaneous */

//=============================================================================
// Configuration
//=============================================================================

/**
 * @struct consolidation_pink_noise_config_t
 * @brief Configuration for pink noise integration with replay system
 *
 * WHAT: Controls how pink noise characteristics affect memory replay
 * WHY:  Different sleep stages require different noise-replay coupling
 * HOW:  Maps noise parameters to replay modulation factors
 */
typedef struct {
    bool enable_amplitude_modulation;     /**< Noise amplitude affects replay strength */
    bool enable_alpha_modulation;         /**< Spectral exponent affects transfer quality */
    bool enable_spindle_priority;         /**< Spindle bursts boost replay priority */
    bool enable_arousal_frequency;        /**< Arousal modulates replay frequency */

    float amplitude_gain;                 /**< Scaling factor for amplitude effect [default: 1.0] */
    float alpha_sensitivity;              /**< Sensitivity to spectral changes [default: 0.5] */
    float spindle_priority_boost;         /**< Priority increase during spindles [default: 0.3] */

    float modulation_smoothing;           /**< Temporal smoothing factor [default: 0.1] */
} consolidation_pink_noise_config_t;

//=============================================================================
// Bridge State
//=============================================================================

/**
 * @struct consolidation_pink_noise_effects_t
 * @brief Computed effects of pink noise on replay system
 *
 * WHAT: Real-time modulation factors derived from pink noise state
 * WHY:  Encapsulates all noise-driven replay adjustments
 * HOW:  Updated each step based on current noise characteristics
 */
typedef struct {
    float replay_strength_factor;         /**< Multiply base replay strength [0.0-1.0] */
    float transfer_quality_factor;        /**< Multiply transfer rate [0.0-1.5] */
    float replay_frequency_hz;            /**< Current replay rate (Hz) */
    float priority_modulation;            /**< Add to replay priority [0.0-1.0] */

    pink_sleep_stage_t current_stage;     /**< Current sleep stage */
    float noise_amplitude;                /**< Current noise amplitude */
    float noise_alpha;                    /**< Current spectral exponent */
    bool in_spindle_burst;                /**< True during N2 spindles */
    float arousal_level;                  /**< Current arousal [0=deep, 1=alert] */
} consolidation_pink_noise_effects_t;

/**
 * @struct consolidation_pink_noise_bridge_t
 * @brief Bridge connecting pink noise sleep module to systems consolidation
 *
 * WHAT: Manages bidirectional integration between noise and replay
 * WHY:  Sleep-stage specific noise profiles optimize consolidation
 * HOW:  Monitors pink noise state, computes modulation, applies to replay
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    // Configuration
    consolidation_pink_noise_config_t config;

    // Module connections (non-owning pointers)
    systems_consolidation_system_t* consolidation_system;
    pink_sleep_bridge_t* pink_noise_bridge;

    // Current effects state
    consolidation_pink_noise_effects_t effects;

    // Smoothing state for temporal consistency
    float smoothed_replay_strength;
    float smoothed_transfer_quality;
    float smoothed_frequency;

    // Statistics
    uint64_t total_updates;
    uint64_t spindle_boosts_applied;
    uint64_t stage_transitions;

    // Thread safety
} consolidation_pink_noise_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration for pink noise bridge
 *
 * WHAT: Returns sensible default parameters for noise-replay integration
 * WHY:  Provides starting point for typical use cases
 * HOW:  Sets biologically-plausible defaults based on sleep research
 *
 * @param config Output parameter for configuration
 * @return 0 on success, negative error code on failure
 *
 * USAGE:
 * consolidation_pink_noise_config_t config;
 * consolidation_pink_noise_default_config(&config);
 */
int consolidation_pink_noise_default_config(consolidation_pink_noise_config_t* config);

/**
 * @brief Create pink noise bridge for systems consolidation
 *
 * WHAT: Initializes bridge between pink noise sleep and memory replay
 * WHY:  Required to enable noise-modulated consolidation
 * HOW:  Allocates bridge structure, connects to both modules
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param consolidation_system Systems consolidation module (must not be NULL)
 * @param pink_noise_bridge Pink noise sleep module (must not be NULL)
 * @return Pointer to new bridge, or NULL on failure
 *
 * BIOLOGICAL RATIONALE:
 * - Connects noise characteristics to replay quality
 * - Enables sleep-stage appropriate consolidation
 *
 * USAGE:
 * consolidation_pink_noise_bridge_t* bridge = consolidation_pink_noise_create(
 *     &config, consolidation_sys, pink_noise);
 */
consolidation_pink_noise_bridge_t* consolidation_pink_noise_create(
    const consolidation_pink_noise_config_t* config,
    systems_consolidation_system_t* consolidation_system,
    pink_sleep_bridge_t* pink_noise_bridge);

/**
 * @brief Destroy pink noise bridge and free resources
 *
 * WHAT: Cleans up bridge structure and releases memory
 * WHY:  Prevents memory leaks
 * HOW:  Frees all allocated resources, nullifies pointers
 *
 * @param bridge Bridge to destroy (can be NULL)
 *
 * USAGE:
 * consolidation_pink_noise_destroy(bridge);
 */
void consolidation_pink_noise_destroy(consolidation_pink_noise_bridge_t* bridge);

//=============================================================================
// Update and Modulation API
//=============================================================================

/**
 * @brief Update bridge state based on current pink noise characteristics
 *
 * WHAT: Reads pink noise state and computes replay modulation factors
 * WHY:  Keeps modulation synchronized with changing noise profile
 * HOW:  Samples noise parameters, applies formulas, smooths transitions
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, negative error code on failure
 *
 * COMPUTATION:
 * - Replay strength = stage_base × (1 + amplitude_gain × amplitude)
 * - Transfer quality = 1 + alpha_sensitivity × (alpha - 1.0)
 * - Frequency = arousal_dependent (0.5-10 Hz)
 * - Priority boost = spindle_factor × spindle_active
 *
 * THREAD-SAFE: Yes (uses internal mutex)
 *
 * USAGE:
 * consolidation_pink_noise_update(bridge);  // Call before execute_replays
 */
int consolidation_pink_noise_update(consolidation_pink_noise_bridge_t* bridge);

/**
 * @brief Apply pink noise modulation to replay operations
 *
 * WHAT: Adjusts replay parameters in consolidation system
 * WHY:  Implements noise-driven consolidation optimization
 * HOW:  Modifies replay strength, transfer rate, frequency based on effects
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, negative error code on failure
 *
 * EFFECTS:
 * - Sets consolidation system's replay frequency
 * - Modulates transfer rate for next replay batch
 * - Adjusts replay priority queue processing
 *
 * THREAD-SAFE: Yes
 *
 * USAGE:
 * consolidation_pink_noise_update(bridge);
 * consolidation_pink_noise_apply_modulation(bridge);
 * systems_consolidation_execute_replays(sys, dt, is_sws, is_rem);
 */
int consolidation_pink_noise_apply_modulation(consolidation_pink_noise_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current pink noise effects on replay
 *
 * WHAT: Retrieves computed modulation factors
 * WHY:  Allows inspection of how noise is affecting consolidation
 * HOW:  Returns copy of current effects structure
 *
 * @param bridge Pink noise bridge
 * @param effects_out Output parameter for effects
 * @return 0 on success, negative error code on failure
 *
 * USAGE:
 * consolidation_pink_noise_effects_t effects;
 * consolidation_pink_noise_get_effects(bridge, &effects);
 * printf("Replay strength: %.2f\n", effects.replay_strength_factor);
 */
int consolidation_pink_noise_get_effects(
    const consolidation_pink_noise_bridge_t* bridge,
    consolidation_pink_noise_effects_t* effects_out);

/**
 * @brief Get current replay strength factor
 *
 * WHAT: Returns current replay strength modulation
 * WHY:  Quick access to primary modulation parameter
 * HOW:  Extracts smoothed replay strength from effects
 *
 * @param bridge Pink noise bridge
 * @return Replay strength factor [0.0-1.0], or -1.0 on error
 *
 * USAGE:
 * float strength = consolidation_pink_noise_get_replay_strength(bridge);
 */
float consolidation_pink_noise_get_replay_strength(
    const consolidation_pink_noise_bridge_t* bridge);

/**
 * @brief Get current transfer quality factor
 *
 * WHAT: Returns current transfer quality modulation
 * WHY:  Indicates how well memories will consolidate
 * HOW:  Extracts smoothed transfer quality from effects
 *
 * @param bridge Pink noise bridge
 * @return Transfer quality factor [0.0-1.5], or -1.0 on error
 *
 * USAGE:
 * float quality = consolidation_pink_noise_get_transfer_quality(bridge);
 */
float consolidation_pink_noise_get_transfer_quality(
    const consolidation_pink_noise_bridge_t* bridge);

/**
 * @brief Get current replay frequency
 *
 * WHAT: Returns arousal-modulated replay frequency
 * WHY:  Indicates how often replays should occur
 * HOW:  Extracts current frequency from effects
 *
 * @param bridge Pink noise bridge
 * @return Replay frequency in Hz [0.5-10.0], or -1.0 on error
 *
 * USAGE:
 * float freq_hz = consolidation_pink_noise_get_replay_frequency(bridge);
 */
float consolidation_pink_noise_get_replay_frequency(
    const consolidation_pink_noise_bridge_t* bridge);

/**
 * @brief Check if currently in spindle burst (N2 sleep)
 *
 * WHAT: Returns whether spindle priority boost is active
 * WHY:  Spindles tag memories for consolidation
 * HOW:  Checks spindle state in pink noise bridge
 *
 * @param bridge Pink noise bridge
 * @return true if in spindle burst, false otherwise
 *
 * USAGE:
 * if (consolidation_pink_noise_in_spindle_burst(bridge)) {
 *     // Apply priority boost to current replay
 * }
 */
bool consolidation_pink_noise_in_spindle_burst(
    const consolidation_pink_noise_bridge_t* bridge);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieves operational statistics for monitoring
 * WHY:  Useful for debugging and performance analysis
 * HOW:  Returns counts and transitions from bridge state
 *
 * @param bridge Pink noise bridge
 * @param total_updates_out Total update calls
 * @param spindle_boosts_out Number of spindle-boosted replays
 * @param stage_transitions_out Number of sleep stage transitions observed
 * @return 0 on success, negative error code on failure
 *
 * USAGE:
 * uint64_t updates, boosts, transitions;
 * consolidation_pink_noise_get_statistics(bridge, &updates, &boosts, &transitions);
 */
int consolidation_pink_noise_get_statistics(
    const consolidation_pink_noise_bridge_t* bridge,
    uint64_t* total_updates_out,
    uint64_t* spindle_boosts_out,
    uint64_t* stage_transitions_out);

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Compute base replay strength for sleep stage
 *
 * WHAT: Returns stage-specific base replay strength
 * WHY:  Different stages have different consolidation potential
 * HOW:  Maps sleep stage enum to empirically-derived strength factor
 *
 * @param stage Sleep stage from pink noise module
 * @return Base replay strength [0.1-1.0]
 *
 * BIOLOGICAL MAPPING:
 * - WAKE: 0.1 (spontaneous reactivation only)
 * - DROWSY: 0.2 (transition)
 * - N1: 0.4 (light sleep beginning)
 * - N2: 0.7 (spindle-enhanced)
 * - N3: 1.0 (optimal SWS consolidation)
 * - REM: 0.6 (integration/abstraction)
 *
 * USAGE:
 * float base_strength = consolidation_pink_noise_stage_replay_strength(PINK_SLEEP_N3);
 */
float consolidation_pink_noise_stage_replay_strength(pink_sleep_stage_t stage);

/**
 * @brief Compute transfer quality factor from spectral exponent
 *
 * WHAT: Maps noise spectral exponent (alpha) to transfer quality
 * WHY:  Redder noise (higher alpha) provides more stable consolidation
 * HOW:  Normalized mapping with optimal point at alpha≈1.5
 *
 * @param alpha Spectral exponent from pink noise [0.5-2.0]
 * @return Transfer quality factor [0.5-1.5]
 *
 * BIOLOGICAL RATIONALE:
 * - White noise (α≈0.5): Poor consolidation, high variability
 * - Pink noise (α≈1.0): Good baseline consolidation
 * - Red noise (α≈1.5): Optimal for SWS replay coordination
 * - Very red (α≈2.0): Saturation, diminishing returns
 *
 * USAGE:
 * float quality = consolidation_pink_noise_alpha_to_quality(1.5f);
 */
float consolidation_pink_noise_alpha_to_quality(float alpha);

/**
 * @brief Compute replay frequency from arousal level
 *
 * WHAT: Maps arousal to replay rate (Hz)
 * WHY:  Deep sleep enables faster replay coordination
 * HOW:  Inverse relationship: low arousal = high frequency
 *
 * @param arousal Arousal level [0=deep sleep, 1=alert]
 * @return Replay frequency in Hz [0.5-10.0]
 *
 * BIOLOGICAL BASIS:
 * - Deep sleep (arousal ≈ 0.0): ~10 Hz sharp-wave ripples
 * - Light sleep (arousal ≈ 0.5): ~5 Hz theta sequences
 * - Awake (arousal ≈ 1.0): ~0.5 Hz spontaneous
 *
 * USAGE:
 * float freq = consolidation_pink_noise_arousal_to_frequency(0.2f);
 */
float consolidation_pink_noise_arousal_to_frequency(float arousal);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SYSTEMS_CONSOLIDATION_PINK_NOISE_BRIDGE_H */
