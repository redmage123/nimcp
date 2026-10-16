/**
 * @file nimcp_symbolic_logic_attachment.h
 * @brief MODULE 1: Symbolic Logic Attachment - Brain-to-engine connection management
 *
 * SINGLE RESPONSIBILITY: Attach/detach symbolic logic engines to/from brains
 *
 * WHAT: Lifecycle management for brain's symbolic logic engine
 * WHY:  Centralize attach/detach logic in one module following SRP
 * HOW:  Direct pointer manipulation with validation
 *
 * SRP ADHERENCE:
 * - ONLY handles attachment/detachment operations
 * - Does NOT manage knowledge base content
 * - Does NOT perform inference operations
 * - Does NOT create engines (see reasoning_factory.h)
 *
 * INTEGRATION POINTS:
 * - brain->logic_engine pointer (src/core/brain/nimcp_brain_internal.h)
 * - Event publishing: EVENT_LOGIC_ENGINE_ATTACHED, EVENT_LOGIC_ENGINE_DETACHED
 *
 * @author NIMCP Development Team - SRP Refactoring
 * @date 2025-11-20
 * @version 3.0.0
 */

#ifndef NIMCP_SYMBOLIC_LOGIC_ATTACHMENT_H
#define NIMCP_SYMBOLIC_LOGIC_ATTACHMENT_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_symbolic_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Attachment API - SOLE RESPONSIBILITY
//=============================================================================

/**
 * @brief Attach symbolic logic engine to brain
 *
 * WHAT: Connect existing logic engine to brain
 * WHY:  Enable symbolic reasoning for this brain
 * HOW:  Set brain->logic_engine pointer, publish event
 *
 * PRECONDITIONS:
 * - brain is non-NULL
 * - logic_engine is non-NULL
 * - brain->logic_engine is currently NULL (not already attached)
 *
 * EFFECTS:
 * - Sets brain->logic_engine = logic_engine
 * - Publishes EVENT_LOGIC_ENGINE_ATTACHED
 * - Updates brain statistics
 *
 * @param brain Brain instance (non-NULL)
 * @param logic_engine Symbolic logic engine to attach (non-NULL)
 * @return true on success, false on failure
 *
 * ERROR CONDITIONS:
 * - brain is NULL → return false
 * - logic_engine is NULL → return false
 * - brain->logic_engine already exists → return false (already attached)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must ensure exclusive access)
 * MALLOC: No
 *
 * EXAMPLE:
 * ```c
 * brain_t brain = brain_create("reasoner", BRAIN_SIZE_SMALL);
 * symbolic_logic_t* engine = symbolic_logic_create(&config);
 * brain_attach_symbolic_logic(brain, engine);
 * ```
 */
bool brain_attach_symbolic_logic(
    brain_t brain,
    symbolic_logic_t* logic_engine
);

/**
 * @brief Detach symbolic logic engine from brain
 *
 * WHAT: Disconnect logic engine from brain (does NOT destroy engine)
 * WHY:  Allow engine to be moved to another brain or destroyed separately
 * HOW:  Clear brain->logic_engine pointer, publish event
 *
 * PRECONDITIONS:
 * - brain is non-NULL
 *
 * EFFECTS:
 * - Sets brain->logic_engine = NULL
 * - Publishes EVENT_LOGIC_ENGINE_DETACHED
 * - Updates brain statistics
 * - Does NOT free the engine (caller's responsibility)
 *
 * @param brain Brain instance
 * @return Detached logic engine (or NULL if none attached)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 * MALLOC: No
 *
 * NOTE: Caller must destroy returned engine with symbolic_logic_destroy()
 *
 * EXAMPLE:
 * ```c
 * symbolic_logic_t* engine = brain_detach_symbolic_logic(brain);
 * if (engine) {
 *     symbolic_logic_destroy(engine);
 * }
 * ```
 */
symbolic_logic_t* brain_detach_symbolic_logic(brain_t brain);

/**
 * @brief Get attached symbolic logic engine from brain
 *
 * WHAT: Retrieve pointer to brain's current logic engine
 * WHY:  Allow inspection/access without detaching
 * HOW:  Return brain->logic_engine pointer
 *
 * PRECONDITIONS:
 * - brain is non-NULL
 *
 * @param brain Brain instance
 * @return Attached logic engine (or NULL if none attached)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 * MALLOC: No
 *
 * EXAMPLE:
 * ```c
 * symbolic_logic_t* engine = brain_get_symbolic_logic(brain);
 * if (engine) {
 *     logic_stats_t stats;
 *     symbolic_logic_get_stats(engine, &stats);
 * }
 * ```
 */
symbolic_logic_t* brain_get_symbolic_logic(brain_t brain);

/**
 * @brief Check if brain has symbolic logic engine attached
 *
 * WHAT: Query attachment status
 * WHY:  Safe check before operations requiring logic engine
 * HOW:  Return brain->logic_engine != NULL
 *
 * @param brain Brain instance
 * @return true if logic engine attached, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 * MALLOC: No
 *
 * EXAMPLE:
 * ```c
 * if (brain_has_symbolic_logic(brain)) {
 *     brain_add_logical_fact(brain, "Bird(tweety)", 0.9f);
 * }
 * ```
 */
bool brain_has_symbolic_logic(brain_t brain);

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Get last error message from attachment operations
 *
 * @return Error message string (thread-local storage)
 */
const char* brain_logic_attachment_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SYMBOLIC_LOGIC_ATTACHMENT_H
