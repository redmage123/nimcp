//=============================================================================
// nimcp_brain_init_swarm_module_registry.c - Swarm Module Registry Init
//=============================================================================
/**
 * @file nimcp_brain_init_swarm_module_registry.c
 * @brief Swarm Module Registry subsystem initialization for brain
 *
 * WHAT: Swarm Module Registry initialization and module registration setup
 * WHY:  Plugin architecture for swarm behaviors with priority-based arbitration
 * HOW:  Creates registry, connects swarm_brain and bio-async, starts coordination
 *
 * BIOLOGICAL BASIS:
 * Models hierarchical behavioral selection (Tinbergen's model):
 * - Multiple concurrent motivations (foraging, mating, escape)
 * - Priority-based arbitration (predator > hunger > curiosity)
 * - Context-sensitive weighting (hunger increases over time)
 * Natural swarms coordinate multiple behaviors:
 * - Bees: Waggle dance + thermoregulation + foraging
 * - Ants: Pheromone trails + nest defense + brood care
 * - Birds: Flocking + predator evasion + migration
 *
 * DESIGN PATTERNS:
 * - Factory: Created by brain factory during lifecycle init
 * - Registry: Central storage and lookup for modules
 * - Plugin: Standardized interface for heterogeneous modules
 * - Chain of Responsibility: Priority-based conflict resolution
 * - Observer: Event callbacks for module state changes
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
#include "swarm/nimcp_swarm_module_registry.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_SWARM_REGISTRY"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_swarm_module_registry, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Main Initialization Function
//=============================================================================

/**
 * @brief Initialize swarm module registry subsystem
 *
 * WHAT: Creates and configures swarm module registry for brain
 * WHY:  Provides plugin architecture for swarm behavioral modules
 * HOW:  Guard clause checks, create registry, wire swarm/async dependencies
 *
 * @param brain Brain instance to initialize registry for
 * @return true on success or non-fatal failure, false on critical error
 */
bool nimcp_brain_factory_init_swarm_module_registry_subsystem(brain_t brain) {
    /* Guard clause: NULL check */
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_swarm_module_registry_subsystem: brain is NULL");
        return false;
    }

    /* Initialize fields to defaults */
    brain->swarm_module_registry = NULL;
    brain->swarm_module_registry_enabled = false;

    /* Check if swarm coordination dependencies are present */
    /* Swarm module registry coordinates swarm behaviors via bio-async messaging */
    /* It's useful when bio-async or immune is enabled for swarm coordination */
    bool should_enable = brain->bio_async_enabled || brain->immune_enabled;

    if (!should_enable) {
        NIMCP_LOGGING_DEBUG("Swarm module registry skipped (no dependencies)");
        return true;
    }

    /* Create registry configuration */
    swarm_registry_config_t config;
    swarm_registry_default_config(&config);

    /* Configure based on brain settings */
    config.enable_bio_async = brain->bio_async_enabled;
    config.enable_auto_wiring = true;  /* Auto-wire modules to swarm_brain */
    config.enable_statistics = true;
    config.arbitration = SWARM_ARBITRATION_HIGHEST_PRIORITY;

    /* Create registry */
    swarm_module_registry_t* registry = swarm_registry_create(&config);
    if (!registry) {
        NIMCP_LOGGING_WARN("Failed to create swarm module registry - "
                          "continuing without swarm coordination");
        return true;  /* Non-fatal */
    }

    /* Note: Swarm brain connection deferred to runtime when swarm modules register */
    /* The registry manages swarm behaviors independently of brain-level swarm fields */

    /* Connect to bio-async router if available */
    if (brain->bio_async_enabled) {
        if (swarm_registry_connect_bio_async(registry) == 0) {
            NIMCP_LOGGING_DEBUG("Swarm registry connected to bio-async");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect swarm registry to bio-async");
        }
    }

    /* Connect to brain immune system if available */
    if (brain->immune_enabled && brain->immune_system) {
        if (swarm_registry_connect_brain_immune(registry, brain->immune_system) == 0) {
            NIMCP_LOGGING_DEBUG("Swarm registry connected to brain immune");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect swarm registry to brain immune");
        }
    }

    /* Store registry in brain */
    brain->swarm_module_registry = registry;
    brain->swarm_module_registry_enabled = true;

    /* Log success */
    swarm_registry_stats_t stats;
    if (swarm_registry_get_stats(registry, &stats) == 0) {
        NIMCP_LOGGING_INFO("Swarm module registry initialized: "
                          "modules=%u, active=%u, wired=%u, "
                          "bio_async=%s, immune=%s",
                          stats.total_modules,
                          stats.active_modules,
                          stats.wired_to_brain,
                          brain->bio_async_enabled ? "connected" : "disabled",
                          brain->immune_enabled ? "connected" : "disabled");
    } else {
        NIMCP_LOGGING_INFO("Swarm module registry initialized");
    }

    return true;
}
