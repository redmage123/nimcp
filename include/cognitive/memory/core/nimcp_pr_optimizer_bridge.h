//=============================================================================
// nimcp_pr_optimizer_bridge.h - Prime Resonant Memory Optimizer Bridge
//=============================================================================
/**
 * @file nimcp_pr_optimizer_bridge.h
 * @brief Bridge between Prime Resonant memory system and neural network optimizers
 *
 * WHAT: Integration layer connecting PR memory nodes with training optimizers
 * WHY:  Enable resonance-aware learning with quaternion-specific optimization
 * HOW:  Adapts standard optimizers (Adam, SGD) with memory-specific extensions
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Biologically-Inspired Learning Rate Modulation:
 *   +-----------------------------------------------------------------------+
 *   |  Memory consolidation research shows that learning is NOT uniform:   |
 *   |                                                                       |
 *   |  1. RESONANCE-BASED LEARNING:                                        |
 *   |     - High resonance memories learn faster (attention-gated)          |
 *   |     - Biological: hippocampal theta rhythm gates plasticity           |
 *   |     - LR_effective = base_lr * (1 + resonance_scale * resonance)     |
 *   |                                                                       |
 *   |  2. CONSOLIDATION-GATED UPDATES:                                      |
 *   |     - Strongly consolidated memories resist modification              |
 *   |     - Biological: synaptic tagging & protein synthesis               |
 *   |     - update_gate = max(0, consolidation - threshold)                |
 *   |                                                                       |
 *   |  3. TIER-ADAPTIVE LEARNING:                                           |
 *   |     - Z0 (working): Fast, plastic - high LR                           |
 *   |     - Z1 (short-term): Moderate plasticity                           |
 *   |     - Z2 (long-term): Slow, stable - low LR                          |
 *   |     - Z3 (permanent): Near-zero modification                         |
 *   |                                                                       |
 *   |  4. ENTANGLEMENT GRADIENT CLIPPING:                                   |
 *   |     - Highly entangled memories have smaller gradient steps           |
 *   |     - Biological: hub neurons in semantic networks are stable        |
 *   |     - clip_norm = base_norm / (1 + entanglement_count * scale)       |
 *   +-----------------------------------------------------------------------+
 *
 *   Quaternion Manifold Optimization:
 *   +-----------------------------------------------------------------------+
 *   |  Memory states live on the quaternion manifold (4D hypersphere):      |
 *   |                                                                       |
 *   |  STANDARD GRADIENT DESCENT FAILS:                                     |
 *   |  - Euclidean gradients don't preserve quaternion structure            |
 *   |  - Results drift off the manifold (invalid states)                    |
 *   |                                                                       |
 *   |  QUATERNION-AWARE OPTIMIZATION:                                       |
 *   |  - Project gradients onto tangent space                               |
 *   |  - Update via exponential map: q' = q * exp(eta * grad_tangent)      |
 *   |  - Momentum in tangent space via parallel transport                  |
 *   |                                                                       |
 *   |  EXPONENTIAL MAP UPDATE:                                              |
 *   |  +-------------------------------------------------------------------+|
 *   |  |     grad_proj = grad - (grad . q) * q   [Project to tangent]     ||
 *   |  |     q_new = q * quat_exp(lr * grad_proj) [Update on manifold]    ||
 *   |  +-------------------------------------------------------------------+|
 *   +-----------------------------------------------------------------------+
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |                    PR OPTIMIZER BRIDGE                                  |
 * +=========================================================================+
 * |                                                                          |
 * |   +-------------------------------------------------------------------+  |
 * |   |                    OPTIMIZER BACKENDS                              |  |
 * |   |                                                                    |  |
 * |   |   +----------------+  +----------------+  +------------------+     |  |
 * |   |   | Resonance Adam |  | Quat Momentum  |  | Consolidation    |     |  |
 * |   |   | (base Adam +   |  | (tangent-space |  | Gated SGD        |     |  |
 * |   |   |  resonance LR) |  |  momentum)     |  | (gate by consol) |     |  |
 * |   |   +----------------+  +----------------+  +------------------+     |  |
 * |   |                                                                    |  |
 * |   |   +----------------+  +----------------+                          |  |
 * |   |   | Tier Adaptive  |  | Entanglement   |                          |  |
 * |   |   | (Z0-Z3 scaling)|  | Clip (hub      |                          |  |
 * |   |   |                |  |  protection)   |                          |  |
 * |   |   +----------------+  +----------------+                          |  |
 * |   +-------------------------------------------------------------------+  |
 * |                                                                          |
 * |   +-------------------------------------------------------------------+  |
 * |   |                    INTEGRATION POINTS                              |  |
 * |   |                                                                    |  |
 * |   |   nimcp_optimizers.h -----> Standard optimizer backend             |  |
 * |   |   nimcp_quaternion.h -----> Quaternion state manipulation          |  |
 * |   |   nimcp_resonance.h -----> Resonance scoring for LR scaling        |  |
 * |   |   nimcp_pr_memory_node.h -> Memory nodes with tier/consolidation   |  |
 * |   +-------------------------------------------------------------------+  |
 * |                                                                          |
 * +=========================================================================+
 * ```
 *
 * PERFORMANCE:
 * - Resonance Adam step: ~150ns per parameter
 * - Quaternion momentum step: ~80ns per quaternion
 * - Tier-adaptive step: ~200ns per parameter (includes node lookup)
 * - Entanglement clip: ~50ns per gradient + O(E) edge lookup
 *
 * MEMORY:
 * - pr_optimizer_bridge_t: ~512 bytes (config + state + stats)
 * - Per-parameter Adam state: 8 bytes (m + v)
 * - Per-quaternion momentum: 16 bytes (velocity quaternion)
 *
 * THREAD SAFETY:
 * - Bridge creation/destruction: NOT thread-safe
 * - Optimization steps: Thread-safe with mutex
 * - Statistics queries: Thread-safe (atomic reads)
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PR_OPTIMIZER_BRIDGE_H
#define NIMCP_PR_OPTIMIZER_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

// Dependencies
#include "middleware/training/nimcp_optimizers.h"
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_resonance.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default learning rate */
#define PR_OPT_DEFAULT_LR               0.001f

/** Default resonance scale factor */
#define PR_OPT_DEFAULT_RESONANCE_SCALE  0.5f

/** Default consolidation gate threshold */
#define PR_OPT_DEFAULT_CONSOL_GATE      0.8f

/** Default Adam beta1 */
#define PR_OPT_DEFAULT_BETA1            0.9f

/** Default Adam beta2 */
#define PR_OPT_DEFAULT_BETA2            0.999f

/** Default epsilon for numerical stability */
#define PR_OPT_DEFAULT_EPSILON          1e-8f

/** Default momentum coefficient for quaternion SGD */
#define PR_OPT_DEFAULT_QUAT_MOMENTUM    0.9f

/** Tier learning rate scales (Z0 = fast, Z3 = near-zero) */
#define PR_OPT_TIER_LR_Z0               1.0f
#define PR_OPT_TIER_LR_Z1               0.5f
#define PR_OPT_TIER_LR_Z2               0.1f
#define PR_OPT_TIER_LR_Z3               0.01f

/** Maximum gradient norm for entanglement clipping */
#define PR_OPT_MAX_GRAD_NORM            10.0f

/** Entanglement scaling factor for gradient clipping */
#define PR_OPT_ENTANGLEMENT_SCALE       0.1f

/** Number of memory tiers */
#define PR_OPT_NUM_TIERS                4

/** Maximum parameter count for batch operations */
#define PR_OPT_MAX_BATCH_SIZE           65536

/** Module ID for bio-async registration */
#define PR_OPT_MODULE_ID                0x5052  // 'P' 'R' for Prime Resonant

//=============================================================================
// Type Definitions - Enumerations
//=============================================================================

/**
 * @brief Optimizer type enumeration
 *
 * WHAT: Supported optimizer backends for PR memory
 * WHY:  Different optimization strategies for different use cases
 */
typedef enum {
    PR_OPT_RESONANCE_ADAM = 0,     /**< Adam with resonance-scaled LR */
    PR_OPT_QUAT_SGD,               /**< Quaternion-aware SGD */
    PR_OPT_QUAT_MOMENTUM,          /**< Quaternion momentum on manifold */
    PR_OPT_CONSOLIDATION_GATED,    /**< SGD gated by consolidation strength */
    PR_OPT_TIER_ADAPTIVE,          /**< Tier-specific learning rates */
    PR_OPT_TYPE_COUNT              /**< Number of optimizer types */
} pr_optimizer_type_t;

/**
 * @brief Error codes for optimizer operations
 */
typedef enum {
    PR_OPT_SUCCESS = 0,                  /**< Operation succeeded */
    PR_OPT_ERROR_NULL_POINTER = -1,      /**< NULL pointer argument */
    PR_OPT_ERROR_INVALID_CONFIG = -2,    /**< Invalid configuration */
    PR_OPT_ERROR_INVALID_TYPE = -3,      /**< Invalid optimizer type */
    PR_OPT_ERROR_NO_MEMORY = -4,         /**< Memory allocation failed */
    PR_OPT_ERROR_NOT_INITIALIZED = -5,   /**< Bridge not initialized */
    PR_OPT_ERROR_SIZE_MISMATCH = -6,     /**< Array size mismatch */
    PR_OPT_ERROR_INVALID_QUATERNION = -7,/**< Invalid quaternion state */
    PR_OPT_ERROR_MUTEX_FAILED = -8,      /**< Mutex operation failed */
    PR_OPT_ERROR_BATCH_TOO_LARGE = -9,   /**< Batch size exceeds limit */
    PR_OPT_ERROR_INVALID_TIER = -10      /**< Invalid memory tier */
} pr_optimizer_error_t;

//=============================================================================
// Type Definitions - Configuration
//=============================================================================

/**
 * @brief Optimizer bridge configuration
 *
 * WHAT: Parameters controlling optimization behavior
 * WHY:  Allow tuning of learning dynamics for different tasks
 * HOW:  Combines standard optimizer params with PR-specific extensions
 *
 * Memory layout: ~80 bytes
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Basic Parameters
    //-------------------------------------------------------------------------
    float base_lr;                      /**< Base learning rate */

    //-------------------------------------------------------------------------
    // Resonance Modulation
    //-------------------------------------------------------------------------
    float resonance_scale;              /**< How much resonance affects LR (0-1) */

    //-------------------------------------------------------------------------
    // Consolidation Gating
    //-------------------------------------------------------------------------
    float consolidation_gate;           /**< Min consolidation for update (0-1) */

    //-------------------------------------------------------------------------
    // Tier-Specific Learning Rates
    //-------------------------------------------------------------------------
    float tier_lr_scale[PR_OPT_NUM_TIERS]; /**< Z0-Z3 LR multipliers */

    //-------------------------------------------------------------------------
    // Adam Parameters
    //-------------------------------------------------------------------------
    float beta1;                        /**< First moment decay rate */
    float beta2;                        /**< Second moment decay rate */
    float epsilon;                      /**< Numerical stability constant */

    //-------------------------------------------------------------------------
    // Quaternion Momentum
    //-------------------------------------------------------------------------
    float quat_momentum;                /**< Momentum coefficient for quat SGD */

    //-------------------------------------------------------------------------
    // Gradient Clipping
    //-------------------------------------------------------------------------
    float max_grad_norm;                /**< Maximum gradient norm */
    float entanglement_scale;           /**< Entanglement effect on clipping */

    //-------------------------------------------------------------------------
    // Feature Flags
    //-------------------------------------------------------------------------
    bool enable_resonance_scaling;      /**< Enable resonance-based LR scaling */
    bool enable_consolidation_gating;   /**< Enable consolidation gating */
    bool enable_tier_adaptation;        /**< Enable tier-specific LR */
    bool enable_entanglement_clipping;  /**< Enable entanglement-based clipping */
    bool enable_weight_decay;           /**< Enable L2 regularization */
    float weight_decay;                 /**< Weight decay coefficient */

} pr_optimizer_config_t;

//=============================================================================
// Type Definitions - State Structures
//=============================================================================

/**
 * @brief Per-parameter Adam state
 *
 * Stores first and second moment estimates for Adam optimizer.
 */
typedef struct {
    float* m;                           /**< First moment estimates */
    float* v;                           /**< Second moment estimates */
    float* v_max;                       /**< Max of past v (for AMSGrad) */
    size_t count;                       /**< Number of parameters */
    uint64_t step;                      /**< Current step count */
} pr_adam_state_t;

/**
 * @brief Quaternion velocity state for momentum
 *
 * Stores momentum in tangent space of quaternion manifold.
 */
typedef struct {
    nimcp_quaternion_t* velocities;     /**< Velocity quaternions */
    size_t count;                       /**< Number of quaternions */
} pr_quat_momentum_state_t;

/**
 * @brief Optimizer state union
 *
 * Contains state for all optimizer types (only one active at a time).
 */
typedef struct {
    pr_optimizer_type_t type;           /**< Active optimizer type */

    union {
        pr_adam_state_t adam;           /**< Adam optimizer state */
        pr_quat_momentum_state_t quat;  /**< Quaternion momentum state */
    } state;

    bool initialized;                   /**< Whether state is initialized */
} pr_optimizer_state_t;

//=============================================================================
// Type Definitions - Statistics
//=============================================================================

/**
 * @brief Optimizer bridge statistics
 *
 * WHAT: Metrics for monitoring optimization performance
 * WHY:  Enable analysis and debugging of learning dynamics
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Step Counts
    //-------------------------------------------------------------------------
    _Atomic uint64_t total_steps;       /**< Total optimization steps */
    _Atomic uint64_t resonance_adam_steps;
    _Atomic uint64_t quat_momentum_steps;
    _Atomic uint64_t consolidation_gated_steps;
    _Atomic uint64_t tier_adaptive_steps;

    //-------------------------------------------------------------------------
    // Gradient Statistics
    //-------------------------------------------------------------------------
    _Atomic uint64_t gradient_clips;    /**< Number of gradient clips */
    float total_gradient_norm;          /**< Sum of gradient norms */
    float max_gradient_norm;            /**< Maximum gradient norm seen */
    float min_gradient_norm;            /**< Minimum gradient norm seen */
    float avg_gradient_norm;            /**< Average gradient norm */

    //-------------------------------------------------------------------------
    // Learning Rate Statistics
    //-------------------------------------------------------------------------
    float total_effective_lr;           /**< Sum of effective LRs */
    float max_effective_lr;             /**< Maximum effective LR */
    float min_effective_lr;             /**< Minimum effective LR */
    float avg_effective_lr;             /**< Average effective LR */

    //-------------------------------------------------------------------------
    // Resonance Statistics
    //-------------------------------------------------------------------------
    float total_resonance;              /**< Sum of resonance scores */
    float avg_resonance;                /**< Average resonance score */

    //-------------------------------------------------------------------------
    // Gating Statistics
    //-------------------------------------------------------------------------
    _Atomic uint64_t gated_updates;     /**< Updates blocked by consolidation */
    _Atomic uint64_t allowed_updates;   /**< Updates allowed through gate */

    //-------------------------------------------------------------------------
    // Timing
    //-------------------------------------------------------------------------
    uint64_t total_time_ns;             /**< Total computation time */
    uint64_t last_step_time_ns;         /**< Last step duration */

} pr_optimizer_stats_t;

//=============================================================================
// Type Definitions - Bridge Structure
//=============================================================================

/**
 * @brief Optimizer bridge handle (opaque pointer)
 */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

typedef struct pr_optimizer_bridge_struct* pr_optimizer_bridge_t;

/**
 * @brief Internal optimizer bridge structure
 *
 * WHAT: Main bridge connecting PR memory to optimizers
 * WHY:  Centralizes all optimization state and configuration
 * HOW:  Maintains config, optimizer state, statistics, and mutex
 */
struct pr_optimizer_bridge_struct {
    //-------------------------------------------------------------------------
    // Base Bridge Infrastructure
    //-------------------------------------------------------------------------
    bridge_base_t base;                 /**< Base bridge (MUST be first) */

    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    pr_optimizer_config_t config;       /**< Active configuration */
    pr_optimizer_type_t active_type;    /**< Currently active optimizer type */

    //-------------------------------------------------------------------------
    // Connected Systems
    //-------------------------------------------------------------------------
    nimcp_optimizer_context_t* base_optimizer; /**< Base NIMCP optimizer */

    //-------------------------------------------------------------------------
    // Optimizer State
    //-------------------------------------------------------------------------
    pr_optimizer_state_t state;         /**< Optimizer-specific state */

    //-------------------------------------------------------------------------
    // Statistics
    //-------------------------------------------------------------------------
    pr_optimizer_stats_t stats;         /**< Performance statistics */

    //-------------------------------------------------------------------------
    // Runtime State
    //-------------------------------------------------------------------------
    float current_lr;                   /**< Current effective learning rate */
    bool initialized;                   /**< Initialization flag */

    //-------------------------------------------------------------------------
    // Instance Health Agent (B25 Upgrade)
    //-------------------------------------------------------------------------
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent */
};

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default optimizer configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 * HOW:  Sets balanced parameters for general optimization
 *
 * @return Default configuration with:
 *         - base_lr: 0.001
 *         - resonance_scale: 0.5
 *         - consolidation_gate: 0.8
 *         - beta1: 0.9, beta2: 0.999
 *         - tier_lr_scale: [1.0, 0.5, 0.1, 0.01]
 *         - All features enabled
 *
 * Performance: ~10ns
 *
 * Example:
 *   pr_optimizer_config_t config = pr_optimizer_config_default();
 *   config.base_lr = 0.01f;  // Higher learning rate
 */
NIMCP_EXPORT pr_optimizer_config_t pr_optimizer_config_default(void);

/**
 * @brief Get configuration for aggressive learning
 *
 * @return Config with high LR, low consolidation gate
 */
NIMCP_EXPORT pr_optimizer_config_t pr_optimizer_config_aggressive(void);

/**
 * @brief Get configuration for conservative learning
 *
 * @return Config with low LR, high consolidation gate
 */
NIMCP_EXPORT pr_optimizer_config_t pr_optimizer_config_conservative(void);

/**
 * @brief Validate optimizer configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Checks:
 * - base_lr > 0
 * - beta1, beta2 in [0, 1)
 * - tier_lr_scale values >= 0
 * - resonance_scale, consolidation_gate in [0, 1]
 */
NIMCP_EXPORT bool pr_optimizer_config_validate(
    const pr_optimizer_config_t* config
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create optimizer bridge
 *
 * WHAT: Allocates and initializes optimizer bridge
 * WHY:  Entry point for using PR-aware optimization
 * HOW:  Allocates memory, initializes state, sets configuration
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 *
 * Performance: ~1us
 * Memory: ~512 bytes + optimizer state
 *
 * Example:
 *   pr_optimizer_config_t cfg = pr_optimizer_config_default();
 *   pr_optimizer_bridge_t bridge = pr_optimizer_bridge_create(&cfg);
 *   if (!bridge) {
 *       // Handle error
 *   }
 */
NIMCP_EXPORT pr_optimizer_bridge_t pr_optimizer_bridge_create(
    const pr_optimizer_config_t* config
);

/**
 * @brief Destroy optimizer bridge
 *
 * WHAT: Releases all bridge resources
 * WHY:  Clean up after optimization complete
 * HOW:  Frees state arrays, destroys mutex, frees bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * Performance: ~500ns
 */
NIMCP_EXPORT void pr_optimizer_bridge_destroy(
    pr_optimizer_bridge_t bridge
);

/**
 * @brief Connect base NIMCP optimizer
 *
 * @param bridge Optimizer bridge
 * @param optimizer NIMCP optimizer context
 * @return PR_OPT_SUCCESS or error code
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_bridge_connect(
    pr_optimizer_bridge_t bridge,
    nimcp_optimizer_context_t* optimizer
);

/**
 * @brief Initialize optimizer state for parameter count
 *
 * WHAT: Allocates state arrays for specified parameter count
 * WHY:  Must be called before optimization steps
 * HOW:  Allocates m, v arrays for Adam; velocity arrays for quat
 *
 * @param bridge Optimizer bridge
 * @param type Optimizer type to initialize
 * @param num_params Number of parameters (or quaternions)
 * @return PR_OPT_SUCCESS or error code
 *
 * Performance: O(n) where n = num_params
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_bridge_init_state(
    pr_optimizer_bridge_t bridge,
    pr_optimizer_type_t type,
    size_t num_params
);

//=============================================================================
// Core Optimization Functions
//=============================================================================

/**
 * @brief Perform resonance-aware Adam step
 *
 * WHAT: Adam optimizer with learning rate scaled by resonance
 * WHY:  High-resonance memories learn faster (attention-gated plasticity)
 * HOW:  Computes effective LR = base_lr * (1 + resonance_scale * resonance)
 *
 * ALGORITHM:
 *   1. Compute effective LR from resonance score
 *   2. Update first moment: m = beta1 * m + (1 - beta1) * grad
 *   3. Update second moment: v = beta2 * v + (1 - beta2) * grad^2
 *   4. Bias correction: m_hat = m / (1 - beta1^t), v_hat = v / (1 - beta2^t)
 *   5. Update params: p = p - lr_eff * m_hat / (sqrt(v_hat) + eps)
 *
 * @param bridge Optimizer bridge
 * @param params Parameter array (modified in place)
 * @param gradients Gradient array
 * @param count Number of parameters
 * @param resonance Resonance score [0, 1]
 * @return PR_OPT_SUCCESS or error code
 *
 * Performance: ~150ns per parameter
 *
 * Example:
 *   float params[100], grads[100];
 *   float resonance = 0.8f;  // High resonance
 *   pr_optimizer_resonance_adam_step(bridge, params, grads, 100, resonance);
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_resonance_adam_step(
    pr_optimizer_bridge_t bridge,
    float* params,
    const float* gradients,
    size_t count,
    float resonance
);

/**
 * @brief Perform quaternion momentum step
 *
 * WHAT: Momentum-based update on quaternion manifold
 * WHY:  Standard momentum doesn't work on curved manifolds
 * HOW:  Updates in tangent space, uses exponential map to stay on manifold
 *
 * ALGORITHM:
 *   1. Project gradient to tangent space: grad_proj = grad - (grad.q)*q
 *   2. Update velocity: v = momentum * v + grad_proj
 *   3. Apply exponential map: q' = q * quat_exp(lr * v)
 *   4. Normalize result (safety check)
 *
 * @param bridge Optimizer bridge
 * @param quat Quaternion to update (modified in place)
 * @param grad Gradient quaternion (in ambient space)
 * @param velocity Momentum velocity (tangent space, modified)
 * @return PR_OPT_SUCCESS or error code
 *
 * Performance: ~80ns per quaternion
 *
 * Example:
 *   nimcp_quaternion_t q = quat_create(0.9f, 0.1f, 0.2f, 0.3f);
 *   nimcp_quaternion_t grad = quat_create(0.01f, 0.02f, -0.01f, 0.01f);
 *   nimcp_quaternion_t vel = quat_create(0, 0, 0, 0);
 *   pr_optimizer_quat_momentum_step(bridge, &q, &grad, &vel);
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_quat_momentum_step(
    pr_optimizer_bridge_t bridge,
    nimcp_quaternion_t* quat,
    const nimcp_quaternion_t* grad,
    nimcp_quaternion_t* velocity
);

/**
 * @brief Batch quaternion momentum update
 *
 * WHAT: Update multiple quaternions with momentum
 * WHY:  More efficient than individual calls
 *
 * @param bridge Optimizer bridge
 * @param quats Array of quaternions to update
 * @param grads Array of gradients
 * @param velocities Array of velocities (modified)
 * @param count Number of quaternions
 * @return PR_OPT_SUCCESS or error code
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_quat_momentum_step_batch(
    pr_optimizer_bridge_t bridge,
    nimcp_quaternion_t* quats,
    const nimcp_quaternion_t* grads,
    nimcp_quaternion_t* velocities,
    size_t count
);

/**
 * @brief Perform consolidation-gated optimization step
 *
 * WHAT: SGD where updates are gated by memory consolidation strength
 * WHY:  Strongly consolidated memories should resist modification
 * HOW:  Multiplies update by gate factor based on consolidation
 *
 * ALGORITHM:
 *   1. For each node, get consolidation strength c from quaternion.w
 *   2. Compute gate: gate = max(0, 1 - c / consolidation_gate)
 *   3. Update: p = p - lr * gate * grad
 *   4. If c >= consolidation_gate, update is completely blocked
 *
 * @param bridge Optimizer bridge
 * @param params Parameter array (modified in place)
 * @param gradients Gradient array
 * @param nodes Array of memory nodes (for consolidation lookup)
 * @param count Number of parameters/nodes
 * @return PR_OPT_SUCCESS or error code
 *
 * Performance: ~200ns per parameter
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_consolidation_gated_step(
    pr_optimizer_bridge_t bridge,
    float* params,
    const float* gradients,
    const pr_memory_node_t* const* nodes,
    size_t count
);

/**
 * @brief Perform tier-adaptive optimization step
 *
 * WHAT: Different learning rates for different memory tiers
 * WHY:  Working memory is plastic, long-term memory is stable
 * HOW:  Uses tier_lr_scale multipliers per node tier
 *
 * ALGORITHM:
 *   1. For each node, get tier (Z0-Z3)
 *   2. effective_lr = base_lr * tier_lr_scale[tier]
 *   3. Update: p = p - effective_lr * grad
 *
 * @param bridge Optimizer bridge
 * @param params Parameter array (modified in place)
 * @param gradients Gradient array
 * @param nodes Array of memory nodes (for tier lookup)
 * @param count Number of parameters/nodes
 * @return PR_OPT_SUCCESS or error code
 *
 * Performance: ~200ns per parameter
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_tier_adaptive_step(
    pr_optimizer_bridge_t bridge,
    float* params,
    const float* gradients,
    const pr_memory_node_t* const* nodes,
    size_t count
);

//=============================================================================
// Learning Rate Functions
//=============================================================================

/**
 * @brief Compute effective learning rate for a memory node
 *
 * WHAT: Calculates actual LR considering all modulations
 * WHY:  Expose effective LR for monitoring and debugging
 * HOW:  Combines resonance scaling, tier adaptation, and consolidation
 *
 * FORMULA:
 *   lr_eff = base_lr
 *          * (1 + resonance_scale * resonance)    [if enabled]
 *          * tier_lr_scale[tier]                  [if enabled]
 *          * (1 - consolidation / consol_gate)   [if enabled]
 *
 * @param bridge Optimizer bridge
 * @param base_lr Base learning rate
 * @param node Memory node (for tier, consolidation)
 * @param resonance Resonance score [0, 1]
 * @return Effective learning rate
 *
 * Performance: ~30ns
 */
NIMCP_EXPORT float pr_optimizer_compute_effective_lr(
    pr_optimizer_bridge_t bridge,
    float base_lr,
    const pr_memory_node_t* node,
    float resonance
);

/**
 * @brief Set base learning rate
 *
 * @param bridge Optimizer bridge
 * @param lr New learning rate
 * @return PR_OPT_SUCCESS or error code
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_set_lr(
    pr_optimizer_bridge_t bridge,
    float lr
);

/**
 * @brief Get current learning rate
 *
 * @param bridge Optimizer bridge
 * @return Current base learning rate
 */
NIMCP_EXPORT float pr_optimizer_get_lr(
    pr_optimizer_bridge_t bridge
);

//=============================================================================
// Gradient Functions
//=============================================================================

/**
 * @brief Clip gradients by entanglement
 *
 * WHAT: Scale gradient norm based on node entanglement count
 * WHY:  Hub nodes (highly entangled) should have smaller updates
 * HOW:  Clips norm to max_norm / (1 + entanglement * scale)
 *
 * ALGORITHM:
 *   1. Compute gradient norm
 *   2. For each edge, accumulate entanglement count
 *   3. Compute clip_norm = max_norm / (1 + total_entanglement * scale)
 *   4. If norm > clip_norm, scale gradients by clip_norm / norm
 *
 * @param bridge Optimizer bridge
 * @param gradients Gradient array (modified in place)
 * @param count Number of gradients
 * @param entanglement_counts Array of entanglement counts per gradient
 * @return Original gradient norm (before clipping)
 *
 * Performance: ~50ns + O(count)
 */
NIMCP_EXPORT float pr_optimizer_clip_by_entanglement(
    pr_optimizer_bridge_t bridge,
    float* gradients,
    size_t count,
    const uint32_t* entanglement_counts
);

/**
 * @brief Clip gradients by maximum norm
 *
 * @param bridge Optimizer bridge
 * @param gradients Gradient array (modified in place)
 * @param count Number of gradients
 * @param max_norm Maximum allowed norm
 * @return Original gradient norm
 */
NIMCP_EXPORT float pr_optimizer_clip_by_norm(
    pr_optimizer_bridge_t bridge,
    float* gradients,
    size_t count,
    float max_norm
);

/**
 * @brief Zero all gradients
 *
 * @param gradients Gradient array
 * @param count Number of gradients
 */
NIMCP_EXPORT void pr_optimizer_zero_grad(
    float* gradients,
    size_t count
);

/**
 * @brief Zero optimizer state (momentum, etc.)
 *
 * @param bridge Optimizer bridge
 * @return PR_OPT_SUCCESS or error code
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_zero_state(
    pr_optimizer_bridge_t bridge
);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get optimizer state
 *
 * @param bridge Optimizer bridge
 * @return Pointer to optimizer state (read-only)
 */
NIMCP_EXPORT const pr_optimizer_state_t* pr_optimizer_get_state(
    pr_optimizer_bridge_t bridge
);

/**
 * @brief Get current step count
 *
 * @param bridge Optimizer bridge
 * @return Current optimization step
 */
NIMCP_EXPORT uint64_t pr_optimizer_get_step(
    pr_optimizer_bridge_t bridge
);

/**
 * @brief Get active optimizer type
 *
 * @param bridge Optimizer bridge
 * @return Currently active optimizer type
 */
NIMCP_EXPORT pr_optimizer_type_t pr_optimizer_get_type(
    pr_optimizer_bridge_t bridge
);

/**
 * @brief Set active optimizer type
 *
 * @param bridge Optimizer bridge
 * @param type New optimizer type
 * @return PR_OPT_SUCCESS or error code
 *
 * Note: May require re-initialization of state
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_set_type(
    pr_optimizer_bridge_t bridge,
    pr_optimizer_type_t type
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get optimizer statistics
 *
 * @param bridge Optimizer bridge
 * @param stats Output statistics structure
 * @return PR_OPT_SUCCESS or error code
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_get_stats(
    pr_optimizer_bridge_t bridge,
    pr_optimizer_stats_t* stats
);

/**
 * @brief Reset optimizer statistics
 *
 * @param bridge Optimizer bridge
 * @return PR_OPT_SUCCESS or error code
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_reset_stats(
    pr_optimizer_bridge_t bridge
);

/**
 * @brief Get optimizer type name as string
 *
 * @param type Optimizer type
 * @return Human-readable optimizer name
 */
NIMCP_EXPORT const char* pr_optimizer_type_name(
    pr_optimizer_type_t type
);

/**
 * @brief Get error code description
 *
 * @param error Error code
 * @return Human-readable error description
 */
NIMCP_EXPORT const char* pr_optimizer_error_string(
    pr_optimizer_error_t error
);

//=============================================================================
// Configuration Update Functions
//=============================================================================

/**
 * @brief Update bridge configuration
 *
 * @param bridge Optimizer bridge
 * @param config New configuration
 * @return PR_OPT_SUCCESS or error code
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_update_config(
    pr_optimizer_bridge_t bridge,
    const pr_optimizer_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param bridge Optimizer bridge
 * @return Pointer to current config (read-only)
 */
NIMCP_EXPORT const pr_optimizer_config_t* pr_optimizer_get_config(
    pr_optimizer_bridge_t bridge
);

/**
 * @brief Set tier learning rate scale
 *
 * @param bridge Optimizer bridge
 * @param tier Memory tier (Z0-Z3)
 * @param scale Learning rate scale for tier
 * @return PR_OPT_SUCCESS or error code
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_set_tier_scale(
    pr_optimizer_bridge_t bridge,
    pr_memory_tier_t tier,
    float scale
);

/**
 * @brief Set resonance scaling factor
 *
 * @param bridge Optimizer bridge
 * @param scale Resonance scale factor
 * @return PR_OPT_SUCCESS or error code
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_set_resonance_scale(
    pr_optimizer_bridge_t bridge,
    float scale
);

/**
 * @brief Set consolidation gate threshold
 *
 * @param bridge Optimizer bridge
 * @param threshold Consolidation gate threshold
 * @return PR_OPT_SUCCESS or error code
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_set_consolidation_gate(
    pr_optimizer_bridge_t bridge,
    float threshold
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Optimizer bridge
 * @return PR_OPT_SUCCESS or error code
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_connect_bio_async(
    pr_optimizer_bridge_t bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Optimizer bridge
 * @return PR_OPT_SUCCESS or error code
 */
NIMCP_EXPORT pr_optimizer_error_t pr_optimizer_disconnect_bio_async(
    pr_optimizer_bridge_t bridge
);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Optimizer bridge
 * @return true if connected
 */
NIMCP_EXPORT bool pr_optimizer_is_bio_async_connected(
    pr_optimizer_bridge_t bridge
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Project gradient to quaternion tangent space
 *
 * WHAT: Projects ambient-space gradient onto tangent space at q
 * WHY:  Required for manifold optimization
 * HOW:  grad_proj = grad - (grad . q) * q
 *
 * @param q Base quaternion (point on manifold)
 * @param grad Gradient in ambient space
 * @return Projected gradient in tangent space
 *
 * Performance: ~20ns
 */
NIMCP_EXPORT nimcp_quaternion_t pr_optimizer_project_to_tangent(
    nimcp_quaternion_t q,
    nimcp_quaternion_t grad
);

/**
 * @brief Apply exponential map for quaternion update
 *
 * WHAT: Maps tangent vector to manifold via exponential map
 * WHY:  Produces valid quaternion from tangent-space update
 * HOW:  q' = q * quat_exp(v) where v is in tangent space
 *
 * @param q Base quaternion
 * @param tangent_vec Tangent space vector (scaled by LR)
 * @return Updated quaternion on manifold
 *
 * Performance: ~40ns
 */
NIMCP_EXPORT nimcp_quaternion_t pr_optimizer_exp_map(
    nimcp_quaternion_t q,
    nimcp_quaternion_t tangent_vec
);

/**
 * @brief Parallel transport of velocity between quaternions
 *
 * WHAT: Moves velocity from tangent space at q1 to tangent space at q2
 * WHY:  Required for momentum when base point changes
 * HOW:  Uses quaternion-specific parallel transport
 *
 * @param velocity Velocity at q_from
 * @param q_from Source quaternion
 * @param q_to Destination quaternion
 * @return Transported velocity at q_to
 *
 * Performance: ~50ns
 */
NIMCP_EXPORT nimcp_quaternion_t pr_optimizer_parallel_transport(
    nimcp_quaternion_t velocity,
    nimcp_quaternion_t q_from,
    nimcp_quaternion_t q_to
);

//=============================================================================
// Inline Helper Functions
//=============================================================================

/**
 * @brief Create config for working memory optimization (fast learning)
 */
static inline pr_optimizer_config_t pr_optimizer_config_working_memory(void) {
    pr_optimizer_config_t cfg = pr_optimizer_config_default();
    cfg.base_lr = 0.01f;                    // High base LR
    cfg.resonance_scale = 0.8f;             // Strong resonance effect
    cfg.consolidation_gate = 0.95f;         // Almost no gating
    cfg.tier_lr_scale[PR_MEMORY_TIER_Z0] = 2.0f;  // Double LR for Z0
    return cfg;
}

/**
 * @brief Create config for long-term memory optimization (stable)
 */
static inline pr_optimizer_config_t pr_optimizer_config_long_term(void) {
    pr_optimizer_config_t cfg = pr_optimizer_config_default();
    cfg.base_lr = 0.0001f;                  // Very low base LR
    cfg.resonance_scale = 0.1f;             // Weak resonance effect
    cfg.consolidation_gate = 0.3f;          // Strong gating
    cfg.tier_lr_scale[PR_MEMORY_TIER_Z2] = 0.05f;  // Even lower for Z2
    cfg.tier_lr_scale[PR_MEMORY_TIER_Z3] = 0.001f; // Near-zero for Z3
    return cfg;
}

/**
 * @brief Create config for quaternion-only optimization
 */
static inline pr_optimizer_config_t pr_optimizer_config_quaternion(void) {
    pr_optimizer_config_t cfg = pr_optimizer_config_default();
    cfg.base_lr = 0.01f;
    cfg.quat_momentum = 0.95f;              // High momentum
    cfg.enable_resonance_scaling = false;    // Disable extra scaling
    cfg.enable_consolidation_gating = false;
    cfg.enable_tier_adaptation = false;
    return cfg;
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PR_OPTIMIZER_BRIDGE_H
