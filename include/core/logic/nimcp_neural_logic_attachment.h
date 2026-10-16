/**
 * @file nimcp_neural_logic_attachment.h
 * @brief MODULE 1: Neural Logic Attachment - Attach/Detach Neural Logic Networks
 * @version 3.0.0
 * @date 2025-11-20
 *
 * WHAT: Attachment layer for connecting neural logic networks to brain instances
 * WHY:  Single Responsibility: Manage lifecycle of brain-logic relationships
 * HOW:  Attach, detach, and query operations with strict NULL-safety
 *
 * SINGLE RESPONSIBILITY PRINCIPLE (SRP):
 * - SOLE RESPONSIBILITY: Manage neural logic network attachment to brains
 * - DOES: Attach networks, detach networks, query attachment status
 * - DOES NOT: Create networks (MODULE 5), evaluate gates (MODULE 2), build circuits (MODULE 3)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEURAL_LOGIC_ATTACHMENT_H
#define NIMCP_NEURAL_LOGIC_ATTACHMENT_H

#include <stdint.h>
#include <stdbool.h>
#include "core/neuron_types/nimcp_neural_logic.h"
#include "core/brain/nimcp_brain.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// MODULE 1: Neural Logic Attachment API
//=============================================================================

/**
 * @brief Attach neural logic network to brain
 *
 * WHAT: Associate existing neural logic network with brain instance
 * WHY:  Enable brain to access logic network for reasoning operations
 * HOW:  Store network handle in brain->logic, set bidirectional reference
 *
 * @param brain Brain instance (must be non-NULL)
 * @param network Neural logic network (must be non-NULL, created externally)
 * @return true on success, false on failure
 *
 * GUARD CLAUSES:
 * - NULL brain → false + error log
 * - NULL network → false + error log
 * - brain->logic already set → false + warning log
 *
 * BEHAVIOR:
 * - Sets brain->logic = network
 * - Calls neural_logic_set_brain(network, brain) for bidirectional link
 * - Logs successful attachment with brain name
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Not thread-safe, call during brain initialization
 *
 * EXAMPLE:
 * ```c
 * brain_t brain = brain_create("reasoner", BRAIN_SIZE_SMALL);
 * neural_logic_network_t net = neural_logic_create(&config);
 *
 * if (brain_attach_neural_logic(brain, net)) {
 *     // Brain can now evaluate logic gates
 * }
 * ```
 */
NIMCP_EXPORT bool brain_attach_neural_logic(
    brain_t brain,
    neural_logic_network_t network
);

/**
 * @brief Detach neural logic network from brain
 *
 * WHAT: Remove association between brain and neural logic network
 * WHY:  Clean up references during brain shutdown or logic network replacement
 * HOW:  Clear brain->logic pointer, clear network's brain reference
 *
 * @param brain Brain instance (NULL-safe)
 * @return Detached network handle, or NULL if no network was attached
 *
 * GUARD CLAUSES:
 * - NULL brain → NULL + silent return (NULL-safe)
 * - brain->logic already NULL → NULL + silent return
 *
 * BEHAVIOR:
 * - Retrieves network from brain->logic
 * - Calls neural_logic_set_brain(network, NULL) to clear bidirectional link
 * - Sets brain->logic = NULL
 * - Returns detached network (caller responsible for destruction)
 * - Logs detachment event
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Not thread-safe
 *
 * OWNERSHIP:
 * - Returns ownership of network to caller
 * - Caller must call neural_logic_destroy() to prevent memory leak
 *
 * EXAMPLE:
 * ```c
 * neural_logic_network_t net = brain_detach_neural_logic(brain);
 * if (net) {
 *     neural_logic_destroy(net);  // Caller's responsibility
 * }
 * ```
 */
NIMCP_EXPORT neural_logic_network_t brain_detach_neural_logic(brain_t brain);

/**
 * @brief Get attached neural logic network
 *
 * WHAT: Query currently attached neural logic network
 * WHY:  Allow inspection and validation of brain's logic capabilities
 * HOW:  Return brain->logic pointer (non-owning reference)
 *
 * @param brain Brain instance (NULL-safe)
 * @return Neural logic network handle, or NULL if none attached
 *
 * GUARD CLAUSES:
 * - NULL brain → NULL + silent return (NULL-safe)
 *
 * BEHAVIOR:
 * - Returns brain->logic (non-owning reference)
 * - Does NOT transfer ownership
 * - Safe to call multiple times
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe for read-only access
 *
 * OWNERSHIP:
 * - Returns non-owning reference
 * - Caller must NOT destroy returned network
 *
 * EXAMPLE:
 * ```c
 * neural_logic_network_t net = brain_get_neural_logic(brain);
 * if (net) {
 *     // Network is attached, can evaluate gates
 *     uint32_t gate_count;
 *     neural_logic_get_stats(net, &gate_count, ...);
 * }
 * ```
 */
NIMCP_EXPORT neural_logic_network_t brain_get_neural_logic(brain_t brain);

/**
 * @brief Check if brain has attached neural logic network
 *
 * WHAT: Test whether brain has logic capabilities
 * WHY:  Provide convenient boolean check for logic availability
 * HOW:  Test brain->logic != NULL
 *
 * @param brain Brain instance (NULL-safe)
 * @return true if network attached, false otherwise
 *
 * GUARD CLAUSES:
 * - NULL brain → false (NULL-safe)
 *
 * BEHAVIOR:
 * - Returns (brain != NULL && brain->logic != NULL)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe for read-only access
 *
 * EXAMPLE:
 * ```c
 * if (brain_has_neural_logic(brain)) {
 *     brain_neural_logic_evaluate(brain, gate_id, inputs, 2, &output);
 * } else {
 *     LOG_ERROR("Brain lacks logic network");
 * }
 * ```
 */
NIMCP_EXPORT bool brain_has_neural_logic(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURAL_LOGIC_ATTACHMENT_H
