/**
 * @file nimcp_hyperthymesia.c
 * @brief Implementation of superhuman autobiographical memory module
 *
 * WHAT: Provides hyperthymesia-like perfect autobiographical memory recall
 * WHY:  Enable superhuman episodic memory capabilities
 * HOW:  Hierarchical temporal encoding, emotional tagging, vivid re-experiencing
 *
 * @version Phase T12: Superhuman Enhancement Modules
 * @date 2026-01-13
 */

#include "superhuman/nimcp_hyperthymesia.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hyperthymesia)

/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define HYPERTHYMESIA_LOG_MODULE "HYPERTHYMESIA"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Temporal index node for hierarchical date indexing
 */
typedef struct temporal_node {
    uint64_t* memory_ids;                /**< Memory IDs at this node */
    uint32_t memory_count;
    uint32_t capacity;
    struct temporal_node* children;      /**< Child nodes (temporal subdivisions) */
    uint32_t child_count;
} temporal_node_t;

/**
 * @brief Memory entry in storage
 */
typedef struct memory_entry {
    autobiographical_memory_t memory;
    struct memory_entry* hash_next;      /**< Hash table collision chain */
} memory_entry_t;

/**
 * @brief Internal module structure
 */
struct hyperthymesia_module {
    /* Configuration */
    hyperthymesia_config_t config;

    /* Memory storage (hash table by ID) */
    memory_entry_t** memory_store;
    uint32_t store_capacity;
    uint64_t memory_count;
    uint64_t next_memory_id;

    /* Hierarchical temporal index [year][month][day][hour] */
    temporal_node_t* year_index;
    uint32_t index_year_base;            /**< Base year for index */
    uint32_t index_year_count;           /**< Years in index */

    /* Callbacks */
    hyperthymesia_encode_callback_t encode_callback;
    void* encode_user_data;
    hyperthymesia_reexperience_callback_t reexperience_callback;
    void* reexperience_user_data;
    hyperthymesia_navigation_callback_t navigation_callback;
    void* navigation_user_data;

    /* State */
    hyperthymesia_status_t status;
    hyperthymesia_error_t last_error;

    /* Statistics */
    hyperthymesia_stats_t stats;
};

/* P3-HT-1: Named constants for vividness thresholds */
#define VIVIDNESS_THRESHOLD_EIDETIC   0.95f
#define VIVIDNESS_THRESHOLD_VIVID     0.80f
#define VIVIDNESS_THRESHOLD_CLEAR     0.60f
#define VIVIDNESS_THRESHOLD_MODERATE  0.40f

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Set error state
 */
static void set_error(hyperthymesia_module_t* module, hyperthymesia_error_t error) {
    if (!module) return;
    module->last_error = error;
    if (error != HYPERTHYMESIA_ERROR_NONE) {
        module->status = HYPERTHYMESIA_STATUS_ERROR;
        LOG_ERROR("[%s] Error: %d", HYPERTHYMESIA_LOG_MODULE, error);
    }
}

/**
 * @brief Hash function for memory ID
 */
static uint32_t hash_memory_id(uint64_t memory_id, uint32_t capacity) {
    return (uint32_t)(memory_id % capacity);
}

/**
 * @brief Convert datetime to Unix timestamp
 */
static uint64_t datetime_to_timestamp(const hyperthymesia_datetime_t* dt) {
    struct tm tm_info = {0};
    tm_info.tm_year = dt->year - 1900;
    tm_info.tm_mon = dt->month - 1;
    tm_info.tm_mday = dt->day;
    tm_info.tm_hour = dt->hour;
    tm_info.tm_min = dt->minute;
    tm_info.tm_sec = dt->second;
    return (uint64_t)mktime(&tm_info) * 1000 + dt->millisecond;
}

/**
 * @brief Get year index position
 */
static int32_t get_year_index(hyperthymesia_module_t* module, uint16_t year) {
    /* P2-HT-4: Year out-of-range is a normal query miss, not an error.
     * Callers check for -1 return and handle gracefully. */
    if (year < module->index_year_base) {
        return -1;
    }
    int32_t idx = year - module->index_year_base;
    if ((uint32_t)idx >= module->index_year_count) {
        return -1;
    }
    return idx;
}

/**
 * @brief Create temporal node
 */
static temporal_node_t* create_temporal_node(uint32_t initial_capacity) {
    temporal_node_t* node = nimcp_calloc(1, sizeof(temporal_node_t));
    if (!node) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(temporal_node_t),
                           "create_temporal_node: Failed to allocate node");
        return NULL;
    }

    if (initial_capacity > 0) {
        node->memory_ids = nimcp_calloc(initial_capacity, sizeof(uint64_t));
        if (!node->memory_ids) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, initial_capacity * sizeof(uint64_t),
                               "create_temporal_node: Failed to allocate memory_ids");
            nimcp_free(node);
            return NULL;
        }
        node->capacity = initial_capacity;
    }
    return node;
}

/**
 * @brief Add memory ID to temporal node
 */
static bool add_to_temporal_node(temporal_node_t* node, uint64_t memory_id) {
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "add_to_temporal_node: NULL node pointer");
        return false;
    }

    if (node->memory_count >= node->capacity) {
        uint32_t new_capacity = node->capacity == 0 ? 16 : node->capacity * 2;
        uint64_t* new_ids = nimcp_realloc(node->memory_ids, new_capacity * sizeof(uint64_t));
        if (!new_ids) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, new_capacity * sizeof(uint64_t),
                               "add_to_temporal_node: Failed to expand memory_ids");
            return false;
        }
        node->memory_ids = new_ids;
        node->capacity = new_capacity;
    }

    node->memory_ids[node->memory_count++] = memory_id;
    return true;
}

/**
 * @brief Free temporal node recursively
 */
static void free_temporal_node(temporal_node_t* node) {
    if (!node) return;

    if (node->memory_ids) nimcp_free(node->memory_ids);

    for (uint32_t i = 0; i < node->child_count; i++) {
        free_temporal_node(&node->children[i]);
    }
    if (node->children) nimcp_free(node->children);
}

/**
 * @brief Copy memory context
 */
static bool copy_memory_context(memory_context_t* dst, const memory_context_t* src) {
    if (!dst || !src) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "copy_memory_context: NULL parameter");
        return false;
    }

    memset(dst, 0, sizeof(memory_context_t));

    if (src->spatial_context && src->spatial_dim > 0) {
        dst->spatial_context = nimcp_calloc(src->spatial_dim, sizeof(float));
        if (!dst->spatial_context) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, src->spatial_dim * sizeof(float),
                               "copy_memory_context: Failed to allocate spatial_context");
            return false;
        }
        memcpy(dst->spatial_context, src->spatial_context, src->spatial_dim * sizeof(float));
        dst->spatial_dim = src->spatial_dim;
    }

    if (src->social_context && src->social_dim > 0) {
        dst->social_context = nimcp_calloc(src->social_dim, sizeof(float));
        if (!dst->social_context) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, src->social_dim * sizeof(float),
                               "copy_memory_context: Failed to allocate social_context");
            nimcp_free(dst->spatial_context);
            dst->spatial_context = NULL;
            return false;
        }
        memcpy(dst->social_context, src->social_context, src->social_dim * sizeof(float));
        dst->social_dim = src->social_dim;
    }

    if (src->activity_context && src->activity_dim > 0) {
        dst->activity_context = nimcp_calloc(src->activity_dim, sizeof(float));
        if (!dst->activity_context) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, src->activity_dim * sizeof(float),
                               "copy_memory_context: Failed to allocate activity_context");
            nimcp_free(dst->spatial_context);
            nimcp_free(dst->social_context);
            dst->spatial_context = NULL;
            dst->social_context = NULL;
            return false;
        }
        memcpy(dst->activity_context, src->activity_context, src->activity_dim * sizeof(float));
        dst->activity_dim = src->activity_dim;
    }

    if (src->semantic_context && src->semantic_dim > 0) {
        dst->semantic_context = nimcp_calloc(src->semantic_dim, sizeof(float));
        if (!dst->semantic_context) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, src->semantic_dim * sizeof(float),
                               "copy_memory_context: Failed to allocate semantic_context");
            nimcp_free(dst->spatial_context);
            nimcp_free(dst->social_context);
            nimcp_free(dst->activity_context);
            dst->spatial_context = NULL;
            dst->social_context = NULL;
            dst->activity_context = NULL;
            return false;
        }
        memcpy(dst->semantic_context, src->semantic_context, src->semantic_dim * sizeof(float));
        dst->semantic_dim = src->semantic_dim;
    }

    return true;
}

/**
 * @brief Free memory context
 */
static void free_memory_context(memory_context_t* ctx) {
    if (!ctx) return;
    if (ctx->spatial_context) nimcp_free(ctx->spatial_context);
    if (ctx->social_context) nimcp_free(ctx->social_context);
    if (ctx->activity_context) nimcp_free(ctx->activity_context);
    if (ctx->semantic_context) nimcp_free(ctx->semantic_context);
    memset(ctx, 0, sizeof(memory_context_t));
}

/**
 * @brief Free memory entry
 */
static void free_memory_entry(memory_entry_t* entry) {
    if (!entry) return;

    if (entry->memory.core_features) nimcp_free(entry->memory.core_features);
    free_memory_context(&entry->memory.context);

    if (entry->memory.sensory_traces) {
        for (uint32_t i = 0; i < entry->memory.trace_count; i++) {
            if (entry->memory.sensory_traces[i].features) {
                nimcp_free(entry->memory.sensory_traces[i].features);
            }
        }
        nimcp_free(entry->memory.sensory_traces);
    }

    if (entry->memory.linked_memories) nimcp_free(entry->memory.linked_memories);

    nimcp_free(entry);
}

/**
 * @brief Index memory by date
 */
static bool index_memory_by_date(
    hyperthymesia_module_t* module,
    uint64_t memory_id,
    const hyperthymesia_datetime_t* timestamp
) {
    int32_t year_idx = get_year_index(module, timestamp->year);
    if (year_idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "index_memory_by_date: validation failed");
        return false;
    }

    temporal_node_t* year_node = &module->year_index[year_idx];

    /* Ensure month children exist */
    if (!year_node->children) {
        year_node->children = nimcp_calloc(12, sizeof(temporal_node_t));
        if (!year_node->children) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "index_memory_by_date: year_node->children is NULL");
            return false;
        }
        year_node->child_count = 12;
    }

    if (timestamp->month < 1 || timestamp->month > 12) return false;
    temporal_node_t* month_node = &year_node->children[timestamp->month - 1];

    /* Ensure day children exist */
    if (!month_node->children) {
        month_node->children = nimcp_calloc(31, sizeof(temporal_node_t));
        if (!month_node->children) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "index_memory_by_date: month_node->children is NULL");
            return false;
        }
        month_node->child_count = 31;
    }

    if (timestamp->day < 1 || timestamp->day > 31) return false;
    temporal_node_t* day_node = &month_node->children[timestamp->day - 1];

    /* Add memory ID to day node */
    return add_to_temporal_node(day_node, memory_id);
}

/**
 * @brief Calculate vividness from encoding strength
 */
static memory_vividness_t calculate_vividness(float strength) {
    /* P3-HT-1: Use named constants instead of magic numbers */
    if (strength >= VIVIDNESS_THRESHOLD_EIDETIC)  return VIVIDNESS_EIDETIC;
    if (strength >= VIVIDNESS_THRESHOLD_VIVID)    return VIVIDNESS_VIVID;
    if (strength >= VIVIDNESS_THRESHOLD_CLEAR)    return VIVIDNESS_CLEAR;
    if (strength >= VIVIDNESS_THRESHOLD_MODERATE)  return VIVIDNESS_MODERATE;
    return VIVIDNESS_FAINT;
}

/**
 * @brief Zeller's congruence for day of week
 */
static int8_t zeller_day_of_week(uint16_t year, uint8_t month, uint8_t day) {
    /* Adjust for January and February */
    int y = year;
    int m = month;
    if (m < 3) {
        m += 12;
        y -= 1;
    }

    int k = y % 100;
    int j = y / 100;

    int h = (day + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;

    /* Convert to 0=Sunday format */
    return (int8_t)((h + 6) % 7);
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hyperthymesia_config_t hyperthymesia_default_config(void) {
    hyperthymesia_config_t config;
    memset(&config, 0, sizeof(config));

    config.memory_capacity = HYPERTHYMESIA_DEFAULT_MEMORY_CAPACITY;
    config.timeline_years = HYPERTHYMESIA_DEFAULT_TIMELINE_YEARS;
    config.context_dim = HYPERTHYMESIA_DEFAULT_CONTEXT_DIM;
    config.sensory_dim = HYPERTHYMESIA_DEFAULT_SENSORY_DIM;
    config.emotional_dim = HYPERTHYMESIA_DEFAULT_EMOTIONAL_DIM;
    config.encoding_strength = HYPERTHYMESIA_DEFAULT_ENCODING_STRENGTH;
    config.decay_rate = HYPERTHYMESIA_DEFAULT_DECAY_RATE;
    config.consolidation_threshold = 0.7f;
    config.reexperience_depth = HYPERTHYMESIA_DEFAULT_REEXPERIENCE_DEPTH;
    config.vividness_threshold = 0.3f;
    config.enable_full_immersion = true;
    config.enable_date_indexing = true;
    config.enable_emotional_tagging = true;
    config.enable_sensory_traces = true;
    config.enable_narrative_binding = true;
    config.enable_parallel_retrieval = false;
    config.enable_context_matching = true;
    config.max_concurrent_recalls = 4;

    return config;
}

hyperthymesia_module_t* hyperthymesia_create(const hyperthymesia_config_t* config) {
    LOG_INFO("[%s] Creating hyperthymesia module", HYPERTHYMESIA_LOG_MODULE);

    hyperthymesia_module_t* module = nimcp_calloc(1, sizeof(hyperthymesia_module_t));
    if (!module) {
        LOG_ERROR("[%s] Failed to allocate module", HYPERTHYMESIA_LOG_MODULE);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(hyperthymesia_module_t),
                           "hyperthymesia_create: Failed to allocate module");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        module->config = *config;
    } else {
        module->config = hyperthymesia_default_config();
    }

    /* Initialize memory storage */
    module->store_capacity = module->config.memory_capacity;
    module->memory_store = nimcp_calloc(module->store_capacity, sizeof(memory_entry_t*));
    if (!module->memory_store) {
        LOG_ERROR("[%s] Failed to allocate memory store", HYPERTHYMESIA_LOG_MODULE);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           module->store_capacity * sizeof(memory_entry_t*),
                           "hyperthymesia_create: Failed to allocate memory store");
        hyperthymesia_destroy(module);
        return NULL;
    }
    module->next_memory_id = 1;

    /* Initialize temporal index */
    if (module->config.enable_date_indexing) {
        module->index_year_count = module->config.timeline_years;
        module->index_year_base = 1970;  /* Unix epoch as base */
        module->year_index = nimcp_calloc(module->index_year_count, sizeof(temporal_node_t));
        if (!module->year_index) {
            LOG_ERROR("[%s] Failed to allocate temporal index", HYPERTHYMESIA_LOG_MODULE);
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                               module->index_year_count * sizeof(temporal_node_t),
                               "hyperthymesia_create: Failed to allocate temporal index");
            hyperthymesia_destroy(module);
            return NULL;
        }
        LOG_DEBUG("[%s] Temporal index created: %u years from %u",
                  HYPERTHYMESIA_LOG_MODULE, module->index_year_count, module->index_year_base);
    }

    /* Initialize statistics */
    memset(&module->stats, 0, sizeof(hyperthymesia_stats_t));
    module->stats.earliest.year = 9999;
    module->stats.earliest.month = 12;
    module->stats.earliest.day = 31;

    module->status = HYPERTHYMESIA_STATUS_IDLE;
    module->last_error = HYPERTHYMESIA_ERROR_NONE;

    LOG_INFO("[%s] Hyperthymesia module created (capacity=%u)",
             HYPERTHYMESIA_LOG_MODULE, module->config.memory_capacity);

    return module;
}

void hyperthymesia_destroy(hyperthymesia_module_t* module) {
    if (!module) return;

    LOG_INFO("[%s] Destroying hyperthymesia module", HYPERTHYMESIA_LOG_MODULE);

    /* Free memory store */
    if (module->memory_store) {
        for (uint32_t i = 0; i < module->store_capacity; i++) {
            memory_entry_t* entry = module->memory_store[i];
            while (entry) {
                memory_entry_t* next = entry->hash_next;
                free_memory_entry(entry);
                entry = next;
            }
        }
        nimcp_free(module->memory_store);
    }

    /* Free temporal index */
    if (module->year_index) {
        for (uint32_t i = 0; i < module->index_year_count; i++) {
            free_temporal_node(&module->year_index[i]);
        }
        nimcp_free(module->year_index);
    }

    nimcp_free(module);
    LOG_DEBUG("[%s] Module destroyed", HYPERTHYMESIA_LOG_MODULE);
}

bool hyperthymesia_reset(hyperthymesia_module_t* module) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperthymesia_reset: NULL module pointer");
        return false;
    }

    LOG_DEBUG("[%s] Resetting module", HYPERTHYMESIA_LOG_MODULE);

    /* Clear memory store */
    for (uint32_t i = 0; i < module->store_capacity; i++) {
        memory_entry_t* entry = module->memory_store[i];
        while (entry) {
            memory_entry_t* next = entry->hash_next;
            free_memory_entry(entry);
            entry = next;
        }
        module->memory_store[i] = NULL;
    }

    /* Reset temporal index */
    if (module->year_index) {
        for (uint32_t i = 0; i < module->index_year_count; i++) {
            free_temporal_node(&module->year_index[i]);
        }
        memset(module->year_index, 0, module->index_year_count * sizeof(temporal_node_t));
    }

    module->memory_count = 0;
    module->next_memory_id = 1;

    /* Reset statistics */
    memset(&module->stats, 0, sizeof(hyperthymesia_stats_t));
    module->stats.earliest.year = 9999;

    module->status = HYPERTHYMESIA_STATUS_IDLE;
    module->last_error = HYPERTHYMESIA_ERROR_NONE;

    return true;
}

/*=============================================================================
 * EPISODIC MEMORY ENCODING
 *===========================================================================*/

uint64_t hyperthymesia_encode_memory(
    hyperthymesia_module_t* module,
    const hyperthymesia_datetime_t* timestamp,
    autobiographical_type_t type,
    const float* core_features,
    uint32_t feature_count,
    const memory_context_t* context,
    const emotional_state_t* emotion
) {
    if (!module || !timestamp || !core_features || feature_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "hyperthymesia_encode_memory: Invalid parameters");
        set_error(module, HYPERTHYMESIA_ERROR_INVALID_INPUT);
        return 0;
    }

    if (module->memory_count >= module->config.memory_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE,
                              "hyperthymesia_encode_memory: Memory capacity full");
        set_error(module, HYPERTHYMESIA_ERROR_MEMORY_FULL);
        return 0;
    }

    module->status = HYPERTHYMESIA_STATUS_ENCODING;
    LOG_DEBUG("[%s] Encoding memory (date=%04u-%02u-%02u, features=%u)",
              HYPERTHYMESIA_LOG_MODULE, timestamp->year, timestamp->month,
              timestamp->day, feature_count);

    /* Create memory entry */
    memory_entry_t* entry = nimcp_calloc(1, sizeof(memory_entry_t));
    if (!entry) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(memory_entry_t),
                           "hyperthymesia_encode_memory: Failed to allocate memory entry");
        set_error(module, HYPERTHYMESIA_ERROR_ENCODING_FAILED);
        return 0;
    }

    uint64_t memory_id = module->next_memory_id++;
    entry->memory.memory_id = memory_id;
    entry->memory.timestamp = *timestamp;
    entry->memory.type = type;

    /* Copy core features */
    entry->memory.core_features = nimcp_calloc(feature_count, sizeof(float));
    if (!entry->memory.core_features) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, feature_count * sizeof(float),
                           "hyperthymesia_encode_memory: Failed to allocate core_features");
        free_memory_entry(entry);
        set_error(module, HYPERTHYMESIA_ERROR_ENCODING_FAILED);
        return 0;
    }
    memcpy(entry->memory.core_features, core_features, feature_count * sizeof(float));
    entry->memory.core_feature_count = feature_count;

    /* Copy context */
    if (context) {
        if (!copy_memory_context(&entry->memory.context, context)) {
            free_memory_entry(entry);
            set_error(module, HYPERTHYMESIA_ERROR_ENCODING_FAILED);
            return 0;
        }
    }

    /* Set emotional state */
    if (emotion) {
        entry->memory.emotion = *emotion;
    }

    /* Calculate encoding strength with emotional enhancement */
    float emotional_boost = 0.0f;
    if (emotion && module->config.enable_emotional_tagging) {
        emotional_boost = fabsf(emotion->valence) * 0.1f + emotion->arousal * 0.1f +
                         emotion->significance * 0.15f;
    }
    entry->memory.encoding_strength = fminf(1.0f, module->config.encoding_strength + emotional_boost);
    entry->memory.current_strength = entry->memory.encoding_strength;
    entry->memory.vividness = calculate_vividness(entry->memory.encoding_strength);

    /* Index by date */
    if (module->config.enable_date_indexing) {
        if (!index_memory_by_date(module, memory_id, timestamp)) {
            LOG_WARNING("[%s] Failed to index memory by date", HYPERTHYMESIA_LOG_MODULE);
        }
    }

    /* Store in hash table */
    uint32_t hash_idx = hash_memory_id(memory_id, module->store_capacity);
    entry->hash_next = module->memory_store[hash_idx];
    module->memory_store[hash_idx] = entry;
    module->memory_count++;

    /* Update statistics */
    module->stats.total_memories++;
    module->stats.active_memories = module->memory_count;
    module->stats.encodings_performed++;

    /* Update temporal coverage */
    if (hyperthymesia_compare_datetime(timestamp, &module->stats.earliest) < 0) {
        module->stats.earliest = *timestamp;
    }
    if (hyperthymesia_compare_datetime(timestamp, &module->stats.latest) > 0) {
        module->stats.latest = *timestamp;
    }

    /* Invoke callback */
    if (module->encode_callback) {
        module->encode_callback(memory_id, entry->memory.encoding_strength, module->encode_user_data);
    }

    module->status = HYPERTHYMESIA_STATUS_IDLE;
    LOG_DEBUG("[%s] Memory encoded (id=%lu, strength=%.3f, vividness=%d)",
              HYPERTHYMESIA_LOG_MODULE, (unsigned long)memory_id,
              entry->memory.encoding_strength, entry->memory.vividness);

    return memory_id;
}

bool hyperthymesia_add_sensory_trace(
    hyperthymesia_module_t* module,
    uint64_t memory_id,
    const sensory_trace_t* trace
) {
    if (!module || memory_id == 0 || !trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "hyperthymesia_add_sensory_trace: Invalid parameters");
        set_error(module, HYPERTHYMESIA_ERROR_INVALID_INPUT);
        return false;
    }

    if (!module->config.enable_sensory_traces) {
        return true;  /* Silently succeed if disabled */
    }

    /* Find memory entry */
    uint32_t hash_idx = hash_memory_id(memory_id, module->store_capacity);
    memory_entry_t* entry = module->memory_store[hash_idx];
    while (entry && entry->memory.memory_id != memory_id) {
        entry = entry->hash_next;
    }

    if (!entry) {
        set_error(module, HYPERTHYMESIA_ERROR_DATE_NOT_FOUND);
        return false;
    }

    /* Expand sensory traces array */
    uint32_t new_count = entry->memory.trace_count + 1;
    sensory_trace_t* new_traces = nimcp_realloc(
        entry->memory.sensory_traces,
        new_count * sizeof(sensory_trace_t)
    );
    if (!new_traces) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, new_count * sizeof(sensory_trace_t),
                           "hyperthymesia_add_sensory_trace: Failed to expand traces");
        set_error(module, HYPERTHYMESIA_ERROR_ENCODING_FAILED);
        return false;
    }
    entry->memory.sensory_traces = new_traces;

    /* Copy trace */
    sensory_trace_t* new_trace = &entry->memory.sensory_traces[entry->memory.trace_count];
    new_trace->modality = trace->modality;
    new_trace->intensity = trace->intensity;
    new_trace->clarity = trace->clarity;
    new_trace->feature_count = trace->feature_count;

    if (trace->features && trace->feature_count > 0) {
        new_trace->features = nimcp_calloc(trace->feature_count, sizeof(float));
        if (!new_trace->features) {
            set_error(module, HYPERTHYMESIA_ERROR_ENCODING_FAILED);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hyperthymesia_add_sensory_trace: new_trace->features is NULL");
            return false;
        }
        memcpy(new_trace->features, trace->features, trace->feature_count * sizeof(float));
    } else {
        new_trace->features = NULL;
    }

    entry->memory.trace_count = new_count;

    LOG_DEBUG("[%s] Sensory trace added (memory=%lu, modality=%d)",
              HYPERTHYMESIA_LOG_MODULE, (unsigned long)memory_id, trace->modality);

    return true;
}

uint64_t hyperthymesia_encode_flashbulb(
    hyperthymesia_module_t* module,
    const hyperthymesia_datetime_t* timestamp,
    const float* core_features,
    uint32_t feature_count,
    const emotional_state_t* emotion,
    const char* narrative_tag
) {
    if (!module || !timestamp || !core_features || feature_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "hyperthymesia_encode_flashbulb: Invalid parameters");
        set_error(module, HYPERTHYMESIA_ERROR_INVALID_INPUT);
        return 0;
    }

    LOG_DEBUG("[%s] Encoding flashbulb memory", HYPERTHYMESIA_LOG_MODULE);

    /* Create enhanced emotional state for flashbulb */
    emotional_state_t enhanced_emotion;
    if (emotion) {
        enhanced_emotion = *emotion;
    } else {
        memset(&enhanced_emotion, 0, sizeof(emotional_state_t));
    }
    enhanced_emotion.significance = fmaxf(enhanced_emotion.significance, 0.9f);
    enhanced_emotion.arousal = fmaxf(enhanced_emotion.arousal, 0.8f);

    /* Encode as flashbulb type */
    uint64_t memory_id = hyperthymesia_encode_memory(
        module, timestamp, AUTOBIO_TYPE_FLASHBULB,
        core_features, feature_count, NULL, &enhanced_emotion
    );

    if (memory_id == 0) return 0;

    /* Set narrative tag */
    if (narrative_tag) {
        uint32_t hash_idx = hash_memory_id(memory_id, module->store_capacity);
        memory_entry_t* entry = module->memory_store[hash_idx];
        while (entry && entry->memory.memory_id != memory_id) {
            entry = entry->hash_next;
        }
        if (entry) {
            strncpy(entry->memory.narrative_tag, narrative_tag,
                    sizeof(entry->memory.narrative_tag) - 1);
            entry->memory.narrative_tag[sizeof(entry->memory.narrative_tag) - 1] = '\0';
        }
    }

    module->stats.flashbulb_memories++;

    LOG_DEBUG("[%s] Flashbulb memory encoded (id=%lu)", HYPERTHYMESIA_LOG_MODULE,
              (unsigned long)memory_id);

    return memory_id;
}

bool hyperthymesia_link_memories(
    hyperthymesia_module_t* module,
    uint64_t memory_id_1,
    uint64_t memory_id_2,
    const char* narrative_tag
) {
    if (!module || memory_id_1 == 0 || memory_id_2 == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "hyperthymesia_link_memories: Invalid parameters");
        set_error(module, HYPERTHYMESIA_ERROR_INVALID_INPUT);
        return false;
    }

    if (!module->config.enable_narrative_binding) return true;

    /* Find both entries */
    memory_entry_t* entry1 = NULL;
    memory_entry_t* entry2 = NULL;

    uint32_t hash1 = hash_memory_id(memory_id_1, module->store_capacity);
    entry1 = module->memory_store[hash1];
    while (entry1 && entry1->memory.memory_id != memory_id_1) {
        entry1 = entry1->hash_next;
    }

    uint32_t hash2 = hash_memory_id(memory_id_2, module->store_capacity);
    entry2 = module->memory_store[hash2];
    while (entry2 && entry2->memory.memory_id != memory_id_2) {
        entry2 = entry2->hash_next;
    }

    if (!entry1 || !entry2) {
        set_error(module, HYPERTHYMESIA_ERROR_DATE_NOT_FOUND);
        return false;
    }

    /* Add link to entry1 */
    uint64_t* new_links1 = nimcp_realloc(
        entry1->memory.linked_memories,
        (entry1->memory.link_count + 1) * sizeof(uint64_t)
    );
    if (new_links1) {
        new_links1[entry1->memory.link_count] = memory_id_2;
        entry1->memory.linked_memories = new_links1;
        entry1->memory.link_count++;
    } else {
        /* P2-HT-2: Report realloc failure instead of silently continuing */
        LOG_ERROR("[%s] Failed to expand linked_memories for entry1 (id=%lu)",
                  HYPERTHYMESIA_LOG_MODULE, (unsigned long)memory_id_1);
        set_error(module, HYPERTHYMESIA_ERROR_ENCODING_FAILED);
        return false;
    }

    /* Add link to entry2 */
    uint64_t* new_links2 = nimcp_realloc(
        entry2->memory.linked_memories,
        (entry2->memory.link_count + 1) * sizeof(uint64_t)
    );
    if (new_links2) {
        new_links2[entry2->memory.link_count] = memory_id_1;
        entry2->memory.linked_memories = new_links2;
        entry2->memory.link_count++;
    } else {
        /* P2-HT-2: Report realloc failure instead of silently continuing */
        LOG_ERROR("[%s] Failed to expand linked_memories for entry2 (id=%lu)",
                  HYPERTHYMESIA_LOG_MODULE, (unsigned long)memory_id_2);
        set_error(module, HYPERTHYMESIA_ERROR_ENCODING_FAILED);
        return false;
    }

    /* Set narrative tags */
    if (narrative_tag) {
        strncpy(entry1->memory.narrative_tag, narrative_tag,
                sizeof(entry1->memory.narrative_tag) - 1);
        entry1->memory.narrative_tag[sizeof(entry1->memory.narrative_tag) - 1] = '\0';
        strncpy(entry2->memory.narrative_tag, narrative_tag,
                sizeof(entry2->memory.narrative_tag) - 1);
        entry2->memory.narrative_tag[sizeof(entry2->memory.narrative_tag) - 1] = '\0';
    }

    LOG_DEBUG("[%s] Memories linked (%lu <-> %lu)", HYPERTHYMESIA_LOG_MODULE,
              (unsigned long)memory_id_1, (unsigned long)memory_id_2);

    return true;
}

/*=============================================================================
 * DATE-INDEXED RETRIEVAL
 *===========================================================================*/

bool hyperthymesia_retrieve_by_date(
    hyperthymesia_module_t* module,
    const hyperthymesia_datetime_t* datetime,
    temporal_resolution_t resolution,
    retrieval_result_t* result
) {
    if (!module || !datetime || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperthymesia_retrieve_by_date: NULL parameter");
        set_error(module, HYPERTHYMESIA_ERROR_INVALID_INPUT);
        return false;
    }

    module->status = HYPERTHYMESIA_STATUS_RETRIEVING;
    memset(result, 0, sizeof(retrieval_result_t));

    LOG_DEBUG("[%s] Retrieving by date (%04u-%02u-%02u, resolution=%d)",
              HYPERTHYMESIA_LOG_MODULE, datetime->year, datetime->month,
              datetime->day, resolution);

    if (!module->config.enable_date_indexing || !module->year_index) {
        set_error(module, HYPERTHYMESIA_ERROR_RETRIEVAL_FAILED);
        return false;
    }

    /* Navigate temporal index */
    int32_t year_idx = get_year_index(module, datetime->year);
    if (year_idx < 0) {
        module->stats.retrievals_performed++;
        module->status = HYPERTHYMESIA_STATUS_IDLE;
        return true;  /* Empty result, but not an error */
    }

    temporal_node_t* node = &module->year_index[year_idx];

    /* Navigate to appropriate resolution */
    if (resolution >= TEMPORAL_RESOLUTION_MONTH && node->children) {
        node = &node->children[datetime->month - 1];

        if (resolution >= TEMPORAL_RESOLUTION_DAY && node->children) {
            node = &node->children[datetime->day - 1];
        }
    }

    /* Collect memory IDs from node */
    if (node->memory_count == 0) {
        module->stats.retrievals_performed++;
        module->status = HYPERTHYMESIA_STATUS_IDLE;
        return true;
    }

    /* Allocate result arrays */
    result->memories = nimcp_calloc(node->memory_count, sizeof(autobiographical_memory_t));
    result->relevance_scores = nimcp_calloc(node->memory_count, sizeof(float));
    if (!result->memories || !result->relevance_scores) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           node->memory_count * sizeof(autobiographical_memory_t),
                           "hyperthymesia_retrieve_by_date: Failed to allocate results");
        hyperthymesia_free_result(result);
        set_error(module, HYPERTHYMESIA_ERROR_RETRIEVAL_FAILED);
        return false;
    }

    /* Retrieve memories */
    uint32_t found = 0;
    for (uint32_t i = 0; i < node->memory_count; i++) {
        autobiographical_memory_t mem;
        if (hyperthymesia_get_memory(module, node->memory_ids[i], &mem)) {
            result->memories[found] = mem;
            result->relevance_scores[found] = 1.0f;  /* Perfect match for date retrieval */
            found++;
        }
    }

    result->count = found;
    result->query_start = *datetime;
    result->query_end = *datetime;

    module->stats.retrievals_performed++;
    module->status = HYPERTHYMESIA_STATUS_IDLE;

    LOG_DEBUG("[%s] Retrieved %u memories for date", HYPERTHYMESIA_LOG_MODULE, found);

    return true;
}

bool hyperthymesia_query_date_range(
    hyperthymesia_module_t* module,
    const date_query_t* query,
    uint32_t max_results,
    retrieval_result_t* result
) {
    if (!module || !query || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperthymesia_query_date_range: NULL parameter");
        set_error(module, HYPERTHYMESIA_ERROR_INVALID_INPUT);
        return false;
    }

    module->status = HYPERTHYMESIA_STATUS_RETRIEVING;
    memset(result, 0, sizeof(retrieval_result_t));

    LOG_DEBUG("[%s] Querying date range", HYPERTHYMESIA_LOG_MODULE);

    /* Allocate result arrays */
    result->memories = nimcp_calloc(max_results, sizeof(autobiographical_memory_t));
    result->relevance_scores = nimcp_calloc(max_results, sizeof(float));
    if (!result->memories || !result->relevance_scores) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           max_results * sizeof(autobiographical_memory_t),
                           "hyperthymesia_query_date_range: Failed to allocate results");
        hyperthymesia_free_result(result);
        set_error(module, HYPERTHYMESIA_ERROR_RETRIEVAL_FAILED);
        return false;
    }

    /* Iterate through date range and collect memories */
    uint32_t found = 0;

    /* Iterate through years in range */
    for (uint16_t year = query->start.year; year <= query->end.year && found < max_results; year++) {
        int32_t year_idx = get_year_index(module, year);
        if (year_idx < 0 || !module->year_index) continue;

        temporal_node_t* year_node = &module->year_index[year_idx];
        if (!year_node->children) continue;

        uint8_t start_month = (year == query->start.year) ? query->start.month : 1;
        uint8_t end_month = (year == query->end.year) ? query->end.month : 12;

        for (uint8_t month = start_month; month <= end_month && found < max_results; month++) {
            temporal_node_t* month_node = &year_node->children[month - 1];
            if (!month_node->children) continue;

            uint8_t start_day = (year == query->start.year && month == query->start.month)
                                ? query->start.day : 1;
            uint8_t end_day = (year == query->end.year && month == query->end.month)
                              ? query->end.day : 31;

            for (uint8_t day = start_day; day <= end_day && found < max_results; day++) {
                if (day > 31) break;
                temporal_node_t* day_node = &month_node->children[day - 1];

                for (uint32_t i = 0; i < day_node->memory_count && found < max_results; i++) {
                    autobiographical_memory_t mem;
                    if (hyperthymesia_get_memory(module, day_node->memory_ids[i], &mem)) {
                        /* Apply filters */
                        if (mem.vividness < (int)query->min_vividness) continue;
                        if (mem.emotion.significance < query->min_emotional_significance) continue;

                        result->memories[found] = mem;
                        result->relevance_scores[found] = mem.current_strength;
                        found++;
                    }
                }
            }
        }
    }

    result->count = found;
    result->query_start = query->start;
    result->query_end = query->end;

    module->stats.retrievals_performed++;
    module->status = HYPERTHYMESIA_STATUS_IDLE;

    LOG_DEBUG("[%s] Range query returned %u memories", HYPERTHYMESIA_LOG_MODULE, found);

    return true;
}

bool hyperthymesia_retrieve_by_anniversary(
    hyperthymesia_module_t* module,
    uint8_t month,
    uint8_t day,
    retrieval_result_t* result
) {
    if (!module || month < 1 || month > 12 || day < 1 || day > 31 || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "hyperthymesia_retrieve_by_anniversary: Invalid parameters");
        set_error(module, HYPERTHYMESIA_ERROR_INVALID_INPUT);
        return false;
    }

    memset(result, 0, sizeof(retrieval_result_t));

    LOG_DEBUG("[%s] Retrieving anniversaries for %02u-%02u",
              HYPERTHYMESIA_LOG_MODULE, month, day);

    /* Count total memories on this date across years */
    uint32_t total = 0;
    if (module->year_index) {
        for (uint32_t y = 0; y < module->index_year_count; y++) {
            if (!module->year_index[y].children) continue;
            temporal_node_t* month_node = &module->year_index[y].children[month - 1];
            if (!month_node->children) continue;
            total += month_node->children[day - 1].memory_count;
        }
    }

    if (total == 0) {
        module->stats.retrievals_performed++;
        return true;
    }

    /* Allocate result arrays */
    result->memories = nimcp_calloc(total, sizeof(autobiographical_memory_t));
    result->relevance_scores = nimcp_calloc(total, sizeof(float));
    if (!result->memories || !result->relevance_scores) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           total * sizeof(autobiographical_memory_t),
                           "hyperthymesia_retrieve_by_anniversary: Failed to allocate results");
        hyperthymesia_free_result(result);
        set_error(module, HYPERTHYMESIA_ERROR_RETRIEVAL_FAILED);
        return false;
    }

    /* Collect memories */
    uint32_t found = 0;
    for (uint32_t y = 0; y < module->index_year_count && found < total; y++) {
        if (!module->year_index[y].children) continue;
        temporal_node_t* month_node = &module->year_index[y].children[month - 1];
        if (!month_node->children) continue;
        temporal_node_t* day_node = &month_node->children[day - 1];

        for (uint32_t i = 0; i < day_node->memory_count && found < total; i++) {
            autobiographical_memory_t mem;
            if (hyperthymesia_get_memory(module, day_node->memory_ids[i], &mem)) {
                result->memories[found] = mem;
                result->relevance_scores[found] = mem.current_strength;
                found++;
            }
        }
    }

    result->count = found;
    module->stats.retrievals_performed++;

    LOG_DEBUG("[%s] Found %u anniversary memories", HYPERTHYMESIA_LOG_MODULE, found);

    return true;
}

int8_t hyperthymesia_get_day_of_week(
    hyperthymesia_module_t* module,
    const hyperthymesia_datetime_t* datetime
) {
    if (!module || !datetime) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperthymesia_get_day_of_week: required parameter is NULL (module, datetime)");
        return -1;
    }

    return zeller_day_of_week(datetime->year, datetime->month, datetime->day);
}

/*=============================================================================
 * VIVID RE-EXPERIENCING
 *===========================================================================*/

bool hyperthymesia_reexperience(
    hyperthymesia_module_t* module,
    uint64_t memory_id,
    float target_depth,
    reexperience_result_t* result
) {
    if (!module || memory_id == 0 || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "hyperthymesia_reexperience: Invalid parameters");
        set_error(module, HYPERTHYMESIA_ERROR_INVALID_INPUT);
        return false;
    }

    module->status = HYPERTHYMESIA_STATUS_REEXPERIENCING;
    memset(result, 0, sizeof(reexperience_result_t));

    LOG_DEBUG("[%s] Re-experiencing memory %lu (depth=%.2f)",
              HYPERTHYMESIA_LOG_MODULE, (unsigned long)memory_id, target_depth);

    /* Find memory entry */
    uint32_t hash_idx = hash_memory_id(memory_id, module->store_capacity);
    memory_entry_t* entry = module->memory_store[hash_idx];
    while (entry && entry->memory.memory_id != memory_id) {
        entry = entry->hash_next;
    }

    if (!entry) {
        set_error(module, HYPERTHYMESIA_ERROR_DATE_NOT_FOUND);
        return false;
    }

    autobiographical_memory_t* mem = &entry->memory;

    /* Calculate achievable depth based on memory strength and vividness */
    float achievable_depth = fminf(target_depth, mem->current_strength);
    if (mem->vividness < VIVIDNESS_CLEAR) {
        achievable_depth *= 0.5f;
    }

    result->memory_id = memory_id;
    result->achieved_vividness = mem->vividness;
    result->immersion_depth = achievable_depth;

    /* Reactivate core features */
    if (mem->core_features && mem->core_feature_count > 0) {
        result->reactivated_features = nimcp_calloc(mem->core_feature_count, sizeof(float));
        if (result->reactivated_features) {
            for (uint32_t i = 0; i < mem->core_feature_count; i++) {
                result->reactivated_features[i] = mem->core_features[i] * achievable_depth;
            }
            result->feature_count = mem->core_feature_count;
        }
    }

    /* Reactivate emotional state */
    result->reactivated_emotion = mem->emotion;
    result->reactivated_emotion.arousal *= achievable_depth;
    result->emotional_intensity = fabsf(mem->emotion.valence) * achievable_depth;

    /* Reactivate sensory traces if enabled */
    if (module->config.enable_sensory_traces && mem->sensory_traces && mem->trace_count > 0) {
        result->reactivated_senses = nimcp_calloc(mem->trace_count, sizeof(sensory_trace_t));
        if (result->reactivated_senses) {
            for (uint32_t i = 0; i < mem->trace_count; i++) {
                result->reactivated_senses[i].modality = mem->sensory_traces[i].modality;
                result->reactivated_senses[i].intensity =
                    mem->sensory_traces[i].intensity * achievable_depth;
                result->reactivated_senses[i].clarity =
                    mem->sensory_traces[i].clarity * achievable_depth;
                result->reactivated_senses[i].feature_count = mem->sensory_traces[i].feature_count;

                if (mem->sensory_traces[i].features) {
                    result->reactivated_senses[i].features =
                        nimcp_calloc(mem->sensory_traces[i].feature_count, sizeof(float));
                    if (result->reactivated_senses[i].features) {
                        for (uint32_t j = 0; j < mem->sensory_traces[i].feature_count; j++) {
                            result->reactivated_senses[i].features[j] =
                                mem->sensory_traces[i].features[j] * achievable_depth;
                        }
                    }
                }
            }
            result->reactivated_trace_count = mem->trace_count;
        }
    }

    /* Calculate quality metrics */
    result->temporal_accuracy = mem->current_strength;
    result->contextual_richness = (mem->context.spatial_dim > 0 ? 0.25f : 0.0f) +
                                  (mem->context.social_dim > 0 ? 0.25f : 0.0f) +
                                  (mem->context.activity_dim > 0 ? 0.25f : 0.0f) +
                                  (mem->context.semantic_dim > 0 ? 0.25f : 0.0f);

    /* Update retrieval count */
    mem->retrieval_count++;
    mem->last_retrieval_ms = (uint64_t)time(NULL) * 1000;

    /* Invoke callback */
    if (module->reexperience_callback) {
        module->reexperience_callback(result, module->reexperience_user_data);
    }

    module->stats.reexperiences_performed++;
    module->stats.avg_reexperience_depth =
        (module->stats.avg_reexperience_depth *
         (module->stats.reexperiences_performed - 1) + achievable_depth) /
        module->stats.reexperiences_performed;

    module->status = HYPERTHYMESIA_STATUS_IDLE;

    LOG_DEBUG("[%s] Re-experience complete (depth=%.2f, vividness=%d)",
              HYPERTHYMESIA_LOG_MODULE, achievable_depth, result->achieved_vividness);

    return true;
}

bool hyperthymesia_reexperience_modality(
    hyperthymesia_module_t* module,
    uint64_t memory_id,
    reexperience_modality_t modality,
    float intensity,
    sensory_trace_t* trace
) {
    if (!module || memory_id == 0 || !trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "hyperthymesia_reexperience_modality: Invalid parameters");
        set_error(module, HYPERTHYMESIA_ERROR_INVALID_INPUT);
        return false;
    }

    memset(trace, 0, sizeof(sensory_trace_t));

    /* Find memory entry */
    uint32_t hash_idx = hash_memory_id(memory_id, module->store_capacity);
    memory_entry_t* entry = module->memory_store[hash_idx];
    while (entry && entry->memory.memory_id != memory_id) {
        entry = entry->hash_next;
    }

    if (!entry) {
        set_error(module, HYPERTHYMESIA_ERROR_DATE_NOT_FOUND);
        return false;
    }

    /* Find matching sensory trace */
    for (uint32_t i = 0; i < entry->memory.trace_count; i++) {
        if (entry->memory.sensory_traces[i].modality == modality) {
            sensory_trace_t* src = &entry->memory.sensory_traces[i];
            trace->modality = modality;
            trace->intensity = src->intensity * intensity;
            trace->clarity = src->clarity * intensity;
            trace->feature_count = src->feature_count;

            if (src->features && src->feature_count > 0) {
                trace->features = nimcp_calloc(src->feature_count, sizeof(float));
                if (trace->features) {
                    for (uint32_t j = 0; j < src->feature_count; j++) {
                        trace->features[j] = src->features[j] * intensity;
                    }
                }
            }
            return true;
        }
    }

    set_error(module, HYPERTHYMESIA_ERROR_REEXPERIENCE_FAILED);
    return false;
}

bool hyperthymesia_reexperience_emotion(
    hyperthymesia_module_t* module,
    uint64_t memory_id,
    float intensity,
    emotional_state_t* emotion
) {
    if (!module || memory_id == 0 || !emotion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "hyperthymesia_reexperience_emotion: Invalid parameters");
        set_error(module, HYPERTHYMESIA_ERROR_INVALID_INPUT);
        return false;
    }

    memset(emotion, 0, sizeof(emotional_state_t));

    /* Find memory entry */
    uint32_t hash_idx = hash_memory_id(memory_id, module->store_capacity);
    memory_entry_t* entry = module->memory_store[hash_idx];
    while (entry && entry->memory.memory_id != memory_id) {
        entry = entry->hash_next;
    }

    if (!entry) {
        set_error(module, HYPERTHYMESIA_ERROR_DATE_NOT_FOUND);
        return false;
    }

    emotion->valence = entry->memory.emotion.valence * intensity;
    emotion->arousal = entry->memory.emotion.arousal * intensity;
    emotion->dominance = entry->memory.emotion.dominance * intensity;
    emotion->surprise = entry->memory.emotion.surprise * intensity;
    emotion->significance = entry->memory.emotion.significance;  /* Don't scale significance */

    return true;
}

/*=============================================================================
 * TIMELINE NAVIGATION
 *===========================================================================*/

bool hyperthymesia_create_cursor(
    hyperthymesia_module_t* module,
    const hyperthymesia_datetime_t* start_date,
    temporal_resolution_t zoom_level,
    timeline_cursor_t* cursor
) {
    if (!module || !start_date || !cursor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperthymesia_create_cursor: NULL parameter");
        set_error(module, HYPERTHYMESIA_ERROR_INVALID_INPUT);
        return false;
    }

    memset(cursor, 0, sizeof(timeline_cursor_t));
    cursor->current = *start_date;
    cursor->zoom_level = zoom_level;

    /* Get visible memories at current position */
    retrieval_result_t result;
    if (hyperthymesia_retrieve_by_date(module, start_date, zoom_level, &result)) {
        if (result.count > 0) {
            cursor->visible_memories = nimcp_calloc(result.count, sizeof(uint64_t));
            if (cursor->visible_memories) {
                for (uint32_t i = 0; i < result.count; i++) {
                    cursor->visible_memories[i] = result.memories[i].memory_id;
                }
                cursor->visible_count = result.count;
            }
        }
        hyperthymesia_free_result(&result);
    }

    module->stats.timeline_navigations++;

    if (module->navigation_callback) {
        module->navigation_callback(cursor, module->navigation_user_data);
    }

    return true;
}

bool hyperthymesia_navigate_timeline(
    hyperthymesia_module_t* module,
    timeline_cursor_t* cursor,
    int32_t offset
) {
    if (!module || !cursor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperthymesia_navigate_timeline: NULL parameter");
        set_error(module, HYPERTHYMESIA_ERROR_INVALID_INPUT);
        return false;
    }

    /* P1-HT-2: Use signed int temporaries to prevent unsigned underflow */
    /* Adjust current position based on zoom level and offset */
    switch (cursor->zoom_level) {
        case TEMPORAL_RESOLUTION_DAY: {
            int32_t day = (int32_t)cursor->current.day + offset;
            int32_t month = (int32_t)cursor->current.month;
            int32_t year = (int32_t)cursor->current.year;
            /* Normalize (simplified - doesn't handle month boundaries perfectly) */
            while (day > 28) {
                day -= 28;
                month++;
                if (month > 12) {
                    month = 1;
                    year++;
                }
            }
            while (day < 1) {
                day += 28;
                month--;
                if (month < 1) {
                    month = 12;
                    year--;
                }
            }
            cursor->current.day = (uint8_t)day;
            cursor->current.month = (uint8_t)month;
            cursor->current.year = (uint16_t)year;
            break;
        }

        case TEMPORAL_RESOLUTION_MONTH: {
            int32_t month = (int32_t)cursor->current.month + offset;
            int32_t year = (int32_t)cursor->current.year;
            while (month > 12) {
                month -= 12;
                year++;
            }
            while (month < 1) {
                month += 12;
                year--;
            }
            cursor->current.month = (uint8_t)month;
            cursor->current.year = (uint16_t)year;
            break;
        }

        case TEMPORAL_RESOLUTION_YEAR:
            cursor->current.year += offset;
            break;

        default:
            break;
    }

    cursor->scroll_offset = offset;

    /* Update visible memories */
    if (cursor->visible_memories) {
        nimcp_free(cursor->visible_memories);
        cursor->visible_memories = NULL;
        cursor->visible_count = 0;
    }

    retrieval_result_t result;
    if (hyperthymesia_retrieve_by_date(module, &cursor->current, cursor->zoom_level, &result)) {
        if (result.count > 0) {
            cursor->visible_memories = nimcp_calloc(result.count, sizeof(uint64_t));
            if (cursor->visible_memories) {
                for (uint32_t i = 0; i < result.count; i++) {
                    cursor->visible_memories[i] = result.memories[i].memory_id;
                }
                cursor->visible_count = result.count;
            }
        }
        hyperthymesia_free_result(&result);
    }

    module->stats.timeline_navigations++;

    if (module->navigation_callback) {
        module->navigation_callback(cursor, module->navigation_user_data);
    }

    return true;
}

bool hyperthymesia_set_zoom(
    hyperthymesia_module_t* module,
    timeline_cursor_t* cursor,
    temporal_resolution_t new_zoom
) {
    if (!module || !cursor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperthymesia_set_zoom: NULL parameter");
        set_error(module, HYPERTHYMESIA_ERROR_INVALID_INPUT);
        return false;
    }

    cursor->zoom_level = new_zoom;

    /* Update visible memories for new zoom level */
    if (cursor->visible_memories) {
        nimcp_free(cursor->visible_memories);
        cursor->visible_memories = NULL;
        cursor->visible_count = 0;
    }

    retrieval_result_t result;
    if (hyperthymesia_retrieve_by_date(module, &cursor->current, new_zoom, &result)) {
        if (result.count > 0) {
            cursor->visible_memories = nimcp_calloc(result.count, sizeof(uint64_t));
            if (cursor->visible_memories) {
                for (uint32_t i = 0; i < result.count; i++) {
                    cursor->visible_memories[i] = result.memories[i].memory_id;
                }
                cursor->visible_count = result.count;
            }
        }
        hyperthymesia_free_result(&result);
    }

    if (module->navigation_callback) {
        module->navigation_callback(cursor, module->navigation_user_data);
    }

    return true;
}

bool hyperthymesia_jump_to_date(
    hyperthymesia_module_t* module,
    timeline_cursor_t* cursor,
    const hyperthymesia_datetime_t* target_date
) {
    if (!module || !cursor || !target_date) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperthymesia_jump_to_date: NULL parameter");
        set_error(module, HYPERTHYMESIA_ERROR_INVALID_INPUT);
        return false;
    }

    cursor->current = *target_date;
    cursor->scroll_offset = 0;

    /* Update visible memories */
    if (cursor->visible_memories) {
        nimcp_free(cursor->visible_memories);
        cursor->visible_memories = NULL;
        cursor->visible_count = 0;
    }

    retrieval_result_t result;
    if (hyperthymesia_retrieve_by_date(module, target_date, cursor->zoom_level, &result)) {
        if (result.count > 0) {
            cursor->visible_memories = nimcp_calloc(result.count, sizeof(uint64_t));
            if (cursor->visible_memories) {
                for (uint32_t i = 0; i < result.count; i++) {
                    cursor->visible_memories[i] = result.memories[i].memory_id;
                }
                cursor->visible_count = result.count;
            }
        }
        hyperthymesia_free_result(&result);
    }

    module->stats.timeline_navigations++;

    if (module->navigation_callback) {
        module->navigation_callback(cursor, module->navigation_user_data);
    }

    return true;
}

void hyperthymesia_free_cursor(timeline_cursor_t* cursor) {
    if (!cursor) return;
    if (cursor->visible_memories) nimcp_free(cursor->visible_memories);
    memset(cursor, 0, sizeof(timeline_cursor_t));
}

/*=============================================================================
 * MEMORY MANAGEMENT
 *===========================================================================*/

bool hyperthymesia_get_memory(
    const hyperthymesia_module_t* module,
    uint64_t memory_id,
    autobiographical_memory_t* memory
) {
    if (!module || memory_id == 0 || !memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "hyperthymesia_get_memory: Invalid parameters");
        return false;
    }

    uint32_t hash_idx = hash_memory_id(memory_id, module->store_capacity);
    memory_entry_t* entry = module->memory_store[hash_idx];

    while (entry) {
        if (entry->memory.memory_id == memory_id) {
            /* P2-HT-1: NOTE - This is a SHALLOW COPY. The caller must NOT free
             * internal pointers (core_features, sensory_traces, linked_memories,
             * context sub-arrays). They are owned by the module's hash table entry.
             * The returned struct is valid only while the module holds the memory. */
            *memory = entry->memory;
            return true;
        }
        entry = entry->hash_next;
    }

    /* P2-HT-3: Not finding a memory by ID is a normal lookup miss, not an error */
    return false;
}

bool hyperthymesia_update_vividness(
    hyperthymesia_module_t* module,
    uint64_t memory_id,
    memory_vividness_t vividness
) {
    if (!module || memory_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "hyperthymesia_update_vividness: Invalid parameters");
        set_error(module, HYPERTHYMESIA_ERROR_INVALID_INPUT);
        return false;
    }

    uint32_t hash_idx = hash_memory_id(memory_id, module->store_capacity);
    memory_entry_t* entry = module->memory_store[hash_idx];

    while (entry) {
        if (entry->memory.memory_id == memory_id) {
            entry->memory.vividness = vividness;
            return true;
        }
        entry = entry->hash_next;
    }

    set_error(module, HYPERTHYMESIA_ERROR_DATE_NOT_FOUND);
    return false;
}

uint32_t hyperthymesia_consolidate(
    hyperthymesia_module_t* module,
    float strength_threshold
) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperthymesia_consolidate: NULL module pointer");
        return 0;
    }

    module->status = HYPERTHYMESIA_STATUS_CONSOLIDATING;

    uint32_t consolidated = 0;

    for (uint32_t i = 0; i < module->store_capacity; i++) {
        memory_entry_t* entry = module->memory_store[i];
        while (entry) {
            if (entry->memory.current_strength >= strength_threshold) {
                /* Strengthen memory (simplified consolidation) */
                entry->memory.current_strength = fminf(1.0f,
                    entry->memory.current_strength * 1.1f);
                entry->memory.vividness = calculate_vividness(entry->memory.current_strength);
                consolidated++;
            }
            entry = entry->hash_next;
        }
    }

    module->status = HYPERTHYMESIA_STATUS_IDLE;

    LOG_DEBUG("[%s] Consolidated %u memories", HYPERTHYMESIA_LOG_MODULE, consolidated);

    return consolidated;
}

/*=============================================================================
 * CALLBACKS
 *===========================================================================*/

bool hyperthymesia_set_encode_callback(
    hyperthymesia_module_t* module,
    hyperthymesia_encode_callback_t callback,
    void* user_data
) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperthymesia_set_encode_callback: NULL module pointer");
        return false;
    }
    module->encode_callback = callback;
    module->encode_user_data = user_data;
    return true;
}

bool hyperthymesia_set_reexperience_callback(
    hyperthymesia_module_t* module,
    hyperthymesia_reexperience_callback_t callback,
    void* user_data
) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperthymesia_set_reexperience_callback: NULL module pointer");
        return false;
    }
    module->reexperience_callback = callback;
    module->reexperience_user_data = user_data;
    return true;
}

bool hyperthymesia_set_navigation_callback(
    hyperthymesia_module_t* module,
    hyperthymesia_navigation_callback_t callback,
    void* user_data
) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperthymesia_set_navigation_callback: NULL module pointer");
        return false;
    }
    module->navigation_callback = callback;
    module->navigation_user_data = user_data;
    return true;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

hyperthymesia_status_t hyperthymesia_get_status(const hyperthymesia_module_t* module) {
    if (!module) return HYPERTHYMESIA_STATUS_ERROR;
    return module->status;
}

hyperthymesia_error_t hyperthymesia_get_last_error(const hyperthymesia_module_t* module) {
    if (!module) return HYPERTHYMESIA_ERROR_NOT_INITIALIZED;
    return module->last_error;
}

const char* hyperthymesia_error_string(hyperthymesia_error_t error) {
    switch (error) {
        case HYPERTHYMESIA_ERROR_NONE: return "No error";
        case HYPERTHYMESIA_ERROR_INVALID_INPUT: return "Invalid input";
        case HYPERTHYMESIA_ERROR_ENCODING_FAILED: return "Memory encoding failed";
        case HYPERTHYMESIA_ERROR_RETRIEVAL_FAILED: return "Memory retrieval failed";
        case HYPERTHYMESIA_ERROR_DATE_NOT_FOUND: return "Date not found";
        case HYPERTHYMESIA_ERROR_MEMORY_FULL: return "Memory capacity full";
        case HYPERTHYMESIA_ERROR_TIMELINE_CORRUPTION: return "Timeline corruption";
        case HYPERTHYMESIA_ERROR_REEXPERIENCE_FAILED: return "Re-experience failed";
        case HYPERTHYMESIA_ERROR_CONTEXT_MISMATCH: return "Context mismatch";
        case HYPERTHYMESIA_ERROR_TEMPORAL_OVERFLOW: return "Temporal overflow";
        case HYPERTHYMESIA_ERROR_NOT_INITIALIZED: return "Not initialized";
        case HYPERTHYMESIA_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* hyperthymesia_status_string(hyperthymesia_status_t status) {
    switch (status) {
        case HYPERTHYMESIA_STATUS_IDLE: return "Idle";
        case HYPERTHYMESIA_STATUS_ENCODING: return "Encoding";
        case HYPERTHYMESIA_STATUS_RETRIEVING: return "Retrieving";
        case HYPERTHYMESIA_STATUS_NAVIGATING: return "Navigating";
        case HYPERTHYMESIA_STATUS_REEXPERIENCING: return "Re-experiencing";
        case HYPERTHYMESIA_STATUS_CONSOLIDATING: return "Consolidating";
        case HYPERTHYMESIA_STATUS_READY: return "Ready";
        case HYPERTHYMESIA_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

bool hyperthymesia_get_stats(const hyperthymesia_module_t* module, hyperthymesia_stats_t* stats) {
    if (!module || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperthymesia_get_stats: NULL parameter");
        return false;
    }
    *stats = module->stats;
    return true;
}

bool hyperthymesia_get_config(const hyperthymesia_module_t* module, hyperthymesia_config_t* config) {
    if (!module || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hyperthymesia_get_config: NULL parameter");
        return false;
    }
    *config = module->config;
    return true;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

int hyperthymesia_compare_datetime(
    const hyperthymesia_datetime_t* dt1,
    const hyperthymesia_datetime_t* dt2
) {
    if (!dt1 || !dt2) return 0;

    if (dt1->year != dt2->year) return (dt1->year < dt2->year) ? -1 : 1;
    if (dt1->month != dt2->month) return (dt1->month < dt2->month) ? -1 : 1;
    if (dt1->day != dt2->day) return (dt1->day < dt2->day) ? -1 : 1;
    if (dt1->hour != dt2->hour) return (dt1->hour < dt2->hour) ? -1 : 1;
    if (dt1->minute != dt2->minute) return (dt1->minute < dt2->minute) ? -1 : 1;
    if (dt1->second != dt2->second) return (dt1->second < dt2->second) ? -1 : 1;
    if (dt1->millisecond != dt2->millisecond) {
        return (dt1->millisecond < dt2->millisecond) ? -1 : 1;
    }

    return 0;
}

bool hyperthymesia_get_current_datetime(hyperthymesia_datetime_t* datetime) {
    if (!datetime) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperthymesia_get_current_datetime: datetime is NULL");
        return false;
    }

    /* P1-HT-1: Use localtime_r for thread safety */
    time_t now = time(NULL);
    struct tm tm_buf;
    struct tm* tm_info = localtime_r(&now, &tm_buf);
    if (!tm_info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hyperthymesia_get_current_datetime: tm_info is NULL");
        return false;
    }

    datetime->year = tm_info->tm_year + 1900;
    datetime->month = tm_info->tm_mon + 1;
    datetime->day = tm_info->tm_mday;
    datetime->hour = tm_info->tm_hour;
    datetime->minute = tm_info->tm_min;
    datetime->second = tm_info->tm_sec;
    datetime->millisecond = 0;
    datetime->timezone_offset = 0;

    return true;
}

void hyperthymesia_free_result(retrieval_result_t* result) {
    if (!result) return;
    if (result->memories) nimcp_free(result->memories);
    if (result->relevance_scores) nimcp_free(result->relevance_scores);
    memset(result, 0, sizeof(retrieval_result_t));
}

void hyperthymesia_free_reexperience_result(reexperience_result_t* result) {
    if (!result) return;

    if (result->reactivated_features) nimcp_free(result->reactivated_features);

    if (result->reactivated_senses) {
        for (uint32_t i = 0; i < result->reactivated_trace_count; i++) {
            if (result->reactivated_senses[i].features) {
                nimcp_free(result->reactivated_senses[i].features);
            }
        }
        nimcp_free(result->reactivated_senses);
    }

    memset(result, 0, sizeof(reexperience_result_t));
}
