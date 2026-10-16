//=============================================================================
// nimcp_reticular_security.c - Reticular Formation Security Registration
//=============================================================================
/**
 * @file nimcp_reticular_security.c
 * @brief Implementation of reticular formation BBB registration
 */

#include "core/brain/regions/reticular/bridges/nimcp_reticular_security.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(reticular_security, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Map operation to required roles
 */
static uint32_t op_to_roles(reticular_security_op_t op) {
    switch (op) {
        case RETICULAR_SEC_OP_READ_STATE:
        case RETICULAR_SEC_OP_READ_STATS:
            return RETICULAR_ROLE_STATE_READ;

        case RETICULAR_SEC_OP_READ_AROUSAL:
            return RETICULAR_ROLE_AROUSAL_READ;

        case RETICULAR_SEC_OP_READ_MODULATORS:
            return RETICULAR_ROLE_MODULATOR_READ;

        case RETICULAR_SEC_OP_READ_AUTONOMIC:
            return RETICULAR_ROLE_STATE_READ;

        case RETICULAR_SEC_OP_WRITE_STATE:
        case RETICULAR_SEC_OP_RESET:
            return RETICULAR_ROLE_STATE_WRITE;

        case RETICULAR_SEC_OP_WRITE_AROUSAL:
            return RETICULAR_ROLE_AROUSAL_WRITE;

        case RETICULAR_SEC_OP_WRITE_MODULATORS:
            return RETICULAR_ROLE_MODULATOR_WRITE;

        case RETICULAR_SEC_OP_WRITE_AUTONOMIC:
            return RETICULAR_ROLE_STATE_WRITE;

        case RETICULAR_SEC_OP_UPDATE:
        case RETICULAR_SEC_OP_COMPUTE_AROUSAL:
        case RETICULAR_SEC_OP_TRIGGER_REFLEX:
            return RETICULAR_ROLE_COMPUTE;

        case RETICULAR_SEC_OP_BROADCAST:
        case RETICULAR_SEC_OP_SUBSCRIBE:
            return RETICULAR_ROLE_BROADCAST;

        case RETICULAR_SEC_OP_KG_REGISTER:
        case RETICULAR_SEC_OP_KG_UPDATE:
            return RETICULAR_ROLE_STATE_WRITE;

        default:
            return 0;
    }
}

/**
 * @brief Map operation to required privilege
 */
static uint32_t op_to_privilege(reticular_security_op_t op) {
    switch (op) {
        case RETICULAR_SEC_OP_READ_STATE:
        case RETICULAR_SEC_OP_READ_STATS:
        case RETICULAR_SEC_OP_READ_AROUSAL:
        case RETICULAR_SEC_OP_READ_MODULATORS:
        case RETICULAR_SEC_OP_READ_AUTONOMIC:
            return 1;  /* Low privilege for reads */

        case RETICULAR_SEC_OP_UPDATE:
        case RETICULAR_SEC_OP_COMPUTE_AROUSAL:
        case RETICULAR_SEC_OP_BROADCAST:
        case RETICULAR_SEC_OP_SUBSCRIBE:
            return 2;  /* Moderate privilege for compute/comms */

        case RETICULAR_SEC_OP_WRITE_STATE:
        case RETICULAR_SEC_OP_WRITE_AUTONOMIC:
        case RETICULAR_SEC_OP_TRIGGER_REFLEX:
        case RETICULAR_SEC_OP_KG_REGISTER:
        case RETICULAR_SEC_OP_KG_UPDATE:
            return 3;  /* High privilege for writes */

        case RETICULAR_SEC_OP_WRITE_AROUSAL:
        case RETICULAR_SEC_OP_WRITE_MODULATORS:
        case RETICULAR_SEC_OP_RESET:
            return 4;  /* Highest privilege for arousal/modulator changes */

        default:
            return 0;
    }
}

//=============================================================================
// Configuration API
//=============================================================================

int reticular_security_default_config(reticular_security_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->strict_mode = false;
    config->enable_audit_log = true;
    config->require_kg_token = true;
    config->admin_token = 0;
    config->arousal_min_privilege = 3;
    config->modulator_min_privilege = 3;

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int reticular_security_register(
    bbb_system_t bbb,
    reticular_security_state_t* state
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    reticular_security_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Create subject for reticular formation */
    local_state.reticular_subject.id = RETICULAR_MODULE_ID;
    local_state.reticular_subject.privilege_level = RETICULAR_PRIVILEGE_LEVEL;
    local_state.reticular_subject.roles = RETICULAR_DEFAULT_ROLES;
    local_state.reticular_subject.capabilities = RETICULAR_DEFAULT_CAPS;

    if (!bbb_register_subject(bbb, &local_state.reticular_subject)) {
        NIMCP_LOG_ERROR(RETICULAR_SECURITY_MODULE_NAME,
            "Failed to register reticular subject with BBB");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reticular_security_register: bbb_register_subject is NULL");
        return -1;
    }

    local_state.registered = true;

    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(RETICULAR_SECURITY_MODULE_NAME,
        "Registered reticular formation with BBB (id=0x%x)", RETICULAR_MODULE_ID);

    return 0;
}

int reticular_security_unregister(
    bbb_system_t bbb,
    reticular_security_state_t* state
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

    NIMCP_LOG_INFO(RETICULAR_SECURITY_MODULE_NAME,
        "Unregistered reticular formation from BBB");

    return 0;
}

int reticular_security_register_memory(
    bbb_system_t bbb,
    void* address,
    size_t size,
    reticular_security_state_t* state
) {
    if (!bbb || !address || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_security_register_memory: required parameter is NULL (bbb, address)");
        return -1;
    }

    uint32_t region_id = bbb_register_memory_region(bbb, address, size, true);
    if (region_id == 0) {
        NIMCP_LOG_ERROR(RETICULAR_SECURITY_MODULE_NAME,
            "Failed to register reticular memory region");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reticular_security_register_memory: region_id is zero");
        return -1;
    }

    if (state) {
        state->memory_region_id = region_id;

        /* Create object for memory region */
        state->reticular_memory.id = region_id;
        state->reticular_memory.required_privilege = RETICULAR_PRIVILEGE_LEVEL;
        state->reticular_memory.required_roles = RETICULAR_ROLE_STATE_WRITE;
        state->reticular_memory.required_capabilities = RETICULAR_CAP_AROUSAL_MODIFY;

        bbb_register_object(bbb, &state->reticular_memory);
    }

    NIMCP_LOG_DEBUG(RETICULAR_SECURITY_MODULE_NAME,
        "Registered memory region: %p size=%zu id=%u", address, size, region_id);

    return 0;
}

//=============================================================================
// Access Control API
//=============================================================================

bool reticular_security_check_access(
    bbb_system_t bbb,
    reticular_security_op_t op
) {
    if (!bbb) {
        return false;
    }

    /* Create subject from reticular module */
    bbb_subject_t subject = {
        .id = RETICULAR_MODULE_ID,
        .privilege_level = RETICULAR_PRIVILEGE_LEVEL,
        .roles = RETICULAR_DEFAULT_ROLES,
        .capabilities = RETICULAR_DEFAULT_CAPS
    };

    /* Create object from operation requirements */
    bbb_object_t object = {
        .id = (uint32_t)op,
        .required_privilege = op_to_privilege(op),
        .required_roles = op_to_roles(op),
        .required_capabilities = 0
    };

    /* Determine access type */
    uint32_t access_type = 1;  /* Default read */
    if (op >= RETICULAR_SEC_OP_WRITE_STATE && op <= RETICULAR_SEC_OP_RESET) {
        access_type = 2;  /* Write */
    } else if (op >= RETICULAR_SEC_OP_UPDATE && op <= RETICULAR_SEC_OP_TRIGGER_REFLEX) {
        access_type = 4;  /* Execute */
    }

    return bbb_check_access(bbb, &subject, &object, access_type);
}

bool reticular_security_has_capability(
    bbb_system_t bbb,
    uint64_t capability
) {
    (void)bbb;
    /* Check against default capabilities */
    return (RETICULAR_DEFAULT_CAPS & capability) == capability;
}

bool reticular_security_validate_kg_token(
    const reticular_security_state_t* state,
    uint64_t token
) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_security_validate_kg_token: state is NULL");
        return false;
    }
    if (!state->registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_security_validate_kg_token: state->registered is NULL");
        return false;
    }
    if (state->admin_token == 0) return true;  /* No token required */
    return state->admin_token == token;
}

int reticular_security_grant_capability(
    bbb_system_t bbb,
    uint64_t capability
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    if (!bbb_grant_capability(bbb, RETICULAR_MODULE_ID, capability)) {
        NIMCP_LOG_WARN(RETICULAR_SECURITY_MODULE_NAME,
            "Failed to grant capability 0x%llx", (unsigned long long)capability);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reticular_security_grant_capability: bbb_grant_capability is NULL");
        return -1;
    }

    return 0;
}

int reticular_security_revoke_capability(
    bbb_system_t bbb,
    uint64_t capability
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    if (!bbb_revoke_capability(bbb, RETICULAR_MODULE_ID, capability)) {
        NIMCP_LOG_WARN(RETICULAR_SECURITY_MODULE_NAME,
            "Failed to revoke capability 0x%llx", (unsigned long long)capability);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reticular_security_revoke_capability: bbb_revoke_capability is NULL");
        return -1;
    }

    return 0;
}

//=============================================================================
// Immune Integration API
//=============================================================================

int reticular_security_connect_immune(
    bbb_system_t bbb,
    void* immune,
    reticular_security_state_t* state
) {
    if (!bbb || !immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reticular_security_connect_immune: required parameter is NULL (bbb, immune)");
        return -1;
    }

    /* Connect BBB to immune system */
    if (!bbb_connect_immune(bbb, (brain_immune_system_t*)immune)) {
        NIMCP_LOG_WARN(RETICULAR_SECURITY_MODULE_NAME,
            "Failed to connect BBB to immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reticular_security_connect_immune: bbb_connect_immune is NULL");
        return -1;
    }

    if (state) {
        state->immune_connected = true;
    }

    NIMCP_LOG_INFO(RETICULAR_SECURITY_MODULE_NAME,
        "Connected reticular security to immune system");

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

const char* reticular_security_op_name(reticular_security_op_t op) {
    switch (op) {
        case RETICULAR_SEC_OP_NONE: return "NONE";
        case RETICULAR_SEC_OP_READ_STATE: return "READ_STATE";
        case RETICULAR_SEC_OP_READ_AROUSAL: return "READ_AROUSAL";
        case RETICULAR_SEC_OP_READ_MODULATORS: return "READ_MODULATORS";
        case RETICULAR_SEC_OP_READ_AUTONOMIC: return "READ_AUTONOMIC";
        case RETICULAR_SEC_OP_READ_STATS: return "READ_STATS";
        case RETICULAR_SEC_OP_WRITE_STATE: return "WRITE_STATE";
        case RETICULAR_SEC_OP_WRITE_AROUSAL: return "WRITE_AROUSAL";
        case RETICULAR_SEC_OP_WRITE_MODULATORS: return "WRITE_MODULATORS";
        case RETICULAR_SEC_OP_WRITE_AUTONOMIC: return "WRITE_AUTONOMIC";
        case RETICULAR_SEC_OP_RESET: return "RESET";
        case RETICULAR_SEC_OP_UPDATE: return "UPDATE";
        case RETICULAR_SEC_OP_COMPUTE_AROUSAL: return "COMPUTE_AROUSAL";
        case RETICULAR_SEC_OP_TRIGGER_REFLEX: return "TRIGGER_REFLEX";
        case RETICULAR_SEC_OP_BROADCAST: return "BROADCAST";
        case RETICULAR_SEC_OP_SUBSCRIBE: return "SUBSCRIBE";
        case RETICULAR_SEC_OP_KG_REGISTER: return "KG_REGISTER";
        case RETICULAR_SEC_OP_KG_UPDATE: return "KG_UPDATE";
        default: return "UNKNOWN";
    }
}
