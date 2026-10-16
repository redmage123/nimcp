//=============================================================================
// nimcp_reticular_brain_init.c - Reticular Formation Brain Factory Integration
//=============================================================================
/**
 * @file nimcp_reticular_brain_init.c
 * @brief Implementation of reticular formation brain factory integration
 */

#include "core/brain/regions/reticular/bridges/nimcp_reticular_brain_init.h"
#include "core/brain/regions/reticular/nimcp_reticular.h"
#include "core/brain/regions/reticular/bridges/nimcp_reticular_security.h"
/* Note: nimcp_reticular_kg_wiring.h not included to avoid type conflicts
 * The KG wiring types are already defined in nimcp_reticular.h */
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(reticular_brain_init, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Version String
//=============================================================================

static const char* reticular_version_string = "1.0.0";

//=============================================================================
// Configuration API
//=============================================================================

int reticular_init_default_config(reticular_init_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->initial_arousal = 0.5f;
    config->default_temperature = 37.0f;
    config->enable_all_nuclei = true;
    config->enable_bio_async = true;
    config->enable_kg_wiring = true;
    config->enable_security = true;
    config->enable_immune_bridge = true;
    config->enable_quantum = true;
    config->enable_logging = true;
    config->admin_token = 0;
    config->platform_tier = 0;  /* Full tier by default */

    return 0;
}

//=============================================================================
// Brain Factory Integration API
//=============================================================================

int reticular_brain_init_register(brain_t brain) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return -1;

    }

    reticular_init_config_t config;
    if (reticular_init_default_config(&config) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "reticular_brain_init_register: validation failed");
        return -1;
    }

    reticular_init_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_reticular_t* reticular = reticular_brain_init_create(brain, &config, &result);
    if (!reticular) {
        NIMCP_LOG_ERROR(RETICULAR_INIT_MODULE_NAME,
            "Reticular subsystem creation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_brain_init_register: reticular is NULL");
        return -1;
    }

    NIMCP_LOG_INFO(RETICULAR_INIT_MODULE_NAME,
        "Reticular subsystem registered: nuclei=%d, mods=%d, auto=%d",
        result.nuclei_initialized, result.modulators_initialized,
        result.autonomic_initialized);

    return result.error_count > 0 ? -1 : 0;
}

nimcp_reticular_t* reticular_brain_init_create(
    brain_t brain,
    const reticular_init_config_t* config,
    reticular_init_result_t* result
) {
    if (!brain || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reticular_brain_init_create: required parameter is NULL (brain, config)");
        return NULL;
    }

    reticular_init_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));

    /* Create reticular configuration from init config */
    reticular_config_t ret_config;
    if (reticular_default_config(&ret_config) < 0) {
        local_result.error_count++;
        if (result) *result = local_result;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reticular_brain_init_create: validation failed");
        return NULL;
    }

    /* Apply init config settings */
    ret_config.enable_bio_async = config->enable_bio_async;
    ret_config.enable_kg_wiring = config->enable_kg_wiring;
    ret_config.enable_security = config->enable_security;
    ret_config.enable_immune = config->enable_immune_bridge;
    ret_config.enable_quantum = config->enable_quantum;
    ret_config.enable_logging = config->enable_logging;
    ret_config.platform_tier = (platform_tier_t)config->platform_tier;

    /* Create reticular formation instance */
    nimcp_reticular_t* reticular = reticular_create(&ret_config);
    if (!reticular) {
        local_result.error_count++;
        NIMCP_LOG_ERROR(RETICULAR_INIT_MODULE_NAME,
            "Failed to create reticular formation instance");
        if (result) *result = local_result;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return NULL;
    }

    local_result.reticular_initialized = true;

    /* Initialize components */
    local_result.nuclei_initialized = reticular_init_nuclei(reticular, config);
    if (!local_result.nuclei_initialized) {
        local_result.warning_count++;
    }

    local_result.modulators_initialized = reticular_init_modulators(reticular, config);
    if (!local_result.modulators_initialized) {
        local_result.warning_count++;
    }

    local_result.autonomic_initialized = reticular_init_autonomic(reticular, config);
    if (!local_result.autonomic_initialized) {
        local_result.warning_count++;
    }

    /* Initialize bridges */
    if (config->enable_bio_async) {
        local_result.bio_async_connected =
            reticular_init_bio_async_bridges(reticular, brain);
        if (!local_result.bio_async_connected) {
            local_result.warning_count++;
        }
    }

    if (config->enable_kg_wiring) {
        local_result.kg_registered =
            reticular_init_kg_wiring(reticular, brain, config->admin_token);
        if (!local_result.kg_registered) {
            local_result.warning_count++;
        }
    }

    if (config->enable_security) {
        local_result.security_registered =
            reticular_init_security(reticular, brain);
        if (!local_result.security_registered) {
            local_result.warning_count++;
        }
    }

    if (config->enable_immune_bridge) {
        local_result.immune_connected =
            reticular_init_immune_bridge(reticular, brain);
        if (!local_result.immune_connected) {
            local_result.warning_count++;
        }
    }

    if (result) {
        *result = local_result;
    }

    return reticular;
}

int reticular_brain_init_destroy(nimcp_reticular_t* reticular) {
    if (!reticular) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular is NULL");

        return -1;

    }

    reticular_destroy(reticular);

    NIMCP_LOG_INFO(RETICULAR_INIT_MODULE_NAME,
        "Reticular subsystem destroyed");

    return 0;
}

//=============================================================================
// Component Initialization
//=============================================================================

bool reticular_init_nuclei(
    nimcp_reticular_t* reticular,
    const reticular_init_config_t* config
) {
    if (!reticular || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_init_nuclei: required parameter is NULL (reticular, config)");
        return false;
    }

    /* Nuclei are initialized by reticular_create via reticular_init */
    /* This function performs any additional post-creation setup */

    NIMCP_LOG_DEBUG(RETICULAR_INIT_MODULE_NAME,
        "Nuclei subsystem initialized");

    return true;
}

bool reticular_init_modulators(
    nimcp_reticular_t* reticular,
    const reticular_init_config_t* config
) {
    if (!reticular || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_init_modulators: required parameter is NULL (reticular, config)");
        return false;
    }

    /* Modulators are initialized by reticular_create */
    /* Additional configuration can be applied here */

    NIMCP_LOG_DEBUG(RETICULAR_INIT_MODULE_NAME,
        "Neuromodulators subsystem initialized");

    return true;
}

bool reticular_init_autonomic(
    nimcp_reticular_t* reticular,
    const reticular_init_config_t* config
) {
    if (!reticular || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_init_autonomic: required parameter is NULL (reticular, config)");
        return false;
    }

    /* Autonomic system initialized by reticular_create */

    NIMCP_LOG_DEBUG(RETICULAR_INIT_MODULE_NAME,
        "Autonomic subsystem initialized");

    return true;
}

//=============================================================================
// Bridge Initialization
//=============================================================================

bool reticular_init_bio_async_bridges(
    nimcp_reticular_t* reticular,
    brain_t brain
) {
    if (!reticular || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_init_bio_async_bridges: required parameter is NULL (reticular, brain)");
        return false;
    }

    /* Bio-async connection would be established here */
    /* Requires access to brain's bio-router */

    NIMCP_LOG_DEBUG(RETICULAR_INIT_MODULE_NAME,
        "Bio-async bridges ready (connect when router available)");

    return true;
}

bool reticular_init_kg_wiring(
    nimcp_reticular_t* reticular,
    brain_t brain,
    uint64_t admin_token
) {
    if (!reticular || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_init_kg_wiring: required parameter is NULL (reticular, brain)");
        return false;
    }

    /* KG wiring would be established here */
    /* Requires access to brain's KG */

    (void)admin_token;

    NIMCP_LOG_DEBUG(RETICULAR_INIT_MODULE_NAME,
        "KG wiring ready (register when KG available)");

    return true;
}

bool reticular_init_security(
    nimcp_reticular_t* reticular,
    brain_t brain
) {
    if (!reticular || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_init_security: required parameter is NULL (reticular, brain)");
        return false;
    }

    /* Security registration would be established here */
    /* Requires access to brain's BBB */

    NIMCP_LOG_DEBUG(RETICULAR_INIT_MODULE_NAME,
        "Security registration ready (register when BBB available)");

    return true;
}

bool reticular_init_immune_bridge(
    nimcp_reticular_t* reticular,
    brain_t brain
) {
    if (!reticular || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_init_immune_bridge: required parameter is NULL (reticular, brain)");
        return false;
    }

    /* Immune bridge would be established here */
    /* Requires access to brain's immune system */

    NIMCP_LOG_DEBUG(RETICULAR_INIT_MODULE_NAME,
        "Immune bridge ready (connect when immune system available)");

    return true;
}

//=============================================================================
// Query API
//=============================================================================

bool reticular_is_initialized(nimcp_reticular_t* reticular) {
    if (!reticular) {
        return false;
    }
    return reticular->initialized;
}

const char* reticular_get_version(void) {
    return reticular_version_string;
}
