//=============================================================================
// nimcp_brain_init_pr_memory.h - Prime Resonant Memory Subsystem Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_pr_memory.h
 * @brief Header for Prime Resonant Memory subsystem initialization
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Brain factory initialization function for PR memory
 * WHY:  Integrate PR memory system into brain lifecycle
 * HOW:  Called by brain_create_custom() during subsystem initialization
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_INIT_PR_MEMORY_H
#define NIMCP_BRAIN_INIT_PR_MEMORY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

struct brain_struct;

//=============================================================================
// Subsystem Initialization Function
//=============================================================================

/**
 * @brief Initialize Prime Resonant memory subsystem for a brain
 *
 * WHAT: Creates and configures Z-Ladder, theta-gamma, and entanglement
 * WHY:  Enables biologically-inspired memory consolidation and retrieval
 * HOW:  Called by brain factory during brain creation
 *
 * Components initialized:
 * - Z-Ladder: Four-tier memory consolidation (Z0-working to Z3-permanent)
 * - Theta-Gamma: Phase-amplitude coupling for encoding/retrieval windows
 * - Entanglement: Associative memory graph with resonance-weighted edges
 *
 * COMPLEXITY: O(1) for initialization (component creation is O(capacity))
 *
 * @param brain Brain structure to initialize PR memory for
 * @return true on success, false on failure
 *
 * @note Called by brain_create_custom() if config.enable_pr_memory is true
 * @note Respects config.lazy_pr_memory_init flag
 */
bool nimcp_brain_factory_init_pr_memory_subsystem(struct brain_struct* brain);

/**
 * @brief Destroy Prime Resonant memory subsystem for a brain
 *
 * WHAT: Releases all PR memory resources
 * WHY:  Cleanup during brain destruction
 * HOW:  Called by brain_destroy()
 *
 * @param brain Brain structure to cleanup PR memory for
 *
 * @note Called by brain_destroy() during brain cleanup
 * @note Safe to call on uninitialized or already-destroyed brain
 */
void nimcp_brain_factory_destroy_pr_memory_subsystem(struct brain_struct* brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_PR_MEMORY_H
