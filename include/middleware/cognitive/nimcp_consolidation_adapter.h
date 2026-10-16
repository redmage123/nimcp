/**
 * @file nimcp_consolidation_adapter.h
 * @brief Middleware adapter for memory consolidation
 *
 * WHAT: Connects middleware features to consolidation module
 * WHY:  Enable consolidation to use neural activity patterns
 * HOW:  Extract features, detect important patterns, feed to consolidation
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#ifndef NIMCP_CONSOLIDATION_ADAPTER_H
#define NIMCP_CONSOLIDATION_ADAPTER_H

#include "middleware/brain_integration.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * WHAT: Consolidation adapter instance
 * WHY:  Manage middleware-to-consolidation connection
 * HOW:  Maintain pattern detection and feature extraction
 */
typedef struct consolidation_adapter_struct* consolidation_adapter_t;

/**
 * WHAT: Consolidation adapter configuration
 * WHY:  Customize feature extraction for consolidation
 * HOW:  Specify pattern detection parameters
 */
typedef struct {
    uint32_t num_channels;           /**< Number of input channels */
    brain_buffer_size_t buffer_size; /**< Temporal buffer size */
    float importance_threshold;      /**< Pattern importance threshold [0-1] */
    uint32_t max_patterns;           /**< Maximum patterns to track */
    bool enable_replay_detection;    /**< Enable replay pattern detection */
    bool enable_synchrony_tracking;  /**< Enable synchrony tracking */
} consolidation_adapter_config_t;

/**
 * WHAT: Pattern importance metrics
 * WHY:  Quantify which patterns should be consolidated
 * HOW:  Combine synchrony, strength, frequency
 */
typedef struct {
    float synchrony;        /**< Population synchrony [0-1] */
    float strength;         /**< Pattern strength [0-1] */
    float frequency;        /**< Occurrence frequency [0-1] */
    float importance;       /**< Overall importance [0-1] */
    bool should_consolidate;/**< Whether to consolidate */
} pattern_importance_t;

/**
 * WHAT: Create consolidation adapter
 * WHY:  Initialize middleware connection
 * HOW:  Allocate pattern detectors and buffers
 *
 * @param config Adapter configuration
 * @return Adapter handle or NULL on error
 */
consolidation_adapter_t consolidation_adapter_create(
    const consolidation_adapter_config_t* config
);

/**
 * WHAT: Destroy consolidation adapter
 * WHY:  Clean memory cleanup
 * HOW:  Free all resources
 *
 * @param adapter Adapter to destroy (NULL is safe)
 */
void consolidation_adapter_destroy(consolidation_adapter_t adapter);

/**
 * WHAT: Analyze neural pattern for consolidation
 * WHY:  Determine if pattern should be consolidated
 * HOW:  Extract features, compute importance metrics
 *
 * @param adapter Adapter instance
 * @param activity Neural activity vector
 * @param num_channels Number of channels
 * @param timestamp Current timestamp
 * @param importance_out Output importance metrics
 * @return true on success
 */
bool consolidation_adapter_analyze_pattern(
    consolidation_adapter_t adapter,
    const float* activity,
    uint32_t num_channels,
    uint64_t timestamp,
    pattern_importance_t* importance_out
);

/**
 * WHAT: Get default consolidation adapter configuration
 * WHY:  Provide sensible defaults
 * HOW:  Return pre-filled config struct
 *
 * @return Default configuration
 */
consolidation_adapter_config_t consolidation_adapter_default_config(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_CONSOLIDATION_ADAPTER_H
