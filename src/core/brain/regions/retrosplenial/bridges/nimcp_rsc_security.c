//=============================================================================
// nimcp_rsc_security.c - Retrosplenial Cortex Security Registration
//=============================================================================
/**
 * @file nimcp_rsc_security.c
 * @brief Implementation of RSC BBB registration
 */

#include "core/brain/regions/retrosplenial/bridges/nimcp_rsc_security.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(rsc_security, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Map operation to required roles
 */
static uint32_t op_to_roles(rsc_security_op_t op) {
    switch (op) {
        case RSC_SEC_OP_READ_STATE:
        case RSC_SEC_OP_READ_NAVIGATION:
        case RSC_SEC_OP_READ_CONTEXT:
            return RSC_ROLE_STATE_READ;

        case RSC_SEC_OP_READ_PARAMS:
            return RSC_ROLE_PARAM_READ;

        case RSC_SEC_OP_WRITE_STATE:
        case RSC_SEC_OP_WRITE_NAVIGATION:
        case RSC_SEC_OP_WRITE_CONTEXT:
            return RSC_ROLE_STATE_WRITE;

        case RSC_SEC_OP_WRITE_PARAMS:
            return RSC_ROLE_PARAM_WRITE;

        case RSC_SEC_OP_RESET:
            return RSC_ROLE_STATE_WRITE | RSC_ROLE_PARAM_WRITE;

        case RSC_SEC_OP_KG_READ:
            return RSC_ROLE_KG_READ;

        case RSC_SEC_OP_KG_REGISTER:
        case RSC_SEC_OP_KG_UPDATE:
            return RSC_ROLE_KG_WRITE;

        default:
            return 0;
    }
}

/**
 * @brief Map operation to required privilege
 */
static uint32_t op_to_privilege(rsc_security_op_t op) {
    switch (op) {
        case RSC_SEC_OP_READ_STATE:
        case RSC_SEC_OP_READ_NAVIGATION:
        case RSC_SEC_OP_READ_CONTEXT:
        case RSC_SEC_OP_READ_PARAMS:
        case RSC_SEC_OP_KG_READ:
            return 1;  /* Low privilege for reads */

        case RSC_SEC_OP_WRITE_STATE:
        case RSC_SEC_OP_WRITE_NAVIGATION:
        case RSC_SEC_OP_WRITE_CONTEXT:
            return 2;  /* Moderate privilege for writes */

        case RSC_SEC_OP_WRITE_PARAMS:
        case RSC_SEC_OP_RESET:
        case RSC_SEC_OP_KG_REGISTER:
        case RSC_SEC_OP_KG_UPDATE:
            return 3;  /* Higher privilege for admin ops */

        default:
            return 0;
    }
}

//=============================================================================
// Configuration API
//=============================================================================

int rsc_security_default_config(rsc_security_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->module_id = RSC_MODULE_ID;
    config->privilege_level = RSC_PRIVILEGE_LEVEL;
    config->roles = RSC_DEFAULT_ROLES;
    config->capabilities = RSC_DEFAULT_CAPS;
    config->enable_kg_write = false;
    config->admin_token = 0;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int rsc_security_register(
    bbb_system_t bbb,
    const rsc_security_config_t* config,
    rsc_security_state_t* state
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    /* Use defaults if no config */
    rsc_security_config_t local_config;
    if (!config) {
        rsc_security_default_config(&local_config);
        config = &local_config;
    }

    /* Build subject */
    bbb_subject_t subject = {
        .id = config->module_id ? config->module_id : RSC_MODULE_ID,
        .privilege_level = config->privilege_level ? config->privilege_level
                                                   : RSC_PRIVILEGE_LEVEL,
        .roles = config->roles ? config->roles : RSC_DEFAULT_ROLES,
        .capabilities = config->capabilities ? config->capabilities
                                             : RSC_DEFAULT_CAPS
    };

    /* Add KG write capability if enabled */
    if (config->enable_kg_write) {
        subject.capabilities |= RSC_CAP_KG_WRITE;
        subject.roles |= RSC_ROLE_KG_WRITE;
    }

    /* Register with BBB */
    if (!bbb_register_subject(bbb, &subject)) {
        NIMCP_LOG_ERROR(RSC_SECURITY_MODULE_NAME,
            "Failed to register RSC subject with BBB");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rsc_security_register: bbb_register_subject is NULL");
        return -1;
    }

    /* Update state if provided */
    if (state) {
        state->rsc_subject = subject;
        state->registered = true;
        state->admin_token = config->admin_token;
    }

    NIMCP_LOG_INFO(RSC_SECURITY_MODULE_NAME,
        "Registered RSC module with BBB (id=0x%x)", subject.id);

    return 0;
}

int rsc_security_unregister(
    bbb_system_t bbb,
    rsc_security_state_t* state
) {
    if (!state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state is NULL");

        return -1;

    }

    /* Unregister memory region if registered */
    if (state->memory_region_id != 0 && bbb) {
        bbb_unregister_memory_region(bbb, state->memory_region_id);
        state->memory_region_id = 0;
    }

    /* Mark as unregistered */
    state->registered = false;
    state->immune_connected = false;

    NIMCP_LOG_INFO(RSC_SECURITY_MODULE_NAME,
        "Unregistered RSC module from BBB");

    return 0;
}

int rsc_security_register_memory(
    bbb_system_t bbb,
    void* address,
    size_t size,
    rsc_security_state_t* state
) {
    if (!bbb || !address || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_security_register_memory: required parameter is NULL (bbb, address)");
        return -1;
    }

    uint32_t region_id = bbb_register_memory_region(bbb, address, size, true);
    if (region_id == 0) {
        NIMCP_LOG_ERROR(RSC_SECURITY_MODULE_NAME,
            "Failed to register RSC memory region");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rsc_security_register_memory: region_id is zero");
        return -1;
    }

    if (state) {
        state->memory_region_id = region_id;

        /* Create object for memory region */
        state->rsc_memory.id = region_id;
        state->rsc_memory.required_privilege = RSC_PRIVILEGE_LEVEL;
        state->rsc_memory.required_roles = RSC_ROLE_STATE_WRITE;
        state->rsc_memory.required_capabilities = RSC_CAP_NAVIGATION_MODIFY;

        bbb_register_object(bbb, &state->rsc_memory);
    }

    NIMCP_LOG_DEBUG(RSC_SECURITY_MODULE_NAME,
        "Registered RSC memory region: %p size=%zu id=%u",
        address, size, region_id);

    return 0;
}

//=============================================================================
// Access Control API
//=============================================================================

bool rsc_security_check_access(
    bbb_system_t bbb,
    rsc_security_op_t op
) {
    if (!bbb) {
        return false;
    }

    /* Create subject from module_id */
    bbb_subject_t subject = {
        .id = RSC_MODULE_ID,
        .privilege_level = RSC_PRIVILEGE_LEVEL,
        .roles = RSC_DEFAULT_ROLES,
        .capabilities = RSC_DEFAULT_CAPS
    };

    /* Create object from operation requirements */
    bbb_object_t object = {
        .id = (uint32_t)op,
        .required_privilege = op_to_privilege(op),
        .required_roles = op_to_roles(op),
        .required_capabilities = 0
    };

    /* Determine access type */
    uint32_t access_type = 1;  /* Default: read */
    if (op >= RSC_SEC_OP_WRITE_STATE && op <= RSC_SEC_OP_RESET) {
        access_type = 2;  /* Write */
    }

    return bbb_check_access(bbb, &subject, &object, access_type);
}

bool rsc_security_validate_kg_token(
    const rsc_security_state_t* state,
    uint64_t token
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_security_validate_kg_token: state is NULL");
        return false;
    }
    if (state->admin_token == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rsc_security_validate_kg_token: state->admin_token is zero");
        return false;
    }

    return state->admin_token == token;
}

bool rsc_security_has_capability(
    bbb_system_t bbb,
    uint64_t capability
) {
    (void)bbb;

    uint64_t module_caps = RSC_DEFAULT_CAPS;
    return (module_caps & capability) == capability;
}

int rsc_security_grant_capability(
    bbb_system_t bbb,
    uint64_t capability
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    if (!bbb_grant_capability(bbb, RSC_MODULE_ID, capability)) {
        NIMCP_LOG_WARN(RSC_SECURITY_MODULE_NAME,
            "Failed to grant capability 0x%llx to RSC",
            (unsigned long long)capability);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rsc_security_grant_capability: bbb_grant_capability is NULL");
        return -1;
    }

    return 0;
}

int rsc_security_revoke_capability(
    bbb_system_t bbb,
    uint64_t capability
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    if (!bbb_revoke_capability(bbb, RSC_MODULE_ID, capability)) {
        NIMCP_LOG_WARN(RSC_SECURITY_MODULE_NAME,
            "Failed to revoke capability 0x%llx from RSC",
            (unsigned long long)capability);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rsc_security_revoke_capability: bbb_revoke_capability is NULL");
        return -1;
    }

    return 0;
}

//=============================================================================
// Immune Integration API
//=============================================================================

int rsc_security_connect_immune(
    bbb_system_t bbb,
    void* immune,
    rsc_security_state_t* state
) {
    if (!bbb || !immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc_security_connect_immune: required parameter is NULL (bbb, immune)");
        return -1;
    }

    /* Connect BBB to immune system */
    if (!bbb_connect_immune(bbb, (brain_immune_system_t*)immune)) {
        NIMCP_LOG_WARN(RSC_SECURITY_MODULE_NAME,
            "Failed to connect BBB to immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rsc_security_connect_immune: bbb_connect_immune is NULL");
        return -1;
    }

    if (state) {
        state->immune_connected = true;
    }

    NIMCP_LOG_INFO(RSC_SECURITY_MODULE_NAME,
        "Connected RSC security to immune system");

    return 0;
}
