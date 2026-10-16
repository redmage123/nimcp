/**
 * @file nimcp_state_manager.h
 * @brief Module State Manager for Fault Tolerance
 *
 * WHAT: Centralized registry for module state operations
 * WHY:  Enable consistent checkpointing and recovery across all modules
 * HOW:  Modules register state ops (serialize, deserialize, validate, reset)
 *
 * PHASE 8: System-Wide Health Integration
 * Part of the resilience infrastructure for state consistency support.
 *
 * USAGE:
 * ```c
 * // Define state ops for your module
 * static nimcp_module_state_ops_t my_module_ops = {
 *     .serialize = my_module_serialize,
 *     .deserialize = my_module_deserialize,
 *     .validate = my_module_validate,
 *     .reset = my_module_reset,
 *     .get_size = my_module_get_state_size
 * };
 *
 * // Register during init
 * nimcp_state_manager_register(manager, "my_module", &my_module_ops, my_ctx);
 *
 * // State manager handles checkpoint/recovery
 * nimcp_state_manager_checkpoint_all(manager, buffer, &size);
 * nimcp_state_manager_restore_all(manager, buffer, size);
 * ```
 *
 * @author NIMCP Team
 * @date 2026-01-20
 * @version 1.0.0
 */

#ifndef NIMCP_STATE_MANAGER_H
#define NIMCP_STATE_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of modules that can be registered */
#define NIMCP_STATE_MANAGER_MAX_MODULES 128

/** Maximum module name length */
#define NIMCP_STATE_MANAGER_MAX_NAME_LEN 64

/** State manager magic for validation */
#define NIMCP_STATE_MANAGER_MAGIC 0x4E53544D  /* "NSTM" */

//=============================================================================
// Module State Operations Interface
//=============================================================================

/**
 * @brief Module state operations interface
 *
 * WHAT: Function pointers for module state management
 * WHY:  Enable uniform state handling across heterogeneous modules
 * HOW:  Each module implements these functions for its state
 *
 * All functions return 0 on success, negative on error.
 */
typedef struct nimcp_module_state_ops {
    /**
     * @brief Serialize module state to buffer
     * @param module_state Pointer to module's state context
     * @param buffer Output buffer (NULL to query size only)
     * @param size In: buffer size, Out: bytes written or required
     * @return 0 on success, -1 on error, -2 if buffer too small
     */
    int (*serialize)(void* module_state, uint8_t* buffer, size_t* size);

    /**
     * @brief Deserialize module state from buffer
     * @param module_state Pointer to module's state context
     * @param buffer Input buffer containing serialized state
     * @param size Size of input buffer
     * @return 0 on success, negative on error
     */
    int (*deserialize)(void* module_state, const uint8_t* buffer, size_t size);

    /**
     * @brief Validate module state integrity
     * @param module_state Pointer to module's state context
     * @return 0 if valid, negative error code if invalid
     */
    int (*validate)(void* module_state);

    /**
     * @brief Reset module state to default/safe values
     * @param module_state Pointer to module's state context
     * @return 0 on success, negative on error
     */
    int (*reset)(void* module_state);

    /**
     * @brief Get estimated serialized state size
     * @param module_state Pointer to module's state context
     * @return Estimated size in bytes, 0 if no state
     */
    size_t (*get_size)(void* module_state);

} nimcp_module_state_ops_t;

//=============================================================================
// Module Registration Entry
//=============================================================================

/**
 * @brief Registered module entry
 */
typedef struct nimcp_state_module_entry {
    char name[NIMCP_STATE_MANAGER_MAX_NAME_LEN];  /**< Module name */
    nimcp_module_state_ops_t ops;                  /**< State operations */
    void* context;                                 /**< Module state context */
    bool enabled;                                  /**< Is module enabled */
    uint32_t priority;                             /**< Checkpoint priority (lower = first) */
    uint64_t last_checkpoint_time;                 /**< Last checkpoint timestamp */
    uint64_t last_restore_time;                    /**< Last restore timestamp */
    uint32_t checkpoint_count;                     /**< Number of checkpoints */
    uint32_t restore_count;                        /**< Number of restores */
    uint32_t validation_failures;                  /**< Validation failure count */
} nimcp_state_module_entry_t;

//=============================================================================
// State Manager Structure
//=============================================================================

/**
 * @brief State manager instance
 */
typedef struct nimcp_state_manager {
    uint32_t magic;                                            /**< Validation magic */
    nimcp_state_module_entry_t modules[NIMCP_STATE_MANAGER_MAX_MODULES];
    uint32_t module_count;                                     /**< Number of registered modules */
    bool initialized;                                          /**< Is manager initialized */

    /* Statistics */
    uint64_t total_checkpoints;                                /**< Total checkpoint operations */
    uint64_t total_restores;                                   /**< Total restore operations */
    uint64_t total_validations;                                /**< Total validation operations */
    uint64_t total_resets;                                     /**< Total reset operations */
    uint64_t last_full_checkpoint_time;                        /**< Last full checkpoint */
    size_t last_checkpoint_size;                               /**< Size of last checkpoint */

    /* Thread safety */
    void* mutex;                                               /**< Mutex for thread safety */
} nimcp_state_manager_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create a new state manager
 * @return Pointer to state manager, NULL on failure
 */
nimcp_state_manager_t* nimcp_state_manager_create(void);

/**
 * @brief Destroy state manager
 * @param manager State manager to destroy
 */
void nimcp_state_manager_destroy(nimcp_state_manager_t* manager);

//=============================================================================
// Module Registration API
//=============================================================================

/**
 * @brief Register a module with the state manager
 * @param manager State manager
 * @param name Module name (must be unique)
 * @param ops State operations (copied, can be stack-allocated)
 * @param context Module state context pointer
 * @return 0 on success, negative on error
 */
int nimcp_state_manager_register(
    nimcp_state_manager_t* manager,
    const char* name,
    const nimcp_module_state_ops_t* ops,
    void* context
);

/**
 * @brief Register a module with priority
 * @param manager State manager
 * @param name Module name (must be unique)
 * @param ops State operations
 * @param context Module state context
 * @param priority Checkpoint priority (0 = highest, checkpoint first)
 * @return 0 on success, negative on error
 */
int nimcp_state_manager_register_with_priority(
    nimcp_state_manager_t* manager,
    const char* name,
    const nimcp_module_state_ops_t* ops,
    void* context,
    uint32_t priority
);

/**
 * @brief Unregister a module
 * @param manager State manager
 * @param name Module name to unregister
 * @return 0 on success, negative on error
 */
int nimcp_state_manager_unregister(
    nimcp_state_manager_t* manager,
    const char* name
);

/**
 * @brief Find a registered module by name
 * @param manager State manager
 * @param name Module name
 * @return Pointer to module entry, NULL if not found
 */
nimcp_state_module_entry_t* nimcp_state_manager_find(
    nimcp_state_manager_t* manager,
    const char* name
);

/**
 * @brief Enable/disable a module for checkpointing
 * @param manager State manager
 * @param name Module name
 * @param enabled True to enable, false to disable
 * @return 0 on success, negative on error
 */
int nimcp_state_manager_set_enabled(
    nimcp_state_manager_t* manager,
    const char* name,
    bool enabled
);

//=============================================================================
// Checkpoint API
//=============================================================================

/**
 * @brief Checkpoint all registered modules
 * @param manager State manager
 * @param buffer Output buffer (NULL to query size)
 * @param size In: buffer size, Out: bytes written or required
 * @return 0 on success, negative on error
 */
int nimcp_state_manager_checkpoint_all(
    nimcp_state_manager_t* manager,
    uint8_t* buffer,
    size_t* size
);

/**
 * @brief Checkpoint a specific module
 * @param manager State manager
 * @param name Module name
 * @param buffer Output buffer (NULL to query size)
 * @param size In: buffer size, Out: bytes written or required
 * @return 0 on success, negative on error
 */
int nimcp_state_manager_checkpoint_module(
    nimcp_state_manager_t* manager,
    const char* name,
    uint8_t* buffer,
    size_t* size
);

//=============================================================================
// Restore API
//=============================================================================

/**
 * @brief Restore all modules from checkpoint
 * @param manager State manager
 * @param buffer Input buffer with checkpoint data
 * @param size Size of input buffer
 * @return 0 on success, negative on error
 */
int nimcp_state_manager_restore_all(
    nimcp_state_manager_t* manager,
    const uint8_t* buffer,
    size_t size
);

/**
 * @brief Restore a specific module from checkpoint
 * @param manager State manager
 * @param name Module name
 * @param buffer Input buffer with module state
 * @param size Size of input buffer
 * @return 0 on success, negative on error
 */
int nimcp_state_manager_restore_module(
    nimcp_state_manager_t* manager,
    const char* name,
    const uint8_t* buffer,
    size_t size
);

//=============================================================================
// Validation API
//=============================================================================

/**
 * @brief Validate all module states
 * @param manager State manager
 * @return Number of modules with valid state (negative on error)
 */
int nimcp_state_manager_validate_all(nimcp_state_manager_t* manager);

/**
 * @brief Validate a specific module's state
 * @param manager State manager
 * @param name Module name
 * @return 0 if valid, negative error code if invalid
 */
int nimcp_state_manager_validate_module(
    nimcp_state_manager_t* manager,
    const char* name
);

//=============================================================================
// Reset/Recovery API
//=============================================================================

/**
 * @brief Reset all modules to default state
 * @param manager State manager
 * @return Number of modules reset (negative on error)
 */
int nimcp_state_manager_reset_all(nimcp_state_manager_t* manager);

/**
 * @brief Reset a specific module
 * @param manager State manager
 * @param name Module name
 * @return 0 on success, negative on error
 */
int nimcp_state_manager_reset_module(
    nimcp_state_manager_t* manager,
    const char* name
);

/**
 * @brief Reset modules that failed validation
 * @param manager State manager
 * @return Number of modules reset (negative on error)
 */
int nimcp_state_manager_reset_invalid(nimcp_state_manager_t* manager);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get total estimated checkpoint size
 * @param manager State manager
 * @return Total size in bytes
 */
size_t nimcp_state_manager_get_total_size(nimcp_state_manager_t* manager);

/**
 * @brief Get number of registered modules
 * @param manager State manager
 * @return Number of modules
 */
uint32_t nimcp_state_manager_get_module_count(nimcp_state_manager_t* manager);

/**
 * @brief Get list of registered module names
 * @param manager State manager
 * @param names Output array of name pointers
 * @param max_names Maximum names to return
 * @return Number of names written
 */
uint32_t nimcp_state_manager_get_module_names(
    nimcp_state_manager_t* manager,
    const char** names,
    uint32_t max_names
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief State manager statistics
 */
typedef struct nimcp_state_manager_stats {
    uint32_t module_count;
    uint32_t enabled_modules;
    uint64_t total_checkpoints;
    uint64_t total_restores;
    uint64_t total_validations;
    uint64_t total_resets;
    uint64_t validation_failures;
    size_t total_state_size;
    uint64_t last_checkpoint_time;
} nimcp_state_manager_stats_t;

/**
 * @brief Get state manager statistics
 * @param manager State manager
 * @param stats Output statistics structure
 * @return 0 on success, negative on error
 */
int nimcp_state_manager_get_stats(
    nimcp_state_manager_t* manager,
    nimcp_state_manager_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STATE_MANAGER_H */
