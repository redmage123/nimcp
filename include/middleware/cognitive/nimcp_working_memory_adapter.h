/**
 * @file nimcp_working_memory_adapter.h
 * @brief Middleware adapter for working memory integration
 *
 * WHAT: Connects middleware features to working memory module
 * WHY:  Enable working memory to use normalized neural features
 * HOW:  Extract features, normalize, and feed to working memory
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#ifndef NIMCP_WORKING_MEMORY_ADAPTER_H
#define NIMCP_WORKING_MEMORY_ADAPTER_H

#include "middleware/brain_integration.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * WHAT: Working memory adapter instance
 * WHY:  Manage middleware-to-WM connection
 * HOW:  Maintain buffers and normalizers
 */
typedef struct working_memory_adapter_struct* working_memory_adapter_t;

/**
 * WHAT: Working memory adapter configuration
 * WHY:  Customize feature extraction for WM
 * HOW:  Specify buffer sizes, normalization, features
 */
typedef struct {
    uint32_t num_channels;           /**< Number of input channels */
    brain_buffer_size_t buffer_size; /**< Temporal buffer size */
    brain_normalize_type_t norm_type;/**< Normalization method */
    uint32_t max_features;           /**< Maximum features to extract */
    bool enable_spike_features;      /**< Enable spike-based features */
    bool enable_oscillations;        /**< Enable oscillation analysis */
} working_memory_adapter_config_t;

/**
 * WHAT: Create working memory adapter
 * WHY:  Initialize middleware connection
 * HOW:  Allocate buffers and normalizers
 *
 * @param config Adapter configuration
 * @return Adapter handle or NULL on error
 */
working_memory_adapter_t working_memory_adapter_create(
    const working_memory_adapter_config_t* config
);

/**
 * WHAT: Destroy working memory adapter
 * WHY:  Clean memory cleanup
 * HOW:  Free all resources
 *
 * @param adapter Adapter to destroy (NULL is safe)
 */
void working_memory_adapter_destroy(working_memory_adapter_t adapter);

/**
 * WHAT: Update adapter with new neural activity
 * WHY:  Process new timestep of neural data
 * HOW:  Buffer activity, extract and normalize features
 *
 * @param adapter Adapter instance
 * @param activity Neural activity vector
 * @param num_channels Number of channels
 * @param timestamp Current timestamp
 * @param features_out Output normalized features
 * @return Number of features extracted
 */
uint32_t working_memory_adapter_update(
    working_memory_adapter_t adapter,
    const float* activity,
    uint32_t num_channels,
    uint64_t timestamp,
    float* features_out
);

/**
 * WHAT: Get default working memory adapter configuration
 * WHY:  Provide sensible defaults
 * HOW:  Return pre-filled config struct
 *
 * @return Default configuration
 */
working_memory_adapter_config_t working_memory_adapter_default_config(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_WORKING_MEMORY_ADAPTER_H
