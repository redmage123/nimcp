//=============================================================================
// nimcp_pr_continual_bridge.h - Continual Learning Bridge for Prime Resonant Memory
//=============================================================================
/**
 * @file nimcp_pr_continual_bridge.h
 * @brief EWC-based continual learning with quaternion consolidation protection
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bridge connecting Prime Resonant memory to continual learning mechanisms,
 *       implementing Elastic Weight Consolidation (EWC) with quaternion-based
 *       importance weighting to prevent catastrophic forgetting
 * WHY:  Neural networks suffer from catastrophic forgetting when learning new
 *       tasks - consolidated memories (high quat.w) should be protected from
 *       modification while still allowing new learning
 * HOW:  Combines EWC Fisher information with quaternion consolidation state,
 *       tier-based protection, and entanglement-aware gradient protection
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Catastrophic Forgetting Prevention:
 *   +-----------------------------------------------------------------------+
 *   |                                                                        |
 *   |  BIOLOGICAL MEMORY PROTECTION:                                         |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Hippocampus acts as a buffer, preventing direct overwriting   |   |
 *   |  |  of neocortical representations. Sleep consolidation transfers |   |
 *   |  |  important memories to protected long-term storage.            |   |
 *   |  |                                                                 |   |
 *   |  |  Key mechanisms:                                                |   |
 *   |  |  - Synaptic tagging: Mark important synapses for protection    |   |
 *   |  |  - Protein synthesis: Structural changes make memories stable  |   |
 *   |  |  - Interleaved replay: Prevent interference during learning    |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  EWC COMPUTATIONAL ANALOG:                                             |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Fisher Information: How much each parameter contributes to    |   |
 *   |  |  current task performance (importance weighting)               |   |
 *   |  |                                                                 |   |
 *   |  |  L_total = L_new_task + (lambda/2) * sum(F_i * (theta_i -      |   |
 *   |  |            theta_i*)^2)                                         |   |
 *   |  |                                                                 |   |
 *   |  |  Where:                                                         |   |
 *   |  |  - F_i = Fisher information for parameter i                     |   |
 *   |  |  - theta_i* = optimal parameters after previous tasks          |   |
 *   |  |  - lambda = protection strength                                 |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   +-----------------------------------------------------------------------+
 *
 *   Prime Resonant Continual Learning Integration:
 *   +-----------------------------------------------------------------------+
 *   |                                                                        |
 *   |  QUATERNION CONSOLIDATION (w component):                               |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Memory consolidation strength gates protection:                |   |
 *   |  |                                                                 |   |
 *   |  |  w < 0.3:  "Fragile" -> Minimal protection, high plasticity    |   |
 *   |  |  0.3-0.6:  "Stabilizing" -> Moderate protection                |   |
 *   |  |  0.6-0.9:  "Consolidated" -> Strong protection                 |   |
 *   |  |  w >= 0.9: "Permanent" -> Maximum protection (Z3-like)         |   |
 *   |  |                                                                 |   |
 *   |  |  Effective Fisher: F_eff = F * (1 + alpha * w^2)               |   |
 *   |  |  Higher consolidation = stronger protection                     |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  Z-LADDER TIER PROTECTION:                                             |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Tier | Protection | Description                                |   |
 *   |  |------|------------|-------------------------------------------|   |
 *   |  |  Z0  | 0.1        | Working memory - fully plastic             |   |
 *   |  |  Z1  | 0.4        | Short-term - some protection               |   |
 *   |  |  Z2  | 0.7        | Long-term - significant protection         |   |
 *   |  |  Z3  | 1.0        | Permanent - maximum protection             |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  ENTANGLEMENT-AWARE UPDATES:                                           |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Highly entangled nodes are more important for network         |   |
 *   |  |  structure - protect them proportionally:                       |   |
 *   |  |                                                                 |   |
 *   |  |  entangle_protection = min(1.0, entangle_count / threshold)    |   |
 *   |  |  total_protection = max(tier_prot, entangle_prot, quat_prot)   |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  EXPERIENCE REPLAY FROM Z-LADDER:                                      |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Sample memories from different tiers for interleaved training:|   |
 *   |  |                                                                 |   |
 *   |  |  - Z0: Fresh samples (recent experiences)                       |   |
 *   |  |  - Z1: Short-term replay (hours-old memories)                   |   |
 *   |  |  - Z2: Long-term replay (consolidated knowledge)                |   |
 *   |  |  - Z3: Anchor samples (core knowledge protection)               |   |
 *   |  |                                                                 |   |
 *   |  |  Replay ratio controls mix of new vs old data                   |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   +-----------------------------------------------------------------------+
 *
 *   Task Boundary Handling:
 *   +-----------------------------------------------------------------------+
 *   |  1. Compute Fisher information for completed task                      |
 *   |  2. Weight by quaternion consolidation state                          |
 *   |  3. Store optimal parameters as reference point                        |
 *   |  4. Promote high-importance memories in Z-Ladder                      |
 *   |  5. Prune low-importance, low-resonance entanglements                 |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Fisher computation: O(n * m) where n = params, m = samples
 * - EWC loss: O(n) where n = parameters
 * - Importance calculation: O(1) per node
 * - Replay sampling: O(k) where k = batch_size
 * - Protection application: O(n) where n = parameters
 *
 * MEMORY:
 * - pr_continual_bridge_t: ~4KB base + Fisher storage
 * - Fisher information: 4 bytes per parameter
 * - Old parameters: 4 bytes per parameter
 * - Task history: ~100 bytes per task
 *
 * INTEGRATION:
 * - Core: Quaternion state, entanglement graph, Z-Ladder
 * - Plasticity: Protection coordinates with STDP/BCM
 * - Training: Gradient modification before optimization
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PR_CONTINUAL_BRIDGE_H
#define NIMCP_PR_CONTINUAL_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Prime Resonant core dependencies */
#include "nimcp_quaternion.h"
#include "nimcp_resonance.h"
#include "nimcp_z_ladder.h"
#include "nimcp_entanglement.h"
#include "utils/memory/nimcp_memory.h"

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

/** Maximum number of tasks to track history for */
#define PR_CONTINUAL_MAX_TASKS              100

/** Default EWC lambda (protection strength) */
#define PR_CONTINUAL_DEFAULT_LAMBDA         1000.0f

/** Default consolidation weight in Fisher scaling */
#define PR_CONTINUAL_DEFAULT_CONSOL_WEIGHT  2.0f

/** Default replay ratio (fraction of batch that is replay) */
#define PR_CONTINUAL_DEFAULT_REPLAY_RATIO   0.5f

/** Default replay batch size */
#define PR_CONTINUAL_DEFAULT_REPLAY_BATCH   32

/** Default protection threshold per tier (Z0) */
#define PR_CONTINUAL_TIER_PROTECT_Z0        0.1f

/** Default protection threshold per tier (Z1) */
#define PR_CONTINUAL_TIER_PROTECT_Z1        0.4f

/** Default protection threshold per tier (Z2) */
#define PR_CONTINUAL_TIER_PROTECT_Z2        0.7f

/** Default protection threshold per tier (Z3) */
#define PR_CONTINUAL_TIER_PROTECT_Z3        1.0f

/** Default entanglement threshold for protection */
#define PR_CONTINUAL_ENTANGLE_THRESHOLD     10

/** Minimum Fisher value to track (sparsity) */
#define PR_CONTINUAL_FISHER_EPSILON         1e-8f

/** Maximum Fisher value (clamp extreme values) */
#define PR_CONTINUAL_FISHER_MAX             1e6f

/** Default number of samples for Fisher estimation */
#define PR_CONTINUAL_DEFAULT_FISHER_SAMPLES 1000

/** Number of memory tiers */
#define PR_CONTINUAL_NUM_TIERS              4

/** Numerical epsilon */
#define PR_CONTINUAL_EPSILON                1e-6f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Continual learning strategy types
 *
 * WHAT: Different approaches to preventing catastrophic forgetting
 * WHY:  Different tasks/scenarios may benefit from different strategies
 */
typedef enum {
    PR_CONTINUAL_EWC = 0,               /**< Elastic Weight Consolidation */
    PR_CONTINUAL_RESONANCE_REPLAY,      /**< Resonance-guided experience replay */
    PR_CONTINUAL_QUATERNION_PROTECTED,  /**< Quaternion consolidation gating */
    PR_CONTINUAL_ENTANGLEMENT_AWARE,    /**< Entanglement-based protection */
    PR_CONTINUAL_COMBINED               /**< All strategies combined */
} pr_continual_type_t;

/**
 * @brief Memory tier indices (matching Z-Ladder)
 */
typedef enum {
    PR_CONTINUAL_TIER_Z0 = 0,  /**< Working memory tier */
    PR_CONTINUAL_TIER_Z1 = 1,  /**< Short-term memory tier */
    PR_CONTINUAL_TIER_Z2 = 2,  /**< Long-term memory tier */
    PR_CONTINUAL_TIER_Z3 = 3   /**< Deep storage tier */
} pr_continual_tier_t;

/**
 * @brief Bridge configuration
 *
 * WHAT: Complete configuration for continual learning bridge
 * WHY:  Centralize all tunable parameters
 */
typedef struct {
    pr_continual_type_t type;           /**< Learning strategy type */
    float ewc_lambda;                   /**< EWC protection strength */
    float consolidation_weight;         /**< Quaternion.w scaling factor */
    float replay_ratio;                 /**< Fraction of batch for replay [0-1] */
    size_t replay_batch_size;           /**< Size of replay batch */
    float protection_threshold[PR_CONTINUAL_NUM_TIERS]; /**< Per-tier protection */
    uint32_t entangle_threshold;        /**< Entanglement count for max protection */
    uint32_t fisher_samples;            /**< Samples for Fisher estimation */
    bool enable_online_fisher;          /**< Update Fisher online during training */
    bool enable_task_specific;          /**< Track per-task Fisher information */
    bool enable_importance_decay;       /**< Decay importance over tasks */
    float importance_decay_rate;        /**< Decay rate for importance (per task) */
    bool enable_sparse_fisher;          /**< Use sparse Fisher storage */
    float sparse_threshold;             /**< Threshold for Fisher sparsification */
} pr_continual_config_t;

/**
 * @brief Task information record
 *
 * WHAT: Information about a completed task
 * WHY:  Track task boundaries for consolidation
 */
typedef struct {
    uint32_t task_id;                   /**< Unique task identifier */
    uint64_t start_time_ms;             /**< Task start timestamp */
    uint64_t end_time_ms;               /**< Task end timestamp */
    uint32_t num_samples;               /**< Samples processed in task */
    float final_loss;                   /**< Loss at task completion */
    float avg_resonance;                /**< Average resonance during task */
    uint32_t memories_consolidated;     /**< Memories promoted during task */
    bool fisher_computed;               /**< Whether Fisher was computed */
} pr_continual_task_info_t;

/**
 * @brief Gradient modification result
 *
 * WHAT: Result of applying protection to gradients
 * WHY:  Track how much gradients were modified
 */
typedef struct {
    size_t num_parameters;              /**< Total parameters */
    size_t num_modified;                /**< Parameters with modified gradients */
    float total_protection;             /**< Sum of protection factors applied */
    float avg_protection;               /**< Average protection factor */
    float max_protection;               /**< Maximum protection factor */
    float gradient_norm_before;         /**< Gradient L2 norm before */
    float gradient_norm_after;          /**< Gradient L2 norm after */
    float protection_loss;              /**< EWC penalty term value */
} pr_continual_grad_result_t;

/**
 * @brief Replay sample
 *
 * WHAT: A memory sample for experience replay
 * WHY:  Interleaved training to prevent forgetting
 */
typedef struct {
    uint64_t node_id;                   /**< Memory node ID */
    pr_continual_tier_t tier;           /**< Tier from which sampled */
    float importance;                   /**< Importance score (0-1) */
    float resonance;                    /**< Resonance with current context */
    nimcp_quaternion_t state;           /**< Memory quaternion state */
    const void* data;                   /**< Pointer to memory data */
    size_t data_size;                   /**< Size of memory data */
} pr_continual_replay_sample_t;

/**
 * @brief Per-node importance record
 *
 * WHAT: Importance information for a memory node
 * WHY:  Track which nodes need protection
 */
typedef struct {
    uint64_t node_id;                   /**< Memory node ID */
    float fisher_importance;            /**< Fisher-based importance */
    float consolidation_importance;     /**< Quaternion.w based importance */
    float tier_importance;              /**< Tier-based importance */
    float entangle_importance;          /**< Entanglement-based importance */
    float total_importance;             /**< Combined importance score */
    uint32_t last_task_id;              /**< Task when last updated */
} pr_continual_node_importance_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Operational metrics for the continual learning bridge
 * WHY:  Monitor bridge health and learning dynamics
 */
typedef struct {
    /* Task statistics */
    uint32_t total_tasks;               /**< Total tasks processed */
    uint32_t current_task_id;           /**< Current task identifier */

    /* Protection statistics */
    uint64_t total_protections;         /**< Total gradient protections applied */
    float avg_protection_strength;      /**< Average protection strength */
    float max_protection_applied;       /**< Maximum protection in session */

    /* Fisher statistics */
    uint64_t fisher_computations;       /**< Fisher matrix computations */
    float avg_fisher_value;             /**< Average Fisher diagonal value */
    float fisher_sparsity;              /**< Fraction of near-zero Fisher */

    /* Replay statistics */
    uint64_t replay_samples_total;      /**< Total replay samples used */
    uint64_t replay_per_tier[PR_CONTINUAL_NUM_TIERS]; /**< Samples per tier */
    float avg_replay_importance;        /**< Average importance of replays */

    /* Consolidation statistics */
    uint64_t memories_consolidated;     /**< Memories promoted due to importance */
    uint64_t memories_pruned;           /**< Low-importance memories removed */

    /* EWC loss tracking */
    float ewc_loss_current;             /**< Current EWC penalty term */
    float ewc_loss_avg;                 /**< Running average EWC loss */
    float ewc_loss_max;                 /**< Maximum EWC loss seen */

    /* Timing */
    float avg_protection_time_us;       /**< Average protection computation time */
    float avg_fisher_time_ms;           /**< Average Fisher computation time */
} pr_continual_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct pr_continual_bridge_struct* pr_continual_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides balanced starting point for most scenarios
 *
 * @return Default configuration with:
 *         - type: PR_CONTINUAL_COMBINED
 *         - ewc_lambda: 1000.0
 *         - consolidation_weight: 2.0
 *         - replay_ratio: 0.5
 *         - replay_batch_size: 32
 *         - Tier protection: [0.1, 0.4, 0.7, 1.0]
 *
 * Performance: ~10ns
 *
 * Example:
 *   pr_continual_config_t config = pr_continual_config_default();
 *   config.ewc_lambda = 5000.0f;  // Stronger protection
 *   pr_continual_bridge_t bridge = pr_continual_bridge_create(&config);
 */
NIMCP_EXPORT pr_continual_config_t pr_continual_config_default(void);

/**
 * @brief Get EWC-only configuration
 *
 * WHAT: Configuration focusing on pure EWC
 * WHY:  For comparison with standard EWC implementations
 *
 * @return Configuration with only EWC enabled
 */
NIMCP_EXPORT pr_continual_config_t pr_continual_config_ewc_only(void);

/**
 * @brief Get replay-focused configuration
 *
 * WHAT: Configuration emphasizing experience replay
 * WHY:  When replay is more appropriate than regularization
 *
 * @return Configuration with strong replay, weaker EWC
 */
NIMCP_EXPORT pr_continual_config_t pr_continual_config_replay_focused(void);

/**
 * @brief Get maximum protection configuration
 *
 * WHAT: Configuration for strongest forgetting prevention
 * WHY:  When preserving old knowledge is critical
 *
 * @return Configuration with all protections maximized
 */
NIMCP_EXPORT pr_continual_config_t pr_continual_config_max_protection(void);

/**
 * @brief Validate bridge configuration
 *
 * WHAT: Check configuration for validity
 * WHY:  Prevent invalid parameters causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Validation rules:
 * - ewc_lambda must be non-negative
 * - consolidation_weight must be non-negative
 * - replay_ratio must be in [0, 1]
 * - replay_batch_size must be > 0
 * - protection thresholds must be in [0, 1]
 *
 * Performance: ~50ns
 */
NIMCP_EXPORT bool pr_continual_config_validate(const pr_continual_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create continual learning bridge
 *
 * WHAT: Initialize bridge for continual learning with memory protection
 * WHY:  Entry point for EWC + quaternion consolidation integration
 * HOW:  Allocate state, initialize Fisher storage, connect to memory system
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle, or NULL on failure
 *
 * Performance: O(1) base allocation
 * Memory: ~4KB base + O(num_parameters) for Fisher
 *
 * Thread safety: The returned bridge is thread-safe for concurrent use
 *
 * Example:
 *   pr_continual_config_t config = pr_continual_config_default();
 *   config.type = PR_CONTINUAL_COMBINED;
 *   pr_continual_bridge_t bridge = pr_continual_bridge_create(&config);
 */
NIMCP_EXPORT pr_continual_bridge_t pr_continual_bridge_create(
    const pr_continual_config_t* config);

/**
 * @brief Destroy continual learning bridge
 *
 * WHAT: Free all bridge resources
 * WHY:  Proper resource cleanup
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * Performance: O(n) where n = parameters
 */
NIMCP_EXPORT void pr_continual_bridge_destroy(pr_continual_bridge_t bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset Fisher information and task history
 * WHY:  Start fresh learning session
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 *
 * Performance: O(n) where n = parameters
 */
NIMCP_EXPORT int pr_continual_bridge_reset(pr_continual_bridge_t bridge);

//=============================================================================
// Fisher Information Functions
//=============================================================================

/**
 * @brief Compute Fisher information diagonal
 *
 * WHAT: Estimate Fisher information for all parameters
 * WHY:  Fisher information measures parameter importance for current task
 * HOW:  Compute E[grad * grad^T] diagonal over data samples
 *
 * @param bridge Continual learning bridge
 * @param gradients Array of gradient arrays (one per sample)
 * @param num_samples Number of gradient samples
 * @param num_params Number of parameters per sample
 * @return 0 on success, -1 on error
 *
 * ALGORITHM:
 *   F_i = (1/N) * sum_n (grad[n][i])^2
 *   This is the empirical Fisher (diagonal approximation)
 *
 * Performance: O(num_samples * num_params)
 *
 * Example:
 *   // Collect gradients during training
 *   float* gradients[1000];
 *   for (int s = 0; s < 1000; s++) {
 *       gradients[s] = compute_gradients(model, data[s]);
 *   }
 *   pr_continual_compute_fisher(bridge, gradients, 1000, num_params);
 */
NIMCP_EXPORT int pr_continual_compute_fisher(
    pr_continual_bridge_t bridge,
    const float** gradients,
    size_t num_samples,
    size_t num_params);

/**
 * @brief Compute Fisher with quaternion weighting
 *
 * WHAT: Estimate Fisher information weighted by consolidation states
 * WHY:  Integrate memory consolidation into importance estimation
 * HOW:  Scale Fisher by (1 + alpha * quat.w^2) per memory
 *
 * @param bridge Continual learning bridge
 * @param gradients Array of gradient arrays
 * @param quaternions Array of quaternion states per sample
 * @param num_samples Number of samples
 * @param num_params Number of parameters
 * @return 0 on success, -1 on error
 *
 * ALGORITHM:
 *   F_i = (1/N) * sum_n ((grad[n][i])^2 * (1 + alpha * quat[n].w^2))
 *   Consolidated memories contribute more to Fisher
 *
 * Performance: O(num_samples * num_params)
 */
NIMCP_EXPORT int pr_continual_compute_fisher_weighted(
    pr_continual_bridge_t bridge,
    const float** gradients,
    const nimcp_quaternion_t* quaternions,
    size_t num_samples,
    size_t num_params);

/**
 * @brief Get Fisher information for a parameter
 *
 * @param bridge Continual learning bridge
 * @param param_idx Parameter index
 * @return Fisher value, or -1.0f if invalid
 *
 * Performance: O(1)
 */
NIMCP_EXPORT float pr_continual_get_fisher(
    pr_continual_bridge_t bridge,
    size_t param_idx);

/**
 * @brief Set Fisher information for a parameter
 *
 * @param bridge Continual learning bridge
 * @param param_idx Parameter index
 * @param value Fisher value
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_continual_set_fisher(
    pr_continual_bridge_t bridge,
    size_t param_idx,
    float value);

/**
 * @brief Accumulate Fisher information from new task
 *
 * WHAT: Add new task's Fisher to existing (online update)
 * WHY:  Build cumulative importance across multiple tasks
 *
 * @param bridge Continual learning bridge
 * @param new_fisher New Fisher diagonal values
 * @param num_params Number of parameters
 * @param decay Apply importance decay to old Fisher first
 * @return 0 on success, -1 on error
 *
 * ALGORITHM:
 *   if (decay): F_old *= (1 - decay_rate)
 *   F_total = F_old + F_new
 *
 * Performance: O(num_params)
 */
NIMCP_EXPORT int pr_continual_accumulate_fisher(
    pr_continual_bridge_t bridge,
    const float* new_fisher,
    size_t num_params,
    bool decay);

//=============================================================================
// EWC Loss Functions
//=============================================================================

/**
 * @brief Compute EWC regularization loss
 *
 * WHAT: Calculate the EWC penalty term for parameter deviation
 * WHY:  Add to task loss to penalize changing important parameters
 * HOW:  L_ewc = (lambda/2) * sum(F_i * (theta_i - theta_i*)^2)
 *
 * @param bridge Continual learning bridge
 * @param current_params Current parameter values
 * @param num_params Number of parameters
 * @return EWC loss value, or -1.0f on error
 *
 * ALGORITHM:
 *   L = (lambda/2) * sum_i (F[i] * (params[i] - old_params[i])^2)
 *
 * Performance: O(num_params)
 *
 * Example:
 *   float ewc_loss = pr_continual_ewc_loss(bridge, model->params, num_params);
 *   float total_loss = task_loss + ewc_loss;
 *   // Backward pass with total_loss
 */
NIMCP_EXPORT float pr_continual_ewc_loss(
    pr_continual_bridge_t bridge,
    const float* current_params,
    size_t num_params);

/**
 * @brief Compute EWC loss with quaternion scaling
 *
 * WHAT: EWC loss with additional consolidation-based scaling
 * WHY:  Weight protection by memory consolidation state
 *
 * @param bridge Continual learning bridge
 * @param current_params Current parameter values
 * @param quaternions Per-parameter quaternion states (or NULL)
 * @param num_params Number of parameters
 * @return Scaled EWC loss value
 *
 * Performance: O(num_params)
 */
NIMCP_EXPORT float pr_continual_ewc_loss_weighted(
    pr_continual_bridge_t bridge,
    const float* current_params,
    const nimcp_quaternion_t* quaternions,
    size_t num_params);

/**
 * @brief Compute EWC gradient contribution
 *
 * WHAT: Calculate gradient of EWC loss for backpropagation
 * WHY:  Need gradient to update parameters with EWC penalty
 * HOW:  grad_ewc[i] = lambda * F[i] * (theta[i] - theta*[i])
 *
 * @param bridge Continual learning bridge
 * @param current_params Current parameter values
 * @param num_params Number of parameters
 * @param ewc_gradients Output: EWC gradient contribution (pre-allocated)
 * @return 0 on success, -1 on error
 *
 * Performance: O(num_params)
 *
 * Example:
 *   float* ewc_grad = nimcp_malloc(num_params * sizeof(float));
 *   pr_continual_ewc_gradient(bridge, model->params, num_params, ewc_grad);
 *   // Add to task gradients
 *   for (int i = 0; i < num_params; i++) {
 *       task_grad[i] += ewc_grad[i];
 *   }
 */
NIMCP_EXPORT int pr_continual_ewc_gradient(
    pr_continual_bridge_t bridge,
    const float* current_params,
    size_t num_params,
    float* ewc_gradients);

//=============================================================================
// Importance Functions
//=============================================================================

/**
 * @brief Get importance from quaternion consolidation
 *
 * WHAT: Extract protection importance from quaternion state
 * WHY:  Consolidated memories (high w) should be more protected
 * HOW:  importance = (1 + alpha * w^2) where alpha = consolidation_weight
 *
 * @param bridge Continual learning bridge
 * @param node Memory node (or just quaternion)
 * @return Importance factor >= 1.0
 *
 * Performance: ~5ns
 *
 * Example:
 *   float imp = pr_continual_consolidation_importance(bridge, node);
 *   // imp = 1.0 for w=0 (fragile)
 *   // imp = 3.0 for w=1.0 with alpha=2.0 (fully consolidated)
 */
NIMCP_EXPORT float pr_continual_consolidation_importance(
    pr_continual_bridge_t bridge,
    const pr_memory_node_t* node);

/**
 * @brief Get importance from quaternion directly
 *
 * @param bridge Continual learning bridge
 * @param quat Quaternion state
 * @return Importance factor >= 1.0
 */
NIMCP_EXPORT float pr_continual_quat_importance(
    pr_continual_bridge_t bridge,
    nimcp_quaternion_t quat);

/**
 * @brief Get tier-based protection mask
 *
 * WHAT: Get protection level for a memory tier
 * WHY:  Different tiers have different protection needs
 *
 * @param bridge Continual learning bridge
 * @param tier Memory tier (Z0-Z3)
 * @return Protection level [0, 1]
 *
 * Performance: O(1)
 *
 * Example:
 *   float prot = pr_continual_tier_protection_mask(bridge, PR_CONTINUAL_TIER_Z3);
 *   // prot = 1.0 for Z3 (full protection)
 */
NIMCP_EXPORT float pr_continual_tier_protection_mask(
    pr_continual_bridge_t bridge,
    pr_continual_tier_t tier);

/**
 * @brief Get entanglement-based protection
 *
 * WHAT: Calculate protection based on node connectivity
 * WHY:  Highly connected nodes are structurally important
 * HOW:  protection = min(1.0, entangle_count / threshold)
 *
 * @param bridge Continual learning bridge
 * @param node Memory node
 * @return Protection level [0, 1]
 *
 * Performance: O(1)
 */
NIMCP_EXPORT float pr_continual_entanglement_protection(
    pr_continual_bridge_t bridge,
    const pr_memory_node_t* node);

/**
 * @brief Get combined importance for a node
 *
 * WHAT: Compute total importance combining all factors
 * WHY:  Unified importance metric for protection decisions
 *
 * @param bridge Continual learning bridge
 * @param node Memory node
 * @param fisher_contribution Fisher-based importance (optional, 0 to skip)
 * @return Combined importance score
 *
 * ALGORITHM:
 *   importance = max(tier_prot, entangle_prot) * quat_importance * (1 + fisher)
 *
 * Performance: O(1)
 */
NIMCP_EXPORT float pr_continual_combined_importance(
    pr_continual_bridge_t bridge,
    const pr_memory_node_t* node,
    float fisher_contribution);

//=============================================================================
// Experience Replay Functions
//=============================================================================

/**
 * @brief Sample memories for experience replay
 *
 * WHAT: Sample a batch of memories from Z-Ladder for interleaved training
 * WHY:  Replay prevents forgetting by mixing old memories with new data
 * HOW:  Sample from each tier according to importance and replay_ratio
 *
 * @param bridge Continual learning bridge
 * @param ladder Z-Ladder to sample from
 * @param tier Tier to sample from (or -1 for all tiers)
 * @param batch_size Number of samples to return
 * @param samples Output array (pre-allocated)
 * @param samples_returned Output: actual samples returned
 * @return 0 on success, -1 on error
 *
 * ALGORITHM:
 *   1. Get candidates from tier(s)
 *   2. Weight by importance (quat.w * tier_weight)
 *   3. Sample proportional to importance
 *   4. Return pointers to memory data
 *
 * Performance: O(k * log(n)) where k = batch_size, n = tier size
 *
 * Example:
 *   pr_continual_replay_sample_t samples[32];
 *   size_t got;
 *   pr_continual_replay_sample(bridge, ladder, -1, 32, samples, &got);
 *   // Use samples for training
 */
NIMCP_EXPORT int pr_continual_replay_sample(
    pr_continual_bridge_t bridge,
    z_ladder_t ladder,
    int tier,  /* -1 for all tiers */
    size_t batch_size,
    pr_continual_replay_sample_t* samples,
    size_t* samples_returned);

/**
 * @brief Sample from specific tier with resonance filtering
 *
 * WHAT: Sample memories that resonate with current context
 * WHY:  Contextually relevant replay is more effective
 *
 * @param bridge Continual learning bridge
 * @param ladder Z-Ladder to sample from
 * @param tier Tier to sample from
 * @param query Current context query
 * @param min_resonance Minimum resonance threshold
 * @param batch_size Number of samples
 * @param samples Output array
 * @param samples_returned Output: actual samples
 * @return 0 on success, -1 on error
 *
 * Performance: O(tier_size) for resonance computation
 */
NIMCP_EXPORT int pr_continual_replay_sample_resonant(
    pr_continual_bridge_t bridge,
    z_ladder_t ladder,
    pr_continual_tier_t tier,
    const resonance_query_t* query,
    float min_resonance,
    size_t batch_size,
    pr_continual_replay_sample_t* samples,
    size_t* samples_returned);

/**
 * @brief Get replay distribution across tiers
 *
 * WHAT: Calculate how many samples should come from each tier
 * WHY:  Balance recent (Z0) vs consolidated (Z3) memories
 *
 * @param bridge Continual learning bridge
 * @param total_batch Total batch size for replay
 * @param tier_counts Output: samples per tier [4]
 * @return 0 on success, -1 on error
 *
 * Default distribution:
 *   Z0: 40%, Z1: 30%, Z2: 20%, Z3: 10%
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_continual_replay_tier_distribution(
    pr_continual_bridge_t bridge,
    size_t total_batch,
    size_t tier_counts[PR_CONTINUAL_NUM_TIERS]);

//=============================================================================
// Gradient Protection Functions
//=============================================================================

/**
 * @brief Apply protection to gradients
 *
 * WHAT: Modify gradients to protect important parameters
 * WHY:  Prevent updates from changing critical parameters
 * HOW:  Scale gradients inversely with importance
 *
 * @param bridge Continual learning bridge
 * @param gradients Gradients to modify (in-place)
 * @param num_params Number of parameters
 * @param result Output: modification statistics (optional)
 * @return 0 on success, -1 on error
 *
 * ALGORITHM:
 *   For each param i:
 *     protection = fisher[i] * (1 + alpha * consolidation)
 *     grad[i] *= 1.0 / (1.0 + protection)
 *
 * Performance: O(num_params)
 *
 * Example:
 *   // After computing task gradients
 *   pr_continual_apply_protection(bridge, gradients, num_params, NULL);
 *   // Apply modified gradients to optimizer
 */
NIMCP_EXPORT int pr_continual_apply_protection(
    pr_continual_bridge_t bridge,
    float* gradients,
    size_t num_params,
    pr_continual_grad_result_t* result);

/**
 * @brief Apply protection with node-specific importance
 *
 * WHAT: Protect gradients based on per-node importance
 * WHY:  Different memory nodes may need different protection
 *
 * @param bridge Continual learning bridge
 * @param gradients Gradients to modify
 * @param nodes Array of memory nodes (param index -> node)
 * @param num_params Number of parameters
 * @param result Output: modification statistics
 * @return 0 on success, -1 on error
 *
 * Performance: O(num_params)
 */
NIMCP_EXPORT int pr_continual_apply_protection_nodes(
    pr_continual_bridge_t bridge,
    float* gradients,
    const pr_memory_node_t** nodes,
    size_t num_params,
    pr_continual_grad_result_t* result);

/**
 * @brief Preview protection without applying
 *
 * WHAT: Calculate what protection would be applied
 * WHY:  For analysis/debugging without side effects
 *
 * @param bridge Continual learning bridge
 * @param gradients Gradients (not modified)
 * @param num_params Number of parameters
 * @param protection_factors Output: protection per param (pre-allocated)
 * @return 0 on success, -1 on error
 *
 * Performance: O(num_params)
 */
NIMCP_EXPORT int pr_continual_preview_protection(
    pr_continual_bridge_t bridge,
    const float* gradients,
    size_t num_params,
    float* protection_factors);

//=============================================================================
// Task Boundary Functions
//=============================================================================

/**
 * @brief Handle task boundary (task completion)
 *
 * WHAT: Process end of task for consolidation
 * WHY:  Consolidate Fisher, store parameters, promote memories
 * HOW:  Compute Fisher, update reference params, consolidate important memories
 *
 * @param bridge Continual learning bridge
 * @param task_id Completed task identifier
 * @return 0 on success, -1 on error
 *
 * ALGORITHM:
 *   1. Finalize Fisher computation for task
 *   2. Store current parameters as reference (theta*)
 *   3. Identify high-importance memories
 *   4. Promote to higher tiers in Z-Ladder
 *   5. Update task history
 *
 * Performance: O(num_params) + O(num_memories)
 *
 * Example:
 *   // After task training completes
 *   pr_continual_task_boundary(bridge, current_task_id);
 *   // Ready for new task
 */
NIMCP_EXPORT int pr_continual_task_boundary(
    pr_continual_bridge_t bridge,
    uint32_t task_id);

/**
 * @brief Store current parameters as reference
 *
 * WHAT: Save parameter snapshot for EWC comparison
 * WHY:  EWC penalizes deviation from these values
 *
 * @param bridge Continual learning bridge
 * @param params Current parameter values
 * @param num_params Number of parameters
 * @return 0 on success, -1 on error
 *
 * Performance: O(num_params)
 */
NIMCP_EXPORT int pr_continual_store_params(
    pr_continual_bridge_t bridge,
    const float* params,
    size_t num_params);

/**
 * @brief Get stored reference parameters
 *
 * @param bridge Continual learning bridge
 * @param param_idx Parameter index
 * @return Stored parameter value, or NaN if not stored
 *
 * Performance: O(1)
 */
NIMCP_EXPORT float pr_continual_get_stored_param(
    pr_continual_bridge_t bridge,
    size_t param_idx);

/**
 * @brief Consolidate after task completion
 *
 * WHAT: Full consolidation pass after task
 * WHY:  Promote important memories, prune unimportant ones
 *
 * @param bridge Continual learning bridge
 * @param ladder Z-Ladder for memory management
 * @param graph Entanglement graph
 * @param task_id Completed task ID
 * @return 0 on success, -1 on error
 *
 * ALGORITHM:
 *   1. Identify high-importance nodes (quat.w > threshold)
 *   2. Promote eligible nodes to higher tiers
 *   3. Prune low-importance, low-entanglement edges
 *   4. Update Fisher importance for all nodes
 *
 * Performance: O(num_nodes + num_edges)
 */
NIMCP_EXPORT int pr_continual_consolidate_task(
    pr_continual_bridge_t bridge,
    z_ladder_t ladder,
    entangle_graph_t graph,
    uint32_t task_id);

//=============================================================================
// Statistics and Information
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Continual learning bridge
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_continual_get_stats(
    pr_continual_bridge_t bridge,
    pr_continual_stats_t* stats);

/**
 * @brief Get task history
 *
 * @param bridge Continual learning bridge
 * @param task_id Task to query
 * @param info Output task information
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_continual_get_task_info(
    pr_continual_bridge_t bridge,
    uint32_t task_id,
    pr_continual_task_info_t* info);

/**
 * @brief Get current task ID
 *
 * @param bridge Continual learning bridge
 * @return Current task ID
 */
NIMCP_EXPORT uint32_t pr_continual_get_current_task(pr_continual_bridge_t bridge);

/**
 * @brief Get number of parameters tracked
 *
 * @param bridge Continual learning bridge
 * @return Number of parameters
 */
NIMCP_EXPORT size_t pr_continual_get_num_params(pr_continual_bridge_t bridge);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Continual learning bridge
 */
NIMCP_EXPORT void pr_continual_reset_stats(pr_continual_bridge_t bridge);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get strategy type name as string
 *
 * @param type Strategy type
 * @return Human-readable string
 */
NIMCP_EXPORT const char* pr_continual_type_name(pr_continual_type_t type);

/**
 * @brief Get tier name as string
 *
 * @param tier Memory tier
 * @return Human-readable string
 */
NIMCP_EXPORT const char* pr_continual_tier_name(pr_continual_tier_t tier);

/**
 * @brief Print bridge statistics to stdout
 *
 * @param bridge Bridge to print stats for
 */
NIMCP_EXPORT void pr_continual_print_stats(pr_continual_bridge_t bridge);

/**
 * @brief Print task info to stdout
 *
 * @param info Task info to print
 */
NIMCP_EXPORT void pr_continual_print_task_info(const pr_continual_task_info_t* info);

/**
 * @brief Validate bridge internal consistency
 *
 * WHAT: Check internal data structure consistency
 * WHY:  Debug/test tool for corruption detection
 *
 * @param bridge Bridge to validate
 * @return true if consistent, false if corruption detected
 *
 * Performance: O(num_params)
 */
NIMCP_EXPORT bool pr_continual_validate(pr_continual_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_CONTINUAL_BRIDGE_H */
