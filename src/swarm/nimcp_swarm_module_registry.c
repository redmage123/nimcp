/**
 * @file nimcp_swarm_module_registry.c
 * @brief Implementation of Swarm Module Registry
 * @version 1.0.0
 * @date 2025-12-15
 */

#include "swarm/nimcp_swarm_module_registry.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_module_registry)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 *
 * WHAT: Platform-independent time retrieval
 * WHY:  Need consistent timing across platforms
 * HOW:  Use platform abstraction layer
 */
static uint64_t get_time_ms(void) {
    return nimcp_time_get_ms();
}

/**
 * @brief Get current timestamp in microseconds
 *
 * WHAT: High-resolution timing for performance measurement
 * WHY:  Microsecond precision for update time tracking
 * HOW:  Use platform abstraction layer
 */
static uint64_t get_time_us(void) {
    return nimcp_time_get_us();
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int swarm_registry_default_config(swarm_registry_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("Null config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Category defaults */
    for (int i = 0; i < SWARM_MODULE_CATEGORY_COUNT; i++) {
        config->categories[i].enabled = true;
        config->categories[i].default_priority = SWARM_PRIORITY_NORMAL;
        config->categories[i].update_interval_ms = 0;  /* Every cycle */
    }

    /* Movement and defense higher priority */
    config->categories[SWARM_MODULE_CATEGORY_MOVEMENT].default_priority = SWARM_PRIORITY_HIGH;
    config->categories[SWARM_MODULE_CATEGORY_DEFENSE].default_priority = SWARM_PRIORITY_CRITICAL;

    /* Global settings */
    config->max_modules = SWARM_REGISTRY_MAX_MODULES;
    config->arbitration = SWARM_ARBITRATION_HIGHEST_PRIORITY;
    config->enable_auto_wiring = true;
    config->enable_bio_async = true;
    config->enable_statistics = true;

    /* Performance */
    config->max_updates_per_cycle = 0;  /* All modules */
    config->update_time_budget_ms = 0.0f;  /* Unlimited */

    return 0;
}

swarm_module_registry_t* swarm_registry_create(
    const swarm_registry_config_t* config
) {
    swarm_module_registry_t* registry = nimcp_malloc(sizeof(*registry));
    if (!registry) {
        NIMCP_LOGGING_ERROR("Failed to allocate registry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_registry_create: registry is NULL");
        return NULL;
    }

    memset(registry, 0, sizeof(*registry));

    /* Apply config */
    if (config) {
        registry->config = *config;
    } else {
        if (swarm_registry_default_config(&registry->config) != 0) {
            nimcp_free(registry);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_registry_create: default config failed");
            return NULL;
        }
    }

    /* Allocate module storage */
    registry->module_capacity = registry->config.max_modules;
    registry->modules = nimcp_malloc(
        registry->module_capacity * sizeof(swarm_module_entry_t)
    );
    if (!registry->modules) {
        NIMCP_LOGGING_ERROR("Failed to allocate module array");
        nimcp_free(registry);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_registry_create: registry->modules is NULL");
        return NULL;
    }
    memset(registry->modules, 0,
           registry->module_capacity * sizeof(swarm_module_entry_t));

    /* Create mutex */
    registry->mutex = nimcp_platform_mutex_create();
    if (!registry->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(registry->modules);
        nimcp_free(registry);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_registry_create: registry->mutex is NULL");
        return NULL;
    }

    /* Initialize state */
    registry->module_count = 0;
    registry->next_module_id = 1;
    registry->start_time = get_time_ms();
    registry->last_update_time = registry->start_time;
    registry->bio_async_connected = false;
    registry->brain_connected = false;

    NIMCP_LOGGING_INFO("Swarm registry created with %u module capacity",
                       registry->module_capacity);

    return registry;
}

void swarm_registry_destroy(swarm_module_registry_t* registry) {
    if (!registry) return;

    /* Disconnect integrations */
    swarm_registry_disconnect_bio_async(registry);
    swarm_registry_disconnect_swarm_brain(registry);

    /* Clean up modules array (caller responsible for destroying modules) */
    if (registry->modules) {
        nimcp_free(registry->modules);
    }

    /* Destroy mutex */
    if (registry->mutex) {
        nimcp_platform_mutex_destroy(registry->mutex);
        nimcp_free(registry->mutex);
        registry->mutex = NULL;
    }

    nimcp_free(registry);
    NIMCP_LOGGING_INFO("Swarm registry destroyed");
}

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

int swarm_registry_register_module(
    swarm_module_registry_t* registry,
    const char* name,
    swarm_module_category_t category,
    swarm_module_handle_t handle,
    const swarm_module_interface_t* interface,
    uint32_t priority,
    uint32_t* module_id_out
) {
    /* Guard clauses */
    if (!registry || !name || !handle || !interface || !module_id_out) {
        NIMCP_LOGGING_ERROR("Null parameter in register_module");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_register_module: required parameter is NULL (registry, name, handle, interface, module_id_out)");
        return -1;
    }

    if (category >= SWARM_MODULE_CATEGORY_COUNT) {
        NIMCP_LOGGING_ERROR("Invalid category: %u", category);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "swarm_registry_register_module: capacity exceeded");
        return -1;
    }

    if (!interface->update_fn) {
        NIMCP_LOGGING_ERROR("Module must provide update function");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_register_module: interface->update_fn is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(registry->mutex);

    /* Check capacity */
    if (registry->module_count >= registry->module_capacity) {
        NIMCP_LOGGING_ERROR("Registry full (%u/%u)",
                           registry->module_count, registry->module_capacity);
        nimcp_platform_mutex_unlock(registry->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "swarm_registry_register_module: capacity exceeded");
        return -1;
    }

    /* Create module entry */
    swarm_module_entry_t* entry = &registry->modules[registry->module_count];
    memset(entry, 0, sizeof(*entry));

    entry->module_id = registry->next_module_id++;
    strncpy(entry->module_name, name, SWARM_REGISTRY_MODULE_NAME_LEN - 1);
    entry->category = category;
    entry->handle = handle;
    entry->interface = *interface;
    entry->state = SWARM_MODULE_STATE_INITIALIZED;
    entry->priority = priority;
    entry->enabled = true;
    entry->last_update_time = get_time_ms();
    entry->wired_to_swarm_brain = false;

    /* Initialize module if callback provided */
    if (entry->interface.init_fn) {
        int result = entry->interface.init_fn(handle, registry);
        if (result != 0) {
            NIMCP_LOGGING_WARN("Module init failed for '%s': %d", name, result);
            entry->state = SWARM_MODULE_STATE_ERROR;
        }
    }

    registry->module_count++;
    registry->stats.total_modules++;

    /* Auto-wire to swarm brain if configured */
    if (registry->config.enable_auto_wiring && registry->brain_connected) {
        /* Note: Actual wiring implementation depends on swarm_brain API */
        entry->wired_to_swarm_brain = true;
        registry->stats.wired_to_brain++;
    }

    *module_id_out = entry->module_id;

    nimcp_platform_mutex_unlock(registry->mutex);

    NIMCP_LOGGING_INFO("Registered module '%s' (ID=%u, category=%s, priority=%u)",
                      name, entry->module_id,
                      swarm_module_category_to_string(category), priority);

    return 0;
}

int swarm_registry_unregister_module(
    swarm_module_registry_t* registry,
    uint32_t module_id
) {
    if (!registry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_unregister_module: registry is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(registry->mutex);

    /* Find module */
    int found_idx = -1;
    for (uint32_t i = 0; i < registry->module_count; i++) {
        if (registry->modules[i].module_id == module_id) {
            found_idx = (int)i;
            break;
        }
    }

    if (found_idx < 0) {
        nimcp_platform_mutex_unlock(registry->mutex);
        NIMCP_LOGGING_WARN("Module ID %u not found", module_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_registry_unregister_module: validation failed");
        return -1;
    }

    /* Remove by shifting array */
    if ((uint32_t)found_idx < registry->module_count - 1) {
        memmove(&registry->modules[found_idx],
                &registry->modules[found_idx + 1],
                (registry->module_count - found_idx - 1) * sizeof(swarm_module_entry_t));
    }

    registry->module_count--;

    nimcp_platform_mutex_unlock(registry->mutex);

    NIMCP_LOGGING_INFO("Unregistered module ID %u", module_id);
    return 0;
}

int swarm_registry_set_module_enabled(
    swarm_module_registry_t* registry,
    uint32_t module_id,
    bool enabled
) {
    if (!registry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_set_module_enabled: registry is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(registry->mutex);

    /* Find module */
    swarm_module_entry_t* entry = NULL;
    for (uint32_t i = 0; i < registry->module_count; i++) {
        if (registry->modules[i].module_id == module_id) {
            entry = &registry->modules[i];
            break;
        }
    }

    if (!entry) {
        nimcp_platform_mutex_unlock(registry->mutex);
        LOG_DEBUG("swarm_registry_set_module_enabled: module %u not found", module_id);
        return -1;
    }

    /* Call module's enable callback if provided */
    if (entry->interface.enable_fn) {
        int result = entry->interface.enable_fn(entry->handle, enabled);
        if (result != 0) {
            nimcp_platform_mutex_unlock(registry->mutex);
            NIMCP_LOGGING_WARN("Enable callback failed for module %u", module_id);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_registry_set_module_enabled: validation failed");
            return -1;
        }
    }

    entry->enabled = enabled;
    entry->state = enabled ? SWARM_MODULE_STATE_ACTIVE : SWARM_MODULE_STATE_DISABLED;

    nimcp_platform_mutex_unlock(registry->mutex);

    NIMCP_LOGGING_INFO("Module %u %s", module_id, enabled ? "enabled" : "disabled");
    return 0;
}

int swarm_registry_set_module_priority(
    swarm_module_registry_t* registry,
    uint32_t module_id,
    uint32_t priority
) {
    if (!registry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_set_module_priority: registry is NULL");
        return -1;
    }
    if (priority > SWARM_PRIORITY_CRITICAL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_registry_set_module_priority: validation failed");
        return -1;
    }

    nimcp_platform_mutex_lock(registry->mutex);

    /* Find module */
    swarm_module_entry_t* entry = NULL;
    for (uint32_t i = 0; i < registry->module_count; i++) {
        if (registry->modules[i].module_id == module_id) {
            entry = &registry->modules[i];
            break;
        }
    }

    if (!entry) {
        nimcp_platform_mutex_unlock(registry->mutex);
        LOG_DEBUG("swarm_registry_set_module_priority: module %u not found", module_id);
        return -1;
    }

    entry->priority = priority;

    nimcp_platform_mutex_unlock(registry->mutex);

    NIMCP_LOGGING_DEBUG("Module %u priority set to %u", module_id, priority);
    return 0;
}

const swarm_module_entry_t* swarm_registry_get_module(
    const swarm_module_registry_t* registry,
    uint32_t module_id
) {
    if (!registry) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "registry is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < registry->module_count; i++) {
        if (registry->modules[i].module_id == module_id) {
            return &registry->modules[i];
        }
    }

    return NULL;  /* Normal search miss - module not found */
}

const swarm_module_entry_t* swarm_registry_find_module_by_name(
    const swarm_module_registry_t* registry,
    const char* name
) {
    if (!registry || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_find_module_by_name: required parameter is NULL (registry, name)");
        return NULL;
    }

    for (uint32_t i = 0; i < registry->module_count; i++) {
        if (strncmp(registry->modules[i].module_name, name,
                    SWARM_REGISTRY_MODULE_NAME_LEN) == 0) {
            return &registry->modules[i];
        }
    }

    return NULL;  /* Normal search miss - module not found */
}

uint32_t swarm_registry_get_modules_by_category(
    const swarm_module_registry_t* registry,
    swarm_module_category_t category,
    const swarm_module_entry_t** modules,
    uint32_t max_modules
) {
    if (!registry || !modules || category >= SWARM_MODULE_CATEGORY_COUNT) {
        return 0;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < registry->module_count && count < max_modules; i++) {
        if (registry->modules[i].category == category) {
            modules[count++] = &registry->modules[i];
        }
    }

    return count;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int swarm_registry_update(
    swarm_module_registry_t* registry,
    uint64_t current_time_ms
) {
    if (!registry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_update: registry is NULL");
        return -1;
    }

    uint64_t cycle_start = get_time_us();
    int modules_updated = 0;
    uint32_t max_updates = registry->config.max_updates_per_cycle;
    if (max_updates == 0) max_updates = UINT32_MAX;

    nimcp_platform_mutex_lock(registry->mutex);

    for (uint32_t i = 0; i < registry->module_count &&
         (uint32_t)modules_updated < max_updates; i++) {
        swarm_module_entry_t* entry = &registry->modules[i];

        /* Skip if disabled or not active */
        if (!entry->enabled || entry->state != SWARM_MODULE_STATE_ACTIVE) {
            continue;
        }

        /* Check category interval */
        swarm_category_config_t* cat_cfg =
            &registry->config.categories[entry->category];
        if (cat_cfg->update_interval_ms > 0) {
            uint64_t elapsed = current_time_ms - entry->last_update_time;
            if (elapsed < cat_cfg->update_interval_ms) {
                continue;
            }
        }

        /* Update module */
        uint64_t delta_time = current_time_ms - entry->last_update_time;
        uint64_t update_start = get_time_us();

        int result = entry->interface.update_fn(entry->handle, delta_time);

        uint64_t update_time = get_time_us() - update_start;

        /* Update statistics */
        entry->last_update_time = current_time_ms;
        entry->update_count++;
        entry->total_update_time_us += update_time;

        if (result != 0) {
            entry->error_count++;
            registry->stats.total_errors++;
            NIMCP_LOGGING_WARN("Module %u update failed: %d",
                              entry->module_id, result);
        } else {
            modules_updated++;
        }

        /* Check time budget */
        if (registry->config.update_time_budget_ms > 0.0f) {
            float elapsed_ms = (get_time_us() - cycle_start) / 1000.0f;
            if (elapsed_ms >= registry->config.update_time_budget_ms) {
                registry->stats.overrun_count++;
                break;
            }
        }
    }

    registry->last_update_time = current_time_ms;
    registry->stats.total_update_cycles++;
    registry->stats.total_module_updates += modules_updated;

    /* Update cycle time statistics */
    uint64_t cycle_time = get_time_us() - cycle_start;
    float cycle_time_us = (float)cycle_time;
    if (registry->stats.total_update_cycles == 1) {
        registry->stats.avg_cycle_time_us = cycle_time_us;
        registry->stats.max_cycle_time_us = cycle_time_us;
    } else {
        float alpha = 0.1f;  /* EMA smoothing factor */
        registry->stats.avg_cycle_time_us =
            alpha * cycle_time_us + (1.0f - alpha) * registry->stats.avg_cycle_time_us;
        if (cycle_time_us > registry->stats.max_cycle_time_us) {
            registry->stats.max_cycle_time_us = cycle_time_us;
        }
    }

    nimcp_platform_mutex_unlock(registry->mutex);

    return modules_updated;
}

int swarm_registry_update_category(
    swarm_module_registry_t* registry,
    swarm_module_category_t category,
    uint64_t current_time_ms
) {
    if (!registry || category >= SWARM_MODULE_CATEGORY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "swarm_registry_update_category: registry is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(registry->mutex);

    int modules_updated = 0;

    for (uint32_t i = 0; i < registry->module_count; i++) {
        swarm_module_entry_t* entry = &registry->modules[i];

        if (entry->category != category || !entry->enabled ||
            entry->state != SWARM_MODULE_STATE_ACTIVE) {
            continue;
        }

        uint64_t delta_time = current_time_ms - entry->last_update_time;
        int result = entry->interface.update_fn(entry->handle, delta_time);

        entry->last_update_time = current_time_ms;
        entry->update_count++;

        if (result != 0) {
            entry->error_count++;
        } else {
            modules_updated++;
        }
    }

    nimcp_platform_mutex_unlock(registry->mutex);

    return modules_updated;
}

int swarm_registry_update_module(
    swarm_module_registry_t* registry,
    uint32_t module_id,
    uint64_t delta_time_ms
) {
    if (!registry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_update_module: registry is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(registry->mutex);

    swarm_module_entry_t* entry = NULL;
    for (uint32_t i = 0; i < registry->module_count; i++) {
        if (registry->modules[i].module_id == module_id) {
            entry = &registry->modules[i];
            break;
        }
    }

    if (!entry) {
        nimcp_platform_mutex_unlock(registry->mutex);
        LOG_DEBUG("swarm_registry_update_module: module %u not found", module_id);
        return -1;
    }

    int result = entry->interface.update_fn(entry->handle, delta_time_ms);

    entry->last_update_time = get_time_ms();
    entry->update_count++;

    if (result != 0) {
        entry->error_count++;
    }

    nimcp_platform_mutex_unlock(registry->mutex);

    return result;
}

/* ============================================================================
 * Arbitration API
 * ============================================================================ */

int swarm_registry_resolve_conflict(
    swarm_module_registry_t* registry,
    const uint32_t* module_ids,
    uint32_t module_count,
    uint32_t* winner_id_out
) {
    if (!registry || !module_ids || !winner_id_out || module_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_resolve_conflict: required parameter is NULL (registry, module_ids, winner_id_out)");
        return -1;
    }

    nimcp_platform_mutex_lock(registry->mutex);

    uint32_t highest_priority = 0;
    uint32_t winner_id = 0;

    /* Apply arbitration strategy */
    switch (registry->config.arbitration) {
    case SWARM_ARBITRATION_HIGHEST_PRIORITY:
        /* Find module with highest priority */
        for (uint32_t i = 0; i < module_count; i++) {
            const swarm_module_entry_t* entry =
                swarm_registry_get_module(registry, module_ids[i]);
            if (entry && entry->priority > highest_priority) {
                highest_priority = entry->priority;
                winner_id = entry->module_id;
            }
        }
        break;

    case SWARM_ARBITRATION_WEIGHTED_BLEND:
        /* Not implemented - would need output blending logic */
        NIMCP_LOGGING_WARN("Weighted blend arbitration not implemented");
        nimcp_platform_mutex_unlock(registry->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_registry_resolve_conflict: operation failed");
        return -1;

    case SWARM_ARBITRATION_SEQUENTIAL:
        /* Execute in priority order - return first valid */
        winner_id = module_ids[0];
        break;

    default:
        nimcp_platform_mutex_unlock(registry->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_registry_resolve_conflict: operation failed");
        return -1;
    }

    if (winner_id > 0) {
        registry->stats.conflicts_resolved++;
        *winner_id_out = winner_id;
    }

    nimcp_platform_mutex_unlock(registry->mutex);

    return (winner_id > 0) ? 0 : -1;
}

int swarm_registry_set_arbitration_strategy(
    swarm_module_registry_t* registry,
    swarm_arbitration_strategy_t strategy
) {
    if (!registry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_set_arbitration_strategy: registry is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(registry->mutex);
    registry->config.arbitration = strategy;
    nimcp_platform_mutex_unlock(registry->mutex);

    NIMCP_LOGGING_INFO("Arbitration strategy set to %s",
                      swarm_arbitration_strategy_to_string(strategy));
    return 0;
}

/* ============================================================================
 * Integration API
 * ============================================================================ */

int swarm_registry_connect_swarm_brain(
    swarm_module_registry_t* registry,
    swarm_brain_t* swarm_brain
) {
    if (!registry || !swarm_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_connect_swarm_brain: required parameter is NULL (registry, swarm_brain)");
        return -1;
    }

    nimcp_platform_mutex_lock(registry->mutex);
    registry->swarm_brain = swarm_brain;
    registry->brain_connected = true;
    nimcp_platform_mutex_unlock(registry->mutex);

    NIMCP_LOGGING_INFO("Connected to swarm brain");
    return 0;
}

int swarm_registry_disconnect_swarm_brain(swarm_module_registry_t* registry) {
    if (!registry) return 0;

    nimcp_platform_mutex_lock(registry->mutex);
    registry->swarm_brain = NULL;
    registry->brain_connected = false;
    nimcp_platform_mutex_unlock(registry->mutex);

    NIMCP_LOGGING_INFO("Disconnected from swarm brain");
    return 0;
}

int swarm_registry_connect_bio_async(swarm_module_registry_t* registry) {
    if (!registry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_connect_bio_async: registry is NULL");
        return -1;
    }

    bio_module_info_t info = {
        .module_id = SWARM_REGISTRY_BIO_MODULE_ID,
        .module_name = "swarm_registry",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_MEDIUM,
        .user_data = registry
    };

    registry->bio_context = bio_router_register_module(&info);
    if (registry->bio_context) {
        registry->bio_async_connected = true;
        registry->stats.bio_async_registered++;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available");
    return -1;  /* Bio-async not available - not a critical error */
}

int swarm_registry_disconnect_bio_async(swarm_module_registry_t* registry) {
    if (!registry || !registry->bio_async_connected) return 0;

    if (registry->bio_context) {
        bio_router_unregister_module(registry->bio_context);
        registry->bio_context = NULL;
    }

    registry->bio_async_connected = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

int swarm_registry_connect_brain_immune(
    swarm_module_registry_t* registry,
    brain_immune_system_t* immune
) {
    if (!registry || !immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_connect_brain_immune: required parameter is NULL (registry, immune)");
        return -1;
    }

    nimcp_platform_mutex_lock(registry->mutex);
    registry->brain_immune = immune;
    nimcp_platform_mutex_unlock(registry->mutex);

    NIMCP_LOGGING_INFO("Connected to brain immune system");
    return 0;
}

int swarm_registry_disconnect_brain_immune(swarm_module_registry_t* registry) {
    if (!registry) return 0;

    nimcp_platform_mutex_lock(registry->mutex);
    registry->brain_immune = NULL;
    nimcp_platform_mutex_unlock(registry->mutex);

    NIMCP_LOGGING_INFO("Disconnected from brain immune system");
    return 0;
}

/* ============================================================================
 * Internal Knowledge Graph Integration API
 * ============================================================================ */

int swarm_registry_connect_internal_kg(
    swarm_module_registry_t* registry,
    brain_t brain
) {
    if (!registry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_connect_internal_kg: registry is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(registry->mutex);

    /* Initialize KG context */
    int result = kg_module_init(
        &registry->kg_context,
        brain,
        "swarm_registry"
    );

    if (result != 0) {
        nimcp_platform_mutex_unlock(registry->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_registry_connect_internal_kg: validation failed");
        return -1;
    }

    /* Check if KG is available */
    if (!kg_is_available(&registry->kg_context)) {
        registry->kg_connected = false;
        NIMCP_LOGGING_DEBUG("Swarm registry KG disabled, graceful degradation");
        nimcp_platform_mutex_unlock(registry->mutex);
        return 0;  /* Success - just no KG */
    }

    registry->kg_connected = true;

    /* Try to find KG nodes for registered modules */
    for (uint32_t i = 0; i < registry->module_count; i++) {
        swarm_module_entry_t* entry = &registry->modules[i];

        /* Find node by module name */
        entry->kg_node_id = kg_find_node_safe(
            &registry->kg_context,
            entry->module_name
        );

        /* Node might not exist yet (dynamic registration) */
        if (entry->kg_node_id == BRAIN_KG_INVALID_NODE) {
            NIMCP_LOGGING_DEBUG("Swarm module %s not found in KG (may be dynamic)",
                entry->module_name);
        }
    }

    NIMCP_LOGGING_INFO("Swarm registry connected to internal KG");
    nimcp_platform_mutex_unlock(registry->mutex);
    return 0;
}

int swarm_registry_disconnect_internal_kg(swarm_module_registry_t* registry) {
    if (!registry) return 0;

    nimcp_platform_mutex_lock(registry->mutex);

    /* Clear all module KG node IDs */
    for (uint32_t i = 0; i < registry->module_count; i++) {
        registry->modules[i].kg_node_id = BRAIN_KG_INVALID_NODE;
    }

    /* Clear context */
    registry->kg_context.kg = NULL;
    registry->kg_context.kg_available = false;
    registry->kg_context.self_node_id = BRAIN_KG_INVALID_NODE;
    registry->kg_connected = false;

    NIMCP_LOGGING_DEBUG("Swarm registry disconnected from internal KG");
    nimcp_platform_mutex_unlock(registry->mutex);
    return 0;
}

int swarm_registry_create_module_kg_node(
    swarm_module_registry_t* registry,
    uint32_t module_id
) {
    if (!registry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_create_module_kg_node: registry is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(registry->mutex);

    /* Check if KG is connected */
    if (!registry->kg_connected || !kg_is_available(&registry->kg_context)) {
        nimcp_platform_mutex_unlock(registry->mutex);
        return 0;  /* No-op if KG not connected */
    }

    /* Find the module */
    swarm_module_entry_t* entry = NULL;
    for (uint32_t i = 0; i < registry->module_count; i++) {
        if (registry->modules[i].module_id == module_id) {
            entry = &registry->modules[i];
            break;
        }
    }

    if (!entry) {
        nimcp_platform_mutex_unlock(registry->mutex);
        LOG_DEBUG("swarm_registry_create_module_kg_node: module %u not found", module_id);
        return -1;
    }

    /* Skip if already has KG node */
    if (entry->kg_node_id != BRAIN_KG_INVALID_NODE) {
        nimcp_platform_mutex_unlock(registry->mutex);
        return 0;
    }

    brain_kg_t* kg = registry->kg_context.kg;

    /* Create the node */
    entry->kg_node_id = brain_kg_add_node(
        kg,
        entry->module_name,
        BRAIN_KG_NODE_SWARM,
        "Dynamically registered swarm module"
    );

    if (entry->kg_node_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_LOGGING_WARN("Failed to create KG node for swarm module %s",
            entry->module_name);
        nimcp_platform_mutex_unlock(registry->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_registry_create_module_kg_node: validation failed");
        return -1;
    }

    /* Add connection edge from registry to new module */
    brain_kg_add_edge(
        kg,
        registry->kg_context.self_node_id,
        entry->kg_node_id,
        BRAIN_KG_EDGE_COORDINATES_WITH,
        "Registry coordinates swarm module",
        1.0f
    );

    NIMCP_LOGGING_DEBUG("Created KG node for swarm module %s", entry->module_name);
    nimcp_platform_mutex_unlock(registry->mutex);
    return 0;
}

int swarm_registry_resolve_conflict_topology_aware(
    swarm_module_registry_t* registry,
    const uint32_t* module_ids,
    uint32_t module_count,
    uint32_t* winner_id_out
) {
    if (!registry || !module_ids || module_count == 0 || !winner_id_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_resolve_conflict_topology_aware: required parameter is NULL (registry, module_ids, winner_id_out)");
        return -1;
    }

    nimcp_platform_mutex_lock(registry->mutex);

    /* If KG not connected, fall back to standard resolution */
    if (!registry->kg_connected || !kg_is_available(&registry->kg_context)) {
        nimcp_platform_mutex_unlock(registry->mutex);
        return swarm_registry_resolve_conflict(registry, module_ids,
                                                module_count, winner_id_out);
    }

    /* Topology-aware resolution: prefer modules with more connections */
    uint32_t best_id = 0;
    uint32_t best_priority = 0;
    uint32_t best_connectivity = 0;

    for (uint32_t i = 0; i < module_count; i++) {
        uint32_t mid = module_ids[i];

        /* Find module entry */
        swarm_module_entry_t* entry = NULL;
        for (uint32_t j = 0; j < registry->module_count; j++) {
            if (registry->modules[j].module_id == mid) {
                entry = &registry->modules[j];
                break;
            }
        }
        if (!entry || !entry->enabled) continue;

        /* Get connectivity from KG */
        uint32_t connectivity = 0;
        if (entry->kg_node_id != BRAIN_KG_INVALID_NODE) {
            brain_kg_edge_list_t* edges = brain_kg_get_outgoing(
                registry->kg_context.kg,
                entry->kg_node_id
            );
            if (edges) {
                connectivity = edges->count;
                brain_kg_edge_list_destroy(edges);
            }
        }

        /* Compare: priority first, then connectivity */
        bool is_better = false;
        if (entry->priority > best_priority) {
            is_better = true;
        } else if (entry->priority == best_priority && connectivity > best_connectivity) {
            is_better = true;
        }

        if (is_better) {
            best_id = mid;
            best_priority = entry->priority;
            best_connectivity = connectivity;
        }
    }

    nimcp_platform_mutex_unlock(registry->mutex);

    if (best_id > 0) {
        *winner_id_out = best_id;
        registry->stats.conflicts_resolved++;
        return 0;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_registry_resolve_conflict_topology_aware: validation failed");
    return -1;
}

brain_kg_node_id_t swarm_registry_get_module_kg_node(
    const swarm_module_registry_t* registry,
    uint32_t module_id
) {
    if (!registry) return BRAIN_KG_INVALID_NODE;

    for (uint32_t i = 0; i < registry->module_count; i++) {
        if (registry->modules[i].module_id == module_id) {
            return registry->modules[i].kg_node_id;
        }
    }

    return BRAIN_KG_INVALID_NODE;
}

/* ============================================================================
 * Discovery API
 * ============================================================================ */

uint32_t swarm_registry_enumerate_modules(
    const swarm_module_registry_t* registry,
    const swarm_module_entry_t** modules,
    uint32_t max_modules
) {
    if (!registry || !modules) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < registry->module_count && count < max_modules; i++) {
        modules[count++] = &registry->modules[i];
    }

    return count;
}

uint32_t swarm_registry_get_category_count(
    const swarm_module_registry_t* registry,
    swarm_module_category_t category
) {
    if (!registry || category >= SWARM_MODULE_CATEGORY_COUNT) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < registry->module_count; i++) {
        if (registry->modules[i].category == category) {
            count++;
        }
    }

    return count;
}

bool swarm_registry_is_module_registered(
    const swarm_module_registry_t* registry,
    uint32_t module_id
) {
    if (!registry) {
        return false;
    }

    for (uint32_t i = 0; i < registry->module_count; i++) {
        if (registry->modules[i].module_id == module_id) {
            return true;
        }
    }

    return false;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int swarm_registry_get_stats(
    const swarm_module_registry_t* registry,
    swarm_registry_stats_t* stats
) {
    if (!registry || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_get_stats: required parameter is NULL (registry, stats)");
        return -1;
    }

    nimcp_platform_mutex_lock(registry->mutex);
    *stats = registry->stats;

    /* Compute active module count */
    stats->active_modules = 0;
    for (uint32_t i = 0; i < registry->module_count; i++) {
        if (registry->modules[i].enabled) {
            stats->active_modules++;
        }
    }

    nimcp_platform_mutex_unlock(registry->mutex);

    return 0;
}

void swarm_registry_reset_stats(swarm_module_registry_t* registry) {
    if (!registry) return;

    nimcp_platform_mutex_lock(registry->mutex);

    memset(&registry->stats, 0, sizeof(registry->stats));
    registry->stats.total_modules = registry->module_count;

    /* Reset per-module stats */
    for (uint32_t i = 0; i < registry->module_count; i++) {
        registry->modules[i].update_count = 0;
        registry->modules[i].total_update_time_us = 0;
        registry->modules[i].error_count = 0;
    }

    nimcp_platform_mutex_unlock(registry->mutex);

    NIMCP_LOGGING_INFO("Statistics reset");
}

int swarm_registry_get_module_stats(
    const swarm_module_registry_t* registry,
    uint32_t module_id,
    uint64_t* update_count,
    float* avg_time_us,
    uint32_t* error_count
) {
    if (!registry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_registry_get_module_stats: registry is NULL");
        return -1;
    }

    const swarm_module_entry_t* entry = swarm_registry_get_module(registry, module_id);
    if (!entry) {
        LOG_DEBUG("swarm_registry_get_module_stats: module %u not found", module_id);
        return -1;
    }

    if (update_count) *update_count = entry->update_count;
    if (error_count) *error_count = entry->error_count;

    if (avg_time_us) {
        if (entry->update_count > 0) {
            *avg_time_us = (float)entry->total_update_time_us / entry->update_count;
        } else {
            *avg_time_us = 0.0f;
        }
    }

    return 0;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* swarm_module_category_to_string(swarm_module_category_t category) {
    switch (category) {
    case SWARM_MODULE_CATEGORY_MOVEMENT:      return "movement";
    case SWARM_MODULE_CATEGORY_COMMUNICATION: return "communication";
    case SWARM_MODULE_CATEGORY_MEMORY:        return "memory";
    case SWARM_MODULE_CATEGORY_DEFENSE:       return "defense";
    case SWARM_MODULE_CATEGORY_COORDINATION:  return "coordination";
    case SWARM_MODULE_CATEGORY_EMERGENCE:     return "emergence";
    case SWARM_MODULE_CATEGORY_LEARNING:      return "learning";
    case SWARM_MODULE_CATEGORY_CUSTOM:        return "custom";
    default:                                  return "unknown";
    }
}

const char* swarm_module_state_to_string(swarm_module_state_t state) {
    switch (state) {
    case SWARM_MODULE_STATE_UNINITIALIZED: return "uninitialized";
    case SWARM_MODULE_STATE_INITIALIZED:   return "initialized";
    case SWARM_MODULE_STATE_ACTIVE:        return "active";
    case SWARM_MODULE_STATE_DISABLED:      return "disabled";
    case SWARM_MODULE_STATE_ERROR:         return "error";
    case SWARM_MODULE_STATE_SHUTDOWN:      return "shutdown";
    default:                               return "unknown";
    }
}

const char* swarm_arbitration_strategy_to_string(
    swarm_arbitration_strategy_t strategy
) {
    switch (strategy) {
    case SWARM_ARBITRATION_HIGHEST_PRIORITY: return "highest_priority";
    case SWARM_ARBITRATION_WEIGHTED_BLEND:   return "weighted_blend";
    case SWARM_ARBITRATION_SEQUENTIAL:       return "sequential";
    case SWARM_ARBITRATION_CUSTOM:           return "custom";
    default:                                 return "unknown";
    }
}
