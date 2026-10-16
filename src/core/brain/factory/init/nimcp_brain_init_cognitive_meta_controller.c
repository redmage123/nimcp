//=============================================================================
// nimcp_brain_init_cognitive_meta_controller.c - Cognitive Meta-Controller Init
//=============================================================================
/**
 * @file nimcp_brain_init_cognitive_meta_controller.c
 * @brief Cognitive Meta-Controller subsystem initialization for brain
 *
 * WHAT: Cognitive Meta-Controller initialization and cognitive subsystem wiring
 * WHY:  High-level arbitrator for cognitive subsystem resources
 * HOW:  Creates controller, connects cognitive systems, configures arbitration
 *
 * BIOLOGICAL BASIS:
 * Models the prefrontal cortex's role in:
 * - Executive control: Coordinating cognitive resources
 * - Working memory management: Capacity allocation
 * - Goal-directed behavior: Prioritizing cognitive processes
 * - Conflict resolution: Mediating between competing cognitive demands
 *
 * DESIGN PATTERNS:
 * - Factory: Created by brain factory during lifecycle init
 * - Coordinator: Arbitrates between cognitive subsystems
 * - Strategy: Configurable resource allocation strategies
 * - Observer: Monitors cognitive load and adapts allocation
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
#include "cognitive/nimcp_cognitive_meta_controller.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_COGNITIVE_META"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_cognitive_meta_controller, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Main Initialization Function
//=============================================================================

/**
 * @brief Initialize cognitive meta-controller subsystem
 *
 * WHAT: Creates and configures cognitive meta-controller for brain
 * WHY:  Provides high-level coordination of cognitive resources
 * HOW:  Guard clause checks, create controller, wire cognitive subsystems
 *
 * @param brain Brain instance to initialize controller for
 * @return true on success or non-fatal failure, false on critical error
 */
bool nimcp_brain_factory_init_cognitive_meta_controller_subsystem(brain_t brain) {
    /* Guard clause: NULL check */
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_cognitive_meta_controller_subsystem: brain is NULL");
        return false;
    }

    /* Initialize fields to defaults */
    brain->cognitive_meta_controller = NULL;
    brain->cognitive_meta_controller_enabled = false;

    /* Check if cognitive subsystems are present */
    bool has_cognitive = brain->working_memory || brain->executive || brain->global_workspace;

    if (!has_cognitive) {
        NIMCP_LOGGING_DEBUG("Cognitive meta-controller skipped (no cognitive subsystems)");
        return true;
    }

    /* Create controller configuration */
    meta_controller_config_t config;
    meta_controller_default_config(&config);

    /* Configure based on brain settings */
    config.enable_bio_async = brain->bio_async_enabled;
    config.enable_brain_immune = brain->immune_enabled;
    config.enable_performance_tracking = true;
    config.enable_uncertainty_modulation = true;

    /* Create controller */
    cognitive_meta_controller_t* ctrl = meta_controller_create(&config);
    if (!ctrl) {
        NIMCP_LOGGING_WARN("Failed to create cognitive meta-controller - "
                          "continuing without cognitive coordination");
        return true;  /* Non-fatal */
    }

    /* Connect to working memory if available */
    if (brain->working_memory) {
        if (meta_controller_connect_working_memory(ctrl, brain->working_memory) == 0) {
            NIMCP_LOGGING_DEBUG("Cognitive meta-controller connected to working memory");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect cognitive meta-controller to working memory");
        }
    }

    /* Connect to executive system if available */
    if (brain->executive) {
        if (meta_controller_connect_executive(ctrl, brain->executive) == 0) {
            NIMCP_LOGGING_DEBUG("Cognitive meta-controller connected to executive");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect cognitive meta-controller to executive");
        }
    }

    /* Connect to global workspace if available */
    if (brain->global_workspace) {
        if (meta_controller_connect_global_workspace(ctrl, brain->global_workspace) == 0) {
            NIMCP_LOGGING_DEBUG("Cognitive meta-controller connected to global workspace");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect cognitive meta-controller to global workspace");
        }
    }

    /* Connect to brain immune system if available */
    if (brain->immune_enabled && brain->immune_system) {
        if (meta_controller_connect_brain_immune(ctrl, brain->immune_system) == 0) {
            NIMCP_LOGGING_DEBUG("Cognitive meta-controller connected to brain immune");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect cognitive meta-controller to immune");
        }
    }

    /* Connect to bio-async router if available */
    if (brain->bio_async_enabled) {
        if (meta_controller_connect_bio_async(ctrl) == 0) {
            NIMCP_LOGGING_DEBUG("Cognitive meta-controller connected to bio-async");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect cognitive meta-controller to bio-async");
        }
    }

    /* Start the controller */
    if (meta_controller_start(ctrl) != 0) {
        NIMCP_LOGGING_WARN("Failed to start cognitive meta-controller");
        meta_controller_destroy(ctrl);
        return true;  /* Non-fatal */
    }

    /* Store controller in brain */
    brain->cognitive_meta_controller = ctrl;
    brain->cognitive_meta_controller_enabled = true;

    /* Log success */
    meta_controller_stats_t stats;
    if (meta_controller_get_stats(ctrl, &stats) == 0) {
        NIMCP_LOGGING_INFO("Cognitive meta-controller initialized: "
                          "requests=%lu, confidence=%.1f%%, "
                          "working_memory=%s, executive=%s, global_workspace=%s",
                          (unsigned long)stats.total_requests,
                          stats.system_confidence * 100.0f,
                          brain->working_memory ? "connected" : "N/A",
                          brain->executive ? "connected" : "N/A",
                          brain->global_workspace ? "connected" : "N/A");
    } else {
        NIMCP_LOGGING_INFO("Cognitive meta-controller initialized");
    }

    return true;
}
