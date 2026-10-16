/**
 * @file nimcp_omni_precision.h
 * @brief Omnidirectional Inference Precision Weighting System
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: Precision weighting for omnidirectional inference modules
 * WHY:  Precision-weighted prediction errors drive adaptive inference
 * HOW:  Coordinate precision across bridges via FEP integration
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * PRECISION-WEIGHTED PREDICTION ERRORS (Friston, 2010):
 * -----------------------------------------------------
 * In the Free Energy Principle, precision weights prediction errors:
 *
 *   ε_weighted = Π * ε
 *
 * Where:
 *   ε = Raw prediction error (observation - prediction)
 *   Π = Precision matrix (inverse variance)
 *   ε_weighted = Precision-weighted error
 *
 * High precision amplifies reliable signals; low precision attenuates noise.
 *
 * ATTENTION AS PRECISION OPTIMIZATION:
 * ------------------------------------
 * Attention can be understood as precision optimization:
 *
 *   - Attending to a sensory channel = Increasing its precision
 *   - Ignoring distractors = Decreasing their precision
 *   - Confidence in predictions = Precision of top-down signals
 *
 * OMNIDIRECTIONAL PRECISION:
 * --------------------------
 * For bidirectional inference, precision must be tracked for:
 *
 *   1. FORWARD PRECISION: Reliability of forward predictions
 *   2. BACKWARD PRECISION: Reliability of backward inferences
 *   3. LATERAL PRECISION: Reliability of cross-modal predictions
 *   4. HIERARCHICAL PRECISION: Precision at each hierarchy level
 *
 * PRECISION PROPAGATION:
 * ----------------------
 * Precision flows through the inference graph:
 *
 *   ┌────────────────┐          ┌────────────────┐
 *   │   Module A     │──Π_ab───▶│   Module B     │
 *   │  precision_a   │◀──Π_ba───│  precision_b   │
 *   └────────────────┘          └────────────────┘
 *          │                            │
 *          ▼                            ▼
 *   ┌────────────────────────────────────────────┐
 *   │          Precision Context                  │
 *   │  - Global precision state                   │
 *   │  - Precision routing/propagation            │
 *   │  - Bio-async precision updates              │
 *   └────────────────────────────────────────────┘
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OMNI_PRECISION_H
#define NIMCP_OMNI_PRECISION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct omni_precision_ctx omni_precision_ctx_t;
typedef struct omni_kg_sync omni_kg_sync_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum precision channels per module */
#define OMNI_PRECISION_MAX_CHANNELS        16

/** @brief Maximum modules in precision context */
#define OMNI_PRECISION_MAX_MODULES         32

/** @brief Precision update rate (higher = faster adaptation) */
#define OMNI_PRECISION_DEFAULT_LR          0.05f

/** @brief Minimum precision (avoid division by zero) */
#define OMNI_PRECISION_MIN                 0.01f

/** @brief Maximum precision (avoid numerical instability) */
#define OMNI_PRECISION_MAX                 100.0f

/** @brief Default precision (unit precision) */
#define OMNI_PRECISION_DEFAULT             1.0f

/** @brief Precision decay rate for unused channels */
#define OMNI_PRECISION_DECAY               0.99f

/** @brief Precision floor for decay */
#define OMNI_PRECISION_FLOOR               0.1f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Precision channel types (inference directions)
 */
typedef enum {
    OMNI_PREC_CHANNEL_FORWARD = 0,       /**< Forward prediction precision */
    OMNI_PREC_CHANNEL_BACKWARD,          /**< Backward inference precision */
    OMNI_PREC_CHANNEL_LATERAL,           /**< Cross-modal precision */
    OMNI_PREC_CHANNEL_HIERARCHICAL_UP,   /**< Bottom-up precision */
    OMNI_PREC_CHANNEL_HIERARCHICAL_DOWN, /**< Top-down precision */
    OMNI_PREC_CHANNEL_ASSOCIATIVE,       /**< Hopfield retrieval precision */
    OMNI_PREC_CHANNEL_TEMPORAL,          /**< Temporal prediction precision */
    OMNI_PREC_CHANNEL_COUNT
} omni_precision_channel_t;

/**
 * @brief Precision update modes
 */
typedef enum {
    OMNI_PREC_UPDATE_BAYESIAN = 0,       /**< Bayesian precision update */
    OMNI_PREC_UPDATE_GRADIENT,           /**< Gradient descent update */
    OMNI_PREC_UPDATE_EXPONENTIAL,        /**< Exponential moving average */
    OMNI_PREC_UPDATE_FIXED               /**< Fixed (no update) */
} omni_precision_update_mode_t;

/**
 * @brief Precision routing modes
 */
typedef enum {
    OMNI_PREC_ROUTE_INDEPENDENT = 0,     /**< Each module independent */
    OMNI_PREC_ROUTE_HIERARCHICAL,        /**< Precision flows up/down hierarchy */
    OMNI_PREC_ROUTE_GRAPH,               /**< Precision follows KG edges */
    OMNI_PREC_ROUTE_BROADCAST            /**< Global precision broadcast */
} omni_precision_route_mode_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Per-channel precision state
 */
typedef struct {
    float value;                          /**< Current precision value */
    float variance;                       /**< Precision uncertainty */
    float error_history;                  /**< EMA of prediction errors */
    uint32_t update_count;                /**< Number of updates */
    bool enabled;                         /**< Channel enabled flag */
} omni_channel_precision_t;

/**
 * @brief Per-module precision state
 */
typedef struct {
    uint16_t module_id;                   /**< Bio-async module ID */
    char module_name[64];                 /**< Module name */
    omni_channel_precision_t channels[OMNI_PREC_CHANNEL_COUNT];
    float aggregate_precision;            /**< Combined precision */
    float confidence;                     /**< Module confidence */
    bool active;                          /**< Module is active */
} omni_module_precision_t;

/**
 * @brief Precision propagation weights
 */
typedef struct {
    uint16_t source_module;               /**< Source module ID */
    uint16_t target_module;               /**< Target module ID */
    omni_precision_channel_t channel;     /**< Channel type */
    float weight;                         /**< Propagation weight */
} omni_precision_edge_t;

/**
 * @brief Precision context configuration
 */
typedef struct {
    /* Update settings */
    omni_precision_update_mode_t update_mode;
    float learning_rate;
    float decay_rate;

    /* Routing settings */
    omni_precision_route_mode_t route_mode;
    bool enable_propagation;

    /* Bounds */
    float min_precision;
    float max_precision;

    /* Integration */
    bool enable_fep_integration;
    bool enable_kg_sync;
    bool enable_bio_async;

    /* Logging */
    bool enable_logging;
} omni_precision_config_t;

/**
 * @brief Precision statistics
 */
typedef struct {
    uint64_t total_updates;               /**< Total precision updates */
    uint64_t propagations;                /**< Precision propagations */
    float avg_precision;                  /**< Average precision */
    float min_precision;                  /**< Minimum observed */
    float max_precision;                  /**< Maximum observed */
    float avg_prediction_error;           /**< Average PE across modules */
    uint32_t low_precision_count;         /**< Modules below threshold */
    uint32_t high_precision_count;        /**< Modules above threshold */
} omni_precision_stats_t;

/**
 * @brief Precision update message (for bio-async)
 */
typedef struct {
    uint16_t source_module;               /**< Module sending update */
    omni_precision_channel_t channel;     /**< Channel updated */
    float new_precision;                  /**< New precision value */
    float prediction_error;               /**< Associated PE */
    uint64_t timestamp_us;                /**< Update timestamp */
} omni_precision_update_msg_t;

/**
 * @brief Precision context (main structure)
 */
struct omni_precision_ctx {
    /* Configuration */
    omni_precision_config_t config;

    /* Module precision states */
    omni_module_precision_t modules[OMNI_PRECISION_MAX_MODULES];
    uint32_t module_count;

    /* Propagation edges */
    omni_precision_edge_t* edges;
    uint32_t edge_count;
    uint32_t edge_capacity;

    /* External integrations */
    void* fep;                            /**< FEP system reference (fep_system_t*) */
    omni_kg_sync_t* kg_sync;              /**< KG sync reference */

    /* Bio-async */
    void* bio_context;                    /**< Bio-async context */
    bool bio_async_connected;

    /* Statistics */
    omni_precision_stats_t stats;

    /* Thread safety */
    void* mutex;
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default precision configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_default_config(omni_precision_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create precision context
 *
 * WHAT: Create centralized precision management context
 * WHY:  Coordinate precision across all omni bridges
 * HOW:  Initialize module states, edges, stats
 *
 * @param config Configuration (NULL for defaults)
 * @return New precision context or NULL on failure
 */
omni_precision_ctx_t* omni_precision_create(const omni_precision_config_t* config);

/**
 * @brief Destroy precision context
 *
 * @param ctx Context to destroy (NULL safe)
 */
void omni_precision_destroy(omni_precision_ctx_t* ctx);

/**
 * @brief Reset precision context to initial state
 *
 * @param ctx Context
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_reset(omni_precision_ctx_t* ctx);

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

/**
 * @brief Register module for precision tracking
 *
 * @param ctx Precision context
 * @param module_id Bio-async module ID
 * @param name Module name
 * @param initial_precision Initial precision value
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_register_module(omni_precision_ctx_t* ctx,
                                    uint16_t module_id,
                                    const char* name,
                                    float initial_precision);

/**
 * @brief Enable specific channel for module
 *
 * @param ctx Precision context
 * @param module_id Module ID
 * @param channel Channel to enable
 * @param initial_precision Initial precision for channel
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_enable_channel(omni_precision_ctx_t* ctx,
                                   uint16_t module_id,
                                   omni_precision_channel_t channel,
                                   float initial_precision);

/**
 * @brief Unregister module
 *
 * @param ctx Precision context
 * @param module_id Module ID
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_unregister_module(omni_precision_ctx_t* ctx,
                                      uint16_t module_id);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update precision for a channel based on prediction error
 *
 * WHAT: Adjust precision based on observed prediction error
 * WHY:  Reliable predictions get higher precision
 * HOW:  Bayesian or gradient-based precision update
 *
 * @param ctx Precision context
 * @param module_id Module ID
 * @param channel Channel to update
 * @param prediction_error Observed prediction error
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_update(omni_precision_ctx_t* ctx,
                           uint16_t module_id,
                           omni_precision_channel_t channel,
                           float prediction_error);

/**
 * @brief Update precision using FEP system state
 *
 * @param ctx Precision context
 * @param module_id Module ID
 * @param fep FEP system
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_update_from_fep(omni_precision_ctx_t* ctx,
                                    uint16_t module_id,
                                    const void* fep);

/**
 * @brief Propagate precision through edges
 *
 * WHAT: Spread precision updates across connected modules
 * WHY:  Precision context affects connected predictions
 * HOW:  Follow propagation edges with weighted updates
 *
 * @param ctx Precision context
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_propagate(omni_precision_ctx_t* ctx);

/**
 * @brief Apply precision decay to inactive channels
 *
 * @param ctx Precision context
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_decay(omni_precision_ctx_t* ctx);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get precision for a channel
 *
 * @param ctx Precision context
 * @param module_id Module ID
 * @param channel Channel
 * @return Precision value, or OMNI_PRECISION_DEFAULT on error
 */
float omni_precision_get(const omni_precision_ctx_t* ctx,
                          uint16_t module_id,
                          omni_precision_channel_t channel);

/**
 * @brief Get aggregate precision for module
 *
 * @param ctx Precision context
 * @param module_id Module ID
 * @return Aggregate precision value
 */
float omni_precision_get_aggregate(omni_precision_ctx_t* ctx,
                                    uint16_t module_id);

/**
 * @brief Get module confidence
 *
 * @param ctx Precision context
 * @param module_id Module ID
 * @return Confidence value [0, 1]
 */
float omni_precision_get_confidence(omni_precision_ctx_t* ctx,
                                     uint16_t module_id);

/**
 * @brief Get all channel precisions for module
 *
 * @param ctx Precision context
 * @param module_id Module ID
 * @param precisions Output array (must hold OMNI_PREC_CHANNEL_COUNT)
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_get_all_channels(const omni_precision_ctx_t* ctx,
                                     uint16_t module_id,
                                     float* precisions);

/**
 * @brief Get precision statistics
 *
 * @param ctx Precision context
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_get_stats(omni_precision_ctx_t* ctx,
                              omni_precision_stats_t* stats);

/**
 * @brief Reset precision statistics
 *
 * @param ctx Precision context
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_reset_stats(omni_precision_ctx_t* ctx);

/* ============================================================================
 * Edge API
 * ============================================================================ */

/**
 * @brief Add precision propagation edge
 *
 * @param ctx Precision context
 * @param source_module Source module ID
 * @param target_module Target module ID
 * @param channel Channel type
 * @param weight Propagation weight
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_add_edge(omni_precision_ctx_t* ctx,
                             uint16_t source_module,
                             uint16_t target_module,
                             omni_precision_channel_t channel,
                             float weight);

/**
 * @brief Add bidirectional precision edges
 *
 * @param ctx Precision context
 * @param module_a First module
 * @param module_b Second module
 * @param weight Propagation weight
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_add_bidirectional_edge(omni_precision_ctx_t* ctx,
                                           uint16_t module_a,
                                           uint16_t module_b,
                                           float weight);

/**
 * @brief Remove precision edge
 *
 * @param ctx Precision context
 * @param source_module Source module ID
 * @param target_module Target module ID
 * @param channel Channel type
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_remove_edge(omni_precision_ctx_t* ctx,
                                uint16_t source_module,
                                uint16_t target_module,
                                omni_precision_channel_t channel);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to FEP system
 *
 * @param ctx Precision context
 * @param fep FEP system
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_connect_fep(omni_precision_ctx_t* ctx,
                                void* fep);

/**
 * @brief Connect to KG sync
 *
 * @param ctx Precision context
 * @param kg_sync KG sync manager
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_connect_kg_sync(omni_precision_ctx_t* ctx,
                                    omni_kg_sync_t* kg_sync);

/**
 * @brief Sync precision to KG edges
 *
 * @param ctx Precision context
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_sync_to_kg(omni_precision_ctx_t* ctx);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param ctx Precision context
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_connect_bio_async(omni_precision_ctx_t* ctx);

/**
 * @brief Disconnect from bio-async router
 *
 * @param ctx Precision context
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_disconnect_bio_async(omni_precision_ctx_t* ctx);

/**
 * @brief Check bio-async connection status
 *
 * @param ctx Precision context
 * @return true if connected
 */
bool omni_precision_is_bio_async_connected(const omni_precision_ctx_t* ctx);

/**
 * @brief Broadcast precision update to all connected modules
 *
 * @param ctx Precision context
 * @param module_id Source module
 * @param channel Updated channel
 * @return NIMCP_SUCCESS on success
 */
int omni_precision_broadcast_update(omni_precision_ctx_t* ctx,
                                     uint16_t module_id,
                                     omni_precision_channel_t channel);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Compute precision from prediction error variance
 *
 * WHAT: Convert variance to precision (Π = 1/σ²)
 * WHY:  Precision is inverse variance in FEP
 * HOW:  With bounds checking and regularization
 *
 * @param variance Error variance
 * @return Bounded precision value
 */
float omni_precision_from_variance(float variance);

/**
 * @brief Compute precision-weighted prediction error
 *
 * @param error Raw prediction error
 * @param precision Precision weight
 * @return Weighted error
 */
float omni_precision_weight_error(float error, float precision);

/**
 * @brief Compute confidence from precision
 *
 * Confidence = precision / (precision + 1)
 * Maps precision [0, ∞) to confidence [0, 1)
 *
 * @param precision Precision value
 * @return Confidence [0, 1]
 */
float omni_precision_to_confidence(float precision);

/**
 * @brief Clamp precision to valid range
 *
 * @param precision Input precision
 * @return Clamped precision
 */
float omni_precision_clamp(float precision);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert channel type to string
 */
const char* omni_precision_channel_to_string(omni_precision_channel_t channel);

/**
 * @brief Convert update mode to string
 */
const char* omni_precision_update_mode_to_string(omni_precision_update_mode_t mode);

/**
 * @brief Convert route mode to string
 */
const char* omni_precision_route_mode_to_string(omni_precision_route_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_PRECISION_H */
