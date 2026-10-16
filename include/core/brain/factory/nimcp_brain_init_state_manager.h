//=============================================================================
// nimcp_brain_init_state_manager.h - State Manager Integration for Brain
//=============================================================================
/**
 * @file nimcp_brain_init_state_manager.h
 * @brief State manager initialization and subsystem registration
 *
 * WHAT: Initialize state manager and register brain subsystems
 * WHY:  Enable checkpointing and recovery for fault tolerance
 * HOW:  Create state ops for each subsystem, register with state manager
 *
 * PHASE 8: System-Wide Health Integration
 *
 * USAGE:
 * ```c
 * // During brain_create()
 * brain_init_state_manager(brain);
 *
 * // Checkpoint brain state
 * size_t size;
 * brain_checkpoint_state(brain, NULL, &size);  // Query size
 * uint8_t* buffer = nimcp_malloc(size);
 * brain_checkpoint_state(brain, buffer, &size);
 *
 * // Restore from checkpoint
 * brain_restore_state(brain, buffer, size);
 *
 * // During brain_destroy()
 * brain_shutdown_state_manager(brain);
 * ```
 *
 * @author NIMCP Team
 * @date 2026-01-22
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_INIT_STATE_MANAGER_H
#define NIMCP_BRAIN_INIT_STATE_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
struct brain_struct;
typedef struct brain_struct* brain_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Initialize state manager for brain
 *
 * Creates a state manager instance and registers brain subsystems:
 * - brain_stats: Brain statistics (priority 10)
 * - working_memory: Active representations (priority 20)
 * - executive: Task management (priority 30)
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool brain_init_state_manager(brain_t brain);

/**
 * @brief Shutdown state manager for brain
 *
 * Destroys the state manager if brain owns it.
 *
 * @param brain Brain instance
 */
void brain_shutdown_state_manager(brain_t brain);

//=============================================================================
// Checkpoint/Restore API
//=============================================================================

/**
 * @brief Checkpoint brain state
 *
 * Serializes all registered module states to a buffer.
 * Call with buffer=NULL to query required size.
 *
 * @param brain Brain instance
 * @param buffer Output buffer (NULL to query size)
 * @param size In: buffer size, Out: bytes written or required
 * @return 0 on success, negative on error
 */
int brain_checkpoint_state(brain_t brain, uint8_t* buffer, size_t* size);

/**
 * @brief Restore brain state from checkpoint
 *
 * Deserializes module states from a buffer.
 *
 * @param brain Brain instance
 * @param buffer Input buffer with checkpoint data
 * @param size Size of input buffer
 * @return 0 on success, negative on error
 */
int brain_restore_state(brain_t brain, const uint8_t* buffer, size_t size);

//=============================================================================
// Validation/Reset API
//=============================================================================

/**
 * @brief Validate all brain module states
 *
 * Checks integrity of all registered module states.
 *
 * @param brain Brain instance
 * @return Number of valid modules, negative on error
 */
int brain_validate_state(brain_t brain);

/**
 * @brief Reset invalid module states
 *
 * Resets modules that failed validation to safe defaults.
 *
 * @param brain Brain instance
 * @return Number of modules reset, negative on error
 */
int brain_reset_invalid_state(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_STATE_MANAGER_H */
