/**
 * @file nimcp_feature_extractor_fep_bridge.h
 * @brief Free Energy Principle - Feature Extractor Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and feature extraction
 * WHY:  Features represent intermediate hierarchical levels in FEP generative models;
 *       FEP precision modulates feature extraction sensitivity and selection
 * HOW:  FEP hierarchy level → feature set selection; features → hierarchical observations;
 *       prediction errors adjust extraction thresholds
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * FEP → FEATURE EXTRACTION PATHWAYS:
 * -----------------------------------
 * 1. Hierarchical Feature Selection:
 *    - FEP hierarchy level determines which features to extract
 *    - Lower levels → simple features (firing rate, ISI)
 *    - Higher levels → complex features (synchrony, oscillations, entropy)
 *    - Reference: Friston (2008) "Hierarchical models in the brain"
 *
 * 2. Precision-Weighted Feature Extraction:
 *    - High precision → extract more features (fine-grained analysis)
 *    - Low precision → extract only robust features (coarse analysis)
 *    - Adaptive feature set based on confidence
 *    - Reference: Feldman & Friston (2010) "Attention, uncertainty, and free-energy"
 *
 * 3. Prediction-Driven Feature Gating:
 *    - Expected features extracted first (prediction-confirmation)
 *    - Unexpected features trigger additional extraction
 *    - Efficient processing via prediction
 *    - Reference: Rao & Ballard (1999) "Predictive coding in the visual cortex"
 *
 * FEATURE EXTRACTION → FEP PATHWAYS:
 * -----------------------------------
 * 1. Features as Hierarchical Observations:
 *    - Rate features → low-level observations
 *    - Temporal features → intermediate observations
 *    - Synchrony/oscillations → high-level observations
 *    - Reference: Bastos et al. (2012) "Canonical microcircuits for predictive coding"
 *
 * 2. Feature Entropy as Uncertainty Signal:
 *    - High feature entropy → high observation uncertainty
 *    - Low entropy → reliable observations
 *    - Entropy feeds back to FEP precision
 *    - Reference: Shannon entropy measures information content
 *
 * 3. Oscillation Features as Brain State:
 *    - Delta/theta → offline processing (low precision)
 *    - Alpha → attention/inhibition (medium precision)
 *    - Beta/gamma → active inference (high precision)
 *    - Reference: Fries (2015) "Rhythms for cognition"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                FEP-FEATURE EXTRACTION BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │            FEP → FEATURE EXTRACTION PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────┐                                         │  ║
 * ║   │   │  FEP HIERARCHY       │                                         │  ║
 * ║   │   │  ────────────────    │                                         │  ║
 * ║   │   │  Level 0 → Rate      │ ──→ Extract basic features              │  ║
 * ║   │   │  Level 1 → Temporal  │ ──→ Add temporal features               │  ║
 * ║   │   │  Level 2 → Synchrony │ ──→ Add population features             │  ║
 * ║   │   │  Level 3 → Oscillate │ ──→ Add oscillation features            │  ║
 * ║   │   └──────────────────────┘                                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────┐                                         │  ║
 * ║   │   │  FEP PRECISION       │                                         │  ║
 * ║   │   │  ────────────────    │                                         │  ║
 * ║   │   │  High: 0.9           │ ──→ Extract all features                │  ║
 * ║   │   │  Med:  0.5           │ ──→ Extract core features               │  ║
 * ║   │   │  Low:  0.2           │ ──→ Extract minimal features            │  ║
 * ║   │   └──────────────────────┘                                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────┐                                         │  ║
 * ║   │   │  PREDICTIONS         │ ──→ Expected Feature Gating             │  ║
 * ║   │   │  Expected rate: 10Hz │ ──→ Confirm or flag deviation           │  ║
 * ║   │   └──────────────────────┘                                         │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │         FEATURE EXTRACTION → FEP PATHWAYS                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌───────────────────────┐                                        │  ║
 * ║   │   │ EXTRACTED FEATURES    │ ──→ Hierarchical Observations          │  ║
 * ║   │   │ Rate: 12 Hz           │ ──→ Level 0 observation                │  ║
 * ║   │   │ Synchrony: 0.7        │ ──→ Level 2 observation                │  ║
 * ║   │   │ Gamma: 0.8            │ ──→ Level 3 observation                │  ║
 * ║   │   └───────────────────────┘                                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌───────────────────────┐                                        │  ║
 * ║   │   │ SPIKE ENTROPY         │ ──→ FEP Uncertainty                    │  ║
 * ║   │   │ High → 4.2 bits       │ ──→ Low precision                      │  ║
 * ║   │   │ Low  → 1.1 bits       │ ──→ High precision                     │  ║
 * ║   │   └───────────────────────┘                                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌───────────────────────┐                                        │  ║
 * ║   │   │ OSCILLATION STATE     │ ──→ Brain State / Precision Mode       │  ║
 * ║   │   │ High gamma → 0.9      │ ──→ Active inference                   │  ║
 * ║   │   │ High alpha → 0.4      │ ──→ Inhibition/rest                    │  ║
 * ║   │   └───────────────────────┘                                        │  ║
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

#ifndef NIMCP_FEATURE_EXTRACTOR_FEP_BRIDGE_H
#define NIMCP_FEATURE_EXTRACTOR_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "middleware/features/nimcp_feature_extractor.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Hierarchy-feature mapping */
#define FEP_HIERARCHY_LEVEL_RATE         0  /**< Basic rate features */
#define FEP_HIERARCHY_LEVEL_TEMPORAL     1  /**< Temporal features */
#define FEP_HIERARCHY_LEVEL_POPULATION   2  /**< Population features */
#define FEP_HIERARCHY_LEVEL_OSCILLATION  3  /**< Oscillation features */

/* Precision-feature count mapping */
#define FEP_PRECISION_HIGH_FEATURE_COUNT    13  /**< All features */
#define FEP_PRECISION_MED_FEATURE_COUNT     8   /**< Core features */
#define FEP_PRECISION_LOW_FEATURE_COUNT     3   /**< Minimal features */

/* Oscillation-precision mapping */
#define GAMMA_HIGH_PRECISION_THRESHOLD   0.6f  /**< Gamma → active inference */
#define BETA_MED_PRECISION_THRESHOLD     0.4f  /**< Beta → active processing */
#define ALPHA_LOW_PRECISION_THRESHOLD    0.5f  /**< Alpha → inhibition */

/* Entropy-precision mapping */
#define ENTROPY_LOW_UNCERTAINTY          1.5f  /**< Low entropy → reliable */
#define ENTROPY_HIGH_UNCERTAINTY         3.5f  /**< High entropy → unreliable */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct feature_extractor_fep_bridge feature_extractor_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for feature extractor-FEP bridge
 */
typedef struct {
    /* Feature enables */
    bool enable_hierarchical_selection;     /**< Hierarchy → feature set */
    bool enable_precision_gating;           /**< Precision → feature count */
    bool enable_prediction_gating;          /**< Predictions → expected features */
    bool enable_entropy_feedback;           /**< Entropy → FEP uncertainty */
    bool enable_oscillation_state;          /**< Oscillations → brain state */

    /* Sensitivity factors */
    float hierarchy_sensitivity;            /**< Hierarchy level scaling */
    float precision_sensitivity;            /**< Precision → gating scaling */
    float entropy_sensitivity;              /**< Entropy → uncertainty scaling */
    float oscillation_sensitivity;          /**< Oscillation → state scaling */
} feature_extractor_fep_config_t;

/**
 * @brief FEP effects on feature extraction
 */
typedef struct {
    /* Feature selection */
    uint32_t active_hierarchy_level;        /**< Current hierarchy level */
    uint32_t enabled_feature_count;         /**< Number of features to extract */
    bool extract_rate_features;             /**< Extract rate features */
    bool extract_temporal_features;         /**< Extract temporal features */
    bool extract_population_features;       /**< Extract population features */
    bool extract_oscillation_features;      /**< Extract oscillation features */

    /* Extraction modulation */
    float extraction_threshold;             /**< Feature detection threshold */
    float expected_rate;                    /**< Expected firing rate */
} feature_extractor_fep_effects_t;

/**
 * @brief Current state of feature extractor-FEP interaction
 */
typedef struct {
    /* FEP state */
    uint32_t fep_hierarchy_level;           /**< Current FEP hierarchy */
    float fep_precision;                    /**< Current precision */
    float fep_prediction;                   /**< Current prediction */

    /* Feature state */
    middleware_features_t current_features; /**< Last extracted features */
    float feature_entropy;                  /**< Entropy of features */
    float dominant_oscillation;             /**< Dominant oscillation band */

    /* Derived state */
    float inferred_precision;               /**< Precision from oscillations */
    float observation_uncertainty;          /**< Uncertainty from entropy */
} feature_extractor_fep_state_t;

/**
 * @brief Statistics for feature extractor-FEP bridge
 */
typedef struct {
    /* Feature extraction stats */
    uint64_t feature_extractions;           /**< Total extractions */
    uint64_t hierarchical_adjustments;      /**< Hierarchy level changes */
    float avg_features_extracted;           /**< Average feature count */

    /* Precision stats */
    uint64_t precision_updates;             /**< Precision adjustments */
    float avg_precision;                    /**< Average precision */
    float avg_entropy;                      /**< Average feature entropy */

    /* Oscillation stats */
    uint64_t oscillation_state_updates;     /**< Brain state updates */
    float avg_gamma_power;                  /**< Average gamma power */
    float avg_alpha_power;                  /**< Average alpha power */
} feature_extractor_fep_stats_t;

/**
 * @brief Feature extractor-FEP bridge state
 */
struct feature_extractor_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    feature_extractor_fep_config_t config;

    /* Connected systems */
    feature_extractor_t feature_extractor;  /**< Feature extractor */
    fep_system_t* fep_system;               /**< FEP system */

    /* Current effects and state */
    feature_extractor_fep_effects_t effects;
    feature_extractor_fep_state_t state;

    /* Statistics */
    feature_extractor_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default feature extractor-FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int feature_extractor_fep_bridge_default_config(
    feature_extractor_fep_config_t* config
);

/**
 * @brief Create feature extractor-FEP bridge
 *
 * WHAT: Initialize feature extractor-FEP integration bridge
 * WHY:  Enable bidirectional feature-FEP interaction
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
feature_extractor_fep_bridge_t* feature_extractor_fep_bridge_create(
    const feature_extractor_fep_config_t* config
);

/**
 * @brief Destroy feature extractor-FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void feature_extractor_fep_bridge_destroy(
    feature_extractor_fep_bridge_t* bridge
);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect feature extractor
 *
 * WHAT: Link bridge to feature extractor
 * WHY:  Enable feature extraction monitoring and control
 * HOW:  Store extractor pointer
 *
 * @param bridge Feature-FEP bridge
 * @param extractor Feature extractor
 * @return 0 on success
 */
int feature_extractor_fep_bridge_connect_extractor(
    feature_extractor_fep_bridge_t* bridge,
    feature_extractor_t extractor
);

/**
 * @brief Connect FEP system
 *
 * WHAT: Link bridge to FEP system
 * WHY:  Enable FEP state monitoring and feedback
 * HOW:  Store FEP system pointer
 *
 * @param bridge Feature-FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int feature_extractor_fep_bridge_connect_fep(
    feature_extractor_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Disconnect all systems
 *
 * WHAT: Unlink extractor and FEP systems
 * WHY:  Safe shutdown
 * HOW:  Clear system pointers
 *
 * @param bridge Feature-FEP bridge
 * @return 0 on success
 */
int feature_extractor_fep_bridge_disconnect(
    feature_extractor_fep_bridge_t* bridge
);

/* ============================================================================
 * FEP → Feature Extraction Direction
 * ============================================================================ */

/**
 * @brief Select features based on hierarchy level
 *
 * WHAT: Enable/disable feature sets based on FEP hierarchy
 * WHY:  Different levels require different feature granularity
 * HOW:  Map hierarchy to feature enable flags
 *
 * @param bridge Feature-FEP bridge
 * @param hierarchy_level FEP hierarchy level
 * @return 0 on success
 */
int feature_extractor_fep_select_hierarchical_features(
    feature_extractor_fep_bridge_t* bridge,
    uint32_t hierarchy_level
);

/**
 * @brief Gate feature extraction by precision
 *
 * WHAT: Adjust number of features based on FEP precision
 * WHY:  High precision requires detailed features
 * HOW:  Map precision to feature count
 *
 * @param bridge Feature-FEP bridge
 * @param precision Current FEP precision [0-1]
 * @return 0 on success
 */
int feature_extractor_fep_gate_by_precision(
    feature_extractor_fep_bridge_t* bridge,
    float precision
);

/**
 * @brief Set expected features from predictions
 *
 * WHAT: Configure extractor to prioritize expected features
 * WHY:  Efficient processing via prediction-confirmation
 * HOW:  Set expected feature values as reference
 *
 * @param bridge Feature-FEP bridge
 * @param expected_rate Expected firing rate (Hz)
 * @return 0 on success
 */
int feature_extractor_fep_set_expected_features(
    feature_extractor_fep_bridge_t* bridge,
    float expected_rate
);

/* ============================================================================
 * Feature Extraction → FEP Direction
 * ============================================================================ */

/**
 * @brief Report features as hierarchical observations
 *
 * WHAT: Convert extracted features to FEP observations
 * WHY:  Features are intermediate-level observations
 * HOW:  Map feature types to hierarchy levels
 *
 * @param bridge Feature-FEP bridge
 * @param features Extracted features
 * @return 0 on success
 */
int feature_extractor_fep_report_features(
    feature_extractor_fep_bridge_t* bridge,
    const middleware_features_t* features
);

/**
 * @brief Update FEP uncertainty from feature entropy
 *
 * WHAT: Derive observation uncertainty from feature entropy
 * WHY:  Entropy indicates information content/reliability
 * HOW:  Map entropy to precision inverse
 *
 * @param bridge Feature-FEP bridge
 * @param entropy Feature entropy (bits)
 * @return 0 on success
 */
int feature_extractor_fep_update_uncertainty_from_entropy(
    feature_extractor_fep_bridge_t* bridge,
    float entropy
);

/**
 * @brief Infer brain state from oscillations
 *
 * WHAT: Derive FEP mode from oscillation features
 * WHY:  Oscillations indicate brain state/precision mode
 * HOW:  Map gamma/beta/alpha to precision levels
 *
 * @param bridge Feature-FEP bridge
 * @param gamma_power Gamma band power
 * @param beta_power Beta band power
 * @param alpha_power Alpha band power
 * @return 0 on success
 */
int feature_extractor_fep_infer_state_from_oscillations(
    feature_extractor_fep_bridge_t* bridge,
    float gamma_power,
    float beta_power,
    float alpha_power
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update feature extractor-FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep feature extraction and FEP synchronized
 * HOW:  Update hierarchy, precision gating, and observations
 *
 * @param bridge Feature-FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int feature_extractor_fep_bridge_update(
    feature_extractor_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Feature-FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int feature_extractor_fep_bridge_get_state(
    const feature_extractor_fep_bridge_t* bridge,
    feature_extractor_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Feature-FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int feature_extractor_fep_bridge_get_stats(
    const feature_extractor_fep_bridge_t* bridge,
    feature_extractor_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for feature-FEP coordination
 * WHY:  Distributed feature observation signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge Feature-FEP bridge
 * @return 0 on success
 */
int feature_extractor_fep_bridge_connect_bio_async(
    feature_extractor_fep_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Feature-FEP bridge
 * @return 0 on success
 */
int feature_extractor_fep_bridge_disconnect_bio_async(
    feature_extractor_fep_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Feature-FEP bridge
 * @return true if bio-async enabled
 */
bool feature_extractor_fep_bridge_is_bio_async_connected(
    const feature_extractor_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEATURE_EXTRACTOR_FEP_BRIDGE_H */
