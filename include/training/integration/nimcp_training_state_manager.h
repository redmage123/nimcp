/**
 * @file nimcp_training_state_manager.h
 * @brief State Manager Integration for Training Modules
 *
 * WHAT: State serialization/recovery for training subsystems
 * WHY:  Enable checkpointing and fault tolerance for long training runs
 * HOW:  Register training contexts with state manager for checkpoint/restore
 *
 * PHASE 8: System-Wide Health Integration
 *
 * REGISTERED MODULES:
 * - Distributed Training: Worker state, sync progress
 * - Meta-Learning: Task adaptation state
 * - Adversarial Training: Attack/defense state
 * - Hyperparameter Optimization: Trial state
 *
 * USAGE:
 * ```c
 * // Create training state registration
 * training_state_registry_t* registry = training_state_registry_create();
 *
 * // Register your training context
 * dist_ctx_t* dist = dist_create(&config);
 * training_state_register_distributed(registry, dist);
 *
 * // Link to state manager
 * training_state_link_to_manager(registry, state_manager);
 * ```
 *
 * @author NIMCP Team
 * @date 2026-01-25
 * @version 1.0.0
 */

#ifndef NIMCP_TRAINING_STATE_MANAGER_H
#define NIMCP_TRAINING_STATE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

/* State manager from fault tolerance */
struct nimcp_state_manager;
typedef struct nimcp_state_manager nimcp_state_manager_t;

/* Training module contexts */
struct dist_ctx_s;
typedef struct dist_ctx_s dist_ctx_t;

struct meta_ctx_s;
typedef struct meta_ctx_s meta_ctx_t;

struct adv_ctx_s;
typedef struct adv_ctx_s adv_ctx_t;

struct hpo_ctx_s;
typedef struct hpo_ctx_s hpo_ctx_t;

//=============================================================================
// Constants
//=============================================================================

/** Maximum training modules that can be registered */
#define TRAINING_STATE_MAX_MODULES 16

/** Training state registry magic for validation */
#define TRAINING_STATE_REGISTRY_MAGIC 0x54535247  /* "TSRG" */

//=============================================================================
// Training State Registry
//=============================================================================

/**
 * @brief Registered training module entry
 */
typedef struct {
    const char* module_name;         /**< Module name */
    void* context;                   /**< Module context */
    bool enabled;                    /**< Is module enabled */
    uint64_t last_checkpoint_time;   /**< Last checkpoint time */
    size_t last_checkpoint_size;     /**< Last checkpoint size */
    uint32_t checkpoint_count;       /**< Total checkpoints */
    uint32_t restore_count;          /**< Total restores */
} training_state_module_t;

/**
 * @brief Training state registry
 *
 * WHAT: Central registry for training module state management
 * WHY:  Coordinate state operations across training modules
 * HOW:  Hold references to contexts, provide unified ops
 */
typedef struct {
    uint32_t magic;                  /**< Validation magic */
    bool initialized;                /**< Is registry initialized */

    /* Registered training modules */
    training_state_module_t modules[TRAINING_STATE_MAX_MODULES];
    uint32_t module_count;           /**< Number of registered modules */

    /* Specific module references */
    dist_ctx_t* distributed_ctx;     /**< Distributed training context */
    meta_ctx_t* meta_learning_ctx;   /**< Meta-learning context */
    adv_ctx_t* adversarial_ctx;      /**< Adversarial training context */
    hpo_ctx_t* hpo_ctx;              /**< HPO context */

    /* Linked state manager */
    nimcp_state_manager_t* state_manager;  /**< Linked state manager */
    bool owns_state_manager;         /**< Do we own the state manager? */

    /* Statistics */
    uint64_t total_checkpoints;      /**< Total checkpoint operations */
    uint64_t total_restores;         /**< Total restore operations */
    uint64_t total_validation_errors;/**< Validation failures */
} training_state_registry_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create training state registry
 *
 * @return Registry or NULL on failure
 */
training_state_registry_t* training_state_registry_create(void);

/**
 * @brief Destroy training state registry
 *
 * @param registry Registry to destroy (NULL-safe)
 */
void training_state_registry_destroy(training_state_registry_t* registry);

//=============================================================================
// Module Registration API
//=============================================================================

/**
 * @brief Register distributed training context
 *
 * @param registry Training state registry
 * @param ctx Distributed training context
 * @return 0 on success, negative on error
 */
int training_state_register_distributed(
    training_state_registry_t* registry,
    dist_ctx_t* ctx
);

/**
 * @brief Register meta-learning context
 *
 * @param registry Training state registry
 * @param ctx Meta-learning context
 * @return 0 on success, negative on error
 */
int training_state_register_meta_learning(
    training_state_registry_t* registry,
    meta_ctx_t* ctx
);

/**
 * @brief Register adversarial training context
 *
 * @param registry Training state registry
 * @param ctx Adversarial training context
 * @return 0 on success, negative on error
 */
int training_state_register_adversarial(
    training_state_registry_t* registry,
    adv_ctx_t* ctx
);

/**
 * @brief Register HPO context
 *
 * @param registry Training state registry
 * @param ctx HPO context
 * @return 0 on success, negative on error
 */
int training_state_register_hpo(
    training_state_registry_t* registry,
    hpo_ctx_t* ctx
);

/**
 * @brief Unregister a training module
 *
 * @param registry Training state registry
 * @param module_name Module name to unregister
 * @return 0 on success, negative on error
 */
int training_state_unregister(
    training_state_registry_t* registry,
    const char* module_name
);

//=============================================================================
// State Manager Integration API
//=============================================================================

/**
 * @brief Link registry to state manager
 *
 * WHAT: Register all training modules with state manager
 * WHY:  Enable checkpoint/restore through state manager
 * HOW:  Create state ops for each module, register with state manager
 *
 * @param registry Training state registry
 * @param manager State manager to link to
 * @return 0 on success, negative on error
 */
int training_state_link_to_manager(
    training_state_registry_t* registry,
    nimcp_state_manager_t* manager
);

/**
 * @brief Unlink registry from state manager
 *
 * @param registry Training state registry
 * @return 0 on success, negative on error
 */
int training_state_unlink_from_manager(training_state_registry_t* registry);

//=============================================================================
// Checkpoint/Restore API (Direct)
//=============================================================================

/**
 * @brief Checkpoint all training modules
 *
 * @param registry Training state registry
 * @param buffer Output buffer (NULL to query size)
 * @param size In: buffer size, Out: bytes written or required
 * @return 0 on success, negative on error
 */
int training_state_checkpoint_all(
    training_state_registry_t* registry,
    uint8_t* buffer,
    size_t* size
);

/**
 * @brief Restore all training modules from checkpoint
 *
 * @param registry Training state registry
 * @param buffer Input buffer with checkpoint data
 * @param size Size of input buffer
 * @return 0 on success, negative on error
 */
int training_state_restore_all(
    training_state_registry_t* registry,
    const uint8_t* buffer,
    size_t size
);

/**
 * @brief Validate all training module states
 *
 * @param registry Training state registry
 * @return Number of valid modules, negative on error
 */
int training_state_validate_all(training_state_registry_t* registry);

/**
 * @brief Reset all training module states
 *
 * @param registry Training state registry
 * @return Number of modules reset, negative on error
 */
int training_state_reset_all(training_state_registry_t* registry);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get number of registered training modules
 *
 * @param registry Training state registry
 * @return Number of registered modules
 */
uint32_t training_state_get_module_count(training_state_registry_t* registry);

/**
 * @brief Get total checkpoint size
 *
 * @param registry Training state registry
 * @return Total size in bytes
 */
size_t training_state_get_total_size(training_state_registry_t* registry);

/**
 * @brief Check if module is registered
 *
 * @param registry Training state registry
 * @param module_name Module name
 * @return true if registered, false otherwise
 */
bool training_state_is_registered(
    training_state_registry_t* registry,
    const char* module_name
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Training state statistics
 */
typedef struct {
    uint32_t registered_modules;     /**< Number of registered modules */
    uint64_t total_checkpoints;      /**< Total checkpoints performed */
    uint64_t total_restores;         /**< Total restores performed */
    uint64_t validation_errors;      /**< Total validation errors */
    size_t total_state_size;         /**< Total state size in bytes */
    uint64_t last_checkpoint_time;   /**< Last checkpoint timestamp */
} training_state_stats_t;

/**
 * @brief Get training state statistics
 *
 * @param registry Training state registry
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int training_state_get_stats(
    training_state_registry_t* registry,
    training_state_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_STATE_MANAGER_H */
