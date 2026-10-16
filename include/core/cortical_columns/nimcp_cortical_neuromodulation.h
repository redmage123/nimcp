/**
 * @file nimcp_cortical_neuromodulation.h
 * @brief Neuromodulatory effects system for cortical columns
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Computational model of neuromodulator effects on cortical column dynamics
 * WHY:  Neuromodulators critically gate cortical processing, plasticity, and attention;
 *       essential for adaptive, context-sensitive neural computation
 * HOW:  Models ACh, DA, NE, and 5-HT effects on gain, lateral inhibition, plasticity,
 *       and signal-to-noise ratio using biologically-grounded parameter modulation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * ACETYLCHOLINE (ACh) EFFECTS:
 * ---------------------------
 * 1. Signal-to-Noise Enhancement:
 *    - ACh suppresses recurrent excitation (layer 2/3)
 *    - Enhances thalamic input (layer 4)
 *    - Sharpens receptive fields
 *    - Reference: Hasselmo (2006) "The role of acetylcholine in learning and memory"
 *
 * 2. Lateral Inhibition Reduction:
 *    - ACh reduces intracortical inhibition
 *    - Allows broader feature representations
 *    - Enables encoding mode
 *    - Reference: Giocomo & Hasselmo (2007) "Neuromodulation by glutamate and acetylcholine"
 *
 * 3. Plasticity Gating:
 *    - ACh enables long-term potentiation (LTP)
 *    - Gates hippocampal encoding
 *    - High ACh = learn new, low ACh = retrieve old
 *    - Reference: Hasselmo & Schnell (1994) "Laminar selectivity of cholinergic suppression"
 *
 * DOPAMINE (DA) EFFECTS:
 * ----------------------
 * 1. Reward Prediction Error:
 *    - Phasic DA encodes RPE (δ = reward - prediction)
 *    - Positive RPE → DA burst
 *    - Negative RPE → DA dip
 *    - Reference: Schultz et al. (1997) "A neural substrate of prediction and reward"
 *
 * 2. Three-Factor Learning Rule:
 *    - Δw ∝ pre × post × DA
 *    - Reinforces causal synapses
 *    - Enables credit assignment
 *    - Reference: Izhikevich (2007) "Solving the distal reward problem"
 *
 * 3. Gain Modulation:
 *    - D1 receptors: increase excitability
 *    - D2 receptors: decrease excitability
 *    - Layer-specific effects (strong in PFC layer 5/6)
 *    - Reference: Seamans & Yang (2004) "The principal features of dopamine"
 *
 * 4. Spatially-Specific DA:
 *    - DA release can be column-specific
 *    - Targeted reward signals
 *    - Local credit assignment
 *    - Reference: Tritsch & Sabatini (2012) "Dopaminergic modulation of synaptic transmission"
 *
 * NOREPINEPHRINE (NE) EFFECTS:
 * ----------------------------
 * 1. Gain Boost:
 *    - NE increases neural excitability
 *    - Enhances sensory responses
 *    - Amplifies signal processing
 *    - Reference: Aston-Jones & Cohen (2005) "Locus coeruleus and attention"
 *
 * 2. Network Reset Probability:
 *    - High NE can trigger network reorganization
 *    - Escape from attractor states
 *    - Enables behavioral flexibility
 *    - Reference: Bouret & Sara (2005) "Network reset by NE"
 *
 * 3. Exploration Boost:
 *    - NE promotes exploration over exploitation
 *    - Increases stochasticity
 *    - Broadens attention
 *    - Reference: Dayan & Yu (2006) "Expected and unexpected uncertainty"
 *
 * SEROTONIN (5-HT) EFFECTS:
 * -------------------------
 * 1. Inhibition Boost:
 *    - 5-HT enhances GABAergic inhibition
 *    - Suppresses impulsive responses
 *    - Stabilizes network dynamics
 *    - Reference: Froemke (2015) "Plasticity of cortical excitatory-inhibitory balance"
 *
 * 2. Impulsivity Reduction:
 *    - High 5-HT → patience, delayed gratification
 *    - Low 5-HT → impulsivity, immediate action
 *    - Reference: Cools et al. (2008) "Serotonin and cognitive flexibility"
 *
 * 3. Mood Regulation:
 *    - 5-HT stabilizes mood
 *    - Modulates stress response
 *    - Reference: Berger et al. (2009) "The expanded biology of serotonin"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              CORTICAL NEUROMODULATION SYSTEM                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    NEUROMODULATOR LEVELS                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ACh:  ████████░░  0.8  (high attention)                          │  ║
 * ║   │   DA:   ██████████  1.0  (reward burst)                            │  ║
 * ║   │   NE:   ████░░░░░░  0.4  (moderate arousal)                        │  ║
 * ║   │   5-HT: ███░░░░░░░  0.3  (low inhibition)                          │  ║
 * ║   │                                                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║                                   ▼                                        ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                     COMPUTED EFFECTS                                │  ║
 * ║   │                                                                     │  ║
 * ║   │   Gain Modulation:          ×1.35  (DA + NE boost)                 │  ║
 * ║   │   Lateral Inhibition:       ×0.65  (ACh reduction)                 │  ║
 * ║   │   Plasticity Gate:           1.00  (ACh high → learning ON)        │  ║
 * ║   │   SNR Modulation:           ×1.50  (ACh sharpening)                │  ║
 * ║   │   Exploration:              ×1.20  (NE exploration boost)          │  ║
 * ║   │                                                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║                                   ▼                                        ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               APPLY TO CORTICAL COLUMNS                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   Column Activation ← Gain × Activation                            │  ║
 * ║   │   Lateral Inhib     ← LI_Mod × Inhibition                          │  ║
 * ║   │   Learning Rate     ← Plasticity_Gate × LR                         │  ║
 * ║   │   Receptive Field   ← SNR_Mod × Selectivity                        │  ║
 * ║   │                                                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * PERFORMANCE:
 * - Level updates: O(1)
 * - Effect computation: O(1)
 * - Per-column DA: O(N) where N = number of columns
 * - Decay/clearance: O(1)
 *
 * INTEGRATION:
 * - Compatible with nimcp_cortical_column.h
 * - Integrates with nimcp_neuromodulators.h (global system)
 * - Bio-async enabled (BIO_MODULE_CORTICAL_NEUROMOD)
 * - Thread-safe with nimcp_mutex_t
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_NEUROMODULATION_H
#define NIMCP_CORTICAL_NEUROMODULATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct cortical_neuromod_system cortical_neuromod_system_t;

//=============================================================================
// Neuromodulator Types (Local Definition)
//=============================================================================

/**
 * @brief Neuromodulator types for cortical columns
 *
 * WHAT: Four major neuromodulatory systems affecting cortical processing
 * WHY:  Each modulator has distinct computational effects
 */
typedef enum {
    CORTICAL_NEUROMOD_ACETYLCHOLINE,  /**< ACh - attention, SNR, plasticity gating */
    CORTICAL_NEUROMOD_DOPAMINE,       /**< DA - reward, reinforcement, gain */
    CORTICAL_NEUROMOD_NOREPINEPHRINE, /**< NE - arousal, gain, reset, exploration */
    CORTICAL_NEUROMOD_SEROTONIN,      /**< 5-HT - inhibition, patience, mood */
    CORTICAL_NEUROMOD_COUNT           /**< Total count */
} cortical_neuromodulator_type_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Neuromodulation configuration
 *
 * WHAT: Configuration parameters for neuromodulatory effects
 * WHY:  Allow tuning of biological parameters for different cortical regions
 */
typedef struct {
    /* Acetylcholine effects */
    float ach_snr_boost;                    /**< SNR improvement factor (default: 1.5) */
    float ach_lateral_inhibition_reduction; /**< LI reduction factor (default: 0.35) */
    float ach_plasticity_gate;              /**< Plasticity gating threshold (default: 0.5) */

    /* Dopamine effects */
    float da_reward_sensitivity;            /**< Reward response gain (default: 1.0) */
    float da_plasticity_modulation;         /**< LR modulation strength (default: 0.5) */
    float da_gain_modulation;               /**< Activation gain (default: 0.3) */

    /* Norepinephrine effects */
    float ne_gain_boost;                    /**< Excitability boost (default: 0.4) */
    float ne_reset_probability;             /**< Network reset prob (default: 0.01) */
    float ne_exploration_boost;             /**< Exploration factor (default: 0.3) */

    /* Serotonin effects */
    float serotonin_inhibition_boost;       /**< Inhibition enhancement (default: 0.4) */
    float serotonin_impulsivity_reduction;  /**< Response delay (default: 0.3) */

    /* Time constants (milliseconds) */
    float release_tau_ms;                   /**< Phasic release time constant (default: 50) */
    float clearance_tau_ms;                 /**< Clearance/decay time constant (default: 200) */

    /* Spatial resolution */
    uint32_t num_columns;                   /**< Number of cortical columns (for per-column DA) */

    /* Integration */
    bool enable_bio_async;                  /**< Enable bio-async messaging */
    bool connect_global_neuromod;           /**< Connect to global neuromodulator system */
} cortical_neuromod_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Current neuromodulator levels
 *
 * WHAT: Normalized concentrations of each neuromodulator
 * WHY:  Track dynamic state of modulatory systems
 * NOTE: All values range [0.0, 1.0]
 */
typedef struct {
    float ach_level;      /**< Acetylcholine concentration */
    float da_level;       /**< Dopamine concentration */
    float ne_level;       /**< Norepinephrine concentration */
    float serotonin_level;/**< Serotonin concentration */
} cortical_neuromod_levels_t;

/**
 * @brief Computed neuromodulatory effects
 *
 * WHAT: Multiplicative factors applied to cortical processing
 * WHY:  Separate computation from application for clarity
 * NOTE: Most factors are multiplicative (1.0 = no change)
 */
typedef struct {
    float gain_modulation;              /**< Overall neural gain (0.5 - 2.0) */
    float lateral_inhibition_modulation;/**< Lateral inhibition strength (0.3 - 1.5) */
    float plasticity_gate;              /**< Learning rate multiplier (0.0 - 1.0) */
    float snr_modulation;               /**< Signal-to-noise ratio (0.5 - 2.0) */
    float exploration_modulation;       /**< Exploration vs exploitation (0.5 - 2.0) */
} cortical_neuromod_effects_t;

/**
 * @brief Internal state of neuromodulation system
 *
 * WHAT: Complete state including levels, targets, and effects
 * WHY:  Track dynamics and enable smooth transitions
 */
typedef struct {
    cortical_neuromod_levels_t current_levels;  /**< Current concentrations */
    cortical_neuromod_levels_t target_levels;   /**< Target concentrations (for smooth decay) */
    cortical_neuromod_effects_t current_effects;/**< Currently computed effects */
    float* per_column_da;                       /**< Column-specific DA [num_columns] */
    uint32_t num_columns;                       /**< Number of columns */
    uint64_t last_update_time_us;               /**< Last update timestamp */
} cortical_neuromod_state_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Statistics for neuromodulation system
 *
 * WHAT: Monitoring and diagnostic information
 * WHY:  Track system behavior and validate biological realism
 */
typedef struct {
    /* Current state */
    cortical_neuromod_levels_t current_levels;
    cortical_neuromod_effects_t current_effects;

    /* Historical averages */
    float avg_ach;
    float avg_da;
    float avg_ne;
    float avg_serotonin;

    /* Event counts */
    uint64_t total_releases;
    uint64_t ach_releases;
    uint64_t da_releases;
    uint64_t ne_releases;
    uint64_t serotonin_releases;

    /* Plasticity events */
    uint64_t plasticity_gated_on;   /**< Times plasticity was enabled */
    uint64_t plasticity_gated_off;  /**< Times plasticity was disabled */

    /* Network resets */
    uint64_t ne_triggered_resets;   /**< Times NE triggered network reset */

    /* Dynamics */
    float da_variance;              /**< Variance in DA signal (RPE encoding) */
    float ne_arousal_stability;     /**< Stability of NE levels */
} cortical_neuromod_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create cortical neuromodulation system
 *
 * WHAT: Initialize neuromodulation with given configuration
 * WHY:  Enable modulator-gated cortical processing
 * HOW:  Allocate state, initialize levels to baseline, setup bio-async
 *
 * @param config Configuration parameters (NULL for defaults)
 * @return System handle or NULL on failure
 *
 * COMPLEXITY: O(N) where N = num_columns
 * THREAD-SAFE: Yes
 */
cortical_neuromod_system_t* cortical_neuromod_create(
    const cortical_neuromod_config_t* config
);

/**
 * @brief Destroy cortical neuromodulation system
 *
 * WHAT: Free all resources associated with neuromodulation system
 * WHY:  Clean shutdown and memory management
 * HOW:  Disconnect bio-async, free per-column DA, free system
 *
 * @param system System handle
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void cortical_neuromod_destroy(cortical_neuromod_system_t* system);

/**
 * @brief Get default configuration
 *
 * WHAT: Returns biologically realistic default parameters
 * WHY:  Simplify system creation with good defaults
 * HOW:  Populate config with literature-based values
 *
 * @param config Output configuration structure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void cortical_neuromod_default_config(cortical_neuromod_config_t* config);

//=============================================================================
// Level Control Functions
//=============================================================================

/**
 * @brief Set neuromodulator level (tonic)
 *
 * WHAT: Set baseline concentration of a neuromodulator
 * WHY:  Model tonic (sustained) neuromodulatory state
 * HOW:  Update current and target levels, recompute effects
 *
 * @param system System handle
 * @param type Neuromodulator type
 * @param level New concentration [0.0, 1.0]
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_neuromod_set_level(
    cortical_neuromod_system_t* system,
    cortical_neuromodulator_type_t type,
    float level
);

/**
 * @brief Get neuromodulator level
 *
 * WHAT: Query current concentration of a neuromodulator
 * WHY:  Read current state
 * HOW:  Return current level
 *
 * @param system System handle
 * @param type Neuromodulator type
 * @return Current level [0.0, 1.0] or -1.0 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float cortical_neuromod_get_level(
    const cortical_neuromod_system_t* system,
    cortical_neuromodulator_type_t type
);

/**
 * @brief Phasic neuromodulator release
 *
 * WHAT: Trigger burst release (phasic event)
 * WHY:  Model event-driven neuromodulation (e.g., reward → DA burst)
 * HOW:  Add magnitude to current level (clamped to 1.0), recompute effects
 *
 * @param system System handle
 * @param type Neuromodulator type
 * @param magnitude Release amount to add [0.0, 1.0]
 * @return Resulting level after release, or -1.0 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * EXAMPLE:
 * ```c
 * // Reward event → dopamine burst
 * cortical_neuromod_release(sys, CORTICAL_NEUROMOD_DOPAMINE, 0.5);
 * ```
 */
float cortical_neuromod_release(
    cortical_neuromod_system_t* system,
    cortical_neuromodulator_type_t type,
    float magnitude
);

/**
 * @brief Set column-specific dopamine level
 *
 * WHAT: Set DA level for a specific cortical column
 * WHY:  Enable spatially-targeted reward signals
 * HOW:  Update per_column_da array
 *
 * @param system System handle
 * @param column_index Column index
 * @param level DA level [0.0, 1.0]
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_neuromod_set_column_da(
    cortical_neuromod_system_t* system,
    uint32_t column_index,
    float level
);

/**
 * @brief Get column-specific dopamine level
 *
 * WHAT: Query DA level for a specific column
 * WHY:  Read column-specific reward state
 * HOW:  Return per_column_da[column_index]
 *
 * @param system System handle
 * @param column_index Column index
 * @return DA level [0.0, 1.0] or -1.0 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float cortical_neuromod_get_column_da(
    const cortical_neuromod_system_t* system,
    uint32_t column_index
);

//=============================================================================
// Dynamics Functions
//=============================================================================

/**
 * @brief Update neuromodulator dynamics (decay/clearance)
 *
 * WHAT: Apply exponential decay to neuromodulator concentrations
 * WHY:  Model biological clearance and reuptake
 * HOW:  c(t+Δt) = c(t) × exp(-Δt/τ), recompute effects
 *
 * @param system System handle
 * @param dt Time step (milliseconds)
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * ALGORITHM:
 * For each neuromodulator:
 *   level_new = level_old × exp(-dt / clearance_tau)
 */
int cortical_neuromod_update(
    cortical_neuromod_system_t* system,
    float dt_ms
);

/**
 * @brief Compute neuromodulatory effects
 *
 * WHAT: Calculate multiplicative effects from current levels
 * WHY:  Transform concentrations into functional modulation
 * HOW:  Apply biological transfer functions to each modulator
 *
 * @param system System handle
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (updates internal state with mutex)
 *
 * ALGORITHM:
 * gain = 1.0 + (da × da_gain_mod) + (ne × ne_gain_boost)
 * lateral_inhib = 1.0 - (ach × ach_li_reduction) + (serotonin × serotonin_inhib_boost)
 * plasticity_gate = (ach > threshold) ? 1.0 : 0.0
 * snr = 1.0 + (ach × ach_snr_boost)
 * exploration = 1.0 + (ne × ne_exploration_boost)
 */
int cortical_neuromod_compute_effects(cortical_neuromod_system_t* system);

//=============================================================================
// Effect Application Functions
//=============================================================================

/**
 * @brief Apply acetylcholine effects
 *
 * WHAT: Modulate cortical processing based on ACh level
 * WHY:  ACh gates attention and encoding
 * HOW:  Reduce lateral inhibition, enhance SNR, gate plasticity
 *
 * @param system System handle
 * @param base_lateral_inhibition Baseline lateral inhibition strength
 * @param base_snr Baseline signal-to-noise ratio
 * @param modulated_li Output: modulated lateral inhibition
 * @param modulated_snr Output: modulated SNR
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_neuromod_apply_ach_effects(
    const cortical_neuromod_system_t* system,
    float base_lateral_inhibition,
    float base_snr,
    float* modulated_li,
    float* modulated_snr
);

/**
 * @brief Apply dopamine effects
 *
 * WHAT: Modulate processing based on DA (reward signal)
 * WHY:  DA reinforces successful computations
 * HOW:  Boost gain, modulate plasticity
 *
 * @param system System handle
 * @param base_gain Baseline neural gain
 * @param base_learning_rate Baseline learning rate
 * @param modulated_gain Output: modulated gain
 * @param modulated_lr Output: modulated learning rate
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_neuromod_apply_da_effects(
    const cortical_neuromod_system_t* system,
    float base_gain,
    float base_learning_rate,
    float* modulated_gain,
    float* modulated_lr
);

/**
 * @brief Apply norepinephrine effects
 *
 * WHAT: Modulate processing based on NE (arousal)
 * WHY:  NE regulates alertness and exploration
 * HOW:  Boost gain, increase exploration, probabilistic reset
 *
 * @param system System handle
 * @param base_gain Baseline neural gain
 * @param modulated_gain Output: modulated gain
 * @param should_reset Output: whether to trigger network reset
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_neuromod_apply_ne_effects(
    cortical_neuromod_system_t* system,
    float base_gain,
    float* modulated_gain,
    bool* should_reset
);

/**
 * @brief Apply serotonin effects
 *
 * WHAT: Modulate processing based on 5-HT (inhibition, patience)
 * WHY:  Serotonin stabilizes and inhibits
 * HOW:  Boost inhibition, reduce impulsivity
 *
 * @param system System handle
 * @param base_inhibition Baseline inhibition strength
 * @param modulated_inhibition Output: modulated inhibition
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_neuromod_apply_serotonin_effects(
    const cortical_neuromod_system_t* system,
    float base_inhibition,
    float* modulated_inhibition
);

/**
 * @brief Get plasticity gate state
 *
 * WHAT: Determine if plasticity is currently enabled
 * WHY:  Plasticity should be gated by neuromodulators (primarily ACh)
 * HOW:  Return computed plasticity gate value
 *
 * @param system System handle
 * @return Plasticity gate [0.0, 1.0] or -1.0 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float cortical_neuromod_get_plasticity_gate(
    const cortical_neuromod_system_t* system
);

/**
 * @brief Modulate learning rate by neuromodulators
 *
 * WHAT: Apply all neuromodulatory effects to learning rate
 * WHY:  Three-factor learning rule (pre × post × DA)
 * HOW:  Multiply base LR by plasticity gate and DA modulation
 *
 * @param system System handle
 * @param base_learning_rate Baseline learning rate
 * @return Modulated learning rate or -1.0 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * ALGORITHM:
 * effective_lr = base_lr × plasticity_gate × (1.0 + da × da_plasticity_mod)
 */
float cortical_neuromod_modulate_plasticity(
    const cortical_neuromod_system_t* system,
    float base_learning_rate
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get neuromodulation statistics
 *
 * WHAT: Retrieve diagnostic and monitoring information
 * WHY:  Track system behavior and validate biological realism
 * HOW:  Copy internal statistics to output structure
 *
 * @param system System handle
 * @param stats Output statistics structure
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_neuromod_get_stats(
    const cortical_neuromod_system_t* system,
    cortical_neuromod_stats_t* stats
);

/**
 * @brief Reset neuromodulation system to baseline
 *
 * WHAT: Reset all levels to baseline values
 * WHY:  Return to neutral state between experiments
 * HOW:  Set all levels to 0.5, clear per-column DA, reset stats
 *
 * @param system System handle
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(N) where N = num_columns
 * THREAD-SAFE: Yes
 */
int cortical_neuromod_reset(cortical_neuromod_system_t* system);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with bio-async messaging system
 * WHY:  Enable inter-module neuromodulator communication
 * HOW:  Register as BIO_MODULE_CORTICAL_NEUROMOD (0x0150)
 *
 * @param system System handle
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_neuromod_connect_bio_async(cortical_neuromod_system_t* system);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async messaging
 * WHY:  Clean shutdown
 * HOW:  Deregister module context
 *
 * @param system System handle
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_neuromod_disconnect_bio_async(cortical_neuromod_system_t* system);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY:  Verify messaging is available
 * HOW:  Check bio_async_enabled flag
 *
 * @param system System handle
 * @return true if connected, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool cortical_neuromod_is_bio_async_connected(
    const cortical_neuromod_system_t* system
);

/**
 * @brief Connect to global neuromodulator system
 *
 * WHAT: Link cortical neuromod to global neuromodulator pool
 * WHY:  Enable coordination with brain-wide neuromodulation
 * HOW:  Store reference, sync levels periodically
 *
 * @param system Cortical neuromod system handle
 * @param global_system Global neuromodulator system handle
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_neuromod_connect_global_system(
    cortical_neuromod_system_t* system,
    neuromodulator_system_t global_system
);

/**
 * @brief Sync with global neuromodulator system
 *
 * WHAT: Update local levels from global neuromodulator pool
 * WHY:  Maintain consistency with brain-wide state
 * HOW:  Query global levels, update local levels
 *
 * @param system System handle
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_neuromod_sync_with_global(cortical_neuromod_system_t* system);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CORTICAL_NEUROMODULATION_H
