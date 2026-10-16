/**
 * @file nimcp_kg_search.c
 * @brief Implementation of Knowledge Graph Search and Query API
 * @version 1.0.0
 * @date 2025-01-16
 *
 * Implements indexed search with multiple operators, pagination, and relevance scoring.
 */

#include "core/brain/nimcp_kg_search.h"
#include "core/brain/nimcp_kg_metadata.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <regex.h>
#include <time.h>
#include <math.h>
#include <errno.h>

/* ============================================================================
 * Logging Configuration
 * ============================================================================ */

#define LOG_MODULE "kg_search"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_search, MESH_ADAPTER_CATEGORY_COGNITIVE)


#ifdef NIMCP_LOGGING_ENABLED
    #define SEARCH_LOG_DEBUG(fmt, ...) NIMCP_LOG_DEBUG(LOG_MODULE, fmt, ##__VA_ARGS__)
    #define SEARCH_LOG_INFO(fmt, ...)  NIMCP_LOG_INFO(LOG_MODULE, fmt, ##__VA_ARGS__)
    #define SEARCH_LOG_WARN(fmt, ...)  NIMCP_LOG_WARN(LOG_MODULE, fmt, ##__VA_ARGS__)
    #define SEARCH_LOG_ERROR(fmt, ...) NIMCP_LOG_ERROR(LOG_MODULE, fmt, ##__VA_ARGS__)
#else
    #define SEARCH_LOG_DEBUG(fmt, ...) ((void)0)
    #define SEARCH_LOG_INFO(fmt, ...)  ((void)0)
    #define SEARCH_LOG_WARN(fmt, ...)  ((void)0)
    #define SEARCH_LOG_ERROR(fmt, ...) ((void)0)
#endif

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MAX_INDEXED_MODULES     4096
#define MAX_INDEXED_LAYERS      6
#define MAX_INDEXED_HEMISPHERES 2
#define MAX_TEXT_TERMS          8192
#define TERM_MAX_LEN            64

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Indexed module entry
 */
typedef struct {
    brain_kg_node_id_t node_id;
    kg_meta_module_t* metadata;     /* Owned copy */
    bool valid;
} indexed_module_t;

/**
 * @brief Indexed layer entry
 */
typedef struct {
    uint8_t layer_index;
    kg_layer_metadata_t* metadata;      /* Owned copy */
    bool valid;
} indexed_layer_t;

/**
 * @brief Indexed hemisphere entry
 */
typedef struct {
    uint8_t hemisphere_id;
    kg_hemisphere_metadata_t* metadata; /* Owned copy */
    bool valid;
} indexed_hemisphere_t;

/**
 * @brief Term entry for full-text index
 */
typedef struct {
    char term[TERM_MAX_LEN];
    brain_kg_node_id_t* postings;       /* List of nodes containing this term */
    uint32_t posting_count;
    uint32_t posting_capacity;
} term_entry_t;

/**
 * @brief Search index internal structure
 */
struct kg_search_index {
    /* Module index */
    indexed_module_t* modules;
    uint32_t module_count;
    uint32_t module_capacity;

    /* Layer index */
    indexed_layer_t layers[MAX_INDEXED_LAYERS];

    /* Hemisphere index */
    indexed_hemisphere_t hemispheres[MAX_INDEXED_HEMISPHERES];

    /* System metadata (single entry) */
    kg_system_metadata_t* system_metadata;

    /* Full-text index */
    term_entry_t* terms;
    uint32_t term_count;
    uint32_t term_capacity;

    /* Statistics */
    uint64_t total_searches;
    uint64_t total_search_time_us;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/**
 * @brief Internal search result for sorting
 */
typedef struct {
    brain_kg_node_id_t node_id;
    kg_search_result_level_t level;
    kg_metadata_t* metadata;
    float relevance_score;
    int match_count;                    /* Number of conditions matched */
} internal_result_t;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/**
 * @brief Case-insensitive substring search
 */
static const char* strcasestr_local(const char* haystack, const char* needle) {
    if (!haystack || !needle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "strcasestr_local: required parameter is NULL (haystack, needle)");
        return NULL;
    }
    if (*needle == '\0') return haystack;

    size_t needle_len = strlen(needle);
    size_t haystack_len = strlen(haystack);

    if (needle_len > haystack_len) {
        return NULL;
    }

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (strncasecmp(&haystack[i], needle, needle_len) == 0) {
            return &haystack[i];
        }
    }
    return NULL;
}

/**
 * @brief Check if string starts with prefix (case-insensitive)
 */
static bool starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "starts_with: required parameter is NULL (str, prefix)");
        return false;
    }
    return strncasecmp(str, prefix, strlen(prefix)) == 0;
}

/**
 * @brief Check if string ends with suffix (case-insensitive)
 */
static bool ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ends_with: required parameter is NULL (str, suffix)");
        return false;
    }
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) {
        return false;
    }
    return strncasecmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

/**
 * @brief Check if value is in comma-separated list
 */
static bool value_in_list(const char* value, const char* list) {
    if (!value || !list) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "value_in_list: required parameter is NULL (value, list)");
        return false;
    }

    char* list_copy = nimcp_strdup(list);
    if (!list_copy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "value_in_list: allocation failed");
        return false;
    }

    bool found = false;
    char* token = strtok(list_copy, ",");
    while (token) {
        /* Trim whitespace */
        while (*token == ' ') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && *end == ' ') *end-- = '\0';

        if (strcasecmp(token, value) == 0) {
            found = true;
            break;
        }
        token = strtok(NULL, ",");
    }

    nimcp_free(list_copy);
    return found;
}

/**
 * @brief Get string value from base metadata for a field
 */
static const char* get_base_meta_field_string(const kg_metadata_t* meta, const char* field) {
    if (!meta || !field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_base_meta_field_string: required parameter is NULL (meta, field)");
        return NULL;
    }

    if (strcmp(field, "name") == 0) return meta->name;
    if (strcmp(field, "description") == 0) return meta->description;
    if (strcmp(field, "uuid") == 0) return meta->uuid;
    if (strcmp(field, "tags") == 0) return meta->tags;
    if (strcmp(field, "created_by") == 0) return meta->created_by;
    if (strcmp(field, "version") == 0) return meta->version;

    /* Check custom entries */
    return kg_metadata_get_string(meta, field);
}

/**
 * @brief Get string value from module metadata for a field
 */
static const char* get_module_field_string(const kg_meta_module_t* meta, const char* field) {
    if (!meta || !field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_module_field_string: required parameter is NULL (meta, field)");
        return NULL;
    }

    /* Check module-specific fields first */
    if (strcmp(field, "module_type") == 0) return meta->module_type;
    if (strcmp(field, "subsystem") == 0) return meta->subsystem;
    if (strcmp(field, "status") == 0) return meta->status;
    if (strcmp(field, "capabilities") == 0) return meta->capabilities;
    if (strcmp(field, "dependencies") == 0) return meta->dependencies;
    if (strcmp(field, "api_version") == 0) return meta->api_version;

    /* Fall through to base metadata */
    return get_base_meta_field_string(&meta->base, field);
}

/**
 * @brief Get float value from module metadata for a field
 */
static float get_module_field_float(const kg_meta_module_t* meta, const char* field) {
    if (!meta || !field) return 0.0f;

    if (strcmp(field, "health_score") == 0) return meta->health_score;
    if (strcmp(field, "avg_latency_ms") == 0) return meta->avg_latency_ms;

    return (float)kg_metadata_get_float(&meta->base, field, 0.0);
}

/**
 * @brief Get int value from module metadata for a field
 */
static int64_t get_module_field_int(const kg_meta_module_t* meta, const char* field) {
    if (!meta || !field) return 0;

    if (strcmp(field, "cortical_layer") == 0) return meta->cortical_layer;
    if (strcmp(field, "hemisphere") == 0) return meta->hemisphere;
    if (strcmp(field, "uptime_ms") == 0) return (int64_t)meta->uptime_ms;
    if (strcmp(field, "message_count") == 0) return (int64_t)meta->message_count;

    return kg_metadata_get_int(&meta->base, field, 0);
}

/**
 * @brief Evaluate a single condition against module metadata
 */
static bool evaluate_condition_module(const kg_meta_module_t* meta,
                                       const kg_search_condition_t* cond,
                                       float* relevance_contribution) {
    if (!meta || !cond) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "evaluate_condition_module: required parameter is NULL (meta, cond)");
        return false;
    }

    *relevance_contribution = 0.0f;

    const char* str_val = get_module_field_string(meta, cond->field);
    float float_val = get_module_field_float(meta, cond->field);
    int64_t int_val = get_module_field_int(meta, cond->field);

    switch (cond->op) {
        case KG_SEARCH_OP_EQUALS:
            if (str_val && strcmp(str_val, cond->value) == 0) {
                *relevance_contribution = 1.0f;
                return true;
            }
            break;

        case KG_SEARCH_OP_NOT_EQUALS:
            if (!str_val || strcmp(str_val, cond->value) != 0) {
                *relevance_contribution = 0.5f;
                return true;
            }
            break;

        case KG_SEARCH_OP_CONTAINS:
            if (str_val && strcasestr_local(str_val, cond->value)) {
                /* Score based on relative length */
                size_t val_len = strlen(cond->value);
                size_t str_len = strlen(str_val);
                *relevance_contribution = (float)val_len / (float)(str_len + 1);
                return true;
            }
            break;

        case KG_SEARCH_OP_STARTS_WITH:
            if (str_val && starts_with(str_val, cond->value)) {
                *relevance_contribution = 0.9f;
                return true;
            }
            break;

        case KG_SEARCH_OP_ENDS_WITH:
            if (str_val && ends_with(str_val, cond->value)) {
                *relevance_contribution = 0.8f;
                return true;
            }
            break;

        case KG_SEARCH_OP_REGEX: {
            if (!str_val) break;
            regex_t regex;
            if (regcomp(&regex, cond->value, REG_EXTENDED | REG_ICASE | REG_NOSUB) == 0) {
                bool match = regexec(&regex, str_val, 0, NULL, 0) == 0;
                regfree(&regex);
                if (match) {
                    *relevance_contribution = 0.7f;
                    return true;
                }
            }
            break;
        }

        case KG_SEARCH_OP_GREATER_THAN: {
            // P1-2 fix: Use strtod instead of atof for safe conversion
            char* endptr;
            errno = 0;
            double threshold = strtod(cond->value, &endptr);
            if (endptr == cond->value || errno == ERANGE) threshold = 0.0;
            if (float_val > threshold || (double)int_val > threshold) {
                *relevance_contribution = 0.6f;
                return true;
            }
            break;
        }

        case KG_SEARCH_OP_LESS_THAN: {
            // P1-2 fix: Use strtod instead of atof for safe conversion
            char* endptr;
            errno = 0;
            double threshold = strtod(cond->value, &endptr);
            if (endptr == cond->value || errno == ERANGE) threshold = 0.0;
            if (float_val < threshold || (double)int_val < threshold) {
                *relevance_contribution = 0.6f;
                return true;
            }
            break;
        }

        case KG_SEARCH_OP_BETWEEN: {
            // P1-2 fix: Use strtod instead of atof for safe conversion
            char* endptr_low;
            char* endptr_high;
            errno = 0;
            double low = strtod(cond->value, &endptr_low);
            if (endptr_low == cond->value || errno == ERANGE) low = 0.0;
            errno = 0;
            double high = strtod(cond->value2, &endptr_high);
            if (endptr_high == cond->value2 || errno == ERANGE) high = 0.0;
            double val = (float_val != 0.0f) ? float_val : (double)int_val;
            if (val >= low && val <= high) {
                *relevance_contribution = 0.7f;
                return true;
            }
            break;
        }

        case KG_SEARCH_OP_IN:
            if (str_val && value_in_list(str_val, cond->value)) {
                *relevance_contribution = 0.9f;
                return true;
            }
            break;

        case KG_SEARCH_OP_HAS_TAG:
            if (kg_metadata_has_tag(&meta->base, cond->value)) {
                *relevance_contribution = 1.0f;
                return true;
            }
            break;

        case KG_SEARCH_OP_FULL_TEXT: {
            /* Search across multiple text fields */
            float best_score = 0.0f;
            const char* fields[] = {meta->base.name, meta->base.description,
                                    meta->module_type, meta->subsystem, meta->status};
            for (int i = 0; i < 5; i++) {
                if (fields[i] && strcasestr_local(fields[i], cond->value)) {
                    float score = (float)strlen(cond->value) / (float)(strlen(fields[i]) + 1);
                    if (score > best_score) best_score = score;
                }
            }
            if (best_score > 0.0f) {
                *relevance_contribution = best_score;
                return true;
            }
            break;
        }
    }

    /* Condition didn't match - normal search behavior, not an error */
    return false;
}

/**
 * @brief Compare function for sorting results by relevance
 */
static int compare_results_by_relevance(const void* a, const void* b) {
    const internal_result_t* ra = (const internal_result_t*)a;
    const internal_result_t* rb = (const internal_result_t*)b;

    /* Higher relevance first */
    if (ra->relevance_score > rb->relevance_score) return -1;
    if (ra->relevance_score < rb->relevance_score) return 1;

    /* Then by match count */
    if (ra->match_count > rb->match_count) return -1;
    if (ra->match_count < rb->match_count) return 1;

    return 0;
}

/* ============================================================================
 * Search Index Management Implementation
 * ============================================================================ */

kg_search_index_t* kg_search_index_create(void) {
    kg_search_index_t* idx = nimcp_calloc(1, sizeof(kg_search_index_t));
    if (!idx) {
        SEARCH_LOG_ERROR("Failed to allocate search index");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_index_create: idx is NULL");
        return NULL;
    }

    /* Allocate module array */
    idx->module_capacity = 256;
    idx->modules = nimcp_calloc(idx->module_capacity, sizeof(indexed_module_t));
    if (!idx->modules) {
        nimcp_free(idx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_index_create: idx->modules is NULL");
        return NULL;
    }

    /* Allocate term array for full-text index */
    idx->term_capacity = 1024;
    idx->terms = nimcp_calloc(idx->term_capacity, sizeof(term_entry_t));
    if (!idx->terms) {
        nimcp_free(idx->modules);
        nimcp_free(idx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_index_create: idx->terms is NULL");
        return NULL;
    }

    /* Create mutex */
    idx->mutex = nimcp_platform_mutex_create();
    if (!idx->mutex) {
        nimcp_free(idx->terms);
        nimcp_free(idx->modules);
        nimcp_free(idx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_index_create: idx->mutex is NULL");
        return NULL;
    }

    SEARCH_LOG_INFO("Created search index");
    return idx;
}

void kg_search_index_destroy(kg_search_index_t* idx) {
    if (!idx) return;

    nimcp_platform_mutex_lock(idx->mutex);

    /* Free modules */
    if (idx->modules) {
        for (uint32_t i = 0; i < idx->module_count; i++) {
            if (idx->modules[i].valid && idx->modules[i].metadata) {
                kg_module_metadata_destroy(idx->modules[i].metadata);
            }
        }
        nimcp_free(idx->modules);
    }

    /* Free layers */
    for (int i = 0; i < MAX_INDEXED_LAYERS; i++) {
        if (idx->layers[i].valid && idx->layers[i].metadata) {
            kg_layer_metadata_destroy(idx->layers[i].metadata);
        }
    }

    /* Free hemispheres */
    for (int i = 0; i < MAX_INDEXED_HEMISPHERES; i++) {
        if (idx->hemispheres[i].valid && idx->hemispheres[i].metadata) {
            kg_hemisphere_metadata_destroy(idx->hemispheres[i].metadata);
        }
    }

    /* Free system metadata */
    if (idx->system_metadata) {
        kg_system_metadata_destroy(idx->system_metadata);
    }

    /* Free terms */
    if (idx->terms) {
        for (uint32_t i = 0; i < idx->term_count; i++) {
            if (idx->terms[i].postings) {
                nimcp_free(idx->terms[i].postings);
            }
        }
        nimcp_free(idx->terms);
    }

    nimcp_platform_mutex_unlock(idx->mutex);
    nimcp_platform_mutex_destroy(idx->mutex);
    nimcp_free(idx->mutex);
    idx->mutex = NULL;

    SEARCH_LOG_INFO("Destroyed search index");
    nimcp_free(idx);
}

int kg_search_index_add_module(kg_search_index_t* idx, brain_kg_node_id_t id,
                                const kg_meta_module_t* meta) {
    if (!idx || !meta) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_index_add_module: required parameter is NULL (idx, meta)");
        return -1;
    }

    nimcp_platform_mutex_lock(idx->mutex);

    /* Check capacity */
    if (idx->module_count >= idx->module_capacity) {
        /* Check for overflow before doubling capacity */
        if (idx->module_capacity > UINT32_MAX / 2) {
            SEARCH_LOG_ERROR("Module capacity overflow");
            nimcp_platform_mutex_unlock(idx->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_search_index_add_module: capacity overflow");
            return -1;
        }
        uint32_t new_capacity = idx->module_capacity * 2;
        /* Check for allocation size overflow */
        if (new_capacity > SIZE_MAX / sizeof(indexed_module_t)) {
            SEARCH_LOG_ERROR("Module array size overflow");
            nimcp_platform_mutex_unlock(idx->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_search_index_add_module: allocation size overflow");
            return -1;
        }
        indexed_module_t* new_modules = nimcp_realloc(
            idx->modules, new_capacity * sizeof(indexed_module_t)
        );
        if (!new_modules) {
            nimcp_platform_mutex_unlock(idx->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_index_add_module: realloc failed");
            return -1;
        }
        memset(&new_modules[idx->module_capacity], 0,
               (new_capacity - idx->module_capacity) * sizeof(indexed_module_t));
        idx->modules = new_modules;
        idx->module_capacity = new_capacity;
    }

    /* Clone metadata */
    kg_meta_module_t* clone = kg_module_metadata_create();
    if (!clone) {
        nimcp_platform_mutex_unlock(idx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_index_add_module: metadata clone failed");
        return -1;
    }

    /* Copy metadata fields */
    memcpy(clone, meta, sizeof(kg_meta_module_t));
    clone->base.entries = NULL;
    clone->base.entry_count = 0;
    clone->base.entry_capacity = 0;
    clone->base.tags = meta->base.tags ? nimcp_strdup(meta->base.tags) : NULL;
    clone->capabilities = meta->capabilities ? nimcp_strdup(meta->capabilities) : NULL;
    clone->dependencies = meta->dependencies ? nimcp_strdup(meta->dependencies) : NULL;

    /* Add to index */
    idx->modules[idx->module_count].node_id = id;
    idx->modules[idx->module_count].metadata = clone;
    idx->modules[idx->module_count].valid = true;
    idx->module_count++;

    nimcp_platform_mutex_unlock(idx->mutex);

    SEARCH_LOG_DEBUG("Added module %u to search index", id);
    return 0;
}

int kg_search_index_add_layer(kg_search_index_t* idx, uint8_t layer,
                               const kg_layer_metadata_t* meta) {
    if (!idx || !meta || layer >= MAX_INDEXED_LAYERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_index_add_layer: required parameter is NULL (idx, meta)");
        return -1;
    }

    nimcp_platform_mutex_lock(idx->mutex);

    /* Free existing if present */
    if (idx->layers[layer].valid && idx->layers[layer].metadata) {
        kg_layer_metadata_destroy(idx->layers[layer].metadata);
    }

    /* Clone metadata */
    kg_layer_metadata_t* clone = kg_layer_metadata_create();
    if (!clone) {
        nimcp_platform_mutex_unlock(idx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_index_add_layer: metadata clone failed");
        return -1;
    }

    memcpy(clone, meta, sizeof(kg_layer_metadata_t));
    clone->base.entries = NULL;
    clone->base.entry_count = 0;
    clone->base.entry_capacity = 0;
    clone->base.tags = meta->base.tags ? nimcp_strdup(meta->base.tags) : NULL;
    clone->module_types = meta->module_types ? nimcp_strdup(meta->module_types) : NULL;
    clone->connection_summary = meta->connection_summary ?
                                nimcp_strdup(meta->connection_summary) : NULL;

    idx->layers[layer].layer_index = layer;
    idx->layers[layer].metadata = clone;
    idx->layers[layer].valid = true;

    nimcp_platform_mutex_unlock(idx->mutex);

    SEARCH_LOG_DEBUG("Added layer %u to search index", layer);
    return 0;
}

int kg_search_index_add_hemisphere(kg_search_index_t* idx, uint8_t hemi,
                                    const kg_hemisphere_metadata_t* meta) {
    if (!idx || !meta || hemi >= MAX_INDEXED_HEMISPHERES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_index_add_hemisphere: required parameter is NULL (idx, meta)");
        return -1;
    }

    nimcp_platform_mutex_lock(idx->mutex);

    /* Free existing if present */
    if (idx->hemispheres[hemi].valid && idx->hemispheres[hemi].metadata) {
        kg_hemisphere_metadata_destroy(idx->hemispheres[hemi].metadata);
    }

    /* Clone metadata */
    kg_hemisphere_metadata_t* clone = kg_hemisphere_metadata_create();
    if (!clone) {
        nimcp_platform_mutex_unlock(idx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_index_add_hemisphere: metadata clone failed");
        return -1;
    }

    memcpy(clone, meta, sizeof(kg_hemisphere_metadata_t));
    clone->base.entries = NULL;
    clone->base.entry_count = 0;
    clone->base.entry_capacity = 0;
    clone->base.tags = meta->base.tags ? nimcp_strdup(meta->base.tags) : NULL;
    clone->layer_summary = meta->layer_summary ? nimcp_strdup(meta->layer_summary) : NULL;
    clone->cross_hemisphere_bridges = meta->cross_hemisphere_bridges ?
                                       nimcp_strdup(meta->cross_hemisphere_bridges) : NULL;

    idx->hemispheres[hemi].hemisphere_id = hemi;
    idx->hemispheres[hemi].metadata = clone;
    idx->hemispheres[hemi].valid = true;

    nimcp_platform_mutex_unlock(idx->mutex);

    SEARCH_LOG_DEBUG("Added hemisphere %u to search index", hemi);
    return 0;
}

int kg_search_index_add_system(kg_search_index_t* idx, const kg_system_metadata_t* meta) {
    if (!idx || !meta) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_index_add_system: required parameter is NULL (idx, meta)");
        return -1;
    }

    nimcp_platform_mutex_lock(idx->mutex);

    /* Free existing if present */
    if (idx->system_metadata) {
        kg_system_metadata_destroy(idx->system_metadata);
    }

    /* Clone metadata */
    kg_system_metadata_t* clone = kg_system_metadata_create();
    if (!clone) {
        nimcp_platform_mutex_unlock(idx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_index_add_system: clone is NULL");
        return -1;
    }

    memcpy(clone, meta, sizeof(kg_system_metadata_t));
    clone->base.entries = NULL;
    clone->base.entry_count = 0;
    clone->base.entry_capacity = 0;
    clone->base.tags = meta->base.tags ? nimcp_strdup(meta->base.tags) : NULL;
    clone->subsystem_status = meta->subsystem_status ?
                              nimcp_strdup(meta->subsystem_status) : NULL;

    idx->system_metadata = clone;

    nimcp_platform_mutex_unlock(idx->mutex);

    SEARCH_LOG_DEBUG("Added system metadata to search index");
    return 0;
}

int kg_search_index_rebuild(kg_search_index_t* idx) {
    if (!idx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_index_rebuild: idx is NULL");
        return -1;
    }

    /* This is a no-op in the current implementation since we don't
     * maintain separate inverted indices. Future enhancement could
     * rebuild term indices here. */

    SEARCH_LOG_INFO("Search index rebuilt");
    return 0;
}

int kg_search_index_remove_module(kg_search_index_t* idx, brain_kg_node_id_t id) {
    if (!idx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_index_remove_module: idx is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(idx->mutex);

    for (uint32_t i = 0; i < idx->module_count; i++) {
        if (idx->modules[i].valid && idx->modules[i].node_id == id) {
            kg_module_metadata_destroy(idx->modules[i].metadata);
            idx->modules[i].metadata = NULL;
            idx->modules[i].valid = false;
            nimcp_platform_mutex_unlock(idx->mutex);
            SEARCH_LOG_DEBUG("Removed module %u from search index", id);
            return 0;
        }
    }

    nimcp_platform_mutex_unlock(idx->mutex);
    /* Module not found in index - not an error, just not present */
    return -1;
}

int kg_search_index_update_module(kg_search_index_t* idx, brain_kg_node_id_t id,
                                   const kg_meta_module_t* meta) {
    if (!idx || !meta) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_index_update_module: required parameter is NULL (idx, meta)");
        return -1;
    }

    kg_search_index_remove_module(idx, id);
    return kg_search_index_add_module(idx, id, meta);
}

int kg_search_index_get_stats(const kg_search_index_t* idx,
                               uint32_t* module_count,
                               uint32_t* layer_count,
                               uint32_t* hemisphere_count,
                               uint32_t* term_count) {
    if (!idx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_index_get_stats: idx is NULL");
        return -1;
    }

    if (module_count) {
        *module_count = 0;
        for (uint32_t i = 0; i < idx->module_count; i++) {
            if (idx->modules[i].valid) (*module_count)++;
        }
    }

    if (layer_count) {
        *layer_count = 0;
        for (int i = 0; i < MAX_INDEXED_LAYERS; i++) {
            if (idx->layers[i].valid) (*layer_count)++;
        }
    }

    if (hemisphere_count) {
        *hemisphere_count = 0;
        for (int i = 0; i < MAX_INDEXED_HEMISPHERES; i++) {
            if (idx->hemispheres[i].valid) (*hemisphere_count)++;
        }
    }

    if (term_count) {
        *term_count = idx->term_count;
    }

    return 0;
}

/* ============================================================================
 * Query Execution Implementation
 * ============================================================================ */

kg_search_results_t* kg_search_execute(const kg_search_index_t* idx,
                                        const kg_search_query_t* query) {
    if (!idx || !query) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_execute: required parameter is NULL (idx, query)");
        return NULL;
    }

    uint64_t start_time = get_time_us();

    /* Allocate results container */
    kg_search_results_t* results = nimcp_calloc(1, sizeof(kg_search_results_t));
    if (!results) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_execute: results allocation failed");
        return NULL;
    }

    /* Allocate temporary results array */
    uint32_t max_results = idx->module_count + MAX_INDEXED_LAYERS +
                           MAX_INDEXED_HEMISPHERES + 1;
    internal_result_t* temp_results = nimcp_calloc(max_results, sizeof(internal_result_t));
    if (!temp_results) {
        nimcp_free(results);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_execute: temp_results allocation failed");
        return NULL;
    }

    uint32_t temp_count = 0;

    /* Lock index for reading */
    nimcp_platform_mutex_lock(((kg_search_index_t*)idx)->mutex);

    /* Search modules */
    if (query->search_modules) {
        for (uint32_t i = 0; i < idx->module_count && temp_count < max_results; i++) {
            if (!idx->modules[i].valid) continue;

            const kg_meta_module_t* meta = idx->modules[i].metadata;
            float total_relevance = 0.0f;
            int match_count = 0;
            bool all_match = true;
            bool any_match = false;

            /* Evaluate all conditions */
            for (uint32_t j = 0; j < query->condition_count; j++) {
                float contrib = 0.0f;
                bool match = evaluate_condition_module(meta, &query->conditions[j], &contrib);
                if (match) {
                    total_relevance += contrib;
                    match_count++;
                    any_match = true;
                } else {
                    all_match = false;
                }
            }

            /* Check logic: AND requires all, OR requires any */
            bool passes = (query->condition_count == 0) ||
                          (query->match_all ? all_match : any_match);

            if (passes) {
                temp_results[temp_count].node_id = idx->modules[i].node_id;
                temp_results[temp_count].level = KG_RESULT_MODULE;
                temp_results[temp_count].metadata = (kg_metadata_t*)&meta->base;
                temp_results[temp_count].relevance_score =
                    query->condition_count > 0 ?
                    total_relevance / query->condition_count : 1.0f;
                temp_results[temp_count].match_count = match_count;
                temp_count++;
            }
        }
    }

    /* TODO: Add search for layers, hemispheres, system level */
    /* For now, focus on module search as the primary use case */

    nimcp_platform_mutex_unlock(((kg_search_index_t*)idx)->mutex);

    /* Sort results by relevance */
    qsort(temp_results, temp_count, sizeof(internal_result_t), compare_results_by_relevance);

    /* Apply pagination */
    results->total_matches = temp_count;
    uint32_t start_idx = query->offset < temp_count ? query->offset : temp_count;
    uint32_t end_idx = start_idx + query->limit;
    if (end_idx > temp_count) end_idx = temp_count;
    results->result_count = end_idx - start_idx;

    if (results->result_count > 0) {
        results->results = nimcp_calloc(results->result_count, sizeof(kg_search_result_t));
        if (!results->results) {
            nimcp_free(temp_results);
            nimcp_free(results);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_execute: results array allocation failed");
            return NULL;
        }

        for (uint32_t i = 0; i < results->result_count; i++) {
            uint32_t src_idx = start_idx + i;
            results->results[i].node_id = temp_results[src_idx].node_id;
            results->results[i].level = temp_results[src_idx].level;
            results->results[i].metadata = temp_results[src_idx].metadata;
            results->results[i].relevance_score = temp_results[src_idx].relevance_score;
        }
    }

    nimcp_free(temp_results);

    results->search_time_us = get_time_us() - start_time;

    SEARCH_LOG_DEBUG("Search completed: %u matches, %u returned, %lu us",
                     results->total_matches, results->result_count,
                     (unsigned long)results->search_time_us);

    return results;
}

void kg_search_results_free(kg_search_results_t* results) {
    if (!results) return;

    if (results->results) {
        nimcp_free(results->results);
    }
    nimcp_free(results);
}

/* ============================================================================
 * Query Builder Implementation
 * ============================================================================ */

kg_search_query_t* kg_search_query_create(void) {
    kg_search_query_t* query = nimcp_calloc(1, sizeof(kg_search_query_t));
    if (!query) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_query_create: allocation failed");
        return NULL;
    }

    /* Set defaults */
    query->match_all = true;
    query->search_modules = true;
    query->search_layers = true;
    query->search_hemispheres = true;
    query->search_system = true;
    query->limit = KG_SEARCH_DEFAULT_LIMIT;
    query->sort_ascending = true;

    return query;
}

void kg_search_query_destroy(kg_search_query_t* query) {
    if (!query) return;

    if (query->conditions) {
        nimcp_free(query->conditions);
    }
    nimcp_free(query);
}

int kg_search_query_add_condition(kg_search_query_t* query, const char* field,
                                   kg_search_op_t op, const char* value) {
    if (!query || !field || !value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_query_add_condition: required parameter is NULL (query, field, value)");
        return -1;
    }
    if (query->condition_count >= KG_SEARCH_MAX_CONDITIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_search_query_add_condition: capacity exceeded");
        return -1;
    }

    /* Allocate conditions array if needed */
    if (!query->conditions) {
        query->conditions = nimcp_calloc(KG_SEARCH_MAX_CONDITIONS,
                                         sizeof(kg_search_condition_t));
        if (!query->conditions) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_query_add_condition: allocation failed");
            return -1;
        }
    }

    kg_search_condition_t* cond = &query->conditions[query->condition_count];
    strncpy(cond->field, field, KG_SEARCH_MAX_FIELD_LEN - 1);
    cond->field[KG_SEARCH_MAX_FIELD_LEN - 1] = '\0';
    cond->op = op;
    strncpy(cond->value, value, KG_SEARCH_MAX_VALUE_LEN - 1);
    cond->value[KG_SEARCH_MAX_VALUE_LEN - 1] = '\0';
    cond->value2[0] = '\0';

    query->condition_count++;
    return 0;
}

int kg_search_query_add_between(kg_search_query_t* query, const char* field,
                                 const char* value1, const char* value2) {
    if (!query || !field || !value1 || !value2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_query_add_between: required parameter is NULL (query, field, value1, value2)");
        return -1;
    }
    if (query->condition_count >= KG_SEARCH_MAX_CONDITIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_search_query_add_between: capacity exceeded");
        return -1;
    }

    if (!query->conditions) {
        query->conditions = nimcp_calloc(KG_SEARCH_MAX_CONDITIONS,
                                         sizeof(kg_search_condition_t));
        if (!query->conditions) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_query_add_between: allocation failed");
            return -1;
        }
    }

    kg_search_condition_t* cond = &query->conditions[query->condition_count];
    strncpy(cond->field, field, KG_SEARCH_MAX_FIELD_LEN - 1);
    cond->op = KG_SEARCH_OP_BETWEEN;
    strncpy(cond->value, value1, KG_SEARCH_MAX_VALUE_LEN - 1);
    strncpy(cond->value2, value2, KG_SEARCH_MAX_VALUE_LEN - 1);

    query->condition_count++;
    return 0;
}

int kg_search_query_set_scope(kg_search_query_t* query, bool modules, bool layers,
                               bool hemispheres, bool system) {
    if (!query) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_query_set_scope: query is NULL");
        return -1;
    }

    query->search_modules = modules;
    query->search_layers = layers;
    query->search_hemispheres = hemispheres;
    query->search_system = system;
    return 0;
}

int kg_search_query_set_pagination(kg_search_query_t* query,
                                    uint32_t offset, uint32_t limit) {
    if (!query) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_query_set_pagination: query is NULL");
        return -1;
    }

    query->offset = offset;
    query->limit = limit > KG_SEARCH_MAX_LIMIT ? KG_SEARCH_MAX_LIMIT : limit;
    return 0;
}

int kg_search_query_set_sort(kg_search_query_t* query, const char* field, bool ascending) {
    if (!query) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_query_set_sort: query is NULL");
        return -1;
    }

    if (field) {
        strncpy(query->sort_field, field, KG_SEARCH_MAX_FIELD_LEN - 1);
        query->sort_field[KG_SEARCH_MAX_FIELD_LEN - 1] = '\0';
    } else {
        query->sort_field[0] = '\0';
    }
    query->sort_ascending = ascending;
    return 0;
}

int kg_search_query_set_logic(kg_search_query_t* query, bool match_all) {
    if (!query) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_query_set_logic: query is NULL");
        return -1;
    }
    query->match_all = match_all;
    return 0;
}

int kg_search_query_clear_conditions(kg_search_query_t* query) {
    if (!query) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_query_clear_conditions: query is NULL");
        return -1;
    }
    query->condition_count = 0;
    return 0;
}

kg_search_query_t* kg_search_query_clone(const kg_search_query_t* query) {
    if (!query) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_query_clone: query is NULL");
        return NULL;
    }

    kg_search_query_t* clone = nimcp_calloc(1, sizeof(kg_search_query_t));
    if (!clone) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_query_clone: allocation failed");
        return NULL;
    }

    memcpy(clone, query, sizeof(kg_search_query_t));
    clone->conditions = NULL;

    if (query->conditions && query->condition_count > 0) {
        clone->conditions = nimcp_calloc(KG_SEARCH_MAX_CONDITIONS,
                                         sizeof(kg_search_condition_t));
        if (!clone->conditions) {
            nimcp_free(clone);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_search_query_clone: clone->conditions is NULL");
            return NULL;
        }
        memcpy(clone->conditions, query->conditions,
               query->condition_count * sizeof(kg_search_condition_t));
    }

    return clone;
}

/* ============================================================================
 * Convenience Search Functions Implementation
 * ============================================================================ */

kg_search_results_t* kg_search_by_tag(const kg_search_index_t* idx, const char* tag) {
    if (!idx || !tag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_by_tag: required parameter is NULL (idx, tag)");
        return NULL;
    }

    kg_search_query_t* query = kg_search_query_create();
    if (!query) return NULL;

    kg_search_query_add_condition(query, "tags", KG_SEARCH_OP_HAS_TAG, tag);
    kg_search_query_set_scope(query, true, false, false, false);

    kg_search_results_t* results = kg_search_execute(idx, query);
    kg_search_query_destroy(query);
    return results;
}

kg_search_results_t* kg_search_by_type(const kg_search_index_t* idx,
                                        const char* module_type) {
    if (!idx || !module_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_by_type: required parameter is NULL (idx, module_type)");
        return NULL;
    }

    kg_search_query_t* query = kg_search_query_create();
    if (!query) return NULL;

    kg_search_query_add_condition(query, "module_type", KG_SEARCH_OP_EQUALS, module_type);
    kg_search_query_set_scope(query, true, false, false, false);

    kg_search_results_t* results = kg_search_execute(idx, query);
    kg_search_query_destroy(query);
    return results;
}

kg_search_results_t* kg_search_by_subsystem(const kg_search_index_t* idx,
                                             const char* subsystem) {
    if (!idx || !subsystem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_by_subsystem: required parameter is NULL (idx, subsystem)");
        return NULL;
    }

    kg_search_query_t* query = kg_search_query_create();
    if (!query) return NULL;

    kg_search_query_add_condition(query, "subsystem", KG_SEARCH_OP_EQUALS, subsystem);
    kg_search_query_set_scope(query, true, false, false, false);

    kg_search_results_t* results = kg_search_execute(idx, query);
    kg_search_query_destroy(query);
    return results;
}

kg_search_results_t* kg_search_full_text(const kg_search_index_t* idx,
                                          const char* query_text) {
    if (!idx || !query_text) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_full_text: required parameter is NULL (idx, query_text)");
        return NULL;
    }

    kg_search_query_t* query = kg_search_query_create();
    if (!query) return NULL;

    kg_search_query_add_condition(query, "", KG_SEARCH_OP_FULL_TEXT, query_text);

    kg_search_results_t* results = kg_search_execute(idx, query);
    kg_search_query_destroy(query);
    return results;
}

kg_search_results_t* kg_search_by_health(const kg_search_index_t* idx,
                                          float min_health, float max_health) {
    if (!idx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_by_health: idx is NULL");
        return NULL;
    }

    kg_search_query_t* query = kg_search_query_create();
    if (!query) return NULL;

    char min_str[32], max_str[32];
    snprintf(min_str, sizeof(min_str), "%.4f", min_health);
    snprintf(max_str, sizeof(max_str), "%.4f", max_health);

    kg_search_query_add_between(query, "health_score", min_str, max_str);
    kg_search_query_set_scope(query, true, false, false, false);

    kg_search_results_t* results = kg_search_execute(idx, query);
    kg_search_query_destroy(query);
    return results;
}

kg_search_results_t* kg_search_by_hemisphere(const kg_search_index_t* idx,
                                              uint8_t hemisphere) {
    if (!idx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_by_hemisphere: idx is NULL");
        return NULL;
    }

    kg_search_query_t* query = kg_search_query_create();
    if (!query) return NULL;

    char hemi_str[8];
    snprintf(hemi_str, sizeof(hemi_str), "%u", hemisphere);

    kg_search_query_add_condition(query, "hemisphere", KG_SEARCH_OP_EQUALS, hemi_str);
    kg_search_query_set_scope(query, true, false, false, false);

    kg_search_results_t* results = kg_search_execute(idx, query);
    kg_search_query_destroy(query);
    return results;
}

kg_search_results_t* kg_search_by_layer(const kg_search_index_t* idx, uint8_t layer) {
    if (!idx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_by_layer: idx is NULL");
        return NULL;
    }

    kg_search_query_t* query = kg_search_query_create();
    if (!query) return NULL;

    char layer_str[8];
    snprintf(layer_str, sizeof(layer_str), "%u", layer);

    kg_search_query_add_condition(query, "cortical_layer", KG_SEARCH_OP_EQUALS, layer_str);
    kg_search_query_set_scope(query, true, false, false, false);

    kg_search_results_t* results = kg_search_execute(idx, query);
    kg_search_query_destroy(query);
    return results;
}

kg_search_results_t* kg_search_by_status(const kg_search_index_t* idx, const char* status) {
    if (!idx || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_search_by_status: required parameter is NULL (idx, status)");
        return NULL;
    }

    kg_search_query_t* query = kg_search_query_create();
    if (!query) return NULL;

    kg_search_query_add_condition(query, "status", KG_SEARCH_OP_EQUALS, status);
    kg_search_query_set_scope(query, true, false, false, false);

    kg_search_results_t* results = kg_search_execute(idx, query);
    kg_search_query_destroy(query);
    return results;
}

/* ============================================================================
 * String Conversion Utilities Implementation
 * ============================================================================ */

const char* kg_search_op_to_string(kg_search_op_t op) {
    switch (op) {
        case KG_SEARCH_OP_EQUALS:       return "EQUALS";
        case KG_SEARCH_OP_NOT_EQUALS:   return "NOT_EQUALS";
        case KG_SEARCH_OP_CONTAINS:     return "CONTAINS";
        case KG_SEARCH_OP_STARTS_WITH:  return "STARTS_WITH";
        case KG_SEARCH_OP_ENDS_WITH:    return "ENDS_WITH";
        case KG_SEARCH_OP_REGEX:        return "REGEX";
        case KG_SEARCH_OP_GREATER_THAN: return "GREATER_THAN";
        case KG_SEARCH_OP_LESS_THAN:    return "LESS_THAN";
        case KG_SEARCH_OP_BETWEEN:      return "BETWEEN";
        case KG_SEARCH_OP_IN:           return "IN";
        case KG_SEARCH_OP_HAS_TAG:      return "HAS_TAG";
        case KG_SEARCH_OP_FULL_TEXT:    return "FULL_TEXT";
        default:                        return "UNKNOWN";
    }
}

const char* kg_search_result_level_to_string(kg_search_result_level_t level) {
    switch (level) {
        case KG_RESULT_MODULE:     return "MODULE";
        case KG_RESULT_LAYER:      return "LAYER";
        case KG_RESULT_HEMISPHERE: return "HEMISPHERE";
        case KG_RESULT_SYSTEM:     return "SYSTEM";
        default:                   return "UNKNOWN";
    }
}
