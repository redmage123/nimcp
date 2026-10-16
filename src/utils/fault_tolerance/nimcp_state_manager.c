/**
 * @file nimcp_state_manager.c
 * @brief Module State Manager Implementation
 *
 * PHASE 8: System-Wide Health Integration
 * Centralized registry for module state operations enabling consistent
 * checkpointing and recovery across all modules.
 *
 * @author NIMCP Team
 * @date 2026-01-20
 * @version 1.0.0
 */

#include "utils/fault_tolerance/nimcp_state_manager.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_time.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdlib.h>

#define LOG_MODULE "STATE_MANAGER"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(state_manager)

//=============================================================================
// Internal Constants
//=============================================================================

/** Checkpoint header for state manager data */
#define STATE_CHECKPOINT_MAGIC 0x4E53544D  /* "NSTM" */
#define STATE_CHECKPOINT_VERSION 1

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Checkpoint module header
 */
typedef struct {
    char name[NIMCP_STATE_MANAGER_MAX_NAME_LEN];
    uint32_t data_size;
    uint32_t checksum;
} state_module_header_t;

/**
 * @brief Full checkpoint header
 */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t module_count;
    uint32_t total_size;
    uint64_t timestamp;
    uint32_t checksum;
} state_checkpoint_header_t;

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Simple checksum calculation
 */
static uint32_t calculate_checksum(const uint8_t* data, size_t size) {
    uint32_t sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum = (sum << 1) | (sum >> 31);  /* Rotate left */
        sum ^= data[i];
    }
    return sum;
}

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    return nimcp_platform_time_monotonic_us();
}

/**
 * @brief Compare modules by priority for sorting
 */
static int compare_by_priority(const void* a, const void* b) {
    const nimcp_state_module_entry_t* ma = (const nimcp_state_module_entry_t*)a;
    const nimcp_state_module_entry_t* mb = (const nimcp_state_module_entry_t*)b;
    if (ma->priority < mb->priority) return -1;
    if (ma->priority > mb->priority) return 1;
    return 0;
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

nimcp_state_manager_t* nimcp_state_manager_create(void) {
    nimcp_state_manager_t* manager = nimcp_calloc(1, sizeof(nimcp_state_manager_t));
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_state_manager_create: failed to allocate state manager");
        LOG_ERROR(LOG_MODULE, "Failed to allocate state manager");
        return NULL;
    }

    manager->magic = NIMCP_STATE_MANAGER_MAGIC;
    manager->module_count = 0;
    manager->initialized = false;

    /* Create mutex */
    manager->mutex = nimcp_mutex_create(NULL);
    if (!manager->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "nimcp_state_manager_create: failed to create mutex");
        LOG_ERROR(LOG_MODULE, "Failed to create state manager mutex");
        nimcp_free(manager);
        return NULL;
    }

    manager->initialized = true;
    LOG_INFO(LOG_MODULE, "State manager created");

    return manager;
}

void nimcp_state_manager_destroy(nimcp_state_manager_t* manager) {
    if (!manager) return;

    if (manager->mutex) {
        nimcp_mutex_free(manager->mutex);
    }

    manager->magic = 0;
    manager->initialized = false;

    nimcp_free(manager);
    LOG_INFO(LOG_MODULE, "State manager destroyed");
}

//=============================================================================
// Module Registration API Implementation
//=============================================================================

int nimcp_state_manager_register(
    nimcp_state_manager_t* manager,
    const char* name,
    const nimcp_module_state_ops_t* ops,
    void* context
) {
    return nimcp_state_manager_register_with_priority(manager, name, ops, context, 100);
}

int nimcp_state_manager_register_with_priority(
    nimcp_state_manager_t* manager,
    const char* name,
    const nimcp_module_state_ops_t* ops,
    void* context,
    uint32_t priority
) {
    if (!manager || !name || !ops) {
        return -NIMCP_ERROR_NULL_POINTER;
    }
    if (!manager->initialized) {
        return -NIMCP_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(manager->mutex);

    /* Check for duplicate */
    for (uint32_t i = 0; i < manager->module_count; i++) {
        if (strncmp(manager->modules[i].name, name, NIMCP_STATE_MANAGER_MAX_NAME_LEN) == 0) {
            nimcp_mutex_unlock(manager->mutex);
            LOG_WARN(LOG_MODULE, "Module '%s' already registered", name);
            return -NIMCP_ERROR_ALREADY_EXISTS;
        }
    }

    /* Check capacity */
    if (manager->module_count >= NIMCP_STATE_MANAGER_MAX_MODULES) {
        nimcp_mutex_unlock(manager->mutex);
        LOG_ERROR(LOG_MODULE, "State manager full, cannot register '%s'", name);
        return -NIMCP_ERROR_OUT_OF_RANGE;
    }

    /* Add module */
    nimcp_state_module_entry_t* entry = &manager->modules[manager->module_count];
    strncpy(entry->name, name, NIMCP_STATE_MANAGER_MAX_NAME_LEN - 1);
    entry->name[NIMCP_STATE_MANAGER_MAX_NAME_LEN - 1] = '\0';
    entry->ops = *ops;
    entry->context = context;
    entry->enabled = true;
    entry->priority = priority;
    entry->last_checkpoint_time = 0;
    entry->last_restore_time = 0;
    entry->checkpoint_count = 0;
    entry->restore_count = 0;
    entry->validation_failures = 0;

    manager->module_count++;

    nimcp_mutex_unlock(manager->mutex);

    LOG_INFO(LOG_MODULE, "Registered module '%s' with priority %u", name, priority);
    return NIMCP_SUCCESS;
}

int nimcp_state_manager_unregister(
    nimcp_state_manager_t* manager,
    const char* name
) {
    if (!manager || !name) {
        return -NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(manager->mutex);

    /* Find module */
    int found_idx = -1;
    for (uint32_t i = 0; i < manager->module_count; i++) {
        if (strncmp(manager->modules[i].name, name, NIMCP_STATE_MANAGER_MAX_NAME_LEN) == 0) {
            found_idx = (int)i;
            break;
        }
    }

    if (found_idx < 0) {
        nimcp_mutex_unlock(manager->mutex);
        return -NIMCP_ERROR_NOT_FOUND;
    }

    /* Shift remaining modules */
    for (uint32_t i = (uint32_t)found_idx; i < manager->module_count - 1; i++) {
        manager->modules[i] = manager->modules[i + 1];
    }
    manager->module_count--;

    nimcp_mutex_unlock(manager->mutex);

    LOG_INFO(LOG_MODULE, "Unregistered module '%s'", name);
    return NIMCP_SUCCESS;
}

nimcp_state_module_entry_t* nimcp_state_manager_find(
    nimcp_state_manager_t* manager,
    const char* name
) {
    if (!manager || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_state_manager_find: required parameter is NULL (manager, name)");
        return NULL;
    }

    for (uint32_t i = 0; i < manager->module_count; i++) {
        if (strncmp(manager->modules[i].name, name, NIMCP_STATE_MANAGER_MAX_NAME_LEN) == 0) {
            return &manager->modules[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_state_manager_find: validation failed");
    return NULL;
}

int nimcp_state_manager_set_enabled(
    nimcp_state_manager_t* manager,
    const char* name,
    bool enabled
) {
    if (!manager || !name) return -NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, name);
    if (!entry) {
        nimcp_mutex_unlock(manager->mutex);
        return -NIMCP_ERROR_NOT_FOUND;
    }

    entry->enabled = enabled;
    nimcp_mutex_unlock(manager->mutex);

    LOG_DEBUG(LOG_MODULE, "Module '%s' %s", name, enabled ? "enabled" : "disabled");
    return NIMCP_SUCCESS;
}

//=============================================================================
// Checkpoint API Implementation
//=============================================================================

int nimcp_state_manager_checkpoint_all(
    nimcp_state_manager_t* manager,
    uint8_t* buffer,
    size_t* size
) {
    if (!manager || !size) return -NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    /* First pass: calculate total size */
    size_t total_size = sizeof(state_checkpoint_header_t);
    uint32_t enabled_count = 0;

    for (uint32_t i = 0; i < manager->module_count; i++) {
        nimcp_state_module_entry_t* entry = &manager->modules[i];
        if (!entry->enabled) continue;
        if (!entry->ops.get_size) continue;

        size_t module_size = entry->ops.get_size(entry->context);
        if (module_size > 0) {
            total_size += sizeof(state_module_header_t) + module_size;
            enabled_count++;
        }
    }

    /* If buffer is NULL, just return required size */
    if (!buffer) {
        *size = total_size;
        nimcp_mutex_unlock(manager->mutex);
        return NIMCP_SUCCESS;
    }

    /* Check buffer size */
    if (*size < total_size) {
        *size = total_size;
        nimcp_mutex_unlock(manager->mutex);
        return -2;  /* Buffer too small */
    }

    /* Write checkpoint header */
    state_checkpoint_header_t* header = (state_checkpoint_header_t*)buffer;
    header->magic = STATE_CHECKPOINT_MAGIC;
    header->version = STATE_CHECKPOINT_VERSION;
    header->module_count = enabled_count;
    header->total_size = (uint32_t)total_size;
    header->timestamp = get_timestamp_us();
    header->checksum = 0;  /* Calculated at end */

    /* Write module data */
    uint8_t* ptr = buffer + sizeof(state_checkpoint_header_t);

    for (uint32_t i = 0; i < manager->module_count; i++) {
        nimcp_state_module_entry_t* entry = &manager->modules[i];
        if (!entry->enabled) continue;
        if (!entry->ops.serialize || !entry->ops.get_size) continue;

        size_t module_size = entry->ops.get_size(entry->context);
        if (module_size == 0) continue;

        /* Write module header */
        state_module_header_t* mheader = (state_module_header_t*)ptr;
        strncpy(mheader->name, entry->name, NIMCP_STATE_MANAGER_MAX_NAME_LEN);
        mheader->data_size = (uint32_t)module_size;
        ptr += sizeof(state_module_header_t);

        /* Serialize module state */
        size_t written = module_size;
        int result = entry->ops.serialize(entry->context, ptr, &written);
        if (result != 0) {
            LOG_WARN(LOG_MODULE, "Failed to serialize module '%s': %d", entry->name, result);
            nimcp_mutex_unlock(manager->mutex);
            return result;
        }

        /* Calculate module checksum */
        mheader->checksum = calculate_checksum(ptr, written);

        ptr += written;
        entry->checkpoint_count++;
        entry->last_checkpoint_time = get_timestamp_us();
    }

    /* Calculate overall checksum (excluding checksum field) */
    header->checksum = calculate_checksum(
        buffer + sizeof(uint32_t) * 3,  /* Skip magic, version, checksum */
        total_size - sizeof(uint32_t) * 3
    );

    *size = total_size;
    manager->total_checkpoints++;
    manager->last_full_checkpoint_time = get_timestamp_us();
    manager->last_checkpoint_size = total_size;

    nimcp_mutex_unlock(manager->mutex);

    LOG_INFO(LOG_MODULE, "Checkpoint complete: %u modules, %zu bytes", enabled_count, total_size);
    return NIMCP_SUCCESS;
}

int nimcp_state_manager_checkpoint_module(
    nimcp_state_manager_t* manager,
    const char* name,
    uint8_t* buffer,
    size_t* size
) {
    if (!manager || !name || !size) return -NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, name);
    if (!entry) {
        nimcp_mutex_unlock(manager->mutex);
        return -NIMCP_ERROR_NOT_FOUND;
    }

    if (!entry->ops.serialize) {
        nimcp_mutex_unlock(manager->mutex);
        return -NIMCP_ERROR_NOT_IMPLEMENTED;
    }

    int result = entry->ops.serialize(entry->context, buffer, size);
    if (result == 0) {
        entry->checkpoint_count++;
        entry->last_checkpoint_time = get_timestamp_us();
    }

    nimcp_mutex_unlock(manager->mutex);
    return result;
}

//=============================================================================
// Restore API Implementation
//=============================================================================

int nimcp_state_manager_restore_all(
    nimcp_state_manager_t* manager,
    const uint8_t* buffer,
    size_t size
) {
    if (!manager || !buffer) return -NIMCP_ERROR_NULL_POINTER;
    if (size < sizeof(state_checkpoint_header_t)) return -NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(manager->mutex);

    /* Validate header */
    const state_checkpoint_header_t* header = (const state_checkpoint_header_t*)buffer;
    if (header->magic != STATE_CHECKPOINT_MAGIC) {
        nimcp_mutex_unlock(manager->mutex);
        LOG_ERROR(LOG_MODULE, "Invalid checkpoint magic");
        return -NIMCP_ERROR_INVALID_STATE;
    }

    if (header->version > STATE_CHECKPOINT_VERSION) {
        nimcp_mutex_unlock(manager->mutex);
        LOG_ERROR(LOG_MODULE, "Unsupported checkpoint version: %u", header->version);
        return -NIMCP_ERROR_INVALID_STATE;
    }

    /* Restore each module */
    const uint8_t* ptr = buffer + sizeof(state_checkpoint_header_t);
    uint32_t restored = 0;

    for (uint32_t i = 0; i < header->module_count; i++) {
        const state_module_header_t* mheader = (const state_module_header_t*)ptr;
        ptr += sizeof(state_module_header_t);

        /* Verify checksum */
        uint32_t checksum = calculate_checksum(ptr, mheader->data_size);
        if (checksum != mheader->checksum) {
            LOG_WARN(LOG_MODULE, "Checksum mismatch for module '%s'", mheader->name);
            ptr += mheader->data_size;
            continue;
        }

        /* Find registered module */
        nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, mheader->name);
        if (!entry) {
            LOG_WARN(LOG_MODULE, "Module '%s' not registered, skipping", mheader->name);
            ptr += mheader->data_size;
            continue;
        }

        if (!entry->ops.deserialize) {
            LOG_WARN(LOG_MODULE, "Module '%s' has no deserialize op", mheader->name);
            ptr += mheader->data_size;
            continue;
        }

        /* Deserialize */
        int result = entry->ops.deserialize(entry->context, ptr, mheader->data_size);
        if (result != 0) {
            LOG_WARN(LOG_MODULE, "Failed to restore module '%s': %d", mheader->name, result);
        } else {
            entry->restore_count++;
            entry->last_restore_time = get_timestamp_us();
            restored++;
        }

        ptr += mheader->data_size;
    }

    manager->total_restores++;
    nimcp_mutex_unlock(manager->mutex);

    LOG_INFO(LOG_MODULE, "Restore complete: %u/%u modules", restored, header->module_count);
    return NIMCP_SUCCESS;
}

int nimcp_state_manager_restore_module(
    nimcp_state_manager_t* manager,
    const char* name,
    const uint8_t* buffer,
    size_t size
) {
    if (!manager || !name || !buffer) return -NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, name);
    if (!entry) {
        nimcp_mutex_unlock(manager->mutex);
        return -NIMCP_ERROR_NOT_FOUND;
    }

    if (!entry->ops.deserialize) {
        nimcp_mutex_unlock(manager->mutex);
        return -NIMCP_ERROR_NOT_IMPLEMENTED;
    }

    int result = entry->ops.deserialize(entry->context, buffer, size);
    if (result == 0) {
        entry->restore_count++;
        entry->last_restore_time = get_timestamp_us();
    }

    nimcp_mutex_unlock(manager->mutex);
    return result;
}

//=============================================================================
// Validation API Implementation
//=============================================================================

int nimcp_state_manager_validate_all(nimcp_state_manager_t* manager) {
    if (!manager) return -NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    int valid_count = 0;
    for (uint32_t i = 0; i < manager->module_count; i++) {
        nimcp_state_module_entry_t* entry = &manager->modules[i];
        if (!entry->enabled) continue;
        if (!entry->ops.validate) {
            valid_count++;  /* No validation = assume valid */
            continue;
        }

        int result = entry->ops.validate(entry->context);
        if (result == 0) {
            valid_count++;
        } else {
            entry->validation_failures++;
            LOG_WARN(LOG_MODULE, "Validation failed for module '%s'", entry->name);
        }
    }

    manager->total_validations++;
    nimcp_mutex_unlock(manager->mutex);

    return valid_count;
}

int nimcp_state_manager_validate_module(
    nimcp_state_manager_t* manager,
    const char* name
) {
    if (!manager || !name) return -NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, name);
    if (!entry) {
        nimcp_mutex_unlock(manager->mutex);
        return -NIMCP_ERROR_NOT_FOUND;
    }

    if (!entry->ops.validate) {
        nimcp_mutex_unlock(manager->mutex);
        return 0;  /* No validation = valid */
    }

    int result = entry->ops.validate(entry->context);
    manager->total_validations++;
    if (result != 0) {
        entry->validation_failures++;
    }

    nimcp_mutex_unlock(manager->mutex);
    return result;
}

//=============================================================================
// Reset/Recovery API Implementation
//=============================================================================

int nimcp_state_manager_reset_all(nimcp_state_manager_t* manager) {
    if (!manager) return -NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    int reset_count = 0;
    for (uint32_t i = 0; i < manager->module_count; i++) {
        nimcp_state_module_entry_t* entry = &manager->modules[i];
        if (!entry->enabled) continue;
        if (!entry->ops.reset) continue;

        int result = entry->ops.reset(entry->context);
        if (result == 0) {
            reset_count++;
            LOG_DEBUG(LOG_MODULE, "Reset module '%s'", entry->name);
        } else {
            LOG_WARN(LOG_MODULE, "Failed to reset module '%s': %d", entry->name, result);
        }
    }

    manager->total_resets++;
    nimcp_mutex_unlock(manager->mutex);

    LOG_INFO(LOG_MODULE, "Reset complete: %d modules", reset_count);
    return reset_count;
}

int nimcp_state_manager_reset_module(
    nimcp_state_manager_t* manager,
    const char* name
) {
    if (!manager || !name) return -NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, name);
    if (!entry) {
        nimcp_mutex_unlock(manager->mutex);
        return -NIMCP_ERROR_NOT_FOUND;
    }

    if (!entry->ops.reset) {
        nimcp_mutex_unlock(manager->mutex);
        return -NIMCP_ERROR_NOT_IMPLEMENTED;
    }

    int result = entry->ops.reset(entry->context);
    nimcp_mutex_unlock(manager->mutex);

    return result;
}

int nimcp_state_manager_reset_invalid(nimcp_state_manager_t* manager) {
    if (!manager) return -NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    int reset_count = 0;
    for (uint32_t i = 0; i < manager->module_count; i++) {
        nimcp_state_module_entry_t* entry = &manager->modules[i];
        if (!entry->enabled) continue;
        if (!entry->ops.validate || !entry->ops.reset) continue;

        /* Check if invalid */
        if (entry->ops.validate(entry->context) != 0) {
            /* Reset it */
            if (entry->ops.reset(entry->context) == 0) {
                reset_count++;
                LOG_INFO(LOG_MODULE, "Reset invalid module '%s'", entry->name);
            }
        }
    }

    nimcp_mutex_unlock(manager->mutex);
    return reset_count;
}

//=============================================================================
// Query API Implementation
//=============================================================================

size_t nimcp_state_manager_get_total_size(nimcp_state_manager_t* manager) {
    if (!manager) return 0;

    nimcp_mutex_lock(manager->mutex);

    size_t total = sizeof(state_checkpoint_header_t);
    for (uint32_t i = 0; i < manager->module_count; i++) {
        nimcp_state_module_entry_t* entry = &manager->modules[i];
        if (!entry->enabled) continue;
        if (!entry->ops.get_size) continue;

        size_t module_size = entry->ops.get_size(entry->context);
        if (module_size > 0) {
            total += sizeof(state_module_header_t) + module_size;
        }
    }

    nimcp_mutex_unlock(manager->mutex);
    return total;
}

uint32_t nimcp_state_manager_get_module_count(nimcp_state_manager_t* manager) {
    if (!manager) return 0;
    return manager->module_count;
}

uint32_t nimcp_state_manager_get_module_names(
    nimcp_state_manager_t* manager,
    const char** names,
    uint32_t max_names
) {
    if (!manager || !names) return 0;

    nimcp_mutex_lock(manager->mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < manager->module_count && count < max_names; i++) {
        names[count++] = manager->modules[i].name;
    }

    nimcp_mutex_unlock(manager->mutex);
    return count;
}

//=============================================================================
// Statistics Implementation
//=============================================================================

int nimcp_state_manager_get_stats(
    nimcp_state_manager_t* manager,
    nimcp_state_manager_stats_t* stats
) {
    if (!manager || !stats) return -NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(manager->mutex);

    memset(stats, 0, sizeof(nimcp_state_manager_stats_t));

    stats->module_count = manager->module_count;
    stats->total_checkpoints = manager->total_checkpoints;
    stats->total_restores = manager->total_restores;
    stats->total_validations = manager->total_validations;
    stats->total_resets = manager->total_resets;
    stats->last_checkpoint_time = manager->last_full_checkpoint_time;
    stats->total_state_size = manager->last_checkpoint_size;

    /* Count enabled modules and validation failures */
    for (uint32_t i = 0; i < manager->module_count; i++) {
        if (manager->modules[i].enabled) {
            stats->enabled_modules++;
        }
        stats->validation_failures += manager->modules[i].validation_failures;
    }

    nimcp_mutex_unlock(manager->mutex);
    return NIMCP_SUCCESS;
}
