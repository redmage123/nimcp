/**
 * @file nimcp_biological_timescales_fep_bridge.h
 * @brief FEP bridge for biological timescales (temporal precision matching)
 *
 * WHAT: Bidirectional integration between Free Energy Principle and biological timing
 * WHY:  Enable temporal precision weighting for multi-scale predictions
 * HOW:  FEP temporal dynamics match biological timescales, precision decays with time
 *
 * BIOLOGICAL BASIS:
 * - Timescale hierarchy = temporal precision in predictive coding
 * - Fast timescales (gamma) = high precision, short predictions
 * - Slow timescales (delta) = low precision, long-term predictions
 * - Decay dynamics = precision decay over temporal distance
 * - Cross-frequency coupling = hierarchical temporal inference
 *
 * INTEGRATION MECHANISMS:
 * 1. FEP → Timescales: Precision modulates temporal resolution
 * 2. Timescales → FEP: Decay rates inform precision updates
 * 3. Bidirectional: Multi-scale predictions match timescale hierarchy
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 * @version 1.0.0
 */

#ifndef NIMCP_BIOLOGICAL_TIMESCALES_FEP_BRIDGE_H
#define NIMCP_BIOLOGICAL_TIMESCALES_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_biological_timescales.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Biological timescales FEP bridge configuration
 */
typedef struct {
    /* FEP temporal parameters */
    float precision_decay_rates[5];    /**< Precision decay per oscillation band */
    float temporal_horizon_ms[5];      /**< Prediction horizon per band */
    bool enable_hierarchical_timing;   /**< Use hierarchy for multi-scale */

    /* Timescale mapping */
    bool map_levels_to_bands;          /**< Map FEP levels to oscillation bands */
    uint32_t level_to_band[FEP_MAX_HIERARCHY_LEVELS]; /**< Level-band mapping */

    /* Learning */
    float learning_rate;               /**< Temporal learning rate */
    bool enable_precision_learning;    /**< Learn precision from timing */
    bool enable_decay_adaptation;      /**< Adapt decay rates */

    /* Integration */
    bool enable_temporal_prediction;   /**< Predict timing via FEP */
    float prediction_tolerance_ms;     /**< Acceptable timing error */
} biological_timescales_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects on biological timescales
 *
 * WHAT: How FEP modulates temporal dynamics
 * WHY:  Precision controls temporal resolution
 * HOW:  High precision = fast timescales, low = slow timescales
 */
typedef struct {
    /* Timescale selection */
    nimcp_oscillation_band_t selected_band; /**< Selected oscillation band */
    float band_preferences[5];         /**< Preference for each band [0,1] */

    /* Temporal prediction */
    float predicted_interval_ms;       /**< Predicted event interval */
    float prediction_precision;        /**< Precision of prediction */

    /* Precision modulation */
    float precision_per_band[5];       /**< Precision weight per band */
    float temporal_resolution_ms;      /**< Effective temporal resolution */

    /* Decay modulation */
    float decay_rate_modulation;       /**< Modulate decay rates */
    bool enable_fast_dynamics;         /**< Enable fast timescale dynamics */
} biological_timescales_fep_effects_t;

/**
 * @brief Biological timescales effects on FEP
 *
 * WHAT: How timing dynamics inform FEP
 * WHY:  Temporal patterns update predictions
 * HOW:  Decay rates inform precision decay, timing errors update beliefs
 */
typedef struct {
    /* Timing observations */
    float observed_interval_ms;        /**< Observed event interval */
    float timing_precision;            /**< Precision of timing measurement */
    nimcp_oscillation_band_t active_band; /**< Currently active band */

    /* Prediction errors */
    float timing_prediction_error;     /**< Timing prediction error */
    float precision_prediction_error;  /**< Precision estimate error */

    /* Surprise signals */
    float timing_surprise;             /**< Surprise from timing mismatch */
    bool unexpected_timing_event;      /**< Unexpected timing detected */

    /* Decay dynamics */
    float measured_decay_rate;         /**< Measured precision decay rate */
    float decay_consistency;           /**< Consistency of decay [0,1] */
} fep_biological_timescales_effects_t;

/* ============================================================================
 * State and Statistics
 * ============================================================================ */

/**
 * @brief Bridge state tracking
 */
typedef struct {
    /* Temporal state */
    uint64_t last_event_time_us;       /**< Last event timestamp */
    float last_interval_ms;            /**< Last measured interval */
    nimcp_oscillation_band_t current_band; /**< Current timescale band */

    /* Prediction state */
    uint32_t active_predictions;       /**< Active temporal predictions */
    uint64_t total_predictions;        /**< Total predictions made */
    uint64_t accurate_predictions;     /**< Accurate timing predictions */

    /* Integration state */
    bool fep_active;                   /**< FEP actively predicting */
    bool timescales_initialized;       /**< Timescales initialized */
    uint64_t last_update_us;           /**< Last update timestamp */
} biological_timescales_fep_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Timing accuracy */
    float avg_timing_error_ms;         /**< Average timing prediction error */
    float avg_timing_surprise;         /**< Average timing surprise */
    uint64_t timing_violations;        /**< Timing predictions > tolerance */

    /* Band statistics */
    uint64_t predictions_per_band[5];  /**< Predictions per oscillation band */
    float accuracy_per_band[5];        /**< Accuracy per band [0,1] */
    uint64_t band_switches;            /**< Number of band transitions */

    /* Precision metrics */
    float avg_precision_per_band[5];   /**< Average precision per band */
    float avg_decay_rate_per_band[5];  /**< Average decay rate per band */

    /* FEP metrics */
    float avg_free_energy;             /**< Average free energy */
    float avg_precision;               /**< Average precision */
} biological_timescales_fep_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Biological timescales FEP bridge
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* Configuration */
    biological_timescales_fep_config_t config;

    /* Module connections */
    fep_system_t* fep_system;
    /* Note: Biological timescales are constants, not a module instance */

    /* Effects */
    biological_timescales_fep_effects_t fep_effects;
    fep_biological_timescales_effects_t timescales_effects;

    /* State */
    biological_timescales_fep_state_t state;

    /* Statistics */
    biological_timescales_fep_stats_t stats;

    } biological_timescales_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default biological timescales FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for timescales-FEP integration
 * WHY:  Easy initialization with biologically-realistic timing
 * HOW:  Returns config with proper timescale hierarchy
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int biological_timescales_fep_default_config(biological_timescales_fep_config_t* config);

/**
 * @brief Create biological timescales FEP bridge
 *
 * WHAT: Initialize bidirectional FEP-timescales integration
 * WHY:  Enable temporal precision weighting for predictions
 * HOW:  Allocate bridge, connect FEP, map hierarchy to timescales
 *
 * @param config Bridge configuration
 * @param fep_system FEP system to integrate
 * @return Bridge instance or NULL on failure
 */
biological_timescales_fep_bridge_t* biological_timescales_fep_create(
    const biological_timescales_fep_config_t* config,
    fep_system_t* fep_system
);

/**
 * @brief Destroy biological timescales FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Free memory and disconnect integrations
 * HOW:  Free state, destroy mutex
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void biological_timescales_fep_destroy(biological_timescales_fep_bridge_t* bridge);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Update FEP effects on timescales
 *
 * WHAT: Compute how FEP precision modulates temporal dynamics
 * WHY:  High precision requires fast timescales
 * HOW:  Map FEP precision to oscillation bands
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int biological_timescales_fep_update_effects(biological_timescales_fep_bridge_t* bridge);

/**
 * @brief Observe timing event
 *
 * WHAT: Feed timing measurement into FEP
 * WHY:  Actual timing updates temporal predictions
 * HOW:  Process interval, compute prediction error
 *
 * @param bridge FEP bridge
 * @param interval_ms Measured interval
 * @param band Oscillation band that was active
 * @return 0 on success, error code on failure
 */
int biological_timescales_fep_observe_timing(
    biological_timescales_fep_bridge_t* bridge,
    float interval_ms,
    nimcp_oscillation_band_t band
);

/**
 * @brief Predict next event timing
 *
 * WHAT: Use FEP to predict when next event will occur
 * WHY:  Enable anticipatory processing
 * HOW:  FEP temporal prediction with band-specific precision
 *
 * @param bridge FEP bridge
 * @param band Oscillation band for prediction
 * @param predicted_interval_ms Output predicted interval
 * @param precision Output prediction precision
 * @return 0 on success, error code on failure
 */
int biological_timescales_fep_predict_timing(
    biological_timescales_fep_bridge_t* bridge,
    nimcp_oscillation_band_t band,
    float* predicted_interval_ms,
    float* precision
);

/**
 * @brief Select optimal timescale band
 *
 * WHAT: Choose oscillation band based on FEP precision
 * WHY:  Match temporal resolution to prediction certainty
 * HOW:  High precision → fast bands, low → slow bands
 *
 * @param bridge FEP bridge
 * @param fep_level FEP hierarchy level
 * @return Optimal oscillation band
 */
nimcp_oscillation_band_t biological_timescales_fep_select_band(
    biological_timescales_fep_bridge_t* bridge,
    uint32_t fep_level
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async for timing updates
 * WHY:  Enable async temporal processing
 * HOW:  Register module, set up timing handlers
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int biological_timescales_fep_connect_bio_async(biological_timescales_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async messaging
 * WHY:  Clean shutdown of async integration
 * HOW:  Unregister module context
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int biological_timescales_fep_disconnect_bio_async(biological_timescales_fep_bridge_t* bridge);

/**
 * @brief Check if connected to bio-async
 *
 * @param bridge FEP bridge
 * @return true if connected
 */
bool biological_timescales_fep_is_bio_async_connected(
    const biological_timescales_fep_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current FEP effects on timescales
 *
 * @param bridge FEP bridge
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int biological_timescales_fep_get_effects(
    const biological_timescales_fep_bridge_t* bridge,
    biological_timescales_fep_effects_t* effects
);

/**
 * @brief Get timescales effects on FEP
 *
 * @param bridge FEP bridge
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int biological_timescales_fep_get_timescales_effects(
    const biological_timescales_fep_bridge_t* bridge,
    fep_biological_timescales_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge FEP bridge
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int biological_timescales_fep_get_stats(
    const biological_timescales_fep_bridge_t* bridge,
    biological_timescales_fep_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int biological_timescales_fep_reset_stats(biological_timescales_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIOLOGICAL_TIMESCALES_FEP_BRIDGE_H */
