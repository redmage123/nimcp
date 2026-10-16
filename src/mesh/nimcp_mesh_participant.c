/**
 * @file nimcp_mesh_participant.c
 * @brief Mesh Network Participant Implementation
 *
 * WHAT: Implementation of participant registration and registry
 * WHY:  Enable modules to participate in mesh network
 * HOW:  Hash table based registry with credential management
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_participant.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"

#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

/* ============================================================================
 * BBB Integration for Mesh Participant
 * ============================================================================ */

/** Global BBB system for mesh participant module - thread-safe access */
static _Atomic(bbb_system_t) g_mesh_participant_bbb = NULL;

/* Health agent boilerplate - thread-safe for mesh module */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mesh_participant)

/**
 * @brief Set BBB system for mesh participant validation
 * @param bbb BBB system (can be NULL to disable)
 */
void mesh_participant_set_bbb(bbb_system_t bbb) {
    atomic_store(&g_mesh_participant_bbb, bbb);
}

/**
 * @brief Get current BBB system for mesh participant
 * @return BBB system or NULL
 */
bbb_system_t mesh_participant_get_bbb(void) {
    return atomic_load(&g_mesh_participant_bbb);
}

/**
 * @brief Validate participant registration data using BBB
 *
 * WHAT: Validate participant interface and config before registration
 * WHY:  Prevent malicious module registration
 * HOW:  Use BBB input validation on string/pointer data
 *
 * @param interface Participant interface to validate
 * @param config Participant config to validate
 * @return true if valid, false if threat detected
 */
static bool validate_registration_bbb(
    const mesh_participant_interface_t* interface,
    const mesh_participant_config_t* config
) {
    if (!interface || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_registration_bbb: required parameter is NULL (interface, config)");
        return false;
    }

    bbb_system_t bbb = atomic_load(&g_mesh_participant_bbb);
    if (!bbb) return true;  /* BBB not configured, allow */

    bbb_validation_result_t result;
    bool valid = true;

    /* Validate module name string */
    if (config->module_name) {
        valid = bbb_validate_string(bbb, config->module_name, &result);
        if (!valid) {
            LOG_WARN("BBB rejected participant name '%s': %s (threat=%s)",
                     config->module_name, result.reason,
                     bbb_threat_type_name(result.threat));
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION,
                                  "BBB rejected participant module name");
            return false;
        }
    }

    /* Validate user context pointer if present */
    if (config->user_context) {
        valid = bbb_validate_pointer(bbb, config->user_context, sizeof(void*), &result);
        if (!valid) {
            LOG_WARN("BBB rejected participant user_context: %s (threat=%s)",
                     result.reason, bbb_threat_type_name(result.threat));
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION,
                                  "BBB rejected participant user context pointer");
            return false;
        }
    }

    return true;
}

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal participant entry
 */
typedef struct participant_entry {
    mesh_participant_interface_t interface; /**< Participant interface */
    credential_t credential;                /**< Copy of credential */
    bool has_credential;                    /**< Whether credential is set */
    bool is_active;                         /**< Whether entry is active */
    uint64_t registered_at_ns;              /**< Registration timestamp */
} participant_entry_t;

/**
 * @brief Participant registry structure
 */
struct mesh_participant_registry {
    uint32_t magic;                         /**< Magic for validation */
    participant_entry_t* entries;           /**< Array of entries */
    size_t capacity;                        /**< Array capacity */
    size_t count;                           /**< Active participants */
    uint32_t next_local_id;                 /**< Next local ID to assign */
    nimcp_mutex_t* mutex;                   /**< Thread safety */
    mesh_registry_stats_t stats;            /**< Statistics */
    bool enable_logging;                    /**< Logging flag */
};

/* ============================================================================
 * Private Functions
 * ============================================================================ */

/* Use global LOG_* macros from nimcp_logging.h */

/**
 * @brief Find entry by participant ID
 */
static participant_entry_t* find_entry_by_id(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id
) {
    if (!registry || !registry->entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_entry_by_id: required parameter is NULL (registry, registry->entries)");
        return NULL;
    }

    for (size_t i = 0; i < registry->capacity; i++) {
        if (registry->entries[i].is_active &&
            registry->entries[i].interface.id == id) {
            return &registry->entries[i];
        }
    }
    return NULL;  /* Not found - normal lookup miss */
}

/**
 * @brief Find entry by module name
 */
static participant_entry_t* find_entry_by_name(
    mesh_participant_registry_t* registry,
    const char* name
) {
    if (!registry || !registry->entries || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_entry_by_name: required parameter is NULL (registry, registry->entries, name)");
        return NULL;
    }

    for (size_t i = 0; i < registry->capacity; i++) {
        if (registry->entries[i].is_active &&
            strcmp(registry->entries[i].interface.module_name, name) == 0) {
            return &registry->entries[i];
        }
    }
    return NULL;  /* Not found - normal lookup miss */
}

/**
 * @brief Find free slot in registry
 */
static participant_entry_t* find_free_slot(mesh_participant_registry_t* registry) {
    if (!registry || !registry->entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_free_slot: required parameter is NULL (registry, registry->entries)");
        return NULL;
    }

    for (size_t i = 0; i < registry->capacity; i++) {
        if (!registry->entries[i].is_active) {
            return &registry->entries[i];
        }
    }
    return NULL;  /* No free slot - registry full */
}

/**
 * @brief Validate registry handle
 */
static bool validate_registry(mesh_participant_registry_t* registry) {
    return registry && registry->magic == NIMCP_MESH_MAGIC;
}

/* ============================================================================
 * String Conversion (Implementation)
 * ============================================================================ */

const char* mesh_participant_type_to_string(mesh_participant_type_t type) {
    switch (type) {
        case MESH_PARTICIPANT_NONE:        return "NONE";
        case MESH_PARTICIPANT_MODULE:      return "MODULE";
        case MESH_PARTICIPANT_COORDINATOR: return "COORDINATOR";
        case MESH_PARTICIPANT_ORDERER:     return "ORDERER";
        case MESH_PARTICIPANT_GATEWAY:     return "GATEWAY";
        case MESH_PARTICIPANT_OBSERVER:    return "OBSERVER";
        default:                           return "UNKNOWN";
    }
}

const char* mesh_coordinator_role_to_string(coordinator_role_t role) {
    switch (role) {
        case COORD_ROLE_NONE:     return "NONE";
        case COORD_ROLE_LEADER:   return "LEADER";
        case COORD_ROLE_WORKER:   return "WORKER";
        case COORD_ROLE_STANDBY:  return "STANDBY";
        case COORD_ROLE_FOLLOWER: return "FOLLOWER";
        default:                  return "UNKNOWN";
    }
}

const char* mesh_coordinator_state_to_string(coordinator_state_t state) {
    switch (state) {
        case COORD_STATE_INIT:     return "INIT";
        case COORD_STATE_JOINING:  return "JOINING";
        case COORD_STATE_ACTIVE:   return "ACTIVE";
        case COORD_STATE_ELECTION: return "ELECTION";
        case COORD_STATE_SYNCING:  return "SYNCING";
        case COORD_STATE_FAILED:   return "FAILED";
        case COORD_STATE_SHUTDOWN: return "SHUTDOWN";
        default:                   return "UNKNOWN";
    }
}

const char* mesh_credential_state_to_string(credential_state_t state) {
    switch (state) {
        case CREDENTIAL_STATE_NONE:      return "NONE";
        case CREDENTIAL_STATE_PENDING:   return "PENDING";
        case CREDENTIAL_STATE_VALID:     return "VALID";
        case CREDENTIAL_STATE_SUSPENDED: return "SUSPENDED";
        case CREDENTIAL_STATE_REVOKED:   return "REVOKED";
        case CREDENTIAL_STATE_EXPIRED:   return "EXPIRED";
        default:                         return "UNKNOWN";
    }
}

const char* mesh_channel_name(mesh_channel_id_t channel) {
    switch (channel) {
        case MESH_CHANNEL_SYSTEM:           return "SYSTEM";
        case MESH_CHANNEL_LEFT_HEMISPHERE:  return "LEFT_HEMISPHERE";
        case MESH_CHANNEL_RIGHT_HEMISPHERE: return "RIGHT_HEMISPHERE";
        case MESH_CHANNEL_SUBCORTICAL:      return "SUBCORTICAL";
        case MESH_CHANNEL_GPU_COMPUTE:      return "GPU_COMPUTE";
        default:                            return "CUSTOM";
    }
}

/* ============================================================================
 * Registry Lifecycle
 * ============================================================================ */

nimcp_error_t mesh_registry_default_config(mesh_registry_config_t* config) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    config->initial_capacity = MESH_DEFAULT_REGISTRY_CAPACITY;
    config->enable_logging = true;

    return NIMCP_SUCCESS;
}

mesh_participant_registry_t* mesh_registry_create(
    const mesh_registry_config_t* config
) {
    mesh_registry_config_t default_config;
    if (!config) {
        mesh_registry_default_config(&default_config);
        config = &default_config;
    }

    mesh_participant_registry_t* registry = nimcp_calloc(1, sizeof(*registry));
    if (!registry) {
        LOG_ERROR("Failed to allocate registry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_registry_create: registry is NULL");
        return NULL;
    }

    registry->capacity = config->initial_capacity;
    registry->entries = nimcp_calloc(registry->capacity, sizeof(participant_entry_t));
    if (!registry->entries) {
        LOG_ERROR("Failed to allocate registry entries");
        nimcp_free(registry);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_registry_create: registry->entries is NULL");
        return NULL;
    }

    registry->mutex = nimcp_mutex_create(NULL);
    if (!registry->mutex) {
        LOG_ERROR("Failed to create registry mutex");
        nimcp_free(registry->entries);
        nimcp_free(registry);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_registry_create: registry->mutex is NULL");
        return NULL;
    }

    registry->magic = NIMCP_MESH_MAGIC;
    registry->count = 0;
    registry->next_local_id = 1;
    registry->enable_logging = config->enable_logging;
    memset(&registry->stats, 0, sizeof(registry->stats));

    LOG_INFO("Created participant registry with capacity %zu", registry->capacity);
    return registry;
}

void mesh_registry_destroy(mesh_participant_registry_t* registry) {
    if (!registry) return;

    if (registry->mutex) {
        nimcp_mutex_destroy(registry->mutex);
    }

    if (registry->entries) {
        nimcp_free(registry->entries);
    }

    registry->magic = 0;
    nimcp_free(registry);
    LOG_INFO("Destroyed participant registry");
}

/* ============================================================================
 * Participant Registration
 * ============================================================================ */

nimcp_error_t mesh_participant_register(
    mesh_participant_registry_t* registry,
    const mesh_participant_interface_t* interface,
    const mesh_participant_config_t* config,
    mesh_participant_id_t* id_out
) {
    if (!validate_registry(registry)) return NIMCP_ERROR_INVALID_PARAM;
    if (!interface || !config || !id_out) return NIMCP_ERROR_NULL_POINTER;

    /* BBB validation of registration data before processing */
    if (!validate_registration_bbb(interface, config)) {
        LOG_ERROR("Rejecting participant registration '%s' - BBB validation failed",
                  config->module_name ? config->module_name : "(null)");
        return NIMCP_ERROR_BBB_VALIDATION;
    }

    /* Send heartbeat for registration operation */
    mesh_participant_heartbeat("participant_register", 0.0f);

    nimcp_mutex_lock(registry->mutex);

    /* Check if name already registered */
    if (config->module_name && find_entry_by_name(registry, config->module_name)) {
        nimcp_mutex_unlock(registry->mutex);
        LOG_WARN("Participant '%s' already registered", config->module_name);
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    /* Find free slot */
    participant_entry_t* entry = find_free_slot(registry);
    if (!entry) {
        nimcp_mutex_unlock(registry->mutex);
        LOG_ERROR("Registry full, cannot register participant");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Generate participant ID */
    uint32_t local_id = registry->next_local_id++;
    mesh_participant_id_t id = mesh_make_participant_id(
        config->home_channel,
        config->type,
        local_id
    );

    /* Copy interface */
    memcpy(&entry->interface, interface, sizeof(mesh_participant_interface_t));

    /* Set identity */
    entry->interface.id = id;
    entry->interface.type = config->type;
    entry->interface.home_channel = config->home_channel;
    entry->interface.user_context = config->user_context;
    entry->interface.has_gpu_acceleration = config->request_gpu;

    if (config->module_name) {
        strncpy(entry->interface.module_name, config->module_name,
                MESH_MAX_NAME_LEN - 1);
        entry->interface.module_name[MESH_MAX_NAME_LEN - 1] = '\0';
    }

    /* Initialize membership */
    entry->interface.channel_memberships[0] = config->home_channel;
    entry->interface.channel_membership_count = 1;

    /* Mark active */
    entry->is_active = true;
    entry->has_credential = false;
    entry->registered_at_ns = nimcp_time_now_ns();

    registry->count++;
    registry->stats.total_participants = registry->count;
    registry->stats.registrations++;
    registry->stats.by_channel[config->home_channel % MESH_MAX_CHANNELS]++;

    if (config->type == MESH_PARTICIPANT_COORDINATOR) {
        registry->stats.coordinators++;
    }

    *id_out = id;

    nimcp_mutex_unlock(registry->mutex);

    LOG_INFO("Registered participant '%s' (id=0x%016lx, type=%s, channel=%s)",
             entry->interface.module_name,
             (unsigned long)id,
             mesh_participant_type_to_string(config->type),
             mesh_channel_name(config->home_channel));

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_participant_unregister(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id
) {
    if (!validate_registry(registry)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(registry->mutex);

    participant_entry_t* entry = find_entry_by_id(registry, id);
    if (!entry) {
        nimcp_mutex_unlock(registry->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    char name[MESH_MAX_NAME_LEN];
    strncpy(name, entry->interface.module_name, MESH_MAX_NAME_LEN);

    /* Update stats */
    mesh_channel_id_t channel = mesh_get_channel(id);
    registry->stats.by_channel[channel % MESH_MAX_CHANNELS]--;

    if (entry->interface.type == MESH_PARTICIPANT_COORDINATOR) {
        registry->stats.coordinators--;
    }

    /* Clear entry */
    memset(entry, 0, sizeof(*entry));
    registry->count--;
    registry->stats.total_participants = registry->count;
    registry->stats.unregistrations++;

    nimcp_mutex_unlock(registry->mutex);

    LOG_INFO("Unregistered participant '%s' (id=0x%016lx)", name, (unsigned long)id);

    return NIMCP_SUCCESS;
}

const mesh_participant_interface_t* mesh_participant_get(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id
) {
    if (!validate_registry(registry)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_participant_get: validate_registry is NULL");
        return NULL;
    }

    nimcp_mutex_lock(registry->mutex);
    participant_entry_t* entry = find_entry_by_id(registry, id);
    nimcp_mutex_unlock(registry->mutex);

    return entry ? &entry->interface : NULL;
}

const mesh_participant_interface_t* mesh_participant_get_by_name(
    mesh_participant_registry_t* registry,
    const char* name
) {
    if (!validate_registry(registry) || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_participant_get_by_name: required parameter is NULL (validate_registry, name)");
        return NULL;
    }

    nimcp_mutex_lock(registry->mutex);
    participant_entry_t* entry = find_entry_by_name(registry, name);
    nimcp_mutex_unlock(registry->mutex);

    return entry ? &entry->interface : NULL;
}

bool mesh_participant_is_registered(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id
) {
    return mesh_participant_get(registry, id) != NULL;
}

/* ============================================================================
 * Channel Membership
 * ============================================================================ */

nimcp_error_t mesh_participant_join_channel(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    mesh_channel_id_t channel
) {
    if (!validate_registry(registry)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(registry->mutex);

    participant_entry_t* entry = find_entry_by_id(registry, id);
    if (!entry) {
        nimcp_mutex_unlock(registry->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Check if already member */
    for (size_t i = 0; i < entry->interface.channel_membership_count; i++) {
        if (entry->interface.channel_memberships[i] == channel) {
            nimcp_mutex_unlock(registry->mutex);
            return NIMCP_SUCCESS; /* Already member */
        }
    }

    /* Check capacity */
    if (entry->interface.channel_membership_count >= MESH_MAX_CHANNEL_MEMBERSHIPS) {
        nimcp_mutex_unlock(registry->mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Add membership */
    entry->interface.channel_memberships[entry->interface.channel_membership_count++] = channel;
    registry->stats.by_channel[channel % MESH_MAX_CHANNELS]++;

    nimcp_mutex_unlock(registry->mutex);

    LOG_DEBUG("Participant 0x%016lx joined channel %s",
              (unsigned long)id, mesh_channel_name(channel));

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_participant_leave_channel(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    mesh_channel_id_t channel
) {
    if (!validate_registry(registry)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(registry->mutex);

    participant_entry_t* entry = find_entry_by_id(registry, id);
    if (!entry) {
        nimcp_mutex_unlock(registry->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Cannot leave home channel */
    if (channel == entry->interface.home_channel) {
        nimcp_mutex_unlock(registry->mutex);
        return NIMCP_ERROR_PERMISSION_DENIED;
    }

    /* Find and remove */
    bool found = false;
    for (size_t i = 0; i < entry->interface.channel_membership_count; i++) {
        if (entry->interface.channel_memberships[i] == channel) {
            /* Shift remaining */
            for (size_t j = i; j < entry->interface.channel_membership_count - 1; j++) {
                entry->interface.channel_memberships[j] =
                    entry->interface.channel_memberships[j + 1];
            }
            entry->interface.channel_membership_count--;
            registry->stats.by_channel[channel % MESH_MAX_CHANNELS]--;
            found = true;
            break;
        }
    }

    nimcp_mutex_unlock(registry->mutex);

    return found ? NIMCP_SUCCESS : NIMCP_ERROR_NOT_FOUND;
}

bool mesh_participant_is_in_channel(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    mesh_channel_id_t channel
) {
    if (!validate_registry(registry)) {
        return false;
    }

    nimcp_mutex_lock(registry->mutex);

    participant_entry_t* entry = find_entry_by_id(registry, id);
    if (!entry) {
        nimcp_mutex_unlock(registry->mutex);
        return false;
    }

    for (size_t i = 0; i < entry->interface.channel_membership_count; i++) {
        if (entry->interface.channel_memberships[i] == channel) {
            nimcp_mutex_unlock(registry->mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(registry->mutex);
    return false;
}

nimcp_error_t mesh_participant_get_channel_members(
    mesh_participant_registry_t* registry,
    mesh_channel_id_t channel,
    mesh_participant_id_t* ids_out,
    size_t max_ids,
    size_t* count_out
) {
    if (!validate_registry(registry)) return NIMCP_ERROR_INVALID_PARAM;
    if (!ids_out || !count_out) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(registry->mutex);

    size_t count = 0;
    for (size_t i = 0; i < registry->capacity && count < max_ids; i++) {
        if (!registry->entries[i].is_active) continue;

        participant_entry_t* entry = &registry->entries[i];
        for (size_t j = 0; j < entry->interface.channel_membership_count; j++) {
            if (entry->interface.channel_memberships[j] == channel) {
                ids_out[count++] = entry->interface.id;
                break;
            }
        }
    }

    *count_out = count;
    nimcp_mutex_unlock(registry->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Credential Management
 * ============================================================================ */

nimcp_error_t mesh_participant_set_credential(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const credential_t* credential
) {
    if (!validate_registry(registry)) return NIMCP_ERROR_INVALID_PARAM;
    if (!credential) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(registry->mutex);

    participant_entry_t* entry = find_entry_by_id(registry, id);
    if (!entry) {
        nimcp_mutex_unlock(registry->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    memcpy(&entry->credential, credential, sizeof(credential_t));
    entry->has_credential = true;
    entry->interface.credential = &entry->credential;

    nimcp_mutex_unlock(registry->mutex);

    LOG_DEBUG("Set credential for participant 0x%016lx", (unsigned long)id);

    return NIMCP_SUCCESS;
}

const credential_t* mesh_participant_get_credential(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id
) {
    if (!validate_registry(registry)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_participant_get_credential: validate_registry is NULL");
        return NULL;
    }

    nimcp_mutex_lock(registry->mutex);

    participant_entry_t* entry = find_entry_by_id(registry, id);
    if (!entry || !entry->has_credential) {
        nimcp_mutex_unlock(registry->mutex);
        return NULL;  /* Entry not found or no credential - normal lookup miss */
    }

    nimcp_mutex_unlock(registry->mutex);
    return &entry->credential;
}

bool mesh_participant_validate_credential(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id
) {
    if (!validate_registry(registry)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_participant_validate_credential: validate_registry is NULL");
        return false;
    }

    nimcp_mutex_lock(registry->mutex);

    participant_entry_t* entry = find_entry_by_id(registry, id);
    if (!entry || !entry->has_credential) {
        nimcp_mutex_unlock(registry->mutex);
        return false;  /* Entry not found or no credential - normal lookup miss */
    }

    bool valid = entry->credential.state == CREDENTIAL_STATE_VALID;

    /* Check expiration */
    if (valid && entry->credential.expires_at_ns > 0) {
        uint64_t now = nimcp_time_now_ns();
        if (now >= entry->credential.expires_at_ns) {
            entry->credential.state = CREDENTIAL_STATE_EXPIRED;
            valid = false;
        }
    }

    nimcp_mutex_unlock(registry->mutex);
    return valid;
}

nimcp_error_t mesh_participant_suspend_credential(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const char* reason
) {
    if (!validate_registry(registry)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(registry->mutex);

    participant_entry_t* entry = find_entry_by_id(registry, id);
    if (!entry || !entry->has_credential) {
        nimcp_mutex_unlock(registry->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    entry->credential.state = CREDENTIAL_STATE_SUSPENDED;

    nimcp_mutex_unlock(registry->mutex);

    LOG_WARN("Suspended credential for participant 0x%016lx: %s",
             (unsigned long)id, reason ? reason : "no reason");

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_participant_revoke_credential(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const char* reason
) {
    if (!validate_registry(registry)) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(registry->mutex);

    participant_entry_t* entry = find_entry_by_id(registry, id);
    if (!entry || !entry->has_credential) {
        nimcp_mutex_unlock(registry->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    entry->credential.state = CREDENTIAL_STATE_REVOKED;

    nimcp_mutex_unlock(registry->mutex);

    LOG_ERROR("Revoked credential for participant 0x%016lx: %s",
              (unsigned long)id, reason ? reason : "no reason");

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Callback Invocation
 * ============================================================================ */

nimcp_error_t mesh_participant_invoke_on_proposal(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const mesh_transaction_t* tx
) {
    const mesh_participant_interface_t* iface = mesh_participant_get(registry, id);
    if (!iface) return NIMCP_ERROR_NOT_FOUND;

    if (!iface->on_proposal) return NIMCP_SUCCESS; /* Optional callback */

    return iface->on_proposal(iface->user_context, tx);
}

nimcp_error_t mesh_participant_invoke_on_endorse(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const mesh_transaction_t* tx,
    mesh_endorsement_t* endorsement
) {
    const mesh_participant_interface_t* iface = mesh_participant_get(registry, id);
    if (!iface) return NIMCP_ERROR_NOT_FOUND;

    if (!iface->on_endorse_request) {
        /* Default: abstain */
        endorsement->endorser_id = id;
        endorsement->result = ENDORSEMENT_ABSTAIN;
        endorsement->timestamp_ns = nimcp_time_now_ns();
        return NIMCP_SUCCESS;
    }

    return iface->on_endorse_request(iface->user_context, tx, endorsement);
}

nimcp_error_t mesh_participant_invoke_on_commit(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const mesh_transaction_t* tx
) {
    const mesh_participant_interface_t* iface = mesh_participant_get(registry, id);
    if (!iface) return NIMCP_ERROR_NOT_FOUND;

    if (!iface->on_commit) return NIMCP_SUCCESS;

    return iface->on_commit(iface->user_context, tx);
}

nimcp_error_t mesh_participant_invoke_on_belief(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const mesh_belief_t* belief
) {
    const mesh_participant_interface_t* iface = mesh_participant_get(registry, id);
    if (!iface) return NIMCP_ERROR_NOT_FOUND;

    if (!iface->on_belief_received) return NIMCP_SUCCESS;

    return iface->on_belief_received(iface->user_context, belief);
}

nimcp_error_t mesh_participant_invoke_on_consensus(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    const mesh_consensus_t* consensus
) {
    const mesh_participant_interface_t* iface = mesh_participant_get(registry, id);
    if (!iface) return NIMCP_ERROR_NOT_FOUND;

    if (!iface->on_consensus_reached) return NIMCP_SUCCESS;

    return iface->on_consensus_reached(iface->user_context, consensus);
}

/* ============================================================================
 * State Query
 * ============================================================================ */

float mesh_participant_get_free_energy(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id
) {
    const mesh_participant_interface_t* iface = mesh_participant_get(registry, id);
    if (!iface || !iface->get_free_energy) return -1.0f;

    return iface->get_free_energy(iface->user_context);
}

nimcp_error_t mesh_participant_get_beliefs(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    mesh_belief_set_t* beliefs
) {
    if (!beliefs) return NIMCP_ERROR_NULL_POINTER;

    const mesh_participant_interface_t* iface = mesh_participant_get(registry, id);
    if (!iface) return NIMCP_ERROR_NOT_FOUND;

    if (!iface->get_beliefs) {
        beliefs->count = 0;
        return NIMCP_SUCCESS;
    }

    iface->get_beliefs(iface->user_context, beliefs);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_participant_get_health(
    mesh_participant_registry_t* registry,
    mesh_participant_id_t id,
    health_metrics_t* metrics
) {
    if (!metrics) return NIMCP_ERROR_NULL_POINTER;

    const mesh_participant_interface_t* iface = mesh_participant_get(registry, id);
    if (!iface) return NIMCP_ERROR_NOT_FOUND;

    if (!iface->get_health_metrics) {
        memset(metrics, 0, sizeof(*metrics));
        metrics->participant = id;
        metrics->is_healthy = true;
        return NIMCP_SUCCESS;
    }

    iface->get_health_metrics(iface->user_context, metrics);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics and Iteration
 * ============================================================================ */

nimcp_error_t mesh_registry_get_stats(
    mesh_participant_registry_t* registry,
    mesh_registry_stats_t* stats
) {
    if (!validate_registry(registry)) return NIMCP_ERROR_INVALID_PARAM;
    if (!stats) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(registry->mutex);

    /* Count active */
    size_t active = 0;
    for (size_t i = 0; i < registry->capacity; i++) {
        if (registry->entries[i].is_active) {
            active++;
        }
    }
    registry->stats.active_participants = active;

    memcpy(stats, &registry->stats, sizeof(mesh_registry_stats_t));

    nimcp_mutex_unlock(registry->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_registry_iterate(
    mesh_participant_registry_t* registry,
    bool (*callback)(const mesh_participant_interface_t*, void*),
    void* user_ctx
) {
    if (!validate_registry(registry)) return NIMCP_ERROR_INVALID_PARAM;
    if (!callback) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(registry->mutex);

    for (size_t i = 0; i < registry->capacity; i++) {
        if (registry->entries[i].is_active) {
            if (!callback(&registry->entries[i].interface, user_ctx)) {
                break;
            }
        }
    }

    nimcp_mutex_unlock(registry->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_registry_iterate_channel(
    mesh_participant_registry_t* registry,
    mesh_channel_id_t channel,
    bool (*callback)(const mesh_participant_interface_t*, void*),
    void* user_ctx
) {
    if (!validate_registry(registry)) return NIMCP_ERROR_INVALID_PARAM;
    if (!callback) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(registry->mutex);

    for (size_t i = 0; i < registry->capacity; i++) {
        if (!registry->entries[i].is_active) continue;

        participant_entry_t* entry = &registry->entries[i];
        for (size_t j = 0; j < entry->interface.channel_membership_count; j++) {
            if (entry->interface.channel_memberships[j] == channel) {
                if (!callback(&entry->interface, user_ctx)) {
                    nimcp_mutex_unlock(registry->mutex);
                    return NIMCP_SUCCESS;
                }
                break;
            }
        }
    }

    nimcp_mutex_unlock(registry->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

void mesh_participant_interface_init(mesh_participant_interface_t* interface) {
    if (!interface) return;
    memset(interface, 0, sizeof(*interface));
    interface->type = MESH_PARTICIPANT_MODULE;
}

void mesh_participant_config_init(mesh_participant_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
    config->type = MESH_PARTICIPANT_MODULE;
    config->home_channel = MESH_CHANNEL_SYSTEM;
}

void mesh_participant_print_info(const mesh_participant_interface_t* interface) {
    if (!interface) {
        printf("Participant: NULL\n");
        return;
    }

    printf("Participant: %s\n", interface->module_name);
    printf("  ID:       0x%016lx\n", (unsigned long)interface->id);
    printf("  Type:     %s\n", mesh_participant_type_to_string(interface->type));
    printf("  Channel:  %s\n", mesh_channel_name(interface->home_channel));
    printf("  GPU:      %s\n", interface->has_gpu_acceleration ? "yes" : "no");
    printf("  Channels: %zu\n", interface->channel_membership_count);

    if (interface->credential) {
        printf("  Cred:     %s\n", mesh_credential_state_to_string(
            interface->credential->state));
    } else {
        printf("  Cred:     NONE\n");
    }
}
