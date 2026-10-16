/**
 * @file nimcp_security_level.c
 * @brief NIMCP Security Level Management Implementation
 */

#include "security/nimcp_security_level.h"
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"

#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_level, MESH_ADAPTER_CATEGORY_SECURITY)


/* Component entry in hash table */
typedef struct nimcp_component_level {
    char name[NIMCP_ID_BUFFER_SIZE];                             /* Component name */
    nimcp_security_level_t level;              /* Component level */
    struct nimcp_component_level* next;        /* Hash chain */
} nimcp_component_level_t;

/* Security state internal structure */
struct nimcp_security_state {
    uint32_t magic;                            /* Validation magic */
    nimcp_security_level_t global_level;       /* Global security level */
    bool is_locked;                            /* Level locked flag */

    /* Component-specific levels */
    nimcp_component_level_t** component_table; /* Hash table */
    size_t component_table_size;               /* Table size */
    size_t component_count;                    /* Number of components */

    /* Audit trail */
    nimcp_security_audit_entry_t* audit_trail; /* Circular buffer */
    size_t audit_size;                         /* Buffer size */
    size_t audit_head;                         /* Next write position */
    size_t audit_count;                        /* Number of entries */

    /* Statistics */
    uint64_t level_upgrades;
    uint64_t level_downgrades_blocked;
    uint64_t emergency_overrides;
    uint64_t component_levels_set;
    uint64_t feature_queries;

    /* Bio-async integration */
    bio_router_t router;
    bio_module_context_t bio_ctx;

    /* Thread safety */
    nimcp_mutex_t mutex;

    void* user_data;
};

/* Feature enablement table: [level][feature] */
static const bool feature_table[5][NIMCP_SECURITY_FEATURE_COUNT] = {
    /* MINIMAL */
    {false, false, false, false, false, false, false, false, false, false},
    /* STANDARD */
    {true, true, true, false, false, false, false, false, false, false},
    /* ELEVATED */
    {true, true, true, true, true, false, true, false, true, false},
    /* MAXIMUM */
    {true, true, true, true, true, true, true, true, true, true},
    /* PARANOID */
    {true, true, true, true, true, true, true, true, true, true}
};

/* Level names for logging */
static const char* level_names[] = {
    "MINIMAL",
    "STANDARD",
    "ELEVATED",
    "MAXIMUM",
    "PARANOID"
};

/**
 * Hash function for component names
 *
 * WHAT: Simple string hash
 * WHY: Distribute components across table
 * HOW: DJB2 algorithm
 */
static size_t hash_component_name(const char* name, size_t table_size) {
    size_t hash = 5381;
    int c;

    while ((c = *name++)) {
        hash = ((hash << 5) + hash) + c;
    }

    return hash % table_size;
}

/**
 * Add audit entry
 *
 * WHAT: Records security level change
 * WHY: Audit trail and forensics
 * HOW: Circular buffer insert
 */
static void add_audit_entry(
    nimcp_security_state_t state,
    nimcp_security_level_t old_level,
    nimcp_security_level_t new_level,
    const char* component,
    const char* reason,
    const char* authorization,
    bool is_override
) {
    if (!state || !state->audit_trail) return;

    nimcp_security_audit_entry_t* entry = &state->audit_trail[state->audit_head];

    entry->timestamp = time(NULL);
    entry->old_level = old_level;
    entry->new_level = new_level;

    if (component) {
        strncpy(entry->component, component, sizeof(entry->component) - 1);
        entry->component[sizeof(entry->component) - 1] = '\0';
    } else {
        entry->component[0] = '\0';
    }

    if (reason) {
        strncpy(entry->reason, reason, sizeof(entry->reason) - 1);
        entry->reason[sizeof(entry->reason) - 1] = '\0';
    } else {
        entry->reason[0] = '\0';
    }

    if (authorization) {
        strncpy(entry->authorization, authorization, sizeof(entry->authorization) - 1);
        entry->authorization[sizeof(entry->authorization) - 1] = '\0';
    } else {
        entry->authorization[0] = '\0';
    }

    entry->is_override = is_override;

    state->audit_head = (state->audit_head + 1) % state->audit_size;
    if (state->audit_count < state->audit_size) {
        state->audit_count++;
    }
}

/**
 * Send bio-async notification
 *
 * WHAT: Notifies system of level change
 * WHY: Other modules need to react to security changes
 * HOW: Posts message to bio-router
 */
static void send_level_change_notification(
    nimcp_security_state_t state,
    nimcp_security_level_t old_level,
    nimcp_security_level_t new_level,
    const char* component
) {
    if (!state->bio_ctx) return;

    char msg[NIMCP_ERROR_BUFFER_SIZE];
    if (component) {
        snprintf(msg, sizeof(msg),
                "Security level change: %s -> %s (component: %s)",
                level_names[old_level], level_names[new_level], component);
    } else {
        snprintf(msg, sizeof(msg),
                "Global security level change: %s -> %s",
                level_names[old_level], level_names[new_level]);
    }

    /* Broadcast level change to interested modules */
    bio_router_broadcast(state->bio_ctx, msg, strlen(msg) + 1);
}

/**
 * Bio-async message handler
 *
 * WHAT: Handles bio-async messages
 * WHY: React to system events
 * HOW: Processes commands and queries
 */
static nimcp_error_t security_level_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)response_promise;
    (void)user_data;
    if (!msg) return NIMCP_ERROR_INVALID_PARAMETER;

    /* Handle security level queries */
    LOG_DEBUG("Security level handler received message size %zu", msg_size);

    return NIMCP_SUCCESS;
}

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * Called by the orchestrator with discovered message types from the knowledge graph.
 * Registers handlers based on message types discovered at runtime.
 *
 * @param ctx Bio-async module context
 * @param message_types Array of discovered message types
 * @param message_count Number of message types
 * @param user_data User-provided context
 * @return 0 on success, -1 on error
 */
static int security_level_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_SECURITY_EVENT:
                bio_router_register_handler(ctx, message_types[i], security_level_message_handler);
                registered++;
                break;
            default:
                LOG_DEBUG("Unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_INFO("KG-driven wiring callback registered %d handlers", registered);
    return (registered > 0) ? 0 : -1;
}

nimcp_security_state_t nimcp_security_state_create(const nimcp_security_state_config_t* config) {
    nimcp_security_state_t state = nimcp_calloc(1, sizeof(struct nimcp_security_state));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_security_state_create: failed to allocate state");
        return NULL;
    }

    state->magic = NIMCP_SECURITY_STATE_MAGIC;

    /* Set defaults or use config */
    if (config) {
        state->global_level = config->initial_level;
        state->is_locked = config->lock_on_create;
        state->component_table_size = config->max_components > 0 ? config->max_components : 64;
        state->audit_size = config->max_audit_entries > 0 ? config->max_audit_entries : 1024;
        state->router = config->router ? *config->router : NULL;
        state->user_data = config->user_data;
    } else {
        state->global_level = NIMCP_SECURITY_LEVEL_STANDARD;
        state->is_locked = false;
        state->component_table_size = 64;
        state->audit_size = 1024;
        state->router = NULL;
        state->user_data = NULL;
    }

    /* Allocate component hash table */
    state->component_table = nimcp_calloc(state->component_table_size, sizeof(nimcp_component_level_t*));
    if (!state->component_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_security_state_create: failed to allocate component table");
        nimcp_free(state);
        return NULL;
    }

    /* Allocate audit trail */
    state->audit_trail = nimcp_calloc(state->audit_size, sizeof(nimcp_security_audit_entry_t));
    if (!state->audit_trail) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_security_state_create: failed to allocate audit trail");
        nimcp_free(state->component_table);
        nimcp_free(state);
        return NULL;
    }

    /* Initialize mutex */
    if (nimcp_mutex_init(&state->mutex, NULL) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_security_state_create: failed to init mutex");
        nimcp_free(state->audit_trail);
        nimcp_free(state->component_table);
        nimcp_free(state);
        return NULL;
    }

    /* Register with bio-async if router provided */
    if (state->router) {
        bio_module_info_t mod_info = {
            .module_id = BIO_MODULE_SECURITY,
            .module_name = "security_level",
            .inbox_capacity = 32,
            .user_data = state
        };
        state->bio_ctx = bio_router_register_module(&mod_info);
        if (state->bio_ctx) {
            // Try KG-driven wiring callback registration first
            nimcp_error_t result = bio_router_register_wiring_callback(
                BIO_MODULE_SECURITY,
                (void*)security_level_wiring_handler_callback,
                state
            );

            if (result == NIMCP_SUCCESS) {
                LOG_INFO("KG-driven wiring callback registered successfully");
            } else {
                // Fallback to legacy handler registration
                LOG_INFO("Falling back to legacy handler registration");

                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(state->bio_ctx, BIO_MSG_SECURITY_EVENT,
                                                security_level_message_handler)
                );
            }
        }
    }

    LOG_INFO("Security state created: initial level=%s, locked=%d",
             level_names[state->global_level], state->is_locked);

    /* Log creation in audit trail */
    add_audit_entry(state, NIMCP_SECURITY_LEVEL_MINIMAL, state->global_level,
                   NULL, "Security state created", NULL, false);

    return state;
}

void nimcp_security_state_destroy(nimcp_security_state_t state) {
    if (!state) return;

    if (state->magic != NIMCP_SECURITY_STATE_MAGIC) {
        LOG_ERROR("Invalid security state magic: 0x%08x", state->magic);
        return;
    }

    nimcp_mutex_lock(&state->mutex);

    /* Unregister from bio-async */
    if (state->bio_ctx) {
        bio_router_unregister_module(state->bio_ctx);
    }

    /* Free component table */
    if (state->component_table) {
        for (size_t i = 0; i < state->component_table_size; i++) {
            nimcp_component_level_t* comp = state->component_table[i];
            while (comp) {
                nimcp_component_level_t* next = comp->next;
                nimcp_free(comp);
                comp = next;
            }
        }
        nimcp_free(state->component_table);
    }

    /* Zero and free audit trail (contains sensitive data) */
    if (state->audit_trail) {
        memset(state->audit_trail, 0, state->audit_size * sizeof(nimcp_security_audit_entry_t));
        nimcp_free(state->audit_trail);
    }

    LOG_INFO("Security state destroyed: final level=%s, upgrades=%lu, overrides=%lu",
             level_names[state->global_level],
             (unsigned long)state->level_upgrades,
             (unsigned long)state->emergency_overrides);

    state->magic = 0;

    nimcp_mutex_unlock(&state->mutex);
    nimcp_mutex_destroy(&state->mutex);

    nimcp_free(state);
}

nimcp_error_t nimcp_security_set_level(
    nimcp_security_state_t state,
    nimcp_security_level_t level
) {
    if (!state || state->magic != NIMCP_SECURITY_STATE_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (level < NIMCP_SECURITY_LEVEL_MINIMAL || level > NIMCP_SECURITY_LEVEL_PARANOID) {
        LOG_ERROR("Invalid security level: %d", level);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&state->mutex);

    nimcp_security_level_t old_level = state->global_level;

    /* Check if locked */
    if (state->is_locked) {
        LOG_WARN("Attempt to change locked security level");
        state->level_downgrades_blocked++;
        nimcp_mutex_unlock(&state->mutex);
        return NIMCP_ERROR_PERMISSION_DENIED;
    }

    /* Only allow upgrades */
    if (level < state->global_level) {
        LOG_WARN("Attempt to downgrade security level: %s -> %s",
                 level_names[state->global_level], level_names[level]);
        state->level_downgrades_blocked++;
        nimcp_mutex_unlock(&state->mutex);
        return NIMCP_ERROR_PERMISSION_DENIED;
    }

    /* No change needed */
    if (level == state->global_level) {
        nimcp_mutex_unlock(&state->mutex);
        return NIMCP_SUCCESS;
    }

    /* Upgrade allowed */
    state->global_level = level;
    state->level_upgrades++;

    LOG_INFO("Security level upgraded: %s -> %s",
             level_names[old_level], level_names[level]);

    /* Add audit entry */
    add_audit_entry(state, old_level, level, NULL, "Level upgrade", NULL, false);

    /* Send notification */
    send_level_change_notification(state, old_level, level, NULL);

    nimcp_mutex_unlock(&state->mutex);

    return NIMCP_SUCCESS;
}

nimcp_security_level_t nimcp_security_get_level(nimcp_security_state_t state) {
    if (!state || state->magic != NIMCP_SECURITY_STATE_MAGIC) {
        return NIMCP_SECURITY_LEVEL_MINIMAL;
    }

    nimcp_mutex_lock(&state->mutex);
    nimcp_security_level_t level = state->global_level;
    nimcp_mutex_unlock(&state->mutex);

    return level;
}

nimcp_error_t nimcp_security_lock_level(nimcp_security_state_t state) {
    if (!state || state->magic != NIMCP_SECURITY_STATE_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&state->mutex);

    if (state->is_locked) {
        nimcp_mutex_unlock(&state->mutex);
        return NIMCP_SUCCESS;  /* Already locked */
    }

    state->is_locked = true;

    LOG_INFO("Security level locked at %s", level_names[state->global_level]);

    /* Add audit entry */
    add_audit_entry(state, state->global_level, state->global_level,
                   NULL, "Security level locked", NULL, false);

    nimcp_mutex_unlock(&state->mutex);

    return NIMCP_SUCCESS;
}

bool nimcp_security_is_locked(nimcp_security_state_t state) {
    if (!state || state->magic != NIMCP_SECURITY_STATE_MAGIC) {
        return false;
    }

    nimcp_mutex_lock(&state->mutex);
    bool locked = state->is_locked;
    nimcp_mutex_unlock(&state->mutex);

    return locked;
}

nimcp_error_t nimcp_security_set_component_level(
    nimcp_security_state_t state,
    const char* component,
    nimcp_security_level_t level
) {
    if (!state || state->magic != NIMCP_SECURITY_STATE_MAGIC || !component) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (level < NIMCP_SECURITY_LEVEL_MINIMAL || level > NIMCP_SECURITY_LEVEL_PARANOID) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&state->mutex);

    /* Component level cannot be lower than global level */
    if (level < state->global_level) {
        LOG_WARN("Component level cannot be lower than global: %s < %s",
                 level_names[level], level_names[state->global_level]);
        nimcp_mutex_unlock(&state->mutex);
        return NIMCP_ERROR_PERMISSION_DENIED;
    }

    /* Find or create component entry */
    size_t hash = hash_component_name(component, state->component_table_size);
    nimcp_component_level_t* comp = state->component_table[hash];
    nimcp_component_level_t* prev = NULL;

    while (comp) {
        if (strcmp(comp->name, component) == 0) {
            /* Found existing entry - only allow upgrades */
            if (level < comp->level) {
                LOG_WARN("Cannot downgrade component level: %s %s -> %s",
                         component, level_names[comp->level], level_names[level]);
                state->level_downgrades_blocked++;
                nimcp_mutex_unlock(&state->mutex);
                return NIMCP_ERROR_PERMISSION_DENIED;
            }

            nimcp_security_level_t old_level = comp->level;
            comp->level = level;
            state->component_levels_set++;

            LOG_INFO("Component level upgraded: %s %s -> %s",
                     component, level_names[old_level], level_names[level]);

            add_audit_entry(state, old_level, level, component,
                           "Component level upgrade", NULL, false);
            send_level_change_notification(state, old_level, level, component);

            nimcp_mutex_unlock(&state->mutex);
            return NIMCP_SUCCESS;
        }
        prev = comp;
        comp = comp->next;
    }

    /* Create new component entry */
    comp = nimcp_calloc(1, sizeof(nimcp_component_level_t));
    if (!comp) {
        LOG_ERROR("Failed to allocate component entry");
        nimcp_mutex_unlock(&state->mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    strncpy(comp->name, component, sizeof(comp->name) - 1);
    comp->name[sizeof(comp->name) - 1] = '\0';
    comp->level = level;
    comp->next = NULL;

    if (prev) {
        prev->next = comp;
    } else {
        state->component_table[hash] = comp;
    }

    state->component_count++;
    state->component_levels_set++;

    LOG_INFO("Component level set: %s -> %s", component, level_names[level]);

    add_audit_entry(state, state->global_level, level, component,
                   "Component level set", NULL, false);
    send_level_change_notification(state, state->global_level, level, component);

    nimcp_mutex_unlock(&state->mutex);

    return NIMCP_SUCCESS;
}

nimcp_security_level_t nimcp_security_get_component_level(
    nimcp_security_state_t state,
    const char* component
) {
    if (!state || state->magic != NIMCP_SECURITY_STATE_MAGIC || !component) {
        return NIMCP_SECURITY_LEVEL_MINIMAL;
    }

    nimcp_mutex_lock(&state->mutex);

    /* Look up component */
    size_t hash = hash_component_name(component, state->component_table_size);
    nimcp_component_level_t* comp = state->component_table[hash];

    while (comp) {
        if (strcmp(comp->name, component) == 0) {
            nimcp_security_level_t level = comp->level;
            nimcp_mutex_unlock(&state->mutex);
            return level;
        }
        comp = comp->next;
    }

    /* Component not found - inherit global level */
    nimcp_security_level_t level = state->global_level;
    nimcp_mutex_unlock(&state->mutex);

    return level;
}

nimcp_error_t nimcp_security_emergency_override(
    nimcp_security_state_t state,
    nimcp_security_level_t level,
    const char* authorization_token,
    const char* reason
) {
    if (!state || state->magic != NIMCP_SECURITY_STATE_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (level < NIMCP_SECURITY_LEVEL_MINIMAL || level > NIMCP_SECURITY_LEVEL_PARANOID) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&state->mutex);

    /* Validate authorization token if provided */
    if (authorization_token) {
        /* WHAT: Simple token validation
         * WHY: In production, integrate with real auth system
         * HOW: Check against expected token format
         */
        if (strlen(authorization_token) < 16) {
            LOG_ERROR("Invalid authorization token for emergency override");
            nimcp_mutex_unlock(&state->mutex);
            return NIMCP_ERROR_PERMISSION_DENIED;
        }
    }

    nimcp_security_level_t old_level = state->global_level;

    state->global_level = level;
    state->emergency_overrides++;
    state->is_locked = false;  /* Override unlocks */

    LOG_WARN("EMERGENCY OVERRIDE: %s -> %s, reason: %s",
             level_names[old_level], level_names[level],
             reason ? reason : "none");

    /* Add audit entry */
    add_audit_entry(state, old_level, level, NULL, reason, authorization_token, true);

    /* Send notification */
    send_level_change_notification(state, old_level, level, NULL);

    nimcp_mutex_unlock(&state->mutex);

    return NIMCP_SUCCESS;
}

bool nimcp_security_feature_enabled(
    nimcp_security_state_t state,
    nimcp_security_feature_t feature
) {
    if (!state || state->magic != NIMCP_SECURITY_STATE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_security_feature_enabled: state is NULL");
        return false;
    }

    if (feature >= NIMCP_SECURITY_FEATURE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_security_feature_enabled: capacity exceeded");
        return false;
    }

    nimcp_mutex_lock(&state->mutex);

    nimcp_security_level_t level = state->global_level;
    bool enabled = feature_table[level][feature];

    state->feature_queries++;

    nimcp_mutex_unlock(&state->mutex);

    return enabled;
}

nimcp_error_t nimcp_security_get_audit_trail(
    nimcp_security_state_t state,
    nimcp_security_audit_entry_t* entries,
    size_t max_entries,
    size_t* count_out
) {
    if (!state || state->magic != NIMCP_SECURITY_STATE_MAGIC || !entries || !count_out) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&state->mutex);

    size_t count = state->audit_count < max_entries ? state->audit_count : max_entries;

    /* Copy from circular buffer */
    size_t start = state->audit_count < state->audit_size ?
                   0 : state->audit_head;

    for (size_t i = 0; i < count; i++) {
        size_t idx = (start + i) % state->audit_size;
        memcpy(&entries[i], &state->audit_trail[idx], sizeof(nimcp_security_audit_entry_t));
    }

    *count_out = count;

    nimcp_mutex_unlock(&state->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_security_level_get_stats(
    nimcp_security_state_t state,
    nimcp_security_state_stats_t* stats
) {
    if (!state || state->magic != NIMCP_SECURITY_STATE_MAGIC || !stats) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&state->mutex);

    stats->level_upgrades = state->level_upgrades;
    stats->level_downgrades_blocked = state->level_downgrades_blocked;
    stats->emergency_overrides = state->emergency_overrides;
    stats->component_levels_set = state->component_levels_set;
    stats->feature_queries = state->feature_queries;
    stats->current_level = state->global_level;
    stats->is_locked = state->is_locked;
    stats->component_count = state->component_count;
    stats->audit_entry_count = state->audit_count;

    nimcp_mutex_unlock(&state->mutex);

    return NIMCP_SUCCESS;
}

const char* nimcp_security_level_name(nimcp_security_level_t level) {
    if (level < NIMCP_SECURITY_LEVEL_MINIMAL || level > NIMCP_SECURITY_LEVEL_PARANOID) {
        return "UNKNOWN";
    }
    return level_names[level];
}
