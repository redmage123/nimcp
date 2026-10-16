//=============================================================================
// nimcp_brain_init_pr_memory.c - Prime Resonant Memory Subsystem Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_pr_memory.c
 * @brief Implementation of Prime Resonant memory subsystem initialization
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Brain factory initialization function for PR memory
 * WHY:  Integrate PR memory system into brain lifecycle
 * HOW:  Delegates to nimcp_brain_pr_memory.c for actual initialization
 *
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_pr_memory.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/internal/nimcp_brain_pr_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_PR_MEMORY"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_pr_memory, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Subsystem Initialization
//=============================================================================

bool nimcp_brain_factory_init_pr_memory_subsystem(struct brain_struct* brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_pr_memory_subsystem: brain is NULL");
        return false;
    }

    /* Check if PR memory is enabled in config */
    if (!brain->config.enable_pr_memory) {
        LOG_DEBUG(LOG_MODULE, "PR memory disabled in config - skipping initialization");
        brain->pr_memory_enabled = false;
        return true;  /* Not an error - just disabled */
    }

    /* Check for lazy initialization */
    if (brain->config.lazy_pr_memory_init) {
        LOG_DEBUG(LOG_MODULE, "PR memory set to lazy initialization - deferring");
        brain->pr_lazy_init = true;
        brain->pr_memory_enabled = false;  /* Will be enabled on first use */
        return true;
    }

    LOG_INFO(LOG_MODULE, "Initializing Prime Resonant memory subsystem...");

    /* Delegate to the main PR memory initialization function */
    if (!nimcp_brain_pr_memory_init(brain, NULL)) {
        LOG_ERROR(LOG_MODULE, "Failed to initialize PR memory subsystem");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_factory_init_pr_memory_subsystem: nimcp_brain_pr_memory_init is NULL");
        return false;
    }

    LOG_INFO(LOG_MODULE, "PR memory subsystem initialized successfully");
    LOG_DEBUG(LOG_MODULE, "  - Z-Ladder: 4-tier consolidation (Z0-Z3)");
    LOG_DEBUG(LOG_MODULE, "  - Theta-Gamma: Phase-gated encoding/retrieval");
    LOG_DEBUG(LOG_MODULE, "  - Entanglement: Associative memory graph");

    return true;
}

void nimcp_brain_factory_destroy_pr_memory_subsystem(struct brain_struct* brain) {
    if (!brain) {
        return;
    }

    /* Only destroy if PR memory was initialized */
    if (brain->pr_memory_enabled || brain->pr_z_ladder ||
        brain->pr_theta_gamma || brain->pr_entanglement) {
        LOG_DEBUG(LOG_MODULE, "Destroying PR memory subsystem...");
        nimcp_brain_pr_memory_destroy(brain);
        LOG_DEBUG(LOG_MODULE, "PR memory subsystem destroyed");
    }
}
