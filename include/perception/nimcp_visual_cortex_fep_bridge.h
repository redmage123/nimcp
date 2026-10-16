/**
 * @file nimcp_visual_cortex_fep_bridge.h
 * @brief Free Energy Principle - Visual Cortex Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and Visual Cortex
 * WHY:  Vision as hierarchical prediction: V1→V2→V4→IT implements predictive
 *       processing with bottom-up sensory signals and top-down predictions.
 * HOW:  FEP generates visual predictions to V1, visual prediction errors update beliefs,
 *       precision modulates visual attention, active inference drives eye movements.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * VISUAL FEP PATHWAYS:
 * --------------------
 * 1. Hierarchical Visual Prediction (Rao & Ballard 1999):
 *    - V1 → V2 → V4 → IT as hierarchical generative model
 *    - Higher areas predict lower area activity
 *    - Prediction errors propagate bottom-up
 *    - Reference: "Predictive coding in the visual cortex: a functional
 *      interpretation of some extra-classical receptive-field effects"
 *
 * 2. Attention as Precision Weighting (Feldman & Friston 2010):
 *    - Attention = precision-weighted prediction errors
 *    - High precision → strong influence on beliefs
 *    - Low precision → ignore unreliable signals
 *    - Salience map driven by precision-weighted PE
 *
 * 3. Eye Movements as Active Inference (Friston et al. 2012):
 *    - Saccades minimize expected free energy
 *    - Foveate high-uncertainty regions
 *    - Visual search as active inference
 *    - Reference: "Active inference and agency: optimal control without cost functions"
 *
 * 4. Visual Illusions as Prior Dominance:
 *    - Strong priors override weak sensory evidence
 *    - Hollow mask illusion: face prior dominates depth cues
 *    - Motion aftereffect: temporal prediction adaptation
 *
 * FEP → VISUAL CORTEX:
 * --------------------
 * 1. Top-down Predictions:
 *    - FEP higher levels generate visual predictions
 *    - Modulate V1 gain for expected features
 *    - Predictive suppression of expected stimuli
 *
 * 2. Precision Modulation:
 *    - High precision → enhanced visual processing
 *    - Low precision → suppress unreliable inputs
 *    - Attention spotlight via precision control
 *
 * 3. Active Vision:
 *    - Eye movements to minimize expected free energy
 *    - Saccade targets = high-uncertainty locations
 *    - Smooth pursuit tracks predicted motion
 *
 * VISUAL CORTEX → FEP:
 * --------------------
 * 1. Visual Prediction Errors:
 *    - Unexpected visual features → high PE
 *    - Update beliefs about visual scene
 *    - Novelty detection triggers learning
 *
 * 2. Visual Observations:
 *    - V1 features as sensory observations
 *    - Feed into FEP generative model
 *    - Drive belief updates
 *
 * 3. Salience Signals:
 *    - Attention map indicates high-PE regions
 *    - Guides active inference exploration
 *    - Updates expected uncertainty
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                 VISUAL CORTEX FEP BRIDGE                                   ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               FEP → VISUAL PATHWAYS                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   FEP Beliefs (μ) → Visual Predictions → V1 Gain Modulation        │  ║
 * ║   │   FEP Precision → Attention Map Boost → Salience Enhancement       │  ║
 * ║   │   Expected Free Energy → Saccade Target Selection                  │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │            VISUAL CORTEX → FEP PATHWAYS                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   V1 Features → Observations (o) → Prediction Error Computation    │  ║
 * ║   │   Gabor Responses → Sensory PE → Belief Updates                    │  ║
 * ║   │   Attention Map → High-PE Regions → Active Inference Targets       │  ║
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

#ifndef NIMCP_VISUAL_CORTEX_FEP_BRIDGE_H
#define NIMCP_VISUAL_CORTEX_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "perception/nimcp_visual_cortex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Prediction error thresholds for visual processing */
#define VISUAL_FEP_PE_THRESHOLD_LOW       1.0f    /**< Minor visual surprise */
#define VISUAL_FEP_PE_THRESHOLD_MEDIUM    3.0f    /**< Moderate visual surprise */
#define VISUAL_FEP_PE_THRESHOLD_HIGH      8.0f    /**< High visual surprise (novelty) */

/* Precision impact on visual gain */
#define VISUAL_FEP_PRECISION_GAIN_MIN     0.5f    /**< Minimum visual gain */
#define VISUAL_FEP_PRECISION_GAIN_MAX     2.0f    /**< Maximum visual gain */
#define VISUAL_FEP_PRECISION_GAIN_DEFAULT 1.0f    /**< Default visual gain */

/* Attention modulation factors */
#define VISUAL_FEP_ATTENTION_BOOST_LOW    1.1f    /**< Slight attention boost */
#define VISUAL_FEP_ATTENTION_BOOST_MEDIUM 1.5f    /**< Moderate attention boost */
#define VISUAL_FEP_ATTENTION_BOOST_HIGH   2.0f    /**< Strong attention boost */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct visual_cortex_fep_bridge visual_cortex_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for Visual Cortex FEP bridge
 */
typedef struct {
    /* Thresholds */
    float prediction_error_threshold;      /**< PE → visual learning trigger */
    float precision_gain_factor;           /**< Precision → visual gain scaling */
    float attention_boost_factor;          /**< Precision → attention boost */

    /* Feature enables */
    bool enable_top_down_predictions;      /**< FEP predictions → V1 modulation */
    bool enable_precision_attention;       /**< Precision → attention control */
    bool enable_active_vision;             /**< Active inference → saccades */
    bool enable_visual_pe_updates;         /**< Visual PE → belief updates */

    /* Sensitivity factors */
    float visual_precision_sensitivity;    /**< Visual precision effect scaling */
    float prediction_gain_sensitivity;     /**< Top-down prediction strength */
    float pe_propagation_rate;             /**< PE propagation speed */
} visual_cortex_fep_config_t;

/**
 * @brief FEP effects on visual processing
 */
typedef struct {
    /* Gain modulation */
    float gabor_gain;                      /**< Gabor filter gain from predictions */
    float attention_boost;                 /**< Attention map boost from precision */
    float feature_gain;                    /**< Feature extraction gain */

    /* Precision effects */
    float visual_precision;                /**< Current visual precision */
    float precision_gain_modifier;         /**< Precision → gain conversion */

    /* Prediction effects */
    float prediction_suppression;          /**< Suppress expected features */
    float novelty_enhancement;             /**< Enhance novel features */
} visual_cortex_fep_effects_t;

/**
 * @brief Current state of Visual-FEP interaction
 */
typedef struct {
    /* Visual prediction errors */
    float current_visual_pe;               /**< Current visual PE magnitude */
    float avg_visual_pe;                   /**< Average visual PE */
    float max_visual_pe;                   /**< Peak visual PE */

    /* Precision state */
    float visual_precision;                /**< Visual sensory precision */
    float attention_precision;             /**< Attention-weighted precision */

    /* Active inference */
    float saccade_target_x;                /**< Saccade target X (normalized) */
    float saccade_target_y;                /**< Saccade target Y (normalized) */
    float expected_info_gain;              /**< Expected information gain */

    /* Processing state */
    uint64_t frames_processed;             /**< Visual frames processed */
    uint64_t pe_events;                    /**< High PE events */
    bool novelty_detected;                 /**< Novel visual pattern detected */
} visual_cortex_fep_state_t;

/**
 * @brief Statistics for Visual FEP bridge
 */
typedef struct {
    /* Visual processing */
    uint64_t total_frames_processed;       /**< Total visual frames */
    uint64_t high_pe_events;               /**< High prediction error events */
    uint64_t novelty_events;               /**< Novelty detections */

    /* Prediction accuracy */
    float avg_prediction_error;            /**< Average PE magnitude */
    float prediction_accuracy;             /**< 1 - normalized PE */

    /* Active inference */
    uint64_t saccades_generated;           /**< Saccades from active inference */
    float avg_info_gain;                   /**< Average information gain */

    /* Modulation effects */
    float avg_precision_gain;              /**< Average precision gain */
    float avg_attention_boost;             /**< Average attention boost */
} visual_cortex_fep_stats_t;

/**
 * @brief Visual Cortex FEP bridge state
 */
struct visual_cortex_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    visual_cortex_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;              /**< FEP system */
    visual_cortex_t* visual_cortex;        /**< Visual cortex */

    /* Current effects */
    visual_cortex_fep_effects_t effects;
    visual_cortex_fep_state_t state;

    /* Statistics */
    visual_cortex_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default Visual Cortex FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int visual_cortex_fep_bridge_default_config(visual_cortex_fep_config_t* config);

/**
 * @brief Create Visual Cortex FEP bridge
 *
 * WHAT: Initialize Visual-FEP integration bridge
 * WHY:  Enable bidirectional visual-FEP interaction
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
visual_cortex_fep_bridge_t* visual_cortex_fep_bridge_create(
    const visual_cortex_fep_config_t* config
);

/**
 * @brief Destroy Visual Cortex FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void visual_cortex_fep_bridge_destroy(visual_cortex_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect FEP system
 *
 * WHAT: Link bridge to FEP system
 * WHY:  Enable FEP state monitoring and modulation
 * HOW:  Store FEP system pointer
 *
 * @param bridge Visual FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int visual_cortex_fep_bridge_connect_fep(
    visual_cortex_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect visual cortex
 *
 * WHAT: Link bridge to visual cortex
 * WHY:  Enable visual processing monitoring and modulation
 * HOW:  Store visual cortex pointer
 *
 * @param bridge Visual FEP bridge
 * @param visual Visual cortex system
 * @return 0 on success
 */
int visual_cortex_fep_bridge_connect_visual_cortex(
    visual_cortex_fep_bridge_t* bridge,
    visual_cortex_t* visual
);

/* ============================================================================
 * FEP → Visual Direction
 * ============================================================================ */

/**
 * @brief Apply FEP predictions to visual processing
 *
 * WHAT: Modulate V1 gain based on FEP predictions
 * WHY:  Top-down predictions enhance expected features
 * HOW:  Scale Gabor responses by prediction confidence
 *
 * @param bridge Visual FEP bridge
 * @return 0 on success
 */
int visual_cortex_fep_apply_predictions(visual_cortex_fep_bridge_t* bridge);

/**
 * @brief Apply precision to visual attention
 *
 * WHAT: Modulate attention map based on FEP precision
 * WHY:  High precision → enhanced attention
 * HOW:  Scale attention weights by precision
 *
 * @param bridge Visual FEP bridge
 * @return 0 on success
 */
int visual_cortex_fep_apply_precision(visual_cortex_fep_bridge_t* bridge);

/**
 * @brief Generate saccade target via active inference
 *
 * WHAT: Select saccade target to minimize expected free energy
 * WHY:  Foveate high-uncertainty regions
 * HOW:  Compute expected information gain across visual field
 *
 * @param bridge Visual FEP bridge
 * @param target_x Output saccade X (normalized 0-1)
 * @param target_y Output saccade Y (normalized 0-1)
 * @return 0 on success
 */
int visual_cortex_fep_generate_saccade(
    visual_cortex_fep_bridge_t* bridge,
    float* target_x,
    float* target_y
);

/* ============================================================================
 * Visual → FEP Direction
 * ============================================================================ */

/**
 * @brief Compute visual prediction error
 *
 * WHAT: Calculate PE from visual features vs FEP predictions
 * WHY:  Visual PE drives belief updates
 * HOW:  Compare V1 features to predicted features
 *
 * @param bridge Visual FEP bridge
 * @param visual_features V1 feature vector
 * @param num_features Feature dimension
 * @param prediction_error Output PE magnitude
 * @return 0 on success
 */
int visual_cortex_fep_compute_prediction_error(
    visual_cortex_fep_bridge_t* bridge,
    const float* visual_features,
    uint32_t num_features,
    float* prediction_error
);

/**
 * @brief Report visual observations to FEP
 *
 * WHAT: Feed V1 features as observations to FEP
 * WHY:  Visual input drives inference
 * HOW:  Convert features to FEP observation format
 *
 * @param bridge Visual FEP bridge
 * @param visual_features V1 feature vector
 * @param num_features Feature dimension
 * @return 0 on success
 */
int visual_cortex_fep_report_observations(
    visual_cortex_fep_bridge_t* bridge,
    const float* visual_features,
    uint32_t num_features
);

/**
 * @brief Report visual novelty to FEP
 *
 * WHAT: Signal novel visual patterns for learning
 * WHY:  Novelty triggers model updates
 * HOW:  Detect high PE and report to FEP
 *
 * @param bridge Visual FEP bridge
 * @param novelty_score Novelty magnitude [0-1]
 * @return 0 on success
 */
int visual_cortex_fep_report_novelty(
    visual_cortex_fep_bridge_t* bridge,
    float novelty_score
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update Visual-FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep visual and FEP systems synchronized
 * HOW:  Update predictions, compute PE, apply modulation
 *
 * @param bridge Visual FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int visual_cortex_fep_bridge_update(
    visual_cortex_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Visual FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int visual_cortex_fep_bridge_get_state(
    const visual_cortex_fep_bridge_t* bridge,
    visual_cortex_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Visual FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int visual_cortex_fep_bridge_get_stats(
    const visual_cortex_fep_bridge_t* bridge,
    visual_cortex_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for visual-FEP coordination
 * WHY:  Distributed visual prediction signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge Visual FEP bridge
 * @return 0 on success
 */
int visual_cortex_fep_bridge_connect_bio_async(
    visual_cortex_fep_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Visual FEP bridge
 * @return 0 on success
 */
int visual_cortex_fep_bridge_disconnect_bio_async(
    visual_cortex_fep_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Visual FEP bridge
 * @return true if bio-async enabled
 */
bool visual_cortex_fep_bridge_is_bio_async_connected(
    const visual_cortex_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VISUAL_CORTEX_FEP_BRIDGE_H */
