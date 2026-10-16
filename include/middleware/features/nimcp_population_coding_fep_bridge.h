/**
 * @file nimcp_population_coding_fep_bridge.h
 * @brief Free Energy Principle - Population Coding Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and population coding
 * WHY:  Population codes represent observation likelihoods in FEP framework; FEP precision
 *       modulates population tuning and decoding weights
 * HOW:  FEP precision → population tuning width; population codes → observation likelihood;
 *       prediction errors modulate synchrony thresholds
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * FEP → POPULATION CODING PATHWAYS:
 * ----------------------------------
 * 1. Precision-Weighted Likelihood Encoding:
 *    - High precision → narrower tuning curves (sharper population codes)
 *    - Low precision → broader tuning (more uncertainty)
 *    - Bayesian optimal cue integration via population variance
 *    - Reference: Ma et al. (2006) "Bayesian inference with probabilistic population codes"
 *
 * 2. Attention Modulation of Population Tuning:
 *    - FEP precision gates → attention-like gain modulation
 *    - Sharpens tuning curves for attended features
 *    - Reduces population noise correlations
 *    - Reference: Cohen & Maunsell (2009) "Attention improves performance primarily by
 *      reducing interneuronal correlations"
 *
 * 3. Prediction-Driven Baseline Activity:
 *    - FEP predictions set baseline population activity
 *    - Expected stimuli pre-activate corresponding population codes
 *    - Reduces latency and enhances detection
 *    - Reference: Bubic et al. (2010) "Prediction, cognition and the brain"
 *
 * POPULATION CODING → FEP PATHWAYS:
 * ----------------------------------
 * 1. Population Codes as Observation Likelihoods:
 *    - Population vector → sensory observation in FEP
 *    - Population variance → observation uncertainty
 *    - Likelihood function from population tuning curves
 *    - Reference: Pouget et al. (2013) "Probabilistic brains: knowns and unknowns"
 *
 * 2. Synchrony as Confidence Signal:
 *    - High population synchrony → high confidence (high precision)
 *    - Desynchronized population → low confidence
 *    - Synchrony feeds back to FEP precision estimates
 *    - Reference: Fries (2015) "Rhythms for cognition: communication through coherence"
 *
 * 3. Sparse Codes as Efficient Representations:
 *    - Sparse population codes minimize free energy
 *    - Energy-efficient encoding (minimize metabolic cost)
 *    - High-dimensional sparse codes reduce overlap (clear beliefs)
 *    - Reference: Olshausen & Field (2004) "Sparse coding of sensory inputs"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              FEP-POPULATION CODING BRIDGE                                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │             FEP → POPULATION CODING PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │  FEP PRECISION   │                                             │  ║
 * ║   │   │  ──────────────  │                                             │  ║
 * ║   │   │  High: 0.9       │ ──→ Tuning Width: Narrow                    │  ║
 * ║   │   │  Med:  0.5       │ ──→ Tuning Width: Medium                    │  ║
 * ║   │   │  Low:  0.2       │ ──→ Tuning Width: Broad                     │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │  FEP PREDICTIONS │ ──→ Baseline Population Activity            │  ║
 * ║   │   │  Expected: 0.7   │ ──→ Pre-activate 70% of population          │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │  PRED ERRORS     │ ──→ Synchrony Threshold Adjustment          │  ║
 * ║   │   │  High PE → ↑ θ   │ ──→ Require stronger synchrony             │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │         POPULATION CODING → FEP PATHWAYS                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────┐                                         │  ║
 * ║   │   │ POPULATION VECTOR    │ ──→ FEP Observation                     │  ║
 * ║   │   │ Vector: [0.7,0.3,0] │ ──→ Sensory input                       │  ║
 * ║   │   └──────────────────────┘                                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────┐                                         │  ║
 * ║   │   │ SYNCHRONY INDEX      │ ──→ FEP Precision Update                │  ║
 * ║   │   │ High sync → 0.9      │ ──→ High confidence                     │  ║
 * ║   │   │ Low sync  → 0.3      │ ──→ Low confidence                      │  ║
 * ║   │   └──────────────────────┘                                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────┐                                         │  ║
 * ║   │   │ SPARSITY METRIC      │ ──→ Free Energy Contribution            │  ║
 * ║   │   │ Sparse code → -ΔF    │ ──→ Energy efficiency bonus             │  ║
 * ║   │   └──────────────────────┘                                         │  ║
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

#ifndef NIMCP_POPULATION_CODING_FEP_BRIDGE_H
#define NIMCP_POPULATION_CODING_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "middleware/encoding/nimcp_population_coding.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Precision-tuning width mapping */
#define FEP_PRECISION_HIGH_TUNING_WIDTH     0.3f   /**< Narrow tuning (high precision) */
#define FEP_PRECISION_MED_TUNING_WIDTH      0.6f   /**< Medium tuning (medium precision) */
#define FEP_PRECISION_LOW_TUNING_WIDTH      1.0f   /**< Broad tuning (low precision) */

/* Synchrony-confidence mapping */
#define SYNCHRONY_HIGH_CONFIDENCE_THRESHOLD  0.7f   /**< High sync → high confidence */
#define SYNCHRONY_MED_CONFIDENCE_THRESHOLD   0.4f   /**< Medium sync → med confidence */
#define SYNCHRONY_LOW_CONFIDENCE_THRESHOLD   0.2f   /**< Low sync → low confidence */

/* Sparsity targets */
#define OPTIMAL_SPARSITY_TARGET             0.1f    /**< 10% active (energy efficient) */
#define SPARSITY_FREE_ENERGY_BONUS          -0.5f   /**< Free energy reduction for sparse codes */

/* Prediction baseline activation */
#define MAX_BASELINE_ACTIVATION             0.8f    /**< Max prediction-driven baseline */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct population_coding_fep_bridge population_coding_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for population coding-FEP bridge
 */
typedef struct {
    /* Feature enables */
    bool enable_precision_tuning;           /**< Precision modulates tuning width */
    bool enable_prediction_baseline;        /**< Predictions set baseline activity */
    bool enable_synchrony_confidence;       /**< Synchrony → precision feedback */
    bool enable_sparsity_optimization;      /**< Optimize for sparse codes */

    /* Sensitivity factors */
    float tuning_sensitivity;               /**< Precision → tuning width scaling */
    float baseline_sensitivity;             /**< Prediction → baseline scaling */
    float synchrony_sensitivity;            /**< Synchrony → confidence scaling */
    float sparsity_weight;                  /**< Sparsity impact on free energy */

    /* Thresholds */
    float min_tuning_width;                 /**< Minimum tuning width */
    float max_tuning_width;                 /**< Maximum tuning width */
    float min_synchrony_threshold;          /**< Minimum synchrony for confidence */
} population_coding_fep_config_t;

/**
 * @brief FEP effects on population coding
 */
typedef struct {
    /* Tuning modulation */
    float tuning_width_modifier;            /**< Current tuning width scaling */
    float baseline_activation;              /**< Prediction-driven baseline [0-1] */
    float synchrony_threshold;              /**< Current synchrony detection threshold */

    /* Precision effects */
    float precision_gain;                   /**< Population response gain */
    float noise_correlation_reduction;      /**< Reduced noise correlations */
} population_coding_fep_effects_t;

/**
 * @brief Current state of population coding-FEP interaction
 */
typedef struct {
    /* FEP state */
    float current_precision;                /**< Current FEP precision [0-1] */
    float current_prediction;               /**< Current prediction magnitude */
    float prediction_error;                 /**< Current prediction error */

    /* Population state */
    float population_synchrony;             /**< Current synchrony index */
    float population_sparsity;              /**< Current sparsity level */
    vector3d_t population_vector;           /**< Current population vector */

    /* Applied effects */
    float effective_tuning_width;           /**< Current tuning width */
    float effective_baseline;               /**< Current baseline activity */
} population_coding_fep_state_t;

/**
 * @brief Statistics for population coding-FEP bridge
 */
typedef struct {
    /* Tuning modulation stats */
    uint64_t tuning_adjustments;            /**< Times tuning width adjusted */
    float avg_tuning_width;                 /**< Average tuning width */
    float avg_precision;                    /**< Average precision */

    /* Baseline activation stats */
    uint64_t baseline_activations;          /**< Times baseline set by predictions */
    float avg_baseline_level;               /**< Average baseline activation */

    /* Synchrony-confidence stats */
    uint64_t synchrony_updates;             /**< Synchrony → confidence updates */
    float avg_synchrony;                    /**< Average synchrony index */
    float avg_confidence;                   /**< Average derived confidence */

    /* Sparsity optimization */
    float avg_sparsity;                     /**< Average population sparsity */
    float free_energy_savings;              /**< Cumulative FE savings from sparsity */
} population_coding_fep_stats_t;

/**
 * @brief Population coding-FEP bridge state
 */
struct population_coding_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    population_coding_fep_config_t config;

    /* Connected systems */
    population_coding_encoder_t population_encoder;  /**< Population coding encoder */
    fep_system_t* fep_system;                        /**< FEP system */

    /* Current effects and state */
    population_coding_fep_effects_t effects;
    population_coding_fep_state_t state;

    /* Statistics */
    population_coding_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default population coding-FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int population_coding_fep_bridge_default_config(population_coding_fep_config_t* config);

/**
 * @brief Create population coding-FEP bridge
 *
 * WHAT: Initialize population coding-FEP integration bridge
 * WHY:  Enable bidirectional population-FEP interaction
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
population_coding_fep_bridge_t* population_coding_fep_bridge_create(
    const population_coding_fep_config_t* config
);

/**
 * @brief Destroy population coding-FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void population_coding_fep_bridge_destroy(population_coding_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect population coding encoder
 *
 * WHAT: Link bridge to population encoder
 * WHY:  Enable population state monitoring and modulation
 * HOW:  Store encoder pointer
 *
 * @param bridge Population-FEP bridge
 * @param encoder Population coding encoder
 * @return 0 on success
 */
int population_coding_fep_bridge_connect_encoder(
    population_coding_fep_bridge_t* bridge,
    population_coding_encoder_t encoder
);

/**
 * @brief Connect FEP system
 *
 * WHAT: Link bridge to FEP system
 * WHY:  Enable FEP state monitoring and feedback
 * HOW:  Store FEP system pointer
 *
 * @param bridge Population-FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int population_coding_fep_bridge_connect_fep(
    population_coding_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Disconnect all systems
 *
 * WHAT: Unlink encoder and FEP systems
 * WHY:  Safe shutdown
 * HOW:  Clear system pointers
 *
 * @param bridge Population-FEP bridge
 * @return 0 on success
 */
int population_coding_fep_bridge_disconnect(population_coding_fep_bridge_t* bridge);

/* ============================================================================
 * FEP → Population Coding Direction
 * ============================================================================ */

/**
 * @brief Apply precision to tuning width
 *
 * WHAT: Modulate population tuning curves based on FEP precision
 * WHY:  High precision requires sharper population codes
 * HOW:  Scale tuning width inversely with precision
 *
 * @param bridge Population-FEP bridge
 * @param precision Current FEP precision [0-1]
 * @return 0 on success
 */
int population_coding_fep_apply_precision_tuning(
    population_coding_fep_bridge_t* bridge,
    float precision
);

/**
 * @brief Set baseline activation from predictions
 *
 * WHAT: Pre-activate population based on FEP predictions
 * WHY:  Expected stimuli should prepare population response
 * HOW:  Scale baseline by prediction magnitude
 *
 * @param bridge Population-FEP bridge
 * @param prediction Prediction magnitude [0-1]
 * @return 0 on success
 */
int population_coding_fep_set_baseline(
    population_coding_fep_bridge_t* bridge,
    float prediction
);

/**
 * @brief Adjust synchrony threshold based on prediction error
 *
 * WHAT: Modulate synchrony detection threshold
 * WHY:  High PE requires stronger synchrony for confidence
 * HOW:  Increase threshold with PE magnitude
 *
 * @param bridge Population-FEP bridge
 * @param prediction_error Current prediction error
 * @return 0 on success
 */
int population_coding_fep_adjust_synchrony_threshold(
    population_coding_fep_bridge_t* bridge,
    float prediction_error
);

/* ============================================================================
 * Population Coding → FEP Direction
 * ============================================================================ */

/**
 * @brief Report population vector as observation
 *
 * WHAT: Convert population vector to FEP observation
 * WHY:  Population codes encode sensory observations
 * HOW:  Extract vector, convert to FEP observation format
 *
 * @param bridge Population-FEP bridge
 * @param vector Population vector
 * @return 0 on success
 */
int population_coding_fep_report_observation(
    population_coding_fep_bridge_t* bridge,
    const vector3d_t* vector
);

/**
 * @brief Update FEP precision from synchrony
 *
 * WHAT: Derive FEP precision from population synchrony
 * WHY:  Synchrony indicates observation confidence
 * HOW:  Map synchrony index to precision value
 *
 * @param bridge Population-FEP bridge
 * @param synchrony Current synchrony index [0-1]
 * @return 0 on success
 */
int population_coding_fep_update_precision_from_synchrony(
    population_coding_fep_bridge_t* bridge,
    float synchrony
);

/**
 * @brief Report sparsity for free energy optimization
 *
 * WHAT: Inform FEP of population sparsity level
 * WHY:  Sparse codes minimize metabolic cost (part of free energy)
 * HOW:  Compute sparsity bonus to free energy
 *
 * @param bridge Population-FEP bridge
 * @param sparsity Current sparsity level [0-1]
 * @return 0 on success
 */
int population_coding_fep_report_sparsity(
    population_coding_fep_bridge_t* bridge,
    float sparsity
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update population coding-FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep population and FEP systems synchronized
 * HOW:  Update tuning, baseline, synchrony, and sparsity
 *
 * @param bridge Population-FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int population_coding_fep_bridge_update(
    population_coding_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Population-FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int population_coding_fep_bridge_get_state(
    const population_coding_fep_bridge_t* bridge,
    population_coding_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Population-FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int population_coding_fep_bridge_get_stats(
    const population_coding_fep_bridge_t* bridge,
    population_coding_fep_stats_t* stats
);

/**
 * @brief Get current effective tuning width
 *
 * @param bridge Population-FEP bridge
 * @param tuning_width Output tuning width
 * @return 0 on success
 */
int population_coding_fep_get_tuning_width(
    const population_coding_fep_bridge_t* bridge,
    float* tuning_width
);

/**
 * @brief Get current baseline activation
 *
 * @param bridge Population-FEP bridge
 * @param baseline Output baseline level
 * @return 0 on success
 */
int population_coding_fep_get_baseline(
    const population_coding_fep_bridge_t* bridge,
    float* baseline
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for population-FEP coordination
 * WHY:  Distributed precision and observation signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge Population-FEP bridge
 * @return 0 on success
 */
int population_coding_fep_bridge_connect_bio_async(
    population_coding_fep_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Population-FEP bridge
 * @return 0 on success
 */
int population_coding_fep_bridge_disconnect_bio_async(
    population_coding_fep_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Population-FEP bridge
 * @return true if bio-async enabled
 */
bool population_coding_fep_bridge_is_bio_async_connected(
    const population_coding_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_POPULATION_CODING_FEP_BRIDGE_H */
