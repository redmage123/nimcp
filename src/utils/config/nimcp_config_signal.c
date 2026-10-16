#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_config_signal.c - Atomic Config Reload Implementation
//=============================================================================
/**
 * @file nimcp_config_signal.c
 * @brief Implementation of atomic configuration reload with rollback
 *
 * WHAT: Thread-safe atomic config reload with snapshot-based rollback
 * WHY:  Safe hot-reload for production systems
 * HOW:  CoW snapshots, validation callbacks, atomic swap
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#include "utils/config/nimcp_config_signal.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/config/nimcp_dynamic_config.h"
#include "utils/config/nimcp_config_array.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_rwlock.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(config_signal)

//=============================================================================
// Internal Data Structures
//=============================================================================

#define MAX_CONFIG_ENTRIES 256
#define MAX_KEY_LENGTH 128
#define MAX_STRING_VALUE 512
#define MAX_VALIDATORS 32
#define MAX_PRE_CALLBACKS 32
#define MAX_POST_CALLBACKS 32

/**
 * @brief Internal config entry snapshot
 */
typedef struct {
    char key[MAX_KEY_LENGTH];
    config_value_type_t type;
    config_value_t value;
    bool in_use;
} config_entry_snapshot_t;

/**
 * @brief Config snapshot structure
 */
typedef struct config_snapshot_internal {
    uint32_t magic;                              /**< Magic for validation */
    uint32_t version;                            /**< Version captured */
    uint64_t timestamp_ns;                       /**< Creation timestamp */
    size_t entry_count;                          /**< Number of entries */
    config_entry_snapshot_t entries[MAX_CONFIG_ENTRIES]; /**< Snapshot data */
    unified_mem_handle_t unified_handle;         /**< Unified memory (optional) */
} config_snapshot_internal_t;

/**
 * @brief Validator registration
 */
typedef struct {
    uint32_t id;
    config_reload_validator_t validator;
    void* user_data;
    bool in_use;
} validator_registration_t;

/**
 * @brief Pre-reload callback registration
 */
typedef struct {
    uint32_t id;
    config_pre_reload_callback_t callback;
    void* user_data;
    bool in_use;
} pre_reload_registration_t;

/**
 * @brief Post-reload callback registration
 */
typedef struct {
    uint32_t id;
    config_post_reload_callback_t callback;
    void* user_data;
    bool in_use;
} post_reload_registration_t;

/**
 * @brief Version history entry
 */
typedef struct {
    uint32_t version;
    config_snapshot_t snapshot;
    uint64_t timestamp_ns;
} version_history_entry_t;

//=============================================================================
// Global State
//=============================================================================

static nimcp_platform_rwlock_t g_atomic_lock = PTHREAD_RWLOCK_INITIALIZER;
static nimcp_platform_mutex_t g_callback_lock = NIMCP_MUTEX_INITIALIZER;

static uint32_t g_current_version = 1;
static uint32_t g_max_history_size = CONFIG_DEFAULT_HISTORY_SIZE;
static version_history_entry_t* g_version_history = NULL;
static size_t g_history_count = 0;

static validator_registration_t g_validators[MAX_VALIDATORS] = {0};
static pre_reload_registration_t g_pre_callbacks[MAX_PRE_CALLBACKS] = {0};
static post_reload_registration_t g_post_callbacks[MAX_POST_CALLBACKS] = {0};

static uint32_t g_next_validator_id = 1;
static uint32_t g_next_pre_callback_id = 1;
static uint32_t g_next_post_callback_id = 1;

static config_atomic_stats_t g_atomic_stats = {0};

static bool g_sighup_installed = false;
static struct sigaction g_old_sighup_handler;

static unified_mem_manager_t g_unified_mem = NULL;

#define SNAPSHOT_MAGIC 0x534E4150  // 'SNAP'

//=============================================================================
// Internal Forward Declarations
//=============================================================================

static bool validate_snapshot(const config_snapshot_internal_t* snap);
static void add_to_history(config_snapshot_t snapshot);
static config_snapshot_t find_version_in_history(uint32_t version);
static void run_pre_reload_callbacks(uint32_t version);
static void run_post_reload_callbacks(uint32_t before, uint32_t after, bool success);
static bool run_validators(void);

//=============================================================================
// Snapshot API Implementation
//=============================================================================

/* Callback context for snapshot capture */
typedef struct {
    config_snapshot_internal_t* snap;
} snapshot_capture_ctx_t;

static bool snapshot_capture_entry(const char* key, config_value_type_t type,
                                    const config_value_t* value, void* user_data) {
    snapshot_capture_ctx_t* ctx = (snapshot_capture_ctx_t*)user_data;
    config_snapshot_internal_t* snap = ctx->snap;

    if (snap->entry_count >= MAX_CONFIG_ENTRIES) return false;

    config_entry_snapshot_t* entry = &snap->entries[snap->entry_count];
    entry->in_use = true;
    entry->type = type;
    strncpy(entry->key, key, MAX_KEY_LENGTH - 1);
    entry->key[MAX_KEY_LENGTH - 1] = '\0';

    if (type == CONFIG_TYPE_STRING && value->string_val) {
        entry->value.string_val = nimcp_strdup(value->string_val);
    } else {
        entry->value = *value;
    }

    snap->entry_count++;
    return true;
}

config_snapshot_t config_create_snapshot(void) {
    LOG_DEBUG("config_create_snapshot: creating snapshot v%u", g_current_version);

    config_snapshot_internal_t* snap = (config_snapshot_internal_t*)
        nimcp_calloc(1, sizeof(config_snapshot_internal_t));
    if (!snap) {
        LOG_ERROR("config_create_snapshot: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snap is NULL");

        return NULL;
    }

    snap->magic = SNAPSHOT_MAGIC;
    snap->version = g_current_version;
    snap->timestamp_ns = nimcp_time_monotonic_ns();
    snap->entry_count = 0;
    snap->unified_handle = NULL;

    nimcp_platform_rwlock_rdlock(&g_atomic_lock);

    /* Capture all current config entries */
    snapshot_capture_ctx_t ctx = { .snap = snap };
    config_iterate_entries(snapshot_capture_entry, &ctx);

    nimcp_platform_rwlock_unlock(&g_atomic_lock);

    g_atomic_stats.snapshots_created++;

    LOG_INFO("config_create_snapshot: created snapshot v%u with %zu entries",
             snap->version, snap->entry_count);

    return (config_snapshot_t)snap;
}

void config_destroy_snapshot(config_snapshot_t snap) {
    if (!snap) return;

    config_snapshot_internal_t* s = (config_snapshot_internal_t*)snap;
    if (!validate_snapshot(s)) {
        LOG_WARN("config_destroy_snapshot: invalid snapshot");
        return;
    }

    LOG_DEBUG("config_destroy_snapshot: destroying snapshot v%u", s->version);

    // Free string values
    for (size_t i = 0; i < s->entry_count; i++) {
        if (s->entries[i].in_use &&
            s->entries[i].type == CONFIG_TYPE_STRING &&
            s->entries[i].value.string_val) {
            nimcp_free(s->entries[i].value.string_val);
        }
    }

    // Release unified memory if used
    if (s->unified_handle && g_unified_mem) {
        unified_mem_free(s->unified_handle);
    }

    s->magic = 0;
    nimcp_free(s);

    g_atomic_stats.snapshots_destroyed++;
}

bool config_restore_snapshot(config_snapshot_t snap) {
    if (!snap) {
        LOG_ERROR("config_restore_snapshot: NULL snapshot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_restore_snapshot: snap is NULL");
        return false;
    }

    config_snapshot_internal_t* s = (config_snapshot_internal_t*)snap;
    if (!validate_snapshot(s)) {
        LOG_ERROR("config_restore_snapshot: invalid snapshot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_restore_snapshot: validate_snapshot is NULL");
        return false;
    }

    LOG_INFO("config_restore_snapshot: restoring snapshot v%u", s->version);

    nimcp_platform_rwlock_wrlock(&g_atomic_lock);

    /* Restore each entry from snapshot */
    for (size_t i = 0; i < s->entry_count; i++) {
        if (!s->entries[i].in_use) continue;

        const char* key = s->entries[i].key;
        switch (s->entries[i].type) {
            case CONFIG_TYPE_INT:
                config_set_int(key, s->entries[i].value.int_val);
                break;
            case CONFIG_TYPE_FLOAT:
                config_set_float(key, s->entries[i].value.float_val);
                break;
            case CONFIG_TYPE_BOOL:
                config_set_bool(key, s->entries[i].value.bool_val);
                break;
            case CONFIG_TYPE_STRING:
                if (s->entries[i].value.string_val) {
                    config_set_string(key, s->entries[i].value.string_val);
                }
                break;
            default:
                break;
        }
    }

    g_current_version = s->version;
    g_atomic_stats.rollbacks++;

    nimcp_platform_rwlock_unlock(&g_atomic_lock);

    LOG_INFO("config_restore_snapshot: restored to v%u with %zu entries",
             g_current_version, s->entry_count);
    return true;
}

uint32_t config_snapshot_get_version(config_snapshot_t snap) {
    if (!snap) return 0;
    config_snapshot_internal_t* s = (config_snapshot_internal_t*)snap;
    if (!validate_snapshot(s)) return 0;
    return s->version;
}

uint64_t config_snapshot_get_timestamp(config_snapshot_t snap) {
    if (!snap) return 0;
    config_snapshot_internal_t* s = (config_snapshot_internal_t*)snap;
    if (!validate_snapshot(s)) return 0;
    return s->timestamp_ns;
}

config_snapshot_t config_clone_snapshot(config_snapshot_t snap) {
    if (!snap) {
        LOG_ERROR("config_clone_snapshot: NULL snapshot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snap is NULL");

        return NULL;
    }

    config_snapshot_internal_t* src = (config_snapshot_internal_t*)snap;
    if (!validate_snapshot(src)) {
        LOG_ERROR("config_clone_snapshot: invalid source snapshot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_clone_snapshot: validate_snapshot is NULL");
        return NULL;
    }

    config_snapshot_internal_t* clone = (config_snapshot_internal_t*)
        nimcp_calloc(1, sizeof(config_snapshot_internal_t));
    if (!clone) {
        LOG_ERROR("config_clone_snapshot: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "clone is NULL");

        return NULL;
    }

    // Copy metadata
    clone->magic = SNAPSHOT_MAGIC;
    clone->version = src->version;
    clone->timestamp_ns = src->timestamp_ns;
    clone->entry_count = src->entry_count;
    clone->unified_handle = NULL;

    // Deep copy entries
    for (size_t i = 0; i < src->entry_count; i++) {
        clone->entries[i] = src->entries[i];

        // Duplicate strings
        if (clone->entries[i].in_use &&
            clone->entries[i].type == CONFIG_TYPE_STRING &&
            src->entries[i].value.string_val) {
            clone->entries[i].value.string_val = nimcp_strdup(src->entries[i].value.string_val);
            if (!clone->entries[i].value.string_val) {
                LOG_ERROR("config_clone_snapshot: string duplication failed");
                config_destroy_snapshot((config_snapshot_t)clone);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_clone_snapshot: clone->entries is NULL");
                return NULL;
            }
        }
    }

    g_atomic_stats.snapshots_created++;
    LOG_DEBUG("config_clone_snapshot: cloned snapshot v%u", src->version);

    return (config_snapshot_t)clone;
}

//=============================================================================
// Atomic Reload API Implementation
//=============================================================================

bool config_atomic_reload(const char* path) {
    LOG_INFO("config_atomic_reload: starting atomic reload");

    nimcp_platform_rwlock_wrlock(&g_atomic_lock);

    uint32_t version_before = g_current_version;
    uint32_t version_after = version_before + 1;

    g_atomic_stats.atomic_reloads++;

    // Step 1: Create snapshot of current state
    config_snapshot_t snapshot = config_create_snapshot();
    if (!snapshot) {
        LOG_ERROR("config_atomic_reload: failed to create snapshot");
        g_atomic_stats.atomic_reload_failures++;
        nimcp_platform_rwlock_unlock(&g_atomic_lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_atomic_reload: snapshot is NULL");
        return false;
    }

    LOG_DEBUG("config_atomic_reload: created snapshot v%u", version_before);

    // Step 2: Run pre-reload callbacks
    nimcp_platform_rwlock_unlock(&g_atomic_lock);  // Unlock for callbacks
    run_pre_reload_callbacks(version_before);
    nimcp_platform_rwlock_wrlock(&g_atomic_lock);  // Re-lock

    // Step 3: Parse new config into temporary state
    // TODO: This requires integration with nimcp_dynamic_config.c
    // For now, we'll just call config_reload()

    bool parse_success = config_reload();
    if (!parse_success) {
        LOG_ERROR("config_atomic_reload: parse failed, rolling back");
        config_restore_snapshot(snapshot);
        config_destroy_snapshot(snapshot);
        g_atomic_stats.atomic_reload_failures++;
        nimcp_platform_rwlock_unlock(&g_atomic_lock);
        run_post_reload_callbacks(version_before, version_before, false);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_atomic_reload: parse_success is NULL");
        return false;
    }

    LOG_DEBUG("config_atomic_reload: parse succeeded");

    // Step 4: Run validators
    nimcp_platform_rwlock_unlock(&g_atomic_lock);  // Unlock for validators
    bool validation_passed = run_validators();
    nimcp_platform_rwlock_wrlock(&g_atomic_lock);  // Re-lock

    if (!validation_passed) {
        LOG_ERROR("config_atomic_reload: validation failed, rolling back");
        config_restore_snapshot(snapshot);
        config_destroy_snapshot(snapshot);
        g_atomic_stats.atomic_reload_failures++;
        g_atomic_stats.validation_failures++;
        nimcp_platform_rwlock_unlock(&g_atomic_lock);
        run_post_reload_callbacks(version_before, version_before, false);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_atomic_reload: validation_passed is NULL");
        return false;
    }

    LOG_DEBUG("config_atomic_reload: validation passed");

    // Step 5: Atomic swap (already done by config_reload)
    g_current_version = version_after;
    g_atomic_stats.last_reload_time_ns = nimcp_time_monotonic_ns();

    // Step 6: Add snapshot to history
    add_to_history(snapshot);

    nimcp_platform_rwlock_unlock(&g_atomic_lock);

    // Step 7: Run post-reload callbacks
    run_post_reload_callbacks(version_before, version_after, true);

    LOG_INFO("config_atomic_reload: successfully reloaded to v%u", version_after);
    return true;
}

bool config_rollback(void) {
    LOG_INFO("config_rollback: attempting rollback to previous version");

    nimcp_platform_rwlock_wrlock(&g_atomic_lock);

    if (g_history_count < 2) {
        LOG_ERROR("config_rollback: no previous version available");
        nimcp_platform_rwlock_unlock(&g_atomic_lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_rollback: validation failed");
        return false;
    }

    // Get previous version (second-to-last in history)
    version_history_entry_t* prev = &g_version_history[g_history_count - 2];
    uint32_t target_version = prev->version;

    nimcp_platform_rwlock_unlock(&g_atomic_lock);

    bool result = config_rollback_to_version(target_version);
    if (result) {
        LOG_INFO("config_rollback: rolled back to v%u", target_version);
    } else {
        LOG_ERROR("config_rollback: rollback failed");
    }

    return result;
}

bool config_rollback_to_version(uint32_t version) {
    LOG_INFO("config_rollback_to_version: rolling back to v%u", version);

    nimcp_platform_rwlock_wrlock(&g_atomic_lock);

    config_snapshot_t snapshot = find_version_in_history(version);
    if (!snapshot) {
        LOG_ERROR("config_rollback_to_version: version %u not found in history", version);
        nimcp_platform_rwlock_unlock(&g_atomic_lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_rollback_to_version: snapshot is NULL");
        return false;
    }

    uint32_t version_before = g_current_version;

    bool result = config_restore_snapshot(snapshot);
    if (result) {
        g_atomic_stats.last_rollback_time_ns = nimcp_time_monotonic_ns();
        LOG_INFO("config_rollback_to_version: successfully rolled back to v%u", version);
    } else {
        g_atomic_stats.rollback_failures++;
        LOG_ERROR("config_rollback_to_version: restore failed");
    }

    nimcp_platform_rwlock_unlock(&g_atomic_lock);

    if (result) {
        run_post_reload_callbacks(version_before, version, false);
    }

    return result;
}

uint32_t config_get_version(void) {
    nimcp_platform_rwlock_rdlock(&g_atomic_lock);
    uint32_t version = g_current_version;
    nimcp_platform_rwlock_unlock(&g_atomic_lock);
    return version;
}

size_t config_get_version_history(uint32_t* versions, size_t max_versions) {
    if (!versions || max_versions == 0) return 0;

    nimcp_platform_rwlock_rdlock(&g_atomic_lock);

    size_t count = g_history_count < max_versions ? g_history_count : max_versions;
    for (size_t i = 0; i < count; i++) {
        versions[i] = g_version_history[i].version;
    }

    nimcp_platform_rwlock_unlock(&g_atomic_lock);
    return count;
}

//=============================================================================
// History Configuration API Implementation
//=============================================================================

void config_set_history_size(uint32_t max_size) {
    if (max_size == 0 || max_size > CONFIG_MAX_HISTORY_SIZE) {
        LOG_ERROR("config_set_history_size: invalid size %u (must be 1-%u)",
                  max_size, CONFIG_MAX_HISTORY_SIZE);
        return;
    }

    nimcp_platform_rwlock_wrlock(&g_atomic_lock);

    g_max_history_size = max_size;

    // Reallocate history array if needed
    if (!g_version_history) {
        g_version_history = (version_history_entry_t*)
            nimcp_calloc(max_size, sizeof(version_history_entry_t));
        if (!g_version_history) {
            LOG_ERROR("config_set_history_size: allocation failed");
            nimcp_platform_rwlock_unlock(&g_atomic_lock);
            return;
        }
    } else {
        // Trim history if shrinking
        while (g_history_count > max_size) {
            config_destroy_snapshot(g_version_history[0].snapshot);
            memmove(&g_version_history[0], &g_version_history[1],
                    (g_history_count - 1) * sizeof(version_history_entry_t));
            g_history_count--;
        }
    }

    nimcp_platform_rwlock_unlock(&g_atomic_lock);

    LOG_INFO("config_set_history_size: set to %u", max_size);
}

uint32_t config_get_history_size(void) {
    return g_max_history_size;
}

void config_clear_history(void) {
    LOG_INFO("config_clear_history: clearing version history");

    nimcp_platform_rwlock_wrlock(&g_atomic_lock);

    for (size_t i = 0; i < g_history_count; i++) {
        config_destroy_snapshot(g_version_history[i].snapshot);
    }

    g_history_count = 0;

    nimcp_platform_rwlock_unlock(&g_atomic_lock);

    LOG_INFO("config_clear_history: history cleared");
}

//=============================================================================
// Validation and Callback API Implementation
//=============================================================================

uint32_t config_register_reload_validator(config_reload_validator_t validator, void* user_data) {
    if (!validator) {
        LOG_ERROR("config_register_reload_validator: NULL validator");
        return 0;
    }

    nimcp_platform_mutex_lock(&g_callback_lock);

    uint32_t id = 0;
    for (size_t i = 0; i < MAX_VALIDATORS; i++) {
        if (!g_validators[i].in_use) {
            g_validators[i].id = g_next_validator_id++;
            g_validators[i].validator = validator;
            g_validators[i].user_data = user_data;
            g_validators[i].in_use = true;
            id = g_validators[i].id;
            break;
        }
    }

    nimcp_platform_mutex_unlock(&g_callback_lock);

    if (id == 0) {
        LOG_ERROR("config_register_reload_validator: no slots available");
    } else {
        LOG_DEBUG("config_register_reload_validator: registered validator %u", id);
    }

    return id;
}

void config_unregister_reload_validator(uint32_t id) {
    nimcp_platform_mutex_lock(&g_callback_lock);

    for (size_t i = 0; i < MAX_VALIDATORS; i++) {
        if (g_validators[i].in_use && g_validators[i].id == id) {
            g_validators[i].in_use = false;
            LOG_DEBUG("config_unregister_reload_validator: unregistered %u", id);
            break;
        }
    }

    nimcp_platform_mutex_unlock(&g_callback_lock);
}

uint32_t config_register_pre_reload_callback(config_pre_reload_callback_t callback,
                                               void* user_data) {
    if (!callback) return 0;

    nimcp_platform_mutex_lock(&g_callback_lock);

    uint32_t id = 0;
    for (size_t i = 0; i < MAX_PRE_CALLBACKS; i++) {
        if (!g_pre_callbacks[i].in_use) {
            g_pre_callbacks[i].id = g_next_pre_callback_id++;
            g_pre_callbacks[i].callback = callback;
            g_pre_callbacks[i].user_data = user_data;
            g_pre_callbacks[i].in_use = true;
            id = g_pre_callbacks[i].id;
            break;
        }
    }

    nimcp_platform_mutex_unlock(&g_callback_lock);
    return id;
}

void config_unregister_pre_reload_callback(uint32_t id) {
    nimcp_platform_mutex_lock(&g_callback_lock);

    for (size_t i = 0; i < MAX_PRE_CALLBACKS; i++) {
        if (g_pre_callbacks[i].in_use && g_pre_callbacks[i].id == id) {
            g_pre_callbacks[i].in_use = false;
            break;
        }
    }

    nimcp_platform_mutex_unlock(&g_callback_lock);
}

uint32_t config_register_post_reload_callback(config_post_reload_callback_t callback,
                                                void* user_data) {
    if (!callback) return 0;

    nimcp_platform_mutex_lock(&g_callback_lock);

    uint32_t id = 0;
    for (size_t i = 0; i < MAX_POST_CALLBACKS; i++) {
        if (!g_post_callbacks[i].in_use) {
            g_post_callbacks[i].id = g_next_post_callback_id++;
            g_post_callbacks[i].callback = callback;
            g_post_callbacks[i].user_data = user_data;
            g_post_callbacks[i].in_use = true;
            id = g_post_callbacks[i].id;
            break;
        }
    }

    nimcp_platform_mutex_unlock(&g_callback_lock);
    return id;
}

void config_unregister_post_reload_callback(uint32_t id) {
    nimcp_platform_mutex_lock(&g_callback_lock);

    for (size_t i = 0; i < MAX_POST_CALLBACKS; i++) {
        if (g_post_callbacks[i].in_use && g_post_callbacks[i].id == id) {
            g_post_callbacks[i].in_use = false;
            break;
        }
    }

    nimcp_platform_mutex_unlock(&g_callback_lock);
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

bool config_get_atomic_stats(config_atomic_stats_t* stats) {
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_get_atomic_stats: stats is NULL");
        return false;
    }

    nimcp_platform_rwlock_rdlock(&g_atomic_lock);

    *stats = g_atomic_stats;
    stats->current_version = g_current_version;
    stats->history_depth = g_history_count;
    stats->max_history_size = g_max_history_size;

    if (g_history_count > 0) {
        stats->oldest_version = g_version_history[0].version;
    } else {
        stats->oldest_version = g_current_version;
    }

    nimcp_platform_rwlock_unlock(&g_atomic_lock);

    return true;
}

void config_reset_atomic_stats(void) {
    nimcp_platform_rwlock_wrlock(&g_atomic_lock);
    memset(&g_atomic_stats, 0, sizeof(g_atomic_stats));
    nimcp_platform_rwlock_unlock(&g_atomic_lock);
    LOG_INFO("config_reset_atomic_stats: statistics reset");
}

//=============================================================================
// Signal Integration API Implementation
//=============================================================================

static void sighup_handler(int sig) {
    (void)sig;  // Unused

    LOG_INFO("SIGHUP received, triggering atomic reload");

    // Trigger reload in signal-safe manner
    // NOTE: We can't call config_atomic_reload() directly from signal handler
    // A real implementation would set a flag and handle in main thread

    // For now, log only
    LOG_WARN("SIGHUP handler: deferred reload not yet implemented");
}

bool config_install_sighup_handler(void) {
    if (g_sighup_installed) {
        LOG_WARN("config_install_sighup_handler: already installed");
        return true;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sighup_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGHUP, &sa, &g_old_sighup_handler) != 0) {
        LOG_ERROR("config_install_sighup_handler: sigaction failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_install_sighup_handler: validation failed");
        return false;
    }

    g_sighup_installed = true;
    LOG_INFO("config_install_sighup_handler: SIGHUP handler installed");
    return true;
}

bool config_uninstall_sighup_handler(void) {
    if (!g_sighup_installed) {
        return true;
    }

    if (sigaction(SIGHUP, &g_old_sighup_handler, NULL) != 0) {
        LOG_ERROR("config_uninstall_sighup_handler: sigaction failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_uninstall_sighup_handler: validation failed");
        return false;
    }

    g_sighup_installed = false;
    LOG_INFO("config_uninstall_sighup_handler: SIGHUP handler uninstalled");
    return true;
}

bool config_is_sighup_handler_installed(void) {
    return g_sighup_installed;
}

//=============================================================================
// Testing and Debugging API Implementation
//=============================================================================

void config_dump_version_history(void) {
    nimcp_platform_rwlock_rdlock(&g_atomic_lock);

    LOG_INFO("=== Config Version History ===");
    LOG_INFO("Current version: %u", g_current_version);
    LOG_INFO("History entries: %zu / %u", g_history_count, g_max_history_size);

    for (size_t i = 0; i < g_history_count; i++) {
        version_history_entry_t* entry = &g_version_history[i];
        LOG_INFO("  [%zu] v%u @ %lu ns", i, entry->version, entry->timestamp_ns);
    }

    nimcp_platform_rwlock_unlock(&g_atomic_lock);
}

uint32_t config_force_version_increment(void) {
    nimcp_platform_rwlock_wrlock(&g_atomic_lock);
    g_current_version++;
    uint32_t version = g_current_version;
    nimcp_platform_rwlock_unlock(&g_atomic_lock);

    LOG_DEBUG("config_force_version_increment: incremented to v%u", version);
    return version;
}

size_t config_compare_with_snapshot(config_snapshot_t snap) {
    if (!snap) return 0;

    config_snapshot_internal_t* s = (config_snapshot_internal_t*)snap;
    if (!validate_snapshot(s)) return 0;

    size_t diffs = 0;

    for (size_t i = 0; i < s->entry_count; i++) {
        if (!s->entries[i].in_use) continue;

        const char* key = s->entries[i].key;
        switch (s->entries[i].type) {
            case CONFIG_TYPE_INT:
                if (config_get_int(key, 0) != s->entries[i].value.int_val) diffs++;
                break;
            case CONFIG_TYPE_FLOAT:
                if (config_get_float(key, 0.0) != s->entries[i].value.float_val) diffs++;
                break;
            case CONFIG_TYPE_BOOL:
                if (config_get_bool(key, false) != s->entries[i].value.bool_val) diffs++;
                break;
            case CONFIG_TYPE_STRING: {
                const char* current = config_get_string(key, "");
                const char* saved = s->entries[i].value.string_val ? s->entries[i].value.string_val : "";
                if (strcmp(current, saved) != 0) diffs++;
                break;
            }
            default:
                break;
        }
    }

    LOG_DEBUG("config_compare_with_snapshot: found %zu differences", diffs);
    return diffs;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

static bool validate_snapshot(const config_snapshot_internal_t* snap) {
    return snap != NULL && snap->magic == SNAPSHOT_MAGIC;
}

static void add_to_history(config_snapshot_t snapshot) {
    if (!snapshot || !g_version_history) return;

    // If history is full, destroy oldest
    if (g_history_count >= g_max_history_size) {
        config_destroy_snapshot(g_version_history[0].snapshot);
        memmove(&g_version_history[0], &g_version_history[1],
                (g_max_history_size - 1) * sizeof(version_history_entry_t));
        g_history_count--;
    }

    // Add new snapshot
    g_version_history[g_history_count].version = config_snapshot_get_version(snapshot);
    g_version_history[g_history_count].snapshot = snapshot;
    g_version_history[g_history_count].timestamp_ns = nimcp_time_monotonic_ns();
    g_history_count++;

    LOG_DEBUG("add_to_history: added v%u to history (count=%zu)",
              g_version_history[g_history_count - 1].version, g_history_count);
}

static config_snapshot_t find_version_in_history(uint32_t version) {
    for (size_t i = 0; i < g_history_count; i++) {
        if (g_version_history[i].version == version) {
            return g_version_history[i].snapshot;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_version_in_history: validation failed");
    return NULL;
}

static void run_pre_reload_callbacks(uint32_t version) {
    nimcp_platform_mutex_lock(&g_callback_lock);

    for (size_t i = 0; i < MAX_PRE_CALLBACKS; i++) {
        if (g_pre_callbacks[i].in_use) {
            g_pre_callbacks[i].callback(version, g_pre_callbacks[i].user_data);
        }
    }

    nimcp_platform_mutex_unlock(&g_callback_lock);
}

static void run_post_reload_callbacks(uint32_t before, uint32_t after, bool success) {
    nimcp_platform_mutex_lock(&g_callback_lock);

    for (size_t i = 0; i < MAX_POST_CALLBACKS; i++) {
        if (g_post_callbacks[i].in_use) {
            g_post_callbacks[i].callback(before, after, success, g_post_callbacks[i].user_data);
        }
    }

    nimcp_platform_mutex_unlock(&g_callback_lock);
}

static bool run_validators(void) {
    nimcp_platform_mutex_lock(&g_callback_lock);

    bool all_passed = true;

    for (size_t i = 0; i < MAX_VALIDATORS; i++) {
        if (g_validators[i].in_use) {
            if (!g_validators[i].validator(g_validators[i].user_data)) {
                LOG_WARN("run_validators: validator %u rejected config", g_validators[i].id);
                all_passed = false;
                break;
            }
        }
    }

    nimcp_platform_mutex_unlock(&g_callback_lock);

    return all_passed;
}
