/**
 * @file nimcp_training_module.h
 * @brief Training Module Integration with Security and Memory Pools
 *
 * Phase TM-1: Training Module Infrastructure
 *
 * Provides unified infrastructure for all training/plasticity modules:
 * 1. SECURITY INTEGRATION - Auto-registration with security framework
 * 2. UNIFIED MEMORY - CoW-enabled memory pools for weight matrices
 * 3. LIFECYCLE MANAGEMENT - Consistent init/destroy patterns
 * 4. BRAIN INTEGRATION - Direct integration with brain module
 *
 * ARCHITECTURE:
 * +------------------------------------------------------------------+
 * |                    Training Module Framework                      |
 * +------------------------------------------------------------------+
 * |  Security Context  |  Unified Memory  |  Brain Integration       |
 * +------------------------------------------------------------------+
 * |  STDP | Dendritic | Predictive | BCM | Homeostatic | Brain Learn |
 * +------------------------------------------------------------------+
 *
 * @version 1.0.0
 * @author NIMCP Training Team
 * @date 2025-11-27
 */

#ifndef NIMCP_TRAINING_MODULE_H
#define NIMCP_TRAINING_MODULE_H

#include "utils/validation/nimcp_common.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Training Module Types
//=============================================================================

/**
 * @brief Training module type enumeration
 */
typedef enum {
    NIMCP_TRAIN_MOD_STDP = 0,           /**< Spike-Timing Dependent Plasticity */
    NIMCP_TRAIN_MOD_DENDRITIC,          /**< Dendritic plasticity */
    NIMCP_TRAIN_MOD_PREDICTIVE,         /**< Predictive coding */
    NIMCP_TRAIN_MOD_BCM,                /**< BCM plasticity rule */
    NIMCP_TRAIN_MOD_HOMEOSTATIC,        /**< Homeostatic plasticity */
    NIMCP_TRAIN_MOD_ELIGIBILITY,        /**< Eligibility traces */
    NIMCP_TRAIN_MOD_BRAIN_LEARNING,     /**< Brain-level learning */
    NIMCP_TRAIN_MOD_BIOLOGICAL,         /**< Biological plasticity */
    NIMCP_TRAIN_MOD_COUNT
} nimcp_training_module_type_t;

/**
 * @brief Training phase enumeration
 */
typedef enum {
    NIMCP_TRAIN_PHASE_T1 = 0,           /**< T1: Homeostatic learning */
    NIMCP_TRAIN_PHASE_T2,               /**< T2: Dendritic learning */
    NIMCP_TRAIN_PHASE_T3,               /**< T3: Predictive coding */
    NIMCP_TRAIN_PHASE_T4,               /**< T4: Meta-learning */
    NIMCP_TRAIN_PHASE_COUNT
} nimcp_training_phase_t;

/**
 * @brief Training module state
 */
typedef enum {
    NIMCP_TRAIN_STATE_UNINITIALIZED = 0,
    NIMCP_TRAIN_STATE_INITIALIZED,
    NIMCP_TRAIN_STATE_ACTIVE,
    NIMCP_TRAIN_STATE_PAUSED,
    NIMCP_TRAIN_STATE_ERROR
} nimcp_training_state_t;

//=============================================================================
// Training Module Context
//=============================================================================

/**
 * @brief Training module configuration
 */
typedef struct {
    nimcp_training_module_type_t type;   /**< Module type */
    const char* name;                    /**< Module name (human-readable) */

    /* Security configuration */
    bool enable_security;                /**< Enable security integration */
    nimcp_sec_integration_t* security_ctx; /**< Shared security context (NULL = create own) */

    /* Memory configuration */
    bool enable_unified_memory;          /**< Enable unified memory pools */
    unified_mem_manager_t mem_manager;   /**< Shared memory manager (NULL = create own) */
    size_t weight_pool_size;             /**< Size of weight pool in bytes (0 = default) */
    bool enable_cow;                     /**< Enable Copy-on-Write for weights */

    /* Training parameters */
    nimcp_training_phase_t phase;        /**< Training phase */
    double learning_rate;                /**< Base learning rate */
    double momentum;                     /**< Momentum coefficient */
    double weight_decay;                 /**< L2 regularization */

    /* User context */
    void* user_data;                     /**< User-provided context */
} nimcp_training_module_config_t;

/**
 * @brief Training module statistics
 */
typedef struct {
    /* Training statistics */
    uint64_t training_steps;             /**< Total training steps */
    uint64_t weight_updates;             /**< Total weight updates */
    double total_delta;                  /**< Total weight change magnitude */
    double avg_learning_rate;            /**< Average effective learning rate */

    /* Memory statistics */
    size_t weights_allocated;            /**< Total weight memory allocated */
    size_t weights_shared;               /**< Weight memory in shared state (CoW) */
    size_t memory_saved;                 /**< Memory saved via CoW */
    uint64_t cow_triggers;               /**< CoW copy operations */

    /* Security statistics */
    uint32_t security_module_id;         /**< Registered security module ID */
    double trust_score;                  /**< Current trust score */
    uint64_t anomalies_detected;         /**< Anomalies detected by security */
    uint64_t integrity_checks;           /**< Integrity checks performed */

    /* Performance statistics */
    uint64_t total_time_ns;              /**< Total processing time */
    uint64_t cow_time_ns;                /**< Time spent in CoW operations */
    uint64_t alloc_time_ns;              /**< Time spent in allocations */
} nimcp_training_stats_t;

/**
 * @brief Training module weight handle
 *
 * Wraps unified_mem_handle_t with training-specific metadata.
 */
typedef struct nimcp_training_weights {
    unified_mem_handle_t handle;         /**< Underlying unified memory handle */
    size_t num_weights;                  /**< Number of weights */
    size_t dimensions[4];                /**< Weight dimensions (max 4D) */
    uint32_t num_dims;                   /**< Number of dimensions */
    uint32_t region_id;                  /**< Security region ID (0 = not registered) */
    bool is_frozen;                      /**< Weights are frozen (no updates) */
} nimcp_training_weights_t;

/**
 * @brief Training module context (opaque)
 */
typedef struct nimcp_training_context nimcp_training_context_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create training module context
 *
 * WHAT: Creates infrastructure for a training module
 * WHY:  Centralized setup of security, memory, and brain integration
 * HOW:  Allocates context, registers with security, creates memory pools
 *
 * @param config Configuration (NULL for defaults)
 * @return Context or NULL on failure
 *
 * EXAMPLE:
 * ```c
 * nimcp_training_module_config_t cfg = nimcp_training_default_config();
 * cfg.type = NIMCP_TRAIN_MOD_STDP;
 * cfg.name = "stdp_plasticity";
 * cfg.enable_security = true;
 * cfg.enable_unified_memory = true;
 * cfg.enable_cow = true;
 * nimcp_training_context_t* ctx = nimcp_training_create(&cfg);
 * ```
 */
nimcp_training_context_t* nimcp_training_create(
    const nimcp_training_module_config_t* config
);

/**
 * @brief Initialize training module
 *
 * WHAT: Performs full initialization after creation
 * WHY:  Two-phase init allows configuration modifications
 * HOW:  Finalizes security registration, memory setup
 *
 * @param ctx Training context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_training_init(nimcp_training_context_t* ctx);

/**
 * @brief Destroy training module context
 *
 * WHAT: Cleans up all resources
 * WHY:  Proper shutdown with security unregistration
 * HOW:  Unregisters from security, releases memory pools
 *
 * @param ctx Training context
 */
void nimcp_training_destroy(nimcp_training_context_t* ctx);

/**
 * @brief Get default configuration
 *
 * @return Default configuration with sensible values
 */
nimcp_training_module_config_t nimcp_training_default_config(void);

//=============================================================================
// Weight Management (Unified Memory Integration)
//=============================================================================

/**
 * @brief Allocate training weights with CoW support
 *
 * WHAT: Allocates weight matrix using unified memory
 * WHY:  Enables efficient cloning and checkpointing
 * HOW:  Uses page-level CoW for large matrices, object-level for small
 *
 * @param ctx Training context
 * @param num_weights Number of weights
 * @param initial_weights Initial values (NULL = zero-initialized)
 * @param weights Output: weight handle
 * @return NIMCP_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * nimcp_training_weights_t weights;
 * float init_data[1000] = {0};
 * nimcp_training_alloc_weights(ctx, 1000, init_data, &weights);
 * ```
 */
nimcp_result_t nimcp_training_alloc_weights(
    nimcp_training_context_t* ctx,
    size_t num_weights,
    const float* initial_weights,
    nimcp_training_weights_t* weights
);

/**
 * @brief Allocate N-dimensional weight tensor
 *
 * WHAT: Allocates weight tensor with specified dimensions
 * WHY:  Support for convolutional and multi-dimensional weights
 * HOW:  Calculates total size, allocates, stores dimension info
 *
 * @param ctx Training context
 * @param dimensions Array of dimensions
 * @param num_dims Number of dimensions (1-4)
 * @param initial_weights Initial values (NULL = zero-initialized)
 * @param weights Output: weight handle
 * @return NIMCP_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * nimcp_training_weights_t conv_weights;
 * size_t dims[] = {64, 32, 3, 3};  // 64x32x3x3 conv kernel
 * nimcp_training_alloc_weights_nd(ctx, dims, 4, NULL, &conv_weights);
 * ```
 */
nimcp_result_t nimcp_training_alloc_weights_nd(
    nimcp_training_context_t* ctx,
    const size_t* dimensions,
    uint32_t num_dims,
    const float* initial_weights,
    nimcp_training_weights_t* weights
);

/**
 * @brief Clone weights (CoW semantics)
 *
 * WHAT: Creates a CoW clone of weights
 * WHY:  O(1) brain state cloning, efficient checkpointing
 * HOW:  Increments refcount, shares until write
 *
 * @param ctx Training context
 * @param source Source weights
 * @param dest Output: cloned weights
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(1) - just increments refcount
 */
nimcp_result_t nimcp_training_clone_weights(
    nimcp_training_context_t* ctx,
    const nimcp_training_weights_t* source,
    nimcp_training_weights_t* dest
);

/**
 * @brief Free training weights
 *
 * @param ctx Training context
 * @param weights Weights to free
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_training_free_weights(
    nimcp_training_context_t* ctx,
    nimcp_training_weights_t* weights
);

/**
 * @brief Get read-only access to weights
 *
 * WHAT: Returns const pointer without triggering CoW
 * WHY:  Fast read access for forward pass
 * HOW:  Returns shared data pointer
 *
 * @param ctx Training context
 * @param weights Weight handle
 * @return Const pointer to weight data, or NULL on error
 *
 * WARNING: Do NOT modify the returned data!
 */
const float* nimcp_training_read_weights(
    nimcp_training_context_t* ctx,
    const nimcp_training_weights_t* weights
);

/**
 * @brief Get writable access to weights
 *
 * WHAT: Returns writable pointer, triggers CoW if needed
 * WHY:  Safe write access during training
 * HOW:  Triggers copy if shared, returns private data
 *
 * @param ctx Training context
 * @param weights Weight handle
 * @return Writable pointer to weight data, or NULL on error
 *
 * NOTE: May trigger O(n) copy if weights are shared
 */
float* nimcp_training_write_weights(
    nimcp_training_context_t* ctx,
    nimcp_training_weights_t* weights
);

/**
 * @brief Check if weights are shared (CoW)
 *
 * @param weights Weight handle
 * @return true if shared, false if private
 */
bool nimcp_training_weights_are_shared(
    const nimcp_training_weights_t* weights
);

/**
 * @brief Register weights with security monitoring
 *
 * WHAT: Registers weight memory region with security system
 * WHY:  Enables tampering detection for trained parameters
 * HOW:  Calculates entropy baseline, registers region
 *
 * @param ctx Training context
 * @param weights Weight handle
 * @param name Human-readable name for region
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_training_register_weights_security(
    nimcp_training_context_t* ctx,
    nimcp_training_weights_t* weights,
    const char* name
);

/**
 * @brief Update security baseline after legitimate weight changes
 *
 * @param ctx Training context
 * @param weights Weight handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_training_update_weights_baseline(
    nimcp_training_context_t* ctx,
    nimcp_training_weights_t* weights
);

//=============================================================================
// Checkpoint/Snapshot API
//=============================================================================

/**
 * @brief Training checkpoint handle
 */
typedef struct nimcp_training_checkpoint {
    unified_mem_snapshot_t* snapshots;   /**< Array of weight snapshots */
    size_t num_snapshots;                /**< Number of snapshots */
    uint64_t step_number;                /**< Training step at checkpoint */
    uint64_t timestamp;                  /**< Checkpoint timestamp */
} nimcp_training_checkpoint_t;

/**
 * @brief Create checkpoint of training state
 *
 * WHAT: Creates instant snapshot of all weights
 * WHY:  Fast rollback for failed training steps
 * HOW:  Uses CoW snapshots for O(1) creation
 *
 * @param ctx Training context
 * @param weights Array of weight handles
 * @param num_weights Number of weight handles
 * @param checkpoint Output: checkpoint handle
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(n) where n = number of weight tensors (not total weights!)
 */
nimcp_result_t nimcp_training_checkpoint_create(
    nimcp_training_context_t* ctx,
    nimcp_training_weights_t* weights,
    size_t num_weights,
    nimcp_training_checkpoint_t* checkpoint
);

/**
 * @brief Restore training state from checkpoint
 *
 * WHAT: Restores all weights to checkpoint state
 * WHY:  Rollback after bad training step
 * HOW:  Discards changes, re-shares checkpoint data
 *
 * @param ctx Training context
 * @param weights Array of weight handles
 * @param num_weights Number of weight handles
 * @param checkpoint Checkpoint to restore from
 * @return NIMCP_SUCCESS or error code
 *
 * WARNING: Discards all changes made since checkpoint!
 */
nimcp_result_t nimcp_training_checkpoint_restore(
    nimcp_training_context_t* ctx,
    nimcp_training_weights_t* weights,
    size_t num_weights,
    const nimcp_training_checkpoint_t* checkpoint
);

/**
 * @brief Destroy checkpoint
 *
 * @param ctx Training context
 * @param checkpoint Checkpoint to destroy
 */
void nimcp_training_checkpoint_destroy(
    nimcp_training_context_t* ctx,
    nimcp_training_checkpoint_t* checkpoint
);

//=============================================================================
// Security Integration
//=============================================================================

/**
 * @brief Record successful training interaction
 *
 * @param ctx Training context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_training_record_success(nimcp_training_context_t* ctx);

/**
 * @brief Record failed training interaction
 *
 * @param ctx Training context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_training_record_failure(nimcp_training_context_t* ctx);

/**
 * @brief Check if training module is trusted
 *
 * @param ctx Training context
 * @return true if trusted, false if not or error
 */
bool nimcp_training_is_trusted(nimcp_training_context_t* ctx);

/**
 * @brief Get security module ID
 *
 * @param ctx Training context
 * @return Security module ID or 0 if not registered
 */
uint32_t nimcp_training_get_security_id(nimcp_training_context_t* ctx);

/**
 * @brief Get security context
 *
 * @param ctx Training context
 * @return Security context or NULL if not enabled
 */
nimcp_sec_integration_t* nimcp_training_get_security_ctx(
    nimcp_training_context_t* ctx
);

//=============================================================================
// Statistics and State
//=============================================================================

/**
 * @brief Get training statistics
 *
 * @param ctx Training context
 * @param stats Output: statistics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_training_get_stats(
    nimcp_training_context_t* ctx,
    nimcp_training_stats_t* stats
);

/**
 * @brief Reset training statistics
 *
 * @param ctx Training context
 */
void nimcp_training_reset_stats(nimcp_training_context_t* ctx);

/**
 * @brief Get training module state
 *
 * @param ctx Training context
 * @return Current state
 */
nimcp_training_state_t nimcp_training_get_state(nimcp_training_context_t* ctx);

/**
 * @brief Get training module type
 *
 * @param ctx Training context
 * @return Module type
 */
nimcp_training_module_type_t nimcp_training_get_type(
    nimcp_training_context_t* ctx
);

/**
 * @brief Get unified memory manager
 *
 * @param ctx Training context
 * @return Memory manager or NULL if not enabled
 */
unified_mem_manager_t nimcp_training_get_mem_manager(
    nimcp_training_context_t* ctx
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get module type name
 *
 * @param type Module type
 * @return Human-readable name
 */
const char* nimcp_training_type_name(nimcp_training_module_type_t type);

/**
 * @brief Get phase name
 *
 * @param phase Training phase
 * @return Human-readable name
 */
const char* nimcp_training_phase_name(nimcp_training_phase_t phase);

/**
 * @brief Get state name
 *
 * @param state Training state
 * @return Human-readable name
 */
const char* nimcp_training_state_name(nimcp_training_state_t state);

//=============================================================================
// Convenience Macros
//=============================================================================

/**
 * @brief Quick weight allocation with default CoW
 */
#define NIMCP_TRAINING_ALLOC_WEIGHTS(ctx, n, w) \
    nimcp_training_alloc_weights(ctx, n, NULL, w)

/**
 * @brief Quick read access
 */
#define NIMCP_TRAINING_READ(ctx, w) \
    nimcp_training_read_weights(ctx, w)

/**
 * @brief Quick write access
 */
#define NIMCP_TRAINING_WRITE(ctx, w) \
    nimcp_training_write_weights(ctx, w)

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_MODULE_H */
