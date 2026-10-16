//=============================================================================
// nimcp_pr_meta_bridge.h - Prime Resonant Meta-Learning Bridge
//=============================================================================
/**
 * @file nimcp_pr_meta_bridge.h
 * @brief Bidirectional integration between Prime Resonant memory and meta-learning
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bridge connecting Prime Resonant memory (entanglement, resonance, quaternion)
 *       with meta-learning algorithms (MAML, Reptile, prototypical networks)
 * WHY:  Enable memory-aware rapid adaptation where prior memories accelerate learning
 *       on new tasks, and meta-learning improves memory organization
 * HOW:  Bidirectional integration where:
 *       - Memory recall guides task adaptation (few-shot from memory)
 *       - Resonance measures task similarity for transfer
 *       - Quaternion states encode meta-parameters
 *       - Entanglement patterns transfer between related tasks
 *       - Memory tier determines adaptation rate
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Meta-Learning <-> Memory Integration Model:
 *   +-----------------------------------------------------------------------+
 *   |                                                                        |
 *   |  MEMORY-AWARE MAML:                                                   |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Standard MAML: Learn initialization theta that adapts quickly |   |
 *   |  |                                                                 |   |
 *   |  |  Memory-enhanced:                                               |   |
 *   |  |  - Query similar tasks from memory before adaptation            |   |
 *   |  |  - Use recalled solutions as initialization bias                |   |
 *   |  |  - Modulate learning rate by memory confidence                  |   |
 *   |  |                                                                 |   |
 *   |  |  Biological: Hippocampal replay during task-switching           |   |
 *   |  |             Prefrontal retrieval of prior solutions             |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  RESONANCE-GUIDED TASK SIMILARITY:                                    |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Task similarity via Prime Resonant metrics:                    |   |
 *   |  |  - Prime signature overlap (content similarity)                 |   |
 *   |  |  - Quaternion geodesic distance (state similarity)              |   |
 *   |  |  - Entanglement graph structure (relational similarity)         |   |
 *   |  |                                                                 |   |
 *   |  |  Used for:                                                      |   |
 *   |  |  - Task clustering for curriculum learning                      |   |
 *   |  |  - Transfer learning decisions                                  |   |
 *   |  |  - Multi-task knowledge sharing                                 |   |
 *   |  |                                                                 |   |
 *   |  |  Biological: Semantic similarity in cortical representations    |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  QUATERNION META-PARAMETERS:                                          |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Encode meta-learning state in quaternion:                      |   |
 *   |  |  - w: Task mastery level (consolidation of skill)               |   |
 *   |  |  - x: Task difficulty valence (easy/hard)                       |   |
 *   |  |  - y: Task novelty/salience                                     |   |
 *   |  |  - z: Task accessibility (retrieval ease)                       |   |
 *   |  |                                                                 |   |
 *   |  |  Meta-learning adapts quaternion state:                         |   |
 *   |  |  - Inner loop: Temporary quaternion shift                       |   |
 *   |  |  - Outer loop: Permanent quaternion update                      |   |
 *   |  |                                                                 |   |
 *   |  |  Biological: Metacognitive state representation                 |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  ENTANGLEMENT TRANSFER:                                               |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  Transfer graph structure between related tasks:                |   |
 *   |  |  - Copy edge patterns from similar tasks                        |   |
 *   |  |  - Scale weights by task similarity                             |   |
 *   |  |  - Merge entanglement graphs for related tasks                  |   |
 *   |  |                                                                 |   |
 *   |  |  Enables:                                                       |   |
 *   |  |  - Structure-aware transfer learning                            |   |
 *   |  |  - Analogical reasoning via graph isomorphism                   |   |
 *   |  |  - Compositional generalization                                 |   |
 *   |  |                                                                 |   |
 *   |  |  Biological: Schema transfer in prefrontal cortex               |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   +-----------------------------------------------------------------------+
 *
 *   Tier-Specific Meta-Learning:
 *   +-----------------------------------------------------------------------+
 *   |  Tier | Adaptation Rate | Memory Role        | Meta-Learning Mode    |
 *   |-------|-----------------|--------------------|-----------------------|
 *   |  Z0   | 1.0x (fastest)  | Working memory     | Rapid online learning |
 *   |  Z1   | 0.5x            | Recent experiences | Few-shot adaptation   |
 *   |  Z2   | 0.2x            | Long-term memory   | Transfer learning     |
 *   |  Z3   | 0.1x (slowest)  | Core knowledge     | Meta-initialization   |
 *   +-----------------------------------------------------------------------+
 *
 *   Memory-Guided Adaptation Flow:
 *   +-----------------------------------------------------------------------+
 *   |  1. New task arrives                                                  |
 *   |  2. Query memory for similar tasks (resonance-based)                  |
 *   |  3. Retrieve top-K similar task solutions                             |
 *   |  4. Initialize adaptation with memory-weighted average                |
 *   |  5. Run inner loop with memory-modulated learning rate               |
 *   |  6. Store adapted solution in appropriate memory tier                 |
 *   |  7. Update entanglement with new task                                 |
 *   |  8. Outer loop: Update meta-parameters based on memory               |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Task similarity (resonance): ~100ns per pair
 * - Memory recall (top-K): ~10us for K=5
 * - Inner loop step: ~1ms (depends on model)
 * - Quaternion adaptation: ~50ns
 * - Entanglement transfer: O(E) where E = edges
 * - Full bridge update: ~5ms for 1K tasks
 *
 * MEMORY:
 * - pr_meta_bridge_t: ~4KB base + task memory buffer
 * - Per-task overhead: ~256 bytes (result + quaternion + metadata)
 *
 * INTEGRATION:
 * - Core: Entanglement graph, resonance engine, quaternion state
 * - Training: Meta-learning (MAML, Reptile, prototypical)
 * - Middleware: Z-Ladder memory tiers
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PR_META_BRIDGE_H
#define NIMCP_PR_META_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Prime Resonant core dependencies */
#include "nimcp_entanglement.h"
#include "nimcp_resonance.h"
#include "nimcp_quaternion.h"

/* Meta-learning dependency */
#include "training/nimcp_meta_learning.h"

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

/** Maximum number of tasks to store in memory */
#define PR_META_MAX_TASK_MEMORY            4096

/** Maximum similar tasks to recall per query */
#define PR_META_MAX_RECALL                 32

/** Default inner loop learning rate */
#define PR_META_DEFAULT_INNER_LR           0.01f

/** Default outer loop learning rate */
#define PR_META_DEFAULT_OUTER_LR           0.001f

/** Default inner loop steps */
#define PR_META_DEFAULT_INNER_STEPS        5

/** Default resonance threshold for task similarity */
#define PR_META_DEFAULT_RESONANCE_THRESHOLD 0.5f

/** Default entanglement transfer weight */
#define PR_META_DEFAULT_TRANSFER_WEIGHT    0.5f

/** Quaternion adaptation rate */
#define PR_META_QUAT_ADAPTATION_RATE       0.1f

/** Minimum adaptation rate (for deep tiers) */
#define PR_META_MIN_ADAPTATION_RATE        0.05f

/** Maximum adaptation rate (for working memory) */
#define PR_META_MAX_ADAPTATION_RATE        1.0f

/** Default task embedding dimension */
#define PR_META_DEFAULT_EMBED_DIM          64

/** Number of memory tiers */
#define PR_META_NUM_TIERS                  4

/** Numerical epsilon */
#define PR_META_EPSILON                    1e-6f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Meta-learning bridge type
 *
 * WHAT: Type of meta-learning integration
 * WHY:  Different algorithms require different bridge behaviors
 */
typedef enum {
    PR_META_MEMORY_MAML = 0,          /**< Memory-aware MAML */
    PR_META_RESONANCE_REPTILE,        /**< Resonance-guided Reptile */
    PR_META_QUATERNION_META,          /**< Quaternion meta-parameters */
    PR_META_ENTANGLEMENT_TRANSFER,    /**< Entanglement pattern transfer */
    PR_META_HYBRID,                   /**< Combined approach */
    PR_META_TYPE_COUNT                /**< Number of meta types */
} pr_meta_type_t;

/**
 * @brief Memory tier for meta-learning (matching Z-Ladder)
 */
typedef enum {
    PR_META_TIER_Z0 = 0,  /**< Working memory tier */
    PR_META_TIER_Z1 = 1,  /**< Short-term memory tier */
    PR_META_TIER_Z2 = 2,  /**< Long-term memory tier */
    PR_META_TIER_Z3 = 3   /**< Deep storage tier */
} pr_meta_tier_t;

/**
 * @brief Task similarity metric type
 */
typedef enum {
    PR_META_SIM_RESONANCE = 0,        /**< Full resonance (Jaccard+Phase+Quat+Kuramoto) */
    PR_META_SIM_PRIME_SIGNATURE,      /**< Prime signature Jaccard only */
    PR_META_SIM_QUATERNION,           /**< Quaternion geodesic distance */
    PR_META_SIM_ENTANGLEMENT,         /**< Graph structure similarity */
    PR_META_SIM_EMBEDDING,            /**< Learned embedding distance */
    PR_META_SIM_COUNT
} pr_meta_similarity_t;

/**
 * @brief Meta-learning bridge configuration
 *
 * WHAT: Parameters controlling bridge behavior
 * WHY:  Configure adaptation rates, thresholds, and integration modes
 */
typedef struct {
    /* Core meta-learning parameters */
    uint32_t inner_steps;             /**< Inner loop gradient steps */
    float inner_lr;                   /**< Inner loop learning rate */
    float outer_lr;                   /**< Outer loop learning rate */
    float resonance_threshold;        /**< Min resonance for similar tasks */
    bool transfer_entanglement;       /**< Enable entanglement transfer */

    /* Memory integration */
    uint32_t max_task_memory;         /**< Max tasks to store */
    uint32_t max_recall;              /**< Max similar tasks to recall */
    pr_meta_similarity_t similarity_metric; /**< Task similarity metric */

    /* Tier-specific parameters */
    float tier_adaptation_rate[PR_META_NUM_TIERS]; /**< Per-tier rates */

    /* Quaternion adaptation */
    float quat_adaptation_rate;       /**< Quaternion learning rate */
    bool adapt_quaternion;            /**< Enable quaternion adaptation */

    /* Entanglement transfer */
    float transfer_weight;            /**< Weight for transferred edges */
    float transfer_threshold;         /**< Min similarity for transfer */

    /* Integration features */
    pr_meta_type_t bridge_type;       /**< Bridge operation mode */
    bool enable_memory_init;          /**< Initialize from memory */
    bool enable_memory_store;         /**< Store results to memory */
    bool track_statistics;            /**< Enable statistics tracking */

    /* Algorithm selection */
    meta_algorithm_t meta_algorithm;  /**< MAML, Reptile, etc. */
    bool first_order;                 /**< Use first-order approximation */
} pr_meta_config_t;

/**
 * @brief Task representation for meta-learning
 *
 * WHAT: Encapsulates a task for meta-learning
 * WHY:  Unified task representation for memory and meta-learning
 */
typedef struct {
    uint64_t task_id;                 /**< Unique task identifier */
    const char* name;                 /**< Task name (optional) */

    /* Prime Resonant representation */
    prime_signature_t* signature;     /**< Task content signature */
    nimcp_quaternion_t quaternion;    /**< Task state quaternion */
    float phase;                      /**< Task phase (for temporal) */

    /* Meta-learning data */
    meta_task_t* meta_task;           /**< Meta-learning task data */

    /* Metadata */
    pr_meta_tier_t tier;              /**< Memory tier */
    uint64_t created_time_ms;         /**< Creation timestamp */
    uint32_t access_count;            /**< Times accessed */
} pr_meta_task_t;

/**
 * @brief Task result after meta-learning
 *
 * WHAT: Result of adapting to a task
 * WHY:  Store adapted state for future recall
 */
typedef struct {
    uint64_t task_id;                 /**< Task this result belongs to */

    /* Adapted state */
    nimcp_quaternion_t adapted_quat;  /**< Quaternion after adaptation */
    float* adapted_params;            /**< Adapted model parameters */
    uint32_t num_params;              /**< Number of parameters */

    /* Performance metrics */
    float support_loss;               /**< Loss on support set */
    float query_loss;                 /**< Loss on query set */
    float query_accuracy;             /**< Accuracy on query set */

    /* Adaptation metadata */
    uint32_t inner_steps_used;        /**< Actual inner steps */
    float total_inner_lr;             /**< Effective inner LR */
    uint32_t memory_tasks_used;       /**< Similar tasks used */

    /* Timing */
    float adaptation_time_ms;         /**< Time to adapt */
} pr_meta_result_t;

/**
 * @brief Similar task recall result
 *
 * WHAT: A similar task retrieved from memory
 * WHY:  Use for memory-guided adaptation
 */
typedef struct {
    uint64_t task_id;                 /**< Recalled task ID */
    float similarity;                 /**< Similarity score (0-1) */
    nimcp_quaternion_t quaternion;    /**< Task quaternion state */
    pr_meta_result_t* result;         /**< Stored result (if available) */
} pr_meta_recall_t;

/**
 * @brief Entanglement transfer specification
 *
 * WHAT: Specification for transferring entanglement patterns
 * WHY:  Define how to transfer structure between tasks
 */
typedef struct {
    uint64_t source_task_id;          /**< Source task */
    uint64_t target_task_id;          /**< Target task */
    float similarity;                 /**< Task similarity */
    float weight_scale;               /**< Scale for transferred edges */
    bool bidirectional;               /**< Transfer both directions */
    uint32_t max_edges;               /**< Max edges to transfer */
} pr_meta_transfer_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Operational metrics for the meta-learning bridge
 * WHY:  Track performance and optimize behavior
 */
typedef struct {
    /* Task processing */
    uint64_t total_tasks_processed;   /**< Total tasks seen */
    uint64_t tasks_stored;            /**< Tasks in memory */
    uint64_t tasks_recalled;          /**< Total recall operations */
    uint64_t tasks_transferred;       /**< Entanglement transfers */

    /* Inner loop statistics */
    uint64_t total_inner_steps;       /**< Total inner loop steps */
    float avg_inner_steps;            /**< Average steps per task */
    float avg_support_loss;           /**< Average support set loss */
    float avg_query_loss;             /**< Average query set loss */

    /* Outer loop statistics */
    uint64_t total_outer_steps;       /**< Total outer loop steps */
    float avg_meta_gradient_norm;     /**< Average meta-gradient norm */

    /* Memory statistics */
    float avg_recall_similarity;      /**< Average similarity of recalls */
    uint32_t avg_recall_count;        /**< Average similar tasks found */
    float memory_hit_rate;            /**< Rate of useful memory hits */

    /* Quaternion statistics */
    float avg_quat_adaptation;        /**< Average quaternion change */
    float avg_quaternion_distance;    /**< Avg distance between task quats */

    /* Entanglement statistics */
    uint64_t edges_transferred;       /**< Total edges transferred */
    float avg_transfer_weight;        /**< Average transferred edge weight */

    /* Per-tier statistics */
    uint64_t tasks_per_tier[PR_META_NUM_TIERS]; /**< Tasks in each tier */
    float adaptation_rate_per_tier[PR_META_NUM_TIERS]; /**< Avg rate per tier */

    /* Performance */
    float avg_adaptation_time_ms;     /**< Average adaptation time */
    float avg_recall_time_ms;         /**< Average recall time */
    uint64_t total_bridge_updates;    /**< Total update cycles */
} pr_meta_bridge_stats_t;

/**
 * @brief Per-tier meta-learning parameters
 */
typedef struct {
    float adaptation_rate;            /**< Learning rate scale */
    float memory_weight;              /**< Weight given to memory */
    float transfer_weight;            /**< Entanglement transfer weight */
    uint32_t max_inner_steps;         /**< Max inner steps for tier */
} pr_meta_tier_params_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct pr_meta_bridge_struct* pr_meta_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default meta-learning bridge configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides biologically-plausible starting point
 *
 * @return Default configuration with:
 *         - inner_steps: 5
 *         - inner_lr: 0.01
 *         - outer_lr: 0.001
 *         - resonance_threshold: 0.5
 *         - Tier rates: [1.0, 0.5, 0.2, 0.1]
 *
 * Performance: ~10ns
 *
 * Example:
 *   pr_meta_config_t config = pr_meta_config_default();
 *   config.inner_steps = 10;  // More adaptation
 *   pr_meta_bridge_t bridge = pr_meta_bridge_create(&config);
 */
NIMCP_EXPORT pr_meta_config_t pr_meta_config_default(void);

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
 * - Learning rates must be positive
 * - Step counts must be > 0
 * - Thresholds must be in [0, 1]
 * - Tier rates must be positive
 *
 * Performance: ~50ns
 */
NIMCP_EXPORT bool pr_meta_config_validate(const pr_meta_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create meta-learning bridge
 *
 * WHAT: Initialize meta-learning bridge for Prime Resonant memory
 * WHY:  Entry point for memory-aware meta-learning
 * HOW:  Allocate state, initialize task memory, set up resonance queries
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle, or NULL on failure
 *
 * Performance: O(max_task_memory) for allocation
 * Memory: ~4KB base + task memory
 *
 * Thread safety: The returned bridge is thread-safe for concurrent use
 *
 * Example:
 *   pr_meta_config_t config = pr_meta_config_default();
 *   config.bridge_type = PR_META_HYBRID;
 *   pr_meta_bridge_t bridge = pr_meta_bridge_create(&config);
 */
NIMCP_EXPORT pr_meta_bridge_t pr_meta_bridge_create(const pr_meta_config_t* config);

/**
 * @brief Destroy meta-learning bridge
 *
 * WHAT: Free all bridge resources
 * WHY:  Proper resource cleanup
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * Performance: O(tasks stored)
 */
NIMCP_EXPORT void pr_meta_bridge_destroy(pr_meta_bridge_t bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Clear task memory and reset statistics
 * WHY:  Start fresh without reallocation
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 *
 * Performance: O(tasks stored)
 */
NIMCP_EXPORT int pr_meta_bridge_reset(pr_meta_bridge_t bridge);

//=============================================================================
// Memory-Aware MAML Functions
//=============================================================================

/**
 * @brief MAML inner loop with memory enhancement
 *
 * WHAT: Perform MAML inner loop adaptation using memory
 * WHY:  Leverage stored task solutions for faster adaptation
 * HOW:  Query similar tasks, blend initializations, adapt
 *
 * @param bridge Meta-learning bridge
 * @param task Task to adapt to
 * @param forward_fn Model forward function
 * @param model Model to adapt
 * @param result Output: adaptation result
 * @return 0 on success, -1 on error
 *
 * Algorithm:
 * 1. Query memory for similar tasks
 * 2. Compute memory-weighted parameter initialization
 * 3. Run inner loop gradient steps
 * 4. Modulate learning rate by memory confidence
 * 5. Record adapted state in result
 *
 * Performance: ~1ms + model-dependent
 *
 * Example:
 *   pr_meta_task_t task = { ... };
 *   pr_meta_result_t result;
 *   pr_meta_maml_inner_loop(bridge, &task, forward_fn, model, &result);
 *   printf("Query loss: %.4f\n", result.query_loss);
 */
NIMCP_EXPORT int pr_meta_maml_inner_loop(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    pr_meta_result_t* result);

/**
 * @brief MAML outer step with memory-guided meta-gradient
 *
 * WHAT: Perform MAML outer loop update across tasks
 * WHY:  Update meta-parameters considering memory structure
 * HOW:  Accumulate memory-weighted query losses, compute meta-gradient
 *
 * @param bridge Meta-learning bridge
 * @param tasks Array of tasks
 * @param num_tasks Number of tasks
 * @param forward_fn Model forward function
 * @param model Model to meta-train
 * @param avg_query_loss Output: average query loss
 * @return 0 on success, -1 on error
 *
 * Algorithm:
 * 1. For each task, run memory-enhanced inner loop
 * 2. Weight task contributions by tier and resonance
 * 3. Compute meta-gradient with memory regularization
 * 4. Update model parameters
 * 5. Store results to memory
 *
 * Performance: O(num_tasks * inner_loop_cost)
 *
 * Example:
 *   pr_meta_task_t tasks[16];
 *   float avg_loss;
 *   pr_meta_maml_outer_step(bridge, tasks, 16, forward_fn, model, &avg_loss);
 */
NIMCP_EXPORT int pr_meta_maml_outer_step(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* tasks,
    uint32_t num_tasks,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* avg_query_loss);

/**
 * @brief Compute memory-informed initialization
 *
 * WHAT: Generate parameter initialization from memory
 * WHY:  Start adaptation closer to solution for similar tasks
 *
 * @param bridge Meta-learning bridge
 * @param task Task to initialize for
 * @param base_params Base model parameters
 * @param num_params Number of parameters
 * @param init_params Output: initialized parameters (caller-allocated)
 * @return Number of memory tasks used in initialization
 *
 * Algorithm:
 * 1. Recall K similar tasks from memory
 * 2. Retrieve their adapted parameters
 * 3. Compute similarity-weighted average
 * 4. Blend with base parameters
 *
 * Performance: ~100us for K=5
 */
NIMCP_EXPORT int pr_meta_memory_init(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    const float* base_params,
    uint32_t num_params,
    float* init_params);

/**
 * @brief Get memory-modulated learning rate
 *
 * WHAT: Compute learning rate based on memory confidence
 * WHY:  High memory similarity -> lower LR (already close)
 *       Low memory similarity -> higher LR (need to explore)
 *
 * @param bridge Meta-learning bridge
 * @param task Task being adapted
 * @param base_lr Base learning rate
 * @return Modulated learning rate
 *
 * Formula:
 *   lr = base_lr * (1 - memory_confidence * modulation_factor)
 *   where memory_confidence = max(similarity of recalled tasks)
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT float pr_meta_memory_lr(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    float base_lr);

//=============================================================================
// Resonance-Guided Task Similarity Functions
//=============================================================================

/**
 * @brief Compute task similarity using resonance
 *
 * WHAT: Measure similarity between two tasks using Prime Resonant metrics
 * WHY:  Determine if tasks are related for transfer learning
 * HOW:  Combine prime signature, quaternion, and phase similarities
 *
 * @param bridge Meta-learning bridge
 * @param task1 First task
 * @param task2 Second task
 * @return Similarity score [0, 1]
 *
 * Components:
 * - Prime signature Jaccard coefficient
 * - Quaternion geodesic distance (inverted)
 * - Phase coherence
 * - Entanglement graph overlap (if available)
 *
 * Performance: ~100ns
 *
 * Example:
 *   float sim = pr_meta_resonance_task_similarity(bridge, &task1, &task2);
 *   if (sim > 0.7f) {
 *       // Tasks are highly similar - transfer knowledge
 *   }
 */
NIMCP_EXPORT float pr_meta_resonance_task_similarity(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task1,
    const pr_meta_task_t* task2);

/**
 * @brief Compute similarity using only prime signatures
 *
 * WHAT: Fast content-only similarity
 * WHY:  When only content similarity matters
 *
 * @param task1 First task
 * @param task2 Second task
 * @return Jaccard similarity [0, 1]
 *
 * Performance: ~30ns
 */
NIMCP_EXPORT float pr_meta_signature_similarity(
    const pr_meta_task_t* task1,
    const pr_meta_task_t* task2);

/**
 * @brief Compute similarity using only quaternions
 *
 * WHAT: Semantic state similarity
 * WHY:  When task "feel" matters more than content
 *
 * @param task1 First task
 * @param task2 Second task
 * @return Quaternion similarity [0, 1]
 *
 * Performance: ~25ns
 */
NIMCP_EXPORT float pr_meta_quaternion_similarity(
    const pr_meta_task_t* task1,
    const pr_meta_task_t* task2);

/**
 * @brief Compute task embedding for similarity
 *
 * WHAT: Generate learned embedding for task
 * WHY:  Enable learned similarity metrics
 *
 * @param bridge Meta-learning bridge
 * @param task Task to embed
 * @param embedding Output: embedding vector (caller-allocated)
 * @param embed_dim Embedding dimension
 * @return 0 on success, -1 on error
 *
 * Performance: ~50us (depends on model)
 */
NIMCP_EXPORT int pr_meta_task_embedding(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    float* embedding,
    uint32_t embed_dim);

/**
 * @brief Find K most similar tasks in memory
 *
 * WHAT: Query task memory for similar tasks
 * WHY:  Retrieve relevant prior experience
 *
 * @param bridge Meta-learning bridge
 * @param task Query task
 * @param k Number of similar tasks to find
 * @param recalls Output: array of recall results (caller-allocated, size >= k)
 * @param num_found Output: actual number found (<= k)
 * @return 0 on success, -1 on error
 *
 * Performance: O(task_memory_size) for brute force, O(log N) with index
 *
 * Example:
 *   pr_meta_recall_t recalls[5];
 *   uint32_t found;
 *   pr_meta_recall_similar_tasks(bridge, &query_task, 5, recalls, &found);
 *   for (uint32_t i = 0; i < found; i++) {
 *       printf("Similar task %lu: %.3f\n", recalls[i].task_id, recalls[i].similarity);
 *   }
 */
NIMCP_EXPORT int pr_meta_recall_similar_tasks(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    uint32_t k,
    pr_meta_recall_t* recalls,
    uint32_t* num_found);

//=============================================================================
// Quaternion Meta-Parameter Functions
//=============================================================================

/**
 * @brief Adapt quaternion for task
 *
 * WHAT: Modify quaternion state based on task adaptation
 * WHY:  Encode task-specific meta-state
 *
 * @param bridge Meta-learning bridge
 * @param base_quat Base quaternion state
 * @param task Task being adapted
 * @param result Adaptation result
 * @param adapted_quat Output: adapted quaternion
 * @return 0 on success, -1 on error
 *
 * Adaptation:
 * - w: Increase with successful adaptation (consolidation)
 * - x: Adjust based on task difficulty (loss relative to prior)
 * - y: Modulate by novelty (inverse of memory similarity)
 * - z: Increase with repeated access (accessibility)
 *
 * Performance: ~50ns
 *
 * Example:
 *   nimcp_quaternion_t base = { 0.5f, 0.0f, 0.5f, 0.3f };
 *   nimcp_quaternion_t adapted;
 *   pr_meta_adapt_quaternion(bridge, base, &task, &result, &adapted);
 */
NIMCP_EXPORT int pr_meta_adapt_quaternion(
    pr_meta_bridge_t bridge,
    nimcp_quaternion_t base_quat,
    const pr_meta_task_t* task,
    const pr_meta_result_t* result,
    nimcp_quaternion_t* adapted_quat);

/**
 * @brief Compute task quaternion from meta-learning state
 *
 * WHAT: Generate quaternion representing task meta-state
 * WHY:  Initialize quaternion for new task
 *
 * @param bridge Meta-learning bridge
 * @param task Task to generate quaternion for
 * @param quat Output: generated quaternion
 * @return 0 on success, -1 on error
 *
 * Components derived from:
 * - Task complexity -> consolidation target
 * - Expected difficulty -> valence
 * - Novelty (memory distance) -> salience
 * - Prior exposure -> accessibility
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT int pr_meta_task_to_quaternion(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    nimcp_quaternion_t* quat);

/**
 * @brief Derive learning rates from quaternion
 *
 * WHAT: Convert quaternion to learning rate parameters
 * WHY:  Quaternion state guides adaptation speed
 *
 * @param bridge Meta-learning bridge
 * @param quat Task quaternion
 * @param inner_lr Output: inner loop learning rate
 * @param outer_lr Output: outer loop learning rate
 * @return 0 on success, -1 on error
 *
 * Mapping:
 * - High consolidation (w) -> lower rates (already learned)
 * - High salience (y) -> higher inner rate (important to learn)
 * - Low accessibility (z) -> higher outer rate (update meta-params)
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT int pr_meta_quaternion_to_lr(
    pr_meta_bridge_t bridge,
    nimcp_quaternion_t quat,
    float* inner_lr,
    float* outer_lr);

/**
 * @brief Interpolate quaternions for task blending
 *
 * WHAT: SLERP between task quaternions
 * WHY:  Blend states of similar tasks
 *
 * @param bridge Meta-learning bridge
 * @param quats Array of quaternions to blend
 * @param weights Array of weights (need not sum to 1)
 * @param count Number of quaternions
 * @param result Output: blended quaternion
 * @return 0 on success, -1 on error
 *
 * Performance: ~100ns for count=5
 */
NIMCP_EXPORT int pr_meta_blend_quaternions(
    pr_meta_bridge_t bridge,
    const nimcp_quaternion_t* quats,
    const float* weights,
    uint32_t count,
    nimcp_quaternion_t* result);

//=============================================================================
// Entanglement Transfer Functions
//=============================================================================

/**
 * @brief Transfer entanglement patterns between tasks
 *
 * WHAT: Copy edge structure from source to target task
 * WHY:  Transfer relational knowledge between similar tasks
 * HOW:  Scale source edges by similarity, add to target
 *
 * @param bridge Meta-learning bridge
 * @param graph Entanglement graph
 * @param source_task Source task
 * @param target_task Target task
 * @param transfer Output: transfer specification used
 * @return Number of edges transferred
 *
 * Algorithm:
 * 1. Compute task similarity
 * 2. If above threshold, find source task edges
 * 3. Scale edge weights by similarity
 * 4. Add edges to target (or strengthen existing)
 *
 * Performance: O(source_edges)
 *
 * Example:
 *   pr_meta_transfer_t transfer;
 *   uint32_t edges = pr_meta_transfer_entanglement(
 *       bridge, graph, &source_task, &target_task, &transfer);
 *   printf("Transferred %u edges with similarity %.3f\n",
 *          edges, transfer.similarity);
 */
NIMCP_EXPORT uint32_t pr_meta_transfer_entanglement(
    pr_meta_bridge_t bridge,
    entangle_graph_t graph,
    const pr_meta_task_t* source_task,
    const pr_meta_task_t* target_task,
    pr_meta_transfer_t* transfer);

/**
 * @brief Batch transfer from multiple source tasks
 *
 * WHAT: Transfer from multiple similar tasks
 * WHY:  Combine knowledge from multiple related tasks
 *
 * @param bridge Meta-learning bridge
 * @param graph Entanglement graph
 * @param source_tasks Array of source tasks
 * @param num_sources Number of source tasks
 * @param target_task Target task
 * @param max_edges_total Maximum total edges to transfer
 * @return Total number of edges transferred
 *
 * Performance: O(num_sources * avg_edges)
 */
NIMCP_EXPORT uint32_t pr_meta_transfer_batch(
    pr_meta_bridge_t bridge,
    entangle_graph_t graph,
    const pr_meta_task_t* source_tasks,
    uint32_t num_sources,
    const pr_meta_task_t* target_task,
    uint32_t max_edges_total);

/**
 * @brief Compute entanglement graph similarity
 *
 * WHAT: Measure structural similarity of task graphs
 * WHY:  Graph structure indicates relational similarity
 *
 * @param bridge Meta-learning bridge
 * @param graph Entanglement graph
 * @param task1 First task
 * @param task2 Second task
 * @return Graph similarity [0, 1]
 *
 * Metrics:
 * - Edge overlap (Jaccard on edge sets)
 * - Node overlap
 * - Path structure similarity
 *
 * Performance: O(E1 + E2) where E = edges
 */
NIMCP_EXPORT float pr_meta_graph_similarity(
    pr_meta_bridge_t bridge,
    entangle_graph_t graph,
    const pr_meta_task_t* task1,
    const pr_meta_task_t* task2);

/**
 * @brief Merge entanglement patterns for task consolidation
 *
 * WHAT: Combine graphs from multiple tasks
 * WHY:  Create unified representation for task family
 *
 * @param bridge Meta-learning bridge
 * @param graph Target graph
 * @param tasks Array of tasks to merge
 * @param num_tasks Number of tasks
 * @param weights Per-task weights (NULL = equal)
 * @return Number of edges in merged graph
 *
 * Performance: O(sum of edges)
 */
NIMCP_EXPORT uint32_t pr_meta_merge_entanglement(
    pr_meta_bridge_t bridge,
    entangle_graph_t graph,
    const pr_meta_task_t* tasks,
    uint32_t num_tasks,
    const float* weights);

//=============================================================================
// Tier-Based Meta-Learning Functions
//=============================================================================

/**
 * @brief Get adaptation rate for memory tier
 *
 * WHAT: Return tier-specific learning rate scale
 * WHY:  Different tiers have different stability needs
 *
 * @param bridge Meta-learning bridge
 * @param tier Memory tier
 * @return Adaptation rate multiplier
 *
 * Tier rates:
 * - Z0 (working): 1.0x - fastest adaptation
 * - Z1 (short-term): 0.5x - moderate
 * - Z2 (long-term): 0.2x - slow
 * - Z3 (deep storage): 0.1x - very slow
 *
 * Performance: ~5ns
 *
 * Example:
 *   float rate = pr_meta_tier_adaptation_rate(bridge, PR_META_TIER_Z1);
 *   float effective_lr = base_lr * rate;  // 0.5x for Z1
 */
NIMCP_EXPORT float pr_meta_tier_adaptation_rate(
    pr_meta_bridge_t bridge,
    pr_meta_tier_t tier);

/**
 * @brief Get tier parameters
 *
 * WHAT: Retrieve full tier-specific parameter set
 * WHY:  Access all tier-specific meta-learning settings
 *
 * @param bridge Meta-learning bridge
 * @param tier Memory tier
 * @param params Output: tier parameters
 * @return 0 on success, -1 on error
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT int pr_meta_get_tier_params(
    pr_meta_bridge_t bridge,
    pr_meta_tier_t tier,
    pr_meta_tier_params_t* params);

/**
 * @brief Assign task to appropriate tier
 *
 * WHAT: Determine which tier a task belongs in
 * WHY:  Place tasks appropriately in memory hierarchy
 *
 * @param bridge Meta-learning bridge
 * @param task Task to classify
 * @param result Adaptation result (affects tier decision)
 * @return Recommended tier
 *
 * Classification based on:
 * - Recency of task (newer -> higher tier)
 * - Adaptation success (better -> lower tier)
 * - Access frequency (more -> lower tier)
 * - Task similarity to existing (similar -> same tier)
 *
 * Performance: ~50ns
 */
NIMCP_EXPORT pr_meta_tier_t pr_meta_classify_tier(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    const pr_meta_result_t* result);

/**
 * @brief Promote/demote task between tiers
 *
 * WHAT: Move task to different tier based on usage
 * WHY:  Implement memory consolidation
 *
 * @param bridge Meta-learning bridge
 * @param task_id Task to move
 * @param new_tier Target tier
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_meta_move_tier(
    pr_meta_bridge_t bridge,
    uint64_t task_id,
    pr_meta_tier_t new_tier);

//=============================================================================
// Task Memory Functions
//=============================================================================

/**
 * @brief Store task and result in memory
 *
 * WHAT: Add task to bridge memory for future recall
 * WHY:  Build up experience for memory-guided learning
 *
 * @param bridge Meta-learning bridge
 * @param task Task to store
 * @param result Adaptation result
 * @return 0 on success, -1 on error (memory full, invalid)
 *
 * Storage includes:
 * - Task representation (signature, quaternion, phase)
 * - Adaptation result (parameters, losses, accuracy)
 * - Metadata (tier, timestamp, access count)
 *
 * Performance: O(1) average, O(N) for eviction
 *
 * Example:
 *   pr_meta_task_t task = { .task_id = 42, .quaternion = q, ... };
 *   pr_meta_result_t result = { .query_loss = 0.1f, ... };
 *   pr_meta_store_task_memory(bridge, &task, &result);
 */
NIMCP_EXPORT int pr_meta_store_task_memory(
    pr_meta_bridge_t bridge,
    const pr_meta_task_t* task,
    const pr_meta_result_t* result);

/**
 * @brief Recall task from memory by ID
 *
 * WHAT: Retrieve specific task from memory
 * WHY:  Access previously stored task
 *
 * @param bridge Meta-learning bridge
 * @param task_id Task ID to recall
 * @param task Output: task data (caller-allocated)
 * @param result Output: result data (optional, can be NULL)
 * @return true if found, false otherwise
 *
 * Performance: O(1) with hash table
 */
NIMCP_EXPORT bool pr_meta_recall_task(
    pr_meta_bridge_t bridge,
    uint64_t task_id,
    pr_meta_task_t* task,
    pr_meta_result_t* result);

/**
 * @brief Remove task from memory
 *
 * WHAT: Delete task from bridge memory
 * WHY:  Explicit memory management, eviction
 *
 * @param bridge Meta-learning bridge
 * @param task_id Task to remove
 * @return true if removed, false if not found
 *
 * Performance: O(1)
 */
NIMCP_EXPORT bool pr_meta_forget_task(
    pr_meta_bridge_t bridge,
    uint64_t task_id);

/**
 * @brief Get number of tasks in memory
 *
 * @param bridge Meta-learning bridge
 * @return Number of stored tasks
 */
NIMCP_EXPORT uint32_t pr_meta_memory_size(pr_meta_bridge_t bridge);

/**
 * @brief Evict least useful tasks from memory
 *
 * WHAT: Remove low-value tasks to make space
 * WHY:  Memory is limited
 *
 * @param bridge Meta-learning bridge
 * @param count Number of tasks to evict
 * @return Number actually evicted
 *
 * Eviction priority (lowest priority removed first):
 * - Low access count
 * - High age
 * - Low adaptation success
 * - Low similarity to remaining tasks
 *
 * Performance: O(N log count)
 */
NIMCP_EXPORT uint32_t pr_meta_evict_tasks(
    pr_meta_bridge_t bridge,
    uint32_t count);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve operational metrics
 * WHY:  Monitoring and debugging
 *
 * @param bridge Meta-learning bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_meta_get_stats(
    pr_meta_bridge_t bridge,
    pr_meta_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Start fresh measurement period
 *
 * @param bridge Meta-learning bridge
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_meta_reset_stats(pr_meta_bridge_t bridge);

/**
 * @brief Print statistics to stdout
 *
 * @param bridge Meta-learning bridge
 */
NIMCP_EXPORT void pr_meta_print_stats(pr_meta_bridge_t bridge);

//=============================================================================
// Integration Functions
//=============================================================================

/**
 * @brief Connect to meta-learning context
 *
 * WHAT: Integrate with existing meta-learning system
 * WHY:  Bidirectional communication
 *
 * @param bridge Meta-learning bridge
 * @param meta_ctx Meta-learning context
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_meta_connect_meta_ctx(
    pr_meta_bridge_t bridge,
    meta_ctx_t* meta_ctx);

/**
 * @brief Connect to entanglement graph
 *
 * WHAT: Associate bridge with entanglement graph
 * WHY:  Enable entanglement transfer operations
 *
 * @param bridge Meta-learning bridge
 * @param graph Entanglement graph
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_meta_connect_graph(
    pr_meta_bridge_t bridge,
    entangle_graph_t graph);

/**
 * @brief Main bridge update
 *
 * WHAT: Perform periodic bridge maintenance
 * WHY:  Decay unused entries, update statistics
 *
 * @param bridge Meta-learning bridge
 * @param dt_ms Time since last update (milliseconds)
 * @return 0 on success, -1 on error
 *
 * Update includes:
 * - Decay task relevance scores
 * - Check for tier promotion/demotion
 * - Update running statistics
 * - Compact memory if needed
 *
 * Performance: ~100us
 */
NIMCP_EXPORT int pr_meta_bridge_update(
    pr_meta_bridge_t bridge,
    float dt_ms);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get meta type name as string
 *
 * @param type Meta bridge type
 * @return Human-readable string
 */
NIMCP_EXPORT const char* pr_meta_type_name(pr_meta_type_t type);

/**
 * @brief Get tier name as string
 *
 * @param tier Memory tier
 * @return Human-readable string
 */
NIMCP_EXPORT const char* pr_meta_tier_name(pr_meta_tier_t tier);

/**
 * @brief Get similarity metric name
 *
 * @param metric Similarity metric type
 * @return Human-readable string
 */
NIMCP_EXPORT const char* pr_meta_similarity_name(pr_meta_similarity_t metric);

/**
 * @brief Print task summary
 *
 * @param task Task to print
 */
NIMCP_EXPORT void pr_meta_task_print(const pr_meta_task_t* task);

/**
 * @brief Print result summary
 *
 * @param result Result to print
 */
NIMCP_EXPORT void pr_meta_result_print(const pr_meta_result_t* result);

/**
 * @brief Validate task structure
 *
 * @param task Task to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool pr_meta_task_validate(const pr_meta_task_t* task);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_META_BRIDGE_H */
