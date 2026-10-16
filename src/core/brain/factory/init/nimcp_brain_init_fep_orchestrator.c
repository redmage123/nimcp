//=============================================================================
// nimcp_brain_init_fep_orchestrator.c - FEP Orchestrator Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_fep_orchestrator.c
 * @brief FEP Orchestrator subsystem initialization for brain
 *
 * WHAT: FEP Orchestrator initialization and bridge registration
 * WHY:  Centralizes coordination of all 93+ FEP bridges
 * HOW:  Creates orchestrator, connects integrations, registers bridges
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_FEP_ORCH"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_fep_orchestrator, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Bridge Registration Helpers
//=============================================================================

/**
 * @brief Register cognitive FEP bridges with orchestrator
 */
static void register_cognitive_bridges(brain_t brain, fep_orchestrator_t* orch) {
    /* Cognitive bridges are registered when their subsystems are active */
    /* The orchestrator will track them via the bridge registry */

    /* Note: Individual bridges should call fep_orchestrator_register_bridge()
     * during their own initialization when they connect to the FEP system.
     * This function provides a central location to verify/force registration
     * for bridges that may not self-register. */

    NIMCP_LOGGING_DEBUG("Cognitive FEP bridges registration point ready");
}

/**
 * @brief Register swarm FEP bridges with orchestrator
 */
static void register_swarm_bridges(brain_t brain, fep_orchestrator_t* orch) {
    NIMCP_LOGGING_DEBUG("Swarm FEP bridges registration point ready");
}

/**
 * @brief Register security FEP bridges with orchestrator
 */
static void register_security_bridges(brain_t brain, fep_orchestrator_t* orch) {
    NIMCP_LOGGING_DEBUG("Security FEP bridges registration point ready");
}

/**
 * @brief Register plasticity FEP bridges with orchestrator
 */
static void register_plasticity_bridges(brain_t brain, fep_orchestrator_t* orch) {
    NIMCP_LOGGING_DEBUG("Plasticity FEP bridges registration point ready");
}

/**
 * @brief Register middleware FEP bridges with orchestrator
 */
static void register_middleware_bridges(brain_t brain, fep_orchestrator_t* orch) {
    NIMCP_LOGGING_DEBUG("Middleware FEP bridges registration point ready");
}

/**
 * @brief Register perception FEP bridges with orchestrator
 */
static void register_perception_bridges(brain_t brain, fep_orchestrator_t* orch) {
    NIMCP_LOGGING_DEBUG("Perception FEP bridges registration point ready");
}

/**
 * @brief Register glial FEP bridges with orchestrator
 */
static void register_glial_bridges(brain_t brain, fep_orchestrator_t* orch) {
    NIMCP_LOGGING_DEBUG("Glial FEP bridges registration point ready");
}

/**
 * @brief Register async FEP bridges with orchestrator
 */
static void register_async_bridges(brain_t brain, fep_orchestrator_t* orch) {
    NIMCP_LOGGING_DEBUG("Async FEP bridges registration point ready");
}

/**
 * @brief Register core FEP bridges with orchestrator
 */
static void register_core_bridges(brain_t brain, fep_orchestrator_t* orch) {
    NIMCP_LOGGING_DEBUG("Core FEP bridges registration point ready");
}

//=============================================================================
// Main Initialization Function
//=============================================================================

bool nimcp_brain_factory_init_fep_orchestrator_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_fep_orchestrator_subsystem: brain is NULL");
        return false;
    }

    /* Initialize FEP orchestrator fields to defaults */
    brain->fep_orchestrator = NULL;
    brain->fep_orchestrator_enabled = false;

    /* Check if FEP orchestrator should be enabled */
    /* For now, enable by default if immune system is enabled */
    bool should_enable = brain->immune_enabled || brain->bio_async_enabled;

    if (!should_enable) {
        NIMCP_LOGGING_DEBUG("FEP orchestrator not enabled (no immune or bio-async)");
        return true;  /* Not an error - just not needed */
    }

    /* Create orchestrator configuration */
    fep_orchestrator_config_t orch_config;
    fep_orchestrator_default_config(&orch_config);

    /* Configure based on brain settings */
    orch_config.enable_bio_async = brain->bio_async_enabled;
    orch_config.enable_brain_immune = brain->immune_enabled;
    orch_config.enable_logging = true;
    orch_config.enable_statistics = true;

    /* Create orchestrator */
    fep_orchestrator_t* orch = fep_orchestrator_create(&orch_config);
    if (!orch) {
        NIMCP_LOGGING_WARN("Failed to create FEP orchestrator - continuing without FEP coordination");
        return true;  /* Non-fatal */
    }

    /* Connect to brain immune system if available */
    if (brain->immune_enabled && brain->immune_system) {
        if (fep_orchestrator_connect_brain_immune(orch, brain->immune_system) == 0) {
            NIMCP_LOGGING_DEBUG("FEP orchestrator connected to brain immune system");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect FEP orchestrator to immune system");
        }
    }

    /* Connect to bio-async router if available */
    if (brain->bio_async_enabled) {
        if (fep_orchestrator_connect_bio_async(orch) == 0) {
            NIMCP_LOGGING_DEBUG("FEP orchestrator connected to bio-async router");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect FEP orchestrator to bio-async");
        }
    }

    /* Register FEP bridges by category */
    register_cognitive_bridges(brain, orch);
    register_swarm_bridges(brain, orch);
    register_security_bridges(brain, orch);
    register_plasticity_bridges(brain, orch);
    register_middleware_bridges(brain, orch);
    register_perception_bridges(brain, orch);
    register_glial_bridges(brain, orch);
    register_async_bridges(brain, orch);
    register_core_bridges(brain, orch);

    /* Register physics-informed bridges (HNN, FNO Audio, FNO Population) */
    {
        extern int fep_register_physics_bridges(void*, void*, void*, void*);
        int physics_count = fep_register_physics_bridges(
            orch,
            brain->lnn_network,       /* HNN — Hamiltonian energy as free energy */
            brain->cortex_cnns[1],    /* FNO Audio cortex — spectral prediction */
            brain->snn_network        /* FNO Population — collective dynamics */
        );
        if (physics_count > 0) {
            NIMCP_LOGGING_INFO("Physics-informed FEP bridges: %d registered "
                             "(HNN=%s, FNO_Audio=%s, FNO_Pop=%s)",
                             physics_count,
                             brain->lnn_network ? "yes" : "no",
                             brain->cortex_cnns[1] ? "yes" : "no",
                             brain->snn_network ? "yes" : "no");
        }
    }

    /* Start orchestrator */
    if (fep_orchestrator_start(orch) != 0) {
        NIMCP_LOGGING_WARN("Failed to start FEP orchestrator");
        fep_orchestrator_destroy(orch);
        return true;  /* Non-fatal */
    }

    /* Store orchestrator in brain structure */
    brain->fep_orchestrator = orch;
    brain->fep_orchestrator_enabled = true;

    /* Log summary */
    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(orch, &stats);

    NIMCP_LOGGING_INFO("FEP orchestrator initialized: bridges=%u, categories=%d, "
                       "immune=%s, bio_async=%s",
                       stats.total_bridges,
                       FEP_BRIDGE_CATEGORY_COUNT,
                       brain->immune_enabled ? "connected" : "disabled",
                       brain->bio_async_enabled ? "connected" : "disabled");

    return true;
}
