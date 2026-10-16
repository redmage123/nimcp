//=============================================================================
// nimcp_hemispheric_fep_bridge.h - Hemispheric Brain FEP Integration
//=============================================================================
/**
 * @file nimcp_hemispheric_fep_bridge.h
 * @brief Bidirectional integration between hemispheric brain and FEP system
 *
 * WHAT: Integration layer connecting hemispheric brain with Free Energy Principle
 * WHY:  Each hemisphere maintains its own predictive hierarchy; FEP modulates
 *       per-hemisphere learning, precision, and cross-hemisphere prediction sharing
 * HOW:  Asymmetric precision weighting, callosum-mediated prediction transfer,
 *       and joint free energy minimization
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * HEMISPHERIC PREDICTIVE PROCESSING:
 * -----------------------------------
 * 1. Left Hemisphere (Analytical):
 *    - High precision for sequential, linguistic predictions
 *    - Narrow priors (detail-focused, high confidence)
 *    - Prediction errors drive analytical refinement
 *
 * 2. Right Hemisphere (Holistic):
 *    - Broader priors for spatial, contextual predictions
 *    - More tolerant of ambiguity (lower precision)
 *    - Prediction errors drive creative reinterpretation
 *
 * CROSS-HEMISPHERE PREDICTIONS:
 * -----------------------------
 * - Callosum transmits prediction summaries between hemispheres
 * - Left sends linguistic/sequential predictions to right
 * - Right sends spatial/contextual predictions to left
 * - Integration creates unified perception while maintaining specialization
 *
 * FREE ENERGY PARTITIONING:
 * -------------------------
 * Total free energy = Left_FE + Right_FE + Integration_FE
 * - Left_FE: Minimized via analytical inference
 * - Right_FE: Minimized via holistic inference
 * - Integration_FE: Minimized via callosum-mediated consensus
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |                    HEMISPHERIC-FEP BRIDGE                               |
 * +=========================================================================+
 * |                                                                          |
 * |   +-------------------------------------------------------------------+  |
 * |   |                   LEFT HEMISPHERE FEP                             |  |
 * |   |                                                                    |  |
 * |   |   Precision: HIGH (analytical)                                    |  |
 * |   |   Prior Width: NARROW (confident)                                 |  |
 * |   |   Learning: Sequential, rule-based                                |  |
 * |   |   Specializations: Language, Logic, Math                          |  |
 * |   +-------------------------------------------------------------------+  |
 * |                               |                                          |
 * |                     +---------+---------+                                |
 * |                     | CALLOSUM TRANSFER |                                |
 * |                     | Prediction sharing |                               |
 * |                     +---------+---------+                                |
 * |                               |                                          |
 * |   +-------------------------------------------------------------------+  |
 * |   |                   RIGHT HEMISPHERE FEP                            |  |
 * |   |                                                                    |  |
 * |   |   Precision: MODERATE (flexible)                                  |  |
 * |   |   Prior Width: BROAD (exploratory)                                |  |
 * |   |   Learning: Holistic, pattern-based                               |  |
 * |   |   Specializations: Spatial, Emotion, Creativity                   |  |
 * |   +-------------------------------------------------------------------+  |
 * |                                                                          |
 * +=========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_HEMISPHERIC_FEP_BRIDGE_H
#define NIMCP_HEMISPHERIC_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "async/nimcp_bio_async.h"
#include "utils/thread/nimcp_thread.h"

// Forward declaration for FEP orchestrator (opaque)
typedef struct fep_orchestrator fep_orchestrator_t;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default precision values per hemisphere */
#define HEMI_FEP_LEFT_BASE_PRECISION     0.85f   // High precision (analytical)
#define HEMI_FEP_RIGHT_BASE_PRECISION    0.65f   // Moderate precision (holistic)

/** Prior width (inverse of precision confidence) */
#define HEMI_FEP_LEFT_PRIOR_WIDTH        0.3f    // Narrow priors
#define HEMI_FEP_RIGHT_PRIOR_WIDTH       0.7f    // Broad priors

/** Learning rate modulation by FEP */
#define HEMI_FEP_MIN_LEARNING_RATE       0.01f
#define HEMI_FEP_MAX_LEARNING_RATE       1.0f

/** Callosum prediction transfer rates */
#define HEMI_FEP_CALLOSUM_TRANSFER_RATE  0.3f    // Fraction of predictions shared
#define HEMI_FEP_CALLOSUM_LATENCY_MS     10.0f   // Transfer latency

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Per-hemisphere FEP effects
 */
typedef struct {
    float precision;              /**< Current precision weight (0.0-1.0) */
    float prior_width;            /**< Width of prior distribution */
    float free_energy;            /**< Current free energy estimate */
    float prediction_error;       /**< Latest prediction error magnitude */
    float learning_rate_factor;   /**< LR multiplier from FEP (0.0-1.0) */
    float confidence;             /**< Overall prediction confidence */
} hemisphere_fep_effects_t;

/**
 * @brief Callosum FEP effects for prediction transfer
 */
typedef struct {
    float transfer_rate;          /**< Rate of prediction sharing */
    float integration_free_energy; /**< FE from cross-hemisphere integration */
    float consensus_strength;     /**< Strength of inter-hemispheric agreement */
    bool transfer_active;         /**< Whether transfer is occurring */
} callosum_fep_effects_t;

/**
 * @brief Global FEP state across hemispheres
 */
typedef struct {
    float total_free_energy;      /**< Combined free energy (L + R + integration) */
    float left_contribution;      /**< Left hemisphere FE fraction */
    float right_contribution;     /**< Right hemisphere FE fraction */
    float integration_contribution; /**< Callosum integration FE fraction */
    bool is_minimizing;           /**< Active free energy minimization */
} global_fep_state_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    // Precision settings
    float left_base_precision;    /**< Base precision for left hemisphere */
    float right_base_precision;   /**< Base precision for right hemisphere */

    // Prior width settings
    float left_prior_width;       /**< Prior width for left hemisphere */
    float right_prior_width;      /**< Prior width for right hemisphere */

    // Learning rate bounds
    float min_learning_rate;      /**< Minimum learning rate */
    float max_learning_rate;      /**< Maximum learning rate */

    // Callosum settings
    float callosum_transfer_rate; /**< Prediction transfer rate */
    float callosum_latency_ms;    /**< Transfer latency in ms */

    // FEP behavior
    bool enable_precision_modulation;  /**< Allow dynamic precision */
    bool enable_learning_modulation;   /**< Allow LR adjustment from FEP */
    bool enable_callosum_transfer;     /**< Enable prediction sharing */

    // Bio-async settings
    bool enable_bio_async;        /**< Enable bio-async messaging */
} hemispheric_fep_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t updates;              /**< Total update calls */
    uint64_t prediction_transfers; /**< Cross-hemisphere transfers */
    float avg_free_energy;         /**< Average total free energy */
    float peak_free_energy;        /**< Peak total free energy */
    float min_free_energy;         /**< Minimum total free energy achieved */
    float avg_left_precision;      /**< Average left precision */
    float avg_right_precision;     /**< Average right precision */
    uint64_t fe_minimization_steps; /**< Free energy minimization iterations */
} hemispheric_fep_stats_t;

/**
 * @brief Hemispheric FEP bridge structure
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    // Connected systems
    hemispheric_brain_t* brain;       /**< Hemispheric brain */
    fep_orchestrator_t* fep_system;   /**< FEP orchestrator (optional) */

    // Configuration
    hemispheric_fep_config_t config;

    // Current effects
    hemisphere_fep_effects_t left_effects;
    hemisphere_fep_effects_t right_effects;
    callosum_fep_effects_t callosum_effects;
    global_fep_state_t global_state;

    // Statistics
    hemispheric_fep_stats_t stats;

    // Bio-async
    // Thread safety
    // State
    bool initialized;
} hemispheric_fep_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default bridge configuration
 */
hemispheric_fep_config_t hemispheric_fep_default_config(void);

/**
 * @brief Create hemispheric FEP bridge
 *
 * @param config Bridge configuration
 * @param brain Hemispheric brain to connect
 * @param fep FEP orchestrator to connect (optional, can be NULL)
 * @return Bridge instance or NULL on failure
 */
hemispheric_fep_bridge_t* hemispheric_fep_create(
    const hemispheric_fep_config_t* config,
    hemispheric_brain_t* brain,
    fep_orchestrator_t* fep
);

/**
 * @brief Destroy hemispheric FEP bridge
 */
void hemispheric_fep_destroy(hemispheric_fep_bridge_t* bridge);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state from FEP system
 *
 * @param bridge Bridge instance
 * @return 0 on success, negative on error
 */
int hemispheric_fep_update(hemispheric_fep_bridge_t* bridge);

/**
 * @brief Apply computed effects to hemispheric brain
 *
 * @param bridge Bridge instance
 * @return 0 on success, negative on error
 */
int hemispheric_fep_apply_modulation(hemispheric_fep_bridge_t* bridge);

/**
 * @brief Perform one step of free energy minimization
 *
 * @param bridge Bridge instance
 * @param dt Time step in seconds
 * @return 0 on success, negative on error
 */
int hemispheric_fep_minimize_step(hemispheric_fep_bridge_t* bridge, float dt);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current left hemisphere FEP effects
 */
hemisphere_fep_effects_t hemispheric_fep_get_left_effects(
    const hemispheric_fep_bridge_t* bridge
);

/**
 * @brief Get current right hemisphere FEP effects
 */
hemisphere_fep_effects_t hemispheric_fep_get_right_effects(
    const hemispheric_fep_bridge_t* bridge
);

/**
 * @brief Get current callosum FEP effects
 */
callosum_fep_effects_t hemispheric_fep_get_callosum_effects(
    const hemispheric_fep_bridge_t* bridge
);

/**
 * @brief Get global FEP state
 */
global_fep_state_t hemispheric_fep_get_global_state(
    const hemispheric_fep_bridge_t* bridge
);

/**
 * @brief Get precision for hemisphere
 *
 * @param bridge Bridge instance
 * @param hemisphere Which hemisphere
 * @return Precision value (0.0-1.0)
 */
float hemispheric_fep_get_precision(
    const hemispheric_fep_bridge_t* bridge,
    hemisphere_id_t hemisphere
);

/**
 * @brief Get free energy for hemisphere
 *
 * @param bridge Bridge instance
 * @param hemisphere Which hemisphere
 * @return Free energy value
 */
float hemispheric_fep_get_free_energy(
    const hemispheric_fep_bridge_t* bridge,
    hemisphere_id_t hemisphere
);

/**
 * @brief Get total free energy across both hemispheres
 */
float hemispheric_fep_get_total_free_energy(
    const hemispheric_fep_bridge_t* bridge
);

//=============================================================================
// Control API
//=============================================================================

/**
 * @brief Set precision for hemisphere
 *
 * @param bridge Bridge instance
 * @param hemisphere Which hemisphere
 * @param precision New precision value (0.0-1.0)
 * @return 0 on success, negative on error
 */
int hemispheric_fep_set_precision(
    hemispheric_fep_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    float precision
);

/**
 * @brief Inject prediction error to hemisphere
 *
 * WHAT: Simulate receiving sensory prediction error
 * WHY:  Testing, external input integration
 *
 * @param bridge Bridge instance
 * @param hemisphere Target hemisphere
 * @param error_magnitude Magnitude of prediction error
 * @return 0 on success, negative on error
 */
int hemispheric_fep_inject_prediction_error(
    hemispheric_fep_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    float error_magnitude
);

/**
 * @brief Trigger cross-hemisphere prediction transfer
 *
 * WHAT: Force immediate prediction sharing via callosum
 * WHY:  Simulate attention-driven integration
 */
int hemispheric_fep_trigger_transfer(hemispheric_fep_bridge_t* bridge);

/**
 * @brief Reset free energy to baseline
 */
int hemispheric_fep_reset_free_energy(hemispheric_fep_bridge_t* bridge);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 */
hemispheric_fep_stats_t hemispheric_fep_get_stats(
    const hemispheric_fep_bridge_t* bridge
);

/**
 * @brief Reset statistics
 */
void hemispheric_fep_reset_stats(hemispheric_fep_bridge_t* bridge);

//=============================================================================
// Bio-async API
//=============================================================================

/**
 * @brief Connect to bio-async router
 */
int hemispheric_fep_connect_bio_async(hemispheric_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int hemispheric_fep_disconnect_bio_async(hemispheric_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_HEMISPHERIC_FEP_BRIDGE_H
