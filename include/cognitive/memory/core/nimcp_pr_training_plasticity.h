//=============================================================================
// nimcp_pr_training_plasticity.h - Training-Plasticity Extension for Prime Resonant Memory
//=============================================================================
/**
 * @file nimcp_pr_training_plasticity.h
 * @brief Integration layer between PR memory plasticity and training layer
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bridge coordinating STDP/BCM plasticity with gradient-based training
 * WHY:  Enable unified learning that combines biological plasticity with
 *       modern deep learning optimization techniques
 * HOW:  Provides bidirectional conversion between gradients and plasticity
 *       events, supports multiple training modes (unified, alternating, hybrid)
 *
 * NEUROSCIENCE + ML INTEGRATION:
 *
 *   The Brain + Backprop Duality:
 *   +-----------------------------------------------------------------------+
 *   |                                                                        |
 *   |  BIOLOGICAL PLASTICITY                GRADIENT-BASED TRAINING         |
 *   |  +---------------------------+        +---------------------------+   |
 *   |  | STDP: Spike timing        |  <-->  | Backprop gradients        |   |
 *   |  | BCM: Rate-based rules     |        | Adam/SGD optimizers       |   |
 *   |  | Homeostatic: Stability    |        | Regularization/Dropout    |   |
 *   |  | Metaplasticity: History   |        | Learning rate schedules   |   |
 *   |  +---------------------------+        +---------------------------+   |
 *   |                     |                            |                    |
 *   |                     v                            v                    |
 *   |              +--------------------------------------+                 |
 *   |              | Training-Plasticity Extension        |                 |
 *   |              | - Unified update steps               |                 |
 *   |              | - Gradient <-> STDP conversion       |                 |
 *   |              | - Alternating training phases        |                 |
 *   |              | - Hybrid loss computation            |                 |
 *   |              +--------------------------------------+                 |
 *   |                                                                        |
 *   +-----------------------------------------------------------------------+
 *
 *   Training Modes:
 *   +-----------------------------------------------------------------------+
 *   | Mode                 | Description                                    |
 *   |----------------------|------------------------------------------------|
 *   | UNIFIED              | Combine gradient + plasticity in every step   |
 *   |                      | Weights = gradient_weight * grad_delta +      |
 *   |                      |           plasticity_weight * plast_delta     |
 *   |----------------------|------------------------------------------------|
 *   | ALTERNATING          | Switch between gradient and plasticity phases |
 *   |                      | Gradient phase: Standard backprop training    |
 *   |                      | Plasticity phase: STDP/BCM updates only       |
 *   |----------------------|------------------------------------------------|
 *   | GRADIENT_PRIMARY     | Gradients primary, plasticity as regularizer  |
 *   |                      | Plasticity prevents catastrophic forgetting   |
 *   |----------------------|------------------------------------------------|
 *   | PLASTICITY_PRIMARY   | Plasticity primary, gradients for fine-tuning |
 *   |                      | Biologically-inspired learning with guidance  |
 *   |----------------------|------------------------------------------------|
 *   | HYBRID               | Adaptive blend based on loss landscape        |
 *   |                      | Auto-adjust weights based on training phase   |
 *   +-----------------------------------------------------------------------+
 *
 *   Gradient -> STDP Timing Conversion:
 *   +-----------------------------------------------------------------------+
 *   |  Gradient Sign/Magnitude  -->  STDP Delta_t                           |
 *   |                                                                        |
 *   |  Positive gradient (want to strengthen):                              |
 *   |    -> Interpret as "pre-before-post" timing                           |
 *   |    -> delta_t = gradient_magnitude * timing_scale                     |
 *   |    -> Results in LTP (Long-Term Potentiation)                         |
 *   |                                                                        |
 *   |  Negative gradient (want to weaken):                                  |
 *   |    -> Interpret as "post-before-pre" timing                           |
 *   |    -> delta_t = -gradient_magnitude * timing_scale                    |
 *   |    -> Results in LTD (Long-Term Depression)                           |
 *   |                                                                        |
 *   |  Formula: delta_t = sign(grad) * |grad| * tau_stdp                    |
 *   |           Clipped to [-2*tau_stdp, +2*tau_stdp]                       |
 *   +-----------------------------------------------------------------------+
 *
 *   STDP -> Pseudo-Gradient Conversion:
 *   +-----------------------------------------------------------------------+
 *   |  STDP Events  -->  Gradient-like Updates                              |
 *   |                                                                        |
 *   |  LTP events (weight increase):                                        |
 *   |    -> pseudo_gradient = -learning_rate * delta_weight                 |
 *   |    -> Negative because gradient descent minimizes loss                |
 *   |                                                                        |
 *   |  LTD events (weight decrease):                                        |
 *   |    -> pseudo_gradient = +learning_rate * delta_weight                 |
 *   |                                                                        |
 *   |  Resonance modulation:                                                |
 *   |    -> High resonance edges get larger pseudo-gradients                |
 *   |    -> pseudo_grad *= (1 + resonance_weight * resonance)               |
 *   +-----------------------------------------------------------------------+
 *
 *   Hybrid Loss Function:
 *   +-----------------------------------------------------------------------+
 *   |  L_total = alpha * L_supervised + beta * L_unsupervised               |
 *   |                                                                        |
 *   |  L_supervised: Standard ML loss (cross-entropy, MSE, etc.)            |
 *   |  L_unsupervised: Plasticity-derived loss                              |
 *   |    - BCM energy: sum of |activity - theta|^2                          |
 *   |    - Homeostatic deviation: |mean_activity - target|^2                |
 *   |    - Resonance coherence: 1 - avg(resonance_scores)                   |
 *   |                                                                        |
 *   |  Adaptive weighting:                                                  |
 *   |    - Early training: higher alpha (supervised dominates)              |
 *   |    - Late training: higher beta (plasticity for stability)            |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Unified step: ~5ms per batch (1K parameters)
 * - Gradient->STDP conversion: ~100us per 1K gradients
 * - STDP->Gradient conversion: ~50us per 1K events
 * - Hybrid loss: ~200us per batch
 * - Epoch consolidation: ~50ms
 *
 * MEMORY:
 * - pr_training_plasticity_t: ~8KB base
 * - Gradient buffer: batch_size * param_count * 4 bytes
 * - Event buffer: max_events * 56 bytes
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe
 * - Internal mutex protects configuration and statistics
 * - Bridge operations are serialized per-instance
 *
 * INTEGRATION:
 * - Depends: nimcp_pr_plasticity_bridge.h for plasticity mechanisms
 * - Depends: nimcp_z_ladder.h for memory tier management
 * - Optional: Loss bridge for hybrid loss computation
 * - Optional: Optimizer bridge for gradient management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PR_TRAINING_PLASTICITY_H
#define NIMCP_PR_TRAINING_PLASTICITY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Prime Resonant core dependencies */
#include "cognitive/memory/core/nimcp_pr_plasticity_bridge.h"
#include "cognitive/memory/core/nimcp_z_ladder.h"
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_entanglement.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Export macro (for shared library builds) */
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum gradient elements to process in one batch */
#define PR_TRAIN_MAX_GRADIENT_BATCH         65536

/** Maximum plasticity events to convert per step */
#define PR_TRAIN_MAX_EVENTS_PER_STEP        4096

/** Default STDP timing scale (ms per unit gradient) */
#define PR_TRAIN_DEFAULT_TIMING_SCALE       20.0f

/** Default gradient weight in unified mode */
#define PR_TRAIN_DEFAULT_GRADIENT_WEIGHT    0.7f

/** Default plasticity weight in unified mode */
#define PR_TRAIN_DEFAULT_PLASTICITY_WEIGHT  0.3f

/** Default alternation period (steps) */
#define PR_TRAIN_DEFAULT_ALTERNATION_PERIOD 100

/** Default hybrid loss supervised weight */
#define PR_TRAIN_DEFAULT_SUPERVISED_WEIGHT  0.8f

/** Default hybrid loss unsupervised weight */
#define PR_TRAIN_DEFAULT_UNSUPERVISED_WEIGHT 0.2f

/** Maximum STDP timing delta (ms) */
#define PR_TRAIN_MAX_STDP_DELTA             40.0f

/** Minimum STDP timing delta (ms) */
#define PR_TRAIN_MIN_STDP_DELTA             0.1f

/** Default learning rate for pseudo-gradients */
#define PR_TRAIN_DEFAULT_PSEUDO_LR          0.01f

/** Default resonance weight for pseudo-gradients */
#define PR_TRAIN_DEFAULT_RESONANCE_WEIGHT   0.5f

/** Maximum consolidation events per epoch */
#define PR_TRAIN_MAX_CONSOLIDATION_EVENTS   1000

/** Epoch consolidation default batch size */
#define PR_TRAIN_CONSOLIDATION_BATCH_SIZE   256

/** Numerical epsilon */
#define PR_TRAIN_EPSILON                    1e-6f

/** Statistics history length */
#define PR_TRAIN_STATS_HISTORY_LEN          100

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Training mode enumeration
 *
 * WHAT: Defines how gradient and plasticity updates are combined
 * WHY:  Different tasks benefit from different training strategies
 *
 * Mode Selection Guide:
 * - UNIFIED: General purpose, balanced learning
 * - ALTERNATING: When plasticity/gradient conflict
 * - GRADIENT_PRIMARY: Standard ML with plasticity regularization
 * - PLASTICITY_PRIMARY: Biologically-realistic with ML guidance
 * - HYBRID: Adaptive, self-adjusting blend
 */
typedef enum {
    PR_TRAINING_UNIFIED = 0,      /**< Combine gradient + plasticity every step */
    PR_TRAINING_ALTERNATING,      /**< Alternate between gradient and plasticity */
    PR_TRAINING_GRADIENT_PRIMARY, /**< Gradients primary, plasticity regularizes */
    PR_TRAINING_PLASTICITY_PRIMARY, /**< Plasticity primary, gradients guide */
    PR_TRAINING_HYBRID,           /**< Adaptive blend based on loss landscape */
    PR_TRAINING_MODE_COUNT        /**< Number of training modes */
} pr_training_mode_t;

/**
 * @brief Training phase (for alternating mode)
 */
typedef enum {
    PR_PHASE_GRADIENT = 0,        /**< Gradient descent phase */
    PR_PHASE_PLASTICITY,          /**< Plasticity-only phase */
    PR_PHASE_CONSOLIDATION,       /**< Memory consolidation phase */
    PR_PHASE_EVALUATION           /**< Evaluation/inference phase */
} pr_training_phase_t;

/**
 * @brief Gradient-to-STDP conversion method
 */
typedef enum {
    PR_GRAD_TO_STDP_LINEAR = 0,   /**< Linear mapping: dt = grad * scale */
    PR_GRAD_TO_STDP_TANH,         /**< Tanh squashing: dt = tanh(grad) * max_dt */
    PR_GRAD_TO_STDP_SIGN,         /**< Sign-based: dt = sign(grad) * fixed_dt */
    PR_GRAD_TO_STDP_ADAPTIVE      /**< Adaptive based on gradient statistics */
} pr_grad_to_stdp_method_t;

/**
 * @brief STDP-to-gradient conversion method
 */
typedef enum {
    PR_STDP_TO_GRAD_DIRECT = 0,   /**< Direct mapping: grad = -delta_weight */
    PR_STDP_TO_GRAD_SCALED,       /**< Scaled by learning rate */
    PR_STDP_TO_GRAD_RESONANCE,    /**< Modulated by resonance score */
    PR_STDP_TO_GRAD_WEIGHTED      /**< Weighted by event type and tier */
} pr_stdp_to_grad_method_t;

/**
 * @brief Hybrid loss component type
 */
typedef enum {
    PR_LOSS_SUPERVISED = 0,       /**< Standard supervised loss */
    PR_LOSS_BCM_ENERGY,           /**< BCM deviation energy */
    PR_LOSS_HOMEOSTATIC,          /**< Homeostatic activity deviation */
    PR_LOSS_RESONANCE,            /**< Resonance coherence loss */
    PR_LOSS_SPARSITY,             /**< Sparsity constraint */
    PR_LOSS_CONSOLIDATION,        /**< Consolidation stability loss */
    PR_LOSS_COMPONENT_COUNT       /**< Number of loss components */
} pr_loss_component_t;

/**
 * @brief Gradient element structure
 *
 * WHAT: Single gradient value with metadata
 * WHY:  Enable gradient <-> plasticity conversion
 */
typedef struct {
    uint64_t param_id;            /**< Parameter identifier */
    float gradient;               /**< Gradient value */
    float current_value;          /**< Current parameter value */
    pr_memory_tier_t tier;        /**< Memory tier (if applicable) */
} pr_gradient_element_t;

/**
 * @brief STDP timing event structure
 *
 * WHAT: Converted gradient as STDP timing information
 * WHY:  Interface between gradient and plasticity systems
 */
typedef struct {
    uint64_t from_node;           /**< Pre-synaptic node */
    uint64_t to_node;             /**< Post-synaptic node */
    float delta_t_ms;             /**< Timing difference (ms) */
    float target_delta_weight;    /**< Expected weight change */
    float resonance;              /**< Current resonance score */
} pr_stdp_timing_t;

/**
 * @brief Pseudo-gradient from plasticity
 *
 * WHAT: Plasticity event converted to gradient-like update
 * WHY:  Allow plasticity to influence gradient optimization
 */
typedef struct {
    uint64_t param_id;            /**< Parameter identifier */
    float pseudo_gradient;        /**< Gradient-like value */
    float confidence;             /**< Confidence in conversion [0-1] */
    pr_plasticity_type_t source;  /**< Source plasticity type */
} pr_pseudo_gradient_t;

/**
 * @brief Hybrid loss result
 *
 * WHAT: Combined loss from supervised and unsupervised components
 * WHY:  Enable joint optimization with both objectives
 */
typedef struct {
    float total_loss;             /**< Combined total loss */
    float supervised_loss;        /**< Supervised component */
    float unsupervised_loss;      /**< Unsupervised component */
    float component_losses[PR_LOSS_COMPONENT_COUNT]; /**< Per-component */
    float supervised_weight;      /**< Current supervised weight */
    float unsupervised_weight;    /**< Current unsupervised weight */
} pr_hybrid_loss_t;

/**
 * @brief Training step result
 *
 * WHAT: Outcome of a single training step
 * WHY:  Track what happened during each step
 */
typedef struct {
    uint32_t weights_updated;     /**< Number of weights modified */
    float total_weight_change;    /**< Sum of |delta_weight| */
    float gradient_contribution;  /**< Fraction from gradients */
    float plasticity_contribution; /**< Fraction from plasticity */
    float step_loss;              /**< Loss at this step */
    float step_time_ms;           /**< Time taken (ms) */
    pr_training_phase_t phase;    /**< Current phase */
    bool consolidation_triggered; /**< Whether consolidation ran */
} pr_training_step_result_t;

/**
 * @brief Epoch consolidation result
 *
 * WHAT: Outcome of epoch-end consolidation
 * WHY:  Track memory consolidation effects
 */
typedef struct {
    uint32_t memories_promoted;   /**< Nodes promoted to higher tier */
    uint32_t memories_demoted;    /**< Nodes demoted to lower tier */
    uint32_t edges_strengthened;  /**< Entanglement edges strengthened */
    uint32_t edges_pruned;        /**< Edges pruned (structural plasticity) */
    float consolidation_time_ms;  /**< Time taken */
    float pre_consolidation_loss; /**< Loss before consolidation */
    float post_consolidation_loss; /**< Loss after consolidation */
} pr_epoch_consolidation_result_t;

/**
 * @brief Training statistics
 *
 * WHAT: Aggregate statistics for training progress
 * WHY:  Monitor training health and progress
 */
typedef struct {
    /* Step counts */
    uint64_t total_steps;                /**< Total training steps */
    uint64_t gradient_steps;             /**< Steps with gradient updates */
    uint64_t plasticity_steps;           /**< Steps with plasticity updates */
    uint64_t consolidation_steps;        /**< Consolidation steps */

    /* Update statistics */
    uint64_t total_weights_updated;      /**< Total weight updates */
    double total_gradient_contribution;  /**< Sum of gradient contributions */
    double total_plasticity_contribution; /**< Sum of plasticity contributions */

    /* Conversion statistics */
    uint64_t gradients_converted;        /**< Gradients -> STDP conversions */
    uint64_t events_converted;           /**< Plasticity -> gradient conversions */
    double avg_conversion_confidence;    /**< Average conversion confidence */

    /* Loss statistics */
    float min_loss;                      /**< Minimum loss observed */
    float max_loss;                      /**< Maximum loss observed */
    double loss_sum;                     /**< Sum of losses (for average) */
    float loss_history[PR_TRAIN_STATS_HISTORY_LEN]; /**< Recent loss values */
    uint32_t loss_history_idx;           /**< Current history index */

    /* Epoch statistics */
    uint64_t epochs_completed;           /**< Total epochs completed */
    uint64_t total_promotions;           /**< Total memory promotions */
    uint64_t total_demotions;            /**< Total memory demotions */

    /* Timing */
    double total_training_time_ms;       /**< Total time in training */
    double avg_step_time_ms;             /**< Average step time */

    /* Mode usage */
    uint64_t mode_step_counts[PR_TRAINING_MODE_COUNT]; /**< Steps per mode */
} pr_training_stats_t;

/**
 * @brief Training-Plasticity configuration
 *
 * WHAT: Complete configuration for training-plasticity integration
 * WHY:  Centralize all configurable parameters
 */
typedef struct {
    /* Mode configuration */
    pr_training_mode_t mode;              /**< Training mode */
    float gradient_weight;                /**< Weight for gradient updates [0-1] */
    float plasticity_weight;              /**< Weight for plasticity updates [0-1] */
    uint32_t alternation_period;          /**< Steps per phase (alternating mode) */
    bool consolidate_after_epoch;         /**< Run consolidation at epoch end */

    /* Gradient -> STDP conversion */
    pr_grad_to_stdp_method_t grad_to_stdp_method; /**< Conversion method */
    float timing_scale;                   /**< Scale factor for timing conversion */
    float max_stdp_delta;                 /**< Maximum STDP timing delta (ms) */
    float min_stdp_delta;                 /**< Minimum STDP timing delta (ms) */

    /* STDP -> Gradient conversion */
    pr_stdp_to_grad_method_t stdp_to_grad_method; /**< Conversion method */
    float pseudo_learning_rate;           /**< Learning rate for pseudo-gradients */
    float resonance_weight;               /**< Resonance modulation weight */

    /* Hybrid loss configuration */
    float supervised_weight;              /**< Weight for supervised loss */
    float unsupervised_weight;            /**< Weight for unsupervised loss */
    bool adaptive_loss_weights;           /**< Auto-adjust loss weights */
    float bcm_loss_weight;                /**< BCM energy contribution */
    float homeostatic_loss_weight;        /**< Homeostatic deviation contribution */
    float resonance_loss_weight;          /**< Resonance coherence contribution */
    float sparsity_loss_weight;           /**< Sparsity constraint contribution */
    float consolidation_loss_weight;      /**< Consolidation stability contribution */

    /* Epoch consolidation */
    float consolidation_strength_threshold; /**< Min strength for promotion */
    uint32_t consolidation_batch_size;    /**< Batch size for consolidation */
    bool sleep_consolidation;             /**< Use sleep-like deep consolidation */

    /* Performance tuning */
    uint32_t max_gradients_per_step;      /**< Max gradients to process */
    uint32_t max_events_per_step;         /**< Max plasticity events to process */
    bool enable_statistics;               /**< Track detailed statistics */
    bool verbose_logging;                 /**< Enable verbose debug output */
} pr_training_plasticity_config_t;

/**
 * @brief Opaque Training-Plasticity handle
 *
 * The bridge maintains:
 * - Configuration and current state
 * - Plasticity bridge connection
 * - Conversion buffers
 * - Statistics tracking
 * - Thread synchronization
 */
typedef struct pr_training_plasticity_struct* pr_training_plasticity_t;

/**
 * @brief Model parameter interface
 *
 * WHAT: Abstraction for model parameters to be updated
 * WHY:  Allow training bridge to work with various model types
 */
typedef struct {
    void* model_handle;                   /**< Opaque model handle */
    size_t param_count;                   /**< Number of parameters */

    /* Function pointers for model operations */
    int (*get_gradients)(void* model, pr_gradient_element_t* grads, size_t max_count, size_t* count);
    int (*apply_updates)(void* model, const pr_gradient_element_t* updates, size_t count);
    float (*compute_loss)(void* model, const void* batch);
    int (*zero_gradients)(void* model);
} pr_model_interface_t;

/**
 * @brief Training batch interface
 *
 * WHAT: Abstraction for training data batch
 * WHY:  Allow bridge to work with various data formats
 */
typedef struct {
    const void* data;                     /**< Batch data pointer */
    const void* labels;                   /**< Batch labels pointer */
    size_t batch_size;                    /**< Number of samples in batch */
    size_t sample_size;                   /**< Size of each sample */
} pr_training_batch_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default Training-Plasticity configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 *
 * @return Default configuration with:
 *         - mode: PR_TRAINING_UNIFIED
 *         - gradient_weight: 0.7
 *         - plasticity_weight: 0.3
 *         - alternation_period: 100
 *         - consolidate_after_epoch: true
 *         - timing_scale: 20.0 ms
 *         - supervised_weight: 0.8
 *         - unsupervised_weight: 0.2
 *
 * Performance: ~10ns
 *
 * Example:
 *   pr_training_plasticity_config_t config = pr_training_plasticity_config_default();
 *   config.mode = PR_TRAINING_ALTERNATING;
 *   config.alternation_period = 50;
 *   pr_training_plasticity_t tp = pr_training_plasticity_create(&config);
 */
NIMCP_EXPORT pr_training_plasticity_config_t pr_training_plasticity_config_default(void);

/**
 * @brief Validate Training-Plasticity configuration
 *
 * WHAT: Checks configuration for validity
 * WHY:  Prevent invalid configurations causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Validation rules:
 * - gradient_weight + plasticity_weight > 0
 * - timing_scale > 0
 * - supervised_weight + unsupervised_weight > 0
 * - alternation_period > 0 (if alternating mode)
 *
 * Performance: ~50ns
 */
NIMCP_EXPORT bool pr_training_plasticity_config_validate(
    const pr_training_plasticity_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create Training-Plasticity integration
 *
 * WHAT: Initialize training-plasticity integration layer
 * WHY:  Entry point for unified training with plasticity
 * HOW:  Allocate state, connect to plasticity bridge, initialize buffers
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle, or NULL on failure
 *
 * Performance: ~100us
 * Memory: ~8KB base + conversion buffers
 *
 * Example:
 *   pr_training_plasticity_config_t config = pr_training_plasticity_config_default();
 *   config.mode = PR_TRAINING_HYBRID;
 *   pr_training_plasticity_t tp = pr_training_plasticity_create(&config);
 *   if (!tp) {
 *       fprintf(stderr, "Failed to create training-plasticity bridge\n");
 *   }
 */
NIMCP_EXPORT pr_training_plasticity_t pr_training_plasticity_create(
    const pr_training_plasticity_config_t* config);

/**
 * @brief Destroy Training-Plasticity integration
 *
 * WHAT: Free all resources
 * WHY:  Clean shutdown
 *
 * @param tp Training-Plasticity handle (NULL safe)
 *
 * Performance: ~50us
 */
NIMCP_EXPORT void pr_training_plasticity_destroy(pr_training_plasticity_t tp);

/**
 * @brief Reset Training-Plasticity state
 *
 * WHAT: Reset statistics and internal state
 * WHY:  Start fresh training run
 *
 * @param tp Training-Plasticity handle
 * @return 0 on success, -1 on error
 *
 * Resets:
 * - Statistics counters
 * - Phase state
 * - Loss history
 * - Conversion buffers
 *
 * Performance: ~10us
 */
NIMCP_EXPORT int pr_training_plasticity_reset(pr_training_plasticity_t tp);

/**
 * @brief Connect to plasticity bridge
 *
 * WHAT: Establish connection to plasticity bridge
 * WHY:  Enable plasticity-based updates
 *
 * @param tp Training-Plasticity handle
 * @param plasticity_bridge Plasticity bridge to connect
 * @return 0 on success, -1 on error
 *
 * Performance: ~1us
 */
NIMCP_EXPORT int pr_training_plasticity_connect(
    pr_training_plasticity_t tp,
    pr_plasticity_bridge_t plasticity_bridge);

/**
 * @brief Connect to Z-Ladder for memory management
 *
 * WHAT: Establish connection to Z-Ladder
 * WHY:  Enable tier-aware training and consolidation
 *
 * @param tp Training-Plasticity handle
 * @param z_ladder Z-Ladder to connect
 * @return 0 on success, -1 on error
 *
 * Performance: ~1us
 */
NIMCP_EXPORT int pr_training_plasticity_connect_z_ladder(
    pr_training_plasticity_t tp,
    z_ladder_t z_ladder);

/**
 * @brief Connect to entanglement graph
 *
 * WHAT: Establish connection to entanglement graph
 * WHY:  Enable entanglement-aware training
 *
 * @param tp Training-Plasticity handle
 * @param graph Entanglement graph to connect
 * @return 0 on success, -1 on error
 *
 * Performance: ~1us
 */
NIMCP_EXPORT int pr_training_plasticity_connect_graph(
    pr_training_plasticity_t tp,
    entangle_graph_t graph);

//=============================================================================
// Core Training Functions
//=============================================================================

/**
 * @brief Perform unified training step
 *
 * WHAT: Single training step combining gradient and plasticity updates
 * WHY:  Primary training function for unified learning
 * HOW:  Computes gradients, converts to STDP, applies both updates
 *
 * @param tp Training-Plasticity handle
 * @param model Model interface for parameter access
 * @param batch Training batch data
 * @param memory Memory nodes affected by this batch
 * @param memory_count Number of memory nodes
 * @param result Output: step result details (can be NULL)
 * @return 0 on success, -1 on error
 *
 * Algorithm:
 * 1. Compute gradients via model interface
 * 2. Convert relevant gradients to STDP timing
 * 3. Apply STDP updates via plasticity bridge
 * 4. Apply gradient updates via model interface
 * 5. Blend updates according to mode and weights
 *
 * Performance: ~5ms per batch
 *
 * Example:
 *   pr_training_step_result_t result;
 *   for (int step = 0; step < num_steps; step++) {
 *       pr_training_batch_t batch = get_batch(step);
 *       if (pr_training_unified_step(tp, &model, &batch,
 *                                     memories, num_memories, &result) == 0) {
 *           printf("Step %d: loss=%.4f, updated=%u weights\n",
 *                  step, result.step_loss, result.weights_updated);
 *       }
 *   }
 */
NIMCP_EXPORT int pr_training_unified_step(
    pr_training_plasticity_t tp,
    pr_model_interface_t* model,
    const pr_training_batch_t* batch,
    pr_memory_node_t** memory,
    size_t memory_count,
    pr_training_step_result_t* result);

/**
 * @brief Perform alternating training step
 *
 * WHAT: Training step in alternating mode
 * WHY:  Separate gradient and plasticity phases
 * HOW:  Applies either gradient or plasticity based on current phase
 *
 * @param tp Training-Plasticity handle
 * @param model Model interface
 * @param batch Training batch
 * @param is_gradient_phase True for gradient phase, false for plasticity
 * @param result Output: step result (can be NULL)
 * @return 0 on success, -1 on error
 *
 * Algorithm:
 * - Gradient phase: Standard backprop update
 * - Plasticity phase: STDP/BCM updates only
 *
 * Performance: ~3ms per step
 */
NIMCP_EXPORT int pr_training_alternating_step(
    pr_training_plasticity_t tp,
    pr_model_interface_t* model,
    const pr_training_batch_t* batch,
    bool is_gradient_phase,
    pr_training_step_result_t* result);

/**
 * @brief Auto-advance alternating phase
 *
 * WHAT: Automatically determine and advance phase
 * WHY:  Handle phase transitions automatically
 * HOW:  Tracks step count, switches at alternation_period
 *
 * @param tp Training-Plasticity handle
 * @return Current phase after potential advance
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT pr_training_phase_t pr_training_advance_phase(
    pr_training_plasticity_t tp);

//=============================================================================
// Gradient <-> Plasticity Conversion Functions
//=============================================================================

/**
 * @brief Convert gradients to STDP timing events
 *
 * WHAT: Map gradient values to STDP pre/post timing differences
 * WHY:  Enable gradients to drive STDP-based learning
 * HOW:  Sign determines direction, magnitude determines timing
 *
 * @param tp Training-Plasticity handle
 * @param gradients Input gradient array
 * @param grad_count Number of gradients
 * @param timing Output STDP timing array (caller-allocated)
 * @param max_timing Maximum timing events to output
 * @param timing_count Output: actual timing events generated
 * @return 0 on success, -1 on error
 *
 * Conversion:
 * - Positive gradient -> positive delta_t (LTP)
 * - Negative gradient -> negative delta_t (LTD)
 * - Magnitude scaled by timing_scale
 *
 * Performance: ~100us per 1K gradients
 *
 * Example:
 *   pr_gradient_element_t grads[1000];
 *   // ... fill gradients ...
 *   pr_stdp_timing_t timing[1000];
 *   size_t timing_count;
 *   pr_training_gradient_to_stdp(tp, grads, 1000, timing, 1000, &timing_count);
 */
NIMCP_EXPORT int pr_training_gradient_to_stdp(
    pr_training_plasticity_t tp,
    const pr_gradient_element_t* gradients,
    size_t grad_count,
    pr_stdp_timing_t* timing,
    size_t max_timing,
    size_t* timing_count);

/**
 * @brief Convert plasticity events to pseudo-gradients
 *
 * WHAT: Map plasticity events to gradient-like updates
 * WHY:  Allow plasticity to influence gradient optimizer
 * HOW:  Weight changes map to pseudo-gradients
 *
 * @param tp Training-Plasticity handle
 * @param events Input plasticity events
 * @param event_count Number of events
 * @param pseudo_grads Output pseudo-gradient array (caller-allocated)
 * @param max_grads Maximum pseudo-gradients to output
 * @param grad_count Output: actual pseudo-gradients generated
 * @return 0 on success, -1 on error
 *
 * Conversion:
 * - LTP (weight increase) -> negative pseudo-gradient
 * - LTD (weight decrease) -> positive pseudo-gradient
 * - Modulated by resonance if configured
 *
 * Performance: ~50us per 1K events
 */
NIMCP_EXPORT int pr_training_plasticity_to_grad(
    pr_training_plasticity_t tp,
    const pr_plasticity_event_t* events,
    size_t event_count,
    pr_pseudo_gradient_t* pseudo_grads,
    size_t max_grads,
    size_t* grad_count);

/**
 * @brief Apply STDP timing to entanglement edge
 *
 * WHAT: Apply converted STDP timing to update edge weight
 * WHY:  Execute gradient-derived plasticity
 *
 * @param tp Training-Plasticity handle
 * @param graph Entanglement graph
 * @param timing STDP timing event to apply
 * @return New edge weight, or -1.0f on error
 *
 * Performance: ~50ns per edge
 */
NIMCP_EXPORT float pr_training_apply_stdp_timing(
    pr_training_plasticity_t tp,
    entangle_graph_t graph,
    const pr_stdp_timing_t* timing);

/**
 * @brief Batch apply STDP timing events
 *
 * WHAT: Apply multiple STDP timing events efficiently
 * WHY:  Batch processing for performance
 *
 * @param tp Training-Plasticity handle
 * @param graph Entanglement graph
 * @param timing Array of timing events
 * @param count Number of events
 * @return Number of edges successfully updated
 *
 * Performance: ~40ns per edge (amortized)
 */
NIMCP_EXPORT uint32_t pr_training_batch_apply_stdp(
    pr_training_plasticity_t tp,
    entangle_graph_t graph,
    const pr_stdp_timing_t* timing,
    size_t count);

//=============================================================================
// Hybrid Loss Functions
//=============================================================================

/**
 * @brief Compute hybrid loss (supervised + unsupervised)
 *
 * WHAT: Compute combined loss from multiple components
 * WHY:  Enable joint optimization with multiple objectives
 * HOW:  Weighted combination of supervised and unsupervised losses
 *
 * @param tp Training-Plasticity handle
 * @param supervised_loss Pre-computed supervised loss
 * @param unsupervised_loss Pre-computed unsupervised loss (can be NaN for auto)
 * @param loss Output: hybrid loss result
 * @return 0 on success, -1 on error
 *
 * Formula:
 *   total = supervised_weight * supervised + unsupervised_weight * unsupervised
 *
 * If unsupervised_loss is NaN, computes from plasticity state:
 *   unsupervised = bcm_energy + homeostatic_dev + resonance_loss + sparsity
 *
 * Performance: ~200us
 *
 * Example:
 *   pr_hybrid_loss_t loss;
 *   float supervised = compute_cross_entropy(model, batch);
 *   pr_training_hybrid_loss(tp, supervised, NAN, &loss);
 *   printf("Total: %.4f (sup: %.4f, unsup: %.4f)\n",
 *          loss.total_loss, loss.supervised_loss, loss.unsupervised_loss);
 */
NIMCP_EXPORT int pr_training_hybrid_loss(
    pr_training_plasticity_t tp,
    float supervised_loss,
    float unsupervised_loss,
    pr_hybrid_loss_t* loss);

/**
 * @brief Compute unsupervised loss from plasticity state
 *
 * WHAT: Derive unsupervised loss from current plasticity state
 * WHY:  Auto-compute unsupervised component for hybrid loss
 *
 * @param tp Training-Plasticity handle
 * @param loss Output: computed unsupervised loss
 * @return 0 on success, -1 on error
 *
 * Components:
 * - BCM energy: deviation from BCM threshold
 * - Homeostatic: deviation from target activity
 * - Resonance: inverse of mean resonance (want high resonance)
 * - Sparsity: activation sparsity constraint
 *
 * Performance: ~100us
 */
NIMCP_EXPORT int pr_training_compute_unsupervised_loss(
    pr_training_plasticity_t tp,
    float* loss);

/**
 * @brief Compute individual loss component
 *
 * WHAT: Compute a specific loss component
 * WHY:  Detailed loss breakdown
 *
 * @param tp Training-Plasticity handle
 * @param component Which component to compute
 * @return Component loss value, or NaN on error
 *
 * Performance: ~20us
 */
NIMCP_EXPORT float pr_training_compute_loss_component(
    pr_training_plasticity_t tp,
    pr_loss_component_t component);

/**
 * @brief Update adaptive loss weights
 *
 * WHAT: Adjust supervised/unsupervised weights based on training progress
 * WHY:  Optimal weighting changes during training
 * HOW:  Based on loss landscape and convergence metrics
 *
 * @param tp Training-Plasticity handle
 * @param recent_loss Array of recent loss values
 * @param loss_count Number of loss values
 * @return 0 on success, -1 on error
 *
 * Heuristic:
 * - Early training (high loss variance): favor supervised
 * - Late training (low loss variance): increase unsupervised
 *
 * Performance: ~50us
 */
NIMCP_EXPORT int pr_training_update_loss_weights(
    pr_training_plasticity_t tp,
    const float* recent_loss,
    size_t loss_count);

//=============================================================================
// Epoch and Consolidation Functions
//=============================================================================

/**
 * @brief Perform epoch-end consolidation
 *
 * WHAT: Memory consolidation at end of training epoch
 * WHY:  Promote/demote memories based on training outcomes
 * HOW:  Uses Z-Ladder promotion criteria with training metrics
 *
 * @param tp Training-Plasticity handle
 * @param result Output: consolidation results (can be NULL)
 * @return 0 on success, -1 on error
 *
 * Algorithm:
 * 1. Compute promotion eligibility based on:
 *    - Access frequency during epoch
 *    - Resonance improvement
 *    - Gradient magnitude history
 * 2. Promote eligible memories to higher tiers
 * 3. Demote weak memories
 * 4. Apply structural plasticity (edge pruning/creation)
 *
 * Performance: ~50ms
 *
 * Example:
 *   for (int epoch = 0; epoch < num_epochs; epoch++) {
 *       // ... training steps ...
 *       pr_epoch_consolidation_result_t result;
 *       pr_training_epoch_consolidate(tp, &result);
 *       printf("Epoch %d: promoted=%u, demoted=%u\n",
 *              epoch, result.memories_promoted, result.memories_demoted);
 *   }
 */
NIMCP_EXPORT int pr_training_epoch_consolidate(
    pr_training_plasticity_t tp,
    pr_epoch_consolidation_result_t* result);

/**
 * @brief Signal epoch start
 *
 * WHAT: Notify bridge of new epoch starting
 * WHY:  Reset per-epoch tracking
 *
 * @param tp Training-Plasticity handle
 * @param epoch_number Current epoch number
 * @return 0 on success, -1 on error
 *
 * Performance: ~1us
 */
NIMCP_EXPORT int pr_training_epoch_start(
    pr_training_plasticity_t tp,
    uint32_t epoch_number);

/**
 * @brief Signal epoch end
 *
 * WHAT: Notify bridge of epoch ending
 * WHY:  Trigger epoch-end processing
 *
 * @param tp Training-Plasticity handle
 * @param trigger_consolidation Whether to run consolidation
 * @return 0 on success, -1 on error
 *
 * Performance: ~1us (without consolidation)
 */
NIMCP_EXPORT int pr_training_epoch_end(
    pr_training_plasticity_t tp,
    bool trigger_consolidation);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get current training phase
 *
 * WHAT: Query current phase in alternating mode
 * WHY:  External code may need to know current phase
 *
 * @param tp Training-Plasticity handle
 * @return Current phase, or PR_PHASE_GRADIENT on error
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT pr_training_phase_t pr_training_get_phase(
    pr_training_plasticity_t tp);

/**
 * @brief Set training mode
 *
 * WHAT: Change training mode at runtime
 * WHY:  Allow dynamic mode switching
 *
 * @param tp Training-Plasticity handle
 * @param mode New training mode
 * @return 0 on success, -1 on error
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT int pr_training_set_mode(
    pr_training_plasticity_t tp,
    pr_training_mode_t mode);

/**
 * @brief Get current training mode
 *
 * @param tp Training-Plasticity handle
 * @return Current mode, or PR_TRAINING_UNIFIED on error
 */
NIMCP_EXPORT pr_training_mode_t pr_training_get_mode(
    pr_training_plasticity_t tp);

/**
 * @brief Get training statistics
 *
 * WHAT: Retrieve accumulated training statistics
 * WHY:  Monitor training progress and health
 *
 * @param tp Training-Plasticity handle
 * @param stats Output: statistics structure
 * @return 0 on success, -1 on error
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT int pr_training_get_stats(
    pr_training_plasticity_t tp,
    pr_training_stats_t* stats);

/**
 * @brief Reset training statistics
 *
 * @param tp Training-Plasticity handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_training_reset_stats(pr_training_plasticity_t tp);

/**
 * @brief Get current configuration
 *
 * @param tp Training-Plasticity handle
 * @param config Output: current configuration
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_training_get_config(
    pr_training_plasticity_t tp,
    pr_training_plasticity_config_t* config);

/**
 * @brief Update configuration at runtime
 *
 * WHAT: Change configuration parameters
 * WHY:  Allow dynamic tuning
 *
 * Note: Not all parameters can be changed after creation
 *
 * @param tp Training-Plasticity handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT int pr_training_set_config(
    pr_training_plasticity_t tp,
    const pr_training_plasticity_config_t* config);

//=============================================================================
// Synchronization Functions
//=============================================================================

/**
 * @brief Synchronize all connected bridges
 *
 * WHAT: Ensure all bridges have consistent state
 * WHY:  After multiple updates, sync for consistency
 *
 * @param tp Training-Plasticity handle
 * @return 0 on success, -1 on error
 *
 * Syncs:
 * - Plasticity bridge state
 * - Z-Ladder tier assignments
 * - Entanglement edge weights
 *
 * Performance: ~500us
 */
NIMCP_EXPORT int pr_training_sync_all(pr_training_plasticity_t tp);

/**
 * @brief Sync with plasticity bridge
 *
 * @param tp Training-Plasticity handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_training_sync_plasticity(pr_training_plasticity_t tp);

/**
 * @brief Sync with Z-Ladder
 *
 * @param tp Training-Plasticity handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_training_sync_z_ladder(pr_training_plasticity_t tp);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get training mode name as string
 *
 * @param mode Training mode
 * @return Human-readable string
 */
NIMCP_EXPORT const char* pr_training_mode_name(pr_training_mode_t mode);

/**
 * @brief Get training phase name as string
 *
 * @param phase Training phase
 * @return Human-readable string
 */
NIMCP_EXPORT const char* pr_training_phase_name(pr_training_phase_t phase);

/**
 * @brief Get loss component name as string
 *
 * @param component Loss component
 * @return Human-readable string
 */
NIMCP_EXPORT const char* pr_loss_component_name(pr_loss_component_t component);

/**
 * @brief Print training statistics to stdout
 *
 * @param tp Training-Plasticity handle
 */
NIMCP_EXPORT void pr_training_print_stats(pr_training_plasticity_t tp);

/**
 * @brief Print step result to stdout
 *
 * @param result Step result to print
 */
NIMCP_EXPORT void pr_training_print_step_result(
    const pr_training_step_result_t* result);

/**
 * @brief Print epoch consolidation result to stdout
 *
 * @param result Consolidation result to print
 */
NIMCP_EXPORT void pr_training_print_consolidation_result(
    const pr_epoch_consolidation_result_t* result);

/**
 * @brief Validate model interface
 *
 * WHAT: Check that model interface is properly configured
 * WHY:  Catch configuration errors early
 *
 * @param model Model interface to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool pr_training_validate_model_interface(
    const pr_model_interface_t* model);

/**
 * @brief Get estimated memory usage
 *
 * @param tp Training-Plasticity handle
 * @return Estimated memory usage in bytes
 */
NIMCP_EXPORT size_t pr_training_get_memory_usage(pr_training_plasticity_t tp);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_TRAINING_PLASTICITY_H */
