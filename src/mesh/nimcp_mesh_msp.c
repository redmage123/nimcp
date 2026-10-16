/**
 * @file nimcp_mesh_msp.c
 * @brief Mesh Network Membership Service Provider Implementation
 *
 * Implementation of identity management, authentication, authorization,
 * and immune system integration for mesh participants.
 *
 * SECURITY INTEGRATION:
 * - BBB: Validates credentials through Blood-Brain Barrier
 * - Immune: Routes quarantine/revocation through immune system
 * - Antigen presentation for security threats
 */

#include "mesh/nimcp_mesh_msp.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

/* ============================================================================
 * BBB and Immune System Type Definitions
 * ============================================================================ */

/* Forward declare to avoid circular dependencies */
typedef struct brain_immune_system brain_immune_system_t;

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Credential entry
 */
typedef struct credential_entry {
    mesh_participant_id_t participant_id;
    credential_t credential;
    mesh_channel_id_t memberships[MESH_MSP_MAX_MEMBERSHIPS];
    size_t membership_count;
    bool quarantined;
    uint64_t quarantine_end_ns;
    struct credential_entry* next;
} credential_entry_t;

/**
 * @brief Policy entry
 */
typedef struct policy_entry {
    msp_access_policy_t policy;
    msp_policy_callback_t callback;
    void* callback_ctx;
    struct policy_entry* next;
} policy_entry_t;

/**
 * @brief MSP structure
 */
struct mesh_msp {
    /* Configuration */
    char* name;
    mesh_msp_config_t config;

    /* Participant registry */
    mesh_participant_registry_t* registry;

    /* Credentials */
    credential_entry_t* credentials_head;
    size_t credential_count;

    /* Policies */
    policy_entry_t* policies_head;
    size_t policy_count;

    /* BBB and Immune integration */
    void* bbb_handle;
    void* immune_handle;
    msp_immune_callback_t immune_callback;
    void* immune_callback_ctx;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Statistics */
    mesh_msp_stats_t stats;

    /* Logging */
    nimcp_logger_t logger;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_time_ns(void) {
    return nimcp_time_now_ns();
}

static credential_entry_t* find_credential(mesh_msp_t* msp, mesh_participant_id_t id) {
    credential_entry_t* entry = msp->credentials_head;
    while (entry) {
        if (entry->participant_id == id) {
            return entry;
        }
        entry = entry->next;
    }
    /* Not found is normal lookup result, not an error (P2: false positive removal) */
    return NULL;
}

static policy_entry_t* find_policy(mesh_msp_t* msp, const char* name) {
    policy_entry_t* entry = msp->policies_head;
    while (entry) {
        if (entry->policy.policy_name && strcmp(entry->policy.policy_name, name) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    /* Not found is normal lookup result, not an error (P2: false positive removal) */
    return NULL;
}

/**
 * P1-47: Unlocked internal helpers for use within already-locked contexts.
 * These avoid deadlock when public functions call each other while holding mutex.
 */
static const credential_t* get_credential_unlocked(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id
) {
    credential_entry_t* entry = ((mesh_msp_t*)msp)->credentials_head;
    while (entry) {
        if (entry->participant_id == participant_id) {
            return &entry->credential;
        }
        entry = entry->next;
    }
    return NULL;
}

static bool is_credential_valid_unlocked(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id
) {
    const credential_t* cred = get_credential_unlocked(msp, participant_id);
    if (!cred || cred->state != CREDENTIAL_STATE_VALID) return false;
    uint64_t now = get_time_ns();
    if (cred->expires_at_ns > 0 && now > cred->expires_at_ns) return false;
    return true;
}

static bool check_capability_unlocked(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    uint64_t capability
) {
    const credential_t* cred = get_credential_unlocked(msp, participant_id);
    if (!cred) return false;
    return (cred->capabilities & capability) == capability;
}

static bool has_channel_membership_unlocked(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    mesh_channel_id_t channel_id
) {
    credential_entry_t* entry = ((mesh_msp_t*)msp)->credentials_head;
    while (entry) {
        if (entry->participant_id == participant_id) {
            for (size_t i = 0; i < entry->membership_count; i++) {
                if (entry->memberships[i] == channel_id) return true;
            }
            return false;
        }
        entry = entry->next;
    }
    return false;
}

static bool is_quarantined_unlocked(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id
) {
    credential_entry_t* entry = ((mesh_msp_t*)msp)->credentials_head;
    while (entry) {
        if (entry->participant_id == participant_id) {
            if (!entry->quarantined) return false;
            uint64_t now = get_time_ns();
            if (entry->quarantine_end_ns > 0 && now > entry->quarantine_end_ns) return false;
            return true;
        }
        entry = entry->next;
    }
    return false;
}

/* P2-145: Thread-safe PRNG using CAS loop instead of non-thread-safe rand() */
static _Atomic uint64_t s_msp_prng_state = 0;

static uint32_t msp_thread_safe_rand(void) {
    uint64_t old_state, new_state;
    do {
        old_state = atomic_load(&s_msp_prng_state);
        if (old_state == 0) {
            /* Seed the PRNG - CAS 0 -> seed value, then loop will read the seed */
            uint64_t seed = (uint64_t)nimcp_time_now_ns();
            if (seed == 0) seed = 1;
            atomic_compare_exchange_strong(&s_msp_prng_state, &old_state, seed);
            continue;  /* Re-read the (now-seeded) state */
        }
        new_state = old_state;
        new_state ^= new_state << 13;
        new_state ^= new_state >> 7;
        new_state ^= new_state << 17;
    } while (!atomic_compare_exchange_weak(&s_msp_prng_state, &old_state, new_state));
    return (uint32_t)(new_state & 0xFFFFFFFF);
}

static void generate_credential_id(uint8_t* id_out) {
    /* P2-145: Use thread-safe PRNG instead of rand() */
    for (int i = 0; i < MESH_CREDENTIAL_ID_SIZE; i++) {
        id_out[i] = (uint8_t)(msp_thread_safe_rand() & 0xFF);
    }
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

nimcp_error_t mesh_msp_default_config(mesh_msp_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp_default_config: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(config, 0, sizeof(*config));

    config->msp_name = "default_msp";
    config->credential_expiration_ms = MESH_MSP_DEFAULT_EXPIRATION_MS;
    config->default_privilege_level = 1;
    config->default_capabilities = MESH_CAP_READ | MESH_CAP_PROPOSE;
    config->require_signature = false;
    config->enable_quarantine = true;
    config->quarantine_duration_ms = MESH_MSP_DEFAULT_QUARANTINE_MS;
    config->enable_logging = true;
    config->enable_audit = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

mesh_msp_t* mesh_msp_create(
    const mesh_msp_config_t* config,
    mesh_participant_registry_t* registry
) {
    mesh_msp_t* msp = nimcp_calloc(1, sizeof(mesh_msp_t));
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_msp_create: msp is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        msp->config = *config;
    } else {
        mesh_msp_default_config(&msp->config);
    }

    /* Copy name */
    if (msp->config.msp_name) {
        msp->name = nimcp_strdup(msp->config.msp_name);
    } else {
        msp->name = nimcp_strdup("msp");
    }

    msp->registry = registry;
    msp->bbb_handle = msp->config.bbb_handle;
    msp->immune_handle = msp->config.immune_handle;

    /* Create mutex for thread safety (P1-27) */
    msp->mutex = nimcp_mutex_create(NULL);
    if (!msp->mutex) {
        nimcp_free(msp->name);
        nimcp_free(msp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_msp_create: mutex creation failed");
        return NULL;
    }

    /* Initialize logger */
    if (msp->config.enable_logging) {
        msp->logger = nimcp_logger_get("mesh.msp");
    }

    return msp;
}

void mesh_msp_destroy(mesh_msp_t* msp) {
    if (!msp) {
        return;
    }

    /* Free credentials */
    credential_entry_t* cred = msp->credentials_head;
    while (cred) {
        credential_entry_t* next = cred->next;
        nimcp_free(cred);
        cred = next;
    }

    /* Free policies */
    policy_entry_t* pol = msp->policies_head;
    while (pol) {
        policy_entry_t* next = pol->next;
        nimcp_free((void*)pol->policy.required_channels);
        nimcp_free(pol);
        pol = next;
    }

    /* Destroy mutex */
    if (msp->mutex) {
        nimcp_mutex_destroy(msp->mutex);
    }

    nimcp_free(msp->name);
    nimcp_free(msp);
}

const char* mesh_msp_get_name(const mesh_msp_t* msp) {
    return msp ? msp->name : NULL;
}

/* ============================================================================
 * BBB and Immune System Connection API
 * ============================================================================ */

/**
 * @brief Connect MSP to real BBB instance
 *
 * WHAT: Wire MSP to BBB for credential validation
 * WHY:  Enable BBB-based transaction and credential validation
 *
 * @param msp MSP handle
 * @param bbb BBB system handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_connect_bbb(
    mesh_msp_t* msp,
    bbb_system_t bbb
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    msp->bbb_handle = bbb;

    if (msp->config.enable_logging && msp->logger) {
        LOG_DEBUG("MSP connected to BBB system");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Connect MSP to real immune system instance
 *
 * WHAT: Wire MSP to immune system for quarantine/revocation routing
 * WHY:  Enable immune-driven security responses
 *
 * @param msp MSP handle
 * @param immune Immune system handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_msp_connect_immune(
    mesh_msp_t* msp,
    brain_immune_system_t* immune
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    msp->immune_handle = immune;

    if (msp->config.enable_logging && msp->logger) {
        LOG_DEBUG("MSP connected to brain immune system");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Get BBB handle from MSP
 *
 * @param msp MSP handle
 * @return BBB handle or NULL if not connected
 */
bbb_system_t mesh_msp_get_bbb(const mesh_msp_t* msp) {
    return msp ? (bbb_system_t)msp->bbb_handle : NULL;
}

/**
 * @brief Get immune system handle from MSP
 *
 * @param msp MSP handle
 * @return Immune system handle or NULL if not connected
 */
brain_immune_system_t* mesh_msp_get_immune(const mesh_msp_t* msp) {
    return msp ? (brain_immune_system_t*)msp->immune_handle : NULL;
}

/* ============================================================================
 * Credential Management
 * ============================================================================ */

nimcp_error_t mesh_msp_issue_credential(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    uint32_t privilege_level,
    uint64_t capabilities,
    credential_t* credential_out
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* P1-47: All credential operations must be mutex-protected */
    nimcp_mutex_lock(msp->mutex);

    /* Check if already has credential */
    credential_entry_t* existing = find_credential(msp, participant_id);
    if (existing && existing->credential.state == CREDENTIAL_STATE_VALID) {
        if (credential_out) {
            *credential_out = existing->credential;
        }
        nimcp_mutex_unlock(msp->mutex);
        return NIMCP_SUCCESS;
    }

    /* Create new entry */
    credential_entry_t* entry;
    if (existing) {
        entry = existing;
    } else {
        entry = nimcp_calloc(1, sizeof(credential_entry_t));
        if (!entry) {
            nimcp_mutex_unlock(msp->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_msp: memory allocation failed");
            return NIMCP_ERROR_NO_MEMORY;
        }
        entry->participant_id = participant_id;
        entry->next = msp->credentials_head;
        msp->credentials_head = entry;
        msp->credential_count++;
    }

    /* Initialize credential */
    credential_t* cred = &entry->credential;
    generate_credential_id(cred->id);
    cred->participant_id = participant_id;
    cred->state = CREDENTIAL_STATE_VALID;
    cred->privilege_level = privilege_level > MESH_MSP_MAX_PRIVILEGE_LEVEL ?
                            MESH_MSP_MAX_PRIVILEGE_LEVEL : privilege_level;
    cred->capabilities = capabilities;

    uint64_t now = get_time_ns();
    cred->issued_at_ns = now;
    cred->expires_at_ns = now + (msp->config.credential_expiration_ms * 1000000ULL);

    if (credential_out) {
        *credential_out = *cred;
    }

    msp->stats.credentials_issued++;
    msp->stats.credentials_active++;

    nimcp_mutex_unlock(msp->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_msp_revoke_credential(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    const char* reason
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    (void)reason;  /* For audit logging */

    /* P1-47: Mutex protection for credential operations */
    nimcp_mutex_lock(msp->mutex);

    credential_entry_t* entry = find_credential(msp, participant_id);
    if (!entry) {
        nimcp_mutex_unlock(msp->mutex);
        /* P2-146: Not-found is normal lookup result, not an error (false positive removal) */
        return NIMCP_ERROR_NOT_FOUND;
    }

    if (entry->credential.state == CREDENTIAL_STATE_VALID ||
        entry->credential.state == CREDENTIAL_STATE_SUSPENDED) {
        msp->stats.credentials_active--;
    }

    entry->credential.state = CREDENTIAL_STATE_REVOKED;
    msp->stats.credentials_revoked++;

    nimcp_mutex_unlock(msp->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_msp_suspend_credential(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    uint64_t duration_ms
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* P1-47: Mutex protection for credential operations */
    nimcp_mutex_lock(msp->mutex);

    credential_entry_t* entry = find_credential(msp, participant_id);
    if (!entry) {
        nimcp_mutex_unlock(msp->mutex);
        /* P2-146: Not-found is normal lookup result, not an error (false positive removal) */
        return NIMCP_ERROR_NOT_FOUND;
    }

    if (entry->credential.state != CREDENTIAL_STATE_VALID) {
        nimcp_mutex_unlock(msp->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "mesh_msp: invalid state");
        return NIMCP_ERROR_INVALID_STATE;
    }

    entry->credential.state = CREDENTIAL_STATE_SUSPENDED;
    entry->quarantine_end_ns = get_time_ns() + (duration_ms * 1000000ULL);
    msp->stats.credentials_suspended++;

    nimcp_mutex_unlock(msp->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_msp_restore_credential(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* P1-47: Mutex protection for credential operations */
    nimcp_mutex_lock(msp->mutex);

    credential_entry_t* entry = find_credential(msp, participant_id);
    if (!entry) {
        nimcp_mutex_unlock(msp->mutex);
        /* P2-146: Not-found is normal lookup result, not an error (false positive removal) */
        return NIMCP_ERROR_NOT_FOUND;
    }

    if (entry->credential.state != CREDENTIAL_STATE_SUSPENDED) {
        nimcp_mutex_unlock(msp->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "mesh_msp: invalid state");
        return NIMCP_ERROR_INVALID_STATE;
    }

    entry->credential.state = CREDENTIAL_STATE_VALID;
    entry->quarantine_end_ns = 0;
    if (msp->stats.credentials_suspended > 0) {
        msp->stats.credentials_suspended--;
    }

    nimcp_mutex_unlock(msp->mutex);
    return NIMCP_SUCCESS;
}

const credential_t* mesh_msp_get_credential(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_msp_get_credential: msp is NULL");
        return NULL;
    }

    /* P1-47: Mutex protection for credential list traversal */
    nimcp_mutex_lock(((mesh_msp_t*)msp)->mutex);

    credential_entry_t* entry = ((mesh_msp_t*)msp)->credentials_head;
    while (entry) {
        if (entry->participant_id == participant_id) {
            nimcp_mutex_unlock(((mesh_msp_t*)msp)->mutex);
            return &entry->credential;
        }
        entry = entry->next;
    }

    nimcp_mutex_unlock(((mesh_msp_t*)msp)->mutex);
    /* Not found is normal lookup result, not an error (P2: false positive removal) */
    return NULL;
}

bool mesh_msp_is_credential_valid(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id
) {
    const credential_t* cred = mesh_msp_get_credential(msp, participant_id);
    if (!cred) {
        /* Credential not found - validation rejection, not an error (P2: false positive removal) */
        return false;
    }

    if (cred->state != CREDENTIAL_STATE_VALID) {
        /* Invalid state is a validation rejection, not an error (P2: false positive removal) */
        return false;
    }

    uint64_t now = get_time_ns();
    if (cred->expires_at_ns > 0 && now > cred->expires_at_ns) {
        /* Expired credential is a validation rejection, not an error (P2: false positive removal) */
        return false;
    }

    return true;
}

nimcp_error_t mesh_msp_refresh_credential(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* P1-47: Mutex protection for credential operations */
    nimcp_mutex_lock(msp->mutex);

    credential_entry_t* entry = find_credential(msp, participant_id);
    if (!entry) {
        nimcp_mutex_unlock(msp->mutex);
        /* P2-146: Not-found is normal lookup result, not an error (false positive removal) */
        return NIMCP_ERROR_NOT_FOUND;
    }

    if (entry->credential.state != CREDENTIAL_STATE_VALID) {
        nimcp_mutex_unlock(msp->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "mesh_msp: invalid state");
        return NIMCP_ERROR_INVALID_STATE;
    }

    uint64_t now = get_time_ns();
    entry->credential.expires_at_ns = now + (msp->config.credential_expiration_ms * 1000000ULL);

    nimcp_mutex_unlock(msp->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Channel Membership
 * ============================================================================ */

nimcp_error_t mesh_msp_grant_channel_membership(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    mesh_channel_id_t channel_id
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* P1-47: Mutex protection for credential operations */
    nimcp_mutex_lock(msp->mutex);

    credential_entry_t* entry = find_credential(msp, participant_id);
    if (!entry) {
        nimcp_mutex_unlock(msp->mutex);
        /* P2-146: Not-found is normal lookup result, not an error (false positive removal) */
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Check if already member */
    for (size_t i = 0; i < entry->membership_count; i++) {
        if (entry->memberships[i] == channel_id) {
            nimcp_mutex_unlock(msp->mutex);
            return NIMCP_SUCCESS;
        }
    }

    if (entry->membership_count >= MESH_MSP_MAX_MEMBERSHIPS) {
        nimcp_mutex_unlock(msp->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_msp: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    entry->memberships[entry->membership_count++] = channel_id;
    nimcp_mutex_unlock(msp->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_msp_revoke_channel_membership(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    mesh_channel_id_t channel_id
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* P1-47: Mutex protection for credential operations */
    nimcp_mutex_lock(msp->mutex);

    credential_entry_t* entry = find_credential(msp, participant_id);
    if (!entry) {
        nimcp_mutex_unlock(msp->mutex);
        /* P2-146: Not-found is normal lookup result, not an error (false positive removal) */
        return NIMCP_ERROR_NOT_FOUND;
    }

    for (size_t i = 0; i < entry->membership_count; i++) {
        if (entry->memberships[i] == channel_id) {
            /* Shift remaining */
            for (size_t j = i; j < entry->membership_count - 1; j++) {
                entry->memberships[j] = entry->memberships[j + 1];
            }
            entry->membership_count--;
            nimcp_mutex_unlock(msp->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_mutex_unlock(msp->mutex);
    /* P2-146: Membership not found is normal lookup result, not an error (false positive removal) */
    return NIMCP_ERROR_NOT_FOUND;
}

bool mesh_msp_has_channel_membership(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    mesh_channel_id_t channel_id
) {
    if (!msp) {
        return false;
    }

    /* P1-47: Mutex protection for credential list traversal */
    nimcp_mutex_lock(((mesh_msp_t*)msp)->mutex);

    bool result = has_channel_membership_unlocked(msp, participant_id, channel_id);

    nimcp_mutex_unlock(((mesh_msp_t*)msp)->mutex);
    /* Participant not found - normal lookup result, not an error (P2: false positive removal) */
    return result;
}

nimcp_error_t mesh_msp_get_channel_memberships(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    mesh_channel_id_t* channels_out,
    size_t max_channels,
    size_t* count_out
) {
    if (!msp || !channels_out || !count_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    *count_out = 0;

    /* P1: Mutex protection for credential list traversal */
    nimcp_mutex_lock(((mesh_msp_t*)msp)->mutex);

    credential_entry_t* entry = ((mesh_msp_t*)msp)->credentials_head;
    while (entry) {
        if (entry->participant_id == participant_id) {
            size_t copy_count = entry->membership_count < max_channels ?
                                entry->membership_count : max_channels;
            memcpy(channels_out, entry->memberships, copy_count * sizeof(mesh_channel_id_t));
            *count_out = copy_count;
            nimcp_mutex_unlock(((mesh_msp_t*)msp)->mutex);
            return NIMCP_SUCCESS;
        }
        entry = entry->next;
    }

    nimcp_mutex_unlock(((mesh_msp_t*)msp)->mutex);
    /* P2-146: Not-found is normal lookup result, not an error (false positive removal) */
    return NIMCP_ERROR_NOT_FOUND;
}

/* ============================================================================
 * Authentication
 * ============================================================================ */

nimcp_error_t mesh_msp_authenticate(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    const uint8_t* signature,
    size_t signature_len
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* P1-47: Mutex protection for authentication */
    nimcp_mutex_lock(msp->mutex);

    msp->stats.auth_requests++;

    /* Check credential - use unlocked helpers to avoid deadlock */
    if (!is_credential_valid_unlocked(msp, participant_id)) {
        msp->stats.auth_denied++;
        nimcp_mutex_unlock(msp->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_UNAUTHORIZED, "mesh_msp: participant has no valid credential");
        return NIMCP_ERROR_UNAUTHORIZED;
    }

    /* Check quarantine */
    if (is_quarantined_unlocked(msp, participant_id)) {
        msp->stats.auth_denied++;
        nimcp_mutex_unlock(msp->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_ACCESS_DENIED, "mesh_msp: error condition");
        return NIMCP_ERROR_ACCESS_DENIED;
    }

    /* BBB signature verification if configured and signature provided */
    if (msp->config.require_signature && signature && signature_len > 0) {
        bbb_system_t bbb = (bbb_system_t)msp->bbb_handle;
        if (bbb) {
            const credential_t* cred = get_credential_unlocked(msp, participant_id);
            if (cred) {
                /* Verify signature using BBB */
                bool valid = bbb_verify_signature(
                    bbb,
                    cred->id,
                    MESH_CREDENTIAL_ID_SIZE,
                    signature,
                    signature_len
                );
                if (!valid) {
                    msp->stats.auth_denied++;

                    /* Route to immune system as potential threat */
                    brain_immune_system_t* immune =
                        (brain_immune_system_t*)msp->immune_handle;
                    if (immune) {
                        uint8_t epitope[32];
                        memcpy(epitope, &participant_id, sizeof(participant_id));
                        uint32_t antigen_id = 0;
                        brain_immune_present_antigen(
                            immune,
                            ANTIGEN_SOURCE_BBB,
                            epitope,
                            sizeof(participant_id),
                            7,  /* High severity for signature failure */
                            (uint32_t)participant_id,
                            &antigen_id
                        );
                    }

                    nimcp_mutex_unlock(msp->mutex);
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION, "mesh_msp: error condition");
                    return NIMCP_ERROR_BBB_VALIDATION;
                }
            }
        }
    }

    msp->stats.auth_granted++;
    nimcp_mutex_unlock(msp->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_msp_validate_transaction(
    mesh_msp_t* msp,
    const mesh_transaction_t* tx
) {
    if (!msp || !tx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Authenticate proposer */
    nimcp_error_t err = mesh_msp_authenticate(msp, tx->proposer_id, NULL, 0);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    /* BBB validation of transaction payload if configured */
    bbb_system_t bbb = (bbb_system_t)msp->bbb_handle;
    if (bbb && tx->payload && tx->payload_size > 0) {
        bbb_validation_result_t result;
        bool valid = bbb_validate_input(bbb, tx->payload, tx->payload_size, &result);
        if (!valid) {
            /* Route to immune system */
            brain_immune_system_t* immune =
                (brain_immune_system_t*)msp->immune_handle;
            if (immune) {
                uint8_t epitope[64];
                size_t epitope_len = 0;
                memcpy(epitope, &tx->proposer_id, sizeof(tx->proposer_id));
                epitope_len += sizeof(tx->proposer_id);
                memcpy(epitope + epitope_len, &result.threat, sizeof(result.threat));
                epitope_len += sizeof(result.threat);

                uint32_t antigen_id = 0;
                brain_immune_present_antigen(
                    immune,
                    ANTIGEN_SOURCE_BBB,
                    epitope,
                    epitope_len,
                    (uint32_t)result.severity + 3,  /* Map BBB severity to 1-10 */
                    (uint32_t)tx->proposer_id,
                    &antigen_id
                );

                /* Auto-quarantine on high severity threats */
                if (result.severity >= BBB_SEVERITY_HIGH &&
                    msp->config.enable_quarantine) {
                    mesh_msp_quarantine(
                        msp,
                        tx->proposer_id,
                        msp->config.quarantine_duration_ms
                    );
                }
            }

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION, "mesh_msp: error condition");
            return NIMCP_ERROR_BBB_VALIDATION;
        }
    }

    /* Check channel membership */
    if (!mesh_msp_has_channel_membership(msp, tx->proposer_id, tx->target_channel)) {
        /* Auto-grant for home channel */
        if (tx->source_channel != tx->target_channel) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_ACCESS_DENIED, "mesh_msp: error condition");
            return NIMCP_ERROR_ACCESS_DENIED;
        }
    }

    /* Check capability */
    if (!mesh_msp_check_capability(msp, tx->proposer_id, MESH_CAP_PROPOSE)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_PERMISSION_DENIED, "mesh_msp: error condition");
        return NIMCP_ERROR_PERMISSION_DENIED;
    }

    /* Check cross-channel */
    if (tx->is_cross_channel) {
        if (!mesh_msp_check_capability(msp, tx->proposer_id, MESH_CAP_CROSS_CHANNEL)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_PERMISSION_DENIED, "mesh_msp: error condition");
            return NIMCP_ERROR_PERMISSION_DENIED;
        }
    }

    /* Check emergency */
    if (tx->is_emergency) {
        if (!mesh_msp_check_capability(msp, tx->proposer_id, MESH_CAP_EMERGENCY)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_PERMISSION_DENIED, "mesh_msp: error condition");
            return NIMCP_ERROR_PERMISSION_DENIED;
        }
    }

    return NIMCP_SUCCESS;
}

bool mesh_msp_check_capability(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    uint64_t capability
) {
    const credential_t* cred = mesh_msp_get_credential(msp, participant_id);
    if (!cred) {
        /* Credential not found - check fails, not an error (P2: false positive removal) */
        return false;
    }

    return (cred->capabilities & capability) == capability;
}

bool mesh_msp_check_privilege(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    uint32_t min_level
) {
    const credential_t* cred = mesh_msp_get_credential(msp, participant_id);
    if (!cred) {
        /* Credential not found - check fails, not an error (P2: false positive removal) */
        return false;
    }

    return cred->privilege_level >= min_level;
}

/* ============================================================================
 * Policy Management
 * ============================================================================ */

nimcp_error_t mesh_msp_add_policy(
    mesh_msp_t* msp,
    const msp_access_policy_t* policy
) {
    if (!msp || !policy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* P1: Mutex protection for policy list modification */
    nimcp_mutex_lock(msp->mutex);

    /* Check for duplicate */
    if (policy->policy_name && find_policy(msp, policy->policy_name)) {
        nimcp_mutex_unlock(msp->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_ALREADY_EXISTS, "mesh_msp: error condition");
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    policy_entry_t* entry = nimcp_calloc(1, sizeof(policy_entry_t));
    if (!entry) {
        nimcp_mutex_unlock(msp->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_msp: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    entry->policy = *policy;

    /* Copy required channels if present */
    if (policy->required_channels && policy->required_channel_count > 0) {
        entry->policy.required_channels = nimcp_calloc(policy->required_channel_count,
                                                  sizeof(mesh_channel_id_t));
        if (entry->policy.required_channels) {
            memcpy((void*)entry->policy.required_channels, policy->required_channels,
                   policy->required_channel_count * sizeof(mesh_channel_id_t));
        }
    }

    entry->next = msp->policies_head;
    msp->policies_head = entry;
    msp->policy_count++;

    nimcp_mutex_unlock(msp->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_msp_remove_policy(
    mesh_msp_t* msp,
    const char* policy_name
) {
    if (!msp || !policy_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* P1: Mutex protection for policy list modification */
    nimcp_mutex_lock(msp->mutex);

    policy_entry_t* prev = NULL;
    policy_entry_t* entry = msp->policies_head;

    while (entry) {
        if (entry->policy.policy_name && strcmp(entry->policy.policy_name, policy_name) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                msp->policies_head = entry->next;
            }
            nimcp_free((void*)entry->policy.required_channels);
            nimcp_free(entry);
            msp->policy_count--;
            nimcp_mutex_unlock(msp->mutex);
            return NIMCP_SUCCESS;
        }
        prev = entry;
        entry = entry->next;
    }

    nimcp_mutex_unlock(msp->mutex);
    return NIMCP_ERROR_NOT_FOUND;
}

bool mesh_msp_evaluate_policy(
    const mesh_msp_t* msp,
    const char* policy_name,
    mesh_participant_id_t participant_id,
    const mesh_transaction_t* tx
) {
    if (!msp || !policy_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_msp_evaluate_policy: required parameter is NULL (msp, policy_name)");
        return false;
    }

    policy_entry_t* entry = find_policy((mesh_msp_t*)msp, policy_name);
    if (!entry) {
        /* Policy not found - evaluation fails, not an error (P2: false positive removal) */
        return false;
    }

    const msp_access_policy_t* policy = &entry->policy;
    const credential_t* cred = mesh_msp_get_credential(msp, participant_id);

    switch (policy->type) {
        case MSP_POLICY_ALLOW_ALL:
            return true;

        case MSP_POLICY_DENY_ALL:
            /* Deny is a policy decision, not an error (P2: false positive removal) */
            return false;

        case MSP_POLICY_PRIVILEGE_LEVEL:
            return cred && cred->privilege_level >= policy->min_privilege_level;

        case MSP_POLICY_CAPABILITY_CHECK:
            return cred && (cred->capabilities & policy->required_capabilities) ==
                           policy->required_capabilities;

        case MSP_POLICY_CHANNEL_MEMBERSHIP:
            if (!policy->required_channels) return true;
            for (size_t i = 0; i < policy->required_channel_count; i++) {
                if (!mesh_msp_has_channel_membership(msp, participant_id,
                                                      policy->required_channels[i])) {
                    /* Membership check failed - policy rejection, not error (P2: false positive removal) */
                    return false;
                }
            }
            return true;

        case MSP_POLICY_CUSTOM:
            if (entry->callback) {
                return entry->callback((mesh_msp_t*)msp, cred, tx, entry->callback_ctx);
            }
            return false;

        default:
            /* Unknown policy type - fail closed (P2: false positive removal) */
            return false;
    }
}

nimcp_error_t mesh_msp_set_policy_callback(
    mesh_msp_t* msp,
    const char* policy_name,
    msp_policy_callback_t callback,
    void* ctx
) {
    if (!msp || !policy_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    policy_entry_t* entry = find_policy(msp, policy_name);
    if (!entry) {
        return NIMCP_ERROR_NOT_FOUND;
    }

    entry->callback = callback;
    entry->callback_ctx = ctx;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Quarantine
 * ============================================================================ */

nimcp_error_t mesh_msp_quarantine(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    uint64_t duration_ms
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* P1-47: Mutex protection for credential operations */
    nimcp_mutex_lock(msp->mutex);

    credential_entry_t* entry = find_credential(msp, participant_id);
    if (!entry) {
        /* Create minimal entry for tracking */
        entry = nimcp_calloc(1, sizeof(credential_entry_t));
        if (!entry) {
            nimcp_mutex_unlock(msp->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_msp: memory allocation failed");
            return NIMCP_ERROR_NO_MEMORY;
        }
        entry->participant_id = participant_id;
        entry->credential.participant_id = participant_id;
        entry->credential.state = CREDENTIAL_STATE_NONE;
        entry->next = msp->credentials_head;
        msp->credentials_head = entry;
        msp->credential_count++;
    }

    entry->quarantined = true;
    entry->quarantine_end_ns = get_time_ns() + (duration_ms * 1000000ULL);

    msp->stats.quarantine_events++;

    nimcp_mutex_unlock(msp->mutex);

    /* Route quarantine through immune system (outside lock - external calls) */
    brain_immune_system_t* immune = (brain_immune_system_t*)msp->immune_handle;
    if (immune) {
        /* Notify immune system of quarantine action */
        brain_immune_handle_bft_quarantine(
            immune,
            (uint32_t)participant_id,
            duration_ms,
            0.3f  /* Trust score drops on quarantine */
        );
    }

    /* Quarantine in BBB memory regions if BBB is connected */
    bbb_system_t bbb = (bbb_system_t)msp->bbb_handle;
    if (bbb) {
        /* Report as a threat to BBB for tracking */
        bbb_report_threat(
            bbb,
            BBB_THREAT_UNAUTHORIZED_ACCESS,
            BBB_SEVERITY_MEDIUM,
            "MSP quarantine issued",
            NULL,
            &participant_id,
            sizeof(participant_id)
        );
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_msp_release_quarantine(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* P1-47: Mutex protection for credential operations */
    nimcp_mutex_lock(msp->mutex);

    credential_entry_t* entry = find_credential(msp, participant_id);
    if (!entry) {
        nimcp_mutex_unlock(msp->mutex);
        /* P2-146: Not-found is normal lookup result, not an error (false positive removal) */
        return NIMCP_ERROR_NOT_FOUND;
    }

    entry->quarantined = false;
    entry->quarantine_end_ns = 0;

    msp->stats.recovery_events++;

    nimcp_mutex_unlock(msp->mutex);

    /* Notify immune system of trust recovery (outside lock - external calls) */
    brain_immune_system_t* immune = (brain_immune_system_t*)msp->immune_handle;
    if (immune) {
        brain_immune_handle_bft_trust_recovery(
            immune,
            (uint32_t)participant_id,
            0.3f,  /* Old trust during quarantine */
            0.7f   /* Restored trust */
        );
    }

    return NIMCP_SUCCESS;
}

bool mesh_msp_is_quarantined(
    const mesh_msp_t* msp,
    mesh_participant_id_t participant_id
) {
    if (!msp) {
        return false;
    }

    /* P1-47: Mutex protection for credential list traversal */
    nimcp_mutex_lock(((mesh_msp_t*)msp)->mutex);

    credential_entry_t* entry = ((mesh_msp_t*)msp)->credentials_head;
    while (entry) {
        if (entry->participant_id == participant_id) {
            if (!entry->quarantined) {
                nimcp_mutex_unlock(((mesh_msp_t*)msp)->mutex);
                /* Not quarantined is normal state, not an error (P2: false positive removal) */
                return false;
            }
            /* Check if quarantine expired */
            uint64_t now = get_time_ns();
            if (entry->quarantine_end_ns > 0 && now > entry->quarantine_end_ns) {
                nimcp_mutex_unlock(((mesh_msp_t*)msp)->mutex);
                /* Quarantine expired is normal result, not an error (P2: false positive removal) */
                return false;
            }
            nimcp_mutex_unlock(((mesh_msp_t*)msp)->mutex);
            return true;
        }
        entry = entry->next;
    }

    nimcp_mutex_unlock(((mesh_msp_t*)msp)->mutex);
    /* Participant not found - not quarantined, not an error (P2: false positive removal) */
    return false;
}

nimcp_error_t mesh_msp_set_immune_callback(
    mesh_msp_t* msp,
    msp_immune_callback_t callback,
    void* ctx
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    msp->immune_callback = callback;
    msp->immune_callback_ctx = ctx;

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_msp_handle_immune_event(
    mesh_msp_t* msp,
    mesh_participant_id_t participant_id,
    int event_type
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    msp->stats.threat_events++;

    /* Event types: 0=threat, 1=chronic_failure, 2=recovery */
    switch (event_type) {
        case 0: /* Threat detected */
            mesh_msp_quarantine(msp, participant_id, msp->config.quarantine_duration_ms);
            break;

        case 1: /* Chronic failure */
            mesh_msp_revoke_credential(msp, participant_id, "chronic_immune_failure");
            break;

        case 2: /* Recovery */
            mesh_msp_release_quarantine(msp, participant_id);
            break;

        default:
            break;
    }

    /* Invoke callback */
    if (msp->immune_callback) {
        msp->immune_callback(msp, participant_id, event_type, msp->immune_callback_ctx);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Update
 * ============================================================================ */

nimcp_error_t mesh_msp_update(
    mesh_msp_t* msp,
    uint64_t delta_ms
) {
    if (!msp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    (void)delta_ms;

    /* P1: Mutex protection for credential list traversal and modification */
    nimcp_mutex_lock(msp->mutex);

    uint64_t now = get_time_ns();

    /* Check credential expirations and quarantine timeouts */
    credential_entry_t* entry = msp->credentials_head;
    while (entry) {
        /* Check expiration */
        if (entry->credential.state == CREDENTIAL_STATE_VALID &&
            entry->credential.expires_at_ns > 0 &&
            now > entry->credential.expires_at_ns) {
            entry->credential.state = CREDENTIAL_STATE_EXPIRED;
            if (msp->stats.credentials_active > 0) {
                msp->stats.credentials_active--;
            }
        }

        /* Check quarantine timeout */
        if (entry->quarantined &&
            entry->quarantine_end_ns > 0 &&
            now > entry->quarantine_end_ns) {
            entry->quarantined = false;
            msp->stats.recovery_events++;
        }

        /* Check suspension timeout */
        if (entry->credential.state == CREDENTIAL_STATE_SUSPENDED &&
            entry->quarantine_end_ns > 0 &&
            now > entry->quarantine_end_ns) {
            entry->credential.state = CREDENTIAL_STATE_VALID;
            if (msp->stats.credentials_suspended > 0) {
                msp->stats.credentials_suspended--;
            }
        }

        entry = entry->next;
    }

    nimcp_mutex_unlock(msp->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

nimcp_error_t mesh_msp_get_stats(
    const mesh_msp_t* msp,
    mesh_msp_stats_t* stats
) {
    if (!msp || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_msp: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    *stats = msp->stats;
    return NIMCP_SUCCESS;
}

void mesh_msp_reset_stats(mesh_msp_t* msp) {
    if (!msp) {
        return;
    }

    memset(&msp->stats, 0, sizeof(msp->stats));

    /* Recount active credentials */
    credential_entry_t* entry = msp->credentials_head;
    while (entry) {
        if (entry->credential.state == CREDENTIAL_STATE_VALID) {
            msp->stats.credentials_active++;
        }
        entry = entry->next;
    }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

void mesh_msp_print_status(const mesh_msp_t* msp) {
    if (!msp) {
        printf("MSP: NULL\n");
        return;
    }

    printf("=== MSP: %s ===\n", msp->name);
    printf("  Credentials: %zu\n", msp->credential_count);
    printf("  Active: %lu\n", (unsigned long)msp->stats.credentials_active);
    printf("  Suspended: %lu\n", (unsigned long)msp->stats.credentials_suspended);
    printf("  Revoked: %lu\n", (unsigned long)msp->stats.credentials_revoked);
    printf("  Policies: %zu\n", msp->policy_count);
    printf("  Auth requests: %lu granted, %lu denied\n",
           (unsigned long)msp->stats.auth_granted,
           (unsigned long)msp->stats.auth_denied);
}

void mesh_msp_print_credential(const credential_t* credential) {
    if (!credential) {
        printf("Credential: NULL\n");
        return;
    }

    printf("=== Credential ===\n");
    printf("  Participant: %lu\n", (unsigned long)credential->participant_id);
    printf("  State: %s\n", mesh_credential_state_to_string(credential->state));
    printf("  Privilege: %u\n", credential->privilege_level);
    printf("  Capabilities: 0x%lx\n", (unsigned long)credential->capabilities);
}
