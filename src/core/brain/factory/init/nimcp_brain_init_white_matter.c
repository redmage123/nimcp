//=============================================================================
// nimcp_brain_init_white_matter.c - White Matter Subsystem Init
//=============================================================================
/**
 * @file nimcp_brain_init_white_matter.c
 * @brief White matter tract subsystem initialization for brain factory
 *
 * WHAT: White matter tract system initialization and teardown
 * WHY:  Models myelinated axon bundles for inter-region signal conduction
 *       with biologically-calibrated delays, integrity tracking, and
 *       use-dependent myelination
 * HOW:  Creates wmt_system_t with default config during brain factory init,
 *       stores on brain struct, teardown in reverse order
 *
 * BIOLOGICAL BASIS:
 * White matter tracts are the "highways" of the brain, connecting cortical
 * and subcortical regions via myelinated axon bundles. Conduction velocity
 * depends on myelination level (saltatory conduction), ranging from 1 m/s
 * (unmyelinated C fibers) to 120 m/s (heavily myelinated A-alpha fibers).
 *
 * DESIGN PATTERNS:
 * - Factory: Created by brain factory during lifecycle init
 * - Idempotent: Returns true if already initialized
 * - Non-fatal: Creation failure logs warning and returns true
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-03-05
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_white_matter.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/white_matter/nimcp_white_matter_tracts.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_WHITE_MATTER"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_white_matter, MESH_ADAPTER_CATEGORY_SYSTEM)

//=============================================================================
// Main Initialization Function
//=============================================================================

/**
 * @brief Initialize white matter tract subsystem
 *
 * WHAT: Creates and configures white matter tract system for brain
 * WHY:  Enables inter-region conduction modeling with myelination dynamics
 * HOW:  Guard clause checks, idempotency, create with default config, store
 *
 * @param brain Brain instance to initialize white matter for
 * @return true on success or non-fatal failure, false on critical error
 */
bool nimcp_brain_factory_init_white_matter_subsystem(brain_t brain) {
    /* Guard clause: NULL check */
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_white_matter_subsystem: brain is NULL");
        return false;
    }

    /* Idempotency guard: already initialized */
    if (brain->white_matter) {
        LOG_INFO(LOG_MODULE, "White matter already initialized, skipping");
        return true;
    }

    /* Initialize fields to defaults */
    brain->white_matter = NULL;
    brain->white_matter_enabled = false;

    /* Create white matter system with default config */
    wmt_config_t config = wmt_default_config();

    wmt_system_t* wmt = wmt_create(&config);
    if (!wmt) {
        NIMCP_LOGGING_WARN("Failed to create white matter tract system - "
                          "continuing without white matter modeling");
        return true;  /* Non-fatal */
    }

    /* Store on brain */
    brain->white_matter = wmt;
    brain->white_matter_enabled = true;

    NIMCP_LOGGING_INFO("White matter tract system initialized: "
                      "8 tracts, myelination=%.2f, integrity=%.2f",
                      config.base_myelination, config.base_integrity);

    return true;
}

//=============================================================================
// Destroy Function
//=============================================================================

/**
 * @brief Destroy white matter tract subsystem
 *
 * WHAT: Cleans up white matter resources
 * WHY:  Proper resource management during brain teardown
 * HOW:  Calls wmt_destroy, NULLs pointer, clears enabled flag
 *
 * @param brain The brain containing the white matter subsystem
 */
void nimcp_brain_factory_destroy_white_matter_subsystem(brain_t brain) {
    if (!brain) {
        return;
    }

    if (brain->white_matter) {
        wmt_destroy(brain->white_matter);
        brain->white_matter = NULL;
    }

    brain->white_matter_enabled = false;

    NIMCP_LOGGING_DEBUG("White matter tract subsystem destroyed");
}
