/**
 * @file nimcp_introspection_immune_bridge.h
 * @brief Introspection-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and introspection/metacognition
 * WHY:  Biological evidence shows inflammation impairs metacognitive accuracy and self-awareness.
 *       Essential for realistic modeling of sickness effects on consciousness.
 * HOW:  Cytokines reduce introspective accuracy and consciousness metrics (Phi),
 *       introspection detects sickness state and triggers self-awareness responses.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → INTROSPECTION PATHWAYS:
 * -------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Impair prefrontal cortex function → reduced metacognitive accuracy
 *    - Reduce integrated information (Φ) → lower consciousness clarity
 *    - Impair temporal pattern detection → difficulty tracking mental states
 *    - Increase epistemic uncertainty → reduced confidence in self-knowledge
 *    - Reference: Harrison et al. (2015) "Inflammation causes mood changes through alterations in subgenual cingulate activity and mesolimbic connectivity"
 *
 * 2. Chronic Inflammation:
 *    - Sustained reduction in self-awareness and introspective ability
 *    - Difficulty monitoring internal states
 *    - Reduced metacognitive sensitivity
 *    - Impaired consciousness state transitions
 *    - Reference: Capuron & Miller (2011) "Immune system to brain signaling: neuropsychopharmacological implications"
 *
 * 3. IL-6 Effects on Prefrontal Metacognitive Circuits:
 *    - IL-6 specifically impairs dorsolateral prefrontal cortex
 *    - Reduces metacognitive monitoring capacity
 *    - Impairs error detection and self-correction
 *    - Reference: Brydon et al. (2008) "Peripheral inflammation is associated with altered substantia nigra activity and psychomotor slowing"
 *
 * 4. Consciousness Clarity Reduction:
 *    - Systemic inflammation reduces consciousness levels
 *    - Cytokine storms can cause delirium (severe consciousness impairment)
 *    - Fever reduces cognitive clarity and self-awareness
 *    - Reference: Wilson et al. (2020) "Cytokine profile in plasma of severe COVID-19 does not differ from ARDS and sepsis"
 *
 * INTROSPECTION → IMMUNE PATHWAYS:
 * -------------------------
 * 1. Self-Awareness of Sickness State:
 *    - Introspection detects abnormal internal state
 *    - Metacognitive monitoring recognizes cognitive impairment
 *    - Conscious detection of reduced Phi or uncertainty triggers awareness
 *    - Reference: Parvizi & Damasio (2001) "Consciousness and the brainstem"
 *
 * 2. Pattern Detection of Immune Activity:
 *    - Temporal pattern detection can identify immune response signatures
 *    - Recurring inflammatory patterns become learned
 *    - Enhanced metacognitive awareness of immune state
 *    - Reference: Craig (2002) "How do you feel? Interoception: the sense of the physiological condition of the body"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                 INTROSPECTION-IMMUNE BRIDGE                                ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              IMMUNE → INTROSPECTION PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -0.3 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -0.4 │         │                                       │  ║
 * ║   │   │ TNF-α → -0.2 │         ├──→ Metacognitive Impairment           │  ║
 * ║   │   │              │         │    (Reduced accuracy, clarity)        │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │    INTROSPECTION SYSTEM         │                             │  ║
 * ║   │   │  - Phi (Φ) reduction            │                             │  ║
 * ║   │   │  - Uncertainty increase         │                             │  ║
 * ║   │   │  - Pattern detection impairment │                             │  ║
 * ║   │   │  - Consciousness clarity ↓      │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │   +0.2       │     Recovery, Clarity Restoration               │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │            INTROSPECTION → IMMUNE PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ PHI DROP     │ ──→ Sickness State Detection                    │  ║
 * ║   │   │ HIGH UNCERT. │ ──→ Cognitive Impairment Awareness              │  ║
 * ║   │   │ PATTERN LOSS │ ──→ Metacognitive Alert                         │  ║
 * ║   │   └──────────────┘                                                 │  ║
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
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_INTROSPECTION_IMMUNE_BRIDGE_H
#define NIMCP_INTROSPECTION_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/introspection/nimcp_introspection.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine introspection impact factors (metacognitive accuracy modulation) */
#define CYTOKINE_IL1_INTROSPECTION_IMPACT     -0.3f   /**< IL-1β → metacognitive impairment */
#define CYTOKINE_IL6_INTROSPECTION_IMPACT     -0.4f   /**< IL-6 → strong metacognitive impairment */
#define CYTOKINE_TNF_INTROSPECTION_IMPACT     -0.2f   /**< TNF-α → mild metacognitive impairment */
#define CYTOKINE_IFN_GAMMA_INTROSPECTION_IMPACT -0.15f /**< IFN-γ → slight impairment */
#define CYTOKINE_IL10_INTROSPECTION_IMPACT     0.2f   /**< IL-10 → recovery/clarity restoration */

/* Inflammation consciousness reduction mapping */
#define INFLAMMATION_PHI_REDUCTION_THRESHOLD  0.5f   /**< Inflammation level for Phi reduction onset */
#define INFLAMMATION_PHI_REDUCTION_MAX        0.7f   /**< Maximum Phi reduction from inflammation */

/* Consciousness impairment thresholds */
#define PHI_SICKNESS_DETECTION_THRESHOLD      0.3f   /**< Phi drop indicating sickness */
#define UNCERTAINTY_SICKNESS_THRESHOLD        0.7f   /**< Uncertainty level indicating sickness */

/* Chronic inflammation metacognitive duration (seconds) */
#define CHRONIC_METACOGNITIVE_IMPAIRMENT_THRESHOLD (86400.0f * 3)  /**< 3 days = chronic */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine introspection effects
 *
 * Represents how cytokine levels impair metacognitive accuracy
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_accuracy_impairment;        /**< IL-1β induced metacognitive error */
    float il6_accuracy_impairment;        /**< IL-6 induced metacognitive error */
    float tnf_accuracy_impairment;        /**< TNF-α induced metacognitive error */
    float ifn_gamma_accuracy_impairment;  /**< IFN-γ induced metacognitive error */

    /* Anti-inflammatory effects */
    float il10_clarity_restoration;       /**< IL-10 recovery/clarity effect */

    /* Aggregate effects */
    float total_accuracy_shift;           /**< Combined metacognitive modulation */
    float consciousness_impairment_level; /**< Overall consciousness impairment [0-1] */
    float phi_reduction;                  /**< Integrated information reduction [0-1] */
    float uncertainty_increase;           /**< Epistemic uncertainty increase [0-1] */
} cytokine_introspection_effects_t;

/**
 * @brief Inflammation consciousness state
 *
 * How chronic inflammation affects consciousness metrics and self-awareness
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;      /**< How long inflamed */
    bool is_chronic;                      /**< >= 3 days */

    /* Consciousness impacts */
    float phi_reduction;                  /**< Integrated information reduction [0-1] */
    float metacognitive_accuracy_loss;    /**< Reduced self-monitoring accuracy [0-1] */
    float pattern_detection_impairment;   /**< Temporal pattern recognition loss [0-1] */
    float state_clarity_loss;             /**< Internal state clarity loss [0-1] */

    /* Uncertainty effects */
    float epistemic_uncertainty_increase; /**< Increased model uncertainty */
    float confidence_reduction;           /**< Reduced confidence in self-knowledge */
} inflammation_consciousness_state_t;

/**
 * @brief Introspection sickness detection
 *
 * How introspection detects and reports sickness state
 */
typedef struct {
    /* Detection indicators */
    float phi_baseline;                   /**< Normal Phi value */
    float phi_current;                    /**< Current Phi value */
    float phi_drop;                       /**< Drop from baseline [0-1] */

    /* Uncertainty indicators */
    float uncertainty_baseline;           /**< Normal uncertainty */
    float uncertainty_current;            /**< Current uncertainty */
    float uncertainty_increase;           /**< Increase from baseline [0-1] */

    /* Pattern indicators */
    uint32_t pattern_disruptions;         /**< Temporal pattern disruptions */
    float pattern_accuracy_loss;          /**< Pattern detection accuracy loss */

    /* Sickness awareness */
    bool sickness_detected;               /**< Metacognitive sickness awareness */
    float sickness_confidence;            /**< Confidence in sickness detection [0-1] */
    uint64_t detection_time;              /**< When sickness was detected */
} introspection_sickness_detection_t;

/**
 * @brief Complete introspection-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    introspection_context_t introspection_context;

    /* Current state */
    cytokine_introspection_effects_t cytokine_effects;
    inflammation_consciousness_state_t consciousness_state;
    introspection_sickness_detection_t sickness_detection;

    /* Baseline metrics (for comparison) */
    float baseline_phi;
    float baseline_uncertainty;
    uint32_t baseline_pattern_count;

    /* Integration flags */
    bool enable_cytokine_introspection_modulation;
    bool enable_inflammation_phi_reduction;
    bool enable_sickness_detection;
    bool enable_pattern_immune_correlation;
    bool enable_uncertainty_immune_coupling;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t sickness_detections;
    uint32_t phi_reductions;
    uint32_t uncertainty_increases;
    } introspection_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_introspection_modulation;
    bool enable_inflammation_phi_reduction;
    bool enable_sickness_detection;
    bool enable_pattern_immune_correlation;
    bool enable_uncertainty_immune_coupling;

    /* Sensitivity tuning */
    float cytokine_sensitivity;           /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;       /**< Inflammation effect multiplier [0.5-2.0] */
    float sickness_detection_sensitivity; /**< Detection sensitivity [0.5-2.0] */

    /* Thresholds */
    float phi_sickness_threshold;         /**< Phi drop for sickness detection [0.2-0.5] */
    float uncertainty_sickness_threshold; /**< Uncertainty for sickness detection [0.5-0.9] */
} introspection_immune_config_t;

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
int introspection_immune_default_config(introspection_immune_config_t* config);

/**
 * @brief Create introspection-immune bridge
 *
 * WHAT: Initialize bidirectional introspection-immune integration
 * WHY:  Enable realistic immune-consciousness coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param introspection_context Introspection context
 * @return New bridge or NULL on failure
 */
introspection_immune_bridge_t* introspection_immune_bridge_create(
    const introspection_immune_config_t* config,
    brain_immune_system_t* immune_system,
    introspection_context_t introspection_context
);

/**
 * @brief Destroy introspection-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void introspection_immune_bridge_destroy(introspection_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Introspection API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to introspection
 *
 * WHAT: Impair metacognitive accuracy based on cytokine levels
 * WHY:  Pro-inflammatory cytokines reduce metacognitive monitoring
 * HOW:  Query immune system cytokines, reduce introspection accuracy/clarity
 *
 * @param bridge Introspection-immune bridge
 * @return 0 on success
 */
int introspection_immune_apply_cytokine_effects(introspection_immune_bridge_t* bridge);

/**
 * @brief Apply chronic inflammation to consciousness metrics
 *
 * WHAT: Reduce Phi and consciousness clarity from prolonged inflammation
 * WHY:  Chronic inflammation causes sustained consciousness impairment
 * HOW:  Check inflammation duration/level, reduce Phi and state clarity
 *
 * @param bridge Introspection-immune bridge
 * @return 0 on success
 */
int introspection_immune_apply_inflammation_effects(introspection_immune_bridge_t* bridge);

/**
 * @brief Compute Phi reduction from inflammation
 *
 * WHAT: Calculate consciousness level reduction from immune state
 * WHY:  Inflammation reduces integrated information
 * HOW:  Map inflammation level/duration to Phi reduction [0-1]
 *
 * @param bridge Introspection-immune bridge
 * @return Phi reduction factor [0-1]
 */
float introspection_immune_compute_phi_reduction(const introspection_immune_bridge_t* bridge);

/**
 * @brief Compute epistemic uncertainty increase from inflammation
 *
 * WHAT: Calculate increased self-knowledge uncertainty from immune state
 * WHY:  Inflammation impairs metacognitive confidence
 * HOW:  Map inflammation to uncertainty increase [0-1]
 *
 * @param bridge Introspection-immune bridge
 * @return Uncertainty increase factor [0-1]
 */
float introspection_immune_compute_uncertainty_increase(const introspection_immune_bridge_t* bridge);

/* ============================================================================
 * Introspection → Immune API
 * ============================================================================ */

/**
 * @brief Detect sickness state from introspection metrics
 *
 * WHAT: Use metacognitive monitoring to detect sickness
 * WHY:  Introspection can provide self-awareness of immune state
 * HOW:  Compare Phi/uncertainty/patterns to baseline, detect anomalies
 *
 * @param bridge Introspection-immune bridge
 * @return 0 on success
 */
int introspection_immune_detect_sickness(introspection_immune_bridge_t* bridge);

/**
 * @brief Correlate temporal patterns with immune events
 *
 * WHAT: Link temporal pattern changes to immune activity
 * WHY:  Pattern disruptions may indicate immune responses
 * HOW:  Compare pattern library changes with immune phase
 *
 * @param bridge Introspection-immune bridge
 * @return 0 on success
 */
int introspection_immune_correlate_patterns(introspection_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update introspection-immune bridge (both directions)
 *
 * WHAT: Process all introspection-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, detect sickness, correlate patterns
 *
 * @param bridge Introspection-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int introspection_immune_bridge_update(
    introspection_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine introspection effects
 *
 * @param bridge Introspection-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int introspection_immune_get_cytokine_effects(
    const introspection_immune_bridge_t* bridge,
    cytokine_introspection_effects_t* effects
);

/**
 * @brief Get current inflammation consciousness state
 *
 * @param bridge Introspection-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int introspection_immune_get_consciousness_state(
    const introspection_immune_bridge_t* bridge,
    inflammation_consciousness_state_t* state
);

/**
 * @brief Check if sickness is detected via introspection
 *
 * WHAT: Determine if metacognitive monitoring detected sickness
 * WHY:  Sickness detection is a key self-awareness capability
 * HOW:  Check sickness detection flags and confidence
 *
 * @param bridge Introspection-immune bridge
 * @return true if sickness detected
 */
bool introspection_immune_is_sickness_detected(const introspection_immune_bridge_t* bridge);

/**
 * @brief Get current Phi reduction severity
 *
 * @param bridge Introspection-immune bridge
 * @return Phi reduction [0-1]
 */
float introspection_immune_get_phi_reduction(const introspection_immune_bridge_t* bridge);

/**
 * @brief Get metacognitive accuracy loss
 *
 * @param bridge Introspection-immune bridge
 * @return Accuracy loss [0-1]
 */
float introspection_immune_get_accuracy_loss(const introspection_immune_bridge_t* bridge);

/**
 * @brief Set baseline consciousness metrics
 *
 * WHAT: Capture baseline Phi/uncertainty for comparison
 * WHY:  Need reference point to detect immune-induced changes
 * HOW:  Query current introspection metrics, store as baseline
 *
 * @param bridge Introspection-immune bridge
 * @return 0 on success
 */
int introspection_immune_set_baseline(introspection_immune_bridge_t* bridge);

/**
 * @brief Reset sickness detection state
 *
 * WHAT: Clear sickness detection flags after recovery
 * WHY:  Allow re-detection after inflammation resolution
 * HOW:  Reset detection state, maintain baselines
 *
 * @param bridge Introspection-immune bridge
 * @return 0 on success
 */
int introspection_immune_reset_sickness_detection(introspection_immune_bridge_t* bridge);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_INTROSPECTION
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int introspection_immune_connect_bio_async(introspection_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int introspection_immune_disconnect_bio_async(introspection_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool introspection_immune_is_bio_async_connected(const introspection_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTROSPECTION_IMMUNE_BRIDGE_H */
