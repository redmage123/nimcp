//=============================================================================
// nimcp_ofc_security.c - OFC Security Registration
//=============================================================================
/**
 * @file nimcp_ofc_security.c
 * @brief Implementation of OFC BBB registration
 */

#include "core/brain/regions/ofc/bridges/nimcp_ofc_security.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(ofc_security, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Map operation to required roles
 */
static uint32_t op_to_roles(ofc_security_op_t op) {
    switch (op) {
        case OFC_SEC_OP_READ_STATE:
        case OFC_SEC_OP_READ_STATS:
        case OFC_SEC_OP_READ_VALUE:
            return OFC_ROLE_STATE_READ;

        case OFC_SEC_OP_READ_PARAMS:
            return OFC_ROLE_PARAM_READ;

        case OFC_SEC_OP_WRITE_STATE:
            return OFC_ROLE_STATE_WRITE;

        case OFC_SEC_OP_WRITE_PARAMS:
            return OFC_ROLE_PARAM_WRITE;

        case OFC_SEC_OP_RESET:
            return OFC_ROLE_STATE_WRITE | OFC_ROLE_PARAM_WRITE;

        case OFC_SEC_OP_COMPUTE_VALUE:
        case OFC_SEC_OP_MAKE_DECISION:
        case OFC_SEC_OP_ASSESS_RISK:
            return OFC_ROLE_COMPUTE;

        case OFC_SEC_OP_BROADCAST:
        case OFC_SEC_OP_SUBSCRIBE:
            return OFC_ROLE_BROADCAST;

        case OFC_SEC_OP_KG_READ:
        case OFC_SEC_OP_KG_WRITE:
        case OFC_SEC_OP_KG_QUERY:
            return OFC_ROLE_KG_ACCESS;

        default:
            return 0;
    }
}

/**
 * @brief Map operation to required privilege
 */
static uint32_t op_to_privilege(ofc_security_op_t op) {
    switch (op) {
        case OFC_SEC_OP_READ_STATE:
        case OFC_SEC_OP_READ_STATS:
        case OFC_SEC_OP_READ_PARAMS:
        case OFC_SEC_OP_READ_VALUE:
        case OFC_SEC_OP_KG_READ:
        case OFC_SEC_OP_KG_QUERY:
            return 1;  /* Low privilege for reads */

        case OFC_SEC_OP_COMPUTE_VALUE:
        case OFC_SEC_OP_MAKE_DECISION:
        case OFC_SEC_OP_ASSESS_RISK:
        case OFC_SEC_OP_BROADCAST:
        case OFC_SEC_OP_SUBSCRIBE:
            return 2;  /* Moderate privilege for compute/comms */

        case OFC_SEC_OP_WRITE_STATE:
        case OFC_SEC_OP_WRITE_PARAMS:
        case OFC_SEC_OP_RESET:
        case OFC_SEC_OP_KG_WRITE:
            return 3;  /* Higher privilege for writes */

        default:
            return 0;
    }
}

//=============================================================================
// Configuration API
//=============================================================================

int ofc_security_default_config(ofc_security_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_bbb = true;
    config->enable_immune = true;
    config->enable_kg_validation = true;
    config->admin_token = 0;
    config->privilege_level = 0;  /* Use default */
    config->roles = 0;            /* Use default */
    config->capabilities = 0;     /* Use default */

    return 0;
}

//=============================================================================
// Registration API
//=============================================================================

int ofc_security_register(
    bbb_system_t bbb,
    const ofc_security_config_t* config,
    ofc_security_state_t* state
) {
    if (!bbb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

        return -1;

    }

    ofc_security_config_t local_config;
    if (!config) {
        ofc_security_default_config(&local_config);
        config = &local_config;
    }

    ofc_security_state_t local_state;
    memset(&local_state, 0, sizeof(local_state));

    /* Build subject */
    local_state.ofc_subject.id = OFC_MODULE_ID;
    local_state.ofc_subject.privilege_level =
        config->privilege_level ? config->privilege_level : OFC_PRIVILEGE_LEVEL;
    local_state.ofc_subject.roles =
        config->roles ? config->roles : OFC_DEFAULT_ROLES;
    local_state.ofc_subject.capabilities =
        config->capabilities ? config->capabilities : OFC_DEFAULT_CAPS;

    /* Register with BBB */
    if (!bbb_register_subject(bbb, &local_state.ofc_subject)) {
        NIMCP_LOG_ERROR(OFC_SECURITY_MODULE_NAME,
            "Failed to register OFC subject with BBB");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ofc_security_register: bbb_register_subject is NULL");
        return -1;
    }

    local_state.admin_token = config->admin_token;
    local_state.kg_validation_enabled = config->enable_kg_validation;
    local_state.registered = true;

    if (state) {
        *state = local_state;
    }

    NIMCP_LOG_INFO(OFC_SECURITY_MODULE_NAME,
        "Registered OFC module with BBB (id=0x%x)", OFC_MODULE_ID);

    return 0;
}

int ofc_security_unregister(
    bbb_system_t bbb,
    ofc_security_state_t* state
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

    /* Clear state */
    state->registered = false;
    state->immune_connected = false;
    state->admin_token = 0;

    NIMCP_LOG_INFO(OFC_SECURITY_MODULE_NAME,
        "Unregistered OFC module from BBB");

    return 0;
}

//=============================================================================
// Access Control API
//=============================================================================

bool ofc_security_check_access(
    bbb_system_t bbb,
    ofc_security_op_t op
) {
    if (!bbb) {
        return false;
    }

    /* Create subject from OFC module */
    bbb_subject_t subject = {
        .id = OFC_MODULE_ID,
        .privilege_level = OFC_PRIVILEGE_LEVEL,
        .roles = OFC_DEFAULT_ROLES,
        .capabilities = OFC_DEFAULT_CAPS
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
    if (op >= OFC_SEC_OP_WRITE_STATE && op <= OFC_SEC_OP_RESET) {
        access_type = 2;  /* Write */
    } else if (op >= OFC_SEC_OP_COMPUTE_VALUE && op <= OFC_SEC_OP_ASSESS_RISK) {
        access_type = 4;  /* Execute */
    } else {
        access_type = 1;  /* Read */
    }

    return bbb_check_access(bbb, &subject, &object, access_type);
}

bool ofc_security_has_capability(
    const ofc_security_state_t* state,
    uint64_t capability
) {
    if (!state || !state->registered) {
        return false;
    }

    return (state->ofc_subject.capabilities & capability) == capability;
}

//=============================================================================
// Token Validation API
//=============================================================================

bool ofc_security_validate_kg_token(
    const ofc_security_state_t* state,
    uint64_t token
) {
    if (!state || !state->kg_validation_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_security_validate_kg_token: required parameter is NULL (state, state->kg_validation_enabled)");
        return false;
    }
    if (state->admin_token == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ofc_security_validate_kg_token: state->admin_token is zero");
        return false;
    }

    /* Constant-time comparison to prevent timing attacks */
    volatile uint64_t result = state->admin_token ^ token;
    return result == 0;
}

int ofc_security_set_admin_token(
    ofc_security_state_t* state,
    uint64_t token
) {
    if (!state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state is NULL");

        return -1;

    }

    state->admin_token = token;
    state->kg_validation_enabled = (token != 0);

    NIMCP_LOG_DEBUG(OFC_SECURITY_MODULE_NAME,
        "Admin token %s", token ? "set" : "cleared");

    return 0;
}

//=============================================================================
// Immune Integration API
//=============================================================================

int ofc_security_connect_immune(
    bbb_system_t bbb,
    void* immune,
    ofc_security_state_t* state
) {
    if (!bbb || !immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_security_connect_immune: required parameter is NULL (bbb, immune)");
        return -1;
    }

    /* Connect BBB to immune system */
    if (!bbb_connect_immune(bbb, (brain_immune_system_t*)immune)) {
        NIMCP_LOG_WARN(OFC_SECURITY_MODULE_NAME,
            "Failed to connect BBB to immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ofc_security_connect_immune: bbb_connect_immune is NULL");
        return -1;
    }

    if (state) {
        state->immune_connected = true;
    }

    NIMCP_LOG_INFO(OFC_SECURITY_MODULE_NAME,
        "Connected OFC security to immune system");

    return 0;
}
