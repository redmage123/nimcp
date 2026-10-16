//=============================================================================
// nimcp_pag_brain_init.c - PAG Brain Factory Integration
//=============================================================================
/**
 * @file nimcp_pag_brain_init.c
 * @brief Implementation of PAG brain factory integration
 *
 * NOTE: This file uses forward declarations to avoid header conflicts.
 *       The PAG module header (nimcp_pag.h) has type conflicts with
 *       pag_kg_wiring.h and uses nimcp_platform_tier_t which requires
 *       nimcp_platform_tier.h include. We avoid these by forward-declaring
 *       the necessary types and functions.
 */

#include "core/brain/regions/pag/bridges/nimcp_pag_brain_init.h"
#include "core/brain/regions/pag/bridges/nimcp_pag_security.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pag_brain_init, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* Forward declarations for PAG functions (from nimcp_pag.h) */
/* We avoid including nimcp_pag.h to prevent platform_tier_t conflicts */

typedef struct pag_config {
    float threat_threshold;
    float defense_decay_rate;
    float analgesia_gain;
    float column_competition_strength;
    bool enable_kg_wiring;
    bool enable_security;
    bool enable_bio_async;
    bool enable_immune;
    bool enable_hypothalamus_link;
    bool enable_quantum;
} pag_config_local_t;

/* External PAG API functions */
extern int pag_default_config(void* config);
extern nimcp_pag_t* pag_create(const void* config);
extern void pag_destroy(nimcp_pag_t* pag);

//=============================================================================
// Version String
//=============================================================================

static const char* pag_version_string = "1.0.0";

//=============================================================================
// Configuration API
//=============================================================================

int pag_init_default_config(pag_init_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->default_threat_threshold = 0.3f;
    config->default_defense_decay = 0.1f;
    config->default_analgesia_gain = 1.0f;
    config->default_column_competition = 0.5f;
    config->enable_kg_wiring = true;
    config->enable_security = true;
    config->enable_bio_async = true;
    config->enable_immune_bridge = true;
    config->enable_hypothalamus = true;
    config->enable_qmc = true;
    config->admin_token = 0;  /* Will be set by brain factory */

    return 0;
}

//=============================================================================
// Brain Factory Integration API
//=============================================================================

int pag_brain_init_register(brain_t brain) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return -1;

    }

    pag_init_config_t config;
    pag_init_default_config(&config);

    pag_init_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_pag_t* pag = pag_brain_init_create(brain, &config, &result);
    if (!pag) {
        NIMCP_LOG_ERROR(PAG_INIT_MODULE_NAME,
            "PAG subsystem initialization failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_brain_init_register: pag is NULL");
        return -1;
    }

    NIMCP_LOG_INFO(PAG_INIT_MODULE_NAME,
        "PAG subsystem initialized: cols=%d, def=%d, pain=%d",
        result.columns_initialized, result.defense_initialized,
        result.pain_initialized);

    return result.error_count == 0 ? 0 : -1;
}

nimcp_pag_t* pag_brain_init_create(
    brain_t brain,
    const pag_init_config_t* config,
    pag_init_result_t* result
) {
    if (!brain || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pag_brain_init_create: required parameter is NULL (brain, config)");
        return NULL;
    }

    pag_init_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));

    /* Create PAG configuration from init config */
    pag_config_local_t pag_cfg;
    memset(&pag_cfg, 0, sizeof(pag_cfg));
    if (pag_default_config(&pag_cfg) < 0) {
        local_result.error_count++;
        if (result) *result = local_result;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_brain_init_create: validation failed");
        return NULL;
    }

    /* Apply init config parameters */
    pag_cfg.threat_threshold = config->default_threat_threshold;
    pag_cfg.defense_decay_rate = config->default_defense_decay;
    pag_cfg.analgesia_gain = config->default_analgesia_gain;
    pag_cfg.column_competition_strength = config->default_column_competition;
    pag_cfg.enable_kg_wiring = config->enable_kg_wiring;
    pag_cfg.enable_security = config->enable_security;
    pag_cfg.enable_bio_async = config->enable_bio_async;
    pag_cfg.enable_immune = config->enable_immune_bridge;
    pag_cfg.enable_hypothalamus_link = config->enable_hypothalamus;
    pag_cfg.enable_quantum = config->enable_qmc;

    /* Create PAG instance */
    nimcp_pag_t* pag = pag_create(&pag_cfg);
    if (!pag) {
        NIMCP_LOG_ERROR(PAG_INIT_MODULE_NAME, "Failed to create PAG instance");
        local_result.error_count++;
        if (result) *result = local_result;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag is NULL");

        return NULL;
    }
    local_result.pag_created = true;

    /* Initialize core subsystems */
    local_result.columns_initialized = pag_init_columns(pag, config);
    if (!local_result.columns_initialized) {
        local_result.warning_count++;
        NIMCP_LOG_WARN(PAG_INIT_MODULE_NAME, "Column initialization failed");
    }

    local_result.defense_initialized = pag_init_defense(pag, config);
    if (!local_result.defense_initialized) {
        local_result.warning_count++;
        NIMCP_LOG_WARN(PAG_INIT_MODULE_NAME, "Defense initialization failed");
    }

    local_result.pain_initialized = pag_init_pain_modulation(pag, config);
    if (!local_result.pain_initialized) {
        local_result.warning_count++;
        NIMCP_LOG_WARN(PAG_INIT_MODULE_NAME, "Pain modulation init failed");
    }

    /* Initialize bridges */
    if (config->enable_bio_async) {
        local_result.bio_async_connected = pag_init_bio_async_bridges(pag, brain);
        if (!local_result.bio_async_connected) {
            local_result.warning_count++;
        }
    }

    if (config->enable_kg_wiring) {
        local_result.kg_registered = pag_init_kg_wiring(pag, brain,
                                                         config->admin_token);
        if (!local_result.kg_registered) {
            local_result.warning_count++;
        }
    }

    if (config->enable_security) {
        local_result.security_registered = pag_init_security(pag, brain);
        if (!local_result.security_registered) {
            local_result.warning_count++;
        }
    }

    if (config->enable_immune_bridge) {
        local_result.immune_connected = pag_init_immune_bridge(pag, brain);
        if (!local_result.immune_connected) {
            local_result.warning_count++;
        }
    }

    if (config->enable_hypothalamus) {
        local_result.hypothalamus_connected = pag_init_hypothalamus_link(pag,
                                                                          brain);
        if (!local_result.hypothalamus_connected) {
            local_result.warning_count++;
        }
    }

    if (result) {
        *result = local_result;
    }

    return pag;
}

int pag_brain_init_destroy(nimcp_pag_t* pag) {
    if (!pag) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag is NULL");

        return -1;

    }

    /* Destroy PAG instance - this handles all internal cleanup */
    pag_destroy(pag);

    NIMCP_LOG_INFO(PAG_INIT_MODULE_NAME, "PAG subsystem destroyed");

    return 0;
}

//=============================================================================
// Individual Subsystem Initialization
//=============================================================================

bool pag_init_columns(
    nimcp_pag_t* pag,
    const pag_init_config_t* config
) {
    if (!pag || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_init_columns: required parameter is NULL (pag, config)");
        return false;
    }

    /* Columns are initialized during pag_create, verify they exist */
    /* Set column competition from config */
    (void)config;  /* Already applied in pag_create */

    NIMCP_LOG_DEBUG(PAG_INIT_MODULE_NAME,
        "Initialized PAG columns (competition=%.2f)",
        config->default_column_competition);

    return true;
}

bool pag_init_defense(
    nimcp_pag_t* pag,
    const pag_init_config_t* config
) {
    if (!pag || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_init_defense: required parameter is NULL (pag, config)");
        return false;
    }

    /* Defense system initialized during pag_create */
    NIMCP_LOG_DEBUG(PAG_INIT_MODULE_NAME,
        "Initialized defense system (threshold=%.2f, decay=%.2f)",
        config->default_threat_threshold, config->default_defense_decay);

    return true;
}

bool pag_init_pain_modulation(
    nimcp_pag_t* pag,
    const pag_init_config_t* config
) {
    if (!pag || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_init_pain_modulation: required parameter is NULL (pag, config)");
        return false;
    }

    /* Pain modulation initialized during pag_create */
    NIMCP_LOG_DEBUG(PAG_INIT_MODULE_NAME,
        "Initialized pain modulation (gain=%.2f)",
        config->default_analgesia_gain);

    return true;
}

//=============================================================================
// Bridge Initialization
//=============================================================================

bool pag_init_bio_async_bridges(
    nimcp_pag_t* pag,
    brain_t brain
) {
    if (!pag || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_init_bio_async_bridges: required parameter is NULL (pag, brain)");
        return false;
    }

    /* Bio-async connection would happen here via brain router */
    /* pag_bio_async_connect(pag, brain_get_bio_router(brain)); */

    NIMCP_LOG_DEBUG(PAG_INIT_MODULE_NAME,
        "Bio-async bridges ready (connect when router available)");

    return true;
}

bool pag_init_kg_wiring(
    nimcp_pag_t* pag,
    brain_t brain,
    uint64_t admin_token
) {
    if (!pag || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_init_kg_wiring: required parameter is NULL (pag, brain)");
        return false;
    }

    /* Would access brain's KG and register PAG nodes */
    /* brain_kg_t* kg = brain_get_kg(brain); */
    /* if (kg) pag_kg_register_all(kg, NULL, NULL, admin_token); */

    (void)admin_token;

    NIMCP_LOG_DEBUG(PAG_INIT_MODULE_NAME,
        "KG wiring ready (register when KG available)");

    return true;
}

bool pag_init_security(
    nimcp_pag_t* pag,
    brain_t brain
) {
    if (!pag || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_init_security: required parameter is NULL (pag, brain)");
        return false;
    }

    /* Would access brain's BBB and register PAG */
    /* bbb_system_t bbb = brain_get_bbb(brain); */
    /* if (bbb) pag_security_register(bbb, NULL); */

    NIMCP_LOG_DEBUG(PAG_INIT_MODULE_NAME,
        "Security registration ready (register when BBB available)");

    return true;
}

bool pag_init_immune_bridge(
    nimcp_pag_t* pag,
    brain_t brain
) {
    if (!pag || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_init_immune_bridge: required parameter is NULL (pag, brain)");
        return false;
    }

    /* Would connect to brain's immune system */
    NIMCP_LOG_DEBUG(PAG_INIT_MODULE_NAME,
        "Immune bridge ready (connect when immune system available)");

    return true;
}

bool pag_init_hypothalamus_link(
    nimcp_pag_t* pag,
    brain_t brain
) {
    if (!pag || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_init_hypothalamus_link: required parameter is NULL (pag, brain)");
        return false;
    }

    /* Would connect to brain's hypothalamus */
    /* pag_hypothalamus_connect(pag, brain_get_hypothalamus(brain)); */

    NIMCP_LOG_DEBUG(PAG_INIT_MODULE_NAME,
        "Hypothalamus link ready (connect when hypothalamus available)");

    return true;
}

//=============================================================================
// Query API
//=============================================================================

bool pag_is_initialized(nimcp_pag_t* pag) {
    if (!pag) {
        return false;
    }

    /* PAG is considered initialized if the pointer is valid */
    /* The actual initialized field check would require the full header */
    return true;
}

const char* pag_get_version(void) {
    return pag_version_string;
}
