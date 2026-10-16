//=============================================================================
// nimcp_brain_init_validation.h - BBB Global System Management
//=============================================================================
/**
 * @file nimcp_brain_init_validation.h
 * @brief Blood-Brain Barrier global system management
 *
 * WHAT: Global BBB system access and lifecycle management
 * WHY:  Provides shared BBB instance across all brain instances
 * HOW:  Thread-safe singleton with reference counting
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#ifndef NIMCP_BRAIN_INIT_VALIDATION_H
#define NIMCP_BRAIN_INIT_VALIDATION_H

#include "security/nimcp_blood_brain_barrier.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get or create the global BBB system (thread-safe)
 *
 * WHAT: Gets existing or creates new global BBB system with reference counting
 * WHY:  Provides shared BBB instance for all brain initialization
 * HOW:  Thread-safe singleton with lazy initialization and refcount increment
 *
 * NOTE: Increments reference count - must call nimcp_bbb_release_global_system() to release
 *
 * @return BBB system handle, or NULL on failure
 */
bbb_system_t get_global_bbb_system(void);

/**
 * @brief Release reference to global BBB system (thread-safe)
 *
 * WHAT: Decrements refcount and destroys BBB when no references remain
 * WHY:  Ensures proper cleanup when brains are destroyed
 * HOW:  Thread-safe decrement with mutex protection
 *
 * NOTE: Called by brain_destroy() to release BBB reference
 */
void nimcp_bbb_release_global_system(void);

/**
 * @brief Get the global BBB system for cross-module protection (thread-safe)
 *
 * WHAT: Returns the shared BBB system instance without incrementing refcount
 * WHY:  Enables all modules to use consistent perimeter security for validation
 * HOW:  Thread-safe read access to global BBB system pointer
 *
 * NOTE: This function does NOT increment the reference count. The returned
 *       handle is valid as long as at least one brain with BBB enabled exists.
 *       For standalone usage, call this after brain_create() with BBB enabled.
 *
 * @return BBB system handle, or NULL if not initialized
 */
bbb_system_t nimcp_bbb_get_global_system(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_VALIDATION_H
