/**
 * @file nimcp_unification_engine.h
 * @brief MODULE 5: Unification Engine - Variable unification for inference
 *
 * SINGLE RESPONSIBILITY: Variable unification and substitution for logical terms
 *
 * @author NIMCP Development Team - SRP Refactoring
 * @date 2025-11-20
 * @version 3.0.0
 */

#ifndef NIMCP_UNIFICATION_ENGINE_H
#define NIMCP_UNIFICATION_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_symbolic_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Event Types - See include/core/events/nimcp_event_bus.h for definitions
// EVENT_UNIFICATION_SUCCEEDED = 0xC005
// EVENT_UNIFICATION_FAILED = 0xC006
//=============================================================================

//=============================================================================
// Unification Operations - SOLE RESPONSIBILITY
//=============================================================================

/**
 * @brief Unify two logical terms
 */
bool brain_unify_terms(
    brain_t brain,
    const logical_term_t* term1,
    const logical_term_t* term2,
    unification_t* result
);

/**
 * @brief Apply substitution to term
 */
bool brain_apply_substitution(
    brain_t brain,
    const logical_term_t* term,
    const unification_t* bindings,
    logical_term_t** result
);

/**
 * @brief Free unification result
 */
void unification_free_result(unification_t* result);

/**
 * @brief Get last error message
 */
const char* unification_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_UNIFICATION_ENGINE_H
