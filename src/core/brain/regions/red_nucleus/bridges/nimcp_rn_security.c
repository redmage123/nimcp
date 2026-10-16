//=============================================================================
// nimcp_rn_security.c - Red Nucleus Security Registration
//=============================================================================
/**
 * @file nimcp_rn_security.c
 * @brief Implementation of Red Nucleus BBB registration
 */

#include "core/brain/regions/red_nucleus/bridges/nimcp_rn_security.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(rn_security, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Map operation to required roles
 */
static uint32_t op_to_roles(rn_security_op_t op) {
    switch (op) {
        case RN_SEC_OP_READ_STATE:
        case RN_SEC_OP_READ_MOTOR:
        case RN_SEC_OP_READ_LEARNING:
        case RN_SEC_OP_KG_READ:
            return RN_ROLE_STATE_READ;

        case RN_SEC_OP_WRITE_STATE:
        case RN_SEC_OP_WRITE_MOTOR:
        case RN_SEC_OP_WRITE_LEARNING:
        case RN_SEC_OP_RESET:
        case RN_SEC_OP_KG_WRITE:
            return RN_ROLE_STATE_WRITE;

        case RN_SEC_OP_ISSUE_CMD:
        case RN_SEC_OP_ABORT_CMD:
        case RN_SEC_OP_TRAJECTORY:
            return RN_ROLE_MOTOR_CMD;

        default:
            return 0;
    }
}

/**
 * @brief Map operation to required privilege
 */
static uint32_t op_to_privilege(rn_security_op_t op) {
    switch (op) {
        case RN_SEC_OP_READ_STATE:
        case RN_SEC_OP_READ_MOTOR:
        case RN_SEC_OP_READ_LEARNING:
        case RN_SEC_OP_KG_READ:
            return 1;  /* Low privilege for reads */

        case RN_SEC_OP_ISSUE_CMD:
        case RN_SEC_OP_TRAJECTORY:
            return 2;  /* Moderate privilege for motor ops */

        case RN_SEC_OP_WRITE_STATE:
        case RN_SEC_OP_WRITE_MOTOR:
        case RN_SEC_OP_WRITE_LEARNING:
        case RN_SEC_OP_ABORT_CMD:
        case RN_SEC_OP_RESET:
        case RN_SEC_OP_KG_WRITE:
            return 3;  /* Higher privilege for writes */

        default:
            return 0;
    }
}

//=============================================================================
// Configuration API
//=============================================================================

int rn_security_default_config(rn_security_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enabled = true;
    config->require_kg_token = true;
    config->privilege_level = RN_PRIVILEGE_LEVEL;
    config->roles = RN_DEFAULT_ROLES;
    config->capabilities = RN_DEFAULT_CAPS;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int rn_security_register(
    bbb_system_t bbb,
    rn_security_state_t* state
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    rn_security_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create subject for Red Nucleus */
    local_state.rn_subject.id = RN_MODULE_ID;
    local_state.rn_subject.privilege_level = RN_PRIVILEGE_LEVEL;
    local_state.rn_subject.roles = RN_DEFAULT_ROLES;
    local_state.rn_subject.capabilities = RN_DEFAULT_CAPS;

    if (!bbb_register_subject(bbb, &local_state.rn_subject)) {
        NIMCP_LOG_ERROR(RN_SECURITY_MODULE_NAME,
            "Failed to register Red Nucleus subject");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rn_security_register: bbb_register_subject is NULL");
        return -1;
    }

    local_state.registered = true;

    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(RN_SECURITY_MODULE_NAME,
        "Registered Red Nucleus with BBB (id=0x%x)", RN_MODULE_ID);

    return 0;
}

int rn_security_unregister(
    bbb_system_t bbb,
    rn_security_state_t* state
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
    state->admin_token = 0;

    NIMCP_LOG_INFO(RN_SECURITY_MODULE_NAME,
        "Unregistered Red Nucleus from BBB");

    return 0;
}

int rn_security_register_memory(
    bbb_system_t bbb,
    void* address,
    size_t size,
    rn_security_state_t* state
) {
    if (!bbb || !address || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_security_register_memory: required parameter is NULL (bbb, address)");
        return -1;
    }

    uint32_t region_id = bbb_register_memory_region(bbb, address, size, true);
    if (region_id == 0) {
        NIMCP_LOG_ERROR(RN_SECURITY_MODULE_NAME,
            "Failed to register memory region");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rn_security_register_memory: region_id is zero");
        return -1;
    }

    if (state) {
        state->memory_region_id = region_id;

        /* Create object for memory region */
        state->rn_memory.id = region_id;
        state->rn_memory.required_privilege = RN_PRIVILEGE_LEVEL;
        state->rn_memory.required_roles = RN_ROLE_STATE_WRITE;
        state->rn_memory.required_capabilities = RN_CAP_MOTOR_MODIFY;

        bbb_register_object(bbb, &state->rn_memory);
    }

    NIMCP_LOG_DEBUG(RN_SECURITY_MODULE_NAME,
        "Registered memory region: %p size=%zu id=%u",
        address, size, region_id);

    return 0;
}

//=============================================================================
// Access Control API
//=============================================================================

bool rn_security_check_access(
    bbb_system_t bbb,
    rn_security_op_t op
) {
    if (!bbb) {
        return false;
    }

    /* Create subject from Red Nucleus defaults */
    bbb_subject_t subject = {
        .id = RN_MODULE_ID,
        .privilege_level = RN_PRIVILEGE_LEVEL,
        .roles = RN_DEFAULT_ROLES,
        .capabilities = RN_DEFAULT_CAPS
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
    if (op >= RN_SEC_OP_WRITE_STATE && op <= RN_SEC_OP_RESET) {
        access_type = 2;  /* Write */
    } else if (op >= RN_SEC_OP_ISSUE_CMD && op <= RN_SEC_OP_TRAJECTORY) {
        access_type = 4;  /* Execute */
    }

    return bbb_check_access(bbb, &subject, &object, access_type);
}

bool rn_security_has_capability(
    const rn_security_state_t* state,
    uint64_t capability
) {
    if (!state || !state->registered) {
        return false;
    }

    return (state->rn_subject.capabilities & capability) == capability;
}

bool rn_security_validate_kg_token(
    const rn_security_state_t* state,
    uint64_t token
) {
    if (!state || !state->registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_security_validate_kg_token: required parameter is NULL (state, state->registered)");
        return false;
    }

    /* Token 0 is never valid */
    if (token == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rn_security_validate_kg_token: token is zero");
        return false;
    }

    /* Compare against stored admin token */
    return state->admin_token == token;
}

int rn_security_set_admin_token(
    rn_security_state_t* state,
    uint64_t token
) {
    if (!state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state is NULL");

        return -1;

    }

    state->admin_token = token;

    NIMCP_LOG_DEBUG(RN_SECURITY_MODULE_NAME,
        "Admin token set for Red Nucleus security");

    return 0;
}
