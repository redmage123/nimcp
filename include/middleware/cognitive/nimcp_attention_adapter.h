/**
 * @file nimcp_attention_adapter.h
 * @brief Middleware adapter for attention system
 *
 * WHAT: Connects middleware features to attention mechanisms
 * WHY:  Enable attention to use neural salience and synchrony
 * HOW:  Extract salient features, compute attention weights
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#ifndef NIMCP_ATTENTION_ADAPTER_H
#define NIMCP_ATTENTION_ADAPTER_H

#include "middleware/brain_integration.h"
#include "middleware/routing/nimcp_attention_gate.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * WHAT: Attention adapter instance
 * WHY:  Manage middleware-to-attention connection
 * HOW:  Maintain salience detection and gating
 */
typedef struct attention_adapter_struct* attention_adapter_t;

/**
 * WHAT: Attention adapter configuration
 * WHY:  Customize salience detection for attention
 * HOW:  Specify salience metrics and gating parameters
 */
typedef struct {
    uint32_t num_channels;           /**< Number of input channels */
    brain_buffer_size_t buffer_size; /**< Temporal buffer size */
    float salience_threshold;        /**< Salience threshold [0-1] */
    float top_k_fraction;            /**< Fraction of channels to attend [0-1] */
    bool enable_oscillation_gating;  /**< Gate by oscillation phase */
    bool enable_synchrony_gating;    /**< Gate by synchrony */
} attention_adapter_config_t;

/**
 * WHAT: Attention metrics for channel
 * WHY:  Quantify which channels deserve attention
 * HOW:  Combine variance, burst rate, synchrony
 */
typedef struct {
    float variance;         /**< Activity variance */
    float burst_rate;       /**< Burst firing rate */
    float synchrony;        /**< Synchrony with population */
    float salience;         /**< Overall salience [0-1] */
    float attention_weight; /**< Attention weight [0-1] */
} attention_metrics_t;

/**
 * WHAT: Create attention adapter
 * WHY:  Initialize middleware connection
 * HOW:  Allocate salience detectors and gates
 *
 * @param config Adapter configuration
 * @return Adapter handle or NULL on error
 */
attention_adapter_t attention_adapter_create(
    const attention_adapter_config_t* config
);

/**
 * WHAT: Destroy attention adapter
 * WHY:  Clean memory cleanup
 * HOW:  Free all resources
 *
 * @param adapter Adapter to destroy (NULL is safe)
 */
void attention_adapter_destroy(attention_adapter_t adapter);

/**
 * WHAT: Compute attention weights for channels
 * WHY:  Determine which channels to attend to
 * HOW:  Extract features, compute salience, apply gating
 *
 * @param adapter Adapter instance
 * @param activity Neural activity vector
 * @param num_channels Number of channels
 * @param timestamp Current timestamp
 * @param weights_out Output attention weights [num_channels]
 * @return Number of channels attended
 */
uint32_t attention_adapter_compute_weights(
    attention_adapter_t adapter,
    const float* activity,
    uint32_t num_channels,
    uint64_t timestamp,
    float* weights_out
);

/**
 * WHAT: Get attention metrics for specific channel
 * WHY:  Detailed attention analysis
 * HOW:  Compute per-channel salience metrics
 *
 * @param adapter Adapter instance
 * @param activity Neural activity vector
 * @param num_channels Number of channels
 * @param channel_idx Channel to analyze
 * @param timestamp Current timestamp
 * @param metrics_out Output attention metrics
 * @return true on success
 */
bool attention_adapter_get_channel_metrics(
    attention_adapter_t adapter,
    const float* activity,
    uint32_t num_channels,
    uint32_t channel_idx,
    uint64_t timestamp,
    attention_metrics_t* metrics_out
);

/**
 * WHAT: Get default attention adapter configuration
 * WHY:  Provide sensible defaults
 * HOW:  Return pre-filled config struct
 *
 * @return Default configuration
 */
attention_adapter_config_t attention_adapter_default_config(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_ATTENTION_ADAPTER_H
