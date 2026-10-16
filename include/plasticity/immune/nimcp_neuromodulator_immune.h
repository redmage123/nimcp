/**
 * @file nimcp_neuromodulator_immune.h
 * @brief Brain Immune-Neuromodulator Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and neuromodulator systems
 * WHY:  Models biological neuroimmune interactions that affect mood, cognition, and behavior
 * HOW:  Cytokines alter monoamine synthesis; neuromodulator imbalance triggers immune surveillance
 *
 * BIOLOGICAL BASIS:
 * ```
 * BIOLOGICAL PHENOMENON           NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────────
 * Cytokine-induced depression  → Pro-inflammatory cytokines ↓ DA/5-HT synthesis
 * Sickness behavior            → IL-1/IL-6/TNF-α → ↓ motivation, ↑ fatigue
 * Stress immunosuppression     → Chronic ↑ NE → immune suppression
 * Dopamine modulation of       → DA receptor activation on immune cells
 *   immune cells               → D2 activation → anti-inflammatory
 * Vagal anti-inflammatory      → ACh → ↓ cytokine release (cholinergic pathway)
 *   reflex
 * ```
 *
 * KEY CONCEPTS:
 *
 * 1. CYTOKINE → NEUROMODULATOR EFFECTS:
 *    - Pro-inflammatory (IL-1, IL-6, TNF-α):
 *      * ↓ Dopamine synthesis (reduce tyrosine hydroxylase activity)
 *      * ↓ Serotonin synthesis (reduce tryptophan hydroxylase)
 *      * ↑ Norepinephrine (stress response)
 *    - Anti-inflammatory (IL-10):
 *      * Restore normal synthesis rates
 *      * ↑ Serotonin (mood stabilization)
 *
 * 2. NEUROMODULATOR → IMMUNE EFFECTS:
 *    - Dopamine excess: Immune suppression (D2 receptors on T cells)
 *    - Serotonin deficiency: Inflammation escalation
 *    - Norepinephrine excess: Immune dysregulation
 *    - Acetylcholine: Anti-inflammatory via vagal pathway
 *
 * 3. CLINICAL RELEVANCE:
 *    - Depression in inflammatory conditions (rheumatoid arthritis, cancer)
 *    - Chronic fatigue syndrome (persistent immune activation)
 *    - Stress-related illness (cortisol-immune-neurotransmitter axis)
 *    - Therapeutic interventions (SSRIs have immune effects)
 *
 * ARCHITECTURE:
 * ```
 * ╔═════════════════════════════════════════════════════════════════════════╗
 * ║                    NEUROMODULATOR-IMMUNE INTEGRATION                     ║
 * ╠═════════════════════════════════════════════════════════════════════════╣
 * ║                                                                          ║
 * ║   ┌──────────────────────────────────────────────────────────────────┐  ║
 * ║   │              IMMUNE → NEUROMODULATOR PATHWAY                      │  ║
 * ║   │                                                                   │  ║
 * ║   │   ┌─────────────┐        ┌──────────────────┐                   │  ║
 * ║   │   │ Cytokines   │──────→ │ Synthesis        │                   │  ║
 * ║   │   │ (IL-1, IL-6 │        │ Modulation       │                   │  ║
 * ║   │   │  TNF-α)     │        │ • ↓ Tyrosine     │                   │  ║
 * ║   │   └─────────────┘        │   hydroxylase    │                   │  ║
 * ║   │                          │ • ↓ Tryptophan   │                   │  ║
 * ║   │                          │   hydroxylase    │                   │  ║
 * ║   │                          └────────┬─────────┘                   │  ║
 * ║   │                                   │                             │  ║
 * ║   │                                   ▼                             │  ║
 * ║   │                          ┌──────────────────┐                   │  ║
 * ║   │                          │ Monoamine Levels │                   │  ║
 * ║   │                          │ • ↓ Dopamine     │                   │  ║
 * ║   │                          │ • ↓ Serotonin    │                   │  ║
 * ║   │                          │ • ↑ Norepineph.  │                   │  ║
 * ║   │                          └──────────────────┘                   │  ║
 * ║   └──────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                          ║
 * ║   ┌──────────────────────────────────────────────────────────────────┐  ║
 * ║   │              NEUROMODULATOR → IMMUNE PATHWAY                      │  ║
 * ║   │                                                                   │  ║
 * ║   │   ┌─────────────┐        ┌──────────────────┐                   │  ║
 * ║   │   │ Imbalance   │──────→ │ Immune           │                   │  ║
 * ║   │   │ Detection   │        │ Surveillance     │                   │  ║
 * ║   │   │ • DA >> norm│        │ • Present as     │                   │  ║
 * ║   │   │ • 5-HT << n │        │   antigen        │                   │  ║
 * ║   │   │ • NE >> norm│        │ • Activate B/T   │                   │  ║
 * ║   │   └─────────────┘        │   cells          │                   │  ║
 * ║   │                          │ • Generate       │                   │  ║
 * ║   │                          │   response       │                   │  ║
 * ║   │                          └────────┬─────────┘                   │  ║
 * ║   │                                   │                             │  ║
 * ║   │                                   ▼                             │  ║
 * ║   │                          ┌──────────────────┐                   │  ║
 * ║   │                          │ Homeostatic      │                   │  ║
 * ║   │                          │ Correction       │                   │  ║
 * ║   │                          │ • Release        │                   │  ║
 * ║   │                          │   regulatory     │                   │  ║
 * ║   │                          │   cytokines      │                   │  ║
 * ║   │                          └──────────────────┘                   │  ║
 * ║   └──────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                          ║
 * ╚═════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DESIGN PATTERNS:
 * - Mediator: Coordinates immune-neuromodulator interactions
 * - Observer: Both systems observe each other's state
 * - Strategy: Pluggable response strategies for different imbalances
 * - Template Method: Update loop with pluggable cytokine effects
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

#ifndef NIMCP_NEUROMODULATOR_IMMUNE_H
#define NIMCP_NEUROMODULATOR_IMMUNE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/neuromodulators/nimcp_metabolic_pathways.h"
#include "plasticity/neuromodulators/nimcp_phasic_tonic.h"
#include "plasticity/neuromodulators/nimcp_receptor_subtypes.h"
#include "glial/microglia/nimcp_microglia.h"  /* For cytokine_type_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NEUROMOD_IMMUNE_MAX_HISTORY           100    /**< History buffer size */
#define NEUROMOD_IMMUNE_IMBALANCE_THRESHOLD   0.4f   /**< Threshold for imbalance detection */
#define NEUROMOD_IMMUNE_CYTOKINE_EFFECT_MAX   0.8f   /**< Max cytokine effect on synthesis */
#define NEUROMOD_IMMUNE_MODULE_NAME           "neuromod_immune"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct neuromod_immune_system neuromod_immune_system_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Neuromodulator imbalance types
 *
 * BIOLOGICAL BASIS:
 * Different imbalance patterns indicate different pathological states
 */
typedef enum {
    NEUROMOD_IMBALANCE_NONE = 0,        /**< Normal state */
    NEUROMOD_IMBALANCE_DA_EXCESS,       /**< Dopamine excess (mania, psychosis) */
    NEUROMOD_IMBALANCE_DA_DEFICIENCY,   /**< Dopamine deficiency (depression, Parkinson's) */
    NEUROMOD_IMBALANCE_5HT_EXCESS,      /**< Serotonin excess (serotonin syndrome) */
    NEUROMOD_IMBALANCE_5HT_DEFICIENCY,  /**< Serotonin deficiency (depression, anxiety) */
    NEUROMOD_IMBALANCE_NE_EXCESS,       /**< Norepinephrine excess (anxiety, PTSD) */
    NEUROMOD_IMBALANCE_NE_DEFICIENCY,   /**< Norepinephrine deficiency (fatigue, ADHD) */
    NEUROMOD_IMBALANCE_ACH_EXCESS,      /**< Acetylcholine excess (cholinergic toxicity) */
    NEUROMOD_IMBALANCE_ACH_DEFICIENCY,  /**< Acetylcholine deficiency (Alzheimer's) */
    NEUROMOD_IMBALANCE_COUNT
} neuromod_imbalance_type_t;

/**
 * @brief Cytokine effect types on neuromodulators
 */
typedef enum {
    CYTOKINE_EFFECT_NONE = 0,           /**< No effect */
    CYTOKINE_EFFECT_SUPPRESS_SYNTHESIS, /**< Reduce monoamine synthesis */
    CYTOKINE_EFFECT_ENHANCE_SYNTHESIS,  /**< Increase synthesis (IL-10) */
    CYTOKINE_EFFECT_INCREASE_CLEARANCE, /**< Faster degradation */
    CYTOKINE_EFFECT_BLOCK_RECEPTORS     /**< Reduce receptor sensitivity */
} cytokine_effect_type_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on neuromodulator synthesis
 *
 * Models how pro/anti-inflammatory cytokines alter monoamine production
 */
typedef struct {
    /* Synthesis rate modulation (0-1, 1=normal, <1=suppressed, >1=enhanced) */
    float dopamine_synthesis_multiplier;
    float serotonin_synthesis_multiplier;
    float norepinephrine_synthesis_multiplier;
    float acetylcholine_synthesis_multiplier;

    /* Precursor availability modulation */
    float tyrosine_availability;        /**< For dopamine/norepinephrine */
    float tryptophan_availability;      /**< For serotonin */
    float choline_availability;         /**< For acetylcholine */

    /* Enzyme activity modulation */
    float tyrosine_hydroxylase_activity;   /**< Rate-limiting for DA */
    float tryptophan_hydroxylase_activity; /**< Rate-limiting for 5-HT */
    float dopa_decarboxylase_activity;     /**< For DA/NE synthesis */

    /* Degradation rate modulation */
    float mao_activity;                 /**< Monoamine oxidase */
    float comt_activity;                /**< Catechol-O-methyltransferase */
    float ache_activity;                /**< Acetylcholinesterase */

    /* Statistics */
    uint64_t total_suppressions;
    uint64_t total_enhancements;
    float avg_suppression_magnitude;
} cytokine_neuromod_effects_t;

/**
 * @brief Neuromodulator imbalance state
 *
 * Tracks deviations from homeostatic baseline for immune surveillance
 */
typedef struct {
    uint32_t id;                        /**< Unique imbalance ID */
    neuromod_imbalance_type_t type;     /**< Type of imbalance */

    /* Deviation metrics */
    float dopamine_deviation;           /**< Deviation from baseline (-1 to +1) */
    float serotonin_deviation;
    float norepinephrine_deviation;
    float acetylcholine_deviation;

    /* Severity and duration */
    float severity;                     /**< Imbalance severity (0-1) */
    uint64_t onset_time;                /**< When imbalance started */
    uint64_t duration_ms;               /**< How long it has persisted */

    /* Immune response status */
    bool immune_alerted;                /**< Immune system notified? */
    uint32_t antigen_id;                /**< Linked immune antigen (if alerted) */
    bool corrective_action_taken;       /**< Correction attempted? */

    /* Biological markers */
    float inflammation_contribution;    /**< How much inflammation contributes */
    float stress_contribution;          /**< Stress component */
} neuromod_imbalance_t;

/**
 * @brief Neuromodulator-immune integration state
 *
 * Complete state tracking for bidirectional neuroimmune interactions
 */
struct neuromod_immune_system {
    /* Connected systems */
    brain_immune_system_t* immune_system;           /**< Brain immune system */
    neuromodulator_system_t neuromod_system;        /**< Neuromodulator system */

    /* Per-neuromodulator metabolic states */
    metabolic_state_t dopamine_metabolism;
    metabolic_state_t serotonin_metabolism;
    metabolic_state_t norepinephrine_metabolism;
    metabolic_state_t acetylcholine_metabolism;

    /* Phasic-tonic states */
    phasic_tonic_state_t dopamine_phasic;
    phasic_tonic_state_t serotonin_phasic;
    phasic_tonic_state_t norepinephrine_phasic;

    /* Cytokine effects */
    cytokine_neuromod_effects_t cytokine_effects;

    /* Imbalance tracking */
    neuromod_imbalance_t* imbalances;
    size_t imbalance_count;
    size_t imbalance_capacity;
    uint32_t next_imbalance_id;

    /* Homeostatic baselines */
    float dopamine_baseline;
    float serotonin_baseline;
    float norepinephrine_baseline;
    float acetylcholine_baseline;

    /* Thresholds */
    float imbalance_detection_threshold;    /**< Deviation threshold */
    float immune_alert_threshold;           /**< When to alert immune system */
    float cytokine_sensitivity;             /**< How much cytokines affect synthesis */

    /* Statistics */
    uint64_t total_imbalances_detected;
    uint64_t total_immune_alerts;
    uint64_t total_corrections;
    float avg_imbalance_duration_ms;

    /* State */
    bool running;
    uint64_t start_time;
    void* mutex;                            /**< Thread safety */
};

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Configuration for neuromodulator-immune integration
 */
typedef struct {
    /* Baseline levels */
    float dopamine_baseline;            /**< Default: 0.05 µM */
    float serotonin_baseline;           /**< Default: 0.03 µM */
    float norepinephrine_baseline;      /**< Default: 0.02 µM */
    float acetylcholine_baseline;       /**< Default: 0.1 µM */

    /* Imbalance detection */
    float imbalance_threshold;          /**< Deviation threshold (default: 0.4) */
    float immune_alert_threshold;       /**< Alert threshold (default: 0.6) */
    uint64_t min_duration_for_alert_ms; /**< Min persistent duration (default: 5000ms) */

    /* Cytokine effect parameters */
    float cytokine_sensitivity;         /**< Effect strength (default: 0.5) */
    float il1_effect_strength;          /**< IL-1 effect (default: 0.7) */
    float il6_effect_strength;          /**< IL-6 effect (default: 0.6) */
    float tnf_alpha_effect_strength;    /**< TNF-α effect (default: 0.8) */
    float il10_effect_strength;         /**< IL-10 effect (default: -0.5, restorative) */

    /* Homeostatic correction */
    bool enable_auto_correction;        /**< Auto-correct imbalances (default: true) */
    float correction_rate;              /**< Correction speed (default: 0.1) */
    uint64_t correction_delay_ms;       /**< Delay before correction (default: 10000ms) */

    /* Integration enables */
    bool enable_cytokine_effects;       /**< Enable cytokine→neuromod (default: true) */
    bool enable_imbalance_detection;    /**< Enable neuromod→immune (default: true) */
    bool enable_metabolic_modeling;     /**< Use detailed metabolism (default: true) */
} neuromod_immune_config_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with good defaults
 * HOW:  Return struct with balanced biological parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int neuromod_immune_default_config(neuromod_immune_config_t* config);

/**
 * @brief Create neuromodulator-immune integration system
 *
 * WHAT: Initialize bidirectional neuroimmune integration
 * WHY:  Model cytokine effects on monoamines and vice versa
 * HOW:  Allocate state, connect to immune and neuromodulator systems
 *
 * @param config Configuration (NULL for defaults)
 * @return New integration system or NULL on failure
 */
neuromod_immune_system_t* neuromod_immune_create(const neuromod_immune_config_t* config);

/**
 * @brief Destroy neuromodulator-immune integration system
 *
 * WHAT: Clean up integration system resources
 * WHY:  Proper resource deallocation
 * HOW:  Free state, disconnect from systems
 *
 * @param system System to destroy
 */
void neuromod_immune_destroy(neuromod_immune_system_t* system);

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Link integration to brain immune system
 * WHY:  Receive cytokine signals, send imbalance alerts
 * HOW:  Store reference, register callbacks
 *
 * @param system Integration system
 * @param immune_system Brain immune system
 * @return 0 on success
 */
int neuromod_immune_connect_immune(
    neuromod_immune_system_t* system,
    brain_immune_system_t* immune_system
);

/**
 * @brief Connect to neuromodulator system
 *
 * WHAT: Link integration to neuromodulator system
 * WHY:  Monitor levels, apply cytokine effects
 * HOW:  Store reference, access levels
 *
 * @param system Integration system
 * @param neuromod_system Neuromodulator system
 * @return 0 on success
 */
int neuromod_immune_connect_neuromod(
    neuromod_immune_system_t* system,
    neuromodulator_system_t neuromod_system
);

/* ============================================================================
 * Cytokine → Neuromodulator API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to neuromodulator synthesis
 *
 * WHAT: Model how pro-inflammatory cytokines suppress monoamine synthesis
 * WHY:  Cytokines induce sickness behavior and depression
 * HOW:  Reduce synthesis rates via enzyme modulation
 *
 * BIOLOGICAL BASIS:
 * - IL-1, IL-6, TNF-α reduce tyrosine hydroxylase activity
 * - Pro-inflammatory cytokines compete for tryptophan (kynurenine pathway)
 * - IL-10 restores normal synthesis rates
 *
 * @param system Integration system
 * @param cytokine_type Cytokine type (from microglia.h)
 * @param concentration Cytokine concentration (0-1)
 * @return 0 on success
 */
int neuromod_immune_apply_cytokine_effect(
    neuromod_immune_system_t* system,
    cytokine_type_t cytokine_type,
    float concentration
);

/**
 * @brief Apply pro-inflammatory cytokine effects
 *
 * WHAT: Suppress dopamine and serotonin synthesis
 * WHY:  Models cytokine-induced depression mechanism
 * HOW:  Reduce enzyme activity, decrease precursor availability
 *
 * EFFECTS:
 * - ↓ Tyrosine hydroxylase (30-50%)
 * - ↓ Tryptophan hydroxylase (40-60%)
 * - ↓ Precursor availability (20-40%)
 *
 * @param system Integration system
 * @param severity Inflammation severity (0-1)
 * @return 0 on success
 */
int neuromod_immune_apply_proinflammatory_effect(
    neuromod_immune_system_t* system,
    float severity
);

/**
 * @brief Apply anti-inflammatory cytokine effects
 *
 * WHAT: Restore normal neuromodulator synthesis
 * WHY:  IL-10 counteracts pro-inflammatory suppression
 * HOW:  Normalize enzyme activity, restore precursor levels
 *
 * @param system Integration system
 * @param il10_concentration IL-10 concentration (0-1)
 * @return 0 on success
 */
int neuromod_immune_apply_antiinflammatory_effect(
    neuromod_immune_system_t* system,
    float il10_concentration
);

/* ============================================================================
 * Neuromodulator → Immune API
 * ============================================================================ */

/**
 * @brief Detect neuromodulator imbalance
 *
 * WHAT: Check if neuromodulator levels deviate from homeostatic baselines
 * WHY:  Persistent imbalances indicate pathology requiring immune surveillance
 * HOW:  Compare current to baseline, compute deviation magnitude
 *
 * DETECTION CRITERIA:
 * - Deviation > threshold (default 0.4)
 * - Duration > min_duration (default 5 seconds)
 * - Severity calculated from deviation magnitude and duration
 *
 * @param system Integration system
 * @param imbalance_out Output: detected imbalance (NULL if none)
 * @return 0 if imbalance detected, -1 if none
 */
int neuromod_immune_detect_imbalance(
    neuromod_immune_system_t* system,
    neuromod_imbalance_t** imbalance_out
);

/**
 * @brief Alert immune system of neuromodulator imbalance
 *
 * WHAT: Present neuromodulator imbalance as antigen to immune system
 * WHY:  Trigger homeostatic immune response to correct imbalance
 * HOW:  Create antigen, activate immune cascade
 *
 * IMMUNE RESPONSE:
 * - Excess dopamine → cytokine modulation to suppress
 * - Deficient serotonin → anti-inflammatory response
 * - Excess norepinephrine → stress response mitigation
 *
 * @param system Integration system
 * @param imbalance Detected imbalance
 * @param antigen_id_out Output: created antigen ID
 * @return 0 on success
 */
int neuromod_immune_alert_imbalance(
    neuromod_immune_system_t* system,
    neuromod_imbalance_t* imbalance,
    uint32_t* antigen_id_out
);

/**
 * @brief Apply homeostatic correction to imbalance
 *
 * WHAT: Generate immune response to restore neuromodulator balance
 * WHY:  Maintain neurotransmitter homeostasis
 * HOW:  Release regulatory cytokines, adjust synthesis rates
 *
 * CORRECTION STRATEGIES:
 * - DA excess: Release anti-inflammatory cytokines
 * - 5-HT deficiency: Enhance synthesis, reduce degradation
 * - NE excess: Vagal activation (ACh anti-inflammatory pathway)
 *
 * @param system Integration system
 * @param imbalance Imbalance to correct
 * @return 0 on success
 */
int neuromod_immune_correct_imbalance(
    neuromod_immune_system_t* system,
    neuromod_imbalance_t* imbalance
);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Update neuromodulator-immune integration
 *
 * WHAT: Process cytokine effects, detect imbalances, apply corrections
 * WHY:  Advance neuroimmune state machine
 * HOW:
 *   1. Query immune system for cytokine levels
 *   2. Apply cytokine effects to neuromodulator synthesis
 *   3. Update metabolic states with modified synthesis rates
 *   4. Detect neuromodulator imbalances
 *   5. Alert immune system if needed
 *   6. Apply homeostatic corrections
 *
 * @param system Integration system
 * @param delta_ms Time since last update (milliseconds)
 * @return 0 on success
 */
int neuromod_immune_update(
    neuromod_immune_system_t* system,
    uint64_t delta_ms
);

/**
 * @brief Get current cytokine effects on neuromodulators
 *
 * @param system Integration system
 * @param effects_out Output: current cytokine effects
 * @return 0 on success
 */
int neuromod_immune_get_cytokine_effects(
    neuromod_immune_system_t* system,
    cytokine_neuromod_effects_t* effects_out
);

/**
 * @brief Get current neuromodulator imbalances
 *
 * @param system Integration system
 * @param imbalances_out Output array (allocated by caller)
 * @param count_in Max imbalances to return
 * @param count_out Actual imbalances returned
 * @return 0 on success
 */
int neuromod_immune_get_imbalances(
    neuromod_immune_system_t* system,
    neuromod_imbalance_t* imbalances_out,
    size_t count_in,
    size_t* count_out
);

/**
 * @brief Check if neuromodulator is within normal range
 *
 * @param system Integration system
 * @param type Neuromodulator type
 * @return true if within normal range
 */
bool neuromod_immune_is_balanced(
    neuromod_immune_system_t* system,
    neuromodulator_type_t type
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* neuromod_immune_imbalance_to_string(neuromod_imbalance_type_t type);
const char* neuromod_immune_cytokine_effect_to_string(cytokine_effect_type_t effect);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMODULATOR_IMMUNE_H */
