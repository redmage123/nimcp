//=============================================================================
// nimcp_pag_security.c - PAG Security Registration (BBB Integration)
//=============================================================================
/**
 * @file nimcp_pag_security.c
 * @brief Implementation of PAG module BBB registration
 */

#include "core/brain/regions/pag/bridges/nimcp_pag_security.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pag_security, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Map operation to required roles
 */
static uint32_t op_to_roles(pag_security_op_t op) {
    switch (op) {
        case PAG_SEC_OP_READ_STATE:
        case PAG_SEC_OP_READ_DEFENSE:
        case PAG_SEC_OP_READ_PAIN:
            return PAG_ROLE_STATE_READ;

        case PAG_SEC_OP_READ_PARAMS:
            return PAG_ROLE_PARAM_READ;

        case PAG_SEC_OP_WRITE_STATE:
            return PAG_ROLE_STATE_WRITE;

        case PAG_SEC_OP_WRITE_PARAMS:
            return PAG_ROLE_PARAM_WRITE;

        case PAG_SEC_OP_WRITE_DEFENSE:
            return PAG_ROLE_STATE_WRITE | PAG_ROLE_DEFENSE_CONTROL;

        case PAG_SEC_OP_WRITE_PAIN:
            return PAG_ROLE_STATE_WRITE | PAG_ROLE_PAIN_MODULATE;

        case PAG_SEC_OP_RESET:
            return PAG_ROLE_STATE_WRITE | PAG_ROLE_PARAM_WRITE;

        case PAG_SEC_OP_UPDATE:
        case PAG_SEC_OP_PROCESS_THREAT:
        case PAG_SEC_OP_PROCESS_PAIN:
            return PAG_ROLE_COMPUTE;

        case PAG_SEC_OP_BROADCAST:
        case PAG_SEC_OP_SUBSCRIBE:
            return PAG_ROLE_BROADCAST;

        case PAG_SEC_OP_KG_REGISTER:
        case PAG_SEC_OP_KG_UPDATE:
            return PAG_ROLE_STATE_WRITE;

        default:
            return 0;
    }
}

/**
 * @brief Map operation to required privilege level
 */
static uint32_t op_to_privilege(pag_security_op_t op) {
    switch (op) {
        case PAG_SEC_OP_READ_STATE:
        case PAG_SEC_OP_READ_DEFENSE:
        case PAG_SEC_OP_READ_PAIN:
        case PAG_SEC_OP_READ_PARAMS:
            return 1;  /* Low privilege for reads */

        case PAG_SEC_OP_UPDATE:
        case PAG_SEC_OP_PROCESS_THREAT:
        case PAG_SEC_OP_PROCESS_PAIN:
        case PAG_SEC_OP_BROADCAST:
        case PAG_SEC_OP_SUBSCRIBE:
            return 2;  /* Moderate privilege for compute/comms */

        case PAG_SEC_OP_WRITE_STATE:
        case PAG_SEC_OP_WRITE_PARAMS:
        case PAG_SEC_OP_WRITE_DEFENSE:
        case PAG_SEC_OP_WRITE_PAIN:
        case PAG_SEC_OP_RESET:
        case PAG_SEC_OP_KG_REGISTER:
        case PAG_SEC_OP_KG_UPDATE:
            return 3;  /* Higher privilege for writes */

        default:
            return 0;
    }
}

//=============================================================================
// Configuration API
//=============================================================================

int pag_security_default_config(pag_security_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->bbb = NULL;
    config->admin_token = 0;
    config->strict_mode = false;
    config->log_access = true;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int pag_security_register(
    bbb_system_t bbb,
    pag_security_state_t* state
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    pag_security_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create PAG subject with appropriate privileges */
    bbb_subject_t pag_subject = {
        .id = PAG_MODULE_ID,
        .privilege_level = PAG_PRIVILEGE_LEVEL,
        .roles = PAG_DEFAULT_ROLES,
        .capabilities = PAG_DEFAULT_CAPS
    };

    if (!bbb_register_subject(bbb, &pag_subject)) {
        NIMCP_LOG_ERROR(PAG_SECURITY_MODULE_NAME,
            "Failed to register PAG subject with BBB");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pag_security_register: bbb_register_subject is NULL");
        return -1;
    }

    local_state.pag_subject = pag_subject;
    local_state.registered = true;

    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(PAG_SECURITY_MODULE_NAME,
        "Registered PAG module with BBB (id=0x%x)", PAG_MODULE_ID);

    return 0;
}

int pag_security_unregister(
    bbb_system_t bbb,
    pag_security_state_t* state
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

    NIMCP_LOG_INFO(PAG_SECURITY_MODULE_NAME,
        "Unregistered PAG module from BBB");

    return 0;
}

int pag_security_register_memory(
    bbb_system_t bbb,
    void* address,
    size_t size,
    pag_security_state_t* state
) {
    if (!bbb || !address || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_security_register_memory: required parameter is NULL (bbb, address)");
        return -1;
    }

    uint32_t region_id = bbb_register_memory_region(bbb, address, size, false);
    if (region_id == 0) {
        NIMCP_LOG_ERROR(PAG_SECURITY_MODULE_NAME,
            "Failed to register PAG memory region");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pag_security_register_memory: region_id is zero");
        return -1;
    }

    if (state) {
        state->memory_region_id = region_id;

        /* Create object for memory region */
        state->pag_memory.id = region_id;
        state->pag_memory.required_privilege = PAG_PRIVILEGE_LEVEL;
        state->pag_memory.required_roles = PAG_ROLE_STATE_WRITE;
        state->pag_memory.required_capabilities = PAG_CAP_DEFENSE_MODIFY;

        bbb_register_object(bbb, &state->pag_memory);
    }

    NIMCP_LOG_DEBUG(PAG_SECURITY_MODULE_NAME,
        "Registered PAG memory region: %p size=%zu id=%u",
        address, size, region_id);

    return 0;
}

//=============================================================================
// Access Control API
//=============================================================================

bool pag_security_check_access(
    bbb_system_t bbb,
    pag_security_op_t op
) {
    if (!bbb) {
        return false;
    }

    /* Create subject from PAG module */
    bbb_subject_t subject = {
        .id = PAG_MODULE_ID,
        .privilege_level = PAG_PRIVILEGE_LEVEL,
        .roles = PAG_DEFAULT_ROLES,
        .capabilities = PAG_DEFAULT_CAPS
    };

    /* Create object from operation requirements */
    bbb_object_t object = {
        .id = (uint32_t)op,
        .required_privilege = op_to_privilege(op),
        .required_roles = op_to_roles(op),
        .required_capabilities = 0
    };

    /* Determine access type based on operation */
    uint32_t access_type = 1;  /* Default: read */
    if (op >= PAG_SEC_OP_WRITE_STATE && op <= PAG_SEC_OP_RESET) {
        access_type = 2;  /* Write */
    } else if (op >= PAG_SEC_OP_UPDATE && op <= PAG_SEC_OP_PROCESS_PAIN) {
        access_type = 4;  /* Execute */
    }

    return bbb_check_access(bbb, &subject, &object, access_type);
}

bool pag_security_has_capability(
    bbb_system_t bbb,
    uint64_t capability
) {
    (void)bbb;

    /* Check against PAG default capabilities */
    return (PAG_DEFAULT_CAPS & capability) == capability;
}

bool pag_security_validate_kg_token(
    const pag_security_state_t* state,
    uint64_t token
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_security_validate_kg_token: state is NULL");
        return false;
    }
    if (!state->registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_security_validate_kg_token: state->registered is NULL");
        return false;
    }
    if (state->admin_token == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pag_security_validate_kg_token: state->admin_token is zero");
        return false;
    }

    return state->admin_token == token;
}

int pag_security_grant_capability(
    bbb_system_t bbb,
    uint64_t capability
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    if (!bbb_grant_capability(bbb, PAG_MODULE_ID, capability)) {
        NIMCP_LOG_WARN(PAG_SECURITY_MODULE_NAME,
            "Failed to grant capability 0x%llx to PAG",
            (unsigned long long)capability);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pag_security_grant_capability: bbb_grant_capability is NULL");
        return -1;
    }

    return 0;
}

int pag_security_revoke_capability(
    bbb_system_t bbb,
    uint64_t capability
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    if (!bbb_revoke_capability(bbb, PAG_MODULE_ID, capability)) {
        NIMCP_LOG_WARN(PAG_SECURITY_MODULE_NAME,
            "Failed to revoke capability 0x%llx from PAG",
            (unsigned long long)capability);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pag_security_revoke_capability: bbb_revoke_capability is NULL");
        return -1;
    }

    return 0;
}

//=============================================================================
// Immune Integration API
//=============================================================================

int pag_security_connect_immune(
    bbb_system_t bbb,
    void* immune,
    pag_security_state_t* state
) {
    if (!bbb || !immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pag_security_connect_immune: required parameter is NULL (bbb, immune)");
        return -1;
    }

    /* Connect BBB to immune system */
    if (!bbb_connect_immune(bbb, (brain_immune_system_t*)immune)) {
        NIMCP_LOG_WARN(PAG_SECURITY_MODULE_NAME,
            "Failed to connect PAG security to immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pag_security_connect_immune: bbb_connect_immune is NULL");
        return -1;
    }

    if (state) {
        state->immune_connected = true;
    }

    NIMCP_LOG_INFO(PAG_SECURITY_MODULE_NAME,
        "Connected PAG security to immune system");

    return 0;
}
