/**
 * @file nimcp_kg_metadata.c
 * @brief Implementation of Searchable Metadata System for Knowledge Graph Hierarchy
 * @version 1.0.0
 * @date 2025-01-16
 *
 * Implements the metadata system for all levels of the KG hierarchy.
 * Provides type-safe key-value storage with indexing and search support.
 */

#include "core/brain/nimcp_kg_metadata.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

/* ============================================================================
 * Logging Configuration
 * ============================================================================ */

#define LOG_MODULE "kg_metadata"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_metadata, MESH_ADAPTER_CATEGORY_COGNITIVE)


#ifdef NIMCP_LOGGING_ENABLED
    #define META_LOG_DEBUG(fmt, ...) NIMCP_LOG_DEBUG(LOG_MODULE, fmt, ##__VA_ARGS__)
    #define META_LOG_INFO(fmt, ...)  NIMCP_LOG_INFO(LOG_MODULE, fmt, ##__VA_ARGS__)
    #define META_LOG_WARN(fmt, ...)  NIMCP_LOG_WARN(LOG_MODULE, fmt, ##__VA_ARGS__)
    #define META_LOG_ERROR(fmt, ...) NIMCP_LOG_ERROR(LOG_MODULE, fmt, ##__VA_ARGS__)
#else
    #define META_LOG_DEBUG(fmt, ...) ((void)0)
    #define META_LOG_INFO(fmt, ...)  ((void)0)
    #define META_LOG_WARN(fmt, ...)  ((void)0)
    #define META_LOG_ERROR(fmt, ...) ((void)0)
#endif

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_current_timestamp_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Find entry by key in metadata
 *
 * @param meta Metadata container
 * @param key Key to find
 * @return Entry pointer or NULL if not found
 */
static kg_metadata_entry_t* find_entry(const kg_metadata_t* meta, const char* key) {
    if (!meta || !key || !meta->entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_entry: required parameter is NULL (meta, key, meta->entries)");
        return NULL;
    }

    for (uint32_t i = 0; i < meta->entry_count; i++) {
        if (strncmp(meta->entries[i].key, key, KG_META_MAX_KEY_LEN) == 0) {
            return &meta->entries[i];
        }
    }
    return NULL;
}

/**
 * @brief Find or create entry for a key
 *
 * @param meta Metadata container
 * @param key Key to find/create
 * @return Entry pointer or NULL on failure
 */
static kg_metadata_entry_t* find_or_create_entry(kg_metadata_t* meta, const char* key) {
    if (!meta || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "find_or_create_entry: required parameter is NULL (meta, key)");
        return NULL;
    }

    /* Check if entry already exists */
    kg_metadata_entry_t* existing = find_entry(meta, key);
    if (existing) {
        return existing;
    }

    /* Need to create new entry - check capacity */
    if (meta->entry_count >= meta->entry_capacity) {
        uint32_t new_capacity = meta->entry_capacity == 0 ?
                                KG_META_DEFAULT_CAPACITY :
                                meta->entry_capacity * 2;
        kg_metadata_entry_t* new_entries = nimcp_realloc(
            meta->entries,
            new_capacity * sizeof(kg_metadata_entry_t)
        );
        if (!new_entries) {
            META_LOG_ERROR("Failed to expand entries array");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "find_or_create_entry: new_entries is NULL");
            return NULL;
        }
        meta->entries = new_entries;
        meta->entry_capacity = new_capacity;
    }

    /* Initialize new entry */
    kg_metadata_entry_t* entry = &meta->entries[meta->entry_count];
    memset(entry, 0, sizeof(kg_metadata_entry_t));
    strncpy(entry->key, key, KG_META_MAX_KEY_LEN - 1);
    entry->key[KG_META_MAX_KEY_LEN - 1] = '\0';
    meta->entry_count++;

    return entry;
}

/**
 * @brief Free resources associated with an entry
 */
static void free_entry_resources(kg_metadata_entry_t* entry) {
    if (!entry) {
        return;
    }

    switch (entry->type) {
        case KG_META_TYPE_BLOB:
            if (entry->value.blob_val.data) {
                nimcp_free(entry->value.blob_val.data);
                entry->value.blob_val.data = NULL;
                entry->value.blob_val.size = 0;
            }
            break;

        case KG_META_TYPE_STRING_ARRAY:
            if (entry->value.str_array_val.items) {
                for (uint32_t i = 0; i < entry->value.str_array_val.count; i++) {
                    if (entry->value.str_array_val.items[i]) {
                        nimcp_free(entry->value.str_array_val.items[i]);
                    }
                }
                nimcp_free(entry->value.str_array_val.items);
                entry->value.str_array_val.items = NULL;
                entry->value.str_array_val.count = 0;
            }
            break;

        case KG_META_TYPE_JSON:
            if (entry->value.json_val) {
                nimcp_free(entry->value.json_val);
                entry->value.json_val = NULL;
            }
            break;

        default:
            /* Other types don't need special cleanup */
            break;
    }
}

/**
 * @brief Initialize base metadata fields
 */
static void init_base_metadata(kg_metadata_t* meta) {
    if (!meta) {
        return;
    }

    memset(meta, 0, sizeof(kg_metadata_t));
    kg_metadata_generate_uuid(meta->uuid);
    meta->created_at = get_current_timestamp_ms();
    meta->updated_at = meta->created_at;
    strncpy(meta->version, "1.0.0", KG_META_MAX_VERSION_LEN - 1);
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

kg_metadata_t* kg_metadata_create(void) {
    kg_metadata_t* meta = nimcp_calloc(1, sizeof(kg_metadata_t));
    if (!meta) {
        META_LOG_ERROR("Failed to allocate metadata container");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_metadata_create: meta is NULL");
        return NULL;
    }

    init_base_metadata(meta);
    META_LOG_DEBUG("Created metadata container uuid=%s", meta->uuid);
    return meta;
}

void kg_metadata_destroy(kg_metadata_t* meta) {
    if (!meta) {
        return;
    }

    /* Free all entries */
    if (meta->entries) {
        for (uint32_t i = 0; i < meta->entry_count; i++) {
            free_entry_resources(&meta->entries[i]);
        }
        nimcp_free(meta->entries);
    }

    /* Free tags */
    if (meta->tags) {
        nimcp_free(meta->tags);
    }

    META_LOG_DEBUG("Destroyed metadata container uuid=%s", meta->uuid);
    nimcp_free(meta);
}

kg_metadata_t* kg_metadata_clone(const kg_metadata_t* meta) {
    if (!meta) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta is NULL");

        return NULL;
    }

    kg_metadata_t* clone = nimcp_calloc(1, sizeof(kg_metadata_t));
    if (!clone) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "clone is NULL");

        return NULL;
    }

    /* Copy fixed fields */
    memcpy(clone->uuid, meta->uuid, KG_META_UUID_LEN);
    memcpy(clone->name, meta->name, KG_META_MAX_NAME_LEN);
    memcpy(clone->description, meta->description, KG_META_MAX_DESC_LEN);
    clone->created_at = meta->created_at;
    clone->updated_at = get_current_timestamp_ms();
    memcpy(clone->created_by, meta->created_by, KG_META_MAX_KEY_LEN);
    memcpy(clone->version, meta->version, KG_META_MAX_VERSION_LEN);

    /* Clone tags */
    if (meta->tags) {
        clone->tags = nimcp_strdup(meta->tags);
        if (!clone->tags) {
            kg_metadata_destroy(clone);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_metadata_clone: clone->tags is NULL");
            return NULL;
        }
    }

    /* Clone entries */
    if (meta->entry_count > 0 && meta->entries) {
        clone->entries = nimcp_calloc(meta->entry_count, sizeof(kg_metadata_entry_t));
        if (!clone->entries) {
            kg_metadata_destroy(clone);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_metadata_clone: clone->entries is NULL");
            return NULL;
        }
        clone->entry_capacity = meta->entry_count;

        for (uint32_t i = 0; i < meta->entry_count; i++) {
            const kg_metadata_entry_t* src = &meta->entries[i];
            kg_metadata_entry_t* dst = &clone->entries[i];

            memcpy(dst->key, src->key, KG_META_MAX_KEY_LEN);
            dst->type = src->type;
            dst->indexed = src->indexed;
            dst->encrypted = src->encrypted;

            switch (src->type) {
                case KG_META_TYPE_STRING:
                    memcpy(dst->value.str_val, src->value.str_val, KG_META_MAX_STRING_LEN);
                    break;
                case KG_META_TYPE_INT64:
                    dst->value.int_val = src->value.int_val;
                    break;
                case KG_META_TYPE_FLOAT64:
                    dst->value.float_val = src->value.float_val;
                    break;
                case KG_META_TYPE_BOOL:
                    dst->value.bool_val = src->value.bool_val;
                    break;
                case KG_META_TYPE_TIMESTAMP:
                    dst->value.timestamp_val = src->value.timestamp_val;
                    break;
                case KG_META_TYPE_BLOB:
                    if (src->value.blob_val.data && src->value.blob_val.size > 0) {
                        dst->value.blob_val.data = nimcp_malloc(src->value.blob_val.size);
                        if (!dst->value.blob_val.data) {
                            kg_metadata_destroy(clone);
                            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_metadata_clone: dst->value is NULL");
                            return NULL;
                        }
                        memcpy(dst->value.blob_val.data, src->value.blob_val.data,
                               src->value.blob_val.size);
                        dst->value.blob_val.size = src->value.blob_val.size;
                    }
                    break;
                case KG_META_TYPE_STRING_ARRAY:
                    if (src->value.str_array_val.items && src->value.str_array_val.count > 0) {
                        dst->value.str_array_val.items = nimcp_calloc(
                            src->value.str_array_val.count, sizeof(char*)
                        );
                        if (!dst->value.str_array_val.items) {
                            kg_metadata_destroy(clone);
                            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_metadata_clone: dst->value is NULL");
                            return NULL;
                        }
                        for (uint32_t j = 0; j < src->value.str_array_val.count; j++) {
                            if (src->value.str_array_val.items[j]) {
                                dst->value.str_array_val.items[j] =
                                    nimcp_strdup(src->value.str_array_val.items[j]);
                                if (!dst->value.str_array_val.items[j]) {
                                    kg_metadata_destroy(clone);
                                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_metadata_clone: dst->value is NULL");
                                    return NULL;
                                }
                            }
                        }
                        dst->value.str_array_val.count = src->value.str_array_val.count;
                    }
                    break;
                case KG_META_TYPE_JSON:
                    if (src->value.json_val) {
                        dst->value.json_val = nimcp_strdup(src->value.json_val);
                        if (!dst->value.json_val) {
                            kg_metadata_destroy(clone);
                            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_metadata_clone: dst->value is NULL");
                            return NULL;
                        }
                    }
                    break;
            }
            clone->entry_count++;
        }
    }

    META_LOG_DEBUG("Cloned metadata container uuid=%s", clone->uuid);
    return clone;
}

/* ============================================================================
 * Setters Implementation
 * ============================================================================ */

int kg_metadata_set_string(kg_metadata_t* meta, const char* key,
                           const char* value, bool indexed) {
    if (!meta || !key || !value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_metadata_clone: required parameter is NULL (meta, key, value)");
        return -1;
    }

    kg_metadata_entry_t* entry = find_or_create_entry(meta, key);
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entry is NULL");

        return -1;
    }

    /* Free any previous resources */
    free_entry_resources(entry);

    entry->type = KG_META_TYPE_STRING;
    strncpy(entry->value.str_val, value, KG_META_MAX_STRING_LEN - 1);
    entry->value.str_val[KG_META_MAX_STRING_LEN - 1] = '\0';
    entry->indexed = indexed;

    meta->updated_at = get_current_timestamp_ms();
    return 0;
}

int kg_metadata_set_int(kg_metadata_t* meta, const char* key,
                        int64_t value, bool indexed) {
    if (!meta || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_metadata_clone: required parameter is NULL (meta, key)");
        return -1;
    }

    kg_metadata_entry_t* entry = find_or_create_entry(meta, key);
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entry is NULL");

        return -1;
    }

    free_entry_resources(entry);

    entry->type = KG_META_TYPE_INT64;
    entry->value.int_val = value;
    entry->indexed = indexed;

    meta->updated_at = get_current_timestamp_ms();
    return 0;
}

int kg_metadata_set_float(kg_metadata_t* meta, const char* key,
                          double value, bool indexed) {
    if (!meta || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_metadata_clone: required parameter is NULL (meta, key)");
        return -1;
    }

    kg_metadata_entry_t* entry = find_or_create_entry(meta, key);
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entry is NULL");

        return -1;
    }

    free_entry_resources(entry);

    entry->type = KG_META_TYPE_FLOAT64;
    entry->value.float_val = value;
    entry->indexed = indexed;

    meta->updated_at = get_current_timestamp_ms();
    return 0;
}

int kg_metadata_set_bool(kg_metadata_t* meta, const char* key,
                         bool value, bool indexed) {
    if (!meta || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_metadata_clone: required parameter is NULL (meta, key)");
        return -1;
    }

    kg_metadata_entry_t* entry = find_or_create_entry(meta, key);
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entry is NULL");

        return -1;
    }

    free_entry_resources(entry);

    entry->type = KG_META_TYPE_BOOL;
    entry->value.bool_val = value;
    entry->indexed = indexed;

    meta->updated_at = get_current_timestamp_ms();
    return 0;
}

int kg_metadata_set_timestamp(kg_metadata_t* meta, const char* key,
                              uint64_t value, bool indexed) {
    if (!meta || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (meta, key)");
        return -1;
    }

    kg_metadata_entry_t* entry = find_or_create_entry(meta, key);
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entry is NULL");

        return -1;
    }

    free_entry_resources(entry);

    entry->type = KG_META_TYPE_TIMESTAMP;
    entry->value.timestamp_val = value;
    entry->indexed = indexed;

    meta->updated_at = get_current_timestamp_ms();
    return 0;
}

int kg_metadata_set_json(kg_metadata_t* meta, const char* key,
                         const char* json, bool indexed) {
    if (!meta || !key || !json) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (meta, key, json)");
        return -1;
    }

    kg_metadata_entry_t* entry = find_or_create_entry(meta, key);
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entry is NULL");

        return -1;
    }

    free_entry_resources(entry);

    entry->type = KG_META_TYPE_JSON;
    entry->value.json_val = nimcp_strdup(json);
    if (!entry->value.json_val) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: entry->value is NULL");
        return -1;
    }
    entry->indexed = indexed;

    meta->updated_at = get_current_timestamp_ms();
    return 0;
}

int kg_metadata_set_tags(kg_metadata_t* meta, const char* comma_separated_tags) {
    if (!meta) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta is NULL");

        return -1;
    }

    /* Free existing tags */
    if (meta->tags) {
        nimcp_free(meta->tags);
        meta->tags = NULL;
    }

    /* Set new tags if provided */
    if (comma_separated_tags && comma_separated_tags[0] != '\0') {
        meta->tags = nimcp_strdup(comma_separated_tags);
        if (!meta->tags) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_metadata_set_tags: meta->tags is NULL");
            return -1;
        }
    }

    meta->updated_at = get_current_timestamp_ms();
    return 0;
}

/* ============================================================================
 * Getters Implementation
 * ============================================================================ */

const char* kg_metadata_get_string(const kg_metadata_t* meta, const char* key) {
    const kg_metadata_entry_t* entry = find_entry(meta, key);
    if (!entry || entry->type != KG_META_TYPE_STRING) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_metadata_get_string: entry is NULL");
        return NULL;
    }
    return entry->value.str_val;
}

int64_t kg_metadata_get_int(const kg_metadata_t* meta, const char* key,
                            int64_t default_val) {
    const kg_metadata_entry_t* entry = find_entry(meta, key);
    if (!entry || entry->type != KG_META_TYPE_INT64) {
        return default_val;
    }
    return entry->value.int_val;
}

double kg_metadata_get_float(const kg_metadata_t* meta, const char* key,
                             double default_val) {
    const kg_metadata_entry_t* entry = find_entry(meta, key);
    if (!entry || entry->type != KG_META_TYPE_FLOAT64) {
        return default_val;
    }
    return entry->value.float_val;
}

bool kg_metadata_get_bool(const kg_metadata_t* meta, const char* key,
                          bool default_val) {
    const kg_metadata_entry_t* entry = find_entry(meta, key);
    if (!entry || entry->type != KG_META_TYPE_BOOL) {
        return default_val;
    }
    return entry->value.bool_val;
}

bool kg_metadata_has_key(const kg_metadata_t* meta, const char* key) {
    return find_entry(meta, key) != NULL;
}

bool kg_metadata_has_tag(const kg_metadata_t* meta, const char* tag) {
    if (!meta || !tag || !meta->tags || tag[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_metadata_has_tag: required parameter is NULL (meta, tag, meta->tags)");
        return false;
    }

    size_t tag_len = strlen(tag);
    const char* p = meta->tags;

    while (*p) {
        /* Skip leading whitespace and commas */
        while (*p == ',' || *p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }

        /* Find end of current tag */
        const char* tag_start = p;
        while (*p && *p != ',') {
            p++;
        }

        /* Trim trailing whitespace */
        const char* tag_end = p;
        while (tag_end > tag_start && (tag_end[-1] == ' ' || tag_end[-1] == '\t')) {
            tag_end--;
        }

        size_t current_len = (size_t)(tag_end - tag_start);
        if (current_len == tag_len && strncmp(tag_start, tag, tag_len) == 0) {
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_metadata_has_tag: validation failed");
    return false;
}

/* ============================================================================
 * Level-Specific Lifecycle Implementation
 * ============================================================================ */

kg_meta_module_t* kg_module_metadata_create(void) {
    kg_meta_module_t* meta = nimcp_calloc(1, sizeof(kg_meta_module_t));
    if (!meta) {
        META_LOG_ERROR("Failed to allocate module metadata");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_module_metadata_create: meta is NULL");
        return NULL;
    }

    init_base_metadata(&meta->base);
    meta->health_score = 1.0f;
    strncpy(meta->status, "created", sizeof(meta->status) - 1);

    META_LOG_DEBUG("Created module metadata uuid=%s", meta->base.uuid);
    return meta;
}

void kg_module_metadata_destroy(kg_meta_module_t* meta) {
    if (!meta) {
        return;
    }

    /* Free base metadata entries */
    if (meta->base.entries) {
        for (uint32_t i = 0; i < meta->base.entry_count; i++) {
            free_entry_resources(&meta->base.entries[i]);
        }
        nimcp_free(meta->base.entries);
    }
    if (meta->base.tags) {
        nimcp_free(meta->base.tags);
    }

    /* Free module-specific heap allocations */
    if (meta->capabilities) {
        nimcp_free(meta->capabilities);
    }
    if (meta->dependencies) {
        nimcp_free(meta->dependencies);
    }

    META_LOG_DEBUG("Destroyed module metadata uuid=%s", meta->base.uuid);
    nimcp_free(meta);
}

kg_layer_metadata_t* kg_layer_metadata_create(void) {
    kg_layer_metadata_t* meta = nimcp_calloc(1, sizeof(kg_layer_metadata_t));
    if (!meta) {
        META_LOG_ERROR("Failed to allocate layer metadata");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_layer_metadata_create: meta is NULL");
        return NULL;
    }

    init_base_metadata(&meta->base);
    meta->aggregate_health = 1.0f;
    meta->layer_coherence = 1.0f;

    META_LOG_DEBUG("Created layer metadata uuid=%s", meta->base.uuid);
    return meta;
}

void kg_layer_metadata_destroy(kg_layer_metadata_t* meta) {
    if (!meta) {
        return;
    }

    /* Free base metadata entries */
    if (meta->base.entries) {
        for (uint32_t i = 0; i < meta->base.entry_count; i++) {
            free_entry_resources(&meta->base.entries[i]);
        }
        nimcp_free(meta->base.entries);
    }
    if (meta->base.tags) {
        nimcp_free(meta->base.tags);
    }

    /* Free layer-specific heap allocations */
    if (meta->module_types) {
        nimcp_free(meta->module_types);
    }
    if (meta->connection_summary) {
        nimcp_free(meta->connection_summary);
    }

    META_LOG_DEBUG("Destroyed layer metadata uuid=%s", meta->base.uuid);
    nimcp_free(meta);
}

kg_hemisphere_metadata_t* kg_hemisphere_metadata_create(void) {
    kg_hemisphere_metadata_t* meta = nimcp_calloc(1, sizeof(kg_hemisphere_metadata_t));
    if (!meta) {
        META_LOG_ERROR("Failed to allocate hemisphere metadata");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_hemisphere_metadata_create: meta is NULL");
        return NULL;
    }

    init_base_metadata(&meta->base);
    meta->specialization_score = 0.5f;

    META_LOG_DEBUG("Created hemisphere metadata uuid=%s", meta->base.uuid);
    return meta;
}

void kg_hemisphere_metadata_destroy(kg_hemisphere_metadata_t* meta) {
    if (!meta) {
        return;
    }

    /* Free base metadata entries */
    if (meta->base.entries) {
        for (uint32_t i = 0; i < meta->base.entry_count; i++) {
            free_entry_resources(&meta->base.entries[i]);
        }
        nimcp_free(meta->base.entries);
    }
    if (meta->base.tags) {
        nimcp_free(meta->base.tags);
    }

    /* Free hemisphere-specific heap allocations */
    if (meta->layer_summary) {
        nimcp_free(meta->layer_summary);
    }
    if (meta->cross_hemisphere_bridges) {
        nimcp_free(meta->cross_hemisphere_bridges);
    }

    META_LOG_DEBUG("Destroyed hemisphere metadata uuid=%s", meta->base.uuid);
    nimcp_free(meta);
}

kg_system_metadata_t* kg_system_metadata_create(void) {
    kg_system_metadata_t* meta = nimcp_calloc(1, sizeof(kg_system_metadata_t));
    if (!meta) {
        META_LOG_ERROR("Failed to allocate system metadata");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_system_metadata_create: meta is NULL");
        return NULL;
    }

    init_base_metadata(&meta->base);
    meta->overall_health = 1.0f;
    meta->integration_score = 0.5f;
    meta->self_model_accuracy = 0.5f;

    META_LOG_DEBUG("Created system metadata uuid=%s", meta->base.uuid);
    return meta;
}

void kg_system_metadata_destroy(kg_system_metadata_t* meta) {
    if (!meta) {
        return;
    }

    /* Free base metadata entries */
    if (meta->base.entries) {
        for (uint32_t i = 0; i < meta->base.entry_count; i++) {
            free_entry_resources(&meta->base.entries[i]);
        }
        nimcp_free(meta->base.entries);
    }
    if (meta->base.tags) {
        nimcp_free(meta->base.tags);
    }

    /* Free system-specific heap allocations */
    if (meta->subsystem_status) {
        nimcp_free(meta->subsystem_status);
    }

    META_LOG_DEBUG("Destroyed system metadata uuid=%s", meta->base.uuid);
    nimcp_free(meta);
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* kg_meta_type_to_string(kg_meta_type_t type) {
    switch (type) {
        case KG_META_TYPE_STRING:       return "STRING";
        case KG_META_TYPE_INT64:        return "INT64";
        case KG_META_TYPE_FLOAT64:      return "FLOAT64";
        case KG_META_TYPE_BOOL:         return "BOOL";
        case KG_META_TYPE_TIMESTAMP:    return "TIMESTAMP";
        case KG_META_TYPE_BLOB:         return "BLOB";
        case KG_META_TYPE_STRING_ARRAY: return "STRING_ARRAY";
        case KG_META_TYPE_JSON:         return "JSON";
        default:                        return "UNKNOWN";
    }
}

int kg_metadata_generate_uuid(char* uuid_out) {
    if (!uuid_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "uuid_out is NULL");

        return -1;
    }

    /* Generate random UUID v4 */
    unsigned char bytes[16];

    /* Get random bytes - use platform random source */
    FILE* urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        size_t read = fread(bytes, 1, 16, urandom);
        fclose(urandom);
        if (read != 16) {
            /* Fallback to rand() if urandom fails */
            for (int i = 0; i < 16; i++) {
                bytes[i] = (unsigned char)(nimcp_tl_rand() & 0xFF);
            }
        }
    } else {
        /* Fallback to rand() */
        for (int i = 0; i < 16; i++) {
            bytes[i] = (unsigned char)(nimcp_tl_rand() & 0xFF);
        }
    }

    /* Set version (4) and variant bits */
    bytes[6] = (bytes[6] & 0x0F) | 0x40;  /* Version 4 */
    bytes[8] = (bytes[8] & 0x3F) | 0x80;  /* Variant 1 */

    /* Format as UUID string */
    snprintf(uuid_out, KG_META_UUID_LEN,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2], bytes[3],
             bytes[4], bytes[5],
             bytes[6], bytes[7],
             bytes[8], bytes[9],
             bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);

    return 0;
}

int kg_metadata_touch(kg_metadata_t* meta) {
    if (!meta) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta is NULL");

        return -1;
    }
    meta->updated_at = get_current_timestamp_ms();
    return 0;
}

int kg_metadata_remove(kg_metadata_t* meta, const char* key) {
    if (!meta || !key || !meta->entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_metadata_remove: required parameter is NULL (meta, key, meta->entries)");
        return -1;
    }

    /* Find the entry */
    for (uint32_t i = 0; i < meta->entry_count; i++) {
        if (strncmp(meta->entries[i].key, key, KG_META_MAX_KEY_LEN) == 0) {
            /* Free entry resources */
            free_entry_resources(&meta->entries[i]);

            /* Shift remaining entries */
            if (i < meta->entry_count - 1) {
                memmove(&meta->entries[i], &meta->entries[i + 1],
                        (meta->entry_count - i - 1) * sizeof(kg_metadata_entry_t));
            }

            meta->entry_count--;
            meta->updated_at = get_current_timestamp_ms();
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_metadata_remove: operation failed");
    return -1;  /* Key not found */
}

uint32_t kg_metadata_entry_count(const kg_metadata_t* meta) {
    if (!meta) {
        return 0;
    }
    return meta->entry_count;
}

const kg_metadata_entry_t* kg_metadata_get_entry(const kg_metadata_t* meta,
                                                  uint32_t index) {
    if (!meta || !meta->entries || index >= meta->entry_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_metadata_entry_count: required parameter is NULL (meta, meta->entries)");
        return NULL;
    }
    return &meta->entries[index];
}
