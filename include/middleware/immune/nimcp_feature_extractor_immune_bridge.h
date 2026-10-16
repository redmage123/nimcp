/**
 * @file nimcp_feature_extractor_immune_bridge.h
 * @brief Feature Extractor-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and feature extraction
 * WHY:  Biological evidence shows inflammation affects sensory cortex processing and
 *       feature detection. Immune state modulates perceptual precision and attention.
 * HOW:  Cytokines reduce feature extraction accuracy, inflammation narrows feature
 *       attention to threat-relevant patterns, abnormal features trigger immune response.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → FEATURE EXTRACTION PATHWAYS:
 * -------------------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Affect sensory cortex neural excitability
 *    - Reduce precision of feature detection
 *    - Increase perceptual noise and decrease signal-to-noise ratio
 *    - Impair temporal precision of spike-based feature coding
 *    - Reference: Dantzer et al. (2014) "Neuroimmune interactions in sickness behavior"
 *
 * 2. IL-6 and Sensory Processing:
 *    - Directly affects primary sensory cortices (V1, A1, S1)
 *    - Reduces firing rate stability → increased ISI variability
 *    - Impairs population synchrony for feature binding
 *    - Decreases oscillatory power in gamma band (feature integration)
 *    - Reference: Huang & Sheng (2010) "Regulation of synaptic plasticity by cytokines"
 *
 * 3. Chronic Inflammation and Perception:
 *    - Sustained inflammation → perceptual bandwidth reduction
 *    - Processing resources allocated to threat detection
 *    - Non-threat features suppressed (narrowed attention)
 *    - Reduced entropy in feature space (less diverse coding)
 *    - Reference: Harrison et al. (2009) "Inflammation affects sensory processing"
 *
 * 4. Sickness Behavior and Feature Extraction:
 *    - Overall reduction in feature extraction accuracy
 *    - Fatigue reduces computational resources
 *    - Withdrawal behavior → reduced sensory sampling
 *    - Reference: Kelley et al. (2003) "Cytokine-induced sickness behavior"
 *
 * FEATURE EXTRACTION → IMMUNE PATHWAYS:
 * --------------------------------------
 * 1. Threat-Relevant Features:
 *    - High burst index → potential threat patterns
 *    - Abnormal synchrony → anomaly detection
 *    - Unusual oscillation patterns → foreign signal detection
 *    - Low entropy → stereotyped (potentially pathological) activity
 *    - Reference: Quiroga & Panzeri (2009) "Extracting information from spike trains"
 *
 * 2. Feature Anomalies Trigger Immune:
 *    - Fano factor >3 (highly variable) → instability alert
 *    - Gamma power collapse → loss of binding → immune investigation
 *    - Extreme ISI CV (>2) → pathological firing → immune response
 *    - Zero entropy → dead/hijacked neurons → cytotoxic response
 *    - Reference: Sporns (2013) "Network attributes for segregation and integration"
 *
 * 3. Feature Quality Feedback:
 *    - Sustained low-quality features → chronic immune activation
 *    - Feature extraction failures → inflammation escalation
 *    - Reference: Chávez et al. (2003) "Functional modularity of background activities"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              FEATURE EXTRACTOR-IMMUNE BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │            IMMUNE → FEATURE EXTRACTION PATHWAYS                     │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -0.3 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -0.4 │         │                                       │  ║
 * ║   │   │ TNF-α → -0.5 │         ├──→ Reduce Feature Precision           │  ║
 * ║   │   │              │         │    (ISI CV ↑, Sync ↓, Noise ↑)        │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │   FEATURE EXTRACTOR             │                             │  ║
 * ║   │   │  - Firing rate ↓                │                             │  ║
 * ║   │   │  - ISI variability ↑            │                             │  ║
 * ║   │   │  - Synchrony ↓                  │                             │  ║
 * ║   │   │  - Gamma power ↓                │                             │  ║
 * ║   │   │  - Entropy ↓ (narrowed)         │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────────┐                                │  ║
 * ║   │   │   INFLAMMATION               │                                │  ║
 * ║   │   │ ──────────────────────────── │                                │  ║
 * ║   │   │ LOCAL    → -10% precision    │                                │  ║
 * ║   │   │ REGIONAL → -25% precision    │                                │  ║
 * ║   │   │ SYSTEMIC → -50% precision    │                                │  ║
 * ║   │   │ STORM    → -80% precision    │                                │  ║
 * ║   │   │                              │                                │  ║
 * ║   │   │ Threat bias: ↑ burst/sync    │                                │  ║
 * ║   │   │              detection        │                                │  ║
 * ║   │   └──────────────────────────────┘                                │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │         FEATURE EXTRACTION → IMMUNE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │  THREAT FEATURES         │ ──→ Immune Alert                    │  ║
 * ║   │   │  - Burst index > 0.7     │ ──→ Severity 6                      │  ║
 * ║   │   │  - Fano factor > 3.0     │ ──→ Severity 7                      │  ║
 * ║   │   │  - ISI CV > 2.0          │ ──→ Severity 8                      │  ║
 * ║   │   │  - Sync index > 0.9      │ ──→ Severity 5                      │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │  PATHOLOGICAL FEATURES   │ ──→ Strong Immune Response          │  ║
 * ║   │   │  - Zero entropy          │ ──→ Severity 10 (dead neurons)      │  ║
 * ║   │   │  - Gamma collapse        │ ──→ Severity 9  (binding failure)   │  ║
 * ║   │   │  - Extreme variability   │ ──→ Severity 8  (instability)       │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
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

#ifndef NIMCP_FEATURE_EXTRACTOR_IMMUNE_BRIDGE_H
#define NIMCP_FEATURE_EXTRACTOR_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "middleware/features/nimcp_feature_extractor.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine feature precision impact factors */
#define CYTOKINE_IL1_PRECISION_IMPACT      -0.30f   /**< IL-1β → reduced precision */
#define CYTOKINE_IL6_PRECISION_IMPACT      -0.40f   /**< IL-6 → reduced precision */
#define CYTOKINE_TNF_PRECISION_IMPACT      -0.50f   /**< TNF-α → strong reduction */
#define CYTOKINE_IFN_GAMMA_PRECISION_IMPACT -0.20f  /**< IFN-γ → mild reduction */

/* Inflammation-precision mapping */
#define INFLAMMATION_PRECISION_NONE        1.00f    /**< No reduction */
#define INFLAMMATION_PRECISION_LOCAL       0.90f    /**< -10% precision */
#define INFLAMMATION_PRECISION_REGIONAL    0.75f    /**< -25% precision */
#define INFLAMMATION_PRECISION_SYSTEMIC    0.50f    /**< -50% precision */
#define INFLAMMATION_PRECISION_STORM       0.20f    /**< -80% precision (severe) */

/* Feature anomaly thresholds for immune triggering */
#define FEATURE_BURST_THREAT_THRESHOLD     0.70f    /**< Burst index threat level */
#define FEATURE_FANO_THREAT_THRESHOLD      3.00f    /**< Fano factor threat level */
#define FEATURE_ISI_CV_THREAT_THRESHOLD    2.00f    /**< ISI CV threat level */
#define FEATURE_SYNC_THREAT_THRESHOLD      0.90f    /**< Synchrony threat level */
#define FEATURE_ENTROPY_DEAD_THRESHOLD     0.10f    /**< Zero entropy = dead neurons */
#define FEATURE_GAMMA_COLLAPSE_THRESHOLD   0.10f    /**< Gamma power collapse */

/* Feature anomaly severity mapping */
#define FEATURE_SEVERITY_BURST_ANOMALY     6        /**< High burst activity */
#define FEATURE_SEVERITY_FANO_ANOMALY      7        /**< High variability */
#define FEATURE_SEVERITY_ISI_ANOMALY       8        /**< Pathological firing */
#define FEATURE_SEVERITY_SYNC_ANOMALY      5        /**< Abnormal synchrony */
#define FEATURE_SEVERITY_ENTROPY_ZERO      10       /**< Dead neurons (critical) */
#define FEATURE_SEVERITY_GAMMA_COLLAPSE    9        /**< Binding failure (severe) */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on feature extraction
 *
 * Represents how cytokine levels modulate feature extraction quality
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_precision_reduction;        /**< IL-1β precision impact [0-1] */
    float il6_precision_reduction;        /**< IL-6 precision impact [0-1] */
    float tnf_precision_reduction;        /**< TNF-α precision impact [0-1] */
    float ifn_gamma_precision_reduction;  /**< IFN-γ precision impact [0-1] */

    /* Aggregate effects */
    float total_precision_factor;         /**< Combined precision multiplier [0-1] */
    float noise_amplification;            /**< Increased perceptual noise [0-1] */
    float bandwidth_reduction;            /**< Processing bandwidth loss [0-1] */
    float temporal_jitter;                /**< Increased timing variability (ms) */
} cytokine_feature_effects_t;

/**
 * @brief Inflammation effects on feature extraction
 *
 * How chronic inflammation affects feature processing
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;       /**< How long inflamed */
    bool is_chronic;                       /**< >= 7 days */

    /* Feature extraction impacts */
    float precision_multiplier;            /**< Overall precision reduction [0-1] */
    float rate_coding_impairment;          /**< Firing rate reliability loss [0-1] */
    float temporal_coding_impairment;      /**< ISI precision loss [0-1] */
    float population_coding_impairment;    /**< Synchrony/binding loss [0-1] */
    float oscillation_impairment;          /**< Gamma power reduction [0-1] */

    /* Attention bias */
    float threat_feature_bias;             /**< Bias toward threat features [0-1] */
    float non_threat_suppression;          /**< Suppression of normal features [0-1] */
} inflammation_feature_state_t;

/**
 * @brief Feature anomalies triggering immune response
 *
 * Abnormal feature patterns that indicate threats
 */
typedef struct {
    /* Anomaly indicators */
    bool burst_anomaly;                    /**< Excessive bursting */
    bool fano_anomaly;                     /**< Extreme variability */
    bool isi_cv_anomaly;                   /**< Pathological ISI patterns */
    bool sync_anomaly;                     /**< Abnormal synchrony */
    bool entropy_collapse;                 /**< Zero/low entropy (dead) */
    bool gamma_collapse;                   /**< Gamma power loss */

    /* Anomaly severity */
    float burst_severity;                  /**< Burst anomaly severity [0-1] */
    float fano_severity;                   /**< Fano anomaly severity [0-1] */
    float isi_cv_severity;                 /**< ISI CV anomaly severity [0-1] */
    float sync_severity;                   /**< Sync anomaly severity [0-1] */
    float entropy_severity;                /**< Entropy anomaly severity [0-1] */
    float gamma_severity;                  /**< Gamma anomaly severity [0-1] */

    /* Combined threat level */
    float total_threat_level;              /**< Overall threat [0-1] */
    uint32_t immune_severity;              /**< Immune response severity [1-10] */
} feature_immune_trigger_t;

/**
 * @brief Feature quality degradation tracking
 *
 * Monitor sustained low-quality features for chronic immune activation
 */
typedef struct {
    /* Quality metrics */
    float mean_precision;                  /**< Average precision [0-1] */
    float min_precision;                   /**< Worst precision [0-1] */
    float precision_stability;             /**< Precision variability [0-1] */

    /* Degradation tracking */
    float degradation_duration_sec;        /**< How long degraded */
    bool chronic_degradation;              /**< Sustained low quality */
    float chronic_threshold;               /**< Threshold for chronic [0-1] */

    /* Immune escalation */
    uint32_t escalation_count;             /**< Times escalated */
    bool immune_activated;                 /**< Immune currently active */
} feature_quality_monitor_t;

/**
 * @brief Complete feature extractor-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    feature_extractor_t feature_extractor;

    /* Current state */
    cytokine_feature_effects_t cytokine_effects;
    inflammation_feature_state_t inflammation_state;
    feature_immune_trigger_t immune_trigger;
    feature_quality_monitor_t quality_monitor;

    /* Integration flags */
    bool enable_cytokine_feature_modulation;
    bool enable_inflammation_precision_reduction;
    bool enable_feature_immune_trigger;
    bool enable_threat_feature_bias;
    bool enable_quality_monitoring;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t feature_triggered_responses;
    uint32_t anomalies_detected;
    uint32_t quality_escalations;

    } feature_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_feature_modulation;
    bool enable_inflammation_precision_reduction;
    bool enable_feature_immune_trigger;
    bool enable_threat_feature_bias;
    bool enable_quality_monitoring;

    /* Sensitivity tuning */
    float cytokine_sensitivity;            /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;        /**< Inflammation effect multiplier [0.5-2.0] */
    float anomaly_trigger_sensitivity;     /**< Anomaly trigger multiplier [0.5-2.0] */

    /* Thresholds */
    float burst_threshold;                 /**< Burst anomaly threshold [0.5-0.9] */
    float fano_threshold;                  /**< Fano anomaly threshold [2.0-5.0] */
    float isi_cv_threshold;                /**< ISI CV anomaly threshold [1.5-3.0] */
    float sync_threshold;                  /**< Sync anomaly threshold [0.7-0.95] */
    float entropy_collapse_threshold;      /**< Entropy collapse threshold [0.0-0.2] */
    float gamma_collapse_threshold;        /**< Gamma collapse threshold [0.0-0.2] */

    /* Quality monitoring */
    float chronic_degradation_threshold;   /**< Chronic quality threshold [0.3-0.7] */
    float chronic_duration_sec;            /**< Duration for chronic (seconds) */
} feature_immune_config_t;

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
int feature_immune_default_config(feature_immune_config_t* config);

/**
 * @brief Create feature extractor-immune bridge
 *
 * WHAT: Initialize bidirectional feature-immune integration
 * WHY:  Enable realistic immune-feature coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param feature_extractor Feature extractor
 * @return New bridge or NULL on failure
 */
feature_immune_bridge_t* feature_immune_bridge_create(
    const feature_immune_config_t* config,
    brain_immune_system_t* immune_system,
    feature_extractor_t feature_extractor
);

/**
 * @brief Destroy feature extractor-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void feature_immune_bridge_destroy(feature_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Feature Extraction API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to feature extraction
 *
 * WHAT: Modulate feature extraction based on cytokine levels
 * WHY:  Pro-inflammatory cytokines reduce sensory precision
 * HOW:  Query immune system cytokines, reduce feature extraction precision
 *
 * @param bridge Feature-immune bridge
 * @return 0 on success
 */
int feature_immune_apply_cytokine_effects(feature_immune_bridge_t* bridge);

/**
 * @brief Apply chronic inflammation to feature extraction
 *
 * WHAT: Reduce feature precision from prolonged inflammation
 * WHY:  Chronic inflammation narrows perceptual bandwidth
 * HOW:  Check inflammation duration/level, reduce extraction quality
 *
 * @param bridge Feature-immune bridge
 * @return 0 on success
 */
int feature_immune_apply_inflammation_effects(feature_immune_bridge_t* bridge);

/**
 * @brief Compute precision reduction from inflammation
 *
 * WHAT: Calculate feature precision loss from immune state
 * WHY:  Inflammation reduces computational resources
 * HOW:  Map inflammation level/duration to precision multiplier [0-1]
 *
 * @param bridge Feature-immune bridge
 * @return Precision multiplier [0-1]
 */
float feature_immune_compute_precision_reduction(const feature_immune_bridge_t* bridge);

/**
 * @brief Bias feature extraction toward threat patterns
 *
 * WHAT: Under inflammation, prioritize threat-relevant features
 * WHY:  Sickness behavior narrows attention to threats
 * HOW:  Increase sensitivity to burst/sync anomalies, suppress normal features
 *
 * @param bridge Feature-immune bridge
 * @return 0 on success
 */
int feature_immune_apply_threat_bias(feature_immune_bridge_t* bridge);

/* ============================================================================
 * Feature Extraction → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from feature anomalies
 *
 * WHAT: Activate immune system from abnormal feature patterns
 * WHY:  Feature anomalies indicate neural threats
 * HOW:  Check features against thresholds, trigger immune response
 *
 * @param bridge Feature-immune bridge
 * @param features Current extracted features
 * @return 0 on success
 */
int feature_immune_trigger_from_anomalies(
    feature_immune_bridge_t* bridge,
    const middleware_features_t* features
);

/**
 * @brief Escalate inflammation from sustained feature degradation
 *
 * WHAT: Increase inflammatory response when feature quality degrades
 * WHY:  Chronic poor features indicate systemic problem
 * HOW:  Monitor quality over time, escalate if sustained low quality
 *
 * @param bridge Feature-immune bridge
 * @param features Current extracted features
 * @return 0 on success
 */
int feature_immune_escalate_from_degradation(
    feature_immune_bridge_t* bridge,
    const middleware_features_t* features
);

/**
 * @brief Detect dead neurons from zero entropy
 *
 * WHAT: Trigger critical immune response when entropy collapses
 * WHY:  Zero entropy indicates dead/hijacked neurons
 * HOW:  Check spike entropy, trigger severity 10 response
 *
 * @param bridge Feature-immune bridge
 * @param features Current extracted features
 * @return 0 on success
 */
int feature_immune_detect_dead_neurons(
    feature_immune_bridge_t* bridge,
    const middleware_features_t* features
);

/**
 * @brief Detect binding failure from gamma collapse
 *
 * WHAT: Trigger severe immune response when gamma power collapses
 * WHY:  Gamma collapse indicates loss of feature binding
 * HOW:  Check gamma power, trigger severity 9 response
 *
 * @param bridge Feature-immune bridge
 * @param features Current extracted features
 * @return 0 on success
 */
int feature_immune_detect_binding_failure(
    feature_immune_bridge_t* bridge,
    const middleware_features_t* features
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update feature-immune bridge (both directions)
 *
 * WHAT: Process all immune-feature interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, check anomalies, monitor quality
 *
 * @param bridge Feature-immune bridge
 * @param features Current extracted features
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int feature_immune_bridge_update(
    feature_immune_bridge_t* bridge,
    const middleware_features_t* features,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine feature effects
 *
 * @param bridge Feature-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int feature_immune_get_cytokine_effects(
    const feature_immune_bridge_t* bridge,
    cytokine_feature_effects_t* effects
);

/**
 * @brief Get current inflammation feature state
 *
 * @param bridge Feature-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int feature_immune_get_inflammation_state(
    const feature_immune_bridge_t* bridge,
    inflammation_feature_state_t* state
);

/**
 * @brief Get current precision reduction factor
 *
 * WHAT: Query how much precision is reduced by immune state
 * WHY:  Downstream systems need to know feature quality
 * HOW:  Return combined cytokine + inflammation precision multiplier
 *
 * @param bridge Feature-immune bridge
 * @return Precision multiplier [0-1]
 */
float feature_immune_get_precision_factor(const feature_immune_bridge_t* bridge);

/**
 * @brief Check if features indicate threat
 *
 * WHAT: Determine if current features are anomalous
 * WHY:  Quick threat assessment
 * HOW:  Check anomaly flags in immune_trigger
 *
 * @param bridge Feature-immune bridge
 * @return true if threat detected
 */
bool feature_immune_is_threat_detected(const feature_immune_bridge_t* bridge);

/**
 * @brief Get feature quality score
 *
 * WHAT: Compute overall feature extraction quality
 * WHY:  Assess health of feature extraction system
 * HOW:  Combine precision, stability, and degradation metrics
 *
 * @param bridge Feature-immune bridge
 * @return Quality score [0-1]
 */
float feature_immune_get_quality_score(const feature_immune_bridge_t* bridge);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_FEATURE_EXTRACTOR
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int feature_extractor_immune_connect_bio_async(feature_immune_bridge_t* bridge);

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
int feature_extractor_immune_disconnect_bio_async(feature_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool feature_extractor_immune_is_bio_async_connected(const feature_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEATURE_EXTRACTOR_IMMUNE_BRIDGE_H */
