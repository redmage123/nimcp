/**
 * @file nimcp_layer_registry.c
 * @brief Layer Registry Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/core/nimcp_layer_registry.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(layer_registry)

#define MAX_LAYERS NIMCP_MAX_LAYERS
#define MAX_MODULES_PER_LAYER NIMCP_MAX_MODULES_PER_LAYER
#define MAX_CONNECTIONS NIMCP_MAX_INTER_LAYER_CONNECTIONS

typedef struct {
    nimcp_module_info_t info;
    bool active;
} module_entry_t;

typedef struct {
    nimcp_layer_id_t layer_id;
    nimcp_layer_config_t config;
    module_entry_t modules[MAX_MODULES_PER_LAYER];
    uint32_t module_count;
    uint32_t next_module_id;
    bool registered;
} layer_entry_t;

typedef struct {
    nimcp_layer_connection_t connection;
    bool active;
} connection_entry_t;

struct nimcp_layer_registry_struct {
    nimcp_layer_registry_config_t config;
    layer_entry_t layers[MAX_LAYERS];
    connection_entry_t connections[MAX_CONNECTIONS];
    uint32_t connection_count;
    uint32_t layers_registered;
    uint32_t total_modules_registered;
};

nimcp_layer_registry_config_t nimcp_layer_registry_default_config(void) {
    nimcp_layer_registry_config_t config = {0};
    config.max_layers = MAX_LAYERS;
    config.max_modules_per_layer = MAX_MODULES_PER_LAYER;
    config.enable_logging = false;
    config.thread_safe = false;
    return config;
}

nimcp_layer_registry_t nimcp_layer_registry_create(const nimcp_layer_registry_config_t* config) {
    nimcp_layer_registry_t registry = (nimcp_layer_registry_t)nimcp_calloc(1, sizeof(struct nimcp_layer_registry_struct));
    NIMCP_API_CHECK_ALLOC(registry, "Failed to allocate layer registry");
    registry->config = config ? *config : nimcp_layer_registry_default_config();
    return registry;
}

void nimcp_layer_registry_destroy(nimcp_layer_registry_t registry) {
    if (registry) nimcp_free(registry);
}

nimcp_layer_error_t nimcp_layer_registry_reset(nimcp_layer_registry_t registry) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in reset");
    memset(registry->layers, 0, sizeof(registry->layers));
    memset(registry->connections, 0, sizeof(registry->connections));
    registry->connection_count = 0;
    registry->layers_registered = 0;
    registry->total_modules_registered = 0;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_registry_register_layer(nimcp_layer_registry_t registry, const nimcp_layer_config_t* config) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in register_layer");
    NIMCP_API_CHECK_NULL(config, NIMCP_LAYER_ERR_NULL_PTR, "Config is NULL in register_layer");
    NIMCP_API_CHECK(config->layer_id < NIMCP_LAYER_COUNT, NIMCP_LAYER_ERR_INVALID_LAYER, "Invalid layer_id in register_layer");

    layer_entry_t* entry = &registry->layers[config->layer_id];
    if (entry->registered) return NIMCP_LAYER_ERR_ALREADY_REGISTERED;

    entry->layer_id = config->layer_id;
    entry->config = *config;
    entry->module_count = 0;
    entry->next_module_id = 1;
    entry->registered = true;
    registry->layers_registered++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_registry_unregister_layer(nimcp_layer_registry_t registry, nimcp_layer_id_t layer_id) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in unregister_layer");
    NIMCP_API_CHECK(layer_id < NIMCP_LAYER_COUNT, NIMCP_LAYER_ERR_INVALID_LAYER, "Invalid layer_id in unregister_layer");

    layer_entry_t* entry = &registry->layers[layer_id];
    if (!entry->registered) return NIMCP_LAYER_ERR_NOT_REGISTERED;

    registry->total_modules_registered -= entry->module_count;
    entry->module_count = 0;
    entry->registered = false;
    if (registry->layers_registered > 0) registry->layers_registered--;
    return NIMCP_LAYER_OK;
}

bool nimcp_layer_registry_is_layer_registered(nimcp_layer_registry_t registry, nimcp_layer_id_t layer_id) {
    if (!registry || layer_id >= NIMCP_LAYER_COUNT) {
        return false;
    }
    return registry->layers[layer_id].registered;
}

nimcp_layer_error_t nimcp_layer_registry_get_layer_config(nimcp_layer_registry_t registry, nimcp_layer_id_t layer_id, nimcp_layer_config_t* config_out) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in get_layer_config");
    NIMCP_API_CHECK_NULL(config_out, NIMCP_LAYER_ERR_NULL_PTR, "config_out is NULL in get_layer_config");
    NIMCP_API_CHECK(layer_id < NIMCP_LAYER_COUNT, NIMCP_LAYER_ERR_INVALID_LAYER, "Invalid layer_id in get_layer_config");
    if (!registry->layers[layer_id].registered) return NIMCP_LAYER_ERR_NOT_REGISTERED;
    *config_out = registry->layers[layer_id].config;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_registry_get_layers(nimcp_layer_registry_t registry, nimcp_layer_id_t* layer_ids_out, size_t max_layers, size_t* count_out) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in get_layers");
    NIMCP_API_CHECK_NULL(layer_ids_out, NIMCP_LAYER_ERR_NULL_PTR, "layer_ids_out is NULL in get_layers");
    NIMCP_API_CHECK_NULL(count_out, NIMCP_LAYER_ERR_NULL_PTR, "count_out is NULL in get_layers");
    size_t count = 0;
    for (int i = 0; i < NIMCP_LAYER_COUNT && count < max_layers; i++) {
        if (registry->layers[i].registered) {
            layer_ids_out[count++] = (nimcp_layer_id_t)i;
        }
    }
    *count_out = count;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_registry_register_module(
    nimcp_layer_registry_t registry, nimcp_layer_id_t layer_id,
    void* module_ptr, nimcp_module_interface_t* interface, const char* name, uint32_t* module_id_out
) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in register_module");
    NIMCP_API_CHECK_NULL(module_ptr, NIMCP_LAYER_ERR_NULL_PTR, "module_ptr is NULL in register_module");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "interface is NULL in register_module");
    NIMCP_API_CHECK_NULL(name, NIMCP_LAYER_ERR_NULL_PTR, "name is NULL in register_module");
    NIMCP_API_CHECK_NULL(module_id_out, NIMCP_LAYER_ERR_NULL_PTR, "module_id_out is NULL in register_module");
    NIMCP_API_CHECK(layer_id < NIMCP_LAYER_COUNT, NIMCP_LAYER_ERR_INVALID_LAYER, "Invalid layer_id in register_module");

    layer_entry_t* layer = &registry->layers[layer_id];
    if (!layer->registered) return NIMCP_LAYER_ERR_NOT_REGISTERED;
    if (layer->module_count >= MAX_MODULES_PER_LAYER) return NIMCP_LAYER_ERR_CAPACITY;

    module_entry_t* entry = &layer->modules[layer->module_count];
    entry->info.module_id = layer->next_module_id++;
    strncpy(entry->info.name, name, NIMCP_MODULE_NAME_MAX - 1);
    entry->info.name[NIMCP_MODULE_NAME_MAX - 1] = '\0';
    entry->info.module_ptr = module_ptr;
    entry->info.interface = interface;
    entry->info.is_active = true;
    entry->info.last_update_ns = 0;
    entry->active = true;

    *module_id_out = entry->info.module_id;
    layer->module_count++;
    registry->total_modules_registered++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_registry_unregister_module(nimcp_layer_registry_t registry, nimcp_layer_id_t layer_id, uint32_t module_id) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in unregister_module");
    NIMCP_API_CHECK(layer_id < NIMCP_LAYER_COUNT, NIMCP_LAYER_ERR_INVALID_LAYER, "Invalid layer_id in unregister_module");

    layer_entry_t* layer = &registry->layers[layer_id];
    if (!layer->registered) return NIMCP_LAYER_ERR_NOT_REGISTERED;

    for (uint32_t i = 0; i < layer->module_count; i++) {
        if (layer->modules[i].info.module_id == module_id && layer->modules[i].active) {
            layer->modules[i].active = false;
            layer->modules[i].info.is_active = false;
            if (registry->total_modules_registered > 0) registry->total_modules_registered--;
            return NIMCP_LAYER_OK;
        }
    }
    return NIMCP_LAYER_ERR_INVALID_MODULE;
}

nimcp_layer_error_t nimcp_layer_registry_get_module(nimcp_layer_registry_t registry, nimcp_layer_id_t layer_id, uint32_t module_id, nimcp_module_info_t* info_out) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in get_module");
    NIMCP_API_CHECK_NULL(info_out, NIMCP_LAYER_ERR_NULL_PTR, "info_out is NULL in get_module");
    NIMCP_API_CHECK(layer_id < NIMCP_LAYER_COUNT, NIMCP_LAYER_ERR_INVALID_LAYER, "Invalid layer_id in get_module");

    layer_entry_t* layer = &registry->layers[layer_id];
    if (!layer->registered) return NIMCP_LAYER_ERR_NOT_REGISTERED;

    for (uint32_t i = 0; i < layer->module_count; i++) {
        if (layer->modules[i].info.module_id == module_id && layer->modules[i].active) {
            *info_out = layer->modules[i].info;
            return NIMCP_LAYER_OK;
        }
    }
    return NIMCP_LAYER_ERR_INVALID_MODULE;
}

nimcp_layer_error_t nimcp_layer_registry_get_modules(nimcp_layer_registry_t registry, nimcp_layer_id_t layer_id, nimcp_module_info_t* modules_out, size_t max_modules, size_t* count_out) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in get_modules");
    NIMCP_API_CHECK_NULL(modules_out, NIMCP_LAYER_ERR_NULL_PTR, "modules_out is NULL in get_modules");
    NIMCP_API_CHECK_NULL(count_out, NIMCP_LAYER_ERR_NULL_PTR, "count_out is NULL in get_modules");
    NIMCP_API_CHECK(layer_id < NIMCP_LAYER_COUNT, NIMCP_LAYER_ERR_INVALID_LAYER, "Invalid layer_id in get_modules");

    layer_entry_t* layer = &registry->layers[layer_id];
    if (!layer->registered) return NIMCP_LAYER_ERR_NOT_REGISTERED;

    size_t count = 0;
    for (uint32_t i = 0; i < layer->module_count && count < max_modules; i++) {
        if (layer->modules[i].active) {
            modules_out[count++] = layer->modules[i].info;
        }
    }
    *count_out = count;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_registry_find_module_by_name(nimcp_layer_registry_t registry, nimcp_layer_id_t layer_id, const char* name, nimcp_module_info_t* info_out) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in find_module_by_name");
    NIMCP_API_CHECK_NULL(name, NIMCP_LAYER_ERR_NULL_PTR, "name is NULL in find_module_by_name");
    NIMCP_API_CHECK_NULL(info_out, NIMCP_LAYER_ERR_NULL_PTR, "info_out is NULL in find_module_by_name");
    NIMCP_API_CHECK(layer_id < NIMCP_LAYER_COUNT, NIMCP_LAYER_ERR_INVALID_LAYER, "Invalid layer_id in find_module_by_name");

    layer_entry_t* layer = &registry->layers[layer_id];
    if (!layer->registered) return NIMCP_LAYER_ERR_NOT_REGISTERED;

    for (uint32_t i = 0; i < layer->module_count; i++) {
        if (layer->modules[i].active && strcmp(layer->modules[i].info.name, name) == 0) {
            *info_out = layer->modules[i].info;
            return NIMCP_LAYER_OK;
        }
    }
    return NIMCP_LAYER_ERR_INVALID_MODULE;
}

int nimcp_layer_registry_get_module_count(nimcp_layer_registry_t registry, nimcp_layer_id_t layer_id) {
    if (!registry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_layer_registry_get_module_count: registry is NULL");
        return -1;
    }
    if (layer_id >= NIMCP_LAYER_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_layer_registry_get_module_count: layer_id out of range");
        return -1;
    }
    if (!registry->layers[layer_id].registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_layer_registry_get_module_count: layer not registered");
        return -1;
    }
    int count = 0;
    layer_entry_t* layer = &registry->layers[layer_id];
    for (uint32_t i = 0; i < layer->module_count; i++) {
        if (layer->modules[i].active) count++;
    }
    return count;
}

nimcp_layer_error_t nimcp_layer_registry_register_connection(nimcp_layer_registry_t registry, const nimcp_layer_connection_t* connection) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in register_connection");
    NIMCP_API_CHECK_NULL(connection, NIMCP_LAYER_ERR_NULL_PTR, "connection is NULL in register_connection");
    NIMCP_API_CHECK(connection->layer_a < NIMCP_LAYER_COUNT && connection->layer_b < NIMCP_LAYER_COUNT,
                    NIMCP_LAYER_ERR_INVALID_LAYER, "Invalid layer IDs in register_connection");
    if (registry->connection_count >= MAX_CONNECTIONS) return NIMCP_LAYER_ERR_CAPACITY;

    connection_entry_t* entry = &registry->connections[registry->connection_count];
    entry->connection = *connection;
    entry->active = true;
    registry->connection_count++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_registry_unregister_connection(nimcp_layer_registry_t registry, nimcp_layer_id_t layer_a, nimcp_layer_id_t layer_b) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in unregister_connection");
    for (uint32_t i = 0; i < registry->connection_count; i++) {
        connection_entry_t* entry = &registry->connections[i];
        if (entry->active &&
            ((entry->connection.layer_a == layer_a && entry->connection.layer_b == layer_b) ||
             (entry->connection.layer_a == layer_b && entry->connection.layer_b == layer_a))) {
            entry->active = false;
            return NIMCP_LAYER_OK;
        }
    }
    return NIMCP_LAYER_ERR_NO_CONNECTION;
}

bool nimcp_layer_registry_are_connected(nimcp_layer_registry_t registry, nimcp_layer_id_t layer_a, nimcp_layer_id_t layer_b) {
    if (!registry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_layer_registry_are_connected: registry is NULL");
        return false;
    }
    for (uint32_t i = 0; i < registry->connection_count; i++) {
        connection_entry_t* entry = &registry->connections[i];
        if (entry->active &&
            ((entry->connection.layer_a == layer_a && entry->connection.layer_b == layer_b) ||
             (entry->connection.layer_a == layer_b && entry->connection.layer_b == layer_a))) {
            return true;
        }
    }
    return false;
}

nimcp_layer_error_t nimcp_layer_registry_get_connection(nimcp_layer_registry_t registry, nimcp_layer_id_t layer_a, nimcp_layer_id_t layer_b, nimcp_layer_connection_t* connection_out) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in get_connection");
    NIMCP_API_CHECK_NULL(connection_out, NIMCP_LAYER_ERR_NULL_PTR, "connection_out is NULL in get_connection");
    for (uint32_t i = 0; i < registry->connection_count; i++) {
        connection_entry_t* entry = &registry->connections[i];
        if (entry->active &&
            ((entry->connection.layer_a == layer_a && entry->connection.layer_b == layer_b) ||
             (entry->connection.layer_a == layer_b && entry->connection.layer_b == layer_a))) {
            *connection_out = entry->connection;
            return NIMCP_LAYER_OK;
        }
    }
    return NIMCP_LAYER_ERR_NO_CONNECTION;
}

nimcp_layer_error_t nimcp_layer_registry_get_layer_connections(nimcp_layer_registry_t registry, nimcp_layer_id_t layer_id, nimcp_layer_connection_t* connections_out, size_t max_connections, size_t* count_out) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in get_layer_connections");
    NIMCP_API_CHECK_NULL(connections_out, NIMCP_LAYER_ERR_NULL_PTR, "connections_out is NULL in get_layer_connections");
    NIMCP_API_CHECK_NULL(count_out, NIMCP_LAYER_ERR_NULL_PTR, "count_out is NULL in get_layer_connections");
    size_t count = 0;
    for (uint32_t i = 0; i < registry->connection_count && count < max_connections; i++) {
        connection_entry_t* entry = &registry->connections[i];
        if (entry->active && (entry->connection.layer_a == layer_id || entry->connection.layer_b == layer_id)) {
            connections_out[count++] = entry->connection;
        }
    }
    *count_out = count;
    return NIMCP_LAYER_OK;
}

int nimcp_layer_registry_get_layer_count(nimcp_layer_registry_t registry) {
    if (!registry) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "registry is NULL");

        return -1;

    }
    return (int)registry->layers_registered;
}

int nimcp_layer_registry_get_total_module_count(nimcp_layer_registry_t registry) {
    if (!registry) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "registry is NULL");

        return -1;

    }
    return (int)registry->total_modules_registered;
}

int nimcp_layer_registry_get_connection_count(nimcp_layer_registry_t registry) {
    if (!registry) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "registry is NULL");

        return -1;

    }
    int count = 0;
    for (uint32_t i = 0; i < registry->connection_count; i++) {
        if (registry->connections[i].active) count++;
    }
    return count;
}

nimcp_layer_error_t nimcp_layer_registry_foreach_layer(nimcp_layer_registry_t registry, nimcp_layer_iterator_cb callback, void* user_data) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in foreach_layer");
    NIMCP_API_CHECK_NULL(callback, NIMCP_LAYER_ERR_NULL_PTR, "callback is NULL in foreach_layer");
    for (int i = 0; i < NIMCP_LAYER_COUNT; i++) {
        if (registry->layers[i].registered) {
            if (!callback((nimcp_layer_id_t)i, &registry->layers[i].config, user_data)) {
                break;
            }
        }
    }
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_registry_foreach_module(nimcp_layer_registry_t registry, nimcp_layer_id_t layer_id, nimcp_module_iterator_cb callback, void* user_data) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in foreach_module");
    NIMCP_API_CHECK_NULL(callback, NIMCP_LAYER_ERR_NULL_PTR, "callback is NULL in foreach_module");
    NIMCP_API_CHECK(layer_id < NIMCP_LAYER_COUNT, NIMCP_LAYER_ERR_INVALID_LAYER, "Invalid layer_id in foreach_module");

    layer_entry_t* layer = &registry->layers[layer_id];
    if (!layer->registered) return NIMCP_LAYER_ERR_NOT_REGISTERED;

    for (uint32_t i = 0; i < layer->module_count; i++) {
        if (layer->modules[i].active) {
            if (!callback(layer_id, &layer->modules[i].info, user_data)) {
                break;
            }
        }
    }
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_layer_registry_foreach_all_modules(nimcp_layer_registry_t registry, nimcp_module_iterator_cb callback, void* user_data) {
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in foreach_all_modules");
    NIMCP_API_CHECK_NULL(callback, NIMCP_LAYER_ERR_NULL_PTR, "callback is NULL in foreach_all_modules");
    for (int i = 0; i < NIMCP_LAYER_COUNT; i++) {
        if (registry->layers[i].registered) {
            layer_entry_t* layer = &registry->layers[i];
            for (uint32_t j = 0; j < layer->module_count; j++) {
                if (layer->modules[j].active) {
                    if (!callback((nimcp_layer_id_t)i, &layer->modules[j].info, user_data)) {
                        return NIMCP_LAYER_OK;
                    }
                }
            }
        }
    }
    return NIMCP_LAYER_OK;
}
