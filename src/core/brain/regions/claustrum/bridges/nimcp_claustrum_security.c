//=============================================================================
// nimcp_claustrum_security.c - Claustrum Security Registration
//=============================================================================
/**
 * @file nimcp_claustrum_security.c
 * @brief Implementation of claustrum BBB registration
 */

#include "core/brain/regions/claustrum/bridges/nimcp_claustrum_security.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(claustrum_security, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Map operation to required roles
 */
static uint32_t op_to_roles(claustrum_security_op_t op) {
    switch (op) {
        case CLAUSTRUM_SEC_OP_READ_STATE:
        case CLAUSTRUM_SEC_OP_READ_MODALITY:
        case CLAUSTRUM_SEC_OP_READ_BINDING:
        case CLAUSTRUM_SEC_OP_READ_WORKSPACE:
            return CLAUSTRUM_ROLE_STATE_READ;

        case CLAUSTRUM_SEC_OP_WRITE_STATE:
            return CLAUSTRUM_ROLE_STATE_WRITE;

        case CLAUSTRUM_SEC_OP_WRITE_MODALITY:
            return CLAUSTRUM_ROLE_MODALITY_WRITE;

        case CLAUSTRUM_SEC_OP_WRITE_BINDING:
        case CLAUSTRUM_SEC_OP_WRITE_WORKSPACE:
            return CLAUSTRUM_ROLE_STATE_WRITE | CLAUSTRUM_ROLE_WORKSPACE_ACCESS;

        case CLAUSTRUM_SEC_OP_UPDATE:
        case CLAUSTRUM_SEC_OP_SYNCHRONIZE:
        case CLAUSTRUM_SEC_OP_SWITCH_STATE:
            return CLAUSTRUM_ROLE_COMPUTE;

        case CLAUSTRUM_SEC_OP_BROADCAST:
        case CLAUSTRUM_SEC_OP_GATE_ACCESS:
            return CLAUSTRUM_ROLE_BROADCAST | CLAUSTRUM_ROLE_WORKSPACE_ACCESS;

        case CLAUSTRUM_SEC_OP_KG_REGISTER:
        case CLAUSTRUM_SEC_OP_KG_UPDATE:
            return CLAUSTRUM_ROLE_STATE_WRITE;

        default:
            return 0;
    }
}

/**
 * @brief Map operation to required privilege
 */
static uint32_t op_to_privilege(claustrum_security_op_t op) {
    switch (op) {
        case CLAUSTRUM_SEC_OP_READ_STATE:
        case CLAUSTRUM_SEC_OP_READ_MODALITY:
        case CLAUSTRUM_SEC_OP_READ_BINDING:
        case CLAUSTRUM_SEC_OP_READ_WORKSPACE:
            return 1;  /* Low privilege for reads */

        case CLAUSTRUM_SEC_OP_UPDATE:
        case CLAUSTRUM_SEC_OP_SYNCHRONIZE:
        case CLAUSTRUM_SEC_OP_BROADCAST:
            return 2;  /* Moderate privilege for compute/comms */

        case CLAUSTRUM_SEC_OP_WRITE_STATE:
        case CLAUSTRUM_SEC_OP_WRITE_MODALITY:
        case CLAUSTRUM_SEC_OP_WRITE_BINDING:
        case CLAUSTRUM_SEC_OP_WRITE_WORKSPACE:
        case CLAUSTRUM_SEC_OP_SWITCH_STATE:
        case CLAUSTRUM_SEC_OP_GATE_ACCESS:
        case CLAUSTRUM_SEC_OP_KG_REGISTER:
        case CLAUSTRUM_SEC_OP_KG_UPDATE:
            return 3;  /* Higher privilege for writes */

        default:
            return 0;
    }
}

//=============================================================================
// Configuration API
//=============================================================================

int claustrum_security_default_config(claustrum_security_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->strict_mode = false;
    config->enable_kg_write = true;
    config->enable_workspace_gate = true;
    config->admin_token = 0;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int claustrum_security_register(
    bbb_system_t bbb,
    claustrum_security_state_t* state
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    claustrum_security_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Initialize default config */
    claustrum_security_default_config(&local_state.config);

    /* Create claustrum subject */
    bbb_subject_t subject = {
        .id = CLAUSTRUM_MODULE_ID,
        .privilege_level = CLAUSTRUM_PRIVILEGE_LEVEL,
        .roles = CLAUSTRUM_DEFAULT_ROLES,
        .capabilities = CLAUSTRUM_DEFAULT_CAPS
    };

    if (!bbb_register_subject(bbb, &subject)) {
        NIMCP_LOG_ERROR(CLAUSTRUM_SECURITY_MODULE_NAME,
            "Failed to register claustrum subject");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "claustrum_security_register: bbb_register_subject is NULL");
        return -1;
    }

    local_state.claustrum_subject = subject;
    local_state.registered = true;

    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(CLAUSTRUM_SECURITY_MODULE_NAME,
        "Registered claustrum module with BBB (id=0x%x)", CLAUSTRUM_MODULE_ID);

    return 0;
}

int claustrum_security_unregister(
    bbb_system_t bbb,
    claustrum_security_state_t* state
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

    NIMCP_LOG_INFO(CLAUSTRUM_SECURITY_MODULE_NAME,
        "Unregistered claustrum module from BBB");

    return 0;
}

int claustrum_security_register_memory(
    bbb_system_t bbb,
    void* address,
    size_t size,
    claustrum_security_state_t* state
) {
    if (!bbb || !address || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_security_register_memory: required parameter is NULL (bbb, address)");
        return -1;
    }

    uint32_t region_id = bbb_register_memory_region(bbb, address, size, true);
    if (region_id == 0) {
        NIMCP_LOG_ERROR(CLAUSTRUM_SECURITY_MODULE_NAME,
            "Failed to register memory region");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "claustrum_security_register_memory: region_id is zero");
        return -1;
    }

    if (state) {
        state->memory_region_id = region_id;

        /* Create object for memory region */
        state->claustrum_memory.id = region_id;
        state->claustrum_memory.required_privilege = CLAUSTRUM_PRIVILEGE_LEVEL;
        state->claustrum_memory.required_roles = CLAUSTRUM_ROLE_STATE_WRITE;
        state->claustrum_memory.required_capabilities = CLAUSTRUM_CAP_BINDING_MODIFY;

        bbb_register_object(bbb, &state->claustrum_memory);
    }

    NIMCP_LOG_DEBUG(CLAUSTRUM_SECURITY_MODULE_NAME,
        "Registered memory region: %p size=%zu id=%u",
        address, size, region_id);

    return 0;
}

//=============================================================================
// Access Control API
//=============================================================================

bool claustrum_security_check_access(
    bbb_system_t bbb,
    claustrum_security_op_t op
) {
    if (!bbb) {
        return false;
    }

    /* Create subject from claustrum module */
    bbb_subject_t subject = {
        .id = CLAUSTRUM_MODULE_ID,
        .privilege_level = CLAUSTRUM_PRIVILEGE_LEVEL,
        .roles = CLAUSTRUM_DEFAULT_ROLES,
        .capabilities = CLAUSTRUM_DEFAULT_CAPS
    };

    /* Create object from operation requirements */
    bbb_object_t object = {
        .id = (uint32_t)op,
        .required_privilege = op_to_privilege(op),
        .required_roles = op_to_roles(op),
        .required_capabilities = 0
    };

    /* Determine access type */
    uint32_t access_type = 0;
    if (op >= CLAUSTRUM_SEC_OP_WRITE_STATE && op <= CLAUSTRUM_SEC_OP_WRITE_WORKSPACE) {
        access_type = 2;  /* Write */
    } else if (op >= CLAUSTRUM_SEC_OP_UPDATE && op <= CLAUSTRUM_SEC_OP_SWITCH_STATE) {
        access_type = 4;  /* Execute */
    } else {
        access_type = 1;  /* Read */
    }

    return bbb_check_access(bbb, &subject, &object, access_type);
}

bool claustrum_security_validate_kg_token(
    const claustrum_security_state_t* state,
    uint64_t token
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_security_validate_kg_token: state is NULL");
        return false;
    }

    /* Token 0 is invalid unless explicitly allowed */
    if (token == 0 && state->config.admin_token != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "claustrum_security_validate_kg_token: token is zero");
        return false;
    }

    /* Check against stored admin token */
    if (state->config.admin_token != 0) {
        return token == state->config.admin_token;
    }

    /* No admin token set - allow if KG write enabled */
    return state->config.enable_kg_write;
}

bool claustrum_security_has_capability(
    bbb_system_t bbb,
    uint64_t capability
) {
    (void)bbb;

    /* Check against default claustrum capabilities */
    return (CLAUSTRUM_DEFAULT_CAPS & capability) == capability;
}

//=============================================================================
// Immune Integration API
//=============================================================================

int claustrum_security_connect_immune(
    bbb_system_t bbb,
    void* immune,
    claustrum_security_state_t* state
) {
    if (!bbb || !immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claustrum_security_connect_immune: required parameter is NULL (bbb, immune)");
        return -1;
    }

    /* Connect BBB to immune system */
    if (!bbb_connect_immune(bbb, (brain_immune_system_t*)immune)) {
        NIMCP_LOG_WARN(CLAUSTRUM_SECURITY_MODULE_NAME,
            "Failed to connect BBB to immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "claustrum_security_connect_immune: bbb_connect_immune is NULL");
        return -1;
    }

    if (state) {
        state->immune_connected = true;
    }

    NIMCP_LOG_INFO(CLAUSTRUM_SECURITY_MODULE_NAME,
        "Connected claustrum security to immune system");

    return 0;
}
