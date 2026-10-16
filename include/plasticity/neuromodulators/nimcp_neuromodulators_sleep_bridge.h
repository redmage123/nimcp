/**
 * @file nimcp_neuromodulators_sleep_bridge.h
 * @brief Sleep-Neuromodulator Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and neuromodulators
 * WHY:  Sleep states fundamentally alter neuromodulator profiles; neuromodulators
 *       can influence sleep onset and quality
 * HOW:  Sleep state modulates neuromodulator baselines and release dynamics
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → NEUROMODULATOR PATHWAYS:
 * ---------------------------------
 * 1. Acetylcholine (ACh) - "Sleep/Wake Switch":
 *    - AWAKE: High ACh (basal forebrain active) - attention, encoding
 *    - NREM: Very low ACh - allows consolidation without new encoding
 *    - REM: High ACh (dream state) - memory reactivation, vivid imagery
 *    - Reference: Hasselmo (1999) "Neuromodulation: ACh and memory consolidation"
 *
 * 2. Norepinephrine (NE) - "Vigilance System":
 *    - AWAKE: Moderate-high NE (locus coeruleus) - arousal, alertness
 *    - NREM: Very low NE - enables synaptic downscaling
 *    - REM: Near-zero NE (LC silent) - muscle atonia, dream state
 *    - Reference: Sara (2009) "LC and attention: A circuit reconsidered"
 *
 * 3. Dopamine (DA) - "Reward and Motivation":
 *    - AWAKE: Normal DA signaling - reward prediction, motivation
 *    - NREM: Reduced DA - no active reinforcement
 *    - REM: Variable DA bursts - may contribute to dream content
 *    - Reference: Dahan et al. (2007) "Dopamine neurons and sleep"
 *
 * 4. Serotonin (5-HT) - "Mood Regulation":
 *    - AWAKE: Moderate 5-HT - mood stability, behavioral inhibition
 *    - NREM: Low 5-HT - allows sleep maintenance
 *    - REM: Near-zero 5-HT (raphe silent) - enables REM characteristics
 *    - Reference: Monti (2011) "Serotonin control of sleep-wake behavior"
 *
 * NEUROMODULATOR → SLEEP PATHWAYS:
 * ---------------------------------
 * 1. High Norepinephrine Prevents Sleep:
 *    - Stress-induced insomnia
 *    - Fight-or-flight incompatible with sleep
 *    - High NE delays sleep onset
 *
 * 2. High Acetylcholine Promotes Wakefulness:
 *    - ACh activation keeps cortex alert
 *    - Can trigger REM from NREM
 *
 * 3. Adenosine (Sleep Pressure) Inhibits Wake Neuromodulators:
 *    - Accumulated adenosine suppresses NE, ACh
 *    - Promotes transition to NREM
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║            SLEEP-NEUROMODULATOR INTEGRATION BRIDGE                         ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              SLEEP → NEUROMODULATOR PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   SLEEP STATE      ACh     NE      DA      5-HT                    │  ║
 * ║   │   ─────────────────────────────────────────────────                │  ║
 * ║   │   AWAKE            1.0     1.0     1.0     1.0     (baseline)      │  ║
 * ║   │   DROWSY           0.7     0.6     0.9     1.0     (transitioning) │  ║
 * ║   │   LIGHT_NREM       0.3     0.3     0.5     0.6     (entering sleep)│  ║
 * ║   │   DEEP_NREM        0.1     0.1     0.3     0.4     (consolidation) │  ║
 * ║   │   REM              0.9     0.05    0.5     0.1     (dreaming)      │  ║
 * ║   │                                                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              NEUROMODULATOR → SLEEP PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   • High NE (>0.8) → Sleep pressure accumulation slowed            │  ║
 * ║   │   • High ACh (>0.8) → Can interrupt NREM → REM transition          │  ║
 * ║   │   • Low 5-HT → May facilitate REM onset                            │  ║
 * ║   │   • Sleep pressure modulates NE/ACh release sensitivity            │  ║
 * ║   │                                                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 * - NIMCP_LOGGING_* macros for logging
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMODULATORS_SLEEP_BRIDGE_H
#define NIMCP_NEUROMODULATORS_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

/* Thread utilities */
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Neuromodulator Profiles
 * ============================================================================ */

/* Acetylcholine modulation by sleep state (normalized to awake baseline) */
#define SLEEP_NEUROMOD_ACH_AWAKE        1.0f   /**< Full attention capacity */
#define SLEEP_NEUROMOD_ACH_DROWSY       0.7f   /**< Reduced attention */
#define SLEEP_NEUROMOD_ACH_LIGHT_NREM   0.3f   /**< Minimal encoding */
#define SLEEP_NEUROMOD_ACH_DEEP_NREM    0.1f   /**< Offline consolidation */
#define SLEEP_NEUROMOD_ACH_REM          0.9f   /**< Dream state reactivation */

/* Norepinephrine modulation by sleep state */
#define SLEEP_NEUROMOD_NE_AWAKE         1.0f   /**< Full arousal */
#define SLEEP_NEUROMOD_NE_DROWSY        0.6f   /**< Reducing vigilance */
#define SLEEP_NEUROMOD_NE_LIGHT_NREM    0.3f   /**< Low arousal */
#define SLEEP_NEUROMOD_NE_DEEP_NREM     0.1f   /**< Near silent LC */
#define SLEEP_NEUROMOD_NE_REM           0.05f  /**< LC completely silent */

/* Dopamine modulation by sleep state */
#define SLEEP_NEUROMOD_DA_AWAKE         1.0f   /**< Normal reward signaling */
#define SLEEP_NEUROMOD_DA_DROWSY        0.9f   /**< Slightly reduced */
#define SLEEP_NEUROMOD_DA_LIGHT_NREM    0.5f   /**< Reduced motivation */
#define SLEEP_NEUROMOD_DA_DEEP_NREM     0.3f   /**< Minimal DA */
#define SLEEP_NEUROMOD_DA_REM           0.5f   /**< Variable (dream content) */

/* Serotonin modulation by sleep state */
#define SLEEP_NEUROMOD_5HT_AWAKE        1.0f   /**< Normal mood regulation */
#define SLEEP_NEUROMOD_5HT_DROWSY       1.0f   /**< Maintained during transition */
#define SLEEP_NEUROMOD_5HT_LIGHT_NREM   0.6f   /**< Reducing */
#define SLEEP_NEUROMOD_5HT_DEEP_NREM    0.4f   /**< Low for sleep maintenance */
#define SLEEP_NEUROMOD_5HT_REM          0.1f   /**< Near silent raphe */

/* Sleep pressure effects on neuromodulator sensitivity */
#define SLEEP_PRESSURE_NE_SUPPRESSION   0.5f   /**< High pressure halves NE response */
#define SLEEP_PRESSURE_ACH_SUPPRESSION  0.4f   /**< High pressure reduces ACh */
#define SLEEP_PRESSURE_THRESHOLD        0.7f   /**< Pressure for suppression onset */

/* Neuromodulator thresholds affecting sleep */
#define NEUROMOD_NE_SLEEP_INHIBIT       0.8f   /**< High NE prevents sleep onset */
#define NEUROMOD_ACH_REM_TRIGGER        0.8f   /**< High ACh can trigger REM */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-neuromodulator bridge configuration
 */
typedef struct {
    bool enable_sleep_state_modulation;  /**< Enable sleep → neuromod effects */
    bool enable_pressure_effects;        /**< Enable sleep pressure → release */
    bool enable_neuromod_sleep_effects;  /**< Enable neuromod → sleep effects */

    /* Modulation strengths (0-1) */
    float ach_modulation_strength;       /**< ACh response to sleep state */
    float ne_modulation_strength;        /**< NE response to sleep state */
    float da_modulation_strength;        /**< DA response to sleep state */
    float serotonin_modulation_strength; /**< 5-HT response to sleep state */

    /* Sleep pressure sensitivity */
    float pressure_sensitivity;          /**< How much pressure affects release */
} neuromodulators_sleep_config_t;

/**
 * @brief Computed sleep effects on neuromodulators
 */
typedef struct {
    /* Current modulation factors (multiply with baseline) */
    float ach_factor;           /**< Acetylcholine modulation factor */
    float ne_factor;            /**< Norepinephrine modulation factor */
    float da_factor;            /**< Dopamine modulation factor */
    float serotonin_factor;     /**< Serotonin modulation factor */

    /* Release sensitivity modifiers */
    float ne_release_sensitivity;   /**< Sensitivity to threat/arousal stimuli */
    float ach_release_sensitivity;  /**< Sensitivity to salience stimuli */

    /* Sleep state info */
    sleep_state_t current_state;    /**< Current sleep state */
    float sleep_pressure;           /**< Current sleep pressure */

    /* Derived effects */
    float learning_rate_modifier;   /**< Combined effect on learning */
    float attention_modifier;       /**< Combined effect on attention */
    bool sleep_inhibited;           /**< High NE preventing sleep */
} neuromod_sleep_effects_t;

/**
 * @brief Sleep-neuromodulator integration bridge
 */
typedef struct neuromod_sleep_bridge_struct* neuromod_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-neuromodulator bridge configuration
 * WHY:  Provide sensible defaults based on biological evidence
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int neuromod_sleep_default_config(neuromodulators_sleep_config_t* config);

/**
 * WHAT: Create sleep-neuromodulator bridge
 * WHY:  Initialize integration between sleep and neuromodulator systems
 * HOW:  Allocate structure, store references, create mutex
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param neuromod_system Neuromodulator system to connect
 * @param sleep_system Sleep system to connect
 * @return New bridge or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
neuromod_sleep_bridge_t neuromod_sleep_bridge_create(
    const neuromodulators_sleep_config_t* config,
    neuromodulator_system_t neuromod_system,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-neuromodulator bridge
 * WHY:  Clean up resources, prevent memory leaks
 * HOW:  Free mutex, free structure (doesn't destroy connected systems)
 *
 * @param bridge Bridge to destroy
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (caller ensures no concurrent access)
 */
void neuromod_sleep_bridge_destroy(neuromod_sleep_bridge_t bridge);

/* ============================================================================
 * Update API (SLEEP → NEUROMODULATORS)
 * ============================================================================ */

/**
 * WHAT: Update neuromodulator effects from sleep system state
 * WHY:  Compute how current sleep state affects neuromodulator profiles
 * HOW:  Query sleep state and pressure, compute modulation factors
 *
 * ALGORITHM:
 * 1. Query sleep_get_current_state() for sleep stage
 * 2. Query sleep_get_pressure() for adenosine level
 * 3. Look up state-specific modulation factors
 * 4. Apply sleep pressure effects to release sensitivity
 * 5. Compute derived effects (learning, attention)
 * 6. Update neuromod_sleep_effects_t
 *
 * BIOLOGICAL BASIS:
 * - Sleep stages have characteristic neuromodulator profiles
 * - Sleep pressure (adenosine) suppresses arousal neuromodulators
 *
 * @param bridge Sleep-neuromodulator bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int neuromod_sleep_update(neuromod_sleep_bridge_t bridge);

/**
 * WHAT: Apply sleep-modulated neuromodulator levels
 * WHY:  Actually modify neuromodulator system based on sleep state
 * HOW:  Set neuromodulator levels using computed factors
 *
 * BIOLOGICAL BASIS:
 * - Sleep stages directly control neuromodulator release
 * - Locus coeruleus, raphe nuclei, basal forebrain state-dependent
 *
 * @param bridge Sleep-neuromodulator bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int neuromod_sleep_apply_modulation(neuromod_sleep_bridge_t bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * WHAT: Get current sleep effects on neuromodulators
 * WHY:  Query integrated sleep-neuromodulator state
 * HOW:  Copy neuromod_sleep_effects_t from bridge
 *
 * @param bridge Sleep-neuromodulator bridge
 * @param effects Output: sleep effects structure
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int neuromod_sleep_get_effects(
    const neuromod_sleep_bridge_t bridge,
    neuromod_sleep_effects_t* effects
);

/**
 * WHAT: Get modulation factor for specific neuromodulator
 * WHY:  Query single neuromodulator's sleep modulation
 * HOW:  Look up from current effects structure
 *
 * @param bridge Sleep-neuromodulator bridge
 * @param type Neuromodulator type
 * @return Modulation factor (0-1), or 1.0 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float neuromod_sleep_get_factor(
    const neuromod_sleep_bridge_t bridge,
    neuromodulator_type_t type
);

/**
 * WHAT: Check if high neuromodulators are inhibiting sleep
 * WHY:  Detect stress-induced insomnia conditions
 * HOW:  Check if NE or ACh above sleep inhibition threshold
 *
 * @param bridge Sleep-neuromodulator bridge
 * @return true if neuromodulators preventing sleep onset
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool neuromod_sleep_is_inhibited(const neuromod_sleep_bridge_t bridge);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Get acetylcholine factor for sleep state
 * WHY:  ACh varies dramatically across sleep stages
 * HOW:  Return predefined factor for each state
 *
 * @param state Current sleep state
 * @return ACh modulation factor [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (pure function)
 */
float neuromod_sleep_get_ach_factor(sleep_state_t state);

/**
 * WHAT: Get norepinephrine factor for sleep state
 * WHY:  NE drops dramatically during sleep, especially REM
 * HOW:  Return predefined factor for each state
 *
 * @param state Current sleep state
 * @return NE modulation factor [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (pure function)
 */
float neuromod_sleep_get_ne_factor(sleep_state_t state);

/**
 * WHAT: Get dopamine factor for sleep state
 * WHY:  DA shows moderate changes during sleep
 * HOW:  Return predefined factor for each state
 *
 * @param state Current sleep state
 * @return DA modulation factor [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (pure function)
 */
float neuromod_sleep_get_da_factor(sleep_state_t state);

/**
 * WHAT: Get serotonin factor for sleep state
 * WHY:  5-HT drops during REM (raphe silent)
 * HOW:  Return predefined factor for each state
 *
 * @param state Current sleep state
 * @return 5-HT modulation factor [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (pure function)
 */
float neuromod_sleep_get_serotonin_factor(sleep_state_t state);

/**
 * WHAT: Compute release sensitivity modifier from sleep pressure
 * WHY:  High sleep pressure suppresses arousal neuromodulator responses
 * HOW:  Reduce sensitivity when pressure > threshold
 *
 * FORMULA:
 *   if pressure < threshold: 1.0
 *   else: 1.0 - suppression * (pressure - threshold) / (1 - threshold)
 *
 * @param pressure Current sleep pressure [0-1]
 * @param threshold Pressure threshold for suppression onset
 * @param suppression Maximum suppression at full pressure
 * @return Release sensitivity modifier [suppression, 1.0]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (pure function)
 */
float neuromod_sleep_compute_release_sensitivity(
    float pressure,
    float threshold,
    float suppression
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMODULATORS_SLEEP_BRIDGE_H */
