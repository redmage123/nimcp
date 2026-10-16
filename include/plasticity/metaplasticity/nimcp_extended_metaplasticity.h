/**
 * @file nimcp_extended_metaplasticity.h
 * @brief Extended Metaplasticity - Multi-timescale Threshold Adaptation
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Multi-timescale metaplasticity with neuromodulator and sleep modulation
 * WHY:  Biological plasticity thresholds adapt on multiple timescales (seconds to days)
 *       and are reset during sleep, shifted by neuromodulators like dopamine
 * HOW:  Track threshold history over hours, apply neuromodulator shifts, reset during sleep
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * METAPLASTICITY (Abraham & Bear 1996):
 * ---------------------------------------
 * "Plasticity of plasticity" - the threshold for inducing LTP/LTD itself changes
 * based on prior activity history. This prevents runaway potentiation/depression
 * and implements activity-dependent homeostasis.
 *
 * CORE PRINCIPLE:
 * θ_m(t) = f(activity_history, neuromodulators, sleep_state)
 *
 * Where θ_m is the modification threshold that determines LTP vs LTD:
 * - If activity > θ_m → LTP (potentiation)
 * - If activity < θ_m → LTD (depression)
 *
 * EXTENDED FEATURES:
 * ------------------
 *
 * 1. MULTI-TIMESCALE HISTORY (Abraham & Bear 1996, Bienenstock et al. 1982):
 *    - Fast threshold (seconds-minutes): Recent activity average
 *    - Slow threshold (hours-days): Long-term activity baseline
 *    - θ_m = θ_baseline + ∫(activity²(t-τ)) dτ  (tau = hours)
 *    - Longer history windows (1-24 hours) vs standard BCM (minutes)
 *    - Reference: Cooper & Bear (2012) "The BCM theory of synapse modification"
 *
 * 2. NEUROMODULATOR THRESHOLD SHIFTS:
 *    - Dopamine: Shifts θ_m DOWN → more LTP susceptibility (reward learning)
 *    - Norepinephrine: Shifts θ_m DOWN → arousal-enhanced plasticity
 *    - Acetylcholine: Modulates θ_m sensitivity to input statistics
 *    - Serotonin: Shifts θ_m UP → stabilization, reduced plasticity
 *    - Reference: Pawlak et al. (2010) "Dopamine receptor activation modulates LTP"
 *
 * 3. SLEEP-DEPENDENT THRESHOLD RESETTING:
 *    - AWAKE: θ_m tracks activity history normally
 *    - NREM Sleep: θ_m resets toward baseline (synaptic homeostasis)
 *    - Deep NREM: Strong reset (70-90% back to baseline)
 *    - REM: Moderate reset with exploration (40-60%)
 *    - Reference: Tononi & Cirelli (2014) "Sleep and synaptic homeostasis"
 *
 * 4. INFLAMMATION EFFECTS (via immune bridge):
 *    - Pro-inflammatory cytokines: Shift θ_m UP → LTP impairment
 *    - IL-1β, IL-6, TNF-α: Increase threshold (harder to potentiate)
 *    - IL-10: Restore θ_m to baseline (anti-inflammatory recovery)
 *
 * MATHEMATICAL FORMULATION:
 * -------------------------
 *
 * Baseline threshold dynamics:
 * dθ_baseline/dt = (⟨r²⟩ - θ_baseline) / τ_baseline
 *
 * Extended threshold with history:
 * θ_m(t) = θ_baseline(t) × (1 + ∫_0^T w(τ) × r²(t-τ) dτ)
 *
 * where w(τ) = exp(-τ/τ_history) is history weighting
 *
 * Neuromodulator shift:
 * θ_effective = θ_m × (1 - DA_shift × [DA] + 5HT_shift × [5HT])
 *
 * Sleep reset:
 * θ_m(wake) = θ_m(sleep) × (1 - reset_factor) + θ_baseline × reset_factor
 *
 * DESIGN PATTERNS:
 * ----------------
 * - Strategy Pattern: Different threshold adaptation strategies
 * - Observer Pattern: Threshold change callbacks
 * - Factory Method: Preset configurations for brain regions
 *
 * PERFORMANCE:
 * ------------
 * - O(1) per synapse threshold update
 * - O(N) for history buffer updates (circular buffer, amortized O(1))
 * - SIMD-friendly (no branches in hot path)
 *
 * @author NIMCP Development Team
 * @reference Abraham & Bear (1996) "Metaplasticity: plasticity of synaptic plasticity"
 * @reference Bienenstock, Cooper, Munro (1982) "Theory for development of neuron selectivity"
 */

#ifndef NIMCP_EXTENDED_METAPLASTICITY_H
#define NIMCP_EXTENDED_METAPLASTICITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* NIMCP core utilities */
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "cognitive/nimcp_sleep_wake.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define METAPLASTICITY_DEFAULT_BASELINE_TAU     3600000.0f   /**< 1 hour in ms */
#define METAPLASTICITY_DEFAULT_HISTORY_TAU      21600000.0f  /**< 6 hours in ms */
#define METAPLASTICITY_DEFAULT_HISTORY_SIZE     1024         /**< History buffer size */
#define METAPLASTICITY_MIN_THRESHOLD            0.001f       /**< Minimum θ_m */
#define METAPLASTICITY_MAX_THRESHOLD            100.0f       /**< Maximum θ_m */
#define METAPLASTICITY_DEFAULT_THETA_BASELINE   1.0f         /**< Default baseline */

/* Neuromodulator threshold shift factors */
#define METAPLASTICITY_DA_SHIFT_FACTOR          0.3f         /**< DA lowers threshold 30% */
#define METAPLASTICITY_NE_SHIFT_FACTOR          0.2f         /**< NE lowers threshold 20% */
#define METAPLASTICITY_ACH_SHIFT_FACTOR         0.1f         /**< ACh modulates sensitivity */
#define METAPLASTICITY_5HT_SHIFT_FACTOR         0.25f        /**< 5HT raises threshold 25% */

/* Sleep reset factors (how much to reset toward baseline) */
#define METAPLASTICITY_SLEEP_RESET_AWAKE        0.0f         /**< No reset */
#define METAPLASTICITY_SLEEP_RESET_DROWSY       0.1f         /**< 10% reset */
#define METAPLASTICITY_SLEEP_RESET_LIGHT_NREM   0.4f         /**< 40% reset */
#define METAPLASTICITY_SLEEP_RESET_DEEP_NREM    0.8f         /**< 80% reset */
#define METAPLASTICITY_SLEEP_RESET_REM          0.5f         /**< 50% reset */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Neuromodulator concentrations
 *
 * WHAT: Current neuromodulator levels affecting threshold
 * WHY:  Different neuromodulators shift threshold in different directions
 */
typedef struct {
    float dopamine;         /**< DA concentration [0-1] */
    float norepinephrine;   /**< NE concentration [0-1] */
    float acetylcholine;    /**< ACh concentration [0-1] */
    float serotonin;        /**< 5HT concentration [0-1] */
} neuromodulator_levels_t;

/**
 * @brief Threshold history entry
 *
 * WHAT: Single history point for threshold adaptation
 * WHY:  Track activity over long timescales (hours)
 */
typedef struct {
    float activity_squared;     /**< r² at this timepoint */
    uint64_t timestamp_ms;      /**< When this was recorded */
} threshold_history_entry_t;

/**
 * @brief Extended metaplasticity state
 *
 * WHAT: Per-synapse or per-neuron metaplasticity state
 * WHY:  Track multi-timescale threshold dynamics
 *
 * MEMORY: ~40 bytes + history buffer
 */
typedef struct {
    /* Baseline threshold (slow timescale) */
    float theta_baseline;           /**< Current baseline threshold */
    float theta_baseline_target;    /**< Target from activity squared average */

    /* Effective threshold (fast timescale) */
    float theta_effective;          /**< Current effective threshold after modulation */

    /* Activity tracking */
    float activity_avg;             /**< Running average of activity */
    float activity_squared_avg;     /**< Running average of r² */

    /* History buffer (long timescale) */
    threshold_history_entry_t* history;  /**< Circular buffer */
    uint32_t history_size;               /**< Buffer capacity */
    uint32_t history_index;              /**< Current write position */
    uint32_t history_count;              /**< Number of valid entries */

    /* Neuromodulator effects */
    float da_shift;                 /**< Current DA threshold shift */
    float ne_shift;                 /**< Current NE threshold shift */
    float ach_modulation;           /**< Current ACh sensitivity modulation */
    float serotonin_shift;          /**< Current 5HT threshold shift */

    /* Sleep effects */
    sleep_state_t current_sleep_state;  /**< Current sleep state */
    float sleep_reset_factor;           /**< Current sleep reset strength */
    float pre_sleep_theta;              /**< Threshold before sleep onset */

    /* Timestamps */
    uint64_t last_update_ms;        /**< Last update time */

    /* Thread safety */
    nimcp_platform_mutex_t lock;    /**< Mutex for concurrent access */
} extended_metaplasticity_state_t;

/**
 * @brief Extended metaplasticity configuration
 *
 * WHAT: Configuration for extended metaplasticity
 * WHY:  Different brain regions have different threshold dynamics
 */
typedef struct {
    /* Time constants */
    float baseline_tau_ms;          /**< Baseline adaptation timescale (hours) */
    float history_tau_ms;           /**< History decay timescale (hours) */
    float activity_tau_ms;          /**< Activity averaging timescale (seconds) */

    /* History buffer */
    uint32_t history_size;          /**< Size of history buffer */
    float history_sample_interval_ms;  /**< How often to sample history */

    /* Threshold bounds */
    float min_theta;                /**< Minimum threshold */
    float max_theta;                /**< Maximum threshold */
    float initial_theta_baseline;   /**< Initial baseline value */

    /* Neuromodulator sensitivity */
    float da_sensitivity;           /**< DA effect strength [0-2] */
    float ne_sensitivity;           /**< NE effect strength [0-2] */
    float ach_sensitivity;          /**< ACh effect strength [0-2] */
    float serotonin_sensitivity;    /**< 5HT effect strength [0-2] */

    /* Sleep integration */
    bool enable_sleep_reset;        /**< Enable sleep-dependent reset */
    float sleep_reset_strength;     /**< Overall sleep reset multiplier [0-2] */

    /* Feature flags */
    bool enable_neuromodulator_shifts;  /**< Enable neuromodulator effects */
    bool enable_long_term_history;      /**< Enable multi-hour history */
    bool enable_callbacks;              /**< Enable threshold change callbacks */
    bool enable_bio_async;              /**< Enable bio-async messaging */
} extended_metaplasticity_config_t;

/**
 * @brief Metaplasticity statistics
 *
 * WHAT: Monitoring metrics for metaplasticity
 * WHY:  Track threshold dynamics and convergence
 */
typedef struct {
    float mean_theta_baseline;      /**< Average baseline threshold */
    float mean_theta_effective;     /**< Average effective threshold */
    float theta_variance;           /**< Threshold variance across synapses */
    float mean_da_shift;            /**< Average DA shift */
    float mean_sleep_reset;         /**< Average sleep reset factor */
    uint64_t total_updates;         /**< Total threshold updates */
    uint64_t sleep_resets;          /**< Number of sleep resets */
    uint64_t threshold_changes;     /**< Significant threshold changes */
} metaplasticity_stats_t;

/**
 * @brief Threshold change callback
 *
 * WHAT: Notification when threshold changes significantly
 * WHY:  Allow other systems to react to metaplastic changes
 *
 * @param state Metaplasticity state
 * @param old_theta Previous effective threshold
 * @param new_theta New effective threshold
 * @param user_data User-provided context
 */
typedef void (*threshold_change_callback_t)(
    const extended_metaplasticity_state_t* state,
    float old_theta,
    float new_theta,
    void* user_data
);

/**
 * @brief Opaque handle to metaplasticity controller
 */
typedef struct metaplasticity_controller_struct* metaplasticity_controller_t;

/* ============================================================================
 * Factory Functions - Configuration Presets
 * ============================================================================ */

/**
 * @brief Get default extended metaplasticity configuration
 *
 * WHAT: Factory method for standard cortical metaplasticity
 * WHY:  Provide biologically plausible defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * BIOLOGICAL: Based on Abraham & Bear (1996) and BCM theory
 * - Baseline tau: 1 hour (slow adaptation)
 * - History tau: 6 hours (long-term integration)
 * - DA shift: 30% reduction (reward learning)
 *
 * @return Default configuration
 */
extended_metaplasticity_config_t metaplasticity_config_default(void);

/**
 * @brief Get fast metaplasticity configuration
 *
 * WHAT: Accelerated threshold adaptation for development
 * WHY:  Critical periods have faster metaplastic changes
 * HOW:  Shorter time constants, larger shifts
 *
 * BIOLOGICAL: Models developmental plasticity
 *
 * @return Fast adaptation configuration
 */
extended_metaplasticity_config_t metaplasticity_config_fast(void);

/**
 * @brief Get slow metaplasticity configuration
 *
 * WHAT: Sluggish threshold adaptation for mature cortex
 * WHY:  Adult cortex has more stable thresholds
 * HOW:  Longer time constants, smaller shifts
 *
 * BIOLOGICAL: Models adult cortical stability
 *
 * @return Slow adaptation configuration
 */
extended_metaplasticity_config_t metaplasticity_config_slow(void);

/**
 * @brief Get hippocampal metaplasticity configuration
 *
 * WHAT: Hippocampus-specific threshold dynamics
 * WHY:  Hippocampus has unique plasticity requirements
 * HOW:  Fast baseline, strong DA sensitivity
 *
 * BIOLOGICAL: Hippocampal place cells and episodic memory
 *
 * @return Hippocampal configuration
 */
extended_metaplasticity_config_t metaplasticity_config_hippocampal(void);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Initialize extended metaplasticity state
 *
 * WHAT: Create and initialize metaplasticity state
 * WHY:  Allocate resources and set initial values
 * HOW:  Allocate history buffer, initialize thresholds
 *
 * COMPLEXITY: O(1) + O(N) for history allocation
 *
 * @param config Configuration (NULL for defaults)
 * @return Initialized state or NULL on failure
 */
extended_metaplasticity_state_t* metaplasticity_state_create(
    const extended_metaplasticity_config_t* config
);

/**
 * @brief Destroy extended metaplasticity state
 *
 * WHAT: Free metaplasticity state resources
 * WHY:  Prevent memory leaks
 * HOW:  Free history buffer and state structure
 *
 * @param state State to destroy
 */
void metaplasticity_state_destroy(extended_metaplasticity_state_t* state);

/**
 * @brief Reset metaplasticity state to initial values
 *
 * WHAT: Clear history and reset thresholds
 * WHY:  Start fresh after major perturbation
 * HOW:  Clear history buffer, reset to baseline
 *
 * @param state State to reset
 * @param config Configuration (NULL to keep current)
 */
void metaplasticity_state_reset(
    extended_metaplasticity_state_t* state,
    const extended_metaplasticity_config_t* config
);

/* ============================================================================
 * Core Update Functions
 * ============================================================================ */

/**
 * @brief Update baseline threshold (slow timescale)
 *
 * WHAT: Adapt baseline threshold based on long-term activity
 * WHY:  Baseline tracks hours-long activity trends
 * HOW:  dθ_baseline/dt = (⟨r²⟩ - θ_baseline) / τ_baseline
 *
 * BIOLOGICAL: Abraham & Bear (1996) multi-timescale adaptation
 * COMPLEXITY: O(1)
 *
 * @param state Metaplasticity state
 * @param current_activity Current activity level
 * @param dt Time step (ms)
 * @param config Configuration
 * @return 0 on success, -1 on error
 */
int metaplasticity_update_baseline(
    extended_metaplasticity_state_t* state,
    float current_activity,
    float dt,
    const extended_metaplasticity_config_t* config
);

/**
 * @brief Update activity history buffer
 *
 * WHAT: Add current activity to long-term history
 * WHY:  Multi-hour history affects threshold
 * HOW:  Circular buffer with exponential decay weighting
 *
 * COMPLEXITY: O(1) amortized
 *
 * @param state Metaplasticity state
 * @param current_activity Current activity level
 * @param timestamp_ms Current time
 * @param config Configuration
 * @return 0 on success, -1 on error
 */
int metaplasticity_update_history(
    extended_metaplasticity_state_t* state,
    float current_activity,
    uint64_t timestamp_ms,
    const extended_metaplasticity_config_t* config
);

/**
 * @brief Apply neuromodulator threshold shifts
 *
 * WHAT: Modulate threshold based on neuromodulator levels
 * WHY:  DA/NE lower threshold, 5HT raises it
 * HOW:  θ_eff = θ_m × (1 - DA_shift + 5HT_shift)
 *
 * BIOLOGICAL: Pawlak et al. (2010) dopamine-LTP interactions
 * COMPLEXITY: O(1)
 *
 * @param state Metaplasticity state
 * @param neuromod_levels Neuromodulator concentrations
 * @param config Configuration
 * @return 0 on success, -1 on error
 */
int metaplasticity_apply_neuromodulator_shifts(
    extended_metaplasticity_state_t* state,
    const neuromodulator_levels_t* neuromod_levels,
    const extended_metaplasticity_config_t* config
);

/**
 * @brief Apply sleep-dependent threshold reset
 *
 * WHAT: Reset threshold toward baseline during sleep
 * WHY:  Sleep renormalizes thresholds (synaptic homeostasis)
 * HOW:  θ_m ← θ_m × (1 - reset) + θ_baseline × reset
 *
 * BIOLOGICAL: Tononi & Cirelli (2014) synaptic homeostasis hypothesis
 * COMPLEXITY: O(1)
 *
 * @param state Metaplasticity state
 * @param sleep_state Current sleep state
 * @param config Configuration
 * @return 0 on success, -1 on error
 */
int metaplasticity_apply_sleep_reset(
    extended_metaplasticity_state_t* state,
    sleep_state_t sleep_state,
    const extended_metaplasticity_config_t* config
);

/**
 * @brief Compute effective threshold from history
 *
 * WHAT: Calculate current effective threshold
 * WHY:  Integrate all factors (baseline, history, neuromod, sleep)
 * HOW:  θ_eff = f(θ_baseline, history, DA, 5HT, sleep)
 *
 * COMPLEXITY: O(N) where N = history size (can be optimized)
 *
 * @param state Metaplasticity state
 * @param config Configuration
 * @return Effective threshold value
 */
float metaplasticity_compute_effective_threshold(
    const extended_metaplasticity_state_t* state,
    const extended_metaplasticity_config_t* config
);

/**
 * @brief Full metaplasticity update (all components)
 *
 * WHAT: Update all metaplasticity components in one call
 * WHY:  Convenience function for complete update
 * HOW:  Call baseline, history, neuromod, sleep in sequence
 *
 * COMPLEXITY: O(N) for history
 *
 * @param state Metaplasticity state
 * @param current_activity Current activity level
 * @param neuromod_levels Neuromodulator levels (NULL to skip)
 * @param dt Time step (ms)
 * @param config Configuration
 * @return 0 on success, -1 on error
 */
int metaplasticity_update(
    extended_metaplasticity_state_t* state,
    float current_activity,
    const neuromodulator_levels_t* neuromod_levels,
    float dt,
    const extended_metaplasticity_config_t* config
);

/* ============================================================================
 * Query Functions
 * ============================================================================ */

/**
 * @brief Get current effective threshold
 *
 * WHAT: Retrieve current θ_effective
 * WHY:  Used for LTP/LTD decision
 * HOW:  Return state->theta_effective
 *
 * @param state Metaplasticity state
 * @return Effective threshold
 */
float metaplasticity_get_threshold(const extended_metaplasticity_state_t* state);

/**
 * @brief Get current baseline threshold
 *
 * WHAT: Retrieve slow-timescale baseline
 * WHY:  Monitor long-term adaptation
 * HOW:  Return state->theta_baseline
 *
 * @param state Metaplasticity state
 * @return Baseline threshold
 */
float metaplasticity_get_baseline(const extended_metaplasticity_state_t* state);

/**
 * @brief Get activity-dependent plasticity rate
 *
 * WHAT: Compute plasticity rate modulation
 * WHY:  Threshold distance affects plasticity strength
 * HOW:  rate ∝ |activity - θ_m|
 *
 * @param state Metaplasticity state
 * @param current_activity Current activity
 * @param base_rate Base plasticity rate
 * @return Modulated plasticity rate
 */
float metaplasticity_get_plasticity_rate(
    const extended_metaplasticity_state_t* state,
    float current_activity,
    float base_rate
);

/**
 * @brief Check if activity will induce LTP
 *
 * WHAT: Determine LTP vs LTD based on threshold
 * WHY:  Decision function for plasticity direction
 * HOW:  Return (activity > θ_effective)
 *
 * @param state Metaplasticity state
 * @param activity Activity level to test
 * @return true if LTP, false if LTD
 */
bool metaplasticity_will_induce_ltp(
    const extended_metaplasticity_state_t* state,
    float activity
);

/* ============================================================================
 * Controller Functions (Multi-synapse Management)
 * ============================================================================ */

/**
 * @brief Create metaplasticity controller
 *
 * WHAT: Factory method for multi-synapse controller
 * WHY:  Manage metaplasticity across synapse population
 * HOW:  Allocate array of states
 *
 * @param config Configuration
 * @param num_synapses Number of synapses
 * @return Controller handle or NULL on failure
 */
metaplasticity_controller_t metaplasticity_controller_create(
    const extended_metaplasticity_config_t* config,
    uint32_t num_synapses
);

/**
 * @brief Destroy metaplasticity controller
 *
 * WHAT: Free controller resources
 * WHY:  Prevent memory leaks
 * HOW:  Free all states and controller
 *
 * @param controller Controller to destroy
 */
void metaplasticity_controller_destroy(metaplasticity_controller_t controller);

/**
 * @brief Update all synapses in controller
 *
 * WHAT: Batch update for all managed synapses
 * WHY:  Efficient parallel processing
 * HOW:  Update each state with corresponding activity
 *
 * @param controller Controller handle
 * @param activities Array of activity levels [num_synapses]
 * @param neuromod_levels Neuromodulator levels (NULL to skip)
 * @param dt Time step (ms)
 * @return 0 on success, -1 on error
 */
int metaplasticity_controller_update_all(
    metaplasticity_controller_t controller,
    const float* activities,
    const neuromodulator_levels_t* neuromod_levels,
    float dt
);

/**
 * @brief Get statistics for controller
 *
 * WHAT: Compute aggregate statistics across synapses
 * WHY:  Monitor population dynamics
 * HOW:  Aggregate state values across all synapses
 *
 * @param controller Controller handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int metaplasticity_controller_get_stats(
    metaplasticity_controller_t controller,
    metaplasticity_stats_t* stats
);

/**
 * @brief Set sleep state for all synapses
 *
 * WHAT: Update sleep state for entire population
 * WHY:  Synchronize sleep effects across network
 * HOW:  Set sleep state in each managed state
 *
 * @param controller Controller handle
 * @param sleep_state New sleep state
 * @return 0 on success, -1 on error
 */
int metaplasticity_controller_set_sleep_state(
    metaplasticity_controller_t controller,
    sleep_state_t sleep_state
);

/**
 * @brief Register threshold change callback
 *
 * WHAT: Set callback for threshold changes
 * WHY:  Notify other systems of metaplastic events
 * HOW:  Store callback pointer, invoke on changes
 *
 * @param controller Controller handle
 * @param callback Callback function
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int metaplasticity_controller_set_callback(
    metaplasticity_controller_t controller,
    threshold_change_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Module Management
 * ============================================================================ */

/**
 * @brief Initialize metaplasticity module
 *
 * WHAT: Set up global metaplasticity module
 * WHY:  Prepare module for use
 * HOW:  Initialize global state, bio-async if enabled
 *
 * @param config Module configuration (NULL for defaults)
 * @return true on success, false on failure
 */
bool metaplasticity_module_init(const extended_metaplasticity_config_t* config);

/**
 * @brief Destroy metaplasticity module
 *
 * WHAT: Clean up module resources
 * WHY:  Proper shutdown
 * HOW:  Reset global state, disconnect bio-async
 */
void metaplasticity_module_destroy(void);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_EXTENDED_METAPLASTICITY_H
