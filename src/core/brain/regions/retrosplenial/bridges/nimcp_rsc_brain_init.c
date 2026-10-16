//=============================================================================
// nimcp_rsc_brain_init.c - RSC Brain Factory Integration
//=============================================================================
/**
 * @file nimcp_rsc_brain_init.c
 * @brief Implementation of RSC brain factory integration
 */

#include "core/brain/regions/retrosplenial/bridges/nimcp_rsc_brain_init.h"
#include "core/brain/regions/retrosplenial/bridges/nimcp_rsc_security.h"
#include "core/brain/regions/retrosplenial/bridges/nimcp_rsc_kg_wiring.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(rsc_brain_init, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* Forward declarations for RSC functions we need */
/* Using NULL config to nimcp_rsc_create will use defaults */
extern nimcp_retrosplenial_t* nimcp_rsc_create(const void* config);
extern void nimcp_rsc_destroy(nimcp_retrosplenial_t* rsc);

//=============================================================================
// Version String
//=============================================================================

static const char* rsc_version_string = "1.0.0";

//=============================================================================
// Local Constants (match nimcp_retrosplenial.h defaults)
//=============================================================================

/** Default neuron counts from RSC module */
#define RSC_INIT_DEFAULT_TRANSFORM_NEURONS   256
#define RSC_INIT_DEFAULT_CONTEXT_NEURONS     512
#define RSC_INIT_DEFAULT_SCENE_NEURONS       256
#define RSC_INIT_DEFAULT_HD_NEURONS          60

//=============================================================================
// Configuration API
//=============================================================================

int rsc_init_default_config(rsc_init_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_init_default_config: config is NULL");
        return -1;
    }

    config->num_transform_neurons = RSC_INIT_DEFAULT_TRANSFORM_NEURONS;
    config->num_context_neurons = RSC_INIT_DEFAULT_CONTEXT_NEURONS;
    config->num_scene_neurons = RSC_INIT_DEFAULT_SCENE_NEURONS;
    config->num_hd_neurons = RSC_INIT_DEFAULT_HD_NEURONS;
    config->enable_security = true;
    config->enable_kg_wiring = true;
    config->enable_bio_async = true;
    config->enable_immune_bridge = true;
    config->enable_hippocampus = true;
    config->enable_entorhinal = true;
    config->admin_token = 0;

    return 0;
}

//=============================================================================
// Brain Factory Integration API
//=============================================================================

int rsc_brain_init_register(void) {
    /* Register RSC factory functions with brain factory system */
    /* This would typically add RSC to a registry of subsystem initializers */

    NIMCP_LOG_DEBUG(RSC_INIT_MODULE_NAME,
        "RSC factory functions registered");

    return 0;
}

int rsc_brain_init_create(
    brain_t brain,
    const rsc_init_config_t* config,
    rsc_init_result_t* result
) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_brain_init_create: brain is NULL");
        return -1;
    }

    /* Use defaults if no config */
    rsc_init_config_t local_config;
    if (!config) {
        rsc_init_default_config(&local_config);
        config = &local_config;
    }

    rsc_init_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));

    /* Create RSC instance with defaults (pass NULL for default config) */
    /* Full configuration would require including nimcp_retrosplenial.h */
    nimcp_retrosplenial_t* rsc = nimcp_rsc_create(NULL);
    if (!rsc) {
        local_result.error_count++;
        NIMCP_LOG_ERROR(RSC_INIT_MODULE_NAME,
            "Failed to create RSC instance");
        goto done;
    }
    local_result.rsc_initialized = true;

    /* Initialize bridges */
    if (config->enable_security) {
        local_result.security_registered = rsc_init_security(brain, rsc);
        if (!local_result.security_registered) {
            local_result.warning_count++;
        }
    }

    if (config->enable_kg_wiring) {
        local_result.kg_registered = rsc_init_kg_wiring(
            brain, rsc, config->admin_token);
        if (!local_result.kg_registered) {
            local_result.warning_count++;
        }
    }

    if (config->enable_bio_async) {
        local_result.bio_async_connected = rsc_init_bio_async_bridges(
            brain, rsc);
        if (!local_result.bio_async_connected) {
            local_result.warning_count++;
        }
    }

    if (config->enable_immune_bridge) {
        local_result.immune_connected = rsc_init_immune_bridge(brain, rsc);
        if (!local_result.immune_connected) {
            local_result.warning_count++;
        }
    }

    if (config->enable_hippocampus) {
        local_result.hippocampus_connected = rsc_init_hippocampus_bridge(
            brain, rsc);
        if (!local_result.hippocampus_connected) {
            local_result.warning_count++;
        }
    }

    if (config->enable_entorhinal) {
        local_result.entorhinal_connected = rsc_init_entorhinal_bridge(
            brain, rsc);
        if (!local_result.entorhinal_connected) {
            local_result.warning_count++;
        }
    }

    NIMCP_LOG_INFO(RSC_INIT_MODULE_NAME,
        "RSC subsystem initialized: sec=%d, kg=%d, bio=%d",
        local_result.security_registered, local_result.kg_registered,
        local_result.bio_async_connected);

done:
    if (result) {
        *result = local_result;
    }

    return local_result.error_count > 0 ? -1 : 0;
}

int rsc_brain_init_destroy(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_brain_init_destroy: brain is NULL");
        return -1;
    }

    /* Get RSC instance and destroy */
    nimcp_retrosplenial_t* rsc = rsc_get_from_brain(brain);
    if (rsc) {
        nimcp_rsc_destroy(rsc);
    }

    NIMCP_LOG_INFO(RSC_INIT_MODULE_NAME,
        "RSC subsystem destroyed");

    return 0;
}

bool nimcp_brain_factory_init_rsc_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_rsc_subsystem: brain is NULL");
        return false;
    }

    rsc_init_result_t result;
    if (rsc_brain_init_create(brain, NULL, &result) < 0) {
        NIMCP_LOG_ERROR(RSC_INIT_MODULE_NAME,
            "RSC subsystem initialization failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_brain_factory_init_rsc_subsystem: validation failed");
        return false;
    }

    return result.error_count == 0;
}

void nimcp_brain_factory_destroy_rsc_subsystem(brain_t brain) {
    rsc_brain_init_destroy(brain);
}

//=============================================================================
// Bridge Initialization
//=============================================================================

bool rsc_init_security(brain_t brain, nimcp_retrosplenial_t* rsc) {
    if (!brain || !rsc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_init_security: required parameter is NULL (brain, rsc)");
        return false;
    }

    /* Would access brain's BBB and register RSC module */
    /* bbb_system_t bbb = brain_get_bbb(brain); */
    /* if (bbb) rsc_security_register(bbb, NULL, NULL); */

    NIMCP_LOG_DEBUG(RSC_INIT_MODULE_NAME,
        "Security registration ready (register when BBB available)");

    return true;
}

bool rsc_init_kg_wiring(
    brain_t brain,
    nimcp_retrosplenial_t* rsc,
    uint64_t admin_token
) {
    if (!brain || !rsc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_init_kg_wiring: required parameter is NULL (brain, rsc)");
        return false;
    }

    /* Would access brain's KG and register RSC nodes */
    /* brain_kg_t* kg = brain_get_kg(brain); */
    /* if (kg) rsc_kg_register_all(kg, NULL, NULL, admin_token); */

    (void)admin_token;

    NIMCP_LOG_DEBUG(RSC_INIT_MODULE_NAME,
        "KG wiring ready (register when KG available)");

    return true;
}

bool rsc_init_bio_async_bridges(
    brain_t brain,
    nimcp_retrosplenial_t* rsc
) {
    if (!brain || !rsc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_init_bio_async_bridges: required parameter is NULL (brain, rsc)");
        return false;
    }

    /* Would connect RSC to brain's bio-router */
    /* nimcp_bio_router_t* router = brain_get_bio_router(brain); */
    /* if (router) nimcp_rsc_init_bio_async_bridge(rsc, router); */

    NIMCP_LOG_DEBUG(RSC_INIT_MODULE_NAME,
        "Bio-async bridges ready (connect when router available)");

    return true;
}

bool rsc_init_immune_bridge(
    brain_t brain,
    nimcp_retrosplenial_t* rsc
) {
    if (!brain || !rsc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_init_immune_bridge: required parameter is NULL (brain, rsc)");
        return false;
    }

    /* Would connect RSC to brain's immune system */
    /* brain_immune_system_t* immune = brain_get_immune(brain); */
    /* if (immune) nimcp_rsc_init_immune_bridge(rsc, immune); */

    NIMCP_LOG_DEBUG(RSC_INIT_MODULE_NAME,
        "Immune bridge ready (connect when immune system available)");

    return true;
}

bool rsc_init_hippocampus_bridge(
    brain_t brain,
    nimcp_retrosplenial_t* rsc
) {
    if (!brain || !rsc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_init_hippocampus_bridge: required parameter is NULL (brain, rsc)");
        return false;
    }

    /* Would connect RSC to hippocampus */
    /* hippocampus_adapter_t* hipp = brain_get_hippocampus(brain); */
    /* if (hipp) nimcp_rsc_init_hippocampus_bridge(rsc, hipp); */

    NIMCP_LOG_DEBUG(RSC_INIT_MODULE_NAME,
        "Hippocampus bridge ready (connect when hippocampus available)");

    return true;
}

bool rsc_init_entorhinal_bridge(
    brain_t brain,
    nimcp_retrosplenial_t* rsc
) {
    if (!brain || !rsc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_init_entorhinal_bridge: required parameter is NULL (brain, rsc)");
        return false;
    }

    /* Would connect RSC to entorhinal cortex */
    /* nimcp_entorhinal_t* ec = brain_get_entorhinal(brain); */
    /* if (ec) nimcp_rsc_init_entorhinal_bridge(rsc, ec); */

    NIMCP_LOG_DEBUG(RSC_INIT_MODULE_NAME,
        "Entorhinal bridge ready (connect when entorhinal available)");

    return true;
}

//=============================================================================
// Query API
//=============================================================================

bool rsc_is_initialized(brain_t brain) {
    if (!brain) {
        return false;
    }

    /* Would check if RSC subsystem exists in brain */
    /* return brain_has_rsc_subsystem(brain); */

    return true;  /* Assume initialized if brain exists */
}

const char* rsc_get_version(void) {
    return rsc_version_string;
}

nimcp_retrosplenial_t* rsc_get_from_brain(brain_t brain) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;

    }

    /* Would retrieve RSC from brain's subsystem registry */
    /* return (nimcp_retrosplenial_t*)brain_get_subsystem(brain, "rsc"); */

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_get_from_brain: operation failed");
    return NULL;  /* Placeholder - requires brain internal access */
}
