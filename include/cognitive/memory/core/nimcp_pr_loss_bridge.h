//=============================================================================
// nimcp_pr_loss_bridge.h - Prime Resonant Loss Bridge
//=============================================================================
/**
 * @file nimcp_pr_loss_bridge.h
 * @brief Bridge between Prime Resonant memory system and training loss functions
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bridge connecting Prime Resonant memory (quaternion states, resonance,
 *       entanglement, Z-ladder tiers) with training loss computation for
 *       memory-aware neural network optimization
 * WHY:  Enable memory-aware loss computation where:
 *       - Quaternion geodesic loss respects hypersphere geometry
 *       - Resonance-triplet loss uses semantic similarity instead of Euclidean
 *       - Consolidation weighting protects well-consolidated memories
 *       - Entanglement regularization encourages memory connectivity
 *       - Tier-aware loss applies different weights per Z-ladder tier
 * HOW:  Specialized loss functions that integrate memory metadata:
 *       - Geodesic distance on quaternion manifold for state loss
 *       - Resonance scoring for triplet margin computation
 *       - Consolidation (quat.w) as loss weighting factor
 *       - Entanglement graph density as regularization term
 *       - Z0-Z3 tier-specific loss scaling
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Memory-Aware Loss Functions:
 *   +-----------------------------------------------------------------------+
 *   |                                                                        |
 *   |  1. QUATERNION GEODESIC LOSS:                                          |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Measures distance on unit hypersphere, not Euclidean space    |   |
 *   |  |                                                                 |   |
 *   |  |  d(q1, q2) = arccos(|q1 . q2|)                                  |   |
 *   |  |                                                                 |   |
 *   |  |  WHY: Memory states live on S^3, not R^4                       |   |
 *   |  |       Geodesic respects manifold geometry                       |   |
 *   |  |       Avoids artifacts from Euclidean approximation             |   |
 *   |  |                                                                 |   |
 *   |  |  BIOLOGICAL: Similar memories cluster on hypersphere            |   |
 *   |  |              Consolidation path follows geodesic arc            |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  2. RESONANCE-TRIPLET LOSS:                                            |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Uses resonance scoring instead of Euclidean distance          |   |
 *   |  |                                                                 |   |
 *   |  |  L = max(0, res(a,n) - res(a,p) + margin)                       |   |
 *   |  |                                                                 |   |
 *   |  |  WHERE:                                                         |   |
 *   |  |    a = anchor memory                                            |   |
 *   |  |    p = positive (should be similar)                             |   |
 *   |  |    n = negative (should be different)                           |   |
 *   |  |                                                                 |   |
 *   |  |  WHY: Resonance captures semantic similarity                    |   |
 *   |  |       Combines content, phase, state, and sync                  |   |
 *   |  |       Biologically meaningful similarity metric                 |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  3. CONSOLIDATION-WEIGHTED LOSS:                                       |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Weight loss by inverse consolidation (quat.w)                  |   |
 *   |  |                                                                 |   |
 *   |  |  L = sum(w_i * L_i) where w_i = 1 - consol_i^power              |   |
 *   |  |                                                                 |   |
 *   |  |  WHY: Well-consolidated memories should not be overwritten     |   |
 *   |  |       Fragile memories can be freely updated                    |   |
 *   |  |       Prevents catastrophic forgetting                          |   |
 *   |  |                                                                 |   |
 *   |  |  BIOLOGICAL: Synaptic tagging protects important memories       |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  4. ENTANGLEMENT REGULARIZATION:                                       |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Penalize memories with low entanglement connectivity           |   |
 *   |  |                                                                 |   |
 *   |  |  R = lambda * mean(1 - entangle_score_i)                        |   |
 *   |  |                                                                 |   |
 *   |  |  WHY: Encourage memories to form associations                   |   |
 *   |  |       Isolated memories are less useful                         |   |
 *   |  |       Promotes schema formation                                 |   |
 *   |  |                                                                 |   |
 *   |  |  BIOLOGICAL: Memory binding through neural synchrony            |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  5. TIER-AWARE LOSS:                                                   |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Different loss weights per Z-ladder tier                       |   |
 *   |  |                                                                 |   |
 *   |  |  Z0 (Working):   High weight - active learning                  |   |
 *   |  |  Z1 (Short-term): Medium-high - consolidating                   |   |
 *   |  |  Z2 (Long-term):  Medium-low - protected                        |   |
 *   |  |  Z3 (Permanent):  Low weight - highly protected                 |   |
 *   |  |                                                                 |   |
 *   |  |  WHY: Match biological memory protection hierarchy              |   |
 *   |  |       Semantic memories resist modification                     |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   +-----------------------------------------------------------------------+
 *
 *   Loss Gradient Flow:
 *   +-----------------------------------------------------------------------+
 *   |                                                                        |
 *   |  Combined Loss = w1*L_geodesic + w2*L_triplet + w3*L_consolidation    |
 *   |                  + w4*R_entanglement + w5*L_tier                       |
 *   |                                                                        |
 *   |  Gradient:       dL/dq for quaternion state optimization              |
 *   |                  dL/dw for edge weight optimization                    |
 *   |                                                                        |
 *   |  Back-propagation respects:                                            |
 *   |  - Quaternion manifold constraints (project gradients to tangent)     |
 *   |  - Consolidation gating (reduce gradient for high consolidation)      |
 *   |  - Tier-specific learning rates                                       |
 *   |                                                                        |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Single geodesic loss: ~30ns
 * - Batch geodesic (N=100): ~2us
 * - Resonance triplet: ~150ns (includes resonance computation)
 * - Consolidation weighted: ~50ns per sample
 * - Entanglement regularization: O(|E|) edges
 * - Combined loss: ~500ns for typical batch
 *
 * MEMORY:
 * - pr_loss_bridge_t: ~1KB base
 * - Per-batch overhead: proportional to batch size
 * - Statistics tracking: ~200 bytes
 *
 * INTEGRATION:
 * - Core: Quaternion states, resonance engine, entanglement graph
 * - Middleware: Standard loss functions, training pipeline
 * - Memory Nodes: Z-ladder tiers, consolidation state
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PR_LOSS_BRIDGE_H
#define NIMCP_PR_LOSS_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Prime Resonant core dependencies */
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_resonance.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"

/* Training loss functions */
#include "middleware/training/nimcp_loss_functions.h"
#include "constants/nimcp_math_constants.h"

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

/** Pi constant */

/** Default geodesic loss weight */
#define PR_LOSS_DEFAULT_GEODESIC_WEIGHT      0.3f

/** Default triplet loss weight */
#define PR_LOSS_DEFAULT_TRIPLET_WEIGHT       0.3f

/** Default triplet loss margin */
#define PR_LOSS_DEFAULT_TRIPLET_MARGIN       0.2f

/** Default consolidation loss weight */
#define PR_LOSS_DEFAULT_CONSOLIDATION_WEIGHT 0.2f

/** Default entanglement regularization lambda */
#define PR_LOSS_DEFAULT_ENTANGLEMENT_LAMBDA  0.1f

/** Default tier weight for Z0 (working memory) */
#define PR_LOSS_DEFAULT_TIER_WEIGHT_Z0       1.0f

/** Default tier weight for Z1 (short-term) */
#define PR_LOSS_DEFAULT_TIER_WEIGHT_Z1       0.7f

/** Default tier weight for Z2 (long-term) */
#define PR_LOSS_DEFAULT_TIER_WEIGHT_Z2       0.4f

/** Default tier weight for Z3 (permanent) */
#define PR_LOSS_DEFAULT_TIER_WEIGHT_Z3       0.1f

/** Maximum batch size for loss computation */
#define PR_LOSS_MAX_BATCH_SIZE               65536

/** Number of memory tiers */
#define PR_LOSS_NUM_TIERS                    4

/** Numerical epsilon for stability */
#define PR_LOSS_EPSILON                      1e-6f

/** Default consolidation power exponent */
#define PR_LOSS_DEFAULT_CONSOLIDATION_POWER  2.0f

/** Minimum entanglement score for regularization */
#define PR_LOSS_MIN_ENTANGLEMENT_SCORE       0.0f

/** Maximum entanglement score for regularization */
#define PR_LOSS_MAX_ENTANGLEMENT_SCORE       1.0f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Loss function types for Prime Resonant memory
 *
 * WHAT: Types of loss functions specialized for memory training
 * WHY:  Different losses capture different memory properties
 */
typedef enum {
    PR_LOSS_GEODESIC = 0,       /**< Quaternion geodesic distance */
    PR_LOSS_RESONANCE_TRIPLET,  /**< Triplet loss using resonance */
    PR_LOSS_CONSOLIDATION,      /**< Consolidation-weighted MSE */
    PR_LOSS_ENTANGLEMENT_REG,   /**< Entanglement regularization */
    PR_LOSS_TIER_WEIGHTED,      /**< Tier-aware loss weighting */
    PR_LOSS_COMBINED,           /**< Combined loss (all above) */
    PR_LOSS_TYPE_COUNT          /**< Number of loss types */
} pr_loss_type_t;

/**
 * @brief Loss configuration for Prime Resonant bridge
 *
 * WHAT: Parameters controlling loss computation behavior
 * WHY:  Customize loss weighting for different training scenarios
 *
 * TYPICAL CONFIGURATIONS:
 * - State Learning: High geodesic weight, low consolidation protection
 * - Fine-tuning: Low weights overall, high consolidation protection
 * - Retrieval Training: High triplet weight, emphasize resonance
 * - Schema Learning: High entanglement regularization
 */
typedef struct {
    float geodesic_weight;           /**< Weight for geodesic loss [0,1] */
    float triplet_weight;            /**< Weight for triplet loss [0,1] */
    float triplet_margin;            /**< Margin for triplet loss (default 0.2) */
    float consolidation_weight;      /**< Weight for consolidation loss [0,1] */
    float consolidation_power;       /**< Power for consolidation decay (default 2.0) */
    float entanglement_lambda;       /**< Lambda for entanglement reg (default 0.1) */
    float tier_weights[PR_LOSS_NUM_TIERS]; /**< Per-tier loss weights [Z0-Z3] */
} pr_loss_config_t;

/**
 * @brief Result of loss computation
 *
 * WHAT: Complete loss result with component breakdown
 * WHY:  Track individual contributions for analysis and debugging
 */
typedef struct {
    float total_loss;                /**< Combined total loss value */
    float geodesic_loss;             /**< Geodesic component contribution */
    float triplet_loss;              /**< Triplet component contribution */
    float consolidation_loss;        /**< Consolidation-weighted component */
    float entanglement_reg;          /**< Entanglement regularization term */
    float tier_weighted_loss;        /**< Tier-weighted component */
    uint32_t sample_count;           /**< Number of samples in batch */
    uint64_t compute_time_ns;        /**< Computation time in nanoseconds */
} pr_loss_result_t;

/**
 * @brief Gradient result for quaternion loss
 *
 * WHAT: Gradient of loss with respect to quaternion components
 * WHY:  Enable gradient-based optimization on quaternion manifold
 *
 * NOTE: Gradient is projected to tangent space of hypersphere
 */
typedef struct {
    float dw;                        /**< Gradient w.r.t. w (consolidation) */
    float dx;                        /**< Gradient w.r.t. x (emotion) */
    float dy;                        /**< Gradient w.r.t. y (salience) */
    float dz;                        /**< Gradient w.r.t. z (accessibility) */
} pr_loss_quat_gradient_t;

/**
 * @brief Gradient result for triplet loss
 *
 * WHAT: Gradients for anchor, positive, and negative samples
 * WHY:  Triplet loss has three distinct gradient contributions
 */
typedef struct {
    pr_loss_quat_gradient_t anchor;   /**< Gradient for anchor quaternion */
    pr_loss_quat_gradient_t positive; /**< Gradient for positive quaternion */
    pr_loss_quat_gradient_t negative; /**< Gradient for negative quaternion */
    float margin_violation;           /**< How much margin was violated */
    bool is_active;                   /**< Whether triplet contributed to loss */
} pr_loss_triplet_gradient_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Operational metrics for the loss bridge
 * WHY:  Track bridge health and performance
 */
typedef struct {
    /* Computation counts */
    uint64_t geodesic_computations;       /**< Number of geodesic loss calls */
    uint64_t triplet_computations;        /**< Number of triplet loss calls */
    uint64_t consolidation_computations;  /**< Number of consolidation loss calls */
    uint64_t entanglement_computations;   /**< Number of entanglement reg calls */
    uint64_t combined_computations;       /**< Number of combined loss calls */

    /* Loss value statistics */
    float total_geodesic_loss;            /**< Cumulative geodesic loss */
    float total_triplet_loss;             /**< Cumulative triplet loss */
    float total_consolidation_loss;       /**< Cumulative consolidation loss */
    float total_entanglement_reg;         /**< Cumulative regularization */
    float avg_combined_loss;              /**< Running average combined loss */
    float min_combined_loss;              /**< Minimum combined loss seen */
    float max_combined_loss;              /**< Maximum combined loss seen */

    /* Gradient statistics */
    uint64_t gradient_computations;       /**< Number of gradient calls */
    float avg_gradient_norm;              /**< Average gradient L2 norm */
    float max_gradient_norm;              /**< Maximum gradient norm */
    uint64_t gradient_clips;              /**< Number of gradient clips */

    /* Triplet statistics */
    uint64_t active_triplets;             /**< Triplets with non-zero loss */
    uint64_t inactive_triplets;           /**< Triplets with zero loss */
    float avg_margin_violation;           /**< Average margin violation */

    /* Tier statistics */
    uint64_t samples_per_tier[PR_LOSS_NUM_TIERS]; /**< Samples processed per tier */
    float loss_per_tier[PR_LOSS_NUM_TIERS];       /**< Avg loss per tier */

    /* Timing */
    uint64_t total_compute_time_ns;       /**< Total computation time */
    float avg_compute_time_us;            /**< Average computation time */
} pr_loss_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct pr_loss_bridge_struct* pr_loss_bridge_t;

//=============================================================================
// Forward Declarations
//=============================================================================

/* Forward declare entanglement graph from nimcp_entanglement.h */
typedef struct entangle_graph_struct* entangle_graph_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default loss bridge configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides biologically-plausible starting point
 *
 * @return Default configuration with:
 *         - geodesic_weight: 0.3
 *         - triplet_weight: 0.3
 *         - triplet_margin: 0.2
 *         - consolidation_weight: 0.2
 *         - entanglement_lambda: 0.1
 *         - tier_weights: [1.0, 0.7, 0.4, 0.1]
 *
 * Performance: ~5ns
 *
 * Example:
 *   pr_loss_config_t config = pr_loss_config_default();
 *   config.triplet_margin = 0.3f;  // Increase margin
 *   pr_loss_bridge_t bridge = pr_loss_bridge_create(&config);
 */
NIMCP_EXPORT pr_loss_config_t pr_loss_config_default(void);

/**
 * @brief Get configuration for state learning
 *
 * WHAT: Configuration emphasizing quaternion state learning
 * WHY:  For training memory state representations
 *
 * @return Config with high geodesic weight, low consolidation protection
 */
NIMCP_EXPORT pr_loss_config_t pr_loss_config_state_learning(void);

/**
 * @brief Get configuration for retrieval training
 *
 * WHAT: Configuration emphasizing resonance-based retrieval
 * WHY:  For training memory retrieval mechanisms
 *
 * @return Config with high triplet weight
 */
NIMCP_EXPORT pr_loss_config_t pr_loss_config_retrieval(void);

/**
 * @brief Get configuration for fine-tuning
 *
 * WHAT: Configuration with high consolidation protection
 * WHY:  For fine-tuning without catastrophic forgetting
 *
 * @return Config with low weights, high consolidation protection
 */
NIMCP_EXPORT pr_loss_config_t pr_loss_config_fine_tuning(void);

/**
 * @brief Validate loss configuration
 *
 * WHAT: Check configuration for validity
 * WHY:  Prevent invalid parameters causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Validation rules:
 * - All weights must be >= 0
 * - At least one loss type must have weight > 0
 * - Triplet margin must be > 0
 * - Tier weights must be in [0, 1]
 *
 * Performance: ~20ns
 */
NIMCP_EXPORT bool pr_loss_config_validate(const pr_loss_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create loss bridge
 *
 * WHAT: Initialize loss bridge for Prime Resonant memory
 * WHY:  Entry point for memory-aware loss computation
 * HOW:  Allocate state, initialize parameters, prepare caches
 *
 * @param config Loss configuration (NULL for defaults)
 * @return Bridge handle, or NULL on failure
 *
 * Performance: O(1)
 * Memory: ~1KB base
 *
 * Thread safety: The returned bridge is thread-safe for concurrent use
 *
 * Example:
 *   pr_loss_config_t config = pr_loss_config_default();
 *   config.geodesic_weight = 0.5f;
 *   pr_loss_bridge_t bridge = pr_loss_bridge_create(&config);
 */
NIMCP_EXPORT pr_loss_bridge_t pr_loss_bridge_create(const pr_loss_config_t* config);

/**
 * @brief Destroy loss bridge
 *
 * WHAT: Free all bridge resources
 * WHY:  Proper resource cleanup
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * Performance: O(1)
 */
NIMCP_EXPORT void pr_loss_bridge_destroy(pr_loss_bridge_t bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset statistics and internal state
 * WHY:  Start fresh measurement period
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_loss_bridge_reset(pr_loss_bridge_t bridge);

/**
 * @brief Update bridge configuration
 *
 * WHAT: Modify bridge configuration at runtime
 * WHY:  Allow dynamic adjustment of loss parameters
 *
 * @param bridge Bridge to update
 * @param config New configuration
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_loss_bridge_set_config(
    pr_loss_bridge_t bridge,
    const pr_loss_config_t* config);

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge to query
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_loss_bridge_get_config(
    pr_loss_bridge_t bridge,
    pr_loss_config_t* config);

//=============================================================================
// Quaternion Geodesic Loss Functions
//=============================================================================

/**
 * @brief Compute quaternion geodesic loss between two states
 *
 * WHAT: Geodesic distance on unit quaternion hypersphere
 * WHY:  Memory states live on S^3; respects manifold geometry
 * HOW:  L = arccos(|q1 . q2|) / pi (normalized to [0, 1])
 *
 * MATHEMATICAL FORMULATION:
 *   d(q1, q2) = arccos(|q1 . q2|)
 *   L = d(q1, q2) / pi  (normalized)
 *
 * @param bridge Loss bridge (can be NULL for standalone use)
 * @param q1 First quaternion state
 * @param q2 Second quaternion state (target)
 * @return Normalized geodesic loss [0, 1], or -1.0f on error
 *
 * Performance: ~30ns
 *
 * Example:
 *   nimcp_quaternion_t predicted = quat_create(0.9f, 0.1f, 0.8f, 0.7f);
 *   nimcp_quaternion_t target = quat_create(0.85f, 0.15f, 0.75f, 0.65f);
 *   float loss = pr_loss_geodesic(bridge, predicted, target);
 *   // loss ~ 0.05 (small distance)
 */
NIMCP_EXPORT float pr_loss_geodesic(
    pr_loss_bridge_t bridge,
    nimcp_quaternion_t q1,
    nimcp_quaternion_t q2);

/**
 * @brief Compute batch quaternion geodesic loss
 *
 * WHAT: Geodesic loss for arrays of quaternion pairs
 * WHY:  Efficient batch processing for training
 * HOW:  Vectorized computation where possible
 *
 * @param bridge Loss bridge
 * @param quats1 Array of predicted quaternions
 * @param quats2 Array of target quaternions
 * @param count Number of pairs
 * @param reduction Reduction mode (mean, sum, none)
 * @param per_sample_loss Optional output for per-sample losses (can be NULL)
 * @return Reduced loss value, or -1.0f on error
 *
 * Performance: ~25ns per pair (amortized)
 *
 * Example:
 *   nimcp_quaternion_t preds[100], targets[100];
 *   float losses[100];
 *   float total = pr_loss_geodesic_batch(
 *       bridge, preds, targets, 100,
 *       NIMCP_LOSS_REDUCE_MEAN, losses);
 */
NIMCP_EXPORT float pr_loss_geodesic_batch(
    pr_loss_bridge_t bridge,
    const nimcp_quaternion_t* quats1,
    const nimcp_quaternion_t* quats2,
    size_t count,
    nimcp_loss_reduction_t reduction,
    float* per_sample_loss);

/**
 * @brief Compute gradient of geodesic loss
 *
 * WHAT: Gradient of geodesic loss w.r.t. first quaternion
 * WHY:  Enable gradient-based optimization
 * HOW:  Project Euclidean gradient onto tangent space of S^3
 *
 * GRADIENT FORMULA:
 *   dL/dq1 = -sign(dot) * q2 / (pi * sqrt(1 - dot^2))
 *   (projected to tangent space at q1)
 *
 * @param bridge Loss bridge
 * @param q1 First quaternion (w.r.t. which we differentiate)
 * @param q2 Second quaternion (target)
 * @param grad Output gradient
 * @return 0 on success, -1 on error
 *
 * Performance: ~40ns
 *
 * Example:
 *   pr_loss_quat_gradient_t grad;
 *   pr_loss_gradient_geodesic(bridge, predicted, target, &grad);
 *   // Apply gradient descent: q_new = normalize(q - lr * grad)
 */
NIMCP_EXPORT int pr_loss_gradient_geodesic(
    pr_loss_bridge_t bridge,
    nimcp_quaternion_t q1,
    nimcp_quaternion_t q2,
    pr_loss_quat_gradient_t* grad);

/**
 * @brief Batch compute geodesic gradients
 *
 * @param bridge Loss bridge
 * @param quats1 Array of predicted quaternions
 * @param quats2 Array of target quaternions
 * @param count Number of pairs
 * @param grads Output gradient array (pre-allocated)
 * @return Number of gradients computed, or -1 on error
 *
 * Performance: ~35ns per pair (amortized)
 */
NIMCP_EXPORT int pr_loss_gradient_geodesic_batch(
    pr_loss_bridge_t bridge,
    const nimcp_quaternion_t* quats1,
    const nimcp_quaternion_t* quats2,
    size_t count,
    pr_loss_quat_gradient_t* grads);

//=============================================================================
// Resonance-Triplet Loss Functions
//=============================================================================

/**
 * @brief Compute resonance-triplet loss
 *
 * WHAT: Triplet loss using resonance instead of Euclidean distance
 * WHY:  Resonance captures semantic similarity better than L2
 * HOW:  L = max(0, res(a,n) - res(a,p) + margin)
 *
 * ALGORITHM:
 *   1. Compute resonance(anchor, positive) using resonance engine
 *   2. Compute resonance(anchor, negative)
 *   3. L = max(0, res_neg - res_pos + margin)
 *
 * NOTE: Higher resonance = more similar, so we want res_pos > res_neg
 *
 * @param bridge Loss bridge
 * @param anchor Anchor memory node
 * @param positive Positive sample (should be similar to anchor)
 * @param negative Negative sample (should be different from anchor)
 * @return Triplet loss value [0, inf), or -1.0f on error
 *
 * Performance: ~150ns (includes resonance computation)
 *
 * Example:
 *   pr_memory_node_t* anchor = ...;
 *   pr_memory_node_t* similar = ...;  // Same category
 *   pr_memory_node_t* different = ...; // Different category
 *   float loss = pr_loss_resonance_triplet(bridge, anchor, similar, different);
 */
NIMCP_EXPORT float pr_loss_resonance_triplet(
    pr_loss_bridge_t bridge,
    const pr_memory_node_t* anchor,
    const pr_memory_node_t* positive,
    const pr_memory_node_t* negative);

/**
 * @brief Compute resonance-triplet loss from quaternion states
 *
 * WHAT: Triplet loss using only quaternion state similarity
 * WHY:  When full memory nodes aren't available
 * HOW:  Uses quaternion geodesic as similarity metric
 *
 * FORMULA:
 *   sim(q1, q2) = 1 - geodesic(q1, q2)
 *   L = max(0, sim(a,n) - sim(a,p) + margin)
 *
 * @param bridge Loss bridge
 * @param anchor Anchor quaternion state
 * @param positive Positive quaternion state
 * @param negative Negative quaternion state
 * @return Triplet loss value
 *
 * Performance: ~60ns
 */
NIMCP_EXPORT float pr_loss_resonance_triplet_quat(
    pr_loss_bridge_t bridge,
    nimcp_quaternion_t anchor,
    nimcp_quaternion_t positive,
    nimcp_quaternion_t negative);

/**
 * @brief Batch resonance-triplet loss
 *
 * @param bridge Loss bridge
 * @param anchors Array of anchor nodes
 * @param positives Array of positive nodes
 * @param negatives Array of negative nodes
 * @param count Number of triplets
 * @param reduction Reduction mode
 * @param per_sample_loss Optional per-triplet losses
 * @return Reduced loss value
 *
 * Performance: ~120ns per triplet (amortized)
 */
NIMCP_EXPORT float pr_loss_resonance_triplet_batch(
    pr_loss_bridge_t bridge,
    const pr_memory_node_t* const* anchors,
    const pr_memory_node_t* const* positives,
    const pr_memory_node_t* const* negatives,
    size_t count,
    nimcp_loss_reduction_t reduction,
    float* per_sample_loss);

/**
 * @brief Compute triplet loss gradient
 *
 * WHAT: Gradients for all three triplet components
 * WHY:  Enable triplet loss optimization
 * HOW:  Chain rule through resonance computation
 *
 * @param bridge Loss bridge
 * @param anchor Anchor quaternion
 * @param positive Positive quaternion
 * @param negative Negative quaternion
 * @param grads Output gradient structure
 * @return 0 on success, -1 on error
 *
 * Performance: ~80ns
 */
NIMCP_EXPORT int pr_loss_gradient_triplet(
    pr_loss_bridge_t bridge,
    nimcp_quaternion_t anchor,
    nimcp_quaternion_t positive,
    nimcp_quaternion_t negative,
    pr_loss_triplet_gradient_t* grads);

//=============================================================================
// Consolidation-Weighted Loss Functions
//=============================================================================

/**
 * @brief Compute consolidation-weighted loss
 *
 * WHAT: MSE loss weighted by inverse consolidation
 * WHY:  Protect well-consolidated memories from modification
 * HOW:  L = sum(w_i * (pred_i - target_i)^2)
 *       where w_i = 1 - consolidation_i^power
 *
 * BIOLOGICAL ANALOGY:
 *   - High consolidation (quat.w near 1) -> low weight -> protected
 *   - Low consolidation (quat.w near 0) -> high weight -> modifiable
 *
 * @param bridge Loss bridge
 * @param predictions Array of predicted values
 * @param targets Array of target values
 * @param nodes Array of memory nodes (for consolidation state)
 * @param count Number of samples
 * @return Consolidation-weighted loss, or -1.0f on error
 *
 * Performance: ~50ns per sample
 *
 * Example:
 *   float preds[100], targets[100];
 *   pr_memory_node_t* nodes[100];
 *   float loss = pr_loss_consolidation_weighted(
 *       bridge, preds, targets, nodes, 100);
 */
NIMCP_EXPORT float pr_loss_consolidation_weighted(
    pr_loss_bridge_t bridge,
    const float* predictions,
    const float* targets,
    const pr_memory_node_t* const* nodes,
    size_t count);

/**
 * @brief Compute consolidation weight for a single node
 *
 * WHAT: Get loss weight based on consolidation state
 * WHY:  Preview weight for debugging or custom loss functions
 * HOW:  w = 1 - consolidation^power
 *
 * @param bridge Loss bridge
 * @param node Memory node to query
 * @return Weight value [0, 1]
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT float pr_loss_get_consolidation_weight(
    pr_loss_bridge_t bridge,
    const pr_memory_node_t* node);

/**
 * @brief Compute consolidation-weighted gradient scale
 *
 * WHAT: Gradient scaling factor based on consolidation
 * WHY:  Reduce gradients for consolidated memories
 *
 * @param bridge Loss bridge
 * @param consolidation Consolidation value [0, 1]
 * @return Gradient scale factor [0, 1]
 */
NIMCP_EXPORT float pr_loss_consolidation_gradient_scale(
    pr_loss_bridge_t bridge,
    float consolidation);

//=============================================================================
// Entanglement Regularization Functions
//=============================================================================

/**
 * @brief Compute entanglement regularization
 *
 * WHAT: Penalize memories with low entanglement connectivity
 * WHY:  Encourage memories to form associations/schemas
 * HOW:  R = lambda * mean(1 - entangle_score_i)
 *
 * ALGORITHM:
 *   1. For each node, compute entanglement score (0-1)
 *   2. Isolation penalty = 1 - score
 *   3. R = lambda * mean(isolation penalties)
 *
 * @param bridge Loss bridge
 * @param graph Entanglement graph
 * @return Regularization term, or -1.0f on error
 *
 * Performance: O(|V|) where V = number of nodes
 *
 * Example:
 *   entangle_graph_t graph = ...;
 *   float reg = pr_loss_entanglement_reg(bridge, graph);
 *   total_loss += reg;  // Add to total loss
 */
NIMCP_EXPORT float pr_loss_entanglement_reg(
    pr_loss_bridge_t bridge,
    entangle_graph_t graph);

/**
 * @brief Compute entanglement regularization for subset of nodes
 *
 * @param bridge Loss bridge
 * @param graph Entanglement graph
 * @param node_ids Array of node IDs to include
 * @param count Number of nodes
 * @return Regularization term
 *
 * Performance: O(count)
 */
NIMCP_EXPORT float pr_loss_entanglement_reg_nodes(
    pr_loss_bridge_t bridge,
    entangle_graph_t graph,
    const uint64_t* node_ids,
    size_t count);

/**
 * @brief Compute entanglement score for a single node
 *
 * WHAT: Normalized entanglement connectivity score
 * WHY:  Measure how well-connected a memory is
 * HOW:  score = tanh(edge_count * edge_weight_sum / normalization)
 *
 * @param bridge Loss bridge
 * @param graph Entanglement graph
 * @param node_id Node to query
 * @return Entanglement score [0, 1]
 *
 * Performance: O(degree)
 */
NIMCP_EXPORT float pr_loss_get_entanglement_score(
    pr_loss_bridge_t bridge,
    entangle_graph_t graph,
    uint64_t node_id);

/**
 * @brief Compute gradient of entanglement regularization
 *
 * WHAT: Gradient for edge weight optimization
 * WHY:  Enable regularization to affect training
 *
 * @param bridge Loss bridge
 * @param graph Entanglement graph
 * @param edge_gradients Output gradient for each edge (pre-allocated)
 * @param max_edges Maximum edges to process
 * @return Number of gradients computed
 */
NIMCP_EXPORT int pr_loss_gradient_entanglement(
    pr_loss_bridge_t bridge,
    entangle_graph_t graph,
    float* edge_gradients,
    size_t max_edges);

//=============================================================================
// Tier-Aware Loss Functions
//=============================================================================

/**
 * @brief Compute tier-aware weighted loss
 *
 * WHAT: Apply different loss weights per memory tier
 * WHY:  Match biological memory protection hierarchy
 * HOW:  L = sum(tier_weight[tier_i] * L_i)
 *
 * TIER SEMANTICS:
 *   Z0 (Working):   High weight (1.0) - active learning
 *   Z1 (Short-term): Medium-high (0.7) - consolidating
 *   Z2 (Long-term):  Medium-low (0.4) - protected
 *   Z3 (Permanent):  Low weight (0.1) - highly protected
 *
 * @param bridge Loss bridge
 * @param predictions Array of predictions
 * @param targets Array of targets
 * @param nodes Array of memory nodes (for tier info)
 * @param count Number of samples
 * @return Tier-weighted loss, or -1.0f on error
 *
 * Performance: ~40ns per sample
 */
NIMCP_EXPORT float pr_loss_tier_aware(
    pr_loss_bridge_t bridge,
    const float* predictions,
    const float* targets,
    const pr_memory_node_t* const* nodes,
    size_t count);

/**
 * @brief Get tier weight
 *
 * @param bridge Loss bridge
 * @param tier Memory tier
 * @return Weight for specified tier
 */
NIMCP_EXPORT float pr_loss_get_tier_weight(
    pr_loss_bridge_t bridge,
    pr_memory_tier_t tier);

/**
 * @brief Set tier weight
 *
 * @param bridge Loss bridge
 * @param tier Memory tier
 * @param weight New weight value [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_loss_set_tier_weight(
    pr_loss_bridge_t bridge,
    pr_memory_tier_t tier,
    float weight);

//=============================================================================
// Combined Loss Functions
//=============================================================================

/**
 * @brief Compute combined loss (all components)
 *
 * WHAT: Full memory-aware loss combining all components
 * WHY:  Single entry point for typical training scenarios
 * HOW:  L = w1*geodesic + w2*triplet + w3*consolidation + w4*entanglement + w5*tier
 *
 * ALGORITHM:
 *   1. Compute geodesic loss between predicted and target states
 *   2. Compute triplet loss if triplets provided
 *   3. Apply consolidation weighting
 *   4. Add entanglement regularization
 *   5. Apply tier-aware weighting
 *   6. Combine with configured weights
 *
 * @param bridge Loss bridge
 * @param predictions Array of predicted quaternion states
 * @param targets Array of target quaternion states
 * @param nodes Array of memory nodes (for metadata)
 * @param graph Entanglement graph (can be NULL to skip reg)
 * @param count Number of samples
 * @param result Output result with component breakdown
 * @return 0 on success, -1 on error
 *
 * Performance: ~500ns per sample typical
 *
 * Example:
 *   pr_loss_result_t result;
 *   pr_loss_combined(bridge, preds, targets, nodes, graph, 100, &result);
 *   printf("Total: %.4f (geo=%.4f, trip=%.4f, cons=%.4f, ent=%.4f)\n",
 *          result.total_loss, result.geodesic_loss,
 *          result.triplet_loss, result.consolidation_loss,
 *          result.entanglement_reg);
 */
NIMCP_EXPORT int pr_loss_combined(
    pr_loss_bridge_t bridge,
    const nimcp_quaternion_t* predictions,
    const nimcp_quaternion_t* targets,
    const pr_memory_node_t* const* nodes,
    entangle_graph_t graph,
    size_t count,
    pr_loss_result_t* result);

/**
 * @brief Compute combined loss with triplet data
 *
 * WHAT: Combined loss including explicit triplet samples
 * WHY:  When triplet mining is done externally
 *
 * @param bridge Loss bridge
 * @param predictions Predicted states
 * @param targets Target states
 * @param nodes Memory nodes
 * @param graph Entanglement graph
 * @param triplet_anchors Triplet anchor indices
 * @param triplet_positives Triplet positive indices
 * @param triplet_negatives Triplet negative indices
 * @param sample_count Number of samples
 * @param triplet_count Number of triplets
 * @param result Output result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_loss_combined_with_triplets(
    pr_loss_bridge_t bridge,
    const nimcp_quaternion_t* predictions,
    const nimcp_quaternion_t* targets,
    const pr_memory_node_t* const* nodes,
    entangle_graph_t graph,
    const size_t* triplet_anchors,
    const size_t* triplet_positives,
    const size_t* triplet_negatives,
    size_t sample_count,
    size_t triplet_count,
    pr_loss_result_t* result);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve operational metrics
 * WHY:  Monitoring and debugging
 *
 * @param bridge Loss bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_loss_get_stats(
    pr_loss_bridge_t bridge,
    pr_loss_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bridge Loss bridge
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_loss_reset_stats(pr_loss_bridge_t bridge);

/**
 * @brief Print statistics summary
 *
 * @param bridge Loss bridge
 */
NIMCP_EXPORT void pr_loss_print_stats(pr_loss_bridge_t bridge);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get loss type name as string
 *
 * @param type Loss type
 * @return Human-readable string
 */
NIMCP_EXPORT const char* pr_loss_type_name(pr_loss_type_t type);

/**
 * @brief Print loss result
 *
 * @param result Result to print
 */
NIMCP_EXPORT void pr_loss_result_print(const pr_loss_result_t* result);

/**
 * @brief Apply gradient to quaternion state
 *
 * WHAT: Update quaternion using gradient descent
 * WHY:  Utility for training loop
 * HOW:  q_new = normalize(q - lr * grad)
 *
 * @param q Input quaternion
 * @param grad Gradient
 * @param learning_rate Learning rate
 * @return Updated quaternion (normalized)
 *
 * Performance: ~20ns
 */
NIMCP_EXPORT nimcp_quaternion_t pr_loss_apply_gradient(
    nimcp_quaternion_t q,
    const pr_loss_quat_gradient_t* grad,
    float learning_rate);

/**
 * @brief Clip gradient by norm
 *
 * WHAT: Clip gradient to maximum L2 norm
 * WHY:  Prevent gradient explosion
 *
 * @param grad Gradient to clip (modified in place)
 * @param max_norm Maximum allowed norm
 * @return Original norm before clipping
 */
NIMCP_EXPORT float pr_loss_clip_gradient(
    pr_loss_quat_gradient_t* grad,
    float max_norm);

/**
 * @brief Compute gradient L2 norm
 *
 * @param grad Gradient
 * @return L2 norm of gradient
 */
NIMCP_EXPORT float pr_loss_gradient_norm(const pr_loss_quat_gradient_t* grad);

/**
 * @brief Initialize loss result structure
 *
 * @param result Result to initialize
 */
NIMCP_EXPORT void pr_loss_result_init(pr_loss_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_LOSS_BRIDGE_H */
