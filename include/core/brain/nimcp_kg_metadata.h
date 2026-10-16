/**
 * @file nimcp_kg_metadata.h
 * @brief Searchable Metadata System for Knowledge Graph Hierarchy
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Full metadata support at every level of the KG hierarchy
 * WHY:  Enable rich querying, versioning, and self-documentation of brain modules
 * HOW:  Type-safe key-value metadata with specialized level-specific extensions
 *
 * METADATA ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    KG METADATA SYSTEM (4 Levels)                          |
 * +===========================================================================+
 * |                                                                            |
 * |   Base Metadata (kg_metadata_t) - Common to all levels                    |
 * |   -----------------------------------------------------------------        |
 * |   | uuid | name | description | tags | created_at | version | ...  |      |
 * |   -----------------------------------------------------------------        |
 * |   | entries[] - Custom key-value pairs (indexed/encrypted)          |      |
 * |   -----------------------------------------------------------------        |
 * |                                                                            |
 * |   Level 0: System (kg_system_metadata_t)                                  |
 * |   ┌──────────────────────────────────────────────────────────────┐        |
 * |   │ brain_id, config_profile, hardware_profile, total_modules,   │        |
 * |   │ phi_score (consciousness), integration_score, overall_health │        |
 * |   └──────────────────────────────────────────────────────────────┘        |
 * |                                                                            |
 * |   Level 1: Hemisphere (kg_hemisphere_metadata_t)                          |
 * |   ┌──────────────────────────────────────────────────────────────┐        |
 * |   │ hemisphere_id, dominant_function, total_modules,              │        |
 * |   │ specialization_score, interhemispheric_bandwidth             │        |
 * |   └──────────────────────────────────────────────────────────────┘        |
 * |                                                                            |
 * |   Level 2: Layer (kg_layer_metadata_t)                                    |
 * |   ┌──────────────────────────────────────────────────────────────┐        |
 * |   │ layer_index, layer_name, biological_analog, module_count,     │        |
 * |   │ aggregate_health, total_throughput, layer_coherence          │        |
 * |   └──────────────────────────────────────────────────────────────┘        |
 * |                                                                            |
 * |   Level 3: Module (kg_meta_module_t)                                  |
 * |   ┌──────────────────────────────────────────────────────────────┐        |
 * |   │ module_type, subsystem, cortical_layer, hemisphere,           │        |
 * |   │ health_score, uptime_ms, message_count, avg_latency_ms       │        |
 * |   └──────────────────────────────────────────────────────────────┘        |
 * |                                                                            |
 * +===========================================================================+
 * ```
 *
 * VALUE TYPES:
 * - String: UTF-8 text (max 256 chars)
 * - Int64: Signed 64-bit integer
 * - Float64: Double precision floating point
 * - Bool: True/false
 * - Timestamp: Unix epoch milliseconds
 * - Blob: Binary data with size
 * - StringArray: Array of strings
 * - JSON: Nested structured data
 *
 * THREAD SAFETY: All metadata operations are thread-safe
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_METADATA_H
#define NIMCP_KG_METADATA_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum length of metadata key */
#define KG_META_MAX_KEY_LEN         64

/** Maximum length of string value */
#define KG_META_MAX_STRING_LEN      256

/** Maximum length of name field */
#define KG_META_MAX_NAME_LEN        128

/** Maximum length of description field */
#define KG_META_MAX_DESC_LEN        512

/** UUID string length (36 chars + null) */
#define KG_META_UUID_LEN            37

/** Maximum length of version string */
#define KG_META_MAX_VERSION_LEN     32

/** Default initial capacity for metadata entries */
#define KG_META_DEFAULT_CAPACITY    16

/* ============================================================================
 * Metadata Value Types
 * ============================================================================ */

/**
 * @brief Metadata value types
 *
 * WHAT: Enumeration of supported metadata value types
 * WHY:  Enable type-safe storage and retrieval of diverse metadata
 * HOW:  Used with kg_metadata_entry_t union to interpret stored values
 */
typedef enum {
    KG_META_TYPE_STRING = 0,        /**< UTF-8 string value */
    KG_META_TYPE_INT64,             /**< Signed 64-bit integer */
    KG_META_TYPE_FLOAT64,           /**< Double precision float */
    KG_META_TYPE_BOOL,              /**< Boolean true/false */
    KG_META_TYPE_TIMESTAMP,         /**< Unix epoch timestamp (ms) */
    KG_META_TYPE_BLOB,              /**< Binary data with size */
    KG_META_TYPE_STRING_ARRAY,      /**< Array of strings */
    KG_META_TYPE_JSON               /**< Nested structured JSON data */
} kg_meta_type_t;

/* ============================================================================
 * Metadata Entry Structure
 * ============================================================================ */

/**
 * @brief Single metadata key-value entry
 *
 * WHAT: A typed key-value pair for storing arbitrary metadata
 * WHY:  Flexible metadata storage with indexing and encryption support
 * HOW:  Tagged union pattern with type discriminator
 */
typedef struct {
    char key[KG_META_MAX_KEY_LEN];      /**< Metadata key (indexed) */
    kg_meta_type_t type;                 /**< Value type discriminator */

    /** Type-specific value storage */
    union {
        char str_val[KG_META_MAX_STRING_LEN];  /**< String value */
        int64_t int_val;                       /**< Integer value */
        double float_val;                      /**< Float value */
        bool bool_val;                         /**< Boolean value */
        uint64_t timestamp_val;                /**< Timestamp (ms since epoch) */

        /** Binary blob value */
        struct {
            void* data;                        /**< Blob data pointer */
            size_t size;                       /**< Blob size in bytes */
        } blob_val;

        /** String array value */
        struct {
            char** items;                      /**< Array of string pointers */
            uint32_t count;                    /**< Number of strings */
        } str_array_val;

        char* json_val;                        /**< JSON string (heap allocated) */
    } value;

    bool indexed;                        /**< Include in search index */
    bool encrypted;                      /**< Encrypt this field at rest */
} kg_metadata_entry_t;

/* ============================================================================
 * Base Metadata Container
 * ============================================================================ */

/**
 * @brief Metadata container (used at all hierarchy levels)
 *
 * WHAT: Base metadata structure with standard fields and custom entries
 * WHY:  Consistent metadata schema across all hierarchy levels
 * HOW:  Fixed standard fields + dynamic key-value entry array
 *
 * Standard fields are always present and indexed by default.
 * Custom entries provide extensibility for level-specific or user-defined data.
 */
typedef struct {
    /* Dynamic custom entries */
    kg_metadata_entry_t* entries;        /**< Array of custom entries */
    uint32_t entry_count;                /**< Number of entries in use */
    uint32_t entry_capacity;             /**< Allocated capacity */

    /* Standard fields (always present) */
    char uuid[KG_META_UUID_LEN];         /**< Unique identifier (UUID v4) */
    char name[KG_META_MAX_NAME_LEN];     /**< Human-readable name */
    char description[KG_META_MAX_DESC_LEN]; /**< Detailed description */
    char* tags;                          /**< Comma-separated tags (indexed) */
    uint64_t created_at;                 /**< Creation timestamp (ms) */
    uint64_t updated_at;                 /**< Last update timestamp (ms) */
    char created_by[KG_META_MAX_KEY_LEN]; /**< Creator identifier */
    char version[KG_META_MAX_VERSION_LEN]; /**< Semantic version string */
} kg_metadata_t;

/* ============================================================================
 * Level 3: Module-Level Metadata
 * ============================================================================ */

/**
 * @brief Module-level metadata
 *
 * WHAT: Extended metadata for individual brain modules
 * WHY:  Track module type, capabilities, dependencies, and runtime metrics
 * HOW:  Extends base metadata with module-specific fields
 *
 * Runtime fields are updated dynamically as the module operates.
 */
typedef struct {
    kg_metadata_t base;                  /**< Common metadata (inherited) */

    /* Module classification */
    char module_type[32];                /**< Type: "SNN", "LNN", "CNN", "COGNITIVE" */
    char subsystem[32];                  /**< Subsystem: "core", "perception", "cognition" */
    uint8_t cortical_layer;              /**< Target cortical layer (I-VI, 0-5) */
    uint8_t hemisphere;                  /**< Hemisphere: LEFT=0, RIGHT=1, BILATERAL=2 */

    /* Capabilities and dependencies (JSON arrays) */
    char* capabilities;                  /**< JSON array of module capabilities */
    char* dependencies;                  /**< JSON array of required modules */
    char api_version[16];                /**< API compatibility version */

    /* Runtime metrics (updated dynamically) */
    float health_score;                  /**< Health score [0.0-1.0] */
    uint64_t uptime_ms;                  /**< Time since initialization (ms) */
    uint64_t message_count;              /**< Total messages processed */
    float avg_latency_ms;                /**< Average processing latency (ms) */
    char status[32];                     /**< Status: "running", "paused", "error" */
} kg_meta_module_t;

/* ============================================================================
 * Level 2: Layer-Level Metadata
 * ============================================================================ */

/**
 * @brief Layer-level metadata
 *
 * WHAT: Extended metadata for cortical layers
 * WHY:  Track layer composition, connectivity, and aggregate metrics
 * HOW:  Extends base metadata with layer-specific fields
 *
 * Provides summary statistics for all modules within the layer.
 */
typedef struct {
    kg_metadata_t base;                  /**< Common metadata (inherited) */

    /* Layer identification */
    uint8_t layer_index;                 /**< Cortical layer index (I-VI, 0-5) */
    char layer_name[32];                 /**< Layer name: "Input", "Association", "Output" */
    char biological_analog[64];          /**< Cortical layer biological analogy */

    /* Module composition */
    uint32_t module_count;               /**< Number of modules in layer */
    char* module_types;                  /**< JSON: distribution of module types */
    char* connection_summary;            /**< JSON: intra/inter-layer connections */

    /* Aggregate performance metrics */
    float aggregate_health;              /**< Average module health [0.0-1.0] */
    uint64_t total_throughput;           /**< Messages/sec across layer */
    float layer_coherence;               /**< Synchronization measure [0.0-1.0] */
} kg_layer_metadata_t;

/* ============================================================================
 * Level 1: Hemisphere-Level Metadata
 * ============================================================================ */

/**
 * @brief Hemisphere-level metadata
 *
 * WHAT: Extended metadata for brain hemispheres
 * WHY:  Track lateralization, specialization, and cross-hemisphere connectivity
 * HOW:  Extends base metadata with hemisphere-specific fields
 *
 * Supports hemispheric specialization analysis and interhemispheric bandwidth monitoring.
 */
typedef struct {
    kg_metadata_t base;                  /**< Common metadata (inherited) */

    /* Hemisphere identification */
    uint8_t hemisphere_id;               /**< Hemisphere: LEFT=0, RIGHT=1 */
    char dominant_function[64];          /**< Dominant function: "language", "spatial", "bilateral" */

    /* Module composition */
    uint32_t total_modules;              /**< Total modules in hemisphere */
    uint32_t total_connections;          /**< Internal connections within hemisphere */
    char* layer_summary;                 /**< JSON: breakdown by cortical layer */

    /* Lateralization metrics */
    float specialization_score;          /**< Specialization vs general [0.0-1.0] */
    float interhemispheric_bandwidth;    /**< Corpus callosum traffic (msgs/sec) */
    char* cross_hemisphere_bridges;      /**< JSON: major connections to other hemisphere */
} kg_hemisphere_metadata_t;

/* ============================================================================
 * Level 0: System-Level Metadata
 * ============================================================================ */

/**
 * @brief System-level metadata (full brain)
 *
 * WHAT: Extended metadata for the entire brain system
 * WHY:  Track global configuration, health, and consciousness metrics
 * HOW:  Extends base metadata with system-wide fields
 *
 * Includes IIT (Integrated Information Theory) consciousness measures.
 */
typedef struct {
    kg_metadata_t base;                  /**< Common metadata (inherited) */

    /* System identification */
    char brain_id[64];                   /**< Unique brain instance identifier */
    char configuration_profile[64];      /**< Profile: "full", "constrained", "minimal" */
    char hardware_profile[64];           /**< Hardware: "cuda", "cpu_only", "embedded" */

    /* System composition */
    uint32_t total_modules;              /**< Total module count across brain */
    uint32_t total_connections;          /**< Total edge count in KG */
    uint64_t total_parameters;           /**< Total learnable parameters */

    /* System health and performance */
    float overall_health;                /**< Aggregate health score [0.0-1.0] */
    float cognitive_load;                /**< Current processing load [0.0-1.0] */
    uint64_t memory_usage_bytes;         /**< Current memory footprint */
    char* subsystem_status;              /**< JSON: status per subsystem */

    /* Consciousness/self-awareness metrics (IIT-inspired) */
    float phi_score;                     /**< IIT phi - consciousness measure */
    float integration_score;             /**< Information integration level [0.0-1.0] */
    float self_model_accuracy;           /**< Self-model fidelity [0.0-1.0] */
} kg_system_metadata_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create a new metadata container
 *
 * WHAT: Allocate and initialize a metadata container
 * WHY:  Provide empty metadata structure for population
 * HOW:  Allocate struct, initialize fields to defaults, generate UUID
 *
 * @return New metadata container or NULL on allocation failure
 */
kg_metadata_t* kg_metadata_create(void);

/**
 * @brief Destroy a metadata container
 *
 * WHAT: Free all resources associated with metadata
 * WHY:  Clean resource management
 * HOW:  Free entries, tags, then container itself
 *
 * @param meta Metadata container to destroy (NULL safe)
 */
void kg_metadata_destroy(kg_metadata_t* meta);

/**
 * @brief Create a deep copy of metadata
 *
 * WHAT: Duplicate metadata container and all its contents
 * WHY:  Safe copying for modification without affecting original
 * HOW:  Allocate new container, copy all fields including heap data
 *
 * @param meta Metadata to clone
 * @return New metadata container or NULL on failure
 */
kg_metadata_t* kg_metadata_clone(const kg_metadata_t* meta);

/* ============================================================================
 * Metadata Manipulation API - Setters
 * ============================================================================ */

/**
 * @brief Set a string metadata value
 *
 * WHAT: Add or update a string key-value entry
 * WHY:  Store text metadata
 * HOW:  Find or create entry, copy string value
 *
 * @param meta Metadata container
 * @param key Entry key (max 63 chars)
 * @param value String value (max 255 chars)
 * @param indexed Include in search index
 * @return 0 on success, -1 on error
 */
int kg_metadata_set_string(kg_metadata_t* meta, const char* key,
                           const char* value, bool indexed);

/**
 * @brief Set an integer metadata value
 *
 * WHAT: Add or update an integer key-value entry
 * WHY:  Store numeric counts, IDs, etc.
 * HOW:  Find or create entry, store int64 value
 *
 * @param meta Metadata container
 * @param key Entry key
 * @param value Integer value
 * @param indexed Include in search index
 * @return 0 on success, -1 on error
 */
int kg_metadata_set_int(kg_metadata_t* meta, const char* key,
                        int64_t value, bool indexed);

/**
 * @brief Set a floating-point metadata value
 *
 * WHAT: Add or update a float key-value entry
 * WHY:  Store measurements, scores, ratios
 * HOW:  Find or create entry, store double value
 *
 * @param meta Metadata container
 * @param key Entry key
 * @param value Float value
 * @param indexed Include in search index
 * @return 0 on success, -1 on error
 */
int kg_metadata_set_float(kg_metadata_t* meta, const char* key,
                          double value, bool indexed);

/**
 * @brief Set a boolean metadata value
 *
 * WHAT: Add or update a boolean key-value entry
 * WHY:  Store flags, toggles, binary states
 * HOW:  Find or create entry, store bool value
 *
 * @param meta Metadata container
 * @param key Entry key
 * @param value Boolean value
 * @param indexed Include in search index
 * @return 0 on success, -1 on error
 */
int kg_metadata_set_bool(kg_metadata_t* meta, const char* key,
                         bool value, bool indexed);

/**
 * @brief Set a timestamp metadata value
 *
 * WHAT: Add or update a timestamp key-value entry
 * WHY:  Store event times, durations
 * HOW:  Find or create entry, store uint64 milliseconds
 *
 * @param meta Metadata container
 * @param key Entry key
 * @param value Timestamp in milliseconds since Unix epoch
 * @param indexed Include in search index
 * @return 0 on success, -1 on error
 */
int kg_metadata_set_timestamp(kg_metadata_t* meta, const char* key,
                              uint64_t value, bool indexed);

/**
 * @brief Set a JSON metadata value
 *
 * WHAT: Add or update a JSON key-value entry
 * WHY:  Store complex nested structures
 * HOW:  Find or create entry, duplicate JSON string
 *
 * @param meta Metadata container
 * @param key Entry key
 * @param json Valid JSON string (will be duplicated)
 * @param indexed Include in search index
 * @return 0 on success, -1 on error
 */
int kg_metadata_set_json(kg_metadata_t* meta, const char* key,
                         const char* json, bool indexed);

/**
 * @brief Set comma-separated tags
 *
 * WHAT: Replace the tags field with new tags
 * WHY:  Enable tag-based searching and filtering
 * HOW:  Free existing tags, duplicate new string
 *
 * @param meta Metadata container
 * @param comma_separated_tags Tags as "tag1,tag2,tag3" (will be duplicated)
 * @return 0 on success, -1 on error
 */
int kg_metadata_set_tags(kg_metadata_t* meta, const char* comma_separated_tags);

/* ============================================================================
 * Metadata Manipulation API - Getters
 * ============================================================================ */

/**
 * @brief Get a string metadata value
 *
 * WHAT: Retrieve string value for a key
 * WHY:  Read stored text metadata
 * HOW:  Find entry by key, return string pointer
 *
 * @param meta Metadata container
 * @param key Entry key to find
 * @return String value or NULL if not found/wrong type
 */
const char* kg_metadata_get_string(const kg_metadata_t* meta, const char* key);

/**
 * @brief Get an integer metadata value
 *
 * WHAT: Retrieve integer value for a key
 * WHY:  Read stored numeric metadata
 * HOW:  Find entry by key, return value or default
 *
 * @param meta Metadata container
 * @param key Entry key to find
 * @param default_val Value to return if key not found
 * @return Integer value or default_val if not found/wrong type
 */
int64_t kg_metadata_get_int(const kg_metadata_t* meta, const char* key,
                            int64_t default_val);

/**
 * @brief Get a floating-point metadata value
 *
 * WHAT: Retrieve float value for a key
 * WHY:  Read stored measurement/score metadata
 * HOW:  Find entry by key, return value or default
 *
 * @param meta Metadata container
 * @param key Entry key to find
 * @param default_val Value to return if key not found
 * @return Float value or default_val if not found/wrong type
 */
double kg_metadata_get_float(const kg_metadata_t* meta, const char* key,
                             double default_val);

/**
 * @brief Get a boolean metadata value
 *
 * WHAT: Retrieve boolean value for a key
 * WHY:  Read stored flag metadata
 * HOW:  Find entry by key, return value or default
 *
 * @param meta Metadata container
 * @param key Entry key to find
 * @param default_val Value to return if key not found
 * @return Boolean value or default_val if not found/wrong type
 */
bool kg_metadata_get_bool(const kg_metadata_t* meta, const char* key,
                          bool default_val);

/**
 * @brief Check if a key exists in metadata
 *
 * WHAT: Test for presence of a metadata key
 * WHY:  Distinguish between missing key and default value
 * HOW:  Search entries for matching key
 *
 * @param meta Metadata container
 * @param key Entry key to find
 * @return true if key exists, false otherwise
 */
bool kg_metadata_has_key(const kg_metadata_t* meta, const char* key);

/**
 * @brief Check if metadata has a specific tag
 *
 * WHAT: Test for presence of a tag in the tags field
 * WHY:  Support tag-based filtering
 * HOW:  Parse comma-separated tags, compare each
 *
 * @param meta Metadata container
 * @param tag Tag to search for (case-sensitive)
 * @return true if tag exists, false otherwise
 */
bool kg_metadata_has_tag(const kg_metadata_t* meta, const char* tag);

/* ============================================================================
 * Level-Specific Lifecycle API
 * ============================================================================ */

/**
 * @brief Create module-level metadata
 *
 * @return New module metadata or NULL on failure
 */
kg_meta_module_t* kg_module_metadata_create(void);

/**
 * @brief Destroy module-level metadata
 *
 * @param meta Module metadata to destroy (NULL safe)
 */
void kg_module_metadata_destroy(kg_meta_module_t* meta);

/**
 * @brief Create layer-level metadata
 *
 * @return New layer metadata or NULL on failure
 */
kg_layer_metadata_t* kg_layer_metadata_create(void);

/**
 * @brief Destroy layer-level metadata
 *
 * @param meta Layer metadata to destroy (NULL safe)
 */
void kg_layer_metadata_destroy(kg_layer_metadata_t* meta);

/**
 * @brief Create hemisphere-level metadata
 *
 * @return New hemisphere metadata or NULL on failure
 */
kg_hemisphere_metadata_t* kg_hemisphere_metadata_create(void);

/**
 * @brief Destroy hemisphere-level metadata
 *
 * @param meta Hemisphere metadata to destroy (NULL safe)
 */
void kg_hemisphere_metadata_destroy(kg_hemisphere_metadata_t* meta);

/**
 * @brief Create system-level metadata
 *
 * @return New system metadata or NULL on failure
 */
kg_system_metadata_t* kg_system_metadata_create(void);

/**
 * @brief Destroy system-level metadata
 *
 * @param meta System metadata to destroy (NULL safe)
 */
void kg_system_metadata_destroy(kg_system_metadata_t* meta);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert metadata type to string
 *
 * @param type Metadata type enum value
 * @return String representation (e.g., "STRING", "INT64")
 */
const char* kg_meta_type_to_string(kg_meta_type_t type);

/**
 * @brief Generate a new UUID v4
 *
 * WHAT: Create a random UUID string
 * WHY:  Unique identification for metadata entries
 * HOW:  Use random bytes formatted as UUID
 *
 * @param uuid_out Output buffer (must be at least KG_META_UUID_LEN bytes)
 * @return 0 on success, -1 on error
 */
int kg_metadata_generate_uuid(char* uuid_out);

/**
 * @brief Update the updated_at timestamp to current time
 *
 * @param meta Metadata container to update
 * @return 0 on success, -1 on error
 */
int kg_metadata_touch(kg_metadata_t* meta);

/**
 * @brief Remove a metadata entry by key
 *
 * WHAT: Delete a key-value entry from metadata
 * WHY:  Clean up obsolete metadata
 * HOW:  Find entry, free resources, compact array
 *
 * @param meta Metadata container
 * @param key Entry key to remove
 * @return 0 on success, -1 if key not found
 */
int kg_metadata_remove(kg_metadata_t* meta, const char* key);

/**
 * @brief Get the number of custom entries in metadata
 *
 * @param meta Metadata container
 * @return Number of custom entries
 */
uint32_t kg_metadata_entry_count(const kg_metadata_t* meta);

/**
 * @brief Get entry at specified index
 *
 * WHAT: Access entry by array index
 * WHY:  Enable iteration over all entries
 * HOW:  Bounds check and return pointer
 *
 * @param meta Metadata container
 * @param index Entry index (0 to entry_count-1)
 * @return Entry pointer or NULL if index out of bounds
 */
const kg_metadata_entry_t* kg_metadata_get_entry(const kg_metadata_t* meta,
                                                  uint32_t index);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_METADATA_H */
