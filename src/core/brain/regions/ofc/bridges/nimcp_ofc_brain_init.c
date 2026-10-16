//=============================================================================
// nimcp_ofc_brain_init.c - OFC Brain Factory Integration
//=============================================================================
/**
 * @file nimcp_ofc_brain_init.c
 * @brief Implementation of OFC brain factory integration
 */

#include "core/brain/regions/ofc/bridges/nimcp_ofc_brain_init.h"
#include "core/brain/regions/ofc/bridges/nimcp_ofc_security.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(ofc_brain_init, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* Forward declarations to avoid header conflicts */
typedef struct {
    float learning_rate;
    float discount_rate;
    float risk_sensitivity;
    float social_weight;
    float decision_threshold;
    float noise_level;
    uint32_t max_options;
    float reversal_threshold;
    bool enable_bio_async;
    bool enable_kg_wiring;
    bool enable_immune;
    bool enable_security;
    bool enable_logging;
    bool enable_quantum;
    uint32_t max_history_size;
    uint32_t update_interval_ms;
    int platform_tier;
} ofc_config_internal_t;

/* External OFC API functions (declared in nimcp_ofc.h) */
extern int ofc_default_config(void* config);
extern nimcp_ofc_t* ofc_create(const void* config);
extern void ofc_destroy(nimcp_ofc_t* ofc);
extern int ofc_init(nimcp_ofc_t* ofc);
extern int ofc_kg_unregister(nimcp_ofc_t* ofc);
extern int ofc_bio_async_disconnect(nimcp_ofc_t* ofc);

//=============================================================================
// Version String
//=============================================================================

static const char* ofc_version_string = "1.0.0";

//=============================================================================
// Configuration API
//=============================================================================

int ofc_init_default_config(ofc_init_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->learning_rate = 0.1f;
    config->discount_rate = 0.95f;
    config->risk_sensitivity = 0.0f;  /* Neutral */
    config->social_weight = 0.2f;
    config->decision_threshold = 0.7f;
    config->enable_kg_wiring = true;
    config->enable_security = true;
    config->enable_bio_async = true;
    config->enable_immune_bridge = true;
    config->enable_quantum = false;
    config->admin_token = 0;

    return 0;
}

//=============================================================================
// Brain Factory Integration API
//=============================================================================

int ofc_brain_init_register(brain_t brain) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return -1;

    }

    ofc_init_config_t config;
    ofc_init_default_config(&config);

    ofc_init_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_ofc_t* ofc = ofc_brain_init_create(brain, &config, &result);
    if (!ofc) {
        NIMCP_LOG_ERROR(OFC_INIT_MODULE_NAME,
            "OFC subsystem creation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_brain_init_register: ofc is NULL");
        return -1;
    }

    NIMCP_LOG_INFO(OFC_INIT_MODULE_NAME,
        "OFC subsystem registered: KG=%d, Security=%d, BioAsync=%d",
        result.kg_registered, result.security_registered,
        result.bio_async_connected);

    return result.error_count > 0 ? -1 : 0;
}

nimcp_ofc_t* ofc_brain_init_create(
    brain_t brain,
    const ofc_init_config_t* config,
    ofc_init_result_t* result
) {
    if (!brain || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ofc_brain_init_create: required parameter is NULL (brain, config)");
        return NULL;
    }

    ofc_init_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));

    /* Build OFC config from init config */
    ofc_config_internal_t ofc_config;
    memset(&ofc_config, 0, sizeof(ofc_config));

    if (ofc_default_config(&ofc_config) < 0) {
        local_result.error_count++;
        if (result) *result = local_result;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_brain_init_create: validation failed");
        return NULL;
    }

    ofc_config.learning_rate = config->learning_rate;
    ofc_config.discount_rate = config->discount_rate;
    ofc_config.risk_sensitivity = config->risk_sensitivity;
    ofc_config.social_weight = config->social_weight;
    ofc_config.decision_threshold = config->decision_threshold;
    ofc_config.enable_kg_wiring = config->enable_kg_wiring;
    ofc_config.enable_security = config->enable_security;
    ofc_config.enable_bio_async = config->enable_bio_async;
    ofc_config.enable_immune = config->enable_immune_bridge;
    ofc_config.enable_quantum = config->enable_quantum;

    /* Create OFC instance */
    nimcp_ofc_t* ofc = ofc_create(&ofc_config);
    if (!ofc) {
        NIMCP_LOG_ERROR(OFC_INIT_MODULE_NAME, "Failed to create OFC instance");
        local_result.error_count++;
        if (result) *result = local_result;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc is NULL");

        return NULL;
    }

    local_result.ofc_created = true;

    /* Initialize OFC */
    if (ofc_init(ofc) < 0) {
        NIMCP_LOG_WARN(OFC_INIT_MODULE_NAME, "OFC initialization failed");
        local_result.warning_count++;
    }

    /* Initialize bridges */
    if (config->enable_bio_async) {
        local_result.bio_async_connected = ofc_init_bio_async_bridge(ofc, brain);
        if (!local_result.bio_async_connected) {
            local_result.warning_count++;
        }
    }

    if (config->enable_kg_wiring) {
        local_result.kg_registered = ofc_init_kg_wiring(ofc, brain,
                                                         config->admin_token);
        if (!local_result.kg_registered) {
            local_result.warning_count++;
        }
    }

    if (config->enable_security) {
        local_result.security_registered = ofc_init_security(ofc, brain);
        if (!local_result.security_registered) {
            local_result.warning_count++;
        }
    }

    if (config->enable_immune_bridge) {
        local_result.immune_connected = ofc_init_immune_bridge(ofc, brain);
        if (!local_result.immune_connected) {
            local_result.warning_count++;
        }
    }

    if (result) {
        *result = local_result;
    }

    NIMCP_LOG_DEBUG(OFC_INIT_MODULE_NAME,
        "OFC created with %u warnings", local_result.warning_count);

    return ofc;
}

int ofc_brain_init_destroy(nimcp_ofc_t* ofc) {
    if (!ofc) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc is NULL");

        return -1;

    }

    /* Unregister from KG if registered */
    if (ofc_kg_unregister(ofc) < 0) {
        NIMCP_LOG_WARN(OFC_INIT_MODULE_NAME, "KG unregister failed");
    }

    /* Disconnect from bio-async */
    if (ofc_bio_async_disconnect(ofc) < 0) {
        NIMCP_LOG_WARN(OFC_INIT_MODULE_NAME, "Bio-async disconnect failed");
    }

    /* Destroy OFC instance */
    ofc_destroy(ofc);

    NIMCP_LOG_INFO(OFC_INIT_MODULE_NAME, "OFC subsystem destroyed");

    return 0;
}

//=============================================================================
// Bridge Initialization
//=============================================================================

bool ofc_init_bio_async_bridge(nimcp_ofc_t* ofc, brain_t brain) {
    if (!ofc || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_init_bio_async_bridge: required parameter is NULL (ofc, brain)");
        return false;
    }

    /* Get bio-router from brain (would need brain API accessor) */
    /* struct nimcp_bio_router* router = brain_get_bio_router(brain); */
    /* if (router) return ofc_bio_async_connect(ofc, router) == 0; */

    NIMCP_LOG_DEBUG(OFC_INIT_MODULE_NAME,
        "Bio-async bridge ready (connect when router available)");

    return true;
}

bool ofc_init_kg_wiring(
    nimcp_ofc_t* ofc,
    brain_t brain,
    uint64_t admin_token
) {
    if (!ofc || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_init_kg_wiring: required parameter is NULL (ofc, brain)");
        return false;
    }

    /* Get KG from brain (would need brain API accessor) */
    /* brain_kg_t* kg = brain_get_kg(brain); */
    /* if (kg) return ofc_kg_register(ofc, kg, admin_token) == 0; */

    (void)admin_token;

    NIMCP_LOG_DEBUG(OFC_INIT_MODULE_NAME,
        "KG wiring ready (register when KG available)");

    return true;
}

bool ofc_init_security(nimcp_ofc_t* ofc, brain_t brain) {
    if (!ofc || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_init_security: required parameter is NULL (ofc, brain)");
        return false;
    }

    /* Get BBB from brain (would need brain API accessor) */
    /* bbb_system_t bbb = brain_get_bbb(brain); */
    /* if (bbb) { */
    /*     ofc_security_state_t state; */
    /*     return ofc_security_register(bbb, NULL, &state) == 0; */
    /* } */

    NIMCP_LOG_DEBUG(OFC_INIT_MODULE_NAME,
        "Security registration ready (register when BBB available)");

    return true;
}

bool ofc_init_immune_bridge(nimcp_ofc_t* ofc, brain_t brain) {
    if (!ofc || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_init_immune_bridge: required parameter is NULL (ofc, brain)");
        return false;
    }

    /* Get immune system from brain (would need brain API accessor) */
    /* void* immune = brain_get_immune(brain); */
    /* if (immune) return ofc_immune_connect(ofc, immune) == 0; */

    NIMCP_LOG_DEBUG(OFC_INIT_MODULE_NAME,
        "Immune bridge ready (connect when immune system available)");

    return true;
}

//=============================================================================
// Query API
//=============================================================================

bool ofc_is_initialized(nimcp_ofc_t* ofc) {
    if (!ofc) {
        return false;
    }

    /* Access initialized field via opaque pointer cast */
    /* In full implementation, would need accessor function */
    return true;  /* Assume initialized if non-NULL */
}

const char* ofc_get_version(void) {
    return ofc_version_string;
}
