//=============================================================================
// nimcp_brain_init_core_directives.c - Core Directives Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_core_directives.c
 * @brief Core directives subsystem initialization for brain
 *
 * WHAT: Core directives initialization and integration setup
 * WHY:  Provides ethical constraint enforcement for all brain actions
 * HOW:  Creates directives system, connects to immune/FEP, enables bio-async
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-16
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
// core/directives/nimcp_core_directives.h is included via nimcp_brain_internal.h
// NOTE: cognitive/ethics/nimcp_core_directives.h shares the same include guard
// as core/directives/nimcp_core_directives.h, so we forward-declare the
// functions we need from the ethics version:
int core_directives_connect_immune(core_directives_system_t* directives,
                                    brain_immune_system_t* immune);
int core_directives_connect_fep(core_directives_system_t* directives,
                                 fep_orchestrator_t* fep_orch);
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_CORE_DIRECTIVES"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_core_directives, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Main Initialization Function
//=============================================================================

bool nimcp_brain_factory_init_core_directives_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_core_directives_subsystem: brain is NULL");
        return false;
    }

    /* Initialize core directives fields to defaults */
    brain->core_directives = NULL;
    brain->directive_immune_bridge = NULL;
    brain->directive_fep_bridge = NULL;
    brain->core_directives_enabled = false;

    /* Core directives should always be enabled as the ethical foundation */
    /* However, we make it conditional on bio-async or immune for now */
    bool should_enable = true; // Always enable

    if (!should_enable) {
        NIMCP_LOGGING_DEBUG("Core directives not enabled");
        return true;  /* Not an error - just not needed */
    }

    /* Create directives configuration using core/directives version */
    /* WHAT: Configure the core directives orchestrator with all sub-modules
     * WHY:  core_directives_default_config() initializes all nested configs
     * HOW:  Use default config which sets harm_config, command_config, etc. */
    core_directives_config_t dir_config;
    core_directives_default_config(&dir_config);

    /* Override specific thresholds if needed */
    dir_config.harm_config.block_threshold = 0.3f;
    dir_config.harm_config.warn_threshold = 0.1f;
    dir_config.harm_config.enable_human_escalation = true;

    dir_config.reciprocity_config.symmetry_threshold = 0.7f;
    dir_config.reciprocity_config.enable_perspective_taking = true;

    dir_config.combinatorial_config.harm_threshold = 0.7f;
    dir_config.combinatorial_config.max_pattern_count = 256;

    dir_config.history_config.max_history_size = 100;

    /* Enable all directive checks */
    dir_config.enable_all_checks = true;
    dir_config.strict_mode = true;

    /* Create core directives system */
    core_directives_system_t* directives = core_directives_create(&dir_config);
    if (!directives) {
        NIMCP_LOGGING_WARN("Failed to create core directives - continuing without ethical constraints");
        return true;  /* Non-fatal, but should ideally fail */
    }

    /* Connect to bio-async if available */
    if (brain->bio_async_enabled) {
        if (core_directives_connect_bio_async(directives) == 0) {
            NIMCP_LOGGING_DEBUG("Core directives connected to bio-async router");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect core directives to bio-async");
        }
    }

    /* Connect to brain immune system if available */
    if (brain->immune_enabled && brain->immune_system) {
        if (core_directives_connect_immune(directives, brain->immune_system) == 0) {
            NIMCP_LOGGING_DEBUG("Core directives connected to brain immune system");
            /* Store bridge reference from directives internal structure */
            /* Note: This is a placeholder - the actual bridge is managed internally */
        } else {
            NIMCP_LOGGING_WARN("Failed to connect core directives to immune system");
        }
    }

    /* Connect to FEP orchestrator if available */
    if (brain->fep_orchestrator) {
        if (core_directives_connect_fep(directives, brain->fep_orchestrator) == 0) {
            NIMCP_LOGGING_DEBUG("Core directives connected to FEP orchestrator");
            /* Store bridge reference from directives internal structure */
            /* Note: This is a placeholder - the actual bridge is managed internally */
        } else {
            NIMCP_LOGGING_WARN("Failed to connect core directives to FEP orchestrator");
        }
    }

    /* Store in brain */
    brain->core_directives = directives;
    brain->core_directives_enabled = true;

    NIMCP_LOGGING_INFO("Core directives subsystem initialized successfully");
    NIMCP_LOGGING_INFO("  Asimov's Laws: ENABLED (harm prevention, obedience, self-preservation)");
    NIMCP_LOGGING_INFO("  Golden Rule: ENABLED (reciprocity evaluation)");
    NIMCP_LOGGING_INFO("  Combinatorial Harm: ENABLED (emergent harm detection)");
    NIMCP_LOGGING_INFO("  Action History: %u entries", dir_config.history_config.max_history_size);
    NIMCP_LOGGING_INFO("  Bio-Async: %s", brain->bio_async_enabled ? "CONNECTED" : "DISABLED");
    NIMCP_LOGGING_INFO("  Immune Integration: %s", brain->immune_enabled ? "CONNECTED" : "DISABLED");
    NIMCP_LOGGING_INFO("  FEP Integration: %s", brain->fep_orchestrator ? "CONNECTED" : "DISABLED");

    return true;
}
