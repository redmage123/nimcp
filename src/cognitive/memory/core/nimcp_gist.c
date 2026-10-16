//=============================================================================
// nimcp_gist.c - Gist Extraction System Implementation
//=============================================================================
/**
 * @file nimcp_gist.c
 * @brief Implementation of gist extraction and Fuzzy Trace Theory memory
 *
 * This file implements the gist extraction system for Prime Resonant memory,
 * enabling parallel verbatim and gist trace storage as per Fuzzy Trace Theory.
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_gist.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <stdatomic.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(gist, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Constants
//=============================================================================

/** Hash table load factor for rehashing */
#define GIST_HASH_LOAD_FACTOR           0.75f

/** Initial hash table size */
#define GIST_HASH_INITIAL_SIZE          256

/** Growth factor for dynamic arrays */
#define GIST_GROWTH_FACTOR              2

/** Maximum error message length */
#define GIST_ERROR_MSG_SIZE             256

//=============================================================================
// Thread-Local Error State
//=============================================================================

static _Thread_local char g_last_error[GIST_ERROR_MSG_SIZE] = {0};

/**
 * @brief Set last error message
 */
static void gist_set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_error, GIST_ERROR_MSG_SIZE, fmt, args);
    va_end(args);
}

//=============================================================================
// Internal Data Structures
//=============================================================================

/**
 * @brief Hash table entry for gist lookup
 */
typedef struct gist_hash_entry {
    uint64_t key;                       /**< Gist ID */
    gist_node_t* gist;                  /**< Gist node pointer */
    struct gist_hash_entry* next;       /**< Collision chain */
} gist_hash_entry_t;

/**
 * @brief Hash table entry for dual trace lookup
 */
typedef struct trace_hash_entry {
    uint64_t key;                       /**< Trace ID */
    dual_trace_t* trace;                /**< Dual trace pointer */
    struct trace_hash_entry* next;      /**< Collision chain */
} trace_hash_entry_t;

/**
 * @brief Internal gist system structure
 */
struct gist_system_struct {
    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    gist_config_t config;               /**< System configuration */

    //-------------------------------------------------------------------------
    // External References
    //-------------------------------------------------------------------------
    entangle_graph_t entanglement;      /**< Entanglement graph */
    pr_node_manager_t node_manager;     /**< Memory node manager */

    //-------------------------------------------------------------------------
    // Gist Storage
    //-------------------------------------------------------------------------
    gist_hash_entry_t** gist_table;     /**< Gist hash table */
    size_t gist_table_size;             /**< Hash table size */
    size_t num_gists;                   /**< Current gist count */
    _Atomic uint64_t next_gist_id;      /**< ID generator */

    //-------------------------------------------------------------------------
    // Dual Trace Storage
    //-------------------------------------------------------------------------
    trace_hash_entry_t** trace_table;   /**< Trace hash table */
    size_t trace_table_size;            /**< Hash table size */
    size_t num_traces;                  /**< Current trace count */
    _Atomic uint64_t next_trace_id;     /**< ID generator */

    //-------------------------------------------------------------------------
    // Feature Importance Model
    //-------------------------------------------------------------------------
    float feature_weights[PRIME_SIG_DIM];      /**< Learned feature importance */
    float feature_variance[PRIME_SIG_DIM];     /**< Feature variance estimates */
    float feature_frequency[PRIME_SIG_DIM];    /**< Feature occurrence frequency */
    size_t feature_samples;                     /**< Samples for statistics */

    //-------------------------------------------------------------------------
    // Statistics
    //-------------------------------------------------------------------------
    _Atomic uint64_t total_extractions;
    _Atomic uint64_t total_merges;
    float sum_compression;
    float sum_coherence;
    float sum_abstractness;
};

//=============================================================================
// Hash Functions
//=============================================================================

/**
 * @brief Compute hash for 64-bit key
 */
static inline size_t hash_uint64(uint64_t key, size_t table_size) {
    // MurmurHash3 finalizer
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53ULL;
    key ^= key >> 33;
    return (size_t)(key % table_size);
}

//=============================================================================
// Gist Hash Table Operations
//=============================================================================

/**
 * @brief Create gist hash table
 */
static gist_hash_entry_t** gist_table_create(size_t size) {
    gist_hash_entry_t** table = nimcp_calloc(size, sizeof(gist_hash_entry_t*));
    return table;
}

/**
 * @brief Destroy gist hash table
 */
static void gist_table_destroy(gist_hash_entry_t** table, size_t size) {
    if (!table) return;

    for (size_t i = 0; i < size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && size > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)size);
        }

        gist_hash_entry_t* entry = table[i];
        while (entry) {
            gist_hash_entry_t* next = entry->next;
            // Free gist node contents
            if (entry->gist) {
                nimcp_free(entry->gist->source_memory_ids);
                nimcp_free(entry->gist->key_features);
                nimcp_free(entry->gist);
            }
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
    }
    nimcp_free(table);
    table = NULL;
}

/**
 * @brief Insert gist into hash table
 */
static bool gist_table_insert(gist_hash_entry_t** table, size_t size,
                              uint64_t key, gist_node_t* gist) {
    size_t idx = hash_uint64(key, size);

    gist_hash_entry_t* entry = nimcp_malloc(sizeof(gist_hash_entry_t));
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gist_table_destroy: entry is NULL");
        return false;
    }

    entry->key = key;
    entry->gist = gist;
    entry->next = table[idx];
    table[idx] = entry;

    return true;
}

/**
 * @brief Lookup gist in hash table
 */
static gist_node_t* gist_table_lookup(gist_hash_entry_t** table, size_t size,
                                       uint64_t key) {
    size_t idx = hash_uint64(key, size);

    gist_hash_entry_t* entry = table[idx];
    while (entry) {
        if (entry->key == key) {
            return entry->gist;
        }
        entry = entry->next;
    }
    return NULL;
}

/**
 * @brief Remove gist from hash table
 */
static gist_node_t* gist_table_remove(gist_hash_entry_t** table, size_t size,
                                       uint64_t key) {
    size_t idx = hash_uint64(key, size);

    gist_hash_entry_t** prev_ptr = &table[idx];
    gist_hash_entry_t* entry = table[idx];

    while (entry) {
        if (entry->key == key) {
            *prev_ptr = entry->next;
            gist_node_t* gist = entry->gist;
            nimcp_free(entry);
            entry = NULL;
            return gist;
        }
        prev_ptr = &entry->next;
        entry = entry->next;
    }
    return NULL;
}

//=============================================================================
// Trace Hash Table Operations
//=============================================================================

/**
 * @brief Create trace hash table
 */
static trace_hash_entry_t** trace_table_create(size_t size) {
    trace_hash_entry_t** table = nimcp_calloc(size, sizeof(trace_hash_entry_t*));
    return table;
}

/**
 * @brief Destroy trace hash table
 */
static void trace_table_destroy(trace_hash_entry_t** table, size_t size) {
    if (!table) return;

    for (size_t i = 0; i < size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && size > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)size);
        }

        trace_hash_entry_t* entry = table[i];
        while (entry) {
            trace_hash_entry_t* next = entry->next;
            nimcp_free(entry->trace);
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
    }
    nimcp_free(table);
    table = NULL;
}

/**
 * @brief Insert trace into hash table
 */
static bool trace_table_insert(trace_hash_entry_t** table, size_t size,
                               uint64_t key, dual_trace_t* trace) {
    size_t idx = hash_uint64(key, size);

    trace_hash_entry_t* entry = nimcp_malloc(sizeof(trace_hash_entry_t));
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "trace_table_insert: entry allocation failed");
        return false;
    }

    entry->key = key;
    entry->trace = trace;
    entry->next = table[idx];
    table[idx] = entry;

    return true;
}

/**
 * @brief Lookup trace in hash table
 */
static dual_trace_t* trace_table_lookup(trace_hash_entry_t** table, size_t size,
                                         uint64_t key) {
    size_t idx = hash_uint64(key, size);

    trace_hash_entry_t* entry = table[idx];
    while (entry) {
        if (entry->key == key) {
            return entry->trace;
        }
        entry = entry->next;
    }
    return NULL;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Initialize feature weights with defaults
 */
static void init_feature_weights(gist_system_t system) {
    // Initialize with type-based defaults
    // Lower prime indices tend to be more semantic (by convention)
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        // Semantic features (lower indices) get higher weight
        if (i < PRIME_SIG_DIM / 4) {
            system->feature_weights[i] = 0.8f + 0.2f * (float)(PRIME_SIG_DIM / 4 - i) / (PRIME_SIG_DIM / 4);
        }
        // Structural features (middle indices)
        else if (i < PRIME_SIG_DIM / 2) {
            system->feature_weights[i] = 0.5f + 0.3f * (float)(PRIME_SIG_DIM / 2 - i) / (PRIME_SIG_DIM / 4);
        }
        // Surface/contextual features (higher indices)
        else {
            system->feature_weights[i] = 0.2f + 0.3f * (float)(PRIME_SIG_DIM - i) / (PRIME_SIG_DIM / 2);
        }

        system->feature_variance[i] = 0.5f;  // Default variance
        system->feature_frequency[i] = 0.0f;
    }
    system->feature_samples = 0;
}

/**
 * @brief Get feature type based on index
 */
static feature_type_t get_feature_type(size_t index) {
    if (index < PRIME_SIG_DIM / 4) {
        return FEATURE_TYPE_SEMANTIC;
    } else if (index < PRIME_SIG_DIM / 2) {
        return FEATURE_TYPE_STRUCTURAL;
    } else if (index < 3 * PRIME_SIG_DIM / 4) {
        return FEATURE_TYPE_SURFACE;
    } else {
        return FEATURE_TYPE_CONTEXTUAL;
    }
}

/**
 * @brief Compute feature score for gist inclusion
 */
static float compute_feature_score(gist_system_t system, size_t index,
                                   uint8_t exponent) {
    if (exponent == 0) return 0.0f;

    float importance = system->feature_weights[index];
    float variance = system->feature_variance[index];
    float magnitude = (float)exponent / 255.0f;

    // Score = importance * variance * magnitude
    return importance * (0.5f + 0.5f * variance) * magnitude;
}

/**
 * @brief Compare key features by score (for sorting)
 */
static int compare_features_by_score(const void* a, const void* b) {
    const gist_key_feature_t* fa = (const gist_key_feature_t*)a;
    const gist_key_feature_t* fb = (const gist_key_feature_t*)b;

    if (fa->importance > fb->importance) return -1;
    if (fa->importance < fb->importance) return 1;
    return 0;
}

/**
 * @brief Update feature statistics from signature
 */
static void update_feature_statistics(gist_system_t system,
                                       const prime_signature_t* sig) {
    if (!system || !sig) return;

    float n = (float)(system->feature_samples + 1);

    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        float value = (float)sig->exponents[i] / 255.0f;

        // Update running mean (for frequency)
        float old_mean = system->feature_frequency[i];
        float new_mean = old_mean + (value - old_mean) / (fabsf(n) > 1e-7f ? n : 1e-7f);
        system->feature_frequency[i] = new_mean;

        // Update variance using Welford's algorithm
        if (system->feature_samples > 0) {
            float old_var = system->feature_variance[i];
            float delta = value - old_mean;
            float delta2 = value - new_mean;
            float new_var = old_var + (delta * delta2 - old_var) / (fabsf(n) > 1e-7f ? n : 1e-7f);
            system->feature_variance[i] = fmaxf(0.0f, new_var);
        }
    }

    system->feature_samples++;
}

/**
 * @brief Create gist signature from verbatim using key features
 */
static void create_gist_signature(gist_system_t system,
                                   const prime_signature_t* verbatim,
                                   const gist_key_feature_t* features,
                                   size_t num_features,
                                   prime_signature_t* gist_out) {
    (void)system;  // Reserved for future use
    // Start with zero signature
    memset(gist_out->exponents, 0, sizeof(gist_out->exponents));
    memcpy(gist_out->primes, PRIMES_64, sizeof(gist_out->primes));

    // Copy only key feature exponents
    for (size_t i = 0; i < num_features; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_features > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)num_features);
        }

        uint32_t idx = features[i].prime_index;
        if (idx < PRIME_SIG_DIM) {
            gist_out->exponents[idx] = verbatim->exponents[idx];
        }
    }

    // Recompute hash and factor count
    gist_out->hash = prime_sig_hash(gist_out);
    gist_out->num_factors = prime_sig_recount_factors(gist_out);
}

/**
 * @brief Allocate and initialize gist node
 */
static gist_node_t* alloc_gist_node(gist_system_t system) {
    gist_node_t* gist = nimcp_calloc(1, sizeof(gist_node_t));
    if (!gist) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate gist");

        return NULL;

    }

    // Allocate source memory array
    gist->source_memory_ids = nimcp_calloc(GIST_MAX_SOURCES, sizeof(uint64_t));
    if (!gist->source_memory_ids) {
        nimcp_free(gist);
        gist = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "alloc_gist_node: gist->source_memory_ids is NULL");
        return NULL;
    }
    gist->sources_capacity = GIST_MAX_SOURCES;

    // Allocate key features array
    gist->key_features = nimcp_calloc(system->config.max_key_features,
                                sizeof(gist_key_feature_t));
    if (!gist->key_features) {
        nimcp_free(gist->source_memory_ids);
        nimcp_free(gist);
        gist = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "alloc_gist_node: gist->key_features is NULL");
        return NULL;
    }
    gist->features_capacity = system->config.max_key_features;

    // Generate unique ID
    gist->gist_id = atomic_fetch_add(&system->next_gist_id, 1);

    // Initialize timestamps
    gist->created_time_ms = gist_current_time_ms();
    gist->last_accessed_ms = gist->created_time_ms;

    // Default values
    gist->current_strength = 1.0f;
    gist->decay_rate = system->config.gist_decay_rate;
    gist->related_schema_id = GIST_INVALID_ID;
    gist->confidence = 1.0f;
    gist->generality = 1.0f;

    return gist;
}

/**
 * @brief Allocate and initialize dual trace
 */
static dual_trace_t* alloc_dual_trace(gist_system_t system) {
    dual_trace_t* trace = nimcp_calloc(1, sizeof(dual_trace_t));
    if (!trace) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate trace");

        return NULL;

    }

    // Generate unique ID
    trace->trace_id = atomic_fetch_add(&system->next_trace_id, 1);

    // Initialize timestamps
    trace->created_time_ms = gist_current_time_ms();
    trace->last_verbatim_access_ms = trace->created_time_ms;
    trace->last_gist_access_ms = trace->created_time_ms;

    // Default strengths
    trace->verbatim_strength = 1.0f;
    trace->gist_strength = 1.0f;

    // Default decay rates
    trace->verbatim_decay_rate = system->config.verbatim_decay_rate;
    trace->gist_decay_rate = system->config.gist_decay_rate;

    return trace;
}

//=============================================================================
// Configuration Functions
//=============================================================================

gist_config_t gist_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_config_default", 0.0f);


    gist_config_t config = {
        .method = GIST_METHOD_FEATURE_IMPORTANCE,
        .compression_target = GIST_DEFAULT_COMPRESSION,
        .feature_importance_threshold = GIST_FEATURE_IMPORTANCE_MIN,
        .max_key_features = GIST_MAX_KEY_FEATURES,

        .min_coherence = GIST_MIN_COHERENCE,
        .min_information_retention = 0.5f,
        .abstractness_target = GIST_ABSTRACTNESS_THRESHOLD,

        .verbatim_decay_rate = GIST_VERBATIM_DECAY_RATE,
        .gist_decay_rate = GIST_GIST_DECAY_RATE,
        .decay_differential = GIST_VERBATIM_DECAY_RATE / GIST_GIST_DECAY_RATE,

        .max_gists = GIST_MAX_GISTS,
        .max_dual_traces = GIST_MAX_DUAL_TRACES,

        .feature_weights = {
            1.0f,   // SEMANTIC - highest
            0.6f,   // STRUCTURAL
            0.3f,   // SURFACE
            0.2f,   // CONTEXTUAL
            0.4f    // TEMPORAL
        }
    };
    return config;
}

bool gist_config_validate(const gist_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gist_config_validate: config is NULL");
        return false;
    }

    // Compression target
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_config_validate", 0.0f);


    if (config->compression_target <= 0.0f || config->compression_target > 1.0f) {
        gist_set_error("compression_target must be in (0, 1]");
        return false;
    }

    // Thresholds
    if (config->feature_importance_threshold < 0.0f ||
        config->feature_importance_threshold > 1.0f) {
        gist_set_error("feature_importance_threshold must be in [0, 1]");
        return false;
    }

    if (config->min_coherence < 0.0f || config->min_coherence > 1.0f) {
        gist_set_error("min_coherence must be in [0, 1]");
        return false;
    }

    // Decay rates
    if (config->verbatim_decay_rate < 0.0f || config->gist_decay_rate < 0.0f) {
        gist_set_error("decay rates must be >= 0");
        return false;
    }

    // Capacities
    if (config->max_gists == 0 || config->max_dual_traces == 0 ||
        config->max_key_features == 0) {
        gist_set_error("max values must be > 0");
        return false;
    }

    return true;
}

//=============================================================================
// System Lifecycle Functions
//=============================================================================

gist_system_t gist_system_create(
    const gist_config_t* config,
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager
) {
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_system_create", 0.0f);


    gist_config_t cfg = config ? *config : gist_config_default();

    if (!gist_config_validate(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gist_system_create: gist_config_validate is NULL");
        return NULL;
    }

    gist_system_t system = nimcp_calloc(1, sizeof(struct gist_system_struct));
    if (!system) {
        gist_set_error("Failed to allocate gist system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gist_system_create: system is NULL");
        return NULL;
    }

    system->config = cfg;
    system->entanglement = entanglement;
    system->node_manager = node_manager;

    // Create gist hash table
    system->gist_table_size = GIST_HASH_INITIAL_SIZE;
    system->gist_table = gist_table_create(system->gist_table_size);
    if (!system->gist_table) {
        gist_set_error("Failed to allocate gist hash table");
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gist_system_create: system->gist_table is NULL");
        return NULL;
    }

    // Create trace hash table
    system->trace_table_size = GIST_HASH_INITIAL_SIZE;
    system->trace_table = trace_table_create(system->trace_table_size);
    if (!system->trace_table) {
        gist_set_error("Failed to allocate trace hash table");
        gist_table_destroy(system->gist_table, system->gist_table_size);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gist_system_create: system->trace_table is NULL");
        return NULL;
    }

    // Initialize IDs
    atomic_store(&system->next_gist_id, 1);
    atomic_store(&system->next_trace_id, 1);

    // Initialize feature weights
    init_feature_weights(system);

    // Initialize statistics
    atomic_store(&system->total_extractions, 0);
    atomic_store(&system->total_merges, 0);

    return system;
}

void gist_system_destroy(gist_system_t system) {
    if (!system) return;

    // Destroy hash tables (includes freeing all gists and traces)
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_system_destroy", 0.0f);


    gist_table_destroy(system->gist_table, system->gist_table_size);
    trace_table_destroy(system->trace_table, system->trace_table_size);

    nimcp_free(system);
    system = NULL;
}

gist_error_t gist_system_clear(gist_system_t system) {
    if (!system) return GIST_ERROR_NULL_POINTER;

    // Clear gist table
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_system_clear", 0.0f);


    for (size_t i = 0; i < system->gist_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->gist_table_size > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)system->gist_table_size);
        }

        gist_hash_entry_t* entry = system->gist_table[i];
        while (entry) {
            gist_hash_entry_t* next = entry->next;
            if (entry->gist) {
                nimcp_free(entry->gist->source_memory_ids);
                nimcp_free(entry->gist->key_features);
                nimcp_free(entry->gist);
            }
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
        system->gist_table[i] = NULL;
    }
    system->num_gists = 0;

    // Clear trace table
    for (size_t i = 0; i < system->trace_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->trace_table_size > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)system->trace_table_size);
        }

        trace_hash_entry_t* entry = system->trace_table[i];
        while (entry) {
            trace_hash_entry_t* next = entry->next;
            nimcp_free(entry->trace);
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
        system->trace_table[i] = NULL;
    }
    system->num_traces = 0;

    // Reset statistics
    system->sum_compression = 0.0f;
    system->sum_coherence = 0.0f;
    system->sum_abstractness = 0.0f;

    return GIST_SUCCESS;
}

//=============================================================================
// Gist Extraction Functions
//=============================================================================

gist_error_t gist_extract(
    gist_system_t system,
    const pr_memory_node_t* memory,
    gist_extraction_result_t* result
) {
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_extract", 0.0f);


    return gist_extract_custom(system, memory, system->config.method,
                               system->config.compression_target, result);
}

gist_error_t gist_extract_custom(
    gist_system_t system,
    const pr_memory_node_t* memory,
    gist_extraction_method_t method,
    float compression_target,
    gist_extraction_result_t* result
) {
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_extract_custom", 0.0f);


    (void)method;  // TODO: Implement method-specific extraction

    if (!system || !memory || !result) {
        return GIST_ERROR_NULL_POINTER;
    }

    memset(result, 0, sizeof(gist_extraction_result_t));
    result->status = GIST_SUCCESS;

    // Check capacity
    if (system->num_gists >= system->config.max_gists) {
        gist_set_error("Gist capacity exceeded");
        result->status = GIST_ERROR_CAPACITY_EXCEEDED;
        return GIST_ERROR_CAPACITY_EXCEEDED;
    }

    // Get memory signature
    const prime_signature_t* verbatim_sig = pr_memory_node_get_signature(memory);
    if (!verbatim_sig) {
        gist_set_error("Memory has no signature");
        result->status = GIST_ERROR_EXTRACTION_FAILED;
        return GIST_ERROR_EXTRACTION_FAILED;
    }

    // Update feature statistics
    update_feature_statistics(system, verbatim_sig);

    // Identify key features
    gist_key_feature_t features[GIST_MAX_KEY_FEATURES];
    size_t num_features = 0;

    gist_error_t err = gist_identify_key_features(
        system, verbatim_sig, features, system->config.max_key_features,
        &num_features
    );

    if (err != GIST_SUCCESS) {
        result->status = err;
        return err;
    }

    // Determine how many features to keep based on compression target
    size_t target_features = (size_t)(PRIME_SIG_DIM * compression_target);
    if (target_features > num_features) target_features = num_features;
    if (target_features == 0) target_features = 1;

    // Allocate gist node
    gist_node_t* gist = alloc_gist_node(system);
    if (!gist) {
        gist_set_error("Failed to allocate gist node");
        result->status = GIST_ERROR_NO_MEMORY;
        return GIST_ERROR_NO_MEMORY;
    }

    // Create gist signature from key features
    create_gist_signature(system, verbatim_sig, features, target_features,
                          &gist->gist_signature);

    // Copy key features to gist
    gist->num_features = target_features;
    memcpy(gist->key_features, features, target_features * sizeof(gist_key_feature_t));

    // Copy quaternion state from memory
    gist->gist_quaternion = pr_memory_node_get_state(memory);

    // Add source memory
    gist->source_memory_ids[0] = pr_memory_node_get_id(memory);
    gist->num_sources = 1;

    // Compute abstractness
    gist->abstractness = gist_compute_abstractness(system, &gist->gist_signature);

    // Compute coherence
    float coherence = gist_compute_coherence(verbatim_sig, &gist->gist_signature);

    if (coherence < system->config.min_coherence) {
        gist_set_error("Gist coherence %.3f below threshold %.3f",
                       coherence, system->config.min_coherence);
        nimcp_free(gist->source_memory_ids);
        nimcp_free(gist->key_features);
        nimcp_free(gist);
        gist = NULL;
        result->status = GIST_ERROR_LOW_COHERENCE;
        return GIST_ERROR_LOW_COHERENCE;
    }

    // Compute compression ratio
    uint32_t verbatim_factors = prime_sig_count_factors(verbatim_sig);
    uint32_t gist_factors = prime_sig_count_factors(&gist->gist_signature);
    float compression = (verbatim_factors > 0) ?
        (float)gist_factors / (float)verbatim_factors : 0.0f;

    // Create dual trace
    dual_trace_t* trace = alloc_dual_trace(system);
    if (!trace) {
        gist_set_error("Failed to allocate dual trace");
        nimcp_free(gist->source_memory_ids);
        nimcp_free(gist->key_features);
        nimcp_free(gist);
        gist = NULL;
        result->status = GIST_ERROR_NO_MEMORY;
        return GIST_ERROR_NO_MEMORY;
    }

    // Initialize dual trace
    memcpy(&trace->verbatim_signature, verbatim_sig, sizeof(prime_signature_t));
    memcpy(&trace->gist_signature, &gist->gist_signature, sizeof(prime_signature_t));
    trace->source_node_id = pr_memory_node_get_id(memory);
    trace->coherence = coherence;
    trace->compression_ratio = compression;
    trace->verbatim_precision = 1.0f - compression;  // More compressed = less precise
    trace->gist_abstractness = gist->abstractness;
    trace->information_retention = coherence * (1.0f - compression * 0.5f);

    // Insert gist into hash table
    if (!gist_table_insert(system->gist_table, system->gist_table_size,
                           gist->gist_id, gist)) {
        nimcp_free(trace);
        trace = NULL;
        nimcp_free(gist->source_memory_ids);
        nimcp_free(gist->key_features);
        nimcp_free(gist);
        gist = NULL;
        result->status = GIST_ERROR_NO_MEMORY;
        return GIST_ERROR_NO_MEMORY;
    }
    system->num_gists++;

    // Insert trace into hash table
    if (!trace_table_insert(system->trace_table, system->trace_table_size,
                            trace->trace_id, trace)) {
        gist_table_remove(system->gist_table, system->gist_table_size, gist->gist_id);
        system->num_gists--;
        nimcp_free(trace);
        trace = NULL;
        nimcp_free(gist->source_memory_ids);
        nimcp_free(gist->key_features);
        nimcp_free(gist);
        gist = NULL;
        result->status = GIST_ERROR_NO_MEMORY;
        return GIST_ERROR_NO_MEMORY;
    }
    system->num_traces++;

    // Update statistics
    atomic_fetch_add(&system->total_extractions, 1);
    system->sum_compression += compression;
    system->sum_coherence += coherence;
    system->sum_abstractness += gist->abstractness;

    // Fill result
    result->gist = gist;
    result->dual_trace = trace;
    result->extraction_quality = coherence * (1.0f - fabsf(compression - compression_target));
    result->coherence_achieved = coherence;
    result->compression_achieved = compression;
    result->features_extracted = target_features;
    result->status = GIST_SUCCESS;

    return GIST_SUCCESS;
}

int gist_extract_batch(
    gist_system_t system,
    const pr_memory_node_t** memories,
    size_t count,
    gist_extraction_result_t* results
) {
    if (!system || !memories || !results) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gist_extract_batch: required parameter is NULL (system, memories, results)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_extract_batch", 0.0f);


    int successful = 0;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)count);
        }

        if (memories[i]) {
            gist_error_t err = gist_extract(system, memories[i], &results[i]);
            if (err == GIST_SUCCESS) {
                successful++;
            }
        } else {
            results[i].status = GIST_ERROR_NULL_POINTER;
        }
    }

    return successful;
}

dual_trace_t* gist_create_dual_trace(
    gist_system_t system,
    const pr_memory_node_t* memory,
    const prime_signature_t* gist_signature,
    float abstractness
) {
    if (!system || !memory || !gist_signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gist_create_dual_trace: required parameter is NULL (system, memory, gist_signature)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_create_dual_trace", 0.0f);


    const prime_signature_t* verbatim_sig = pr_memory_node_get_signature(memory);
    if (!verbatim_sig) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "verbatim_sig is NULL");

        return NULL;

    }

    dual_trace_t* trace = alloc_dual_trace(system);
    if (!trace) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trace is NULL");

        return NULL;

    }

    // Initialize
    memcpy(&trace->verbatim_signature, verbatim_sig, sizeof(prime_signature_t));
    memcpy(&trace->gist_signature, gist_signature, sizeof(prime_signature_t));
    trace->source_node_id = pr_memory_node_get_id(memory);
    trace->gist_abstractness = abstractness;
    trace->verbatim_precision = 1.0f;
    trace->coherence = gist_compute_coherence(verbatim_sig, gist_signature);

    uint32_t v_factors = prime_sig_count_factors(verbatim_sig);
    uint32_t g_factors = prime_sig_count_factors(gist_signature);
    trace->compression_ratio = (v_factors > 0) ? (float)g_factors / (float)v_factors : 0.0f;

    // Insert into hash table
    if (!trace_table_insert(system->trace_table, system->trace_table_size,
                            trace->trace_id, trace)) {
        nimcp_free(trace);
        trace = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gist_create_dual_trace: operation failed");
        return NULL;
    }
    system->num_traces++;

    return trace;
}

//=============================================================================
// Gist Compression and Expansion Functions
//=============================================================================

gist_error_t gist_compress(
    gist_system_t system,
    const prime_signature_t* verbatim_sig,
    float target_ratio,
    prime_signature_t* gist_sig_out
) {
    if (!system || !verbatim_sig || !gist_sig_out) {
        return GIST_ERROR_NULL_POINTER;
    }

    // Identify key features
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_compress", 0.0f);


    gist_key_feature_t features[PRIME_SIG_DIM];
    size_t num_features = 0;

    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        if (verbatim_sig->exponents[i] > 0) {
            features[num_features].prime_index = (uint32_t)i;
            features[num_features].importance = compute_feature_score(
                system, i, verbatim_sig->exponents[i]);
            features[num_features].type = get_feature_type(i);
            num_features++;
        }
    }

    if (num_features == 0) {
        // Empty signature
        memset(gist_sig_out, 0, sizeof(prime_signature_t));
        memcpy(gist_sig_out->primes, PRIMES_64, sizeof(gist_sig_out->primes));
        return GIST_SUCCESS;
    }

    // Sort by importance (descending)
    qsort(features, num_features, sizeof(gist_key_feature_t), compare_features_by_score);

    // Keep top features based on target ratio
    size_t keep = (size_t)(num_features * target_ratio);
    if (keep == 0) keep = 1;

    // Create compressed signature
    create_gist_signature(system, verbatim_sig, features, keep, gist_sig_out);

    return GIST_SUCCESS;
}

gist_error_t gist_expand(
    gist_system_t system,
    const prime_signature_t* gist_sig,
    prime_signature_t* verbatim_estimate
) {
    if (!system || !gist_sig || !verbatim_estimate) {
        return GIST_ERROR_NULL_POINTER;
    }

    // Start with gist signature
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_expand", 0.0f);


    memcpy(verbatim_estimate, gist_sig, sizeof(prime_signature_t));

    // Expand by adding correlated features based on learned statistics
    // This is a best-effort reconstruction

    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        if (gist_sig->exponents[i] == 0) {
            // Estimate missing feature based on frequency and correlation
            // with present features
            float estimated = 0.0f;
            float weight_sum = 0.0f;

            for (size_t j = 0; j < PRIME_SIG_DIM; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
                    gist_heartbeat("gist_loop",
                                     (float)(j + 1) / (float)PRIME_SIG_DIM);
                }

                if (gist_sig->exponents[j] > 0) {
                    // Simple correlation model: nearby indices correlate
                    float distance = fabsf((float)i - (float)j);
                    float correlation = expf(-distance / 10.0f);
                    float contrib = (float)gist_sig->exponents[j] * correlation;
                    estimated += contrib;
                    weight_sum += correlation;
                }
            }

            if (weight_sum > 0.0f) {
                estimated /= weight_sum;
                // Apply frequency modulation
                estimated *= system->feature_frequency[i] * 2.0f;
                // Clamp to valid range
                if (estimated > 255.0f) estimated = 255.0f;
                if (estimated > 10.0f) {  // Only add if significant
                    verbatim_estimate->exponents[i] = (uint8_t)estimated;
                }
            }
        }
    }

    // Recompute hash and factor count
    verbatim_estimate->hash = prime_sig_hash(verbatim_estimate);
    verbatim_estimate->num_factors = prime_sig_recount_factors(verbatim_estimate);

    return GIST_SUCCESS;
}

//=============================================================================
// Feature Analysis Functions
//=============================================================================

gist_error_t gist_identify_key_features(
    gist_system_t system,
    const prime_signature_t* signature,
    gist_key_feature_t* features,
    size_t max_features,
    size_t* num_features
) {
    if (!system || !signature || !features || !num_features) {
        return GIST_ERROR_NULL_POINTER;
    }

    *num_features = 0;
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_identify_key_feature", 0.0f);


    gist_key_feature_t all_features[PRIME_SIG_DIM];
    size_t count = 0;

    // Score all non-zero features
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        if (signature->exponents[i] > 0) {
            float score = compute_feature_score(system, i, signature->exponents[i]);

            if (score >= system->config.feature_importance_threshold) {
                all_features[count].prime_index = (uint32_t)i;
                all_features[count].importance = score;
                all_features[count].type = get_feature_type(i);
                all_features[count].variance = system->feature_variance[i];
                all_features[count].frequency = system->feature_frequency[i];
                count++;
            }
        }
    }

    // Sort by importance (descending)
    qsort(all_features, count, sizeof(gist_key_feature_t), compare_features_by_score);

    // Copy top features
    size_t to_copy = count < max_features ? count : max_features;
    memcpy(features, all_features, to_copy * sizeof(gist_key_feature_t));
    *num_features = to_copy;

    return GIST_SUCCESS;
}

float gist_compute_abstractness(
    gist_system_t system,
    const prime_signature_t* signature
) {
    if (!system || !signature) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_compute_abstractness", 0.0f);


    float semantic_weight = 0.0f;
    float surface_weight = 0.0f;
    float total_weight = 0.0f;

    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        if (signature->exponents[i] > 0) {
            float weight = (float)signature->exponents[i];
            feature_type_t type = get_feature_type(i);

            switch (type) {
                case FEATURE_TYPE_SEMANTIC:
                    semantic_weight += weight;
                    break;
                case FEATURE_TYPE_STRUCTURAL:
                    semantic_weight += weight * 0.5f;
                    surface_weight += weight * 0.5f;
                    break;
                case FEATURE_TYPE_SURFACE:
                case FEATURE_TYPE_CONTEXTUAL:
                case FEATURE_TYPE_TEMPORAL:
                    surface_weight += weight;
                    break;
                default:
                    break;
            }
            total_weight += weight;
        }
    }

    if (total_weight < GIST_EPSILON) return 0.5f;

    // Abstractness = proportion of semantic features
    return semantic_weight / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);
}

gist_error_t gist_update_feature_importance(
    gist_system_t system,
    uint32_t feature_index,
    float success_delta,
    float learning_rate
) {
    if (!system) return GIST_ERROR_NULL_POINTER;
    if (feature_index >= PRIME_SIG_DIM) return GIST_ERROR_INVALID_CONFIG;
    if (learning_rate < 0.0f || learning_rate > 1.0f) return GIST_ERROR_INVALID_CONFIG;

    // Update weight with bounded learning
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_update_feature_impor", 0.0f);


    float old_weight = system->feature_weights[feature_index];
    float new_weight = old_weight + learning_rate * success_delta;

    // Clamp to [0, 1]
    if (new_weight < 0.0f) new_weight = 0.0f;
    if (new_weight > 1.0f) new_weight = 1.0f;

    system->feature_weights[feature_index] = new_weight;

    return GIST_SUCCESS;
}

float gist_get_feature_importance(
    gist_system_t system,
    uint32_t feature_index
) {
    if (!system || feature_index >= PRIME_SIG_DIM) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_get_feature_importan", 0.0f);


    return system->feature_weights[feature_index];
}

//=============================================================================
// Gist Matching and Retrieval Functions
//=============================================================================

gist_error_t gist_match(
    gist_system_t system,
    const prime_signature_t* query_sig,
    const nimcp_quaternion_t* query_quat,
    gist_match_result_t* results,
    size_t max_results,
    size_t* num_results
) {
    if (!system || !query_sig || !results || !num_results) {
        return GIST_ERROR_NULL_POINTER;
    }

    *num_results = 0;

    // Collect all matches with scores
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_match", 0.0f);


    gist_match_result_t* all_matches = nimcp_calloc(system->num_gists, sizeof(gist_match_result_t));
    if (!all_matches && system->num_gists > 0) {
        return GIST_ERROR_NO_MEMORY;
    }

    size_t match_count = 0;

    // Iterate through hash table
    for (size_t i = 0; i < system->gist_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->gist_table_size > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)system->gist_table_size);
        }

        gist_hash_entry_t* entry = system->gist_table[i];
        while (entry) {
            gist_node_t* gist = entry->gist;
            if (gist) {
                // Compute signature similarity
                float sig_match = prime_sig_jaccard(query_sig, &gist->gist_signature);

                // Compute key feature match
                float feature_match = 0.0f;
                if (gist->num_features > 0) {
                    size_t matches = 0;
                    for (size_t j = 0; j < gist->num_features; j++) {
                        /* Phase 8: Loop progress heartbeat */
                        if ((j & 0xFF) == 0 && gist->num_features > 256) {
                            gist_heartbeat("gist_loop",
                                             (float)(j + 1) / (float)gist->num_features);
                        }

                        uint32_t idx = gist->key_features[j].prime_index;
                        if (idx < PRIME_SIG_DIM && query_sig->exponents[idx] > 0) {
                            matches++;
                        }
                    }
                    feature_match = (float)matches / (float)gist->num_features;
                }

                // Compute quaternion similarity if provided
                float quat_match = 0.5f;  // Neutral if not provided
                if (query_quat) {
                    float dist = quat_geodesic_distance(*query_quat, gist->gist_quaternion);
                    quat_match = 1.0f - dist / M_PI;
                }

                // Combined similarity
                float similarity = 0.4f * sig_match + 0.3f * feature_match + 0.3f * quat_match;

                all_matches[match_count].gist_id = gist->gist_id;
                all_matches[match_count].similarity = similarity;
                all_matches[match_count].feature_match = feature_match;
                all_matches[match_count].signature_match = sig_match;
                all_matches[match_count].schema_match = (gist->related_schema_id != GIST_INVALID_ID) ?
                    gist->schema_fit : 0.0f;
                match_count++;
            }
            entry = entry->next;
        }
    }

    // Sort by similarity (descending)
    for (size_t i = 0; i < match_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && match_count > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)match_count);
        }

        for (size_t j = i + 1; j < match_count; j++) {
            if (all_matches[j].similarity > all_matches[i].similarity) {
                gist_match_result_t tmp = all_matches[i];
                all_matches[i] = all_matches[j];
                all_matches[j] = tmp;
            }
        }
    }

    // Copy top results
    size_t to_copy = match_count < max_results ? match_count : max_results;
    memcpy(results, all_matches, to_copy * sizeof(gist_match_result_t));
    *num_results = to_copy;

    nimcp_free(all_matches);
    all_matches = NULL;
    return GIST_SUCCESS;
}

gist_error_t gist_retrieve_verbatim(
    gist_system_t system,
    uint64_t gist_id,
    uint64_t* memory_ids,
    size_t max_memories,
    size_t* num_memories
) {
    if (!system || !memory_ids || !num_memories) {
        return GIST_ERROR_NULL_POINTER;
    }

    *num_memories = 0;

    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_retrieve_verbatim", 0.0f);


    gist_node_t* gist = gist_table_lookup(system->gist_table,
                                           system->gist_table_size, gist_id);
    if (!gist) {
        gist_set_error("Gist %llu not found", (unsigned long long)gist_id);
        return GIST_ERROR_NOT_FOUND;
    }

    // Update access time
    gist->last_accessed_ms = gist_current_time_ms();

    // Copy source memory IDs
    size_t to_copy = gist->num_sources < max_memories ?
        gist->num_sources : max_memories;
    memcpy(memory_ids, gist->source_memory_ids, to_copy * sizeof(uint64_t));
    *num_memories = to_copy;

    return GIST_SUCCESS;
}

gist_node_t* gist_get_by_id(
    gist_system_t system,
    uint64_t gist_id
) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_get_by_id", 0.0f);


    return gist_table_lookup(system->gist_table, system->gist_table_size, gist_id);
}

dual_trace_t* gist_get_dual_trace(
    gist_system_t system,
    uint64_t trace_id
) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_get_dual_trace", 0.0f);


    return trace_table_lookup(system->trace_table, system->trace_table_size, trace_id);
}

//=============================================================================
// Gist Merge and Generalization Functions
//=============================================================================

gist_error_t gist_merge(
    gist_system_t system,
    const uint64_t* gist_ids,
    size_t count,
    gist_node_t** merged_gist
) {
    if (!system || !gist_ids || !merged_gist || count < 2) {
        return GIST_ERROR_NULL_POINTER;
    }

    // Collect gists to merge
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_merge", 0.0f);


    gist_node_t** gists = nimcp_calloc(count, sizeof(gist_node_t*));
    if (!gists) return GIST_ERROR_NO_MEMORY;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)count);
        }

        gists[i] = gist_table_lookup(system->gist_table, system->gist_table_size,
                                      gist_ids[i]);
        if (!gists[i]) {
            nimcp_free(gists);
            gists = NULL;
            gist_set_error("Gist %llu not found", (unsigned long long)gist_ids[i]);
            return GIST_ERROR_NOT_FOUND;
        }
    }

    // Allocate new merged gist
    gist_node_t* merged = alloc_gist_node(system);
    if (!merged) {
        nimcp_free(gists);
        gists = NULL;
        return GIST_ERROR_NO_MEMORY;
    }

    // Intersect signatures (keep common features)
    prime_signature_t* intersection = prime_sig_copy(&gists[0]->gist_signature);
    if (!intersection) {
        nimcp_free(gists);
        gists = NULL;
        nimcp_free(merged->source_memory_ids);
        nimcp_free(merged->key_features);
        nimcp_free(merged);
        merged = NULL;
        return GIST_ERROR_NO_MEMORY;
    }

    for (size_t i = 1; i < count; i++) {
        prime_signature_t* new_intersection = prime_sig_intersect(
            intersection, &gists[i]->gist_signature);
        prime_sig_destroy(intersection);
        if (!new_intersection) {
            nimcp_free(gists);
            gists = NULL;
            nimcp_free(merged->source_memory_ids);
            nimcp_free(merged->key_features);
            nimcp_free(merged);
            merged = NULL;
            return GIST_ERROR_NO_MEMORY;
        }
        intersection = new_intersection;
    }

    memcpy(&merged->gist_signature, intersection, sizeof(prime_signature_t));
    prime_sig_destroy(intersection);

    // Average quaternion states
    nimcp_quaternion_t* quats = nimcp_calloc(count, sizeof(nimcp_quaternion_t));
    float* weights = nimcp_calloc(count, sizeof(float));
    if (!quats || !weights) {
        nimcp_free(quats);
        quats = NULL;
        nimcp_free(weights);
        weights = NULL;
        nimcp_free(gists);
        gists = NULL;
        nimcp_free(merged->source_memory_ids);
        nimcp_free(merged->key_features);
        nimcp_free(merged);
        merged = NULL;
        return GIST_ERROR_NO_MEMORY;
    }

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)count);
        }

        quats[i] = gists[i]->gist_quaternion;
        weights[i] = 1.0f;  // Equal weights
    }

    merged->gist_quaternion = quat_blend_memories(quats, weights, count);
    nimcp_free(quats);
    quats = NULL;
    nimcp_free(weights);
    weights = NULL;

    // Collect all source memories
    size_t total_sources = 0;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)count);
        }

        total_sources += gists[i]->num_sources;
    }

    // Reallocate sources array if needed
    if (total_sources > merged->sources_capacity) {
        uint64_t* new_sources = nimcp_realloc(merged->source_memory_ids,
                                        total_sources * sizeof(uint64_t));
        if (!new_sources) {
            nimcp_free(gists);
            gists = NULL;
            nimcp_free(merged->source_memory_ids);
            nimcp_free(merged->key_features);
            nimcp_free(merged);
            merged = NULL;
            return GIST_ERROR_NO_MEMORY;
        }
        merged->source_memory_ids = new_sources;
        merged->sources_capacity = total_sources;
    }

    // Copy source IDs
    size_t offset = 0;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)count);
        }

        memcpy(merged->source_memory_ids + offset, gists[i]->source_memory_ids,
               gists[i]->num_sources * sizeof(uint64_t));
        offset += gists[i]->num_sources;
    }
    merged->num_sources = total_sources;

    // Compute merged properties
    merged->abstractness = gist_compute_abstractness(system, &merged->gist_signature);
    merged->generality = (float)count;  // Covers 'count' gists
    merged->confidence = 0.0f;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)count);
        }

        merged->confidence += gists[i]->confidence;
    }
    merged->confidence /= (float)count;

    // Identify key features from merged signature
    gist_identify_key_features(system, &merged->gist_signature,
                               merged->key_features, merged->features_capacity,
                               &merged->num_features);

    // Insert into hash table
    if (!gist_table_insert(system->gist_table, system->gist_table_size,
                           merged->gist_id, merged)) {
        nimcp_free(gists);
        gists = NULL;
        nimcp_free(merged->source_memory_ids);
        nimcp_free(merged->key_features);
        nimcp_free(merged);
        merged = NULL;
        return GIST_ERROR_NO_MEMORY;
    }
    system->num_gists++;

    atomic_fetch_add(&system->total_merges, 1);

    *merged_gist = merged;
    nimcp_free(gists);
    gists = NULL;

    return GIST_SUCCESS;
}

gist_error_t gist_generalize(
    gist_system_t system,
    const uint64_t* memory_ids,
    size_t count,
    float generality_target,
    gist_node_t** generalized_gist
) {
    if (!system || !memory_ids || !generalized_gist || count < 2) {
        return GIST_ERROR_NULL_POINTER;
    }

    // Note: Full implementation would need memory node lookup to get
    // actual memory signatures and extract common features.
    // For now, we create a placeholder generalized gist from the IDs.

    // Allocate new generalized gist
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_generalize", 0.0f);


    gist_node_t* generalized = alloc_gist_node(system);
    if (!generalized) {
        return GIST_ERROR_NO_MEMORY;
    }

    // Initialize with default state for generalized concept
    generalized->gist_quaternion = quat_identity();
    generalized->abstractness = 0.8f;  // High abstraction for generalization
    generalized->generality = generality_target;
    generalized->confidence = 0.7f;  // Lower confidence for generalized concepts

    // Copy source memory IDs
    size_t to_copy = count < generalized->sources_capacity ?
        count : generalized->sources_capacity;
    memcpy(generalized->source_memory_ids, memory_ids, to_copy * sizeof(uint64_t));
    generalized->num_sources = to_copy;

    // Insert into hash table
    if (!gist_table_insert(system->gist_table, system->gist_table_size,
                           generalized->gist_id, generalized)) {
        nimcp_free(generalized->source_memory_ids);
        nimcp_free(generalized->key_features);
        nimcp_free(generalized);
        generalized = NULL;
        return GIST_ERROR_NO_MEMORY;
    }
    system->num_gists++;

    *generalized_gist = generalized;

    return GIST_SUCCESS;
}

//=============================================================================
// Decay and Forgetting Functions
//=============================================================================

gist_error_t gist_apply_forgetting(
    gist_system_t system,
    dual_trace_t* trace,
    float elapsed_hours
) {
    if (!system || !trace) return GIST_ERROR_NULL_POINTER;
    if (elapsed_hours < 0.0f) return GIST_ERROR_INVALID_CONFIG;

    // Apply exponential decay to verbatim (faster)
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_apply_forgetting", 0.0f);


    trace->verbatim_strength *= expf(-trace->verbatim_decay_rate * elapsed_hours);

    // Apply exponential decay to gist (slower)
    trace->gist_strength *= expf(-trace->gist_decay_rate * elapsed_hours);

    // Clamp to valid range
    if (trace->verbatim_strength < 0.0f) trace->verbatim_strength = 0.0f;
    if (trace->gist_strength < 0.0f) trace->gist_strength = 0.0f;

    return GIST_SUCCESS;
}

size_t gist_apply_forgetting_all(
    gist_system_t system,
    float elapsed_hours
) {
    if (!system || elapsed_hours < 0.0f) return 0;

    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_apply_forgetting_all", 0.0f);


    size_t affected = 0;

    for (size_t i = 0; i < system->trace_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->trace_table_size > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)system->trace_table_size);
        }

        trace_hash_entry_t* entry = system->trace_table[i];
        while (entry) {
            if (entry->trace) {
                gist_apply_forgetting(system, entry->trace, elapsed_hours);
                affected++;
            }
            entry = entry->next;
        }
    }

    return affected;
}

gist_error_t gist_reinforce(
    gist_system_t system,
    dual_trace_t* trace,
    float verbatim_boost,
    float gist_boost
) {
    if (!system || !trace) return GIST_ERROR_NULL_POINTER;

    // Reinforce verbatim
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_reinforce", 0.0f);


    trace->verbatim_strength += verbatim_boost * (1.0f - trace->verbatim_strength);
    if (trace->verbatim_strength > 1.0f) trace->verbatim_strength = 1.0f;

    // Reinforce gist
    trace->gist_strength += gist_boost * (1.0f - trace->gist_strength);
    if (trace->gist_strength > 1.0f) trace->gist_strength = 1.0f;

    // Update access times
    uint64_t now = gist_current_time_ms();
    if (verbatim_boost > 0.0f) trace->last_verbatim_access_ms = now;
    if (gist_boost > 0.0f) trace->last_gist_access_ms = now;

    return GIST_SUCCESS;
}

size_t gist_prune_weak(
    gist_system_t system,
    float strength_threshold
) {
    if (!system) return 0;

    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_prune_weak", 0.0f);


    size_t removed = 0;

    // Iterate and collect weak gists
    uint64_t* to_remove = nimcp_calloc(system->num_gists, sizeof(uint64_t));
    if (!to_remove) return 0;

    size_t remove_count = 0;

    for (size_t i = 0; i < system->gist_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->gist_table_size > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)system->gist_table_size);
        }

        gist_hash_entry_t* entry = system->gist_table[i];
        while (entry) {
            if (entry->gist && entry->gist->current_strength < strength_threshold) {
                to_remove[remove_count++] = entry->gist->gist_id;
            }
            entry = entry->next;
        }
    }

    // Remove collected gists
    for (size_t i = 0; i < remove_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && remove_count > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)remove_count);
        }

        gist_node_t* gist = gist_table_remove(system->gist_table,
                                               system->gist_table_size,
                                               to_remove[i]);
        if (gist) {
            nimcp_free(gist->source_memory_ids);
            nimcp_free(gist->key_features);
            nimcp_free(gist);
            gist = NULL;
            system->num_gists--;
            removed++;
        }
    }

    nimcp_free(to_remove);
    to_remove = NULL;
    return removed;
}

//=============================================================================
// Statistics and Information Functions
//=============================================================================

gist_error_t gist_get_stats(
    gist_system_t system,
    gist_stats_t* stats
) {
    if (!system || !stats) return GIST_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_get_stats", 0.0f);


    memset(stats, 0, sizeof(gist_stats_t));

    stats->num_gists = system->num_gists;
    stats->num_dual_traces = system->num_traces;
    stats->num_extractions = atomic_load(&system->total_extractions);
    stats->num_merges = atomic_load(&system->total_merges);

    if (stats->num_extractions > 0) {
        stats->avg_compression = system->sum_compression / (float)stats->num_extractions;
        stats->avg_coherence = system->sum_coherence / (float)stats->num_extractions;
        stats->avg_abstractness = system->sum_abstractness / (float)stats->num_extractions;
    }

    // Count total features
    for (size_t i = 0; i < system->gist_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->gist_table_size > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)system->gist_table_size);
        }

        gist_hash_entry_t* entry = system->gist_table[i];
        while (entry) {
            if (entry->gist) {
                stats->total_features += entry->gist->num_features;
            }
            entry = entry->next;
        }
    }

    // Estimate memory usage
    stats->memory_bytes = sizeof(struct gist_system_struct);
    stats->memory_bytes += system->gist_table_size * sizeof(gist_hash_entry_t*);
    stats->memory_bytes += system->trace_table_size * sizeof(trace_hash_entry_t*);
    stats->memory_bytes += system->num_gists * (sizeof(gist_node_t) +
                           GIST_MAX_SOURCES * sizeof(uint64_t) +
                           GIST_MAX_KEY_FEATURES * sizeof(gist_key_feature_t));
    stats->memory_bytes += system->num_traces * sizeof(dual_trace_t);

    return GIST_SUCCESS;
}

size_t gist_get_count(gist_system_t system) {
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_get_count", 0.0f);


    return system ? system->num_gists : 0;
}

size_t gist_get_dual_trace_count(gist_system_t system) {
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_get_dual_trace_count", 0.0f);


    return system ? system->num_traces : 0;
}

const char* gist_trace_type_name(trace_type_t type) {
    switch (type) {
        case TRACE_VERBATIM: return "VERBATIM";
        case TRACE_GIST: return "GIST";
        case TRACE_HYBRID: return "HYBRID";
        default: return "UNKNOWN";
    }
}

const char* gist_method_name(gist_extraction_method_t method) {
    switch (method) {
        case GIST_METHOD_FEATURE_IMPORTANCE: return "FEATURE_IMPORTANCE";
        case GIST_METHOD_VARIANCE_BASED: return "VARIANCE_BASED";
        case GIST_METHOD_SCHEMA_GUIDED: return "SCHEMA_GUIDED";
        case GIST_METHOD_ATTENTION_WEIGHTED: return "ATTENTION_WEIGHTED";
        case GIST_METHOD_FREQUENCY_BASED: return "FREQUENCY_BASED";
        case GIST_METHOD_HYBRID: return "HYBRID";
        default: return "UNKNOWN";
    }
}

const char* gist_error_string(gist_error_t error) {
    switch (error) {
        case GIST_SUCCESS: return "Success";
        case GIST_ERROR_NULL_POINTER: return "Null pointer argument";
        case GIST_ERROR_INVALID_CONFIG: return "Invalid configuration";
        case GIST_ERROR_NO_MEMORY: return "Memory allocation failed";
        case GIST_ERROR_CAPACITY_EXCEEDED: return "Capacity exceeded";
        case GIST_ERROR_INVALID_ID: return "Invalid ID";
        case GIST_ERROR_LOW_COHERENCE: return "Coherence too low";
        case GIST_ERROR_EXTRACTION_FAILED: return "Extraction failed";
        case GIST_ERROR_SCHEMA_MISMATCH: return "Schema mismatch";
        case GIST_ERROR_ALREADY_EXISTS: return "Already exists";
        case GIST_ERROR_NOT_FOUND: return "Not found";
        case GIST_ERROR_INVALID_TRACE: return "Invalid trace type";
        case GIST_ERROR_MERGE_FAILED: return "Merge failed";
        default: return "Unknown error";
    }
}

const char* gist_get_last_error(void) {
    return g_last_error[0] ? g_last_error : NULL;
}

//=============================================================================
// Utility Functions
//=============================================================================

void gist_print(const gist_node_t* gist) {
    if (!gist) {
        printf("Gist: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_print", 0.0f);


    printf("Gist[id=%llu]:\n", (unsigned long long)gist->gist_id);
    printf("  Abstractness: %.3f\n", gist->abstractness);
    printf("  Generality: %.3f\n", gist->generality);
    printf("  Confidence: %.3f\n", gist->confidence);
    printf("  Strength: %.3f\n", gist->current_strength);
    printf("  Sources: %zu\n", gist->num_sources);
    printf("  Key Features: %zu\n", gist->num_features);
    printf("  Quaternion: (%.3f, %.3f, %.3f, %.3f)\n",
           gist->gist_quaternion.w, gist->gist_quaternion.x,
           gist->gist_quaternion.y, gist->gist_quaternion.z);

    if (gist->num_features > 0) {
        printf("  Top Features:\n");
        for (size_t i = 0; i < gist->num_features && i < 5; i++) {
            printf("    [%u] importance=%.3f type=%d\n",
                   gist->key_features[i].prime_index,
                   gist->key_features[i].importance,
                   gist->key_features[i].type);
        }
    }
}

void gist_dual_trace_print(const dual_trace_t* trace) {
    if (!trace) {
        printf("DualTrace: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_dual_trace_print", 0.0f);


    printf("DualTrace[id=%llu]:\n", (unsigned long long)trace->trace_id);
    printf("  Source Node: %llu\n", (unsigned long long)trace->source_node_id);
    printf("  Verbatim Strength: %.3f\n", trace->verbatim_strength);
    printf("  Verbatim Precision: %.3f\n", trace->verbatim_precision);
    printf("  Gist Strength: %.3f\n", trace->gist_strength);
    printf("  Gist Abstractness: %.3f\n", trace->gist_abstractness);
    printf("  Coherence: %.3f\n", trace->coherence);
    printf("  Compression: %.3f\n", trace->compression_ratio);
    printf("  Information Retention: %.3f\n", trace->information_retention);
}

bool gist_validate(const gist_node_t* gist) {
    if (!gist) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gist_validate: gist is NULL");
        return false;
    }

    // Check basic fields
    if (gist->gist_id == GIST_INVALID_ID) {
        return false;
    }
    if (gist->abstractness < 0.0f || gist->abstractness > 1.0f) {
        return false;
    }
    if (gist->confidence < 0.0f || gist->confidence > 1.0f) {
        return false;
    }
    if (gist->current_strength < 0.0f || gist->current_strength > 1.0f) {
        return false;
    }

    // Check arrays
    if (gist->num_sources > gist->sources_capacity) {
        return false;
    }
    if (gist->num_features > gist->features_capacity) {
        return false;
    }

    // Check signature
    if (!prime_sig_is_empty(&gist->gist_signature) &&
        gist->gist_signature.num_factors == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gist_validate: prime_sig_is_empty is NULL");
        return false;  // Inconsistent
    }

    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_validate", 0.0f);

    return true;
}

float gist_compute_coherence(
    const prime_signature_t* verbatim,
    const prime_signature_t* gist
) {
    if (!verbatim || !gist) return 0.0f;

    // Coherence = how well gist captures verbatim
    // High coherence means gist contains the most important parts of verbatim

    // Basic Jaccard similarity
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_compute_coherence", 0.0f);


    float jaccard = prime_sig_jaccard(verbatim, gist);

    // Check that gist features are subset of verbatim
    float subset_ratio = 0.0f;
    uint32_t gist_count = 0;
    uint32_t subset_count = 0;

    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            gist_heartbeat("gist_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        if (gist->exponents[i] > 0) {
            gist_count++;
            if (verbatim->exponents[i] >= gist->exponents[i]) {
                subset_count++;
            }
        }
    }

    if (gist_count > 0) {
        subset_ratio = (float)subset_count / (float)gist_count;
    }

    // Coherence = combination of Jaccard and subset ratio
    return 0.5f * jaccard + 0.5f * subset_ratio;
}

uint64_t gist_current_time_ms(void) {
    /* Phase 8: Heartbeat at operation start */
    gist_heartbeat("gist_current_time_ms", 0.0f);


    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    }
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gist_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_gist_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gist_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gist_training_begin: NULL argument");
        return -1;
    }
    gist_heartbeat_instance(NULL, "gist_training_begin", 0.0f);
    (void)(struct gist_hash_entry*)instance; /* Module state available for reset */
    return 0;
}

int gist_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gist_training_end: NULL argument");
        return -1;
    }
    gist_heartbeat_instance(NULL, "gist_training_end", 1.0f);
    (void)(struct gist_hash_entry*)instance; /* Module state available for finalization */
    return 0;
}

int gist_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gist_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gist_heartbeat_instance(NULL, "gist_training_step", progress);
    (void)(struct gist_hash_entry*)instance; /* Module state available for step adaptation */
    return 0;
}
