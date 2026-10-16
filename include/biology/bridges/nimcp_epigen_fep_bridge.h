/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_epigen_fep_bridge.h - Epigenetics to Free Energy Principle Bridge
//=============================================================================
/**
 * @file nimcp_epigen_fep_bridge.h
 * @brief Bridge between Epigenetics and Free Energy Principle
 *
 * WHAT: Connects epigenetic modifications to Free Energy Principle (FEP)
 *       computations, enabling long-term adaptation of predictive models
 *       through gene expression-mediated model updates.
 *
 * WHY:  Bridges the gap between:
 *       - FEP inference (active inference, prediction error minimization)
 *       - Epigenetic memory (persistent structural changes)
 *       - Model adaptation (long-term prior updates)
 *
 * HOW:  Two-way integration:
 *       1. Epigenetics -> FEP: Methylation affects model precision
 *       2. FEP -> Epigenetics: Persistent prediction errors trigger marks
 *       3. Chromatin state -> Prior flexibility
 *       4. Gene expression -> Model structure updates
 *
 * BIOLOGICAL BASIS:
 * ```
 * EPIGENETICS                           FEP EFFECTS
 * ---------------------------------------------------------------------------
 * DNA methylation                    -> Prior precision (confidence)
 * Histone modifications              -> Learning rate (update speed)
 * Chromatin accessibility            -> Model flexibility
 * Critical period chromatin          -> Enhanced prior updates
 * Persistent prediction error       <- Triggers epigenetic marks
 * Model structure changes           <- Gene expression patterns
 * ```
 *
 * FEP-EPIGENETICS COUPLING:
 * - Methylated priors: High precision, resistant to update
 * - Open chromatin: Flexible priors, rapid adaptation
 * - Chronic prediction error: Triggers demethylation
 * - Successful prediction: Reinforces methylation
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_EPIGEN_FEP_BRIDGE_H
#define NIMCP_EPIGEN_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define EPIGEN_FEP_MODULE_NAME          "epigen_fep_bridge"

/** Maximum tracked priors for epigenetic effects */
#define EPIGEN_FEP_MAX_PRIORS           512

/** Maximum prediction error accumulation events */
#define EPIGEN_FEP_MAX_ERROR_EVENTS     256

/** Maximum model update events per cycle */
#define EPIGEN_FEP_MAX_MODEL_UPDATES    128

/** Prediction error threshold for epigenetic trigger (nats) */
#define EPIGEN_FEP_ERROR_THRESHOLD      2.0f

/** Chronic error duration for trigger (ms) */
#define EPIGEN_FEP_CHRONIC_DURATION_MS  60000.0f

/** Default methylation effect on precision */
#define EPIGEN_FEP_METHYL_PRECISION     2.0f

/** Default acetylation effect on learning rate */
#define EPIGEN_FEP_ACETYL_LEARNING      1.5f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Prior adaptation state
 */
typedef enum {
    EPIGEN_FEP_PRIOR_FLEXIBLE = 0,   /**< Easily updated (open chromatin) */
    EPIGEN_FEP_PRIOR_STABLE,         /**< Moderate stability */
    EPIGEN_FEP_PRIOR_RIGID,          /**< Resistant to change (methylated) */
    EPIGEN_FEP_PRIOR_LOCKED          /**< Permanently fixed */
} epigen_fep_prior_state_t;

/**
 * @brief Prediction error response
 */
typedef enum {
    EPIGEN_FEP_ERROR_TRANSIENT = 0,  /**< Brief error, no epigenetic change */
    EPIGEN_FEP_ERROR_PERSISTENT,     /**< Ongoing error, may trigger change */
    EPIGEN_FEP_ERROR_CHRONIC,        /**< Long-term error, triggers demethyl */
    EPIGEN_FEP_ERROR_RESOLVED        /**< Error resolved, may lock prior */
} epigen_fep_error_type_t;

/**
 * @brief Model structure change type
 */
typedef enum {
    EPIGEN_FEP_MODEL_EXPAND = 0,     /**< Add new model components */
    EPIGEN_FEP_MODEL_PRUNE,          /**< Remove unused components */
    EPIGEN_FEP_MODEL_RESTRUCTURE,    /**< Reorganize model hierarchy */
    EPIGEN_FEP_MODEL_CONSOLIDATE     /**< Lock successful predictions */
} epigen_fep_model_change_t;

/**
 * @brief Epigenetic trigger for FEP
 */
typedef enum {
    EPIGEN_FEP_TRIGGER_METHYLATION = 0, /**< Increase prior precision */
    EPIGEN_FEP_TRIGGER_DEMETHYLATION,   /**< Decrease prior precision */
    EPIGEN_FEP_TRIGGER_ACETYLATION,     /**< Boost learning rate */
    EPIGEN_FEP_TRIGGER_DEACETYLATION    /**< Reduce learning rate */
} epigen_fep_trigger_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for epigenetics-FEP bridge
 */
typedef struct {
    /** Prior precision modulation */
    float methylation_precision_factor;  /**< Precision boost per methylation */
    float demethylation_precision_factor;/**< Precision reduction on demethyl */
    float precision_range_min;           /**< Minimum allowed precision */
    float precision_range_max;           /**< Maximum allowed precision */

    /** Learning rate modulation */
    float acetylation_learning_boost;    /**< Learning rate boost */
    float deacetylation_learning_reduce; /**< Learning rate reduction */
    float base_learning_rate;            /**< Default learning rate */

    /** Prediction error integration */
    float error_threshold_nats;          /**< Error threshold for trigger */
    float chronic_error_duration_ms;     /**< Duration for chronic classification */
    float error_integration_tau_ms;      /**< Time constant for error smoothing */
    bool enable_error_feedback;          /**< Error triggers epigenetic change */

    /** Model structure adaptation */
    bool enable_structure_changes;       /**< Allow model restructuring */
    float expansion_threshold;           /**< Error level for expansion */
    float pruning_threshold;             /**< Inactivity level for pruning */
    float consolidation_threshold;       /**< Success level for consolidation */

    /** Critical period effects */
    float critical_period_flexibility;   /**< Prior flexibility in critical period */
    float critical_period_learning;      /**< Learning rate in critical period */
    bool enable_critical_periods;        /**< Enable critical period effects */

    /** Update parameters */
    float update_interval_ms;
    bool enable_logging;
    bool enable_metrics;
} epigen_fep_config_t;

/**
 * @brief Prior precision state
 */
typedef struct {
    uint32_t prior_id;                   /**< Prior identifier */
    epigen_fep_prior_state_t state;      /**< Current adaptation state */
    float base_precision;                /**< Baseline precision */
    float epigen_precision_modifier;     /**< Epigenetic modifier */
    float effective_precision;           /**< Final precision value */
    float methylation_level;             /**< Current methylation */
    float acetylation_level;             /**< Current acetylation */
    bool is_locked;                      /**< Prior permanently locked */
} epigen_fep_prior_t;

/**
 * @brief Prediction error accumulation
 */
typedef struct {
    uint32_t prior_id;                   /**< Affected prior */
    float instantaneous_error;           /**< Current prediction error */
    float integrated_error;              /**< Time-integrated error */
    float duration_ms;                   /**< Duration of elevated error */
    epigen_fep_error_type_t error_type;  /**< Classified error type */
    bool trigger_pending;                /**< Epigenetic trigger pending */
    epigen_fep_trigger_t pending_trigger;/**< What trigger to apply */
} epigen_fep_error_state_t;

/**
 * @brief Model structure update event
 */
typedef struct {
    uint32_t model_region_id;            /**< Affected model region */
    epigen_fep_model_change_t change_type;/**< Type of change */
    uint32_t components_affected;        /**< Number of components */
    float trigger_error_level;           /**< Error level that triggered */
    float update_time_ms;                /**< When update occurred */
    bool gene_expression_required;       /**< Required gene expression */
} epigen_fep_model_update_t;

/**
 * @brief Learning rate state
 */
typedef struct {
    uint32_t region_id;                  /**< Neural region */
    float base_learning_rate;            /**< Baseline rate */
    float epigen_learning_modifier;      /**< Epigenetic modifier */
    float effective_learning_rate;       /**< Final learning rate */
    float histone_acetylation;           /**< Current acetylation level */
    bool in_critical_period;             /**< In critical period */
} epigen_fep_learning_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t precision_modulations;      /**< Precision changes applied */
    uint64_t learning_rate_modulations;  /**< Learning rate changes */
    uint64_t error_triggers;             /**< Errors triggering epigen change */
    uint64_t model_expansions;           /**< Model expansion events */
    uint64_t model_consolidations;       /**< Model consolidation events */
    uint64_t priors_locked;              /**< Priors permanently locked */
    float avg_prediction_error;          /**< Average prediction error */
    float avg_precision_modifier;        /**< Average precision modifier */
    float last_update_ms;                /**< Last update timestamp */
} epigen_fep_stats_t;

/** Opaque bridge handle */
typedef struct epigen_fep_bridge_struct epigen_fep_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_fep_default_config(epigen_fep_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create epigenetics-FEP bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT epigen_fep_bridge_t* epigen_fep_bridge_create(
    const epigen_fep_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void epigen_fep_bridge_destroy(epigen_fep_bridge_t* bridge);

//=============================================================================
// Prior Precision API (Epigenetics -> FEP)
//=============================================================================

/**
 * @brief Set epigenetic state for prior
 *
 * WHAT: Updates prior precision based on epigenetic marks
 * WHY:  Methylation increases confidence in priors
 * HOW:  Scales precision by methylation level
 *
 * @param bridge Bridge handle
 * @param prior_id Prior to modify
 * @param methylation_level Methylation (0-1)
 * @param acetylation_level Acetylation (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_fep_set_prior_state(
    epigen_fep_bridge_t* bridge,
    uint32_t prior_id,
    float methylation_level,
    float acetylation_level
);

/**
 * @brief Get precision modifier for prior
 *
 * WHAT: Returns epigenetic precision modifier
 * WHY:  FEP needs to scale prior precision
 * HOW:  Based on methylation state
 *
 * @param bridge Bridge handle
 * @param prior_id Prior to query
 * @param precision_modifier Output modifier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_fep_get_precision_modifier(
    epigen_fep_bridge_t* bridge,
    uint32_t prior_id,
    float* precision_modifier
);

/**
 * @brief Get prior adaptation state
 *
 * @param bridge Bridge handle
 * @param prior_id Prior to query
 * @param state Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_fep_get_prior_state(
    epigen_fep_bridge_t* bridge,
    uint32_t prior_id,
    epigen_fep_prior_state_t* state
);

/**
 * @brief Lock prior with epigenetic mark
 *
 * WHAT: Permanently fixes prior precision
 * WHY:  Successful predictions deserve consolidation
 * HOW:  Applies permanent methylation pattern
 *
 * @param bridge Bridge handle
 * @param prior_id Prior to lock
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_fep_lock_prior(
    epigen_fep_bridge_t* bridge,
    uint32_t prior_id
);

//=============================================================================
// Learning Rate API
//=============================================================================

/**
 * @brief Get learning rate modifier for region
 *
 * WHAT: Returns epigenetic learning rate modifier
 * WHY:  FEP needs to scale update step size
 * HOW:  Based on histone acetylation state
 *
 * @param bridge Bridge handle
 * @param region_id Region to query
 * @param learning_modifier Output modifier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_fep_get_learning_modifier(
    epigen_fep_bridge_t* bridge,
    uint32_t region_id,
    float* learning_modifier
);

/**
 * @brief Set critical period state
 *
 * WHAT: Activates critical period effects
 * WHY:  Critical periods enhance model flexibility
 * HOW:  Increases learning rate, reduces precision
 *
 * @param bridge Bridge handle
 * @param region_id Region for critical period
 * @param is_critical Enable/disable critical period
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_fep_set_critical_period(
    epigen_fep_bridge_t* bridge,
    uint32_t region_id,
    bool is_critical
);

//=============================================================================
// Prediction Error Feedback API (FEP -> Epigenetics)
//=============================================================================

/**
 * @brief Report prediction error for prior
 *
 * WHAT: Reports prediction error for epigenetic tracking
 * WHY:  Chronic errors trigger epigenetic changes
 * HOW:  Integrates error over time, classifies type
 *
 * @param bridge Bridge handle
 * @param prior_id Affected prior
 * @param prediction_error Current prediction error (nats)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_fep_report_error(
    epigen_fep_bridge_t* bridge,
    uint32_t prior_id,
    float prediction_error
);

/**
 * @brief Get pending epigenetic triggers from errors
 *
 * WHAT: Returns error-triggered epigenetic changes
 * WHY:  Epigenetics module needs to apply changes
 * HOW:  Returns accumulated trigger events
 *
 * @param bridge Bridge handle
 * @param triggers Output array for triggers
 * @param max_triggers Maximum triggers to return
 * @return Number of triggers, -1 on error
 */
NIMCP_EXPORT int epigen_fep_get_error_triggers(
    epigen_fep_bridge_t* bridge,
    epigen_fep_trigger_t* triggers,
    uint32_t max_triggers
);

/**
 * @brief Report successful prediction
 *
 * WHAT: Reports prediction success for consolidation
 * WHY:  Successful predictions may be locked
 * HOW:  Tracks success duration, triggers methylation
 *
 * @param bridge Bridge handle
 * @param prior_id Successful prior
 * @param confidence Prediction confidence
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_fep_report_success(
    epigen_fep_bridge_t* bridge,
    uint32_t prior_id,
    float confidence
);

//=============================================================================
// Model Structure API
//=============================================================================

/**
 * @brief Request model structure change
 *
 * WHAT: Requests gene expression-mediated model change
 * WHY:  Chronic errors require structural adaptation
 * HOW:  Triggers gene expression for model update
 *
 * @param bridge Bridge handle
 * @param region_id Model region to modify
 * @param change_type Type of structural change
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_fep_request_model_change(
    epigen_fep_bridge_t* bridge,
    uint32_t region_id,
    epigen_fep_model_change_t change_type
);

/**
 * @brief Get pending model updates
 *
 * @param bridge Bridge handle
 * @param updates Output array for model updates
 * @param max_updates Maximum updates to return
 * @return Number of updates, -1 on error
 */
NIMCP_EXPORT int epigen_fep_get_model_updates(
    epigen_fep_bridge_t* bridge,
    epigen_fep_model_update_t* updates,
    uint32_t max_updates
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Integrate errors, process triggers, decay states
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_fep_update(
    epigen_fep_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_fep_reset(epigen_fep_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_fep_get_stats(
    const epigen_fep_bridge_t* bridge,
    epigen_fep_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_fep_reset_stats(epigen_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPIGEN_FEP_BRIDGE_H */