#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_physics_security.c - Physics Layer Security Registration
//=============================================================================
/**
 * @file nimcp_physics_security.c
 * @brief Implementation of physics layer BBB registration
 */

#include "physics/bridges/nimcp_physics_security.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(physics_security)

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Map operation to required roles
 */
static uint32_t op_to_roles(physics_security_op_t op) {
    switch (op) {
        case PHYSICS_SEC_OP_READ_STATE:
        case PHYSICS_SEC_OP_READ_STATS:
            return PHYSICS_ROLE_STATE_READ;

        case PHYSICS_SEC_OP_READ_PARAMS:
            return PHYSICS_ROLE_PARAM_READ;

        case PHYSICS_SEC_OP_WRITE_STATE:
            return PHYSICS_ROLE_STATE_WRITE;

        case PHYSICS_SEC_OP_WRITE_PARAMS:
            return PHYSICS_ROLE_PARAM_WRITE;

        case PHYSICS_SEC_OP_RESET:
            return PHYSICS_ROLE_STATE_WRITE | PHYSICS_ROLE_PARAM_WRITE;

        case PHYSICS_SEC_OP_UPDATE:
        case PHYSICS_SEC_OP_COMPUTE_LFP:
        case PHYSICS_SEC_OP_SYNC_PHASE:
            return PHYSICS_ROLE_COMPUTE;

        case PHYSICS_SEC_OP_BROADCAST:
        case PHYSICS_SEC_OP_SUBSCRIBE:
            return PHYSICS_ROLE_BROADCAST;

        case PHYSICS_SEC_OP_KG_REGISTER:
        case PHYSICS_SEC_OP_KG_UPDATE:
            return PHYSICS_ROLE_STATE_WRITE;

        default:
            return 0;
    }
}

/**
 * @brief Map operation to required privilege
 */
static uint32_t op_to_privilege(physics_security_op_t op) {
    switch (op) {
        case PHYSICS_SEC_OP_READ_STATE:
        case PHYSICS_SEC_OP_READ_STATS:
        case PHYSICS_SEC_OP_READ_PARAMS:
            return 1;  /* Low privilege for reads */

        case PHYSICS_SEC_OP_UPDATE:
        case PHYSICS_SEC_OP_COMPUTE_LFP:
        case PHYSICS_SEC_OP_SYNC_PHASE:
        case PHYSICS_SEC_OP_BROADCAST:
        case PHYSICS_SEC_OP_SUBSCRIBE:
            return 2;  /* Moderate privilege for compute/comms */

        case PHYSICS_SEC_OP_WRITE_STATE:
        case PHYSICS_SEC_OP_WRITE_PARAMS:
        case PHYSICS_SEC_OP_RESET:
        case PHYSICS_SEC_OP_KG_REGISTER:
        case PHYSICS_SEC_OP_KG_UPDATE:
            return 3;  /* Higher privilege for writes */

        default:
            return 0;
    }
}

//=============================================================================
// Registration API
//=============================================================================

int physics_security_register_all(
    bbb_system_t bbb,
    physics_security_state_t* state
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    physics_security_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Register each module */
    if (physics_security_register_hh(bbb, &local_state.hh_subject) < 0) {
        NIMCP_LOG_WARN(PHYSICS_SECURITY_MODULE_NAME, "Failed to register HH");
    }

    if (physics_security_register_thermo(bbb, &local_state.thermo_subject) < 0) {
        NIMCP_LOG_WARN(PHYSICS_SECURITY_MODULE_NAME, "Failed to register Thermo");
    }

    if (physics_security_register_ephaptic(bbb, &local_state.ephaptic_subject) < 0) {
        NIMCP_LOG_WARN(PHYSICS_SECURITY_MODULE_NAME, "Failed to register Ephaptic");
    }

    local_state.registered = true;

    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(PHYSICS_SECURITY_MODULE_NAME,
        "Registered physics modules with BBB");

    return 0;
}

int physics_security_register_hh(
    bbb_system_t bbb,
    bbb_subject_t* subject
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    bbb_subject_t local_subject = {
        .id = PHYSICS_MODULE_ID_HH,
        .privilege_level = PHYSICS_PRIVILEGE_LEVEL,
        .roles = PHYSICS_DEFAULT_ROLES,
        .capabilities = PHYSICS_DEFAULT_CAPS | PHYSICS_CAP_MEMBRANE_MODIFY
    };

    if (!bbb_register_subject(bbb, &local_subject)) {
        NIMCP_LOG_ERROR(PHYSICS_SECURITY_MODULE_NAME,
            "Failed to register HH subject");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_security_register_hh: bbb_register_subject is NULL");
        return -1;
    }

    if (subject) {
        *subject = local_subject;
    }

    NIMCP_LOG_DEBUG(PHYSICS_SECURITY_MODULE_NAME,
        "Registered HH module (id=0x%x)", PHYSICS_MODULE_ID_HH);

    return 0;
}

int physics_security_register_thermo(
    bbb_system_t bbb,
    bbb_subject_t* subject
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    bbb_subject_t local_subject = {
        .id = PHYSICS_MODULE_ID_THERMO,
        .privilege_level = PHYSICS_PRIVILEGE_LEVEL,
        .roles = PHYSICS_DEFAULT_ROLES,
        .capabilities = PHYSICS_DEFAULT_CAPS | PHYSICS_CAP_TEMPERATURE_MODIFY |
                       PHYSICS_CAP_ENERGY_MODIFY
    };

    if (!bbb_register_subject(bbb, &local_subject)) {
        NIMCP_LOG_ERROR(PHYSICS_SECURITY_MODULE_NAME,
            "Failed to register Thermo subject");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_security_register_thermo: bbb_register_subject is NULL");
        return -1;
    }

    if (subject) {
        *subject = local_subject;
    }

    NIMCP_LOG_DEBUG(PHYSICS_SECURITY_MODULE_NAME,
        "Registered Thermo module (id=0x%x)", PHYSICS_MODULE_ID_THERMO);

    return 0;
}

int physics_security_register_ephaptic(
    bbb_system_t bbb,
    bbb_subject_t* subject
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    bbb_subject_t local_subject = {
        .id = PHYSICS_MODULE_ID_EPHAPTIC,
        .privilege_level = PHYSICS_PRIVILEGE_LEVEL,
        .roles = PHYSICS_DEFAULT_ROLES,
        .capabilities = PHYSICS_DEFAULT_CAPS | PHYSICS_CAP_FIELD_MODIFY
    };

    if (!bbb_register_subject(bbb, &local_subject)) {
        NIMCP_LOG_ERROR(PHYSICS_SECURITY_MODULE_NAME,
            "Failed to register Ephaptic subject");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_security_register_ephaptic: bbb_register_subject is NULL");
        return -1;
    }

    if (subject) {
        *subject = local_subject;
    }

    NIMCP_LOG_DEBUG(PHYSICS_SECURITY_MODULE_NAME,
        "Registered Ephaptic module (id=0x%x)", PHYSICS_MODULE_ID_EPHAPTIC);

    return 0;
}

int physics_security_register_memory(
    bbb_system_t bbb,
    void* address,
    size_t size,
    physics_security_state_t* state
) {
    if (!bbb || !address || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_security_register_memory: required parameter is NULL (bbb, address)");
        return -1;
    }

    uint32_t region_id = bbb_register_memory_region(bbb, address, size, true);
    if (region_id == 0) {
        NIMCP_LOG_ERROR(PHYSICS_SECURITY_MODULE_NAME,
            "Failed to register memory region");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_security_register_memory: region_id is zero");
        return -1;
    }

    if (state) {
        state->memory_region_id = region_id;

        /* Create object for memory region */
        state->physics_memory.id = region_id;
        state->physics_memory.required_privilege = PHYSICS_PRIVILEGE_LEVEL;
        state->physics_memory.required_roles = PHYSICS_ROLE_STATE_WRITE;
        state->physics_memory.required_capabilities = PHYSICS_CAP_MEMBRANE_MODIFY;

        bbb_register_object(bbb, &state->physics_memory);
    }

    NIMCP_LOG_DEBUG(PHYSICS_SECURITY_MODULE_NAME,
        "Registered memory region: %p size=%zu id=%u",
        address, size, region_id);

    return 0;
}

//=============================================================================
// Access Control API
//=============================================================================

bool physics_security_check_access(
    bbb_system_t bbb,
    uint32_t module_id,
    physics_security_op_t op
) {
    if (!bbb) {
        return false;
    }

    /* Create subject from module_id */
    bbb_subject_t subject = {
        .id = module_id,
        .privilege_level = PHYSICS_PRIVILEGE_LEVEL,
        .roles = PHYSICS_DEFAULT_ROLES,
        .capabilities = PHYSICS_DEFAULT_CAPS
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
    if (op >= PHYSICS_SEC_OP_WRITE_STATE && op <= PHYSICS_SEC_OP_RESET) {
        access_type = 2;  /* Write */
    } else if (op >= PHYSICS_SEC_OP_UPDATE && op <= PHYSICS_SEC_OP_SYNC_PHASE) {
        access_type = 4;  /* Execute */
    } else {
        access_type = 1;  /* Read */
    }

    return bbb_check_access(bbb, &subject, &object, access_type);
}

bool physics_security_has_capability(
    bbb_system_t bbb,
    uint32_t module_id,
    uint64_t capability
) {
    (void)bbb;

    /* Check against default capabilities for each module */
    uint64_t module_caps = PHYSICS_DEFAULT_CAPS;

    switch (module_id) {
        case PHYSICS_MODULE_ID_HH:
            module_caps |= PHYSICS_CAP_MEMBRANE_MODIFY;
            break;
        case PHYSICS_MODULE_ID_THERMO:
            module_caps |= PHYSICS_CAP_TEMPERATURE_MODIFY | PHYSICS_CAP_ENERGY_MODIFY;
            break;
        case PHYSICS_MODULE_ID_EPHAPTIC:
            module_caps |= PHYSICS_CAP_FIELD_MODIFY;
            break;
        default:
            return false;
    }

    return (module_caps & capability) == capability;
}

int physics_security_grant_capability(
    bbb_system_t bbb,
    uint32_t module_id,
    uint64_t capability
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    if (!bbb_grant_capability(bbb, module_id, capability)) {
        NIMCP_LOG_WARN(PHYSICS_SECURITY_MODULE_NAME,
            "Failed to grant capability 0x%llx to module 0x%x",
            (unsigned long long)capability, module_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_security_grant_capability: bbb_grant_capability is NULL");
        return -1;
    }

    return 0;
}

int physics_security_revoke_capability(
    bbb_system_t bbb,
    uint32_t module_id,
    uint64_t capability
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    if (!bbb_revoke_capability(bbb, module_id, capability)) {
        NIMCP_LOG_WARN(PHYSICS_SECURITY_MODULE_NAME,
            "Failed to revoke capability 0x%llx from module 0x%x",
            (unsigned long long)capability, module_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_security_revoke_capability: bbb_revoke_capability is NULL");
        return -1;
    }

    return 0;
}

//=============================================================================
// Immune Integration API
//=============================================================================

int physics_security_connect_immune(
    bbb_system_t bbb,
    void* immune,
    physics_security_state_t* state
) {
    if (!bbb || !immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_security_connect_immune: required parameter is NULL (bbb, immune)");
        return -1;
    }

    /* Connect BBB to immune system */
    if (!bbb_connect_immune(bbb, (brain_immune_system_t*)immune)) {
        NIMCP_LOG_WARN(PHYSICS_SECURITY_MODULE_NAME,
            "Failed to connect BBB to immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_security_connect_immune: bbb_connect_immune is NULL");
        return -1;
    }

    if (state) {
        state->immune_connected = true;
    }

    NIMCP_LOG_INFO(PHYSICS_SECURITY_MODULE_NAME,
        "Connected physics security to immune system");

    return 0;
}

//=============================================================================
// Cleanup API
//=============================================================================

int physics_security_unregister_all(
    bbb_system_t bbb,
    physics_security_state_t* state
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

    NIMCP_LOG_INFO(PHYSICS_SECURITY_MODULE_NAME,
        "Unregistered physics modules from BBB");

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

uint32_t physics_security_get_module_id(const char* name) {
    if (!name) return 0;

    if (strcmp(name, "hh") == 0 || strcmp(name, "hodgkin_huxley") == 0) {
        return PHYSICS_MODULE_ID_HH;
    }
    if (strcmp(name, "thermo") == 0 || strcmp(name, "thermodynamics") == 0) {
        return PHYSICS_MODULE_ID_THERMO;
    }
    if (strcmp(name, "ephaptic") == 0 || strcmp(name, "ephaptic_coupling") == 0) {
        return PHYSICS_MODULE_ID_EPHAPTIC;
    }

    return 0;
}
