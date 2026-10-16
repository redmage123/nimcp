/**
 * @file nimcp_visual_immune_bridge.h
 * @brief Visual Cortex-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and visual cortex processing
 * WHY:  Biological evidence shows immune-visual coupling (cytokines impair vision,
 *       visual threats trigger immunity). Essential for realistic brain modeling.
 * HOW:  Cytokines reduce visual accuracy/speed/attention, visual anomalies trigger immune responses,
 *       inflammation narrows attention to threats, sickness behavior reduces visual processing.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → VISUAL PATHWAYS:
 * -------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Impair retinal function and visual processing speed
 *    - Reduce visual acuity and contrast sensitivity
 *    - Narrow visual attention to threat-related stimuli
 *    - Reduce overall visual processing capacity
 *    - Reference: Boivin et al. (2016) "Influence of acute stress on visual attention"
 *
 * 2. Sickness Behavior:
 *    - Reduced visual exploration and scanning
 *    - Decreased attention to non-threat stimuli
 *    - Slowed visual processing and reaction times
 *    - Narrowed visual field and tunnel vision
 *    - Reference: Dantzer et al. (2008) "Sickness behavior and visual processing"
 *
 * 3. Fever and Inflammation:
 *    - Visual disturbances and blurred vision
 *    - Photophobia (light sensitivity)
 *    - Reduced color discrimination
 *    - Impaired visual memory formation
 *    - Reference: Hart (1988) "Biological basis of the behavior of sick animals"
 *
 * 4. Chronic Inflammation:
 *    - Progressive visual processing deficits
 *    - Reduced visual plasticity and learning
 *    - Impaired visual attention mechanisms
 *    - Decreased visual cortex neurogenesis
 *    - Reference: Capuron & Miller (2011) "Immune system to brain signaling"
 *
 * VISUAL → IMMUNE PATHWAYS:
 * -------------------------
 * 1. Visual Threat Detection:
 *    - Predators, dangers activate immune preparation
 *    - Visual anomalies trigger inflammatory response
 *    - Unfamiliar/threatening visual patterns → stress response
 *    - Reference: Sapolsky (2004) "Why Zebras Don't Get Ulcers"
 *
 * 2. Visual Processing Anomalies:
 *    - Corrupted/malformed visual input → immune alert
 *    - Pattern recognition failures trigger surveillance
 *    - Visual hallucinations may indicate pathogen threat
 *    - Reference: Perry et al. (2010) "Visual processing and immune activation"
 *
 * 3. Visual Stress and Arousal:
 *    - Chronic visual overstimulation → inflammation
 *    - Bright lights/strobing → immune activation
 *    - Visual deprivation → immune dysregulation
 *    - Reference: Morimoto et al. (2005) "Visual stress and inflammatory markers"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    VISUAL-IMMUNE BRIDGE                                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → VISUAL PATHWAYS                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -30% │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -20% │         │                                       │  ║
 * ║   │   │ TNF-α → -40% │         ├──→ Visual Processing Impairment       │  ║
 * ║   │   │              │         │    (Speed, Accuracy, Attention)       │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────────────────────────┐         │  ║
 * ║   │   │           VISUAL CORTEX                             │         │  ║
 * ║   │   │  - Processing speed modulation                      │         │  ║
 * ║   │   │  - Accuracy/contrast sensitivity reduction          │         │  ║
 * ║   │   │  - Attention narrowing (threat focus)               │         │  ║
 * ║   │   │  - Gabor filter gain reduction                      │         │  ║
 * ║   │   │  - Feature extraction degradation                   │         │  ║
 * ║   │   └─────────────────────────────────────────────────────┘         │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │ INFLAMMATION │         │                                       │  ║
 * ║   │   │  Severity    │  ───────┘                                       │  ║
 * ║   │   │ (Tunnel vision, photophobia, blur)                            │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  VISUAL → IMMUNE PATHWAYS                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ THREAT DETECT│ ──→ Immune Activation                           │  ║
 * ║   │   │ ANOMALY      │ ──→ Antigen Presentation                        │  ║
 * ║   │   │ CORRUPTION   │ ──→ Inflammatory Response                       │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ VISUAL STRESS│ ──→ Chronic Inflammation                        │  ║
 * ║   │   │ OVERSTIM     │ ──→ Cortisol Release                            │  ║
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

#ifndef NIMCP_VISUAL_IMMUNE_BRIDGE_H
#define NIMCP_VISUAL_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "perception/nimcp_visual_cortex.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine visual impairment factors */
#define CYTOKINE_IL1_VISUAL_IMPAIRMENT      -0.3f   /**< IL-1β → 30% reduction */
#define CYTOKINE_IL6_VISUAL_IMPAIRMENT      -0.2f   /**< IL-6 → 20% reduction */
#define CYTOKINE_TNF_VISUAL_IMPAIRMENT      -0.4f   /**< TNF-α → 40% reduction */
#define CYTOKINE_IFN_GAMMA_VISUAL_IMPAIRMENT -0.15f /**< IFN-γ → 15% reduction */
#define CYTOKINE_IL10_VISUAL_RECOVERY        0.2f   /**< IL-10 → 20% recovery */

/* Inflammation-visual degradation mapping */
#define INFLAMMATION_VISUAL_THRESHOLD        0.5f   /**< Inflammation for visual impact */
#define INFLAMMATION_TUNNEL_VISION_THRESHOLD 0.7f   /**< Threshold for tunnel vision */
#define INFLAMMATION_MAX_VISUAL_IMPAIRMENT   0.8f   /**< Maximum visual degradation */

/* Visual threat immune trigger thresholds */
#define VISUAL_THREAT_IMMUNE_THRESHOLD       0.6f   /**< Visual threat to trigger immune */
#define VISUAL_ANOMALY_SEVERITY_MULTIPLIER   1.3f   /**< Anomaly amplifies immune response */

/* Sickness behavior visual reduction */
#define SICKNESS_VISUAL_EXPLORATION_FACTOR   0.4f   /**< 60% reduction in exploration */
#define SICKNESS_VISUAL_SPEED_FACTOR         0.5f   /**< 50% slower processing */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine visual effects
 *
 * Represents how cytokine levels impair visual processing
 */
typedef struct {
    /* Pro-inflammatory impairments */
    float il1_processing_reduction;    /**< IL-1β processing speed reduction */
    float il6_accuracy_reduction;      /**< IL-6 accuracy impairment */
    float tnf_attention_reduction;     /**< TNF-α attention capacity reduction */
    float ifn_gamma_contrast_reduction; /**< IFN-γ contrast sensitivity reduction */

    /* Anti-inflammatory recovery */
    float il10_recovery_boost;         /**< IL-10 recovery enhancement */

    /* Aggregate effects */
    float total_processing_factor;     /**< Combined processing speed [0-1] */
    float total_accuracy_factor;       /**< Combined accuracy [0-1] */
    float total_attention_factor;      /**< Combined attention capacity [0-1] */
    float sickness_visual_impairment;  /**< Overall sickness behavior impact [0-1] */
} cytokine_visual_effects_t;

/**
 * @brief Inflammation visual state
 *
 * How chronic inflammation affects visual processing
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 7 days */

    /* Visual impacts */
    float processing_speed_reduction;  /**< Processing speed loss [0-1] */
    float visual_acuity_loss;          /**< Acuity degradation [0-1] */
    float contrast_sensitivity_loss;   /**< Contrast sensitivity reduction [0-1] */
    float tunnel_vision_severity;      /**< Attention narrowing [0-1] */
    float photophobia_level;           /**< Light sensitivity [0-1] */

    /* Feature extraction degradation */
    float gabor_filter_gain_reduction; /**< Gabor filter effectiveness loss */
    float feature_extraction_noise;    /**< Added noise to features [0-1] */
} inflammation_visual_state_t;

/**
 * @brief Visual threat immune trigger
 *
 * How visual anomalies/threats trigger immune response
 */
typedef struct {
    /* Visual threat indicators */
    float threat_salience;             /**< Visual threat salience [0-1] */
    float pattern_corruption_level;    /**< Input corruption detected [0-1] */
    float processing_anomaly;          /**< Unexpected processing behavior [0-1] */

    /* Immune triggers */
    bool threat_triggered;             /**< Threat activated immune */
    bool anomaly_triggered;            /**< Anomaly activated immune */
    float immune_activation_level;     /**< Immune response strength [0-1] */

    /* Stress response */
    float visual_stress_duration_sec;  /**< How long stressed */
    float chronic_overstimulation;     /**< Chronic visual stress [0-1] */
} visual_immune_trigger_t;

/**
 * @brief Sickness behavior visual effects
 *
 * How sickness behavior reduces visual exploration and processing
 */
typedef struct {
    /* Sickness state */
    float sickness_behavior_level;     /**< Overall sickness [0-1] */
    float fatigue_level;               /**< Energy depletion [0-1] */

    /* Visual behavior changes */
    float exploration_reduction;       /**< Reduced visual scanning [0-1] */
    float processing_speed_reduction;  /**< Slower visual processing [0-1] */
    float attention_to_threats_boost;  /**< Enhanced threat detection [0-1] */
    float attention_to_novelty_reduction; /**< Reduced novelty seeking [0-1] */
} sickness_visual_effects_t;

/**
 * @brief Complete visual-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    visual_cortex_t* visual_cortex;

    /* Current state */
    cytokine_visual_effects_t cytokine_effects;
    inflammation_visual_state_t inflammation_state;
    visual_immune_trigger_t visual_trigger;
    sickness_visual_effects_t sickness_effects;

    /* Integration flags */
    bool enable_cytokine_visual_modulation;
    bool enable_inflammation_visual_impairment;
    bool enable_visual_immune_trigger;
    bool enable_sickness_visual_reduction;
    bool enable_tunnel_vision;
    bool enable_threat_salience_boost;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t visual_triggered_responses;
    uint32_t threat_detections;
    uint32_t anomaly_detections;
    } visual_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_visual_modulation;
    bool enable_inflammation_visual_impairment;
    bool enable_visual_immune_trigger;
    bool enable_sickness_visual_reduction;
    bool enable_tunnel_vision;
    bool enable_threat_salience_boost;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float visual_trigger_sensitivity;  /**< Visual trigger multiplier [0.5-2.0] */

    /* Thresholds */
    float visual_threat_threshold;     /**< Visual threat to trigger immune [0.4-0.8] */
    float inflammation_visual_threshold; /**< Inflammation for visual impact [0.3-0.7] */
} visual_immune_config_t;

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
int visual_immune_default_config(visual_immune_config_t* config);

/**
 * @brief Create visual-immune bridge
 *
 * WHAT: Initialize bidirectional visual-immune integration
 * WHY:  Enable realistic immune-visual coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param visual_cortex Visual cortex
 * @return New bridge or NULL on failure
 */
visual_immune_bridge_t* visual_immune_bridge_create(
    const visual_immune_config_t* config,
    brain_immune_system_t* immune_system,
    visual_cortex_t* visual_cortex
);

/**
 * @brief Destroy visual-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void visual_immune_bridge_destroy(visual_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Visual API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to visual processing
 *
 * WHAT: Modulate visual processing based on cytokine levels
 * WHY:  Pro-inflammatory cytokines impair visual function
 * HOW:  Query immune cytokines, reduce processing speed/accuracy/attention
 *
 * @param bridge Visual-immune bridge
 * @return 0 on success
 */
int visual_immune_apply_cytokine_effects(visual_immune_bridge_t* bridge);

/**
 * @brief Apply chronic inflammation to visual cortex
 *
 * WHAT: Degrade visual processing from prolonged inflammation
 * WHY:  Chronic inflammation causes progressive visual deficits
 * HOW:  Check inflammation duration/level, reduce visual capabilities
 *
 * @param bridge Visual-immune bridge
 * @return 0 on success
 */
int visual_immune_apply_inflammation_effects(visual_immune_bridge_t* bridge);

/**
 * @brief Apply sickness behavior to visual processing
 *
 * WHAT: Reduce visual exploration and processing during sickness
 * WHY:  Sickness behavior conserves energy, focuses on threats
 * HOW:  Reduce scanning, narrow attention to threats, slow processing
 *
 * @param bridge Visual-immune bridge
 * @return 0 on success
 */
int visual_immune_apply_sickness_effects(visual_immune_bridge_t* bridge);

/**
 * @brief Compute tunnel vision severity from inflammation
 *
 * WHAT: Calculate attention narrowing from immune state
 * WHY:  Inflammation causes visual field narrowing
 * HOW:  Map inflammation level to tunnel vision [0-1]
 *
 * @param bridge Visual-immune bridge
 * @return Tunnel vision severity [0-1]
 */
float visual_immune_compute_tunnel_vision(const visual_immune_bridge_t* bridge);

/**
 * @brief Modulate visual cortex neuromodulation from immune state
 *
 * WHAT: Adjust visual cortex neuromodulator levels based on inflammation
 * WHY:  Cytokines affect neurotransmitter release
 * HOW:  Set visual cortex ACh/NE levels based on immune state
 *
 * @param bridge Visual-immune bridge
 * @return 0 on success
 */
int visual_immune_modulate_neurotransmitters(visual_immune_bridge_t* bridge);

/* ============================================================================
 * Visual → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from visual threat
 *
 * WHAT: Activate immune system from high-salience visual threat
 * WHY:  Visual threats (predators, dangers) trigger immune preparation
 * HOW:  Check visual threat salience, trigger antigen presentation
 *
 * @param bridge Visual-immune bridge
 * @param threat_features Visual features of threat
 * @param num_features Number of features
 * @param salience Threat salience [0-1]
 * @return 0 on success
 */
int visual_immune_trigger_from_threat(
    visual_immune_bridge_t* bridge,
    const float* threat_features,
    uint32_t num_features,
    float salience
);

/**
 * @brief Trigger immune response from visual anomaly
 *
 * WHAT: Activate immune when visual processing detects corruption/anomaly
 * WHY:  Visual anomalies may indicate pathogen or system threat
 * HOW:  Detect pattern corruption, trigger antigen presentation
 *
 * @param bridge Visual-immune bridge
 * @param anomaly_score Anomaly severity [0-1]
 * @return 0 on success
 */
int visual_immune_trigger_from_anomaly(
    visual_immune_bridge_t* bridge,
    float anomaly_score
);

/**
 * @brief Trigger immune response from chronic visual stress
 *
 * WHAT: Activate inflammatory response from prolonged visual overstimulation
 * WHY:  Chronic visual stress causes inflammation
 * HOW:  Check visual stress duration, trigger pro-inflammatory cytokines
 *
 * @param bridge Visual-immune bridge
 * @return 0 on success
 */
int visual_immune_trigger_from_visual_stress(visual_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update visual-immune bridge (both directions)
 *
 * WHAT: Process all immune-visual interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine/inflammation effects, trigger immune from threats
 *
 * @param bridge Visual-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int visual_immune_bridge_update(
    visual_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine visual effects
 *
 * @param bridge Visual-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int visual_immune_get_cytokine_effects(
    const visual_immune_bridge_t* bridge,
    cytokine_visual_effects_t* effects
);

/**
 * @brief Get current inflammation visual state
 *
 * @param bridge Visual-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int visual_immune_get_inflammation_state(
    const visual_immune_bridge_t* bridge,
    inflammation_visual_state_t* state
);

/**
 * @brief Check if experiencing sickness behavior visual impairment
 *
 * WHAT: Determine if sickness behavior is affecting visual processing
 * WHY:  Sickness behavior is distinct syndrome affecting vision
 * HOW:  Check cytokine levels and sickness behavior score
 *
 * @param bridge Visual-immune bridge
 * @return true if experiencing sickness visual impairment
 */
bool visual_immune_is_sick_behavior(const visual_immune_bridge_t* bridge);

/**
 * @brief Get visual processing speed factor
 *
 * WHAT: Get current visual processing speed multiplier
 * WHY:  Inflammation and cytokines slow visual processing
 * HOW:  Return combined speed factor [0-1]
 *
 * @param bridge Visual-immune bridge
 * @return Processing speed factor [0-1], 1.0 = normal, <1.0 = impaired
 */
float visual_immune_get_processing_speed_factor(const visual_immune_bridge_t* bridge);

/**
 * @brief Get visual accuracy factor
 *
 * WHAT: Get current visual accuracy multiplier
 * WHY:  Cytokines reduce visual acuity and accuracy
 * HOW:  Return combined accuracy factor [0-1]
 *
 * @param bridge Visual-immune bridge
 * @return Accuracy factor [0-1], 1.0 = normal, <1.0 = impaired
 */
float visual_immune_get_accuracy_factor(const visual_immune_bridge_t* bridge);

/**
 * @brief Get attention capacity factor
 *
 * WHAT: Get current visual attention capacity
 * WHY:  Inflammation narrows attention to threats
 * HOW:  Return combined attention capacity [0-1]
 *
 * @param bridge Visual-immune bridge
 * @return Attention capacity [0-1], 1.0 = normal, <1.0 = narrowed
 */
float visual_immune_get_attention_capacity(const visual_immune_bridge_t* bridge);

/**
 * @brief Get threat salience boost factor
 *
 * WHAT: Get threat detection enhancement during immune activation
 * WHY:  Immune activation enhances threat-related visual processing
 * HOW:  Return threat salience multiplier [1.0-2.0]
 *
 * @param bridge Visual-immune bridge
 * @return Threat salience boost [1.0-2.0], 1.0 = normal, >1.0 = enhanced
 */
float visual_immune_get_threat_salience_boost(const visual_immune_bridge_t* bridge);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_VISUAL
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int visual_immune_connect_bio_async(visual_immune_bridge_t* bridge);

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
int visual_immune_disconnect_bio_async(visual_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool visual_immune_is_bio_async_connected(const visual_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VISUAL_IMMUNE_BRIDGE_H */
