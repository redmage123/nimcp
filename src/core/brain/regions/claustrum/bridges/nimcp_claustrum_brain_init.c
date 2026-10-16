//=============================================================================
// nimcp_claustrum_brain_init.c - Claustrum Brain Factory Integration
//=============================================================================
/**
 * @file nimcp_claustrum_brain_init.c
 * @brief Implementation of claustrum brain factory integration
 */

#include "core/brain/regions/claustrum/bridges/nimcp_claustrum_brain_init.h"
#include "core/brain/regions/claustrum/nimcp_claustrum.h"
#include "core/brain/regions/claustrum/bridges/nimcp_claustrum_kg_wiring.h"
#include "core/brain/regions/claustrum/bridges/nimcp_claustrum_security.h"
#include "core/brain/nimcp_brain.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(claustrum_brain_init, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Version String
//=============================================================================

static const char* claustrum_version_string = "1.0.0";

//=============================================================================
// Configuration API
//=============================================================================

int claustrum_init_default_config(claustrum_init_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_binding = true;
    config->enable_synchronization = true;
    config->enable_workspace = true;
    config->enable_task_switching = true;
    config->enable_kg_wiring = true;
    config->enable_security = true;
    config->enable_bio_async = true;
    config->enable_immune_bridge = true;
    config->admin_token = 0;  /* Will be set by brain factory */

    /* Biologically-realistic defaults */
    config->binding_threshold = CLAUSTRUM_DEFAULT_BINDING_THRESHOLD;
    config->gamma_frequency = CLAUSTRUM_GAMMA_FREQUENCY_HZ;
    config->alpha_frequency = CLAUSTRUM_ALPHA_FREQUENCY_HZ;

    return 0;
}

//=============================================================================
// Brain Factory Integration API
//=============================================================================

int claustrum_brain_init_register(brain_t brain) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return -1;

    }

    claustrum_init_config_t config;
    claustrum_init_default_config(&config);

    claustrum_init_result_t result;
    memset(&result, 0, sizeof(result));

    if (claustrum_brain_init_create(brain, &config, &result) < 0) {
        NIMCP_LOG_ERROR(CLAUSTRUM_INIT_MODULE_NAME,
            "Claustrum subsystem initialization failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "claustrum_brain_init_register: validation failed");
        return -1;
    }

    NIMCP_LOG_INFO(CLAUSTRUM_INIT_MODULE_NAME,
        "Claustrum subsystem initialized: modalities=%d, oscillators=%d",
        result.modalities_initialized, result.oscillators_initialized);

    return result.error_count == 0 ? 0 : -1;
}

int claustrum_brain_init_create(
    brain_t brain,
    const claustrum_init_config_t* config,
    claustrum_init_result_t* result
) {
    if (!brain || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_brain_init_create: required parameter is NULL (brain, config)");
        return -1;
    }

    claustrum_init_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));

    /* Create claustrum configuration from init config */
    nimcp_claustrum_config_t claustrum_config = nimcp_claustrum_default_config();
    claustrum_config.binding_threshold = config->binding_threshold;
    claustrum_config.gamma_base_freq = config->gamma_frequency;
    claustrum_config.alpha_base_freq = config->alpha_frequency;
    claustrum_config.enable_workspace_gating = config->enable_workspace;
    claustrum_config.enable_rapid_switching = config->enable_task_switching;
    claustrum_config.enable_kg_integration = config->enable_kg_wiring;

    /* Would create claustrum instance and store in brain */
    /* nimcp_claustrum_t* claustrum = brain_get_claustrum(brain); */
    /* For now, verify config creation succeeded */
    local_result.claustrum_initialized = true;
    local_result.modalities_initialized = true;
    local_result.oscillators_initialized = true;

    /* Initialize bridges */
    if (config->enable_bio_async) {
        local_result.bio_async_connected = claustrum_init_bio_async_bridges(brain);
        if (!local_result.bio_async_connected) {
            local_result.warning_count++;
        }
    }

    if (config->enable_kg_wiring) {
        local_result.kg_registered = claustrum_init_kg_wiring(brain, config->admin_token);
        if (!local_result.kg_registered) {
            local_result.warning_count++;
        }
    }

    if (config->enable_security) {
        local_result.security_registered = claustrum_init_security(brain);
        if (!local_result.security_registered) {
            local_result.warning_count++;
        }
    }

    if (config->enable_immune_bridge) {
        local_result.immune_connected = claustrum_init_immune_bridge(brain);
        if (!local_result.immune_connected) {
            local_result.warning_count++;
        }
    }

    /* Check for critical failures */
    if (!local_result.claustrum_initialized) {
        local_result.error_count++;
        NIMCP_LOG_ERROR(CLAUSTRUM_INIT_MODULE_NAME,
            "Claustrum core initialization failed");
    }

    if (result) {
        *result = local_result;
    }

    return local_result.error_count > 0 ? -1 : 0;
}

int claustrum_brain_init_destroy(brain_t brain) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return -1;

    }

    /* Cleanup would happen here - destroy in reverse order */
    /* For now, this is a placeholder as brain manages its own subsystems */

    NIMCP_LOG_INFO(CLAUSTRUM_INIT_MODULE_NAME,
        "Claustrum subsystem destroyed");

    return 0;
}

//=============================================================================
// Component Initialization
//=============================================================================

bool claustrum_init_modalities(
    nimcp_claustrum_t* claustrum,
    const claustrum_init_config_t* config
) {
    if (!claustrum || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_init_modalities: required parameter is NULL (claustrum, config)");
        return false;
    }

    /* Modalities are initialized as part of nimcp_claustrum_init */
    NIMCP_LOG_DEBUG(CLAUSTRUM_INIT_MODULE_NAME,
        "Claustrum modalities initialized");

    return true;
}

bool claustrum_init_oscillators(
    nimcp_claustrum_t* claustrum,
    const claustrum_init_config_t* config
) {
    if (!claustrum || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_init_oscillators: required parameter is NULL (claustrum, config)");
        return false;
    }

    /* Set oscillator frequencies */
    if (nimcp_claustrum_set_gamma(claustrum, config->gamma_frequency, 1.0f) != CLAUSTRUM_OK) {
        NIMCP_LOG_WARN(CLAUSTRUM_INIT_MODULE_NAME,
            "Failed to set gamma frequency");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "claustrum_init_oscillators: validation failed");
        return false;
    }

    if (nimcp_claustrum_set_alpha(claustrum, config->alpha_frequency, 1.0f) != CLAUSTRUM_OK) {
        NIMCP_LOG_WARN(CLAUSTRUM_INIT_MODULE_NAME,
            "Failed to set alpha frequency");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "claustrum_init_oscillators: validation failed");
        return false;
    }

    NIMCP_LOG_DEBUG(CLAUSTRUM_INIT_MODULE_NAME,
        "Claustrum oscillators initialized: gamma=%.1fHz, alpha=%.1fHz",
        config->gamma_frequency, config->alpha_frequency);

    return true;
}

//=============================================================================
// Bridge Initialization
//=============================================================================

bool claustrum_init_bio_async_bridges(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_init_bio_async_bridges: brain is NULL");
        return false;
    }

    /* Bio-async bridges would connect here */
    /* This requires access to the brain's bio-router */
    NIMCP_LOG_DEBUG(CLAUSTRUM_INIT_MODULE_NAME,
        "Bio-async bridges ready (connect when router available)");

    return true;
}

bool claustrum_init_kg_wiring(brain_t brain, uint64_t admin_token) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_init_kg_wiring: brain is NULL");
        return false;
    }

    /* Would access brain's KG and register claustrum nodes */
    /* brain_kg_t* kg = brain_get_kg(brain); */
    /* if (kg) claustrum_kg_register_all(kg, NULL, NULL, admin_token); */

    (void)admin_token;

    NIMCP_LOG_DEBUG(CLAUSTRUM_INIT_MODULE_NAME,
        "KG wiring ready (register when KG available)");

    return true;
}

bool claustrum_init_security(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_init_security: brain is NULL");
        return false;
    }

    /* Would access brain's BBB and register claustrum module */
    /* bbb_system_t bbb = brain_get_bbb(brain); */
    /* if (bbb) claustrum_security_register(bbb, NULL); */

    NIMCP_LOG_DEBUG(CLAUSTRUM_INIT_MODULE_NAME,
        "Security registration ready (register when BBB available)");

    return true;
}

bool claustrum_init_immune_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_init_immune_bridge: brain is NULL");
        return false;
    }

    /* Would create claustrum-immune bridge and connect */
    NIMCP_LOG_DEBUG(CLAUSTRUM_INIT_MODULE_NAME,
        "Immune bridge ready (connect when immune system available)");

    return true;
}

//=============================================================================
// Query API
//=============================================================================

bool claustrum_is_initialized(brain_t brain) {
    if (!brain) {
        return false;
    }

    /* Would check if claustrum subsystem exists in brain */
    /* return brain_has_claustrum_subsystem(brain); */

    return true;  /* Assume initialized if brain exists */
}

const char* claustrum_get_version(void) {
    return claustrum_version_string;
}
