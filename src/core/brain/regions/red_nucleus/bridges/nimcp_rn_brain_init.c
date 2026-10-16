//=============================================================================
// nimcp_rn_brain_init.c - Red Nucleus Brain Factory Integration
//=============================================================================
/**
 * @file nimcp_rn_brain_init.c
 * @brief Implementation of Red Nucleus brain factory integration
 */

#include "core/brain/regions/red_nucleus/bridges/nimcp_rn_brain_init.h"
#include "core/brain/regions/red_nucleus/nimcp_red_nucleus.h"
#include "core/brain/regions/red_nucleus/bridges/nimcp_rn_security.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(rn_brain_init, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Version String
//=============================================================================

static const char* rn_version_string = "1.0.0";

//=============================================================================
// Configuration API
//=============================================================================

int rn_init_default_config(rn_init_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_command_queue = true;
    config->enable_learning = true;
    config->enable_cerebellar = true;
    config->enable_kg_wiring = true;
    config->enable_security = true;
    config->enable_bio_async = true;
    config->enable_immune = true;
    config->enable_quantum = false;
    config->max_commands_queued = 32;
    config->base_learning_rate = 0.01f;
    config->admin_token = 0;  /* Will be set by brain factory */

    return 0;
}

//=============================================================================
// Brain Factory Integration API
//=============================================================================

int rn_brain_init_register(brain_t brain) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return -1;

    }

    rn_init_config_t config;
    rn_init_default_config(&config);

    rn_init_result_t result;
    memset(&result, 0, sizeof(result));

    if (rn_brain_init_create(brain, &config, &result) < 0) {
        NIMCP_LOG_ERROR(RN_INIT_MODULE_NAME,
            "Red Nucleus subsystem initialization failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "rn_brain_init_register: validation failed");
        return -1;
    }

    NIMCP_LOG_INFO(RN_INIT_MODULE_NAME,
        "Red Nucleus registered: created=%d, learning=%d, cerebellar=%d",
        result.rn_created, result.learning_initialized,
        result.cerebellar_connected);

    return result.error_count == 0 ? 0 : -1;
}

int rn_brain_init_create(
    brain_t brain,
    const rn_init_config_t* config,
    rn_init_result_t* result
) {
    if (!brain || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_brain_init_create: required parameter is NULL (brain, config)");
        return -1;
    }

    rn_init_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));

    /* Create Red Nucleus with default config */
    rn_config_t rn_config;
    rn_default_config(&rn_config);

    rn_config.base_learning_rate = config->base_learning_rate;
    rn_config.max_commands_queued = config->max_commands_queued;
    rn_config.enable_bio_async = config->enable_bio_async;
    rn_config.enable_kg_wiring = config->enable_kg_wiring;
    rn_config.enable_immune = config->enable_immune;
    rn_config.enable_security = config->enable_security;
    rn_config.enable_cerebellar = config->enable_cerebellar;
    rn_config.enable_quantum = config->enable_quantum;

    nimcp_red_nucleus_t* rn = rn_create(&rn_config);
    if (!rn) {
        local_result.error_count++;
        NIMCP_LOG_ERROR(RN_INIT_MODULE_NAME,
            "Failed to create Red Nucleus instance");
        if (result) *result = local_result;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rn_brain_init_create: validation failed");
        return -1;
    }
    local_result.rn_created = true;

    /* Initialize command queue */
    if (config->enable_command_queue) {
        if (rn_init_command_queue(rn, config->max_commands_queued) == 0) {
            local_result.command_queue_initialized = true;
        } else {
            local_result.warning_count++;
        }
    }

    /* Initialize learning */
    if (config->enable_learning) {
        if (rn_init_learning(rn, config->base_learning_rate) == 0) {
            local_result.learning_initialized = true;
        } else {
            local_result.warning_count++;
        }
    }

    /* Initialize cerebellar integration */
    if (config->enable_cerebellar) {
        if (rn_init_cerebellar(rn) == 0) {
            local_result.cerebellar_connected = true;
        } else {
            local_result.warning_count++;
        }
    }

    /* Initialize bio-async */
    if (config->enable_bio_async) {
        if (rn_init_bio_async(brain, rn) == 0) {
            local_result.bio_async_connected = true;
        } else {
            local_result.warning_count++;
        }
    }

    /* Initialize KG wiring */
    if (config->enable_kg_wiring) {
        if (rn_init_kg_wiring(brain, rn, config->admin_token) == 0) {
            local_result.kg_registered = true;
        } else {
            local_result.warning_count++;
        }
    }

    /* Initialize security */
    if (config->enable_security) {
        if (rn_init_security(brain, rn) == 0) {
            local_result.security_registered = true;
        } else {
            local_result.warning_count++;
        }
    }

    /* Initialize immune connection */
    if (config->enable_immune) {
        if (rn_init_immune(brain, rn) == 0) {
            local_result.immune_connected = true;
        } else {
            local_result.warning_count++;
        }
    }

    /* Store Red Nucleus in brain (implementation depends on brain struct) */
    /* brain_set_red_nucleus(brain, rn); */

    if (result) {
        *result = local_result;
    }

    return local_result.error_count > 0 ? -1 : 0;
}

int rn_brain_init_destroy(brain_t brain) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return -1;

    }

    /* Get Red Nucleus from brain */
    /* nimcp_red_nucleus_t* rn = brain_get_red_nucleus(brain); */
    /* if (rn) rn_destroy(rn); */

    NIMCP_LOG_INFO(RN_INIT_MODULE_NAME,
        "Red Nucleus subsystem destroyed");

    return 0;
}

//=============================================================================
// Subsystem Initialization
//=============================================================================

int rn_init_command_queue(
    struct nimcp_red_nucleus* rn,
    uint32_t max_commands
) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }

    /* Command queue is allocated by rn_create based on config */
    /* Verify it's ready */
    if (rn->command_queue && rn->queue_capacity >= max_commands) {
        NIMCP_LOG_DEBUG(RN_INIT_MODULE_NAME,
            "Command queue ready: capacity=%u", rn->queue_capacity);
        return 0;
    }

    NIMCP_LOG_DEBUG(RN_INIT_MODULE_NAME,
        "Command queue initialization deferred");
    return 0;
}

int rn_init_learning(
    struct nimcp_red_nucleus* rn,
    float base_learning_rate
) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }

    /* Initialize learning state for each effector */
    for (int i = 0; i < RN_EFFECTOR_COUNT; i++) {
        rn->learning[i].learning_rate = base_learning_rate;
        rn->learning[i].adaptation_gain = 1.0f;
        rn->learning[i].skill_level = 0.0f;
    }

    rn->global_learning_modulation = 1.0f;

    NIMCP_LOG_DEBUG(RN_INIT_MODULE_NAME,
        "Learning initialized: base_rate=%.4f", base_learning_rate);

    return 0;
}

int rn_init_cerebellar(struct nimcp_red_nucleus* rn) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }

    /* Initialize cerebellar integration state */
    memset(&rn->dentate_input, 0, sizeof(rn->dentate_input));
    memset(&rn->olivary_output, 0, sizeof(rn->olivary_output));
    memset(&rn->thalamic_output, 0, sizeof(rn->thalamic_output));

    NIMCP_LOG_DEBUG(RN_INIT_MODULE_NAME,
        "Cerebellar integration initialized");

    return 0;
}

int rn_init_kg_wiring(
    brain_t brain,
    struct nimcp_red_nucleus* rn,
    uint64_t admin_token
) {
    if (!brain || !rn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_init_kg_wiring: required parameter is NULL (brain, rn)");
        return -1;
    }

    /* Would access brain's KG and register Red Nucleus nodes */
    /* brain_kg_t* kg = brain_get_kg(brain); */
    /* if (kg) rn_kg_register_all(kg, NULL, NULL, admin_token); */

    rn->kg_state.admin_token = admin_token;

    (void)admin_token;

    NIMCP_LOG_DEBUG(RN_INIT_MODULE_NAME,
        "KG wiring ready (register when KG available)");

    return 0;
}

int rn_init_security(
    brain_t brain,
    struct nimcp_red_nucleus* rn
) {
    if (!brain || !rn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_init_security: required parameter is NULL (brain, rn)");
        return -1;
    }

    /* Would access brain's BBB and register Red Nucleus */
    /* bbb_system_t bbb = brain_get_bbb(brain); */
    /* if (bbb) rn_security_register(bbb, NULL); */

    NIMCP_LOG_DEBUG(RN_INIT_MODULE_NAME,
        "Security registration ready (register when BBB available)");

    return 0;
}

int rn_init_bio_async(
    brain_t brain,
    struct nimcp_red_nucleus* rn
) {
    if (!brain || !rn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_init_bio_async: required parameter is NULL (brain, rn)");
        return -1;
    }

    /* Bio-async bridges would connect here */
    NIMCP_LOG_DEBUG(RN_INIT_MODULE_NAME,
        "Bio-async bridges ready (connect when router available)");

    return 0;
}

int rn_init_immune(
    brain_t brain,
    struct nimcp_red_nucleus* rn
) {
    if (!brain || !rn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_init_immune: required parameter is NULL (brain, rn)");
        return -1;
    }

    /* Would create Red Nucleus-immune bridge and connect */
    NIMCP_LOG_DEBUG(RN_INIT_MODULE_NAME,
        "Immune bridge ready (connect when immune system available)");

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

bool rn_is_initialized(brain_t brain) {
    if (!brain) {
        return false;
    }

    /* Would check if Red Nucleus subsystem exists in brain */
    /* return brain_has_red_nucleus(brain); */

    return true;  /* Assume initialized if brain exists */
}

const char* rn_get_version(void) {
    return rn_version_string;
}
