/**
 * @file nimcp_neural_logic_attachment.c
 * @brief MODULE 1: Neural Logic Attachment Implementation
 * @version 3.0.0
 * @date 2025-11-20
 *
 * WHAT: Attachment layer for connecting neural logic networks to brain instances
 * WHY:  Single Responsibility: Manage lifecycle of brain-logic relationships
 * HOW:  Attach, detach, and query operations with strict NULL-safety
 *
 * @author NIMCP Development Team
 */

#include "core/logic/nimcp_neural_logic_attachment.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "core/brain/nimcp_brain_internal.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"

#include <stddef.h>  /* for NULL */
// === BIO-ASYNC + LOGGING + UNIFIED MEMORY INTEGRATION ===
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "neural_logic_attachment"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neural_logic_attachment)

#define BIO_MODULE_ID 0x0135


//=============================================================================
// MODULE 1: Neural Logic Attachment Implementation
//=============================================================================

bool brain_attach_neural_logic(
    brain_t brain,
    neural_logic_network_t network
) {
    // WHAT: Validate inputs before attachment
    // WHY:  Prevent NULL pointer derefs and double-attachment
    // HOW:  Guard clauses with early returns

    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("brain_attach_neural_logic: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_attach_neural_logic: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: NULL network
    if (!nimcp_validate_pointer(network, "network")) {
        LOG_ERROR("brain_attach_neural_logic: NULL network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_attach_neural_logic: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: network already attached
    if (brain->logic != NULL) {
        LOG_WARNING("brain_attach_neural_logic: brain already has logic network attached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_attach_neural_logic: validation failed");
        return false;
    }

    // WHAT: Establish bidirectional brain ↔ logic link
    // WHY:  Enable brain to access network AND network to read brain neuromodulators
    // HOW:  Store in brain->logic, call neural_logic_set_brain()

    // Store network in brain
    brain->logic = network;

    // Set brain reference in network for neuromodulation
    neural_logic_set_brain(network, brain);

    LOG_INFO("brain_attach_neural_logic: attached logic network to brain");

    return true;
}

neural_logic_network_t brain_detach_neural_logic(brain_t brain) {
    // WHAT: Safely detach network with NULL checks
    // WHY:  Clean up references during shutdown
    // HOW:  Clear both sides of bidirectional link

    // Guard: NULL brain (NULL-safe)
    if (brain == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    // Guard: no network attached
    if (brain->logic == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_detach_neural_logic: validation failed");
        return NULL;
    }

    // WHAT: Clear bidirectional link and return ownership
    // WHY:  Caller may want to reuse network or destroy it
    // HOW:  Save pointer, clear references, return saved pointer

    // Save network pointer
    neural_logic_network_t network = brain->logic;

    // Clear brain's reference
    brain->logic = NULL;

    // Clear network's brain reference
    neural_logic_set_brain(network, NULL);

    LOG_INFO("brain_detach_neural_logic: detached logic network from brain");

    return network;
}

neural_logic_network_t brain_get_neural_logic(brain_t brain) {
    // WHAT: Return non-owning reference to attached network
    // WHY:  Allow inspection without ownership transfer
    // HOW:  Simple pointer return with NULL check

    // Guard: NULL brain (NULL-safe)
    if (brain == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    return brain->logic;
}

bool brain_has_neural_logic(brain_t brain) {
    // WHAT: Boolean test for logic network presence
    // WHY:  Convenient check before evaluation operations
    // HOW:  Test both brain and brain->logic non-NULL

    return (brain != NULL) && (brain->logic != NULL);
}
