/**
 * @file nimcp_mesh_module_registry.c
 * @brief Type-Safe Module Registration API Implementation
 *
 * WHAT: Provides type-safe registration of real NIMCP modules into the mesh
 * WHY:  Replace dummy pointers with validated, type-checked module instances
 * HOW:  Magic validation, size checking, and category-based organization
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_module_registry.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

#define MODULE_REGISTRY_MAGIC 0x4D4F4452  /* "MODR" */
#define DEFAULT_MAX_MODULES 512

struct mesh_module_registry {
    uint32_t magic;
    mesh_module_registry_config_t config;

    /* Registered modules array */
    mesh_registered_module_t* modules;
    size_t module_count;
    size_t module_capacity;

    /* Statistics */
    mesh_module_registry_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Optional: link to mesh integration for adapter creation */
    mesh_integration_t* integration;
};

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

nimcp_error_t mesh_module_registry_default_config(
    mesh_module_registry_config_t* config
) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(*config));
    config->max_modules = DEFAULT_MAX_MODULES;
    config->require_magic_validation = true;
    config->require_size_validation = true;
    config->enable_duplicate_detection = true;
    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

mesh_module_registry_t* mesh_module_registry_create(
    const mesh_module_registry_config_t* config
) {
    mesh_module_registry_config_t default_config;
    if (!config) {
        mesh_module_registry_default_config(&default_config);
        config = &default_config;
    }

    mesh_module_registry_t* registry = nimcp_calloc(1, sizeof(*registry));
    if (!registry) {
        LOG_ERROR("Failed to allocate module registry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_module_registry_create: registry is NULL");
        return NULL;
    }

    registry->magic = MODULE_REGISTRY_MAGIC;
    registry->config = *config;
    registry->module_capacity = config->max_modules;

    /* Allocate modules array */
    registry->modules = nimcp_calloc(
        registry->module_capacity,
        sizeof(mesh_registered_module_t)
    );
    if (!registry->modules) {
        LOG_ERROR("Failed to allocate modules array");
        nimcp_free(registry);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_module_registry_create: registry->modules is NULL");
        return NULL;
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    registry->mutex = nimcp_mutex_create(&attr);
    if (!registry->mutex) {
        LOG_ERROR("Failed to create registry mutex");
        nimcp_free(registry->modules);
        nimcp_free(registry);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_module_registry_create: registry->mutex is NULL");
        return NULL;
    }

    LOG_DEBUG("Module registry created with capacity %zu", registry->module_capacity);
    return registry;
}

void mesh_module_registry_destroy(mesh_module_registry_t* registry) {
    if (!registry || registry->magic != MODULE_REGISTRY_MAGIC) return;

    nimcp_mutex_lock(registry->mutex);

    /* Cleanup adapters (modules are not owned by registry) */
    for (size_t i = 0; i < registry->module_count; i++) {
        if (registry->modules[i].adapter) {
            mesh_adapter_base_cleanup(registry->modules[i].adapter);
            nimcp_free(registry->modules[i].adapter);
            registry->modules[i].adapter = NULL;
        }
    }

    nimcp_free(registry->modules);
    registry->modules = NULL;

    nimcp_mutex_unlock(registry->mutex);
    nimcp_mutex_destroy(registry->mutex);

    registry->magic = 0;
    nimcp_free(registry);

    LOG_DEBUG("Module registry destroyed");
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static void update_category_count(
    mesh_module_registry_stats_t* stats,
    mesh_adapter_category_t category,
    int delta
) {
    switch (category) {
        case MESH_ADAPTER_CATEGORY_COGNITIVE:
            stats->cognitive_count += delta;
            break;
        case MESH_ADAPTER_CATEGORY_PERCEPTION:
            stats->perception_count += delta;
            break;
        case MESH_ADAPTER_CATEGORY_SUBCORTICAL:
            stats->subcortical_count += delta;
            break;
        case MESH_ADAPTER_CATEGORY_MOTOR:
            stats->motor_count += delta;
            break;
        case MESH_ADAPTER_CATEGORY_MEMORY:
            stats->memory_count += delta;
            break;
        case MESH_ADAPTER_CATEGORY_SECURITY:
            stats->security_count += delta;
            break;
        case MESH_ADAPTER_CATEGORY_SWARM:
            stats->swarm_count += delta;
            break;
        case MESH_ADAPTER_CATEGORY_GPU:
            stats->gpu_count += delta;
            break;
        case MESH_ADAPTER_CATEGORY_PLASTICITY:
            stats->plasticity_count += delta;
            break;
        case MESH_ADAPTER_CATEGORY_GLIAL:
            stats->glial_count += delta;
            break;
        case MESH_ADAPTER_CATEGORY_SYSTEM:
            stats->system_count += delta;
            break;
    }
}

static int find_module_by_name(
    const mesh_module_registry_t* registry,
    const char* name
) {
    if (!name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_module_by_name: name is NULL");
        return -1;
    }

    for (size_t i = 0; i < registry->module_count; i++) {
        if (registry->modules[i].descriptor.module_name &&
            strcmp(registry->modules[i].descriptor.module_name, name) == 0) {
            return (int)i;
        }
    }
    /* Not found is normal lookup result, not an error */
    return -1;
}

static int find_module_by_id(
    const mesh_module_registry_t* registry,
    mesh_participant_id_t id
) {
    for (size_t i = 0; i < registry->module_count; i++) {
        if (registry->modules[i].participant_id == id) {
            return (int)i;
        }
    }
    /* Not found is normal lookup result, not an error */
    return -1;
}

/* ============================================================================
 * Registration API
 * ============================================================================ */

nimcp_error_t mesh_module_registry_register(
    mesh_module_registry_t* registry,
    const mesh_module_descriptor_t* descriptor
) {
    if (!registry || registry->magic != MODULE_REGISTRY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_module_registry: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!descriptor || !descriptor->module_name || !descriptor->module_instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_module_registry: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(registry->mutex);

    /* Check capacity */
    if (registry->module_count >= registry->module_capacity) {
        nimcp_mutex_unlock(registry->mutex);
        LOG_ERROR("Module registry full (capacity=%zu)", registry->module_capacity);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_module_registry: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    /* Check for duplicates */
    if (registry->config.enable_duplicate_detection) {
        int existing = find_module_by_name(registry, descriptor->module_name);
        if (existing >= 0) {
            registry->stats.duplicate_detections++;
            nimcp_mutex_unlock(registry->mutex);
            if (registry->config.verbose_logging) {
                LOG_WARN("Duplicate module name: %s", descriptor->module_name);
            }
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_ALREADY_EXISTS, "mesh_module_registry: error condition");
            return NIMCP_ERROR_ALREADY_EXISTS;
        }
    }

    /* Validate magic if required */
    if (registry->config.require_magic_validation && descriptor->module_magic != 0) {
        if (!mesh_module_validate_magic(descriptor->module_instance,
                                        descriptor->module_magic)) {
            registry->stats.magic_validation_failures++;
            nimcp_mutex_unlock(registry->mutex);
            LOG_ERROR("Magic validation failed for %s (expected 0x%08X)",
                     descriptor->module_name, descriptor->module_magic);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION, "mesh_module_registry: error condition");
            return NIMCP_ERROR_BBB_VALIDATION;
        }
    }

    /* Add to registry */
    mesh_registered_module_t* entry = &registry->modules[registry->module_count];
    memset(entry, 0, sizeof(*entry));

    /* Copy descriptor */
    entry->descriptor = *descriptor;

    /* Create mesh adapter */
    entry->adapter = nimcp_calloc(1, sizeof(mesh_adapter_base_t));
    if (!entry->adapter) {
        nimcp_mutex_unlock(registry->mutex);
        LOG_ERROR("Failed to allocate adapter for %s", descriptor->module_name);
        registry->stats.registration_failures++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_MEMORY, "mesh_module_registry: error condition");
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    mesh_adapter_config_t adapter_config;
    mesh_adapter_config_init(&adapter_config, descriptor->module_name,
                             descriptor->category);
    adapter_config.endorser_role = descriptor->endorser_role;
    adapter_config.policies = descriptor->policies;
    adapter_config.policy_count = descriptor->policy_count;

    nimcp_error_t err = mesh_adapter_base_init(
        entry->adapter,
        descriptor->module_instance,
        &adapter_config,
        NULL  /* callbacks set per-module */
    );

    if (err != NIMCP_SUCCESS) {
        nimcp_free(entry->adapter);
        entry->adapter = NULL;
        registry->stats.registration_failures++;
        nimcp_mutex_unlock(registry->mutex);
        LOG_ERROR("Failed to init adapter for %s: %d", descriptor->module_name, err);
        return err;
    }

    entry->participant_id = entry->adapter->participant_id;
    entry->registered = true;
    entry->registration_time_ns = nimcp_time_now_ns();

    /* Update statistics */
    registry->module_count++;
    registry->stats.total_registered++;
    update_category_count(&registry->stats, descriptor->category, 1);

    if (registry->config.verbose_logging) {
        LOG_DEBUG("Registered module '%s' (ID=0x%llx) category=%d",
                 descriptor->module_name,
                 (unsigned long long)entry->participant_id,
                 descriptor->category);
    }

    nimcp_mutex_unlock(registry->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_module_registry_unregister(
    mesh_module_registry_t* registry,
    const char* module_name
) {
    if (!registry || registry->magic != MODULE_REGISTRY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_module_registry: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!module_name) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(registry->mutex);

    int idx = find_module_by_name(registry, module_name);
    if (idx < 0) {
        nimcp_mutex_unlock(registry->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_module_registry: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    mesh_registered_module_t* entry = &registry->modules[idx];

    /* Update category count */
    update_category_count(&registry->stats, entry->descriptor.category, -1);

    /* Cleanup adapter */
    if (entry->adapter) {
        mesh_adapter_base_cleanup(entry->adapter);
        nimcp_free(entry->adapter);
        entry->adapter = NULL;
    }

    /* Remove from array by shifting */
    for (size_t i = idx; i < registry->module_count - 1; i++) {
        registry->modules[i] = registry->modules[i + 1];
    }
    registry->module_count--;

    if (registry->config.verbose_logging) {
        LOG_DEBUG("Unregistered module '%s'", module_name);
    }

    nimcp_mutex_unlock(registry->mutex);
    return NIMCP_SUCCESS;
}

bool mesh_module_registry_contains(
    const mesh_module_registry_t* registry,
    const char* module_name
) {
    if (!registry || registry->magic != MODULE_REGISTRY_MAGIC || !module_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_module_registry_contains: required parameter is NULL (registry, module_name)");
        return false;
    }

    nimcp_mutex_lock(((mesh_module_registry_t*)registry)->mutex);
    int idx = find_module_by_name(registry, module_name);
    nimcp_mutex_unlock(((mesh_module_registry_t*)registry)->mutex);

    return idx >= 0;
}

/* ============================================================================
 * Lookup API
 * ============================================================================ */

const mesh_registered_module_t* mesh_module_registry_get(
    const mesh_module_registry_t* registry,
    const char* module_name
) {
    if (!registry || registry->magic != MODULE_REGISTRY_MAGIC || !module_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_module_registry_get: required parameter is NULL (registry, module_name)");
        return NULL;
    }

    nimcp_mutex_lock(((mesh_module_registry_t*)registry)->mutex);
    int idx = find_module_by_name(registry, module_name);
    const mesh_registered_module_t* result = (idx >= 0) ?
        &registry->modules[idx] : NULL;
    nimcp_mutex_unlock(((mesh_module_registry_t*)registry)->mutex);

    return result;
}

const mesh_registered_module_t* mesh_module_registry_get_by_id(
    const mesh_module_registry_t* registry,
    mesh_participant_id_t participant_id
) {
    if (!registry || registry->magic != MODULE_REGISTRY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_module_registry_get_by_id: registry is NULL");
        return NULL;
    }

    nimcp_mutex_lock(((mesh_module_registry_t*)registry)->mutex);
    int idx = find_module_by_id(registry, participant_id);
    const mesh_registered_module_t* result = (idx >= 0) ?
        &registry->modules[idx] : NULL;
    nimcp_mutex_unlock(((mesh_module_registry_t*)registry)->mutex);

    return result;
}

void* mesh_module_registry_get_instance(
    const mesh_module_registry_t* registry,
    const char* module_name
) {
    const mesh_registered_module_t* entry = mesh_module_registry_get(
        registry, module_name);
    return entry ? entry->descriptor.module_instance : NULL;
}

nimcp_error_t mesh_module_registry_get_by_category(
    const mesh_module_registry_t* registry,
    mesh_adapter_category_t category,
    const mesh_registered_module_t** modules,
    size_t max_modules,
    size_t* count_out
) {
    if (!registry || registry->magic != MODULE_REGISTRY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_module_registry: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!modules || !count_out) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(((mesh_module_registry_t*)registry)->mutex);

    size_t count = 0;
    for (size_t i = 0; i < registry->module_count && count < max_modules; i++) {
        if (registry->modules[i].descriptor.category == category) {
            modules[count++] = &registry->modules[i];
        }
    }
    *count_out = count;

    nimcp_mutex_unlock(((mesh_module_registry_t*)registry)->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Validation API
 * ============================================================================ */

bool mesh_module_validate_magic(
    const void* instance,
    uint32_t expected_magic
) {
    if (!instance || expected_magic == 0) {
        return true;  /* No validation if magic not provided */
    }

    /* Magic is assumed to be first field in struct */
    const uint32_t* magic_ptr = (const uint32_t*)instance;
    return *magic_ptr == expected_magic;
}

nimcp_error_t mesh_module_registry_validate_all(
    const mesh_module_registry_t* registry,
    size_t* invalid_count
) {
    if (!registry || registry->magic != MODULE_REGISTRY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_module_registry: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(((mesh_module_registry_t*)registry)->mutex);

    size_t invalid = 0;
    for (size_t i = 0; i < registry->module_count; i++) {
        const mesh_module_descriptor_t* desc = &registry->modules[i].descriptor;
        if (desc->module_magic != 0) {
            if (!mesh_module_validate_magic(desc->module_instance, desc->module_magic)) {
                invalid++;
                LOG_WARN("Module '%s' failed magic validation", desc->module_name);
            }
        }
    }

    nimcp_mutex_unlock(((mesh_module_registry_t*)registry)->mutex);

    if (invalid_count) *invalid_count = invalid;
    return (invalid == 0) ? NIMCP_SUCCESS : NIMCP_ERROR_BBB_VALIDATION;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

nimcp_error_t mesh_module_registry_get_stats(
    const mesh_module_registry_t* registry,
    mesh_module_registry_stats_t* stats
) {
    if (!registry || registry->magic != MODULE_REGISTRY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_module_registry: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(((mesh_module_registry_t*)registry)->mutex);
    *stats = registry->stats;
    nimcp_mutex_unlock(((mesh_module_registry_t*)registry)->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bootstrap Integration
 * ============================================================================ */

/* External declaration - bootstrap needs to expose registry getter */
/* This is implemented in nimcp_mesh_bootstrap.c */

/*
 * mesh_bootstrap_t needs to include a module_registry field:
 *
 * struct mesh_bootstrap {
 *     ...
 *     mesh_module_registry_t* module_registry;
 *     ...
 * };
 *
 * And expose:
 * mesh_module_registry_t* mesh_bootstrap_get_module_registry(mesh_bootstrap_t* bootstrap) {
 *     if (!bootstrap) return NULL;
 *     return bootstrap->module_registry;
 * }
 */
