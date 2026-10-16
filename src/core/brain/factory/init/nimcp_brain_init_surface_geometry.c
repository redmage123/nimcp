/**
 * @file nimcp_brain_init_surface_geometry.c
 * @brief Brain Initialization for Surface Geometry Subsystem
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "core/brain/factory/init/nimcp_brain_init_surface_geometry.h"
#include "core/geometry/nimcp_surface_geometry.h"
#include "core/brain/bridges/nimcp_surface_geometry_bridge.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_surface_geometry, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// INTERNAL STORAGE STRUCTURE
//=============================================================================

/**
 * @brief Surface geometry subsystem state stored in brain
 *
 * This structure is allocated and stored in the brain's extension data
 * during initialization.
 */
typedef struct surface_geometry_subsystem_struct {
    surface_geometry_ctx_t* geometry_ctx;
    surface_geometry_bridge_t* bridge;
    surface_geometry_init_flags_t flags;

    /* Callbacks */
    dendrite_geometry_callback_t dendrite_callback;
    void* dendrite_callback_data;
    axon_geometry_callback_t axon_callback;
    void* axon_callback_data;

    /* KG registration */
    uint32_t kg_node_id;

    bool initialized;
} surface_geometry_subsystem_t;

/* Extension data key for storing in brain */
#define SURFACE_GEOMETRY_EXTENSION_KEY "surface_geometry_subsystem"

//=============================================================================
// INTERNAL HELPERS
//=============================================================================

/**
 * @brief Get or create subsystem storage in brain
 */
static surface_geometry_subsystem_t* get_or_create_subsystem(brain_t brain) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;

    }

    /* For now, we'll use a static allocation since we don't have
     * direct access to brain's extension storage mechanism.
     * In a full implementation, this would use brain_get_extension()
     * or similar. */
    static surface_geometry_subsystem_t s_subsystem;
    static bool s_initialized = false;

    if (!s_initialized) {
        memset(&s_subsystem, 0, sizeof(s_subsystem));
        s_initialized = true;
    }

    return &s_subsystem;
}

/**
 * @brief Get existing subsystem (no create)
 */
static surface_geometry_subsystem_t* get_subsystem(brain_t brain) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;

    }

    surface_geometry_subsystem_t* subsystem = get_or_create_subsystem(brain);
    if (!subsystem || !subsystem->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "get_subsystem: required parameter is NULL (subsystem, subsystem->initialized)");
        return NULL;
    }
    return subsystem;
}

//=============================================================================
// INITIALIZATION
//=============================================================================

bool nimcp_brain_factory_init_surface_geometry_subsystem(brain_t brain) {
    return nimcp_brain_factory_init_surface_geometry_with_flags(
        brain,
        SURFACE_INIT_DEFAULT
    );
}

bool nimcp_brain_factory_init_surface_geometry_with_flags(
    brain_t brain,
    surface_geometry_init_flags_t flags
) {
    if (!brain) {
        NIMCP_LOGGING_ERROR("NULL brain handle");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_surface_geometry_with_flags: brain is NULL");
        return false;
    }

    surface_geometry_subsystem_t* subsystem = get_or_create_subsystem(brain);
    if (!subsystem) {
        NIMCP_LOGGING_ERROR("Failed to get subsystem storage");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_surface_geometry_with_flags: subsystem is NULL");
        return false;
    }

    /* Check if already initialized */
    if (subsystem->initialized) {
        NIMCP_LOGGING_WARN("Surface geometry already initialized");
        return true;
    }

    subsystem->flags = flags;

    /* Step 1: Create surface geometry context */
    surface_geometry_config_t geo_config;
    surface_geometry_default_config(&geo_config);

    /* Disable bio-async in geometry context if requested */
    if (flags & SURFACE_INIT_NO_BIO_ASYNC) {
        geo_config.enable_bio_async = false;
    }

    /* Disable quantum in geometry context if requested */
    if (flags & SURFACE_INIT_NO_QUANTUM) {
        geo_config.enable_quantum = false;
    }

    subsystem->geometry_ctx = surface_geometry_create(&geo_config);
    if (!subsystem->geometry_ctx) {
        NIMCP_LOGGING_ERROR("Failed to create surface geometry context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_surface_geometry_with_flags: subsystem->geometry_ctx is NULL");
        return false;
    }

    /* Step 2: Create surface geometry bridge */
    surface_geometry_bridge_config_t bridge_config;
    surface_geometry_bridge_default_config(&bridge_config);

    if (flags & SURFACE_INIT_NO_BIO_ASYNC) {
        bridge_config.enable_bio_async = false;
    }

    subsystem->bridge = surface_geometry_bridge_create(&bridge_config);
    if (!subsystem->bridge) {
        NIMCP_LOGGING_ERROR("Failed to create surface geometry bridge");
        surface_geometry_destroy(subsystem->geometry_ctx);
        subsystem->geometry_ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_surface_geometry_with_flags: subsystem->bridge is NULL");
        return false;
    }

    /* Step 3: Connect geometry context to bridge */
    if (surface_geometry_bridge_connect_geometry(
            subsystem->bridge, subsystem->geometry_ctx) != 0) {
        NIMCP_LOGGING_ERROR("Failed to connect geometry to bridge");
        surface_geometry_bridge_destroy(subsystem->bridge);
        surface_geometry_destroy(subsystem->geometry_ctx);
        subsystem->bridge = NULL;
        subsystem->geometry_ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_factory_init_surface_geometry_with_flags: operation failed");
        return false;
    }

    /* Step 4: Connect brain to bridge */
    if (surface_geometry_bridge_connect_brain(subsystem->bridge, brain) != 0) {
        NIMCP_LOGGING_ERROR("Failed to connect brain to bridge");
        surface_geometry_bridge_destroy(subsystem->bridge);
        surface_geometry_destroy(subsystem->geometry_ctx);
        subsystem->bridge = NULL;
        subsystem->geometry_ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_factory_init_surface_geometry_with_flags: validation failed");
        return false;
    }

    /* Step 5: Connect bio-async if enabled */
    if (!(flags & SURFACE_INIT_NO_BIO_ASYNC)) {
        if (surface_geometry_bridge_connect_bio_async(subsystem->bridge) != 0) {
            NIMCP_LOGGING_WARN("Failed to connect bio-async (continuing without)");
            /* Non-fatal - continue without bio-async */
        }
    }

    /* Step 6: Register with internal KG (see docs/claude/kg-node-naming-registry.md).
     * Surface geometry is infrastructure (dendrite/axon morphology), so it lives
     * in the BRAIN_KG_NODE_CORE category and is named by subsystem identity.
     *
     * This function can run AFTER brain_populate downgrades access to READ, so
     * we elevate to ADMIN via brain->internal_kg_admin_token, write, then
     * restore to READ (matching the pattern at init_internal_kg.c line 290). */
    subsystem->kg_node_id = BRAIN_KG_INVALID_NODE;
    if (brain->internal_kg_enabled && brain->internal_kg) {
        uint64_t admin_token = brain->internal_kg_admin_token;
        brain_kg_set_access_level(brain->internal_kg,
                                  BRAIN_KG_ACCESS_ADMIN, admin_token);

        brain_kg_node_id_t existing =
            brain_kg_find_node(brain->internal_kg, "surface_geometry_subsystem");
        if (existing != BRAIN_KG_INVALID_NODE) {
            subsystem->kg_node_id = existing;
        } else {
            subsystem->kg_node_id = brain_kg_add_node(
                brain->internal_kg,
                "surface_geometry_subsystem",
                BRAIN_KG_NODE_CORE,
                "Surface geometry: dendrite/axon morphology + curvature models"
            );
        }

        /* Link to neural substrate if that node exists (created elsewhere at init). */
        if (subsystem->kg_node_id != BRAIN_KG_INVALID_NODE) {
            brain_kg_node_id_t substrate =
                brain_kg_find_node(brain->internal_kg, "neural_substrate");
            if (substrate != BRAIN_KG_INVALID_NODE) {
                brain_kg_add_edge(
                    brain->internal_kg,
                    subsystem->kg_node_id,
                    substrate,
                    BRAIN_KG_EDGE_PROVIDES_TO,
                    "geometry provides morphology to substrate",
                    0.8f
                );
            }
        }

        brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
    }

    /* Mark as initialized */
    subsystem->initialized = true;

    NIMCP_LOGGING_INFO("Surface geometry subsystem initialized successfully");

    return true;
}

bool nimcp_brain_factory_shutdown_surface_geometry_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_shutdown_surface_geometry_subsystem: brain is NULL");
        return false;
    }

    surface_geometry_subsystem_t* subsystem = get_subsystem(brain);
    if (!subsystem) {
        return true;  /* Not initialized, nothing to do */
    }

    /* Disconnect and destroy bridge */
    if (subsystem->bridge) {
        surface_geometry_bridge_disconnect_bio_async(subsystem->bridge);
        surface_geometry_bridge_disconnect_brain(subsystem->bridge);
        surface_geometry_bridge_disconnect_geometry(subsystem->bridge);
        surface_geometry_bridge_destroy(subsystem->bridge);
        subsystem->bridge = NULL;
    }

    /* Destroy geometry context */
    if (subsystem->geometry_ctx) {
        surface_geometry_destroy(subsystem->geometry_ctx);
        subsystem->geometry_ctx = NULL;
    }

    /* Clear callbacks */
    subsystem->dendrite_callback = NULL;
    subsystem->dendrite_callback_data = NULL;
    subsystem->axon_callback = NULL;
    subsystem->axon_callback_data = NULL;

    subsystem->initialized = false;

    NIMCP_LOGGING_INFO("Surface geometry subsystem shutdown complete");

    return true;
}

//=============================================================================
// ACCESSORS
//=============================================================================

surface_geometry_ctx_t* nimcp_brain_get_surface_geometry_ctx(brain_t brain) {
    surface_geometry_subsystem_t* subsystem = get_subsystem(brain);
    return subsystem ? subsystem->geometry_ctx : NULL;
}

surface_geometry_bridge_t* nimcp_brain_get_surface_geometry_bridge(brain_t brain) {
    surface_geometry_subsystem_t* subsystem = get_subsystem(brain);
    return subsystem ? subsystem->bridge : NULL;
}

bool nimcp_brain_has_surface_geometry(brain_t brain) {
    surface_geometry_subsystem_t* subsystem = get_subsystem(brain);
    return subsystem && subsystem->initialized;
}

//=============================================================================
// INTEGRATION HOOKS
//=============================================================================

int nimcp_brain_register_dendrite_geometry_callback(
    brain_t brain,
    dendrite_geometry_callback_t callback,
    void* user_data
) {
    surface_geometry_subsystem_t* subsystem = get_subsystem(brain);
    if (!subsystem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_register_dendrite_geometry_callback: subsystem is NULL");
        return -1;
    }

    subsystem->dendrite_callback = callback;
    subsystem->dendrite_callback_data = user_data;

    return 0;
}

int nimcp_brain_register_axon_geometry_callback(
    brain_t brain,
    axon_geometry_callback_t callback,
    void* user_data
) {
    surface_geometry_subsystem_t* subsystem = get_subsystem(brain);
    if (!subsystem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_register_axon_geometry_callback: subsystem is NULL");
        return -1;
    }

    subsystem->axon_callback = callback;
    subsystem->axon_callback_data = user_data;

    return 0;
}
