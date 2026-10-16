/**
 * @file nimcp_weight_update_adapter.h
 * @brief Middleware adapter for synaptic weight updates
 *
 * WHAT: Connects middleware features to synaptic plasticity rules
 * WHY:  Enable biologically-inspired weight updates from neural activity
 * HOW:  Extract features, apply STDP/BCM/Hebbian rules, modulate updates
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#ifndef NIMCP_WEIGHT_UPDATE_ADAPTER_H
#define NIMCP_WEIGHT_UPDATE_ADAPTER_H

#include "middleware/brain_integration.h"
#include "middleware/training/nimcp_learning_signal_adapter.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * WHAT: Weight update adapter instance
 * WHY:  Manage middleware-to-plasticity connection
 * HOW:  Maintain spike timing, activity history
 */
typedef struct weight_update_adapter_struct* weight_update_adapter_t;

/**
 * WHAT: Plasticity rule type
 * WHY:  Different learning rules for different contexts
 * HOW:  Categorize by biological mechanism
 */
typedef enum {
    PLASTICITY_STDP,        /**< Spike-timing dependent plasticity */
    PLASTICITY_BCM,         /**< BCM sliding threshold */
    PLASTICITY_HEBBIAN,     /**< Simple Hebbian */
    PLASTICITY_TRIPLE,      /**< Triplet STDP */
    PLASTICITY_VOLTAGE      /**< Voltage-based plasticity */
} plasticity_rule_t;

/**
 * WHAT: Weight update adapter configuration
 * WHY:  Customize plasticity computation
 * HOW:  Specify rules, rates, bounds
 */
typedef struct {
    uint32_t num_pre;            /**< Number of presynaptic neurons */
    uint32_t num_post;           /**< Number of postsynaptic neurons */
    plasticity_rule_t rule;      /**< Plasticity rule to use */
    float learning_rate;         /**< Base learning rate [0-1] */
    float tau_plus;              /**< LTP time constant (ms) */
    float tau_minus;             /**< LTD time constant (ms) */
    float weight_min;            /**< Minimum weight value */
    float weight_max;            /**< Maximum weight value */
    bool enable_normalization;   /**< Normalize weight updates */
    bool enable_homeostasis;     /**< Enable homeostatic scaling */
} weight_update_adapter_config_t;

/**
 * WHAT: Weight update delta
 * WHY:  Provide detailed update information
 * HOW:  Store delta, mechanism, metadata
 */
typedef struct {
    float** delta_weights;       /**< Weight changes [num_pre x num_post] */
    plasticity_rule_t rule_used; /**< Which rule generated update */
    float mean_delta;            /**< Mean weight change */
    float max_delta;             /**< Maximum weight change */
    uint32_t num_updated;        /**< Number of weights updated */
    uint64_t timestamp;          /**< When update was computed */
} weight_update_delta_t;

/**
 * WHAT: Create weight update adapter
 * WHY:  Initialize middleware connection
 * HOW:  Allocate spike timing trackers and plasticity engines
 *
 * @param config Adapter configuration
 * @return Adapter handle or NULL on error
 */
weight_update_adapter_t weight_update_adapter_create(
    const weight_update_adapter_config_t* config
);

/**
 * WHAT: Destroy weight update adapter
 * WHY:  Clean memory cleanup
 * HOW:  Free all resources
 *
 * @param adapter Adapter to destroy (NULL is safe)
 */
void weight_update_adapter_destroy(weight_update_adapter_t adapter);

/**
 * WHAT: Compute weight updates from pre/post activity
 * WHY:  Apply plasticity rules to compute weight changes
 * HOW:  Extract spike timings, apply STDP/BCM/etc rules
 *
 * @param adapter Adapter instance
 * @param pre_activity Presynaptic activity [num_pre]
 * @param post_activity Postsynaptic activity [num_post]
 * @param learning_signal Learning signal (modulates updates)
 * @param timestamp Current timestamp
 * @param delta_out Output weight delta
 * @return true on success
 */
bool weight_update_adapter_compute(
    weight_update_adapter_t adapter,
    const float* pre_activity,
    const float* post_activity,
    const learning_signal_t* learning_signal,
    uint64_t timestamp,
    weight_update_delta_t* delta_out
);

/**
 * WHAT: Apply homeostatic scaling to weights
 * WHY:  Maintain stable firing rates
 * HOW:  Scale weights to achieve target activity
 *
 * @param adapter Adapter instance
 * @param current_weights Current weight matrix [num_pre x num_post]
 * @param target_rate Target postsynaptic rate (Hz)
 * @param actual_rate Actual postsynaptic rate (Hz)
 * @return true on success (weights modified in-place)
 */
bool weight_update_adapter_apply_homeostasis(
    weight_update_adapter_t adapter,
    float** current_weights,
    float target_rate,
    float actual_rate
);

/**
 * WHAT: Get default weight update adapter configuration
 * WHY:  Provide sensible defaults
 * HOW:  Return pre-filled config struct
 *
 * @return Default configuration
 */
weight_update_adapter_config_t weight_update_adapter_default_config(void);

/**
 * WHAT: Create weight update delta structure
 * WHY:  Allocate storage for weight changes
 * HOW:  Allocate 2D array
 *
 * @param num_pre Number of presynaptic neurons
 * @param num_post Number of postsynaptic neurons
 * @return Allocated delta or NULL on error
 */
weight_update_delta_t* weight_update_delta_create(
    uint32_t num_pre,
    uint32_t num_post
);

/**
 * WHAT: Destroy weight update delta structure
 * WHY:  Free allocated memory
 * HOW:  Free 2D array and structure
 *
 * @param delta Delta to destroy (NULL is safe)
 */
void weight_update_delta_destroy(weight_update_delta_t* delta);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_WEIGHT_UPDATE_ADAPTER_H
