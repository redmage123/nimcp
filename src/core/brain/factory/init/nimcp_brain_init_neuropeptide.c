/**
 * @file nimcp_brain_init_neuropeptide.c
 * @brief Factory initialization for Neuropeptide Subsystem
 * @version 1.0.0
 * @date 2026-03-05
 *
 * WHAT: Creates and wires the neuropeptide system into the brain
 * WHY:  Neuropeptides provide slow neuromodulation for behavioral drives
 * HOW:  Allocate npt_create(), store on brain, idempotent guard
 *
 * EXTRACTED FROM: standalone module (new subsystem)
 *
 * @author NIMCP Development Team
 */

/*=============================================================================
 * Includes
 *===========================================================================*/

#include "core/brain/factory/init/nimcp_brain_init_neuropeptide.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/regions/neuropeptide/nimcp_neuropeptide.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_NEUROPEPTIDE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_neuropeptide, MESH_ADAPTER_CATEGORY_SYSTEM)

/* Compatibility macro for set_error */
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

/*=============================================================================
 * Init
 *===========================================================================*/

bool nimcp_brain_factory_init_neuropeptide_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_neuropeptide_subsystem: brain is NULL");
        return false;
    }

    /* Idempotency: already initialized */
    if (brain->neuropeptide) {
        return true;
    }

    /* Create with default config */
    npt_config_t config = npt_default_config();

    neuropeptide_system_t* npt = npt_create(&config);
    if (!npt) {
        set_error("Failed to create neuropeptide system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_brain_factory_init_neuropeptide_subsystem: npt_create returned NULL");
        return false;
    }

    brain->neuropeptide = npt;
    brain->neuropeptide_enabled = true;

    LOG_INFO(LOG_MODULE, "Neuropeptide subsystem initialized (%d peptides)", NPT_COUNT);
    return true;
}

/*=============================================================================
 * Destroy
 *===========================================================================*/

void nimcp_brain_factory_destroy_neuropeptide_subsystem(brain_t brain) {
    if (!brain) {
        return;
    }

    if (brain->neuropeptide) {
        npt_destroy(brain->neuropeptide);
        brain->neuropeptide = NULL;
        brain->neuropeptide_enabled = false;
        LOG_INFO(LOG_MODULE, "Neuropeptide subsystem destroyed");
    }
}
