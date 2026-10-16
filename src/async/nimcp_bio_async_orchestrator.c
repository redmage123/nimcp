/**
 * @file nimcp_bio_async_orchestrator.c
 * @brief Bio-Async Orchestrator implementation
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#include "async/nimcp_bio_async_orchestrator.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_wiring_diagram.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_common.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(bio_async_orchestrator)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Find module entry by ID
 * WHY:  Centralize lookup logic
 * HOW:  Linear search through module array
 */
static bio_module_entry_t* find_module(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_module: orchestrator is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < orchestrator->module_count; i++) {
        if (orchestrator->modules[i].module_id == module_id) {
            return &orchestrator->modules[i];
        }
    }
    return NULL;  /* Module not found - normal return */
}

/**
 * WHAT: Compute system health score
 * WHY:  Aggregate module health into single metric
 * HOW:  Weighted average: healthy=1.0, degraded=0.7, unhealthy=0.3, failed=0.0
 */
static float compute_health_score(const bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator || orchestrator->module_count == 0) return 0.0f;

    uint32_t healthy = 0, degraded = 0, unhealthy = 0, failed = 0;

    for (uint32_t i = 0; i < orchestrator->module_count; i++) {
        switch (orchestrator->modules[i].health_status) {
            case BIO_MODULE_HEALTH_HEALTHY: healthy++; break;
            case BIO_MODULE_HEALTH_DEGRADED: degraded++; break;
            case BIO_MODULE_HEALTH_UNHEALTHY: unhealthy++; break;
            case BIO_MODULE_HEALTH_FAILED: failed++; break;
            default: break;
        }
    }

    float score = (healthy * 1.0f + degraded * 0.7f + unhealthy * 0.3f + failed * 0.0f);
    return score / (float)orchestrator->module_count;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int bio_orchestrator_default_config(bio_orchestrator_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(bio_orchestrator_config_t));

    /* Category defaults */
    for (int i = 0; i < BIO_MODULE_CATEGORY_COUNT; i++) {
        config->categories[i].enabled = true;
        config->categories[i].health_check_interval_ms = BIO_ORCHESTRATOR_HEALTH_CHECK_MS;
        config->categories[i].max_health_failures = 3;
    }

    /* Global settings */
    config->max_modules = BIO_ORCHESTRATOR_MAX_MODULES;
    config->enable_auto_health_check = true;
    config->global_health_check_ms = BIO_ORCHESTRATOR_HEALTH_CHECK_MS;

    /* Integration */
    config->enable_bio_async = true;
    config->enable_brain_immune = false;
    config->enable_statistics = true;
    config->enable_logging = true;

    /* Startup */
    config->enforce_startup_order = true;
    config->startup_timeout_ms = 10000;

    return 0;
}

bio_async_orchestrator_t* bio_orchestrator_create(const bio_orchestrator_config_t* config) {
    /* nimcp_calloc zeroes all pointers for safe cleanup */
    bio_async_orchestrator_t* orch = (bio_async_orchestrator_t*)nimcp_calloc(
        1, sizeof(bio_async_orchestrator_t));
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "bio_orchestrator_create: failed to allocate orchestrator");
        NIMCP_LOGGING_ERROR("Failed to allocate orchestrator");
        return NULL;
    }

    /* Apply config */
    if (config) {
        orch->config = *config;
    } else {
        bio_orchestrator_default_config(&orch->config);
    }

    /* Allocate module array */
    orch->module_capacity = orch->config.max_modules;
    orch->modules = (bio_module_entry_t*)nimcp_calloc(
        orch->module_capacity, sizeof(bio_module_entry_t));
    if (!orch->modules) goto cleanup;

    /* Create mutex */
    orch->mutex = nimcp_platform_mutex_create();
    if (!orch->mutex) goto cleanup;

    /* Initialize state */
    orch->state = BIO_ORCHESTRATOR_STOPPED;
    orch->module_count = 0;
    orch->start_time = nimcp_time_get_ms();
    orch->bio_async_connected = false;
    orch->immune_connected = false;
    orch->current_startup_phase = 0;

    /* Connect to bio-async if enabled */
    if (orch->config.enable_bio_async) {
        bio_module_info_t info = {
            .module_id = BIO_MODULE_BIO_ASYNC_ORCHESTRATOR,
            .module_name = BIO_ORCHESTRATOR_MODULE_NAME,
            .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
            .user_data = orch
        };

        orch->bio_context = bio_router_register_module(&info);
        if (orch->bio_context) {
            orch->bio_async_connected = true;
            if (orch->config.enable_logging) {
                NIMCP_LOGGING_INFO("Bio-async orchestrator registered with router");
            }
        }
    }

    return orch;

cleanup:
    NIMCP_LOGGING_ERROR("Failed to allocate orchestrator resources");
    if (orch->mutex) {
        nimcp_platform_mutex_destroy(orch->mutex);
        nimcp_free(orch->mutex);
    }
    nimcp_free(orch->modules);
    nimcp_free(orch);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bio_orchestrator_create: allocation failed");
    return NULL;
}

void bio_orchestrator_destroy(bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator) return;

    /* Stop if running */
    if (orchestrator->state == BIO_ORCHESTRATOR_RUNNING) {
        bio_orchestrator_stop(orchestrator);
    }

    /* Disconnect from bio-async */
    if (orchestrator->bio_async_connected && orchestrator->bio_context) {
        bio_router_unregister_module(orchestrator->bio_context);
        orchestrator->bio_async_connected = false;
    }

    /* Free module dependencies and strdup'd names (P2-50 fix) */
    for (uint32_t i = 0; i < orchestrator->module_count; i++) {
        if (orchestrator->modules[i].module_name) {
            nimcp_free((void*)orchestrator->modules[i].module_name);
        }
        if (orchestrator->modules[i].dependencies) {
            nimcp_free(orchestrator->modules[i].dependencies);
        }
    }

    /* Free arrays */
    nimcp_free(orchestrator->modules);

    /* Destroy mutex */
    if (orchestrator->mutex) {
        nimcp_platform_mutex_destroy(orchestrator->mutex);
        nimcp_free(orchestrator->mutex);
        orchestrator->mutex = NULL;
    }

    /* Free orchestrator */
    nimcp_free(orchestrator);
}

int bio_orchestrator_start(bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_start: orchestrator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    if (orchestrator->state == BIO_ORCHESTRATOR_RUNNING) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        return 0;  /* Already running */
    }

    orchestrator->state = BIO_ORCHESTRATOR_RUNNING;
    orchestrator->start_time = nimcp_time_get_ms();
    orchestrator->state_version++;  // P2 fix: Increment version on state change

    /* Discover wiring from KG if wiring diagram is set */
    if (orchestrator->wiring_diagram) {
        uint32_t version_before = orchestrator->state_version;
        nimcp_platform_mutex_unlock(orchestrator->mutex);

        int discovered = bio_orchestrator_discover_all_wiring(orchestrator);
        if (orchestrator->config.enable_logging && discovered > 0) {
            NIMCP_LOGGING_INFO("Discovered wiring for %d modules from KG", discovered);
        }

        /* Invoke handler callbacks with discovered message types */
        int invoked = bio_orchestrator_invoke_handler_callbacks(orchestrator);
        if (orchestrator->config.enable_logging && invoked > 0) {
            NIMCP_LOGGING_INFO("Invoked handler callbacks for %d modules", invoked);
        }

        nimcp_platform_mutex_lock(orchestrator->mutex);

        // P2 fix: Re-validate state after reacquiring mutex.
        // If state_version changed, another thread modified state while we were unlocked.
        if (orchestrator->state_version != version_before) {
            if (orchestrator->config.enable_logging) {
                NIMCP_LOGGING_WARN("Orchestrator state changed during wiring discovery "
                                   "(version %u -> %u), state may be %d",
                                   version_before, orchestrator->state_version,
                                   orchestrator->state);
            }
        }
    }

    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("Bio-async orchestrator started");
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}

int bio_orchestrator_stop(bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_stop: orchestrator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    if (orchestrator->state == BIO_ORCHESTRATOR_STOPPED) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        return 0;  /* Already stopped */
    }

    orchestrator->state = BIO_ORCHESTRATOR_STOPPED;

    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("Bio-async orchestrator stopped");
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}

/* ============================================================================
 * Module Registration Implementation
 * ============================================================================ */

int bio_orchestrator_register_module(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id,
    const char* name,
    bio_module_category_t category,
    bio_module_context_t bio_context,
    uint32_t startup_phase
) {
    if (!orchestrator || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_register_module: required parameter is NULL (orchestrator, name)");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    /* Check capacity */
    if (orchestrator->module_count >= orchestrator->module_capacity) {
        NIMCP_LOGGING_ERROR("Module registry full");
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bio_orchestrator_register_module: capacity exceeded");
        return -1;
    }

    /* Check if already registered */
    if (find_module(orchestrator, module_id)) {
        if (orchestrator->config.enable_logging) {
            NIMCP_LOGGING_WARN("Module already registered: %s", name);
        }
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        return 0;  /* Not an error */
    }

    /* Add module */
    bio_module_entry_t* entry = &orchestrator->modules[orchestrator->module_count];
    memset(entry, 0, sizeof(bio_module_entry_t));

    entry->module_id = module_id;
    /* P2-50 fix: Copy the string to prevent dangling pointer if caller frees name */
    entry->module_name = name ? nimcp_strdup(name) : NULL;
    entry->category = category;
    entry->bio_context = bio_context;
    entry->startup_phase = startup_phase;
    entry->health_status = BIO_MODULE_HEALTH_UNKNOWN;
    entry->registered = true;
    entry->enabled = true;
    entry->registration_time = nimcp_time_get_ms();

    orchestrator->module_count++;
    orchestrator->stats.total_modules = orchestrator->module_count;

    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("Registered module: %s (ID=0x%04X, category=%d, phase=%u)",
            name, module_id, category, startup_phase);
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}

int bio_orchestrator_unregister_module(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_unregister_module: orchestrator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    /* Find module */
    int found_idx = -1;
    for (uint32_t i = 0; i < orchestrator->module_count; i++) {
        if (orchestrator->modules[i].module_id == module_id) {
            found_idx = (int)i;
            break;
        }
    }

    if (found_idx < 0) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bio_orchestrator_unregister_module: validation failed");
        return -1;
    }

    /* P2-50 fix: Free strdup'd module name */
    if (orchestrator->modules[found_idx].module_name) {
        nimcp_free((void*)orchestrator->modules[found_idx].module_name);
    }

    /* Free dependencies */
    if (orchestrator->modules[found_idx].dependencies) {
        nimcp_free(orchestrator->modules[found_idx].dependencies);
    }

    /* Shift remaining modules */
    if (found_idx < (int)orchestrator->module_count - 1) {
        memmove(&orchestrator->modules[found_idx],
                &orchestrator->modules[found_idx + 1],
                (orchestrator->module_count - found_idx - 1) * sizeof(bio_module_entry_t));
    }

    orchestrator->module_count--;
    orchestrator->stats.total_modules = orchestrator->module_count;

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}

int bio_orchestrator_add_dependency(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id,
    bio_module_id_t depends_on
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_add_dependency: orchestrator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    bio_module_entry_t* entry = find_module(orchestrator, module_id);
    if (!entry) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_add_dependency: entry is NULL");
        return -1;
    }

    /* Reallocate dependency array */
    bio_module_id_t* new_deps = (bio_module_id_t*)nimcp_realloc(
        entry->dependencies,
        (entry->dependency_count + 1) * sizeof(bio_module_id_t)
    );
    if (!new_deps) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_add_dependency: new_deps is NULL");
        return -1;
    }

    new_deps[entry->dependency_count] = depends_on;
    entry->dependencies = new_deps;
    entry->dependency_count++;

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}

int bio_orchestrator_set_module_enabled(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id,
    bool enabled
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_set_module_enabled: orchestrator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    bio_module_entry_t* entry = find_module(orchestrator, module_id);
    if (!entry) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_set_module_enabled: entry is NULL");
        return -1;
    }

    entry->enabled = enabled;

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}

/* ============================================================================
 * Startup Sequencing Implementation
 * ============================================================================ */

int bio_orchestrator_execute_startup(bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_execute_startup: orchestrator is NULL");
        return -1;
    }

    if (!orchestrator->config.enforce_startup_order) {
        /* No ordering enforcement */
        return 0;
    }

    /* Execute phases in order */
    for (uint32_t phase = 0; phase < BIO_STARTUP_PHASE_COUNT; phase++) {
        uint32_t phase_count = 0;

        for (uint32_t i = 0; i < orchestrator->module_count; i++) {
            if (orchestrator->modules[i].startup_phase == phase &&
                orchestrator->modules[i].enabled) {
                phase_count++;
            }
        }

        if (phase_count > 0 && orchestrator->config.enable_logging) {
            NIMCP_LOGGING_INFO("Startup phase %u: %u modules", phase, phase_count);
        }
    }

    return 0;
}

uint32_t bio_orchestrator_get_phase_modules(
    const bio_async_orchestrator_t* orchestrator,
    uint32_t phase,
    bio_module_id_t* module_ids,
    uint32_t max_modules
) {
    if (!orchestrator || !module_ids || phase >= BIO_STARTUP_PHASE_COUNT) {
        return 0;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < orchestrator->module_count && count < max_modules; i++) {
        if (orchestrator->modules[i].startup_phase == phase) {
            module_ids[count++] = orchestrator->modules[i].module_id;
        }
    }

    return count;
}

/* ============================================================================
 * Health Monitoring Implementation
 * ============================================================================ */

int bio_orchestrator_health_check_all(bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_health_check_all: orchestrator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    uint64_t current_time = nimcp_time_get_ms();
    orchestrator->last_health_check = current_time;
    orchestrator->stats.total_health_checks++;

    uint32_t healthy_count = 0;

    for (uint32_t i = 0; i < orchestrator->module_count; i++) {
        bio_module_entry_t* entry = &orchestrator->modules[i];

        if (!entry->enabled) continue;

        /* Simple health check: assume healthy if registered */
        entry->health_status = BIO_MODULE_HEALTH_HEALTHY;
        entry->last_health_check = current_time;
        entry->health_check_count++;
        healthy_count++;
    }

    orchestrator->stats.healthy_modules = healthy_count;
    orchestrator->stats.system_health_score = compute_health_score(orchestrator);

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return (int)healthy_count;
}

int bio_orchestrator_health_check_module(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id,
    bio_module_health_t* health_out
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_health_check_module: orchestrator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    bio_module_entry_t* entry = find_module(orchestrator, module_id);
    if (!entry) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_health_check_module: entry is NULL");
        return -1;
    }

    /* Simple check: assume healthy if registered */
    entry->health_status = BIO_MODULE_HEALTH_HEALTHY;
    entry->last_health_check = nimcp_time_get_ms();
    entry->health_check_count++;

    if (health_out) {
        *health_out = entry->health_status;
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}

bio_module_health_t bio_orchestrator_get_module_health(
    const bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id
) {
    if (!orchestrator) return BIO_MODULE_HEALTH_UNKNOWN;

    /* Cast away const for mutex lock - safe since we're only reading */
    bio_async_orchestrator_t* mutable_orch = (bio_async_orchestrator_t*)orchestrator;
    nimcp_platform_mutex_lock(mutable_orch->mutex);

    bio_module_health_t health = BIO_MODULE_HEALTH_UNKNOWN;
    for (uint32_t i = 0; i < orchestrator->module_count; i++) {
        if (orchestrator->modules[i].module_id == module_id) {
            health = orchestrator->modules[i].health_status;
            break;
        }
    }

    nimcp_platform_mutex_unlock(mutable_orch->mutex);
    return health;
}

/* ============================================================================
 * Discovery Implementation
 * ============================================================================ */

uint32_t bio_orchestrator_get_all_modules(
    const bio_async_orchestrator_t* orchestrator,
    bio_module_id_t* module_ids,
    uint32_t max_modules
) {
    if (!orchestrator || !module_ids) return 0;

    /* Cast away const for mutex lock - safe since we're only reading */
    bio_async_orchestrator_t* mutable_orch = (bio_async_orchestrator_t*)orchestrator;
    nimcp_platform_mutex_lock(mutable_orch->mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < orchestrator->module_count && count < max_modules; i++) {
        module_ids[count++] = orchestrator->modules[i].module_id;
    }

    nimcp_platform_mutex_unlock(mutable_orch->mutex);
    return count;
}

uint32_t bio_orchestrator_get_modules_by_category(
    const bio_async_orchestrator_t* orchestrator,
    bio_module_category_t category,
    bio_module_id_t* module_ids,
    uint32_t max_modules
) {
    if (!orchestrator || !module_ids) return 0;

    /* Cast away const for mutex lock - safe since we're only reading */
    bio_async_orchestrator_t* mutable_orch = (bio_async_orchestrator_t*)orchestrator;
    nimcp_platform_mutex_lock(mutable_orch->mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < orchestrator->module_count && count < max_modules; i++) {
        if (orchestrator->modules[i].category == category) {
            module_ids[count++] = orchestrator->modules[i].module_id;
        }
    }

    nimcp_platform_mutex_unlock(mutable_orch->mutex);
    return count;
}

const bio_module_entry_t* bio_orchestrator_get_module_info(
    const bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_get_module_info: orchestrator is NULL");
        return NULL;
    }

    /* P2-51 fix: The returned pointer is to internal data. We hold the lock
     * during lookup to ensure consistent read, but the pointer remains valid
     * only while the orchestrator is not modified (no register/unregister).
     *
     * IMPORTANT: Caller must ensure no concurrent modification. For a fully
     * safe API, use bio_orchestrator_copy_module_info() with a caller-provided buffer.
     * We document this contract rather than copy because bio_module_entry_t contains
     * nested pointers (dependencies, bio_context) that cannot be safely deep-copied. */
    bio_async_orchestrator_t* mutable_orch = (bio_async_orchestrator_t*)orchestrator;
    nimcp_platform_mutex_lock(mutable_orch->mutex);

    const bio_module_entry_t* result = NULL;
    for (uint32_t i = 0; i < orchestrator->module_count; i++) {
        if (orchestrator->modules[i].module_id == module_id) {
            result = &orchestrator->modules[i];
            break;
        }
    }

    nimcp_platform_mutex_unlock(mutable_orch->mutex);
    return result;
}

bool bio_orchestrator_is_module_registered(
    const bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id
) {
    return bio_orchestrator_get_module_info(orchestrator, module_id) != NULL;
}

/* ============================================================================
 * Integration Implementation
 * ============================================================================ */

int bio_orchestrator_connect_brain_immune(
    bio_async_orchestrator_t* orchestrator,
    void* immune
) {
    if (!orchestrator || !immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_connect_brain_immune: required parameter is NULL (orchestrator, immune)");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    orchestrator->brain_immune = immune;
    orchestrator->immune_connected = true;

    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("Connected to brain immune system");
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}

int bio_orchestrator_disconnect_brain_immune(bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_disconnect_brain_immune: orchestrator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    orchestrator->brain_immune = NULL;
    orchestrator->immune_connected = false;

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

int bio_orchestrator_get_stats(
    const bio_async_orchestrator_t* orchestrator,
    bio_orchestrator_stats_t* stats
) {
    if (!orchestrator || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_get_stats: required parameter is NULL (orchestrator, stats)");
        return -1;
    }

    /* P2-52 fix: Acquire lock for consistent stats read */
    bio_async_orchestrator_t* mutable_orch = (bio_async_orchestrator_t*)orchestrator;
    nimcp_platform_mutex_lock(mutable_orch->mutex);

    *stats = orchestrator->stats;

    /* Update runtime stats */
    stats->uptime_ms = nimcp_time_get_ms() - orchestrator->start_time;

    /* Count active modules */
    uint32_t active = 0;
    for (uint32_t i = 0; i < orchestrator->module_count; i++) {
        if (orchestrator->modules[i].enabled) active++;
    }
    stats->active_modules = active;

    nimcp_platform_mutex_unlock(mutable_orch->mutex);

    return 0;
}

void bio_orchestrator_reset_stats(bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator) return;

    nimcp_platform_mutex_lock(orchestrator->mutex);

    memset(&orchestrator->stats, 0, sizeof(bio_orchestrator_stats_t));
    orchestrator->stats.total_modules = orchestrator->module_count;

    nimcp_platform_mutex_unlock(orchestrator->mutex);
}

float bio_orchestrator_get_health_score(const bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator) return 0.0f;

    /* Cast away const for mutex lock - safe since we're only reading */
    bio_async_orchestrator_t* mutable_orch = (bio_async_orchestrator_t*)orchestrator;
    nimcp_platform_mutex_lock(mutable_orch->mutex);

    float score = compute_health_score(orchestrator);

    nimcp_platform_mutex_unlock(mutable_orch->mutex);
    return score;
}

bio_orchestrator_state_t bio_orchestrator_get_state(
    const bio_async_orchestrator_t* orchestrator
) {
    if (!orchestrator) return BIO_ORCHESTRATOR_ERROR;

    /* Cast away const for mutex lock - safe since we're only reading */
    bio_async_orchestrator_t* mutable_orch = (bio_async_orchestrator_t*)orchestrator;
    nimcp_platform_mutex_lock(mutable_orch->mutex);

    bio_orchestrator_state_t state = orchestrator->state;

    nimcp_platform_mutex_unlock(mutable_orch->mutex);
    return state;
}

/* ============================================================================
 * String Conversion Implementation
 * ============================================================================ */

const char* bio_module_category_to_string(bio_module_category_t category) {
    switch (category) {
        case BIO_MODULE_CATEGORY_CORE: return "core";
        case BIO_MODULE_CATEGORY_PLASTICITY: return "plasticity";
        case BIO_MODULE_CATEGORY_PERCEPTION: return "perception";
        case BIO_MODULE_CATEGORY_COGNITIVE: return "cognitive";
        case BIO_MODULE_CATEGORY_HIGHLEVEL: return "highlevel";
        case BIO_MODULE_CATEGORY_IMMUNE: return "immune";
        case BIO_MODULE_CATEGORY_SWARM: return "swarm";
        case BIO_MODULE_CATEGORY_SECURITY: return "security";
        case BIO_MODULE_CATEGORY_MIDDLEWARE: return "middleware";
        case BIO_MODULE_CATEGORY_GLIAL: return "glial";
        default: return "unknown";
    }
}

const char* bio_orchestrator_state_to_string(bio_orchestrator_state_t state) {
    switch (state) {
        case BIO_ORCHESTRATOR_STOPPED: return "stopped";
        case BIO_ORCHESTRATOR_STARTING: return "starting";
        case BIO_ORCHESTRATOR_RUNNING: return "running";
        case BIO_ORCHESTRATOR_PAUSED: return "paused";
        case BIO_ORCHESTRATOR_STOPPING: return "stopping";
        case BIO_ORCHESTRATOR_ERROR: return "error";
        default: return "unknown";
    }
}

const char* bio_module_health_to_string(bio_module_health_t health) {
    switch (health) {
        case BIO_MODULE_HEALTH_UNKNOWN: return "unknown";
        case BIO_MODULE_HEALTH_HEALTHY: return "healthy";
        case BIO_MODULE_HEALTH_DEGRADED: return "degraded";
        case BIO_MODULE_HEALTH_UNHEALTHY: return "unhealthy";
        case BIO_MODULE_HEALTH_FAILED: return "failed";
        default: return "unknown";
    }
}

/* ============================================================================
 * Internal Knowledge Graph Integration Implementation
 * ============================================================================ */

/**
 * WHAT: Map bio-module health to KG node state
 * WHY:  Consistent state representation across systems
 * HOW:  Direct mapping based on semantics
 */
static brain_kg_node_state_t health_to_kg_state(bio_module_health_t health) {
    switch (health) {
        case BIO_MODULE_HEALTH_HEALTHY:
        case BIO_MODULE_HEALTH_DEGRADED:
            return BRAIN_KG_STATE_ACTIVE;
        case BIO_MODULE_HEALTH_UNHEALTHY:
        case BIO_MODULE_HEALTH_FAILED:
            return BRAIN_KG_STATE_ERROR;
        case BIO_MODULE_HEALTH_UNKNOWN:
        default:
            return BRAIN_KG_STATE_UNKNOWN;
    }
}

int bio_orchestrator_connect_internal_kg(
    bio_async_orchestrator_t* orchestrator,
    brain_t brain
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_connect_internal_kg: orchestrator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    /* Initialize KG context */
    int result = kg_module_init(
        &orchestrator->kg_context,
        brain,
        BIO_ORCHESTRATOR_MODULE_NAME
    );

    if (result != 0) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bio_orchestrator_connect_internal_kg: validation failed");
        return -1;
    }

    /* Check if KG is available */
    if (!kg_is_available(&orchestrator->kg_context)) {
        /* KG disabled - graceful degradation */
        orchestrator->kg_connected = false;
        if (orchestrator->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Bio-orchestrator KG disabled, graceful degradation");
        }
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        return 0;  /* Success - just no KG */
    }

    orchestrator->kg_connected = true;

    /* Try to find or create KG nodes for registered modules */
    for (uint32_t i = 0; i < orchestrator->module_count; i++) {
        bio_module_entry_t* entry = &orchestrator->modules[i];

        /* Find node by module name */
        entry->kg_node_id = kg_find_node_safe(
            &orchestrator->kg_context,
            entry->module_name
        );

        /* Node might not exist yet (dynamic registration) */
        if (entry->kg_node_id == BRAIN_KG_INVALID_NODE) {
            if (orchestrator->config.enable_logging) {
                NIMCP_LOGGING_DEBUG("Module %s not found in KG (may be dynamic)",
                    entry->module_name);
            }
        }
    }

    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("Bio-orchestrator connected to internal KG");
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}

int bio_orchestrator_disconnect_internal_kg(bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_disconnect_internal_kg: orchestrator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    /* Clear all module KG node IDs */
    for (uint32_t i = 0; i < orchestrator->module_count; i++) {
        orchestrator->modules[i].kg_node_id = BRAIN_KG_INVALID_NODE;
    }

    /* Clear context */
    orchestrator->kg_context.kg = NULL;
    orchestrator->kg_context.kg_available = false;
    orchestrator->kg_context.self_node_id = BRAIN_KG_INVALID_NODE;
    orchestrator->kg_connected = false;

    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Bio-orchestrator disconnected from internal KG");
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}

int bio_orchestrator_sync_health_to_kg(bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_sync_health_to_kg: orchestrator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    /* Check if KG is connected */
    if (!orchestrator->kg_connected || !kg_is_available(&orchestrator->kg_context)) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        return 0;  /* No-op if KG not connected */
    }

    brain_kg_t* kg = orchestrator->kg_context.kg;
    if (!kg) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        return 0;
    }

    /* Sync each module's health to its KG node state */
    for (uint32_t i = 0; i < orchestrator->module_count; i++) {
        bio_module_entry_t* entry = &orchestrator->modules[i];

        if (entry->kg_node_id == BRAIN_KG_INVALID_NODE) {
            continue;  /* No KG node for this module */
        }

        /* Map health to KG state */
        brain_kg_node_state_t kg_state = health_to_kg_state(entry->health_status);

        /* Update node state (description=NULL to keep current) */
        brain_kg_update_node(kg, entry->kg_node_id, NULL, kg_state);

        /* Add degraded metadata if applicable */
        if (entry->health_status == BIO_MODULE_HEALTH_DEGRADED) {
            brain_kg_add_metadata(
                kg,
                entry->kg_node_id,
                "health_degraded",
                "true"
            );
        }
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}

int bio_orchestrator_validate_startup_ordering(bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_validate_startup_ordering: orchestrator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    /* Graceful degradation if KG not connected */
    if (!orchestrator->kg_connected || !kg_is_available(&orchestrator->kg_context)) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        return 0;  /* Assume valid if can't check */
    }

    bool valid = true;

    /* Check each module's dependencies in KG */
    for (uint32_t i = 0; i < orchestrator->module_count && valid; i++) {
        bio_module_entry_t* entry = &orchestrator->modules[i];

        if (entry->kg_node_id == BRAIN_KG_INVALID_NODE) {
            continue;  /* No KG node - can't validate */
        }

        /* Get incoming edges to this module's node from KG */
        brain_kg_edge_list_t* deps = brain_kg_get_incoming(
            orchestrator->kg_context.kg,
            entry->kg_node_id
        );
        if (!deps) continue;

        /* Check each dependency's startup phase */
        for (uint32_t j = 0; j < deps->count; j++) {
            const brain_kg_edge_t* edge = deps->edges[j];

            if (!edge || edge->type != BRAIN_KG_EDGE_DEPENDS_ON) {
                continue;
            }

            /* Find the dependency module by its node ID */
            for (uint32_t k = 0; k < orchestrator->module_count; k++) {
                if (orchestrator->modules[k].kg_node_id == edge->from) {
                    /* Dependency found - check startup phase ordering */
                    if (orchestrator->modules[k].startup_phase > entry->startup_phase) {
                        if (orchestrator->config.enable_logging) {
                            NIMCP_LOGGING_WARN(
                                "Invalid startup order: %s (phase %u) depends on %s (phase %u)",
                                entry->module_name, entry->startup_phase,
                                orchestrator->modules[k].module_name,
                                orchestrator->modules[k].startup_phase
                            );
                        }
                        valid = false;
                    }
                    break;
                }
            }
        }

        brain_kg_edge_list_destroy(deps);
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return valid ? 0 : -1;
}

brain_kg_node_id_t bio_orchestrator_get_module_kg_node(
    const bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id
) {
    if (!orchestrator) return BRAIN_KG_INVALID_NODE;

    for (uint32_t i = 0; i < orchestrator->module_count; i++) {
        if (orchestrator->modules[i].module_id == module_id) {
            return orchestrator->modules[i].kg_node_id;
        }
    }

    return BRAIN_KG_INVALID_NODE;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from the knowledge graph
 *
 * WHAT: Retrieves structural self-knowledge about the Bio_Async_Orchestrator module
 * WHY:  Enables runtime introspection and self-awareness capabilities
 * HOW:  Queries KG for Bio_Async_Orchestrator entity and logs observations/relations
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge was found, 0 otherwise
 */
int bio_async_orchestrator_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Bio_Async_Orchestrator");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("Bio_Async_Orchestrator self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Bio_Async_Orchestrator");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Bio_Async_Orchestrator");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Wiring Diagram Integration - Handler Callback API
 * ============================================================================ */

/**
 * @brief Register handler callback for a module
 *
 * WHAT: Store callback that receives discovered message types
 * WHY:  Enable modules to auto-register handlers based on KG wiring
 * HOW:  Store in module entry, invoked by discover_all_wiring
 */
int bio_orchestrator_register_handler_callback(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id,
    wiring_handler_callback_t callback,
    void* user_data
) {
    if (!orchestrator || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_register_handler_callback: required parameter is NULL (orchestrator, callback)");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    bio_module_entry_t* entry = find_module(orchestrator, module_id);
    if (!entry) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        NIMCP_LOGGING_WARN("Cannot register callback: module 0x%04X not found", module_id);
        return -1;
    }

    entry->handler_callback = callback;
    entry->handler_callback_data = user_data;

    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Registered handler callback for module: %s", entry->module_name);
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return 0;
}

/**
 * @brief Get module's discovered message handlers
 *
 * WHAT: Query message types a module should handle
 * WHY:  Introspection of discovered wiring
 * HOW:  Return array from module's wiring config
 */
uint32_t bio_orchestrator_get_module_handlers(
    const bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id,
    bio_message_type_t* message_types,
    uint32_t max_types
) {
    if (!orchestrator || !message_types || max_types == 0) return 0;

    const bio_module_entry_t* entry = bio_orchestrator_get_module_info(orchestrator, module_id);
    if (!entry || !entry->wiring_discovered) return 0;

    uint32_t count = entry->wiring.handles_message_count;
    if (count > max_types) count = max_types;

    for (uint32_t i = 0; i < count; i++) {
        message_types[i] = entry->wiring.handles_messages[i];
    }

    return count;
}

/**
 * @brief Invoke handler callbacks for all modules with discovered wiring
 *
 * WHAT: Call registered callbacks with discovered message types
 * WHY:  Enable modules to auto-register handlers
 * HOW:  Iterate modules, call callbacks with wiring.handles_messages
 */
int bio_orchestrator_invoke_handler_callbacks(bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_invoke_handler_callbacks: orchestrator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    uint32_t invoked = 0;
    for (uint32_t i = 0; i < orchestrator->module_count; i++) {
        bio_module_entry_t* entry = &orchestrator->modules[i];

        /* Skip if no callback or no wiring discovered */
        if (!entry->handler_callback) continue;
        if (!entry->wiring_discovered) continue;
        if (entry->wiring.handles_message_count == 0) continue;

        /* Invoke callback */
        int result = entry->handler_callback(
            entry->bio_context,
            entry->wiring.handles_messages,
            entry->wiring.handles_message_count,
            entry->handler_callback_data
        );

        if (result == 0) {
            invoked++;
            if (orchestrator->config.enable_logging) {
                NIMCP_LOGGING_DEBUG("Invoked handler callback for %s (%u handlers)",
                    entry->module_name, entry->wiring.handles_message_count);
            }
        } else {
            NIMCP_LOGGING_WARN("Handler callback failed for module: %s", entry->module_name);
        }
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);

    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("Invoked %u module handler callbacks", invoked);
    }

    return (int)invoked;
}

/**
 * @brief Set the wiring diagram for this orchestrator
 *
 * WHAT: Associate a wiring diagram with the orchestrator
 * WHY:  Enable runtime wiring discovery from JSONL diagrams
 * HOW:  Store pointer (not owned by orchestrator)
 */
int bio_orchestrator_set_wiring_diagram(
    bio_async_orchestrator_t* orchestrator,
    wiring_diagram_t* wd
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_set_wiring_diagram: orchestrator is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);
    orchestrator->wiring_diagram = wd;
    nimcp_platform_mutex_unlock(orchestrator->mutex);

    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("Wiring diagram %s for orchestrator",
            wd ? "set" : "cleared");
    }

    return 0;
}

/**
 * @brief Get the wiring diagram from this orchestrator
 *
 * WHAT: Retrieve associated wiring diagram
 * WHY:  Allow modules to access wiring diagram
 * HOW:  Return stored pointer
 */
wiring_diagram_t* bio_orchestrator_get_wiring_diagram(
    const bio_async_orchestrator_t* orchestrator
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_get_wiring_diagram: orchestrator is NULL");
        return NULL;
    }
    return orchestrator->wiring_diagram;
}

/**
 * @brief Discover wiring for a single module from wiring diagram
 *
 * WHAT: Populate module's wiring config from wiring diagram
 * WHY:  Enable runtime wiring discovery
 * HOW:  Query wiring diagram by module name, copy to entry
 */
int bio_orchestrator_discover_module_wiring(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id
) {
    if (!orchestrator || !orchestrator->wiring_diagram) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_discover_module_wiring: required parameter is NULL (orchestrator, orchestrator->wiring_diagram)");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    bio_module_entry_t* entry = find_module(orchestrator, module_id);
    if (!entry) {
        nimcp_platform_mutex_unlock(orchestrator->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_discover_module_wiring: entry is NULL");
        return -1;
    }

    /* Query wiring diagram for this module */
    wiring_module_config_t config;
    wiring_module_config_init(&config);

    int result = wiring_diagram_get_module_config(
        orchestrator->wiring_diagram, entry->module_name, &config);
    if (result == 0) {
        /* Copy discovered wiring (shallow copy for now) */
        entry->wiring = config;
        entry->wiring_discovered = true;

        if (orchestrator->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Discovered wiring for %s: %u handlers",
                entry->module_name, config.handles_message_count);
        }
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);
    return result;
}

/**
 * @brief Discover wiring for all registered modules
 *
 * WHAT: Populate wiring configs for all modules from wiring diagram
 * WHY:  Bulk wiring discovery at startup
 * HOW:  Iterate all modules, query wiring diagram for each
 */
int bio_orchestrator_discover_all_wiring(bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator || !orchestrator->wiring_diagram) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_discover_all_wiring: required parameter is NULL (orchestrator, orchestrator->wiring_diagram)");
        return -1;
    }

    nimcp_platform_mutex_lock(orchestrator->mutex);

    uint32_t discovered = 0;
    for (uint32_t i = 0; i < orchestrator->module_count; i++) {
        bio_module_entry_t* entry = &orchestrator->modules[i];

        wiring_module_config_t config;
        wiring_module_config_init(&config);

        int result = wiring_diagram_get_module_config(
            orchestrator->wiring_diagram, entry->module_name, &config);
        if (result == 0) {
            entry->wiring = config;
            entry->wiring_discovered = true;
            discovered++;
        }
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);

    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("Discovered wiring for %u/%u modules",
            discovered, orchestrator->module_count);
    }

    return (int)discovered;
}

/* ============================================================================
 * Phase 10: Automatic Self-Assembly Implementation
 * ============================================================================ */

/**
 * WHAT: Check if self-assembly is available
 * WHY:  Allow callers to verify before using self-assembly APIs
 * HOW:  Check wiring_diagram is set and has modules
 */
bool bio_orchestrator_self_assembly_available(
    const bio_async_orchestrator_t* orchestrator
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_self_assembly_available: orchestrator is NULL");
        return false;
    }
    if (!orchestrator->wiring_diagram) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_self_assembly_available: orchestrator->wiring_diagram is NULL");
        return false;
    }

    /* Check if wiring diagram has any modules */
    uint32_t count = wiring_diagram_get_module_count(orchestrator->wiring_diagram);
    return count > 0;
}

/**
 * WHAT: Compute startup order using KG-driven topological sort
 * WHY:  Replace hardcoded phases with automatic dependency resolution
 * HOW:  Delegates to wiring_diagram_get_startup_order() (Kahn's algorithm)
 */
int bio_orchestrator_compute_startup_order(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t* order_out,
    uint32_t max_modules
) {
    if (!orchestrator || !order_out || max_modules == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_compute_startup_order: required parameter is NULL (orchestrator, order_out)");
        return -1;
    }

    /* If wiring diagram available, use KG-driven ordering */
    if (orchestrator->wiring_diagram) {
        int count = wiring_diagram_get_startup_order(
            orchestrator->wiring_diagram,
            order_out,
            max_modules
        );

        if (count > 0) {
            if (orchestrator->config.enable_logging) {
                NIMCP_LOGGING_DEBUG("Computed KG-driven startup order: %d modules", count);
            }
            return count;
        }

        /* Fall through to phase-based ordering on failure */
        if (orchestrator->config.enable_logging) {
            NIMCP_LOGGING_WARN("KG startup order failed, falling back to phases");
        }
    }

    /* Fallback: phase-based ordering */
    nimcp_platform_mutex_lock(orchestrator->mutex);

    uint32_t count = 0;
    for (uint32_t phase = 0; phase < BIO_STARTUP_PHASE_COUNT && count < max_modules; phase++) {
        for (uint32_t i = 0; i < orchestrator->module_count && count < max_modules; i++) {
            if (orchestrator->modules[i].startup_phase == phase &&
                orchestrator->modules[i].enabled) {
                order_out[count++] = orchestrator->modules[i].module_id;
            }
        }
    }

    nimcp_platform_mutex_unlock(orchestrator->mutex);

    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Computed phase-based startup order: %u modules", count);
    }

    return (int)count;
}

/**
 * WHAT: Start modules in KG-computed dependency order
 * WHY:  Enable true self-assembly without hardcoded phases
 * HOW:  Compute order, discover wiring, invoke callbacks, start each
 */
int bio_orchestrator_start_modules_ordered(bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_start_modules_ordered: orchestrator is NULL");
        return -1;
    }

    /* First, discover wiring if available */
    if (orchestrator->wiring_diagram) {
        int discovered = bio_orchestrator_discover_all_wiring(orchestrator);
        if (discovered > 0 && orchestrator->config.enable_logging) {
            NIMCP_LOGGING_INFO("Self-assembly: discovered wiring for %d modules", discovered);
        }
    }

    /* Compute startup order */
    bio_module_id_t order[BIO_ORCHESTRATOR_MAX_MODULES];
    int order_count = bio_orchestrator_compute_startup_order(
        orchestrator, order, BIO_ORCHESTRATOR_MAX_MODULES);

    if (order_count < 0) {
        NIMCP_LOGGING_ERROR("Self-assembly: failed to compute startup order");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bio_orchestrator_start_modules_ordered: validation failed");
        return -1;
    }

    if (order_count == 0) {
        if (orchestrator->config.enable_logging) {
            NIMCP_LOGGING_INFO("Self-assembly: no modules to start");
        }
        return 0;
    }

    /* Invoke handler callbacks before starting (to register message handlers) */
    if (orchestrator->wiring_diagram) {
        int invoked = bio_orchestrator_invoke_handler_callbacks(orchestrator);
        if (invoked > 0 && orchestrator->config.enable_logging) {
            NIMCP_LOGGING_INFO("Self-assembly: invoked %d handler callbacks", invoked);
        }
    }

    /* Start modules in computed order */
    nimcp_platform_mutex_lock(orchestrator->mutex);

    uint32_t started = 0;
    for (int i = 0; i < order_count; i++) {
        bio_module_entry_t* entry = find_module(orchestrator, order[i]);
        if (!entry) continue;
        if (!entry->enabled) continue;

        /* Mark as healthy on startup (actual health check can happen later) */
        entry->health_status = BIO_MODULE_HEALTH_HEALTHY;
        started++;

        if (orchestrator->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Self-assembly: started module [%u/%d] %s (0x%04X)",
                started, order_count, entry->module_name, entry->module_id);
        }
    }

    orchestrator->state = BIO_ORCHESTRATOR_RUNNING;
    orchestrator->stats.active_modules = started;

    nimcp_platform_mutex_unlock(orchestrator->mutex);

    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("Self-assembly: started %u modules in dependency order", started);
    }

    return (int)started;
}

/**
 * WHAT: Stop modules in reverse dependency order
 * WHY:  Ensure dependents stop before their dependencies
 * HOW:  Compute startup order, traverse in reverse
 */
int bio_orchestrator_stop_modules_ordered(bio_async_orchestrator_t* orchestrator) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_stop_modules_ordered: orchestrator is NULL");
        return -1;
    }

    /* Compute startup order */
    bio_module_id_t order[BIO_ORCHESTRATOR_MAX_MODULES];
    int order_count = bio_orchestrator_compute_startup_order(
        orchestrator, order, BIO_ORCHESTRATOR_MAX_MODULES);

    if (order_count < 0) {
        NIMCP_LOGGING_ERROR("Self-assembly: failed to compute shutdown order");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bio_orchestrator_stop_modules_ordered: validation failed");
        return -1;
    }

    if (order_count == 0) {
        if (orchestrator->config.enable_logging) {
            NIMCP_LOGGING_INFO("Self-assembly: no modules to stop");
        }
        return 0;
    }

    /* Stop modules in REVERSE order (dependents first) */
    nimcp_platform_mutex_lock(orchestrator->mutex);

    uint32_t stopped = 0;
    for (int i = order_count - 1; i >= 0; i--) {
        bio_module_entry_t* entry = find_module(orchestrator, order[i]);
        if (!entry) continue;

        /* Mark health as unknown after stop */
        entry->health_status = BIO_MODULE_HEALTH_UNKNOWN;
        stopped++;

        if (orchestrator->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Self-assembly: stopped module [%u/%d] %s (0x%04X)",
                stopped, order_count, entry->module_name, entry->module_id);
        }
    }

    orchestrator->state = BIO_ORCHESTRATOR_STOPPED;
    orchestrator->stats.active_modules = 0;

    nimcp_platform_mutex_unlock(orchestrator->mutex);

    if (orchestrator->config.enable_logging) {
        NIMCP_LOGGING_INFO("Self-assembly: stopped %u modules in reverse order", stopped);
    }

    return (int)stopped;
}

/**
 * WHAT: Get module's computed startup position
 * WHY:  Introspection of self-assembly results
 * HOW:  Compute order, find module's position
 */
int bio_orchestrator_get_module_startup_position(
    bio_async_orchestrator_t* orchestrator,
    bio_module_id_t module_id
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_get_module_startup_position: orchestrator is NULL");
        return -1;
    }

    /* Compute startup order */
    bio_module_id_t order[BIO_ORCHESTRATOR_MAX_MODULES];
    int order_count = bio_orchestrator_compute_startup_order(
        orchestrator, order, BIO_ORCHESTRATOR_MAX_MODULES);

    if (order_count <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bio_orchestrator_get_module_startup_position: validation failed");
        return -1;
    }

    /* Find module's position */
    for (int i = 0; i < order_count; i++) {
        if (order[i] == module_id) {
            return i;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bio_orchestrator_get_module_startup_position: validation failed");
    return -1;  /* Not found */
}

/**
 * WHAT: Validate self-assembly configuration
 * WHY:  Prevent startup failures due to invalid configurations
 * HOW:  Delegates to wiring_diagram_validate()
 */
int bio_orchestrator_validate_self_assembly(
    bio_async_orchestrator_t* orchestrator,
    wiring_validation_result_t* result
) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_orchestrator_validate_self_assembly: orchestrator is NULL");
        return -1;
    }

    /* No wiring diagram means no self-assembly - not an error, just not available */
    if (!orchestrator->wiring_diagram) {
        if (result) {
            memset(result, 0, sizeof(wiring_validation_result_t));
            result->valid = true;  /* Trivially valid (no wiring to validate) */
        }
        return 0;
    }

    /* Use local result if none provided */
    wiring_validation_result_t local_result;
    wiring_validation_result_t* use_result = result ? result : &local_result;

    int validate_result = wiring_diagram_validate(orchestrator->wiring_diagram, use_result);

    if (orchestrator->config.enable_logging) {
        if (use_result->valid) {
            NIMCP_LOGGING_DEBUG("Self-assembly validation passed");
        } else {
            NIMCP_LOGGING_WARN("Self-assembly validation failed: %u errors",
                use_result->error_count);
            if (use_result->has_circular_deps) {
                NIMCP_LOGGING_WARN("  - Circular dependencies detected");
            }
            if (use_result->has_missing_deps) {
                NIMCP_LOGGING_WARN("  - Missing dependencies detected");
            }
        }
    }

    return validate_result;
}
