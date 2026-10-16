/**
 * @file nimcp_oscillations_immune_bridge.h
 * @brief Brain Oscillations-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and neural oscillations
 * WHY:  Biological evidence shows strong immune-oscillation coupling (cytokines alter
 *       brain waves, abnormal oscillations trigger immune surveillance). Essential for
 *       realistic sickness behavior and neuroimmune modeling.
 * HOW:  Cytokines shift power spectrum toward slow waves, suppress gamma/beta. Abnormal
 *       oscillation patterns trigger immune responses through antigen presentation.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → OSCILLATIONS PATHWAYS:
 * ------------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Increase slow-wave (delta) activity during infection/inflammation
 *    - Suppress gamma oscillations → impaired cognition, reduced attention
 *    - Disrupt theta-gamma coupling → memory consolidation deficits
 *    - Reduce beta power → decreased motor control, slowed thinking
 *    - Reference: Zielinski et al. (2019) "The neurobiology of cytokine-induced sleep"
 *    - Reference: Hodes et al. (2014) "Neuroimmune mechanisms of depression"
 *
 * 2. Sickness Behavior and Sleep-Like States:
 *    - IL-1β promotes delta oscillations (non-REM sleep patterns)
 *    - TNF-α enhances slow-wave sleep as part of recovery response
 *    - Inflammation → increased delta power even during wakefulness
 *    - Reference: Imeri & Opp (2009) "How (and why) cytokines make us sleepy"
 *
 * 3. Cognitive Impairment During Illness:
 *    - Systemic inflammation → reduced gamma power → attention deficits
 *    - IL-6 disrupts theta oscillations → working memory impairment
 *    - Loss of alpha rhythm → reduced relaxed wakefulness
 *    - Reference: Harrison et al. (2009) "Inflammation causes mood changes through EEG"
 *
 * 4. Anti-inflammatory Cytokines (IL-10):
 *    - Restore normal oscillation patterns during recovery
 *    - Gradual return of gamma/beta power
 *    - Normalization of theta-gamma coupling
 *    - Reference: Lasselin et al. (2016) "IL-10 and recovery from inflammation"
 *
 * OSCILLATIONS → IMMUNE PATHWAYS:
 * -------------------------------
 * 1. Abnormal Oscillation Patterns as Danger Signals:
 *    - Excessive delta during wakefulness → possible CNS infection
 *    - Suppressed gamma → neural dysfunction (potential pathology)
 *    - Loss of coherence → network breakdown (immune surveillance trigger)
 *    - Persistent desynchronization → system-level threat
 *    - Reference: Steriade (2006) "Abnormal EEG patterns in neurological disorders"
 *
 * 2. Seizure-Like Activity:
 *    - Abnormal synchronization → immune activation
 *    - Excessive gamma → excitotoxicity danger signal
 *    - Network hyperexcitability → inflammation trigger
 *    - Reference: Vezzani et al. (2011) "The role of inflammation in epilepsy"
 *
 * 3. Sleep Disruption:
 *    - Chronic sleep deprivation (low delta during sleep) → immune dysfunction
 *    - Lack of slow-wave sleep → increased inflammation
 *    - Reference: Besedovsky et al. (2012) "Sleep and immune function"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              BRAIN OSCILLATIONS-IMMUNE BRIDGE                              ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               IMMUNE → OSCILLATIONS PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → +60% │  ───────┐                                       │  ║
 * ║   │   │  delta power │         │                                       │  ║
 * ║   │   │ TNF-α → -70% │         ├──→ Power Spectrum Shift               │  ║
 * ║   │   │  gamma power │         │    (Delta ↑, Gamma/Beta ↓)           │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     OSCILLATION ANALYZER        │                             │  ║
 * ║   │   │  - Delta amplification          │                             │  ║
 * ║   │   │  - Gamma/Beta suppression       │                             │  ║
 * ║   │   │  - Theta-gamma decoupling       │                             │  ║
 * ║   │   │  - Coherence disruption         │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │  Restoration │     Recovery → Normal Oscillations              │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │             OSCILLATIONS → IMMUNE PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ EXCESSIVE    │ ──→ Infection Marker                            │  ║
 * ║   │   │ DELTA        │ ──→ Immune Surveillance                         │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ SUPPRESSED   │ ──→ Neural Dysfunction                          │  ║
 * ║   │   │ GAMMA        │ ──→ Immune Activation                           │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ LOW          │ ──→ Network Breakdown                           │  ║
 * ║   │   │ COHERENCE    │ ──→ Antigen Presentation                        │  ║
 * ║   │   └──────────────┘                                                 │  ║
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
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OSCILLATIONS_IMMUNE_BRIDGE_H
#define NIMCP_OSCILLATIONS_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "core/brain_oscillations/nimcp_brain_oscillations.h"
#include "cognitive/immune/nimcp_brain_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine oscillation impact factors */
#define CYTOKINE_IL1_DELTA_AMPLIFICATION     1.6f   /**< IL-1β → 60% delta increase */
#define CYTOKINE_IL1_GAMMA_SUPPRESSION       0.7f   /**< IL-1β → 30% gamma decrease */
#define CYTOKINE_IL6_DELTA_AMPLIFICATION     1.4f   /**< IL-6 → 40% delta increase */
#define CYTOKINE_IL6_BETA_SUPPRESSION        0.8f   /**< IL-6 → 20% beta decrease */
#define CYTOKINE_TNF_DELTA_AMPLIFICATION     1.8f   /**< TNF-α → 80% delta increase */
#define CYTOKINE_TNF_GAMMA_SUPPRESSION       0.5f   /**< TNF-α → 50% gamma decrease */
#define CYTOKINE_IFN_GAMMA_THETA_SUPPRESSION 0.75f  /**< IFN-γ → 25% theta decrease */
#define CYTOKINE_IL10_RESTORATION_RATE       0.9f   /**< IL-10 → 90% restoration toward normal */

/* Inflammation-oscillation mapping */
#define INFLAMMATION_DELTA_THRESHOLD         0.5f   /**< Delta > 50% total → infection marker */
#define INFLAMMATION_GAMMA_THRESHOLD         0.05f  /**< Gamma < 5% total → cognitive impairment */
#define INFLAMMATION_COHERENCE_THRESHOLD     0.3f   /**< Coherence < 0.3 → network dysfunction */
#define INFLAMMATION_SYNCHRONY_THRESHOLD     0.2f   /**< Synchrony < 0.2 → desynchronization */

/* Abnormality persistence for immune trigger */
#define ABNORMALITY_PERSISTENCE_THRESHOLD    3      /**< 3 consecutive abnormal readings → immune trigger */

/* Oscillation abnormality severity weights */
#define ABNORMALITY_WEIGHT_EXCESSIVE_DELTA   0.35f  /**< Excessive delta → high severity */
#define ABNORMALITY_WEIGHT_SUPPRESSED_GAMMA  0.30f  /**< Suppressed gamma → high severity */
#define ABNORMALITY_WEIGHT_LOW_COHERENCE     0.20f  /**< Low coherence → moderate severity */
#define ABNORMALITY_WEIGHT_LOW_SYNCHRONY     0.15f  /**< Low synchrony → moderate severity */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine oscillation effects
 *
 * Represents how cytokine levels modulate brain oscillation patterns
 */
typedef struct {
    /* Pro-inflammatory effects on power spectrum */
    float il1_delta_amplification;      /**< IL-1β induced delta increase */
    float il1_gamma_suppression;        /**< IL-1β induced gamma decrease */
    float il6_delta_amplification;      /**< IL-6 induced delta increase */
    float il6_beta_suppression;         /**< IL-6 induced beta decrease */
    float tnf_delta_amplification;      /**< TNF-α induced delta increase */
    float tnf_gamma_suppression;        /**< TNF-α induced gamma decrease */
    float ifn_gamma_theta_suppression;  /**< IFN-γ induced theta decrease */

    /* Anti-inflammatory effects */
    float il10_restoration;             /**< IL-10 restoration toward normal */

    /* Aggregate effects */
    float total_delta_amplification;    /**< Combined delta amplification [1.0-3.0] */
    float total_gamma_suppression;      /**< Combined gamma suppression [0.3-1.0] */
    float total_beta_suppression;       /**< Combined beta suppression [0.5-1.0] */
    float total_theta_suppression;      /**< Combined theta suppression [0.5-1.0] */

    /* Network effects */
    float coherence_disruption;         /**< Network coherence reduction [0.0-1.0] */
    float synchrony_disruption;         /**< Synchrony reduction [0.0-1.0] */
    float theta_gamma_decoupling;       /**< Theta-gamma PAC disruption [0.0-1.0] */
} cytokine_oscillation_effects_t;

/**
 * @brief Inflammation oscillation state
 *
 * How inflammation level affects oscillatory patterns
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_intensity;       /**< Normalized inflammation [0-1] */
    float inflammation_duration_sec;    /**< How long inflamed */

    /* Power spectrum shifts */
    float delta_power_shift;            /**< Delta power change [0-2] (multiplier) */
    float theta_power_shift;            /**< Theta power change [0-2] */
    float alpha_power_shift;            /**< Alpha power change [0-2] */
    float beta_power_shift;             /**< Beta power change [0-2] */
    float gamma_power_shift;            /**< Gamma power change [0-2] */

    /* Network disruption */
    float coherence_reduction;          /**< Coherence loss [0-1] */
    float synchrony_reduction;          /**< Synchrony loss [0-1] */

    /* Cognitive state impact */
    cognitive_state_t expected_state;   /**< Expected state under inflammation */
    float state_shift_severity;         /**< How much inflammation shifts state */
} inflammation_oscillation_state_t;

/**
 * @brief Oscillation abnormality immune trigger
 *
 * How abnormal oscillations trigger immune responses
 */
typedef struct {
    /* Abnormality indicators */
    bool excessive_delta;               /**< Delta > 50% total power */
    bool suppressed_gamma;              /**< Gamma < 5% total power */
    bool low_coherence;                 /**< Coherence < 0.3 */
    bool low_synchrony;                 /**< Synchrony < 0.2 */

    /* Persistence tracking */
    uint32_t consecutive_abnormal;      /**< Consecutive abnormal readings */
    float abnormality_score;            /**< Overall abnormality [0-1] */

    /* Immune triggers */
    bool immune_surveillance_triggered; /**< Immune system alerted */
    bool antigen_presented;             /**< Antigen created for abnormality */
    uint32_t antigen_id;                /**< ID of presented antigen */

    /* Severity assessment */
    uint32_t immune_severity;           /**< Severity for immune system [1-10] */
} oscillation_immune_trigger_t;

/**
 * @brief Normal oscillation baseline
 *
 * Reference state before immune modulation for restoration
 */
typedef struct {
    brain_wave_power_t baseline_power;  /**< Normal power distribution */
    cognitive_state_t baseline_state;   /**< Normal cognitive state */
    float baseline_coherence;           /**< Normal coherence */
    float baseline_synchrony;           /**< Normal synchrony */
    float baseline_theta_gamma_pac;     /**< Normal theta-gamma coupling */

    bool baseline_established;          /**< Has baseline been set */
    uint64_t baseline_timestamp;        /**< When baseline was established */
} oscillation_baseline_t;

/**
 * @brief Complete oscillations-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_oscillation_analyzer_t* oscillation_analyzer;
    brain_immune_system_t* immune_system;

    /* Current state */
    cytokine_oscillation_effects_t cytokine_effects;
    inflammation_oscillation_state_t inflammation_state;
    oscillation_immune_trigger_t immune_trigger;
    oscillation_baseline_t baseline;

    /* Integration flags */
    bool enable_cytokine_oscillation_modulation;
    bool enable_inflammation_power_shift;
    bool enable_oscillation_immune_trigger;
    bool enable_abnormality_surveillance;
    bool enable_il10_restoration;

    /* Sensitivity tuning */
    float cytokine_sensitivity;         /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;     /**< Inflammation effect multiplier [0.5-2.0] */
    float abnormality_sensitivity;      /**< Abnormality trigger sensitivity [0.5-2.0] */

    /* Thresholds */
    float excessive_delta_threshold;    /**< Delta threshold for abnormality [0.4-0.7] */
    float suppressed_gamma_threshold;   /**< Gamma threshold for abnormality [0.03-0.1] */
    uint32_t persistence_threshold;     /**< Consecutive abnormal for trigger [2-5] */

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t power_spectrum_shifts;
    uint32_t immune_triggers;
    uint32_t antigens_presented;
    uint32_t il10_restorations;

    void* bio_ctx;                      /**< Bio-async module context */} oscillations_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_oscillation_modulation;
    bool enable_inflammation_power_shift;
    bool enable_oscillation_immune_trigger;
    bool enable_abnormality_surveillance;
    bool enable_il10_restoration;

    /* Sensitivity tuning */
    float cytokine_sensitivity;         /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;     /**< Inflammation effect multiplier [0.5-2.0] */
    float abnormality_sensitivity;      /**< Abnormality sensitivity [0.5-2.0] */

    /* Thresholds */
    float excessive_delta_threshold;    /**< Delta threshold [0.4-0.7] */
    float suppressed_gamma_threshold;   /**< Gamma threshold [0.03-0.1] */
    uint32_t persistence_threshold;     /**< Consecutive abnormal [2-5] */
} oscillations_immune_config_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int oscillations_immune_default_config(oscillations_immune_config_t* config);

/**
 * @brief Create oscillations-immune bridge
 *
 * WHAT: Initialize bidirectional oscillations-immune integration
 * WHY:  Enable realistic immune-oscillation coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param oscillation_analyzer Brain oscillation analyzer
 * @param immune_system Brain immune system
 * @return New bridge or NULL on failure
 */
oscillations_immune_bridge_t* oscillations_immune_bridge_create(
    const oscillations_immune_config_t* config,
    brain_oscillation_analyzer_t* oscillation_analyzer,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy oscillations-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void oscillations_immune_bridge_destroy(oscillations_immune_bridge_t* bridge);

/**
 * @brief Establish oscillation baseline
 *
 * WHAT: Capture current oscillation state as normal baseline
 * WHY:  Need reference for IL-10 restoration and abnormality detection
 * HOW:  Sample current power, coherence, synchrony, store as baseline
 *
 * @param bridge Oscillations-immune bridge
 * @return 0 on success
 */
int oscillations_immune_establish_baseline(oscillations_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Oscillations API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to oscillations
 *
 * WHAT: Modulate oscillation patterns based on cytokine levels
 * WHY:  Pro-inflammatory cytokines increase delta, suppress gamma/beta
 * HOW:  Query immune system cytokines, apply power spectrum shifts
 *
 * @param bridge Oscillations-immune bridge
 * @return 0 on success
 */
int oscillations_immune_apply_cytokine_effects(oscillations_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to oscillations
 *
 * WHAT: Shift power spectrum based on inflammation level
 * WHY:  Systemic inflammation causes sickness behavior (delta increase)
 * HOW:  Map inflammation level to power shifts, apply to analyzer
 *
 * @param bridge Oscillations-immune bridge
 * @return 0 on success
 */
int oscillations_immune_apply_inflammation_effects(oscillations_immune_bridge_t* bridge);

/**
 * @brief Restore oscillations with IL-10
 *
 * WHAT: Gradually restore normal oscillations during recovery
 * WHY:  IL-10 (anti-inflammatory) normalizes brain waves
 * HOW:  Interpolate current state toward baseline
 *
 * @param bridge Oscillations-immune bridge
 * @param il10_concentration IL-10 level [0-1]
 * @return 0 on success
 */
int oscillations_immune_restore_with_il10(
    oscillations_immune_bridge_t* bridge,
    float il10_concentration
);

/**
 * @brief Compute expected cognitive state shift
 *
 * WHAT: Calculate how inflammation should alter cognitive state
 * WHY:  Inflammation shifts state (e.g., focused → drowsy)
 * HOW:  Map inflammation level to expected state change
 *
 * @param bridge Oscillations-immune bridge
 * @return Expected cognitive state under inflammation
 */
cognitive_state_t oscillations_immune_compute_state_shift(
    const oscillations_immune_bridge_t* bridge
);

/* ============================================================================
 * Oscillations → Immune API
 * ============================================================================ */

/**
 * @brief Detect oscillation abnormalities
 *
 * WHAT: Check for pathological oscillation patterns
 * WHY:  Abnormal patterns indicate potential neural dysfunction
 * HOW:  Check delta, gamma, coherence, synchrony against thresholds
 *
 * @param bridge Oscillations-immune bridge
 * @return true if abnormalities detected
 */
bool oscillations_immune_detect_abnormality(oscillations_immune_bridge_t* bridge);

/**
 * @brief Trigger immune response from abnormality
 *
 * WHAT: Present abnormal oscillation pattern as antigen to immune system
 * WHY:  Persistent abnormalities may indicate CNS pathology
 * HOW:  Create epitope from oscillation signature, present to immune
 *
 * @param bridge Oscillations-immune bridge
 * @return 0 on success
 */
int oscillations_immune_trigger_from_abnormality(oscillations_immune_bridge_t* bridge);

/**
 * @brief Compute abnormality severity score
 *
 * WHAT: Calculate weighted abnormality score for immune severity
 * WHY:  Different abnormalities have different severity implications
 * HOW:  Weighted sum: delta (35%), gamma (30%), coherence (20%), synchrony (15%)
 *
 * @param bridge Oscillations-immune bridge
 * @return Abnormality score [0-1]
 */
float oscillations_immune_compute_abnormality_score(
    const oscillations_immune_bridge_t* bridge
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update oscillations-immune bridge (both directions)
 *
 * WHAT: Process all immune-oscillation interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine/inflammation effects, detect abnormalities, trigger immune
 *
 * @param bridge Oscillations-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int oscillations_immune_bridge_update(
    oscillations_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine oscillation effects
 *
 * @param bridge Oscillations-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int oscillations_immune_get_cytokine_effects(
    const oscillations_immune_bridge_t* bridge,
    cytokine_oscillation_effects_t* effects
);

/**
 * @brief Get current inflammation oscillation state
 *
 * @param bridge Oscillations-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int oscillations_immune_get_inflammation_state(
    const oscillations_immune_bridge_t* bridge,
    inflammation_oscillation_state_t* state
);

/**
 * @brief Get immune trigger state
 *
 * @param bridge Oscillations-immune bridge
 * @param trigger Output trigger structure
 * @return 0 on success
 */
int oscillations_immune_get_trigger_state(
    const oscillations_immune_bridge_t* bridge,
    oscillation_immune_trigger_t* trigger
);

/**
 * @brief Check if oscillations are under immune modulation
 *
 * WHAT: Determine if cytokines are currently affecting oscillations
 * WHY:  Distinguish normal vs inflammation-altered states
 * HOW:  Check if any cytokine effects are active
 *
 * @param bridge Oscillations-immune bridge
 * @return true if immune system is modulating oscillations
 */
bool oscillations_immune_is_modulated(const oscillations_immune_bridge_t* bridge);

/**
 * @brief Get delta amplification factor
 *
 * @param bridge Oscillations-immune bridge
 * @return Delta amplification [1.0-3.0]
 */
float oscillations_immune_get_delta_amplification(
    const oscillations_immune_bridge_t* bridge
);

/**
 * @brief Get gamma suppression factor
 *
 * @param bridge Oscillations-immune bridge
 * @return Gamma suppression [0.3-1.0]
 */
float oscillations_immune_get_gamma_suppression(
    const oscillations_immune_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed oscillation/immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_OSCILLATIONS
 *
 * @param bridge Oscillations-immune bridge
 * @return 0 on success, -1 on error
 */
int oscillations_immune_connect_bio_async(oscillations_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Oscillations-immune bridge
 * @return 0 on success, -1 on error
 */
int oscillations_immune_disconnect_bio_async(oscillations_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY:  Determine if messaging is available
 * HOW:  Return bio_async_enabled flag
 *
 * @param bridge Oscillations-immune bridge
 * @return true if connected to bio-async router
 */
bool oscillations_immune_is_bio_async_connected(const oscillations_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OSCILLATIONS_IMMUNE_BRIDGE_H */
