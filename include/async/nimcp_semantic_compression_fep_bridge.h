/**
 * @file nimcp_semantic_compression_fep_bridge.h
 * @brief FEP bridge for semantic compression system
 *
 * WHAT: Bidirectional integration between Free Energy Principle and semantic compression
 * WHY:  Enable prediction-based compression with semantic meaning preservation
 * HOW:  FEP predicts signal patterns, compression preserves semantic structure
 *
 * BIOLOGICAL BASIS:
 * - Semantic compression = predictive coding (transmit only prediction errors)
 * - Compression primitives = learned generative model components
 * - Compression quality = prediction accuracy (low error = high compression)
 * - Semantic loss = surprise (unexpected information loss)
 *
 * INTEGRATION MECHANISMS:
 * 1. FEP → Compression: Predictions identify compressible patterns
 * 2. Compression → FEP: Compression ratio informs prediction quality
 * 3. Bidirectional: Semantic primitives align with FEP beliefs
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 * @version 1.0.0
 */

#ifndef NIMCP_SEMANTIC_COMPRESSION_FEP_BRIDGE_H
#define NIMCP_SEMANTIC_COMPRESSION_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "async/nimcp_semantic_compression.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
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
 * @brief Semantic compression FEP bridge configuration
 */
typedef struct {
    /* FEP parameters */
    float prediction_threshold;         /**< Min prediction confidence for compression */
    float semantic_surprise_threshold;  /**< Surprise threshold for semantic loss */
    uint32_t primitive_learning_iterations; /**< FEP iterations for primitive learning */

    /* Compression guidance */
    bool enable_predictive_compression; /**< Use FEP to guide compression */
    bool enable_semantic_preservation;  /**< Preserve semantic meaning via FEP */
    float quality_vs_compression_tradeoff; /**< Balance quality vs ratio [0,1] */

    /* Learning */
    float learning_rate;                /**< Primitive adaptation rate */
    bool enable_primitive_learning;     /**< Learn primitives from FEP beliefs */
    uint32_t max_primitives;            /**< Maximum semantic primitives */

    /* Integration */
    bool enable_error_feedback;         /**< Feed compression errors to FEP */
    float error_feedback_gain;          /**< Gain for error feedback */
} semantic_compression_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects on semantic compression
 *
 * WHAT: How FEP predictions guide compression
 * WHY:  Predictive patterns enable better compression
 * HOW:  FEP beliefs identify compressible semantic structures
 */
typedef struct {
    /* Compression guidance */
    float predicted_compressibility;    /**< Predicted compression ratio */
    float compression_confidence;       /**< Confidence in compression [0,1] */

    /* Primitive selection */
    uint32_t predicted_primitive_id;    /**< Predicted best primitive */
    float primitive_match_confidence;   /**< Confidence in primitive match */

    /* Quality control */
    float acceptable_semantic_loss;     /**< Acceptable loss threshold */
    bool preserve_semantic_structure;   /**< Whether to preserve structure */

    /* Modulation */
    float quality_modulation;           /**< Quality adjustment factor */
    float compression_aggressiveness;   /**< How aggressive to compress [0,1] */
} semantic_compression_fep_effects_t;

/**
 * @brief Semantic compression effects on FEP
 *
 * WHAT: How compression statistics inform FEP
 * WHY:  Compression performance validates predictions
 * HOW:  Compression ratio and loss update FEP beliefs
 */
typedef struct {
    /* Compression observations */
    float achieved_compression_ratio;   /**< Actual compression achieved */
    float semantic_loss_measured;       /**< Measured semantic loss */
    uint32_t primitives_used;           /**< Number of primitives used */

    /* Prediction errors */
    float compression_prediction_error; /**< Error in compression prediction */
    float quality_prediction_error;     /**< Error in quality prediction */

    /* Surprise signals */
    float compression_surprise;         /**< Surprise from compression mismatch */
    bool high_semantic_loss_event;      /**< High semantic loss detected */

    /* Learning signals */
    float primitive_alignment;          /**< How well primitives align with beliefs */
    uint32_t novel_patterns_discovered; /**< New patterns not predicted */
} fep_semantic_compression_effects_t;

/* ============================================================================
 * State and Statistics
 * ============================================================================ */

/**
 * @brief Bridge state tracking
 */
typedef struct {
    /* Compression state */
    uint32_t active_compressions;       /**< Active compression operations */
    uint64_t total_compressions;        /**< Total compressions performed */
    uint64_t successful_compressions;   /**< Successful compressions */

    /* Primitive state */
    uint32_t current_primitives;        /**< Current number of primitives */
    uint64_t primitives_learned;        /**< Primitives learned from FEP */
    uint64_t primitives_validated;      /**< Primitives validated by compression */

    /* Integration state */
    bool fep_active;                    /**< FEP actively predicting */
    bool compressor_connected;          /**< Compressor connected */
    uint64_t last_update_us;            /**< Last update timestamp */
} semantic_compression_fep_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Compression performance */
    float avg_compression_ratio;        /**< Average compression ratio */
    float avg_semantic_loss;            /**< Average semantic loss */
    float avg_compression_surprise;     /**< Average compression surprise */

    /* Prediction accuracy */
    float avg_compression_error;        /**< Average compression prediction error */
    float avg_quality_error;            /**< Average quality prediction error */
    uint64_t accurate_predictions;      /**< Accurate compression predictions */

    /* Primitive statistics */
    uint64_t primitive_hits;            /**< Primitive predictions correct */
    uint64_t primitive_misses;          /**< Primitive predictions incorrect */
    float primitive_hit_rate;           /**< Primitive prediction accuracy */

    /* FEP metrics */
    float avg_free_energy;              /**< Average free energy */
    float avg_precision;                /**< Average precision */
} semantic_compression_fep_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Semantic compression FEP bridge
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* Configuration */
    semantic_compression_fep_config_t config;

    /* Module connections */
    fep_system_t* fep_system;
    nimcp_semantic_compressor_t* compressor;

    /* Effects */
    semantic_compression_fep_effects_t fep_effects;
    fep_semantic_compression_effects_t compression_effects;

    /* State */
    semantic_compression_fep_state_t state;

    /* Statistics */
    semantic_compression_fep_stats_t stats;

    } semantic_compression_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default semantic compression FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for compression-FEP integration
 * WHY:  Easy initialization with effective predictive compression
 * HOW:  Returns config optimized for semantic preservation
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int semantic_compression_fep_default_config(semantic_compression_fep_config_t* config);

/**
 * @brief Create semantic compression FEP bridge
 *
 * WHAT: Initialize bidirectional FEP-compression integration
 * WHY:  Enable predictive compression with semantic awareness
 * HOW:  Allocate bridge, connect FEP and compressor, initialize primitives
 *
 * @param config Bridge configuration
 * @param fep_system FEP system to integrate
 * @param compressor Semantic compressor instance
 * @return Bridge instance or NULL on failure
 */
semantic_compression_fep_bridge_t* semantic_compression_fep_create(
    const semantic_compression_fep_config_t* config,
    fep_system_t* fep_system,
    nimcp_semantic_compressor_t* compressor
);

/**
 * @brief Destroy semantic compression FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Free memory and disconnect integrations
 * HOW:  Unregister callbacks, free state, destroy mutex
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void semantic_compression_fep_destroy(semantic_compression_fep_bridge_t* bridge);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Update FEP effects on compression
 *
 * WHAT: Compute how FEP predictions guide compression
 * WHY:  Apply predictive guidance to compression decisions
 * HOW:  Use FEP beliefs to select primitives and quality
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int semantic_compression_fep_update_effects(semantic_compression_fep_bridge_t* bridge);

/**
 * @brief Observe compression result
 *
 * WHAT: Feed compression statistics into FEP
 * WHY:  Actual compression performance updates predictions
 * HOW:  Process result, compute prediction errors
 *
 * @param bridge FEP bridge
 * @param ratio Achieved compression ratio
 * @param semantic_loss Measured semantic loss
 * @return 0 on success, error code on failure
 */
int semantic_compression_fep_observe_compression(
    semantic_compression_fep_bridge_t* bridge,
    float ratio,
    float semantic_loss
);

/**
 * @brief Predict compressibility
 *
 * WHAT: Use FEP to predict how well signal can be compressed
 * WHY:  Enable adaptive compression strategies
 * HOW:  FEP prediction of signal predictability
 *
 * @param bridge FEP bridge
 * @param signal Input signal to analyze
 * @param len Signal length
 * @param predicted_ratio Output predicted compression ratio
 * @param confidence Output prediction confidence
 * @return 0 on success, error code on failure
 */
int semantic_compression_fep_predict_compressibility(
    semantic_compression_fep_bridge_t* bridge,
    const float* signal,
    size_t len,
    float* predicted_ratio,
    float* confidence
);

/**
 * @brief Learn semantic primitive from FEP beliefs
 *
 * WHAT: Extract semantic primitive from FEP generative model
 * WHY:  Align compression primitives with learned beliefs
 * HOW:  Convert FEP belief structure to compression primitive
 *
 * @param bridge FEP bridge
 * @param level FEP hierarchy level to extract from
 * @param primitive_id Output primitive ID
 * @return 0 on success, error code on failure
 */
int semantic_compression_fep_learn_primitive(
    semantic_compression_fep_bridge_t* bridge,
    uint32_t level,
    uint32_t* primitive_id
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async for message handling
 * WHY:  Enable async compression operations
 * HOW:  Register module, set up message handlers
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int semantic_compression_fep_connect_bio_async(semantic_compression_fep_bridge_t* bridge);

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
int semantic_compression_fep_disconnect_bio_async(semantic_compression_fep_bridge_t* bridge);

/**
 * @brief Check if connected to bio-async
 *
 * @param bridge FEP bridge
 * @return true if connected
 */
bool semantic_compression_fep_is_bio_async_connected(
    const semantic_compression_fep_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current FEP effects on compression
 *
 * @param bridge FEP bridge
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int semantic_compression_fep_get_effects(
    const semantic_compression_fep_bridge_t* bridge,
    semantic_compression_fep_effects_t* effects
);

/**
 * @brief Get compression effects on FEP
 *
 * @param bridge FEP bridge
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int semantic_compression_fep_get_compression_effects(
    const semantic_compression_fep_bridge_t* bridge,
    fep_semantic_compression_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge FEP bridge
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int semantic_compression_fep_get_stats(
    const semantic_compression_fep_bridge_t* bridge,
    semantic_compression_fep_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge FEP bridge
 * @return 0 on success, error code on failure
 */
int semantic_compression_fep_reset_stats(semantic_compression_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SEMANTIC_COMPRESSION_FEP_BRIDGE_H */
