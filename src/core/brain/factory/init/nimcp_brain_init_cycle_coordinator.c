/**
 * @file nimcp_brain_init_cycle_coordinator.c
 * @brief Brain Cycle Coordinator Subsystem Initialization & Integration
 *
 * WHAT: Initialize the brain cycle coordinator for unified cycle observability
 * WHY:  Provides centralized health tracking, stall detection, cross-cycle
 *       dependency management, and a single diagnostic entry point across
 *       all 9 brain cycle types
 * HOW:  Create coordinator, register all enabled cycles, set up dependencies,
 *       connect to bio-async, immune, KG, and other brain subsystems
 *
 * INTEGRATION POINTS:
 * - Bio-Async: Publish cycle health events for system-wide coordination
 * - Immune System: Stall/degradation triggers antigen presentation
 * - KG: Persist cycle statistics for historical analysis
 * - Introspection: Monitor consciousness metrics during cycle transitions
 * - Oscillations/FEP: Track predictive processing coherence
 * - Pink Noise: Modulate noise amplitude based on system health
 * - Global Workspace: Broadcast health for consciousness-level awareness
 * - World Model: Track prediction quality as health indicator
 *
 * NOTE: This file uses forward declarations to avoid header conflicts.
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_cycle_coordinator, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Main Initialization Function
//=============================================================================

/**
 * @brief Initialize cycle coordinator subsystem for brain
 *
 * @param brain Brain instance to initialize cycle coordinator for
 * @return true on success, false on failure
 *
 * PROCESS:
 * 1. Check if cycle coordinator is enabled in config
 * 2. Create coordinator with appropriate configuration
 * 3. Connect to available subsystems (immune, bio-async, KG, etc.)
 * 4. Register all enabled cycles
 * 5. Set up cross-cycle dependencies
 */
bool nimcp_brain_factory_init_cycle_coordinator_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_cycle_coordinator_subsystem: brain is NULL");
        return false;
    }

    /* Initialize fields */
    brain->cycle_coordinator = NULL;
    brain->cycle_coordinator_enabled = false;

    /* Check if cycle coordinator is enabled */
    if (!brain->config.enable_cycle_coordinator) {
        NIMCP_LOGGING_DEBUG("Cycle coordinator disabled by config");
        return true;  /* Success - disabled by config */
    }

    NIMCP_LOGGING_INFO("Initializing brain cycle coordinator...");

    /* Configure the coordinator */
    brain_cycle_coordinator_config_t config;
    brain_cycle_coordinator_default_config(&config);

    config.enable_timing_checks = true;
    config.enable_dependency_tracking = true;
    config.enable_auto_health_check = true;
    config.enable_logging = true;

    /* Wire bio-async context if available */
    if (brain->bio_async_ctx && brain->bio_async_enabled) {
        config.enable_bio_async = true;
        config.bio_context = (bio_module_context_t*)brain->bio_async_ctx;
        NIMCP_LOGGING_DEBUG("Cycle coordinator: bio-async integration enabled");
    }

    /* Wire immune system if available */
    if (brain->immune_system && brain->immune_enabled) {
        config.enable_immune_reporting = true;
        config.immune_system = brain->immune_system;
        NIMCP_LOGGING_DEBUG("Cycle coordinator: immune integration enabled");
    }

    /* Wire introspection if available (struct, take address) */
    config.enable_introspection = true;
    config.introspection_ctx = (introspection_context_t*)&brain->introspection;

    /* Wire global workspace if available */
    if (brain->global_workspace) {
        config.enable_global_workspace = true;
        config.gw_bridge = (snn_global_workspace_bridge_t*)brain->global_workspace;
        NIMCP_LOGGING_DEBUG("Cycle coordinator: global workspace integration enabled");
    }

    /* Wire pink noise if available */
    if (brain->pink_noise) {
        config.enable_pink_noise_modulation = true;
        config.pink_noise_bridge = (sfa_pink_noise_bridge_t*)brain->pink_noise;
        config.noise_health_sensitivity = 0.5f;
        NIMCP_LOGGING_DEBUG("Cycle coordinator: pink noise integration enabled");
    }

    /* Wire FEP orchestrator if available */
    if (brain->fep_orchestrator && brain->fep_orchestrator_enabled) {
        config.enable_fep_monitoring = true;
        config.fep_bridge = (oscillations_fep_bridge_t*)brain->fep_orchestrator;
        NIMCP_LOGGING_DEBUG("Cycle coordinator: FEP integration enabled");
    }

    /* Wire world model if available */
    if (brain->multimodal_world_model && brain->world_model_enabled) {
        config.enable_world_model = true;
        config.world_model = (world_model_multimodal_t*)brain->multimodal_world_model;
        NIMCP_LOGGING_DEBUG("Cycle coordinator: world model integration enabled");
    }

    /* Create the coordinator */
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&config);
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_brain_factory_init_cycle_coordinator_subsystem: "
            "failed to create cycle coordinator");
        return false;
    }

    brain->cycle_coordinator = coord;
    brain->cycle_coordinator_enabled = true;

    NIMCP_LOGGING_INFO("Cycle coordinator created, registering cycles...");

    /* ====================================================================
     * REGISTER CYCLES
     * ==================================================================== */

    /* Register immune tick cycle if immune system is available */
    if (brain->immune_system && brain->immune_enabled) {
        int rc = brain_cycle_coordinator_register(coord,
            BRAIN_CYCLE_IMMUNE_TICK, brain->immune_system, NULL);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Registered cycle: Immune Tick (50ms)");
        } else {
            NIMCP_LOGGING_WARN("Failed to register Immune Tick cycle");
        }
    }

    /* Register health agent cycle if available */
    if (brain->health_agent && brain->health_agent_enabled) {
        int rc = brain_cycle_coordinator_register(coord,
            BRAIN_CYCLE_HEALTH_AGENT, brain->health_agent, NULL);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Registered cycle: Health Agent (100ms)");
        } else {
            NIMCP_LOGGING_WARN("Failed to register Health Agent cycle");
        }
    }

    /* Register sleep-wake cycle (struct, take address) */
    {
        int rc = brain_cycle_coordinator_register(coord,
            BRAIN_CYCLE_SLEEP_WAKE, brain->sleep_system, NULL);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Registered cycle: Sleep-Wake (state machine)");
        } else {
            NIMCP_LOGGING_WARN("Failed to register Sleep-Wake cycle");
        }
    }

    /* Register circadian cycle from medulla (struct, take address) */
    if (brain->medulla_enabled) {
        int rc = brain_cycle_coordinator_register(coord,
            BRAIN_CYCLE_CIRCADIAN, &brain->medulla, NULL);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Registered cycle: Circadian (continuous)");
        } else {
            NIMCP_LOGGING_WARN("Failed to register Circadian cycle");
        }

        /* Register arousal cycle from medulla */
        rc = brain_cycle_coordinator_register(coord,
            BRAIN_CYCLE_AROUSAL, &brain->medulla, NULL);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Registered cycle: Arousal (event-driven)");
        } else {
            NIMCP_LOGGING_WARN("Failed to register Arousal cycle");
        }
    }

    /* Register oscillations cycle if available */
    if (brain->oscillations) {
        int rc = brain_cycle_coordinator_register(coord,
            BRAIN_CYCLE_OSCILLATIONS, brain->oscillations, NULL);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Registered cycle: Oscillations (10ms)");
        } else {
            NIMCP_LOGGING_WARN("Failed to register Oscillations cycle");
        }
    }

    /* Register GC agent cycle (part of brain, always available) */
    {
        int rc = brain_cycle_coordinator_register(coord,
            BRAIN_CYCLE_GC_AGENT, brain, NULL);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Registered cycle: GC Agent (60s adaptive)");
        } else {
            NIMCP_LOGGING_WARN("Failed to register GC Agent cycle");
        }
    }

    /* Register I/O dispatcher cycle (part of brain, always available) */
    {
        int rc = brain_cycle_coordinator_register(coord,
            BRAIN_CYCLE_IO_DISPATCHER, brain, NULL);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Registered cycle: I/O Dispatcher (queue-driven)");
        } else {
            NIMCP_LOGGING_WARN("Failed to register I/O Dispatcher cycle");
        }
    }

    /* Register brain update cycle (the main loop) */
    {
        int rc = brain_cycle_coordinator_register(coord,
            BRAIN_CYCLE_BRAIN_UPDATE, brain, NULL);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Registered cycle: Brain Update (16ms)");
        } else {
            NIMCP_LOGGING_WARN("Failed to register Brain Update cycle");
        }
    }

    /* ====================================================================
     * SET UP DEPENDENCIES
     * ==================================================================== */

    NIMCP_LOGGING_INFO("Setting up cross-cycle dependencies...");

    /* Sleep-Wake depends on Circadian (sleep pressure modulation) */
    brain_cycle_coordinator_add_dependency(coord,
        BRAIN_CYCLE_SLEEP_WAKE, BRAIN_CYCLE_CIRCADIAN);

    /* Sleep-Wake depends on Arousal (initiation threshold) */
    brain_cycle_coordinator_add_dependency(coord,
        BRAIN_CYCLE_SLEEP_WAKE, BRAIN_CYCLE_AROUSAL);

    /* Arousal depends on Circadian (baseline modulation) */
    brain_cycle_coordinator_add_dependency(coord,
        BRAIN_CYCLE_AROUSAL, BRAIN_CYCLE_CIRCADIAN);

    /* Immune Tick depends on Health Agent (message source) */
    brain_cycle_coordinator_add_dependency(coord,
        BRAIN_CYCLE_IMMUNE_TICK, BRAIN_CYCLE_HEALTH_AGENT);

    /* Brain Update depends on Oscillations (phase coherence) */
    brain_cycle_coordinator_add_dependency(coord,
        BRAIN_CYCLE_BRAIN_UPDATE, BRAIN_CYCLE_OSCILLATIONS);

    NIMCP_LOGGING_INFO("Cross-cycle dependencies configured");

    /* ====================================================================
     * CONNECT INTEGRATION SUBSYSTEMS (post-creation connections)
     * ==================================================================== */

    /* Connect bio-async for event publishing */
    if (brain->bio_async_ctx && brain->bio_async_enabled) {
        int rc = brain_cycle_coordinator_connect_bio_async(coord,
            (bio_module_context_t*)brain->bio_async_ctx);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Cycle coordinator connected to bio-async");
        }
    }

    /* Connect immune system for health reporting */
    if (brain->immune_system && brain->immune_enabled) {
        int rc = brain_cycle_coordinator_connect_immune(coord,
            brain->immune_system);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Cycle coordinator connected to immune system");
        }
    }

    /* Connect introspection */
    {
        int rc = brain_cycle_coordinator_connect_introspection(coord,
            (introspection_context_t*)&brain->introspection);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Cycle coordinator connected to introspection");
        }
    }

    /* Connect global workspace */
    if (brain->global_workspace) {
        int rc = brain_cycle_coordinator_connect_global_workspace(coord,
            (snn_global_workspace_bridge_t*)brain->global_workspace);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Cycle coordinator connected to global workspace");
        }
    }

    /* Connect pink noise */
    if (brain->pink_noise) {
        int rc = brain_cycle_coordinator_connect_pink_noise(coord,
            (sfa_pink_noise_bridge_t*)brain->pink_noise);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Cycle coordinator connected to pink noise");
        }
    }

    /* Connect FEP orchestrator */
    if (brain->fep_orchestrator && brain->fep_orchestrator_enabled) {
        int rc = brain_cycle_coordinator_connect_fep(coord,
            (oscillations_fep_bridge_t*)brain->fep_orchestrator);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Cycle coordinator connected to FEP orchestrator");
        }
    }

    /* Connect world model */
    if (brain->multimodal_world_model && brain->world_model_enabled) {
        int rc = brain_cycle_coordinator_connect_world_model(coord,
            (world_model_multimodal_t*)brain->multimodal_world_model);
        if (rc == 0) {
            NIMCP_LOGGING_INFO("Cycle coordinator connected to world model");
        }
    }

    /* Run initial health check */
    int issues = brain_cycle_coordinator_check_health(coord);
    NIMCP_LOGGING_INFO("Cycle coordinator initialization complete (%d initial issues)", issues);

    /* Log initial state */
    brain_cycle_coordinator_log_state(coord);

    return true;
}

//=============================================================================
// Shutdown Function
//=============================================================================

/**
 * @brief Destroy cycle coordinator subsystem
 *
 * @param brain Brain instance to destroy cycle coordinator for
 */
void nimcp_brain_factory_destroy_cycle_coordinator_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_cycle_coordinator_subsystem: brain is NULL"); return; }

    if (brain->cycle_coordinator && brain->cycle_coordinator_enabled) {
        NIMCP_LOGGING_INFO("Shutting down brain cycle coordinator...");

        /* Log final state before destruction */
        brain_cycle_coordinator_log_state(brain->cycle_coordinator);

        /* Flush any pending KG writes */
        brain_cycle_coordinator_flush_to_kg(brain->cycle_coordinator);

        /* Destroy the coordinator */
        brain_cycle_coordinator_destroy(brain->cycle_coordinator);
    }

    brain->cycle_coordinator = NULL;
    brain->cycle_coordinator_enabled = false;
}

//=============================================================================
// Accessor Functions
//=============================================================================

/**
 * @brief Get cycle coordinator from brain
 *
 * @param brain Brain instance
 * @return Cycle coordinator handle or NULL if not enabled
 */
struct brain_cycle_coordinator* brain_get_cycle_coordinator(brain_t brain) {
    if (!brain || !brain->cycle_coordinator_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_cycle_coordinator: required parameter is NULL (brain, brain->cycle_coordinator_enabled)");
        return NULL;
    }
    return brain->cycle_coordinator;
}
