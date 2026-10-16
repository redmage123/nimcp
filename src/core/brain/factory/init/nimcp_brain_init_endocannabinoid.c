/**
 * @file nimcp_brain_init_endocannabinoid.c
 * @brief Factory initialization for the Endocannabinoid System (ECS)
 * @date 2026-03-05
 *
 * WHAT: Initialization and teardown of the ECS during brain factory lifecycle
 * WHY:  Enable retrograde synaptic modulation, pain gating, and appetite regulation
 * HOW:  Creates ECS with default config, stores on brain->endocannabinoid field
 *
 * BIOLOGICAL MOTIVATION:
 * - The endocannabinoid system is a ubiquitous neuromodulatory system
 * - 2-AG provides activity-dependent retrograde suppression (DSI/DSE)
 * - Anandamide provides tonic presynaptic inhibition
 * - CB1 receptors are the most abundant GPCR in the brain
 * - CB2 receptors on microglia mediate neuroinflammatory control
 */

#include "core/brain/factory/init/nimcp_brain_init_endocannabinoid.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/regions/endocannabinoid/nimcp_endocannabinoid.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_ENDOCANNABINOID"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_endocannabinoid, MESH_ADAPTER_CATEGORY_SYSTEM)

/* Compatibility macro for set_error (converts to LOG_ERROR) */
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

bool nimcp_brain_factory_init_endocannabinoid_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_endocannabinoid_subsystem: brain is NULL");
        return false;
    }

    /* Idempotent: already initialized */
    if (brain->endocannabinoid) {
        return true;
    }

    /* Create ECS with default configuration */
    ecb_config_t config = ecb_default_config();

    brain->endocannabinoid = ecb_create(&config);
    if (!brain->endocannabinoid) {
        set_error("Failed to create endocannabinoid system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_brain_factory_init_endocannabinoid_subsystem: ecb_create returned NULL");
        return false;
    }

    brain->endocannabinoid_enabled = true;

    LOG_INFO(LOG_MODULE, "Endocannabinoid system initialized (2-AG=%.2f, AEA=%.2f)",
             config.base_two_ag, config.base_aea);

    return true;
}

void nimcp_brain_factory_destroy_endocannabinoid_subsystem(brain_t brain)
{
    if (!brain) {
        return;
    }

    if (brain->endocannabinoid) {
        ecb_destroy(brain->endocannabinoid);
        brain->endocannabinoid = NULL;
        brain->endocannabinoid_enabled = false;
        LOG_INFO(LOG_MODULE, "Endocannabinoid system destroyed");
    }
}
