//=============================================================================
// nimcp_schemas.c - Schema System Implementation
//=============================================================================
/**
 * @file nimcp_schemas.c
 * @brief Implementation of cognitive schemas for knowledge organization
 *
 * WHAT: Structured knowledge templates (schemas) for organizing memories
 * WHY:  Human cognition uses schemas to interpret events, fill gaps, and
 *       make predictions - enabling efficient knowledge organization
 * HOW:  Slot-based templates with prime signature integration, inheritance,
 *       and statistical learning from instances
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_schemas.h"
#include "cognitive/memory/nimcp_memory_kg_events.h"  /* W6: KG event emitters */
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(schemas, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal schema system structure
 */
struct schema_system_struct {
    // PR integration
    entangle_graph_t entanglement;
    pr_node_manager_t node_manager;

    // Schema library
    schema_t** schemas;
    size_t num_schemas;
    size_t schemas_capacity;

    // Schema ID -> index hash table (simple linear probe)
    uint64_t* id_table_keys;
    size_t* id_table_values;
    size_t id_table_capacity;

    // Active instantiations
    schema_instantiation_t** active;
    size_t num_active;
    size_t active_capacity;

    // Learning: slot cooccurrence matrix (flattened)
    // Indices: [slot_a * cooc_dim + slot_b] = count of co-occurrence
    float* slot_cooccurrence;
    size_t cooc_dim;

    // ID generation
    uint64_t next_schema_id;
    uint64_t next_instantiation_id;

    // Configuration
    schema_config_t config;

    // Statistics
    schema_stats_t stats;
};

//=============================================================================
// Static Variables
//=============================================================================

/** Thread-local error message buffer */
static _Thread_local char s_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Set the last error message
 */
static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(s_last_error, sizeof(s_last_error), format, args);
    va_end(args);
}

/**
 * @brief Clear the last error message
 */
static void clear_error(void) {
    s_last_error[0] = '\0';
}

/**
 * @brief Fast absolute value for floats
 */
static inline float fabsf_fast(float x) {
    return (x < 0.0f) ? -x : x;
}

/**
 * @brief Duplicate string (strdup may not be available everywhere)
 */
static char* str_dup(const char* s) {
    if (!s) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "s is NULL");

        return NULL;

    }
    size_t len = strlen(s) + 1;
    char* dup = (char*)nimcp_malloc(len);
    if (dup) {
        memcpy(dup, s, len);
    }
    return dup;
}

/**
 * @brief Simple hash for schema ID
 */
static size_t hash_id(uint64_t id, size_t capacity) {
    // FNV-1a inspired mixing
    uint64_t h = id * 0x9E3779B97F4A7C15ULL;
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    return (size_t)(h % capacity);
}

/**
 * @brief Insert into ID hash table
 */
static bool id_table_insert(schema_system_t system, uint64_t key, size_t value) {
    if (!system || system->id_table_capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "id_table_insert: system is NULL");
        return false;
    }

    size_t idx = hash_id(key, system->id_table_capacity);
    size_t start = idx;

    do {
        if (system->id_table_keys[idx] == 0 ||
            system->id_table_keys[idx] == UINT64_MAX) {
            // Empty slot
            system->id_table_keys[idx] = key;
            system->id_table_values[idx] = value;
            return true;
        }
        if (system->id_table_keys[idx] == key) {
            // Already exists, update
            system->id_table_values[idx] = value;
            return true;
        }
        idx = (idx + 1) % system->id_table_capacity;
    } while (idx != start);

    // Table full
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "id_table_insert: validation failed");
    return false;
}

/**
 * @brief Lookup in ID hash table
 */
static bool id_table_lookup(schema_system_t system, uint64_t key, size_t* value) {
    if (!system || system->id_table_capacity == 0 || key == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "id_table_lookup: system is NULL");
        return false;
    }

    size_t idx = hash_id(key, system->id_table_capacity);
    size_t start = idx;

    do {
        if (system->id_table_keys[idx] == key) {
            if (value) *value = system->id_table_values[idx];
            return true;
        }
        if (system->id_table_keys[idx] == 0) {
            return false;  // Not found
        }
        idx = (idx + 1) % system->id_table_capacity;
    } while (idx != start);

    return false;
}

/**
 * @brief Remove from ID hash table
 */
static bool id_table_remove(schema_system_t system, uint64_t key) {
    if (!system || system->id_table_capacity == 0 || key == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "id_table_remove: system is NULL");
        return false;
    }

    size_t idx = hash_id(key, system->id_table_capacity);
    size_t start = idx;

    do {
        if (system->id_table_keys[idx] == key) {
            system->id_table_keys[idx] = UINT64_MAX;  // Tombstone
            return true;
        }
        if (system->id_table_keys[idx] == 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "id_table_remove: validation failed");
            return false;
        }
        idx = (idx + 1) % system->id_table_capacity;
    } while (idx != start);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "id_table_remove: validation failed");
    return false;
}

/**
 * @brief Free slot resources
 */
static void free_slot(schema_slot_t* slot) {
    if (!slot) return;
    if (slot->slot_name) {
        nimcp_free(slot->slot_name);
        slot->slot_name = NULL;
    }
}

/**
 * @brief Free schema resources (internal only, does not free struct itself)
 */
static void free_schema_contents(schema_t* schema) {
    if (!schema) return;

    if (schema->schema_name) {
        nimcp_free(schema->schema_name);
        schema->schema_name = NULL;
    }

    if (schema->slots) {
        for (size_t i = 0; i < schema->num_slots; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && schema->num_slots > 256) {
                schemas_heartbeat("schemas_loop",
                                 (float)(i + 1) / (float)schema->num_slots);
            }

            free_slot(&schema->slots[i]);
        }
        nimcp_free(schema->slots);
        schema->slots = NULL;
    }

    if (schema->child_schemas) {
        nimcp_free(schema->child_schemas);
        schema->child_schemas = NULL;
    }

    schema->num_slots = 0;
    schema->num_children = 0;
}

/**
 * @brief Copy slot
 */
static bool copy_slot(schema_slot_t* dest, const schema_slot_t* src) {
    if (!dest || !src) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "copy_slot: required parameter is NULL (dest, src)");
        return false;
    }

    memcpy(dest, src, sizeof(schema_slot_t));

    // Deep copy name
    dest->slot_name = NULL;
    if (src->slot_name) {
        dest->slot_name = str_dup(src->slot_name);
        if (!dest->slot_name) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "copy_slot: dest->slot_name is NULL");
            return false;
        }
    }

    return true;
}

/**
 * @brief Find slot index by name
 */
static int find_slot_index(const schema_t* schema, const char* slot_name) {
    if (!schema || !slot_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_slot_index: required parameter is NULL (schema, slot_name)");
        return -1;
    }

    for (size_t i = 0; i < schema->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && schema->num_slots > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)schema->num_slots);
        }

        if (schema->slots[i].slot_name &&
            strcmp(schema->slots[i].slot_name, slot_name) == 0) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_slot_index: operation failed");
    return -1;
}

/**
 * @brief Update cooccurrence matrix
 */
static void update_cooccurrence(
    schema_system_t system,
    const schema_instantiation_t* instantiation)
{
    if (!system || !instantiation || !system->config.enable_cooccurrence) return;
    if (!system->slot_cooccurrence || system->cooc_dim == 0) return;

    // For each pair of filled slots, increment cooccurrence
    for (size_t i = 0; i < instantiation->num_filled; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && instantiation->num_filled > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)instantiation->num_filled);
        }

        for (size_t j = i + 1; j < instantiation->num_filled; j++) {
            // Use simple hash of slot names as indices
            // In production, would use proper slot indexing
            size_t idx_i = ((size_t)instantiation->filled_slots[i].slot_name) % system->cooc_dim;
            size_t idx_j = ((size_t)instantiation->filled_slots[j].slot_name) % system->cooc_dim;

            size_t cooc_idx = idx_i * system->cooc_dim + idx_j;
            if (cooc_idx < system->cooc_dim * system->cooc_dim) {
                system->slot_cooccurrence[cooc_idx] += 1.0f;
            }

            // Symmetric
            cooc_idx = idx_j * system->cooc_dim + idx_i;
            if (cooc_idx < system->cooc_dim * system->cooc_dim) {
                system->slot_cooccurrence[cooc_idx] += 1.0f;
            }
        }
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

schema_config_t schema_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_config_defaul", 0.0f);


    schema_config_t config = {
        .min_fit_threshold = SCHEMA_DEFAULT_MIN_FIT,
        .inferred_confidence = SCHEMA_DEFAULT_INFERRED_CONFIDENCE,
        .abstraction_threshold = SCHEMA_DEFAULT_ABSTRACTION_THRESHOLD,
        .enable_learning = true,
        .enable_cooccurrence = true,
        .enable_hierarchy = true,
        .max_schemas = SCHEMA_MAX_SCHEMAS,
        .max_active = SCHEMA_MAX_ACTIVE_INSTANTIATIONS
    };
    return config;
}

bool schema_config_validate(const schema_config_t* config) {
    if (!config) {
        set_error("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_config_validate: config is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_config_valida", 0.0f);


    if (config->min_fit_threshold < 0.0f || config->min_fit_threshold > 1.0f) {
        set_error("min_fit_threshold must be in [0, 1]");
        return false;
    }

    if (config->inferred_confidence < 0.0f || config->inferred_confidence > 1.0f) {
        set_error("inferred_confidence must be in [0, 1]");
        return false;
    }

    if (config->abstraction_threshold < 0.0f || config->abstraction_threshold > 1.0f) {
        set_error("abstraction_threshold must be in [0, 1]");
        return false;
    }

    if (config->max_schemas == 0 || config->max_schemas > 1000000) {
        set_error("max_schemas out of range");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "schema_config_validate: config->max_schemas is zero");
        return false;
    }

    if (config->max_active == 0 || config->max_active > 100000) {
        set_error("max_active out of range");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "schema_config_validate: config->max_active is zero");
        return false;
    }

    clear_error();
    return true;
}

//=============================================================================
// Schema System Lifecycle
//=============================================================================

schema_system_t schema_system_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const schema_config_t* config)
{
    // Use default config if not provided
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_system_create", 0.0f);


    schema_config_t cfg = config ? *config : schema_config_default();

    if (!schema_config_validate(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_system_create: schema_config_validate is NULL");
        return NULL;  // Error already set
    }

    // Allocate system
    schema_system_t system = (schema_system_t)nimcp_calloc(1, sizeof(struct schema_system_struct));
    if (!system) {
        set_error("Failed to allocate schema system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_system_create: system is NULL");
        return NULL;
    }

    // Store integration points
    system->entanglement = entanglement;
    system->node_manager = node_manager;
    system->config = cfg;

    // Allocate schema array
    system->schemas_capacity = 64;  // Start small, grow as needed
    system->schemas = (schema_t**)nimcp_calloc(system->schemas_capacity, sizeof(schema_t*));
    if (!system->schemas) {
        set_error("Failed to allocate schema array");
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_system_create: system->schemas is NULL");
        return NULL;
    }

    // Allocate ID hash table (2x capacity for load factor)
    system->id_table_capacity = system->schemas_capacity * 2;
    system->id_table_keys = (uint64_t*)nimcp_calloc(system->id_table_capacity, sizeof(uint64_t));
    system->id_table_values = (size_t*)nimcp_calloc(system->id_table_capacity, sizeof(size_t));
    if (!system->id_table_keys || !system->id_table_values) {
        set_error("Failed to allocate ID table");
        nimcp_free(system->schemas);
        nimcp_free(system->id_table_keys);
        nimcp_free(system->id_table_values);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_system_create: required parameter is NULL (system->id_table_keys, system->id_table_values)");
        return NULL;
    }

    // Allocate active instantiations array
    system->active_capacity = 32;
    system->active = (schema_instantiation_t**)nimcp_calloc(
        system->active_capacity, sizeof(schema_instantiation_t*));
    if (!system->active) {
        set_error("Failed to allocate active array");
        nimcp_free(system->schemas);
        nimcp_free(system->id_table_keys);
        nimcp_free(system->id_table_values);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_system_create: system->active is NULL");
        return NULL;
    }

    // Allocate cooccurrence matrix if enabled
    if (cfg.enable_cooccurrence) {
        system->cooc_dim = 128;  // Fixed size for simplicity
        size_t cooc_size = system->cooc_dim * system->cooc_dim;
        system->slot_cooccurrence = (float*)nimcp_calloc(cooc_size, sizeof(float));
        if (!system->slot_cooccurrence) return NULL;
        // OK if this fails, just disables cooccurrence
    }

    // Initialize ID counters
    system->next_schema_id = 1;
    system->next_instantiation_id = 1;

    clear_error();
    return system;
}

void schema_system_destroy(schema_system_t system) {
    if (!system) return;

    // Destroy all schemas
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_system_destro", 0.0f);


    for (size_t i = 0; i < system->num_schemas; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_schemas > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)system->num_schemas);
        }

        if (system->schemas[i]) {
            free_schema_contents(system->schemas[i]);
            nimcp_free(system->schemas[i]);
        }
    }
    nimcp_free(system->schemas);

    // Destroy all instantiations
    for (size_t i = 0; i < system->num_active; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_active > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)system->num_active);
        }

        if (system->active[i]) {
            schema_instantiation_destroy(system->active[i]);
        }
    }
    nimcp_free(system->active);

    // Free hash table
    nimcp_free(system->id_table_keys);
    nimcp_free(system->id_table_values);

    // Free cooccurrence matrix
    nimcp_free(system->slot_cooccurrence);

    // Free system
    nimcp_free(system);
    system = NULL;
}

bool schema_system_clear(schema_system_t system) {
    if (!system) {
        set_error("NULL system pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_system_clear: system is NULL");
        return false;
    }

    // Destroy all schemas
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_system_clear", 0.0f);


    for (size_t i = 0; i < system->num_schemas; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_schemas > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)system->num_schemas);
        }

        if (system->schemas[i]) {
            free_schema_contents(system->schemas[i]);
            nimcp_free(system->schemas[i]);
            system->schemas[i] = NULL;
        }
    }
    system->num_schemas = 0;

    // Clear hash table
    memset(system->id_table_keys, 0, system->id_table_capacity * sizeof(uint64_t));
    memset(system->id_table_values, 0, system->id_table_capacity * sizeof(size_t));

    // Destroy all instantiations
    for (size_t i = 0; i < system->num_active; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_active > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)system->num_active);
        }

        if (system->active[i]) {
            schema_instantiation_destroy(system->active[i]);
            system->active[i] = NULL;
        }
    }
    system->num_active = 0;

    // Clear cooccurrence
    if (system->slot_cooccurrence && system->cooc_dim > 0) {
        memset(system->slot_cooccurrence, 0,
               system->cooc_dim * system->cooc_dim * sizeof(float));
    }

    // Reset stats
    memset(&system->stats, 0, sizeof(schema_stats_t));

    clear_error();
    return true;
}

//=============================================================================
// Schema Creation and Management
//=============================================================================

schema_t* schema_create(
    schema_system_t system,
    const char* name,
    schema_type_t type)
{
    if (!system) {
        set_error("NULL system pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_create: system is NULL");
        return NULL;
    }

    if (!name || strlen(name) == 0) {
        set_error("Schema name required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "schema_create: name is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_create", 0.0f);


    if (type < 0 || type >= SCHEMA_TYPE_COUNT) {
        set_error("Invalid schema type");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_create: capacity exceeded");
        return NULL;
    }

    // Allocate schema
    schema_t* schema = (schema_t*)nimcp_calloc(1, sizeof(schema_t));
    if (!schema) {
        set_error("Failed to allocate schema");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_create: schema is NULL");
        return NULL;
    }

    // Initialize
    schema->schema_id = system->next_schema_id++;
    schema->schema_name = str_dup(name);
    if (!schema->schema_name) {
        set_error("Failed to duplicate schema name");
        nimcp_free(schema);
        schema = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_create: schema->schema_name is NULL");
        return NULL;
    }

    schema->type = type;
    schema->parent_schema_id = SCHEMA_INVALID_ID;
    schema->abstraction_level = 0.0f;  // Start specific

    // Allocate initial slot array
    schema->slots_capacity = 8;
    schema->slots = (schema_slot_t*)nimcp_calloc(schema->slots_capacity, sizeof(schema_slot_t));
    if (!schema->slots) {
        set_error("Failed to allocate slots");
        nimcp_free(schema->schema_name);
        nimcp_free(schema);
        schema = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_create: schema->slots is NULL");
        return NULL;
    }

    // Allocate initial children array
    schema->children_capacity = 4;
    schema->child_schemas = (uint64_t*)nimcp_calloc(schema->children_capacity, sizeof(uint64_t));
    if (!schema->child_schemas) {
        set_error("Failed to allocate children");
        nimcp_free(schema->slots);
        nimcp_free(schema->schema_name);
        nimcp_free(schema);
        schema = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_create: schema->child_schemas is NULL");
        return NULL;
    }

    // Timestamps
    schema->created_time_ms = schema_current_time_ms();
    schema->last_activated_ms = schema->created_time_ms;

    clear_error();
    return schema;
}

bool schema_destroy(schema_system_t system, schema_t* schema) {
    if (!system || !schema) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_destroy: required parameter is NULL (system, schema)");
        return false;
    }

    // Find and remove from system
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_destroy", 0.0f);


    size_t idx = 0;
    if (!id_table_lookup(system, schema->schema_id, &idx)) {
        set_error("Schema not in system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "schema_destroy: id_table_lookup is NULL");
        return false;
    }

    // Update parent's children list if this has a parent
    if (schema->parent_schema_id != SCHEMA_INVALID_ID) {
        schema_t* parent = schema_find_by_id(system, schema->parent_schema_id);
        if (parent) {
            for (size_t i = 0; i < parent->num_children; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && parent->num_children > 256) {
                    schemas_heartbeat("schemas_loop",
                                     (float)(i + 1) / (float)parent->num_children);
                }

                if (parent->child_schemas[i] == schema->schema_id) {
                    // Remove by shifting
                    memmove(&parent->child_schemas[i],
                            &parent->child_schemas[i + 1],
                            (parent->num_children - i - 1) * sizeof(uint64_t));
                    parent->num_children--;
                    break;
                }
            }
        }
    }

    // Remove instantiations of this schema
    for (size_t i = 0; i < system->num_active; ) {
        if (system->active[i] && system->active[i]->schema == schema) {
            schema_instantiation_destroy(system->active[i]);
            memmove(&system->active[i], &system->active[i + 1],
                    (system->num_active - i - 1) * sizeof(schema_instantiation_t*));
            system->num_active--;
        } else {
            i++;
        }
    }

    // Remove from hash table
    id_table_remove(system, schema->schema_id);

    // Remove from array
    memmove(&system->schemas[idx], &system->schemas[idx + 1],
            (system->num_schemas - idx - 1) * sizeof(schema_t*));
    system->num_schemas--;

    // Update hash table indices for shifted schemas
    for (size_t i = idx; i < system->num_schemas; i++) {
        if (system->schemas[i]) {
            id_table_insert(system, system->schemas[i]->schema_id, i);
        }
    }

    // Free schema
    free_schema_contents(schema);
    nimcp_free(schema);
    schema = NULL;

    // Update stats
    system->stats.num_schemas = system->num_schemas;

    clear_error();
    return true;
}

bool schema_add(schema_system_t system, schema_t* schema) {
    if (!system || !schema) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_add: required parameter is NULL (system, schema)");
        return false;
    }

    // Check capacity
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_add", 0.0f);


    if (system->num_schemas >= system->schemas_capacity) {
        // Grow array
        size_t new_capacity = system->schemas_capacity * 2;
        if (new_capacity > system->config.max_schemas) {
            new_capacity = system->config.max_schemas;
        }
        if (new_capacity <= system->num_schemas) {
            set_error("Schema library full");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "schema_add: validation failed");
            return false;
        }

        schema_t** new_schemas = (schema_t**)nimcp_realloc(
            system->schemas, new_capacity * sizeof(schema_t*));
        if (!new_schemas) {
            set_error("Failed to grow schema array");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_add: new_schemas is NULL");
            return false;
        }
        system->schemas = new_schemas;
        system->schemas_capacity = new_capacity;

        // Grow hash table too
        size_t new_table_cap = new_capacity * 2;
        uint64_t* new_keys = (uint64_t*)nimcp_calloc(new_table_cap, sizeof(uint64_t));
        size_t* new_values = (size_t*)nimcp_calloc(new_table_cap, sizeof(size_t));
        if (!new_keys || !new_values) {
            nimcp_free(new_keys);
            new_keys = NULL;
            nimcp_free(new_values);
            new_values = NULL;
            set_error("Failed to grow hash table");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_add: required parameter is NULL (new_keys, new_values)");
            return false;
        }

        // Rehash
        uint64_t* old_keys = system->id_table_keys;
        size_t* old_values = system->id_table_values;
        size_t old_cap = system->id_table_capacity;

        system->id_table_keys = new_keys;
        system->id_table_values = new_values;
        system->id_table_capacity = new_table_cap;

        for (size_t i = 0; i < old_cap; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && old_cap > 256) {
                schemas_heartbeat("schemas_loop",
                                 (float)(i + 1) / (float)old_cap);
            }

            if (old_keys[i] != 0 && old_keys[i] != UINT64_MAX) {
                id_table_insert(system, old_keys[i], old_values[i]);
            }
        }

        nimcp_free(old_keys);
        old_keys = NULL;
        nimcp_free(old_values);
        old_values = NULL;
    }

    // Add schema
    size_t idx = system->num_schemas;
    system->schemas[idx] = schema;
    system->num_schemas++;

    // Add to hash table
    if (!id_table_insert(system, schema->schema_id, idx)) {
        system->num_schemas--;
        set_error("Failed to insert into hash table");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "schema_add: id_table_insert is NULL");
        return false;
    }

    // Update stats
    system->stats.num_schemas = system->num_schemas;
    if (schema->type < SCHEMA_TYPE_COUNT) {
        system->stats.schemas_by_type[schema->type]++;
    }

    clear_error();

    /* W6: emit KG event for schema addition */
    memory_kg_emit_schema_added(
        memory_kg_events_get_registered_brain(),
        schema->schema_id, schema->schema_name);

    return true;
}

schema_t* schema_find_by_id(schema_system_t system, uint64_t schema_id) {
    if (!system || schema_id == SCHEMA_INVALID_ID) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_find_by_id: system is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_find_by_id", 0.0f);


    size_t idx = 0;
    if (id_table_lookup(system, schema_id, &idx)) {
        if (idx < system->num_schemas) {
            return system->schemas[idx];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_find_by_id: validation failed");
    return NULL;
}

schema_t* schema_find_by_name(schema_system_t system, const char* name) {
    if (!system || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_find_by_name: required parameter is NULL (system, name)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_find_by_name", 0.0f);


    for (size_t i = 0; i < system->num_schemas; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_schemas > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)system->num_schemas);
        }

        if (system->schemas[i] && system->schemas[i]->schema_name &&
            strcmp(system->schemas[i]->schema_name, name) == 0) {
            return system->schemas[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_find_by_name: operation failed");
    return NULL;
}

bool schema_get_by_type(
    schema_system_t system,
    schema_type_t type,
    schema_t** schemas,
    size_t max_schemas,
    size_t* count)
{
    if (!system || !schemas || !count) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_get_by_type: required parameter is NULL (system, schemas, count)");
        return false;
    }

    *count = 0;
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_get_by_type", 0.0f);


    for (size_t i = 0; i < system->num_schemas && *count < max_schemas; i++) {
        if (system->schemas[i] && system->schemas[i]->type == type) {
            schemas[*count] = system->schemas[i];
            (*count)++;
        }
    }

    clear_error();
    return true;
}

//=============================================================================
// Slot Management
//=============================================================================

bool schema_define_slot(
    schema_t* schema,
    const char* slot_name,
    bool is_required,
    const prime_signature_t* default_sig)
{
    if (!schema || !slot_name || strlen(slot_name) == 0) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_define_slot: required parameter is NULL (schema, slot_name)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_define_slot", 0.0f);


    if (strlen(slot_name) >= SCHEMA_MAX_SLOT_NAME_LENGTH) {
        set_error("Slot name too long");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "schema_define_slot: capacity exceeded");
        return false;
    }

    // Check if slot already exists
    if (find_slot_index(schema, slot_name) >= 0) {
        set_error("Slot already exists: %s", slot_name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "schema_define_slot: capacity exceeded");
        return false;
    }

    // Check capacity
    if (schema->num_slots >= schema->slots_capacity) {
        size_t new_capacity = schema->slots_capacity * 2;
        if (new_capacity > SCHEMA_MAX_SLOTS) {
            new_capacity = SCHEMA_MAX_SLOTS;
        }
        if (new_capacity <= schema->num_slots) {
            set_error("Too many slots");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "schema_define_slot: validation failed");
            return false;
        }

        schema_slot_t* new_slots = (schema_slot_t*)nimcp_realloc(
            schema->slots, new_capacity * sizeof(schema_slot_t));
        if (!new_slots) {
            set_error("Failed to grow slots array");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_define_slot: new_slots is NULL");
            return false;
        }
        schema->slots = new_slots;
        schema->slots_capacity = new_capacity;
    }

    // Initialize new slot
    schema_slot_t* slot = &schema->slots[schema->num_slots];
    memset(slot, 0, sizeof(schema_slot_t));

    slot->slot_name = str_dup(slot_name);
    if (!slot->slot_name) {
        set_error("Failed to duplicate slot name");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_define_slot: slot->slot_name is NULL");
        return false;
    }

    slot->is_required = is_required;
    slot->is_filled = false;
    slot->confidence = 0.0f;
    slot->constraint_type = SLOT_CONSTRAINT_NONE;

    if (default_sig) {
        slot->default_value = *default_sig;
    }

    schema->num_slots++;

    // Update schema signature to reflect structure
    // (Simplified: just mark as modified)

    clear_error();
    return true;
}

bool schema_define_slot_constrained(
    schema_t* schema,
    const char* slot_name,
    bool is_required,
    const prime_signature_t* default_sig,
    slot_constraint_type_t constraint_type,
    const void* constraint_data)
{
    // First define the basic slot
    if (!schema_define_slot(schema, slot_name, is_required, default_sig)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "schema_define_slot_constrained: schema_define_slot is NULL");
        return false;
    }

    // Then add constraint
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_define_slot_c", 0.0f);


    schema_slot_t* slot = &schema->slots[schema->num_slots - 1];
    slot->constraint_type = constraint_type;

    if (constraint_data) {
        switch (constraint_type) {
            case SLOT_CONSTRAINT_RANGE: {
                const float* range = (const float*)constraint_data;
                slot->constraint_data.range.min_value = range[0];
                slot->constraint_data.range.max_value = range[1];
                break;
            }
            case SLOT_CONSTRAINT_SCHEMA: {
                const uint64_t* schema_id = (const uint64_t*)constraint_data;
                slot->constraint_data.schema_ref.schema_id = *schema_id;
                break;
            }
            case SLOT_CONSTRAINT_ENUM: {
                const uint32_t* count = (const uint32_t*)constraint_data;
                slot->constraint_data.enumeration.enum_count = *count;
                break;
            }
            default:
                break;
        }
    }

    return true;
}

bool schema_remove_slot(schema_t* schema, const char* slot_name) {
    if (!schema || !slot_name) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_remove_slot: required parameter is NULL (schema, slot_name)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_remove_slot", 0.0f);


    int idx = find_slot_index(schema, slot_name);
    if (idx < 0) {
        set_error("Slot not found: %s", slot_name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "schema_remove_slot: validation failed");
        return false;
    }

    // Free slot resources
    free_slot(&schema->slots[idx]);

    // Shift remaining slots
    memmove(&schema->slots[idx], &schema->slots[idx + 1],
            (schema->num_slots - idx - 1) * sizeof(schema_slot_t));
    schema->num_slots--;

    clear_error();
    return true;
}

schema_slot_t* schema_get_slot(schema_t* schema, const char* slot_name) {
    if (!schema || !slot_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_get_slot: required parameter is NULL (schema, slot_name)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_get_slot", 0.0f);


    int idx = find_slot_index(schema, slot_name);
    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_get_slot: validation failed");
        return NULL;
    }

    return &schema->slots[idx];
}

size_t schema_get_slot_count(const schema_t* schema) {
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_get_slot_coun", 0.0f);


    return schema ? schema->num_slots : 0;
}

size_t schema_get_required_slot_count(const schema_t* schema) {
    if (!schema) return 0;

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_get_required_", 0.0f);


    size_t count = 0;
    for (size_t i = 0; i < schema->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && schema->num_slots > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)schema->num_slots);
        }

        if (schema->slots[i].is_required) {
            count++;
        }
    }
    return count;
}

//=============================================================================
// Schema Instantiation
//=============================================================================

schema_instantiation_t* schema_instantiate(
    schema_system_t system,
    schema_t* schema,
    const char** slot_names,
    const prime_signature_t* fillers,
    size_t num_fillers)
{
    if (!system || !schema) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_instantiate: required parameter is NULL (system, schema)");
        return NULL;
    }

    // Check active capacity
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_instantiate", 0.0f);


    if (system->num_active >= system->active_capacity) {
        size_t new_capacity = system->active_capacity * 2;
        if (new_capacity > system->config.max_active) {
            new_capacity = system->config.max_active;
        }
        if (new_capacity <= system->num_active) {
            set_error("Too many active instantiations");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_instantiate: validation failed");
            return NULL;
        }

        schema_instantiation_t** new_active = (schema_instantiation_t**)nimcp_realloc(
            system->active, new_capacity * sizeof(schema_instantiation_t*));
        if (!new_active) {
            set_error("Failed to grow active array");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_instantiate: new_active is NULL");
            return NULL;
        }
        system->active = new_active;
        system->active_capacity = new_capacity;
    }

    // Allocate instantiation
    schema_instantiation_t* inst = (schema_instantiation_t*)nimcp_calloc(
        1, sizeof(schema_instantiation_t));
    if (!inst) {
        set_error("Failed to allocate instantiation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_instantiate: inst is NULL");
        return NULL;
    }

    inst->schema = schema;
    inst->instantiation_id = system->next_instantiation_id++;
    inst->created_time_ms = schema_current_time_ms();

    // Allocate slot arrays
    inst->filled_slots = (schema_slot_t*)nimcp_calloc(schema->num_slots, sizeof(schema_slot_t));
    inst->inferred_slots = (schema_slot_t*)nimcp_calloc(schema->num_slots, sizeof(schema_slot_t));
    if (!inst->filled_slots || !inst->inferred_slots) {
        set_error("Failed to allocate slot arrays");
        nimcp_free(inst->filled_slots);
        nimcp_free(inst->inferred_slots);
        nimcp_free(inst);
        inst = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_instantiate: required parameter is NULL (inst->filled_slots, inst->inferred_slots)");
        return NULL;
    }

    // Fill slots from input
    size_t matched = 0;
    size_t required_matched = 0;

    for (size_t i = 0; i < num_fillers && slot_names && fillers; i++) {
        int idx = find_slot_index(schema, slot_names[i]);
        if (idx >= 0) {
            // Copy slot definition
            if (!copy_slot(&inst->filled_slots[inst->num_filled], &schema->slots[idx])) {
                continue;
            }

            // Fill with provided value
            inst->filled_slots[inst->num_filled].filler = fillers[i];
            inst->filled_slots[inst->num_filled].is_filled = true;
            inst->filled_slots[inst->num_filled].confidence = 1.0f;  // Direct input

            inst->num_filled++;
            matched++;

            if (schema->slots[idx].is_required) {
                required_matched++;
            }
        }
    }

    // Compute fit score
    size_t total_required = schema_get_required_slot_count(schema);
    if (total_required > 0) {
        inst->fit_score = (float)required_matched / (float)total_required;
    } else if (schema->num_slots > 0) {
        inst->fit_score = (float)matched / (float)schema->num_slots;
    } else {
        inst->fit_score = 1.0f;
    }

    // Compute overall confidence
    float conf_sum = 0.0f;
    for (size_t i = 0; i < inst->num_filled; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && inst->num_filled > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)inst->num_filled);
        }

        conf_sum += inst->filled_slots[i].confidence;
    }
    inst->overall_confidence = inst->num_filled > 0 ?
        conf_sum / (float)inst->num_filled : 0.0f;

    // Add to active list
    system->active[system->num_active] = inst;
    system->num_active++;

    // Update schema statistics
    schema->activation_count++;
    schema->instantiation_count++;
    schema->last_activated_ms = inst->created_time_ms;

    // Update running average fit
    float n = (float)schema->instantiation_count;
    if (isfinite(inst->fit_score) && fabsf(n) > 1e-7f) {
        schema->avg_fit = ((n - 1.0f) * schema->avg_fit + inst->fit_score) / n;
    }

    // Update system stats
    system->stats.num_active = system->num_active;

    clear_error();

    /* W6: emit KG event for schema activation */
    memory_kg_emit_schema_activated(
        memory_kg_events_get_registered_brain(), schema->schema_id);

    return inst;
}

schema_instantiation_t* schema_instantiate_from_memory(
    schema_system_t system,
    schema_t* schema,
    pr_memory_node_t* memory)
{
    if (!system || !schema || !memory) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_instantiate_from_memory: required parameter is NULL (system, schema, memory)");
        return NULL;
    }

    // Get memory data
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_instantiate_f", 0.0f);


    const void* data = pr_memory_node_read(memory);
    if (!data) {
        set_error("Failed to read memory node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_instantiate_from_memory: data is NULL");
        return NULL;
    }

    // For now, create instantiation with memory signature as single filler
    // In production, would parse memory content to extract slot values
    const prime_signature_t* sig = pr_memory_node_get_signature(memory);
    if (!sig) {
        set_error("Memory has no signature");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_instantiate_from_memory: sig is NULL");
        return NULL;
    }

    // Create basic instantiation (single source)
    schema_instantiation_t* inst = schema_instantiate(system, schema, NULL, NULL, 0);
    if (!inst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "inst is NULL");

        return NULL;
    }

    // Track source memory
    inst->source_memory_ids = (uint64_t*)nimcp_malloc(sizeof(uint64_t));
    if (inst->source_memory_ids) {
        inst->source_memory_ids[0] = pr_memory_node_get_id(memory);
        inst->num_sources = 1;
    }

    return inst;
}

void schema_instantiation_destroy(schema_instantiation_t* instantiation) {
    if (!instantiation) return;

    // Free filled slots
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_instantiation", 0.0f);


    if (instantiation->filled_slots) {
        for (size_t i = 0; i < instantiation->num_filled; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && instantiation->num_filled > 256) {
                schemas_heartbeat("schemas_loop",
                                 (float)(i + 1) / (float)instantiation->num_filled);
            }

            free_slot(&instantiation->filled_slots[i]);
        }
        nimcp_free(instantiation->filled_slots);
    }

    // Free inferred slots
    if (instantiation->inferred_slots) {
        for (size_t i = 0; i < instantiation->num_inferred; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && instantiation->num_inferred > 256) {
                schemas_heartbeat("schemas_loop",
                                 (float)(i + 1) / (float)instantiation->num_inferred);
            }

            free_slot(&instantiation->inferred_slots[i]);
        }
        nimcp_free(instantiation->inferred_slots);
    }

    // Free source tracking
    nimcp_free(instantiation->source_memory_ids);

    nimcp_free(instantiation);
    instantiation = NULL;
}

schema_instantiation_t* schema_get_instantiation(
    schema_system_t system,
    uint64_t instantiation_id)
{
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_get_instantia", 0.0f);


    for (size_t i = 0; i < system->num_active; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_active > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)system->num_active);
        }

        if (system->active[i] &&
            system->active[i]->instantiation_id == instantiation_id) {
            return system->active[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_get_instantiation: operation failed");
    return NULL;
}

bool schema_instantiation_update_slot(
    schema_instantiation_t* instantiation,
    const char* slot_name,
    const prime_signature_t* filler,
    float confidence)
{
    if (!instantiation || !slot_name || !filler) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_instantiation_update_slot: required parameter is NULL (instantiation, slot_name, filler)");
        return false;
    }

    if (!instantiation->schema) {
        set_error("Instantiation has no schema");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_instantiation_update_slot: instantiation->schema is NULL");
        return false;
    }

    // Check if slot exists in schema
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_instantiation", 0.0f);


    int schema_idx = find_slot_index(instantiation->schema, slot_name);
    if (schema_idx < 0) {
        set_error("Slot not in schema: %s", slot_name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "schema_instantiation_update_slot: validation failed");
        return false;
    }

    // Check if already in filled slots
    for (size_t i = 0; i < instantiation->num_filled; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && instantiation->num_filled > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)instantiation->num_filled);
        }

        if (instantiation->filled_slots[i].slot_name &&
            strcmp(instantiation->filled_slots[i].slot_name, slot_name) == 0) {
            // Update existing
            instantiation->filled_slots[i].filler = *filler;
            instantiation->filled_slots[i].confidence = confidence;
            return true;
        }
    }

    // Add as new filled slot
    if (!copy_slot(&instantiation->filled_slots[instantiation->num_filled],
                   &instantiation->schema->slots[schema_idx])) {
        set_error("Failed to copy slot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "schema_instantiation_update_slot: operation failed");
        return false;
    }

    instantiation->filled_slots[instantiation->num_filled].filler = *filler;
    instantiation->filled_slots[instantiation->num_filled].is_filled = true;
    instantiation->filled_slots[instantiation->num_filled].confidence = confidence;
    instantiation->num_filled++;

    // Remove from inferred if present
    for (size_t i = 0; i < instantiation->num_inferred; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && instantiation->num_inferred > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)instantiation->num_inferred);
        }

        if (instantiation->inferred_slots[i].slot_name &&
            strcmp(instantiation->inferred_slots[i].slot_name, slot_name) == 0) {
            free_slot(&instantiation->inferred_slots[i]);
            memmove(&instantiation->inferred_slots[i],
                    &instantiation->inferred_slots[i + 1],
                    (instantiation->num_inferred - i - 1) * sizeof(schema_slot_t));
            instantiation->num_inferred--;
            break;
        }
    }

    clear_error();
    return true;
}

//=============================================================================
// Schema Matching
//=============================================================================

float schema_compute_fit(
    const schema_t* schema,
    const char** slot_names,
    const prime_signature_t* values,
    size_t num_slots)
{
    if (!schema) {
        set_error("NULL schema");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_compute_fit", 0.0f);


    if (schema->num_slots == 0) {
        return 1.0f;  // Empty schema matches everything
    }

    float total_score = 0.0f;
    float max_score = 0.0f;
    size_t matched = 0;
    size_t required_matched = 0;
    size_t total_required = 0;

    // Weight required slots higher
    const float required_weight = 2.0f;
    const float optional_weight = 1.0f;

    for (size_t i = 0; i < schema->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && schema->num_slots > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)schema->num_slots);
        }

        float weight = schema->slots[i].is_required ? required_weight : optional_weight;
        max_score += weight;

        if (schema->slots[i].is_required) {
            total_required++;
        }

        // Check if this slot is provided
        bool found = false;
        for (size_t j = 0; j < num_slots && slot_names && values; j++) {
            if (slot_names[j] &&
                strcmp(schema->slots[i].slot_name, slot_names[j]) == 0) {
                found = true;
                matched++;

                // Score based on signature similarity (if slot has pattern)
                float similarity = 1.0f;
                if (schema->slots[i].slot_signature.num_factors > 0) {
                    similarity = prime_sig_jaccard(&schema->slots[i].slot_signature,
                                                    &values[j]);
                    if (similarity < 0.0f) similarity = 0.5f;  // Error case
                }

                total_score += weight * similarity;

                if (schema->slots[i].is_required) {
                    required_matched++;
                }
                break;
            }
        }
    }

    // Penalize missing required slots heavily
    if (total_required > 0 && required_matched < total_required) {
        float required_ratio = (float)required_matched / (float)total_required;
        total_score *= required_ratio;
    }

    float fit = (max_score > 0.0f) ? (total_score / max_score) : 0.0f;
    return nimcp_clamp01(fit);
}

bool schema_match(
    schema_system_t system,
    const char** slot_names,
    const prime_signature_t* values,
    size_t num_slots,
    schema_match_result_t* result)
{
    if (!system || !result) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_match: required parameter is NULL (system, result)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_match", 0.0f);


    memset(result, 0, sizeof(schema_match_result_t));
    result->schema_id = SCHEMA_INVALID_ID;

    float best_fit = 0.0f;
    schema_t* best_schema = NULL;

    for (size_t i = 0; i < system->num_schemas; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_schemas > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)system->num_schemas);
        }

        if (!system->schemas[i]) continue;

        float fit = schema_compute_fit(system->schemas[i], slot_names, values, num_slots);
        if (fit > best_fit) {
            best_fit = fit;
            best_schema = system->schemas[i];
        }
    }

    system->stats.total_matches++;

    if (best_schema && best_fit >= system->config.min_fit_threshold) {
        result->schema_id = best_schema->schema_id;
        result->fit_score = best_fit;
        result->slots_total = best_schema->num_slots;
        result->required_total = schema_get_required_slot_count(best_schema);

        // Count matched slots
        for (size_t i = 0; i < num_slots && slot_names; i++) {
            if (find_slot_index(best_schema, slot_names[i]) >= 0) {
                result->slots_matched++;
                // Check if required
                schema_slot_t* slot = schema_get_slot(best_schema, slot_names[i]);
                if (slot && slot->is_required) {
                    result->required_matched++;
                }
            }
        }

        system->stats.successful_matches++;
        clear_error();
        return true;
    }

    set_error("No schema matched above threshold");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "schema_match: operation failed");
    return false;
}

bool schema_match_top_k(
    schema_system_t system,
    const char** slot_names,
    const prime_signature_t* values,
    size_t num_slots,
    size_t k,
    schema_match_result_t* results,
    size_t* result_count)
{
    if (!system || !results || !result_count) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_match_top_k: required parameter is NULL (system, results, result_count)");
        return false;
    }

    *result_count = 0;

    // Compute fit for all schemas
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_match_top_k", 0.0f);


    typedef struct {
        schema_t* schema;
        float fit;
    } fit_entry_t;

    fit_entry_t* entries = (fit_entry_t*)nimcp_calloc(system->num_schemas, sizeof(fit_entry_t));
    if (!entries) {
        set_error("Failed to allocate fit array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_match_top_k: entries is NULL");
        return false;
    }

    size_t valid_count = 0;
    for (size_t i = 0; i < system->num_schemas; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_schemas > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)system->num_schemas);
        }

        if (!system->schemas[i]) continue;

        float fit = schema_compute_fit(system->schemas[i], slot_names, values, num_slots);
        if (fit >= system->config.min_fit_threshold) {
            entries[valid_count].schema = system->schemas[i];
            entries[valid_count].fit = fit;
            valid_count++;
        }
    }

    // Sort by fit (simple bubble sort for small k)
    for (size_t i = 0; i < valid_count && i < k; i++) {
        for (size_t j = i + 1; j < valid_count; j++) {
            if (entries[j].fit > entries[i].fit) {
                fit_entry_t tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }

        // Store result
        schema_t* s = entries[i].schema;
        results[*result_count].schema_id = s->schema_id;
        results[*result_count].fit_score = entries[i].fit;
        results[*result_count].slots_total = s->num_slots;
        results[*result_count].required_total = schema_get_required_slot_count(s);

        // Count matched
        results[*result_count].slots_matched = 0;
        results[*result_count].required_matched = 0;
        for (size_t j = 0; j < num_slots && slot_names; j++) {
            if (find_slot_index(s, slot_names[j]) >= 0) {
                results[*result_count].slots_matched++;
                schema_slot_t* slot = schema_get_slot(s, slot_names[j]);
                if (slot && slot->is_required) {
                    results[*result_count].required_matched++;
                }
            }
        }

        (*result_count)++;
    }

    nimcp_free(entries);
    entries = NULL;
    clear_error();
    return true;
}

//=============================================================================
// Schema Inference
//=============================================================================

int schema_infer(schema_instantiation_t* instantiation) {
    if (!instantiation || !instantiation->schema) {
        set_error("NULL pointer");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_infer", 0.0f);


    int inferred_count = 0;
    schema_t* schema = instantiation->schema;

    for (size_t i = 0; i < schema->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && schema->num_slots > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)schema->num_slots);
        }

        // Skip if already filled
        bool is_filled = false;
        for (size_t j = 0; j < instantiation->num_filled && !is_filled; j++) {
            if (instantiation->filled_slots[j].slot_name &&
                strcmp(instantiation->filled_slots[j].slot_name,
                       schema->slots[i].slot_name) == 0) {
                is_filled = true;
            }
        }
        for (size_t j = 0; j < instantiation->num_inferred && !is_filled; j++) {
            if (instantiation->inferred_slots[j].slot_name &&
                strcmp(instantiation->inferred_slots[j].slot_name,
                       schema->slots[i].slot_name) == 0) {
                is_filled = true;
            }
        }

        if (is_filled) continue;

        // Check if slot has default value
        if (schema->slots[i].default_value.num_factors > 0) {
            // Copy slot with default
            if (copy_slot(&instantiation->inferred_slots[instantiation->num_inferred],
                          &schema->slots[i])) {
                instantiation->inferred_slots[instantiation->num_inferred].filler =
                    schema->slots[i].default_value;
                instantiation->inferred_slots[instantiation->num_inferred].is_filled = true;
                instantiation->inferred_slots[instantiation->num_inferred].confidence =
                    SCHEMA_DEFAULT_INFERRED_CONFIDENCE;
                instantiation->num_inferred++;
                inferred_count++;
            }
        }
    }

    return inferred_count;
}

bool schema_infer_slot(
    schema_system_t system,
    const schema_instantiation_t* instantiation,
    const char* slot_name,
    prime_signature_t* inferred_value,
    float* confidence)
{
    if (!system || !instantiation || !slot_name || !inferred_value || !confidence) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_infer_slot: required parameter is NULL (system, instantiation, slot_name, inferred_value, confidence)");
        return false;
    }

    if (!instantiation->schema) {
        set_error("Instantiation has no schema");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_infer_slot: instantiation->schema is NULL");
        return false;
    }

    // First try default value
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_infer_slot", 0.0f);


    schema_slot_t* slot = schema_get_slot(instantiation->schema, slot_name);
    if (!slot) {
        set_error("Slot not in schema: %s", slot_name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_infer_slot: slot is NULL");
        return false;
    }

    if (slot->default_value.num_factors > 0) {
        *inferred_value = slot->default_value;
        *confidence = system->config.inferred_confidence;

        system->stats.total_inferences++;
        clear_error();
        return true;
    }

    // TODO: Use cooccurrence statistics for better inference
    // For now, cannot infer without default
    set_error("No default value for slot: %s", slot_name);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "schema_infer_slot: operation failed");
    return false;
}

bool schema_get_expectation(
    schema_system_t system,
    const schema_instantiation_t* instantiation,
    schema_expectation_t* expectations,
    size_t max_expectations,
    size_t* count)
{
    if (!system || !instantiation || !expectations || !count) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_get_expectation: required parameter is NULL (system, instantiation, expectations, count)");
        return false;
    }

    *count = 0;

    if (!instantiation->schema) {
        set_error("Instantiation has no schema");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_get_expectation: instantiation->schema is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_get_expectati", 0.0f);


    schema_t* schema = instantiation->schema;

    for (size_t i = 0; i < schema->num_slots && *count < max_expectations; i++) {
        // Check if slot is unfilled
        bool is_filled = false;
        for (size_t j = 0; j < instantiation->num_filled && !is_filled; j++) {
            if (instantiation->filled_slots[j].slot_name &&
                strcmp(instantiation->filled_slots[j].slot_name,
                       schema->slots[i].slot_name) == 0) {
                is_filled = true;
            }
        }

        if (is_filled) continue;

        // Create expectation for unfilled slot
        schema_expectation_t* exp = &expectations[*count];
        exp->schema_id = schema->schema_id;
        exp->slot_name = schema->slots[i].slot_name;  // Not copied

        if (schema->slots[i].default_value.num_factors > 0) {
            exp->expected_value = schema->slots[i].default_value;
            exp->probability = 0.8f;  // High prob for default
        } else {
            memset(&exp->expected_value, 0, sizeof(prime_signature_t));
            exp->probability = 0.5f;  // Unknown
        }

        // Required slots expected with higher confidence
        exp->confidence = schema->slots[i].is_required ? 0.9f : 0.6f;

        (*count)++;
    }

    clear_error();
    return true;
}

//=============================================================================
// Schema Learning
//=============================================================================

bool schema_learn_from_instance(
    schema_system_t system,
    const schema_instantiation_t* instantiation)
{
    if (!system || !instantiation) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_learn_from_instance: required parameter is NULL (system, instantiation)");
        return false;
    }

    if (!system->config.enable_learning) {
        clear_error();
        return true;  // Learning disabled, but not an error
    }

    if (!instantiation->schema) {
        set_error("Instantiation has no schema");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_learn_from_instance: instantiation->schema is NULL");
        return false;
    }

    // Update schema statistics (already done in instantiate)

    // Update slot statistics
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_learn_from_in", 0.0f);


    for (size_t i = 0; i < instantiation->num_filled; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && instantiation->num_filled > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)instantiation->num_filled);
        }

        schema_slot_t* schema_slot = schema_get_slot(
            instantiation->schema, instantiation->filled_slots[i].slot_name);
        if (schema_slot) {
            schema_slot->fill_count++;

            // Update average confidence
            float n = (float)schema_slot->fill_count;
            schema_slot->avg_confidence =
                ((n - 1.0f) * schema_slot->avg_confidence +
                 instantiation->filled_slots[i].confidence) / (fabsf(n) > 1e-7f ? n : 1e-7f);
        }
    }

    // Update cooccurrence matrix
    update_cooccurrence(system, instantiation);

    clear_error();
    return true;
}

schema_t* schema_abstract(
    schema_system_t system,
    const schema_instantiation_t** instantiations,
    size_t num_instances,
    const char* name)
{
    if (!system || !instantiations || num_instances == 0 || !name) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_abstract: required parameter is NULL (system, instantiations, name)");
        return NULL;
    }

    // Find common schema type
    if (!instantiations[0] || !instantiations[0]->schema) {
        set_error("Invalid instantiation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_abstract: required parameter is NULL (instantiations, instantiations)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_abstract", 0.0f);


    schema_type_t type = instantiations[0]->schema->type;

    // Create new abstract schema
    schema_t* abstract = schema_create(system, name, type);
    if (!abstract) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "abstract is NULL");

        return NULL;

    }

    abstract->abstraction_level = 1.0f;  // Most abstract

    // Find common slots across all instances
    schema_t* first = instantiations[0]->schema;

    for (size_t s = 0; s < first->num_slots; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && first->num_slots > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(s + 1) / (float)first->num_slots);
        }

        const char* slot_name = first->slots[s].slot_name;
        bool in_all = true;
        size_t fill_count = 0;

        // Check if this slot is in all instances
        for (size_t i = 0; i < num_instances && in_all; i++) {
            if (!instantiations[i] || !instantiations[i]->schema) {
                in_all = false;
                continue;
            }

            if (find_slot_index(instantiations[i]->schema, slot_name) < 0) {
                in_all = false;
            } else {
                // Check if filled in this instance
                for (size_t f = 0; f < instantiations[i]->num_filled; f++) {
                    if (instantiations[i]->filled_slots[f].slot_name &&
                        strcmp(instantiations[i]->filled_slots[f].slot_name, slot_name) == 0) {
                        fill_count++;
                        break;
                    }
                }
            }
        }

        if (in_all) {
            // Slot is common - is it required?
            // Consider required if filled in most instances
            bool is_required = fill_count > num_instances / 2;

            schema_define_slot(abstract, slot_name, is_required, NULL);
        }
    }

    // Add to system
    if (!schema_add(system, abstract)) {
        free_schema_contents(abstract);
        nimcp_free(abstract);
        abstract = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_abstract: schema_add is NULL");
        return NULL;
    }

    return abstract;
}

schema_t* schema_specialize(
    schema_system_t system,
    const schema_t* parent_schema,
    const char* name,
    const schema_slot_t* extra_slots,
    size_t num_extra)
{
    if (!system || !parent_schema || !name) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_specialize: required parameter is NULL (system, parent_schema, name)");
        return NULL;
    }

    // Create new schema with same type
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_specialize", 0.0f);


    schema_t* child = schema_create(system, name, parent_schema->type);
    if (!child) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "child is NULL");

        return NULL;

    }

    child->abstraction_level = parent_schema->abstraction_level * 0.5f;  // More specific

    // Copy parent slots
    for (size_t i = 0; i < parent_schema->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && parent_schema->num_slots > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)parent_schema->num_slots);
        }

        schema_define_slot(child, parent_schema->slots[i].slot_name,
                          parent_schema->slots[i].is_required,
                          &parent_schema->slots[i].default_value);
    }

    // Add extra slots
    for (size_t i = 0; i < num_extra && extra_slots; i++) {
        schema_define_slot(child, extra_slots[i].slot_name,
                          extra_slots[i].is_required,
                          &extra_slots[i].default_value);
    }

    // Set up hierarchy
    child->parent_schema_id = parent_schema->schema_id;

    // Add child to parent's children list (need non-const access)
    schema_t* parent_mut = schema_find_by_id(system, parent_schema->schema_id);
    if (parent_mut) {
        if (parent_mut->num_children >= parent_mut->children_capacity) {
            size_t new_cap = parent_mut->children_capacity * 2;
            if (new_cap < 4) new_cap = 4;
            if (new_cap > SCHEMA_MAX_CHILDREN) new_cap = SCHEMA_MAX_CHILDREN;

            uint64_t* new_children = (uint64_t*)nimcp_realloc(
                parent_mut->child_schemas, new_cap * sizeof(uint64_t));
            if (!new_children) {
                /* Realloc failed - cannot add child to parent hierarchy */
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                    "schema_specialize: failed to grow children array");
            } else {
                parent_mut->child_schemas = new_children;
                parent_mut->children_capacity = new_cap;
            }
        }

        if (parent_mut->num_children < parent_mut->children_capacity) {
            parent_mut->child_schemas[parent_mut->num_children] = child->schema_id;
            parent_mut->num_children++;
        }
    }

    // Add to system
    if (!schema_add(system, child)) {
        free_schema_contents(child);
        nimcp_free(child);
        child = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_specialize: schema_add is NULL");
        return NULL;
    }

    return child;
}

schema_t* schema_merge(
    schema_system_t system,
    const schema_t* schema1,
    const schema_t* schema2,
    const char* name)
{
    if (!system || !schema1 || !schema2 || !name) {
        set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_merge: required parameter is NULL (system, schema1, schema2, name)");
        return NULL;
    }

    // Check similarity
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_merge", 0.0f);


    float sim = schema_similarity(schema1, schema2);
    if (sim < system->config.abstraction_threshold) {
        set_error("Schemas not similar enough to merge (%.2f < %.2f)",
                  sim, system->config.abstraction_threshold);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_merge: validation failed");
        return NULL;
    }

    // Use type of first schema
    schema_t* merged = schema_create(system, name, schema1->type);
    if (!merged) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "merged is NULL");

        return NULL;

    }

    merged->abstraction_level = (schema1->abstraction_level + schema2->abstraction_level) / 2.0f;

    // Union of slots
    for (size_t i = 0; i < schema1->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && schema1->num_slots > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)schema1->num_slots);
        }

        schema_define_slot(merged, schema1->slots[i].slot_name,
                          schema1->slots[i].is_required,
                          &schema1->slots[i].default_value);
    }

    for (size_t i = 0; i < schema2->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && schema2->num_slots > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)schema2->num_slots);
        }

        // Only add if not already present
        if (find_slot_index(merged, schema2->slots[i].slot_name) < 0) {
            schema_define_slot(merged, schema2->slots[i].slot_name,
                              schema2->slots[i].is_required,
                              &schema2->slots[i].default_value);
        }
    }

    // Add to system
    if (!schema_add(system, merged)) {
        free_schema_contents(merged);
        nimcp_free(merged);
        merged = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_merge: schema_add is NULL");
        return NULL;
    }

    return merged;
}

//=============================================================================
// Schema Violation Detection
//=============================================================================

bool schema_detect_violation(
    schema_system_t system,
    const schema_instantiation_t* instantiation,
    const char* slot_name,
    const prime_signature_t* value,
    schema_violation_t* violation)
{
    if (!system || !instantiation || !slot_name || !value || !violation) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_detect_violation: required parameter is NULL (system, instantiation, slot_name, value, violation)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_detect_violat", 0.0f);


    memset(violation, 0, sizeof(schema_violation_t));

    if (!instantiation->schema) {
        set_error("Instantiation has no schema");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_detect_violation: instantiation->schema is NULL");
        return false;
    }

    schema_slot_t* slot = schema_get_slot(instantiation->schema, slot_name);
    if (!slot) {
        set_error("Slot not in schema");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_detect_violation: slot is NULL");
        return false;
    }

    // Check constraint violation
    bool has_violation = false;

    switch (slot->constraint_type) {
        case SLOT_CONSTRAINT_SIGNATURE:
            if (slot->slot_signature.num_factors > 0) {
                float sim = prime_sig_jaccard(&slot->slot_signature, value);
                if (sim >= 0.0f && sim < 0.3f) {  // Low similarity = violation
                    has_violation = true;
                    violation->violation_severity = 1.0f - sim;
                }
            }
            break;

        case SLOT_CONSTRAINT_SCHEMA:
            // Would need to check if value is instance of required schema
            // Simplified: no check for now
            break;

        default:
            break;
    }

    // Check against default value if present
    if (!has_violation && slot->default_value.num_factors > 0) {
        float sim = prime_sig_jaccard(&slot->default_value, value);
        if (sim >= 0.0f && sim < 0.2f) {  // Very different from default
            has_violation = true;
            violation->violation_severity = (1.0f - sim) * 0.5f;  // Lower severity
        }
    }

    if (has_violation) {
        violation->schema_id = instantiation->schema->schema_id;
        violation->slot_name = (char*)slot_name;  // Not copied
        violation->expected = slot->default_value;
        violation->actual = *value;

        // Compute surprise (information-theoretic)
        // Simplified: based on fill frequency
        float p_expected = (slot->fill_count > 0) ?
            slot->avg_confidence : 0.5f;
        violation->surprise = -log2f(1.0f - p_expected + SCHEMA_EPSILON);

        system->stats.violations_detected++;
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "schema_detect_violation: operation failed");
    return false;
}

bool schema_get_violations(
    schema_system_t system,
    const schema_instantiation_t* instantiation,
    schema_violation_t* violations,
    size_t max_violations,
    size_t* count)
{
    if (!system || !instantiation || !violations || !count) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_get_violations: required parameter is NULL (system, instantiation, violations, count)");
        return false;
    }

    *count = 0;

    // Check each filled slot for violations
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_get_violation", 0.0f);


    for (size_t i = 0; i < instantiation->num_filled && *count < max_violations; i++) {
        schema_violation_t v;
        if (schema_detect_violation(system, instantiation,
                                    instantiation->filled_slots[i].slot_name,
                                    &instantiation->filled_slots[i].filler, &v)) {
            violations[*count] = v;
            (*count)++;
        }
    }

    clear_error();
    return true;
}

//=============================================================================
// Hierarchy Management
//=============================================================================

bool schema_set_parent(
    schema_system_t system,
    schema_t* child,
    const schema_t* parent)
{
    if (!system || !child || !parent) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_set_parent: required parameter is NULL (system, child, parent)");
        return false;
    }

    if (!system->config.enable_hierarchy) {
        set_error("Hierarchy disabled");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_set_parent: system->config is NULL");
        return false;
    }

    // Remove from old parent if any
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_set_parent", 0.0f);


    if (child->parent_schema_id != SCHEMA_INVALID_ID) {
        schema_t* old_parent = schema_find_by_id(system, child->parent_schema_id);
        if (old_parent) {
            for (size_t i = 0; i < old_parent->num_children; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && old_parent->num_children > 256) {
                    schemas_heartbeat("schemas_loop",
                                     (float)(i + 1) / (float)old_parent->num_children);
                }

                if (old_parent->child_schemas[i] == child->schema_id) {
                    memmove(&old_parent->child_schemas[i],
                            &old_parent->child_schemas[i + 1],
                            (old_parent->num_children - i - 1) * sizeof(uint64_t));
                    old_parent->num_children--;
                    break;
                }
            }
        }
    }

    // Set new parent
    child->parent_schema_id = parent->schema_id;

    // Add to new parent's children
    schema_t* parent_mut = schema_find_by_id(system, parent->schema_id);
    if (parent_mut) {
        if (parent_mut->num_children >= parent_mut->children_capacity) {
            size_t new_cap = parent_mut->children_capacity * 2;
            if (new_cap > SCHEMA_MAX_CHILDREN) new_cap = SCHEMA_MAX_CHILDREN;

            uint64_t* new_children = (uint64_t*)nimcp_realloc(
                parent_mut->child_schemas, new_cap * sizeof(uint64_t));
            if (!new_children) {
                set_error("Failed to grow children array");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "schema_set_parent: new_children is NULL");
                return false;
            }
            parent_mut->child_schemas = new_children;
            parent_mut->children_capacity = new_cap;
        }

        parent_mut->child_schemas[parent_mut->num_children] = child->schema_id;
        parent_mut->num_children++;
    }

    clear_error();
    return true;
}

schema_t* schema_get_parent(schema_system_t system, const schema_t* schema) {
    if (!system || !schema) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_get_parent: required parameter is NULL (system, schema)");
        return NULL;
    }
    if (schema->parent_schema_id == SCHEMA_INVALID_ID) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_get_parent: validation failed");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_get_parent", 0.0f);


    return schema_find_by_id(system, schema->parent_schema_id);
}

bool schema_get_children(
    schema_system_t system,
    const schema_t* schema,
    schema_t** children,
    size_t max_children,
    size_t* count)
{
    if (!system || !schema || !children || !count) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_get_children: required parameter is NULL (system, schema, children, count)");
        return false;
    }

    *count = 0;

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_get_children", 0.0f);


    for (size_t i = 0; i < schema->num_children && *count < max_children; i++) {
        schema_t* child = schema_find_by_id(system, schema->child_schemas[i]);
        if (child) {
            children[*count] = child;
            (*count)++;
        }
    }

    clear_error();
    return true;
}

bool schema_get_ancestors(
    schema_system_t system,
    const schema_t* schema,
    schema_t** ancestors,
    size_t max_ancestors,
    size_t* count)
{
    if (!system || !schema || !ancestors || !count) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_get_ancestors: required parameter is NULL (system, schema, ancestors, count)");
        return false;
    }

    *count = 0;

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_get_ancestors", 0.0f);


    schema_t* current = schema_get_parent(system, schema);
    while (current && *count < max_ancestors) {
        ancestors[*count] = current;
        (*count)++;
        current = schema_get_parent(system, current);
    }

    clear_error();
    return true;
}

//=============================================================================
// Statistics and Utilities
//=============================================================================

bool schema_get_stats(schema_system_t system, schema_stats_t* stats) {
    if (!system || !stats) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_get_stats: required parameter is NULL (system, stats)");
        return false;
    }

    *stats = system->stats;

    // Update dynamic stats
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_get_stats", 0.0f);


    stats->num_schemas = system->num_schemas;
    stats->num_active = system->num_active;

    // Recalculate type counts
    memset(stats->schemas_by_type, 0, sizeof(stats->schemas_by_type));
    for (size_t i = 0; i < system->num_schemas; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_schemas > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)system->num_schemas);
        }

        if (system->schemas[i] &&
            system->schemas[i]->type < SCHEMA_TYPE_COUNT) {
            stats->schemas_by_type[system->schemas[i]->type]++;
        }
    }

    // Estimate memory
    stats->memory_bytes = sizeof(struct schema_system_struct);
    stats->memory_bytes += system->schemas_capacity * sizeof(schema_t*);
    stats->memory_bytes += system->id_table_capacity * (sizeof(uint64_t) + sizeof(size_t));
    stats->memory_bytes += system->active_capacity * sizeof(schema_instantiation_t*);

    if (system->slot_cooccurrence) {
        stats->memory_bytes += system->cooc_dim * system->cooc_dim * sizeof(float);
    }

    for (size_t i = 0; i < system->num_schemas; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->num_schemas > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)system->num_schemas);
        }

        if (system->schemas[i]) {
            stats->memory_bytes += sizeof(schema_t);
            stats->memory_bytes += system->schemas[i]->num_slots * sizeof(schema_slot_t);
            stats->memory_bytes += system->schemas[i]->num_children * sizeof(uint64_t);
        }
    }

    clear_error();
    return true;
}

void schema_reset_stats(schema_system_t system) {
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_reset_stats", 0.0f);


    if (system) {
        memset(&system->stats, 0, sizeof(schema_stats_t));
    }
}

const char* schema_get_last_error(void) {
    return s_last_error[0] ? s_last_error : NULL;
}

const char* schema_type_name(schema_type_t type) {
    static const char* names[] = {
        "EVENT",
        "OBJECT",
        "PERSON",
        "SITUATION",
        "PROCEDURE"
    };

    if (type >= 0 && type < SCHEMA_TYPE_COUNT) {
        return names[type];
    }
    return "UNKNOWN";
}

void schema_print(const schema_t* schema) {
    if (!schema) {
        printf("Schema: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_print", 0.0f);


    printf("Schema [%lu]: %s\n", schema->schema_id,
           schema->schema_name ? schema->schema_name : "(unnamed)");
    printf("  Type: %s\n", schema_type_name(schema->type));
    printf("  Abstraction: %.2f (%s)\n", schema->abstraction_level,
           schema_abstraction_category(schema));
    printf("  Slots: %zu (required: %zu)\n", schema->num_slots,
           schema_get_required_slot_count(schema));

    for (size_t i = 0; i < schema->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && schema->num_slots > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)schema->num_slots);
        }

        printf("    [%zu] %s%s", i,
               schema->slots[i].slot_name ? schema->slots[i].slot_name : "?",
               schema->slots[i].is_required ? " (required)" : "");
        if (schema->slots[i].default_value.num_factors > 0) {
            printf(" [has default]");
        }
        printf("\n");
    }

    printf("  Parent: %s\n",
           schema->parent_schema_id != SCHEMA_INVALID_ID ? "yes" : "none");
    printf("  Children: %zu\n", schema->num_children);
    printf("  Activations: %lu, Instantiations: %lu, Avg fit: %.2f\n",
           schema->activation_count, schema->instantiation_count, schema->avg_fit);
}

void schema_instantiation_print(const schema_instantiation_t* instantiation) {
    if (!instantiation) {
        printf("Instantiation: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_instantiation", 0.0f);


    printf("Instantiation [%lu]\n", instantiation->instantiation_id);

    if (instantiation->schema) {
        printf("  Schema: %s [%lu]\n",
               instantiation->schema->schema_name ?
                   instantiation->schema->schema_name : "(unnamed)",
               instantiation->schema->schema_id);
    }

    printf("  Fit: %.2f, Confidence: %.2f\n",
           instantiation->fit_score, instantiation->overall_confidence);
    printf("  Filled slots: %zu\n", instantiation->num_filled);

    for (size_t i = 0; i < instantiation->num_filled; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && instantiation->num_filled > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)instantiation->num_filled);
        }

        printf("    %s (conf: %.2f)\n",
               instantiation->filled_slots[i].slot_name ?
                   instantiation->filled_slots[i].slot_name : "?",
               instantiation->filled_slots[i].confidence);
    }

    printf("  Inferred slots: %zu\n", instantiation->num_inferred);
    for (size_t i = 0; i < instantiation->num_inferred; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && instantiation->num_inferred > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)instantiation->num_inferred);
        }

        printf("    %s (conf: %.2f)\n",
               instantiation->inferred_slots[i].slot_name ?
                   instantiation->inferred_slots[i].slot_name : "?",
               instantiation->inferred_slots[i].confidence);
    }

    printf("  Sources: %zu\n", instantiation->num_sources);
}

bool schema_validate(const schema_t* schema) {
    if (!schema) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_validate: schema is NULL");
        return false;
    }

    // Check name
    if (!schema->schema_name || strlen(schema->schema_name) == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_validate: schema->schema_name is NULL");
        return false;
    }

    // Check type
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_validate", 0.0f);


    if (schema->type < 0 || schema->type >= SCHEMA_TYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "schema_validate: capacity exceeded");
        return false;
    }

    // Check slots array consistency
    if (schema->num_slots > schema->slots_capacity) {
        return false;
    }

    if (schema->num_slots > 0 && !schema->slots) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_validate: schema->slots is NULL");
        return false;
    }

    // Check children array consistency
    if (schema->num_children > schema->children_capacity) {
        return false;
    }

    if (schema->num_children > 0 && !schema->child_schemas) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "schema_validate: schema->child_schemas is NULL");
        return false;
    }

    // Check abstraction level range
    if (schema->abstraction_level < 0.0f || schema->abstraction_level > 1.0f) {
        return false;
    }

    return true;
}

float schema_similarity(const schema_t* schema1, const schema_t* schema2) {
    if (!schema1 || !schema2) {
        set_error("NULL schema");
        return -1.0f;
    }

    // Count shared slots
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_similarity", 0.0f);


    size_t shared = 0;
    size_t total = schema1->num_slots + schema2->num_slots;

    for (size_t i = 0; i < schema1->num_slots; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && schema1->num_slots > 256) {
            schemas_heartbeat("schemas_loop",
                             (float)(i + 1) / (float)schema1->num_slots);
        }

        if (find_slot_index((schema_t*)schema2, schema1->slots[i].slot_name) >= 0) {
            shared++;
        }
    }

    if (total == 0) return 1.0f;  // Both empty

    // Jaccard-style similarity
    float similarity = (2.0f * (float)shared) / (float)total;

    // Bonus for same type
    if (schema1->type == schema2->type) {
        similarity = similarity * 0.8f + 0.2f;
    }

    return nimcp_clamp01(similarity);
}

uint64_t schema_current_time_ms(void) {
    /* Phase 8: Heartbeat at operation start */
    schemas_heartbeat("schemas_schema_current_time_", 0.0f);


    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    }
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void schemas_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_schemas_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration
 * ============================================================================ */

int schemas_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "schemas_training_begin: NULL argument");
        return -1;
    }
    schemas_heartbeat_instance(NULL, "schemas_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int schemas_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "schemas_training_end: NULL argument");
        return -1;
    }
    schemas_heartbeat_instance(NULL, "schemas_training_end", 1.0f);
    (void)instance;
    return 0;
}

int schemas_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "schemas_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    schemas_heartbeat_instance(NULL, "schemas_training_step", progress);
    (void)instance;
    return 0;
}
