/**
 * @file nimcp_kg_reader.h
 * @brief Knowledge Graph Reader for NIMCP Self-Awareness
 *
 * Provides runtime access to NIMCP's knowledge graph stored in .aim/memory-nimcp.jsonl.
 * This enables the system to introspect its own structure, capabilities, and integrations.
 *
 * SELF-AWARENESS ARCHITECTURE:
 * The KG contains structural self-knowledge:
 * - Entities: Modules, integrations, conventions, architectures
 * - Relations: How components connect and interact
 * - Observations: Capabilities, file locations, test status
 *
 * INTEGRATION POINTS:
 * - self_model_system_t: "What am I?" - structural identity
 * - introspection_context_t: "What can I do?" - capability reflection
 * - autobiographical_memory_t: Links runtime experiences to structural knowledge
 *
 * USAGE:
 * ```c
 * kg_reader_t* kg = kg_reader_create();
 * kg_reader_load(kg, ".aim/memory-nimcp.jsonl");
 *
 * // Query entities
 * kg_entity_t* parietal = kg_reader_get_entity(kg, "Parietal_Lobe_Module");
 * kg_entity_list_t* modules = kg_reader_get_entities_by_type(kg, "Module");
 *
 * // Query relations
 * kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Parietal_Brain_Integration");
 *
 * // Introspection queries
 * const char* caps = kg_reader_get_observation(kg, "Parietal_Lobe_Module", "capabilities");
 *
 * kg_reader_destroy(kg);
 * ```
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-29
 */

#ifndef NIMCP_KG_READER_H
#define NIMCP_KG_READER_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum length of entity/relation names */
#define KG_MAX_NAME_LENGTH          256

/** Maximum length of entity type */
#define KG_MAX_TYPE_LENGTH          64

/** Maximum length of a single observation */
#define KG_MAX_OBSERVATION_LENGTH   1024

/** Maximum observations per entity */
#define KG_MAX_OBSERVATIONS         64

/** Maximum entities in KG */
#define KG_MAX_ENTITIES             512

/** Maximum relations in KG */
#define KG_MAX_RELATIONS            1024

/** Default KG file path (relative to project root) */
#define KG_DEFAULT_PATH             ".aim/memory-nimcp.jsonl"

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief Knowledge graph entity (module, integration, convention, etc.)
 */
typedef struct {
    char name[KG_MAX_NAME_LENGTH];              /**< Entity name (unique identifier) */
    char entity_type[KG_MAX_TYPE_LENGTH];       /**< Entity type (Module, Integration, etc.) */
    char* observations[KG_MAX_OBSERVATIONS];    /**< Array of observation strings */
    uint32_t num_observations;                  /**< Number of observations */
} kg_entity_t;

/**
 * @brief Knowledge graph relation (connection between entities)
 */
typedef struct {
    char from[KG_MAX_NAME_LENGTH];              /**< Source entity name */
    char to[KG_MAX_NAME_LENGTH];                /**< Target entity name */
    char relation_type[KG_MAX_TYPE_LENGTH];     /**< Relation type (connects_to, has_integration, etc.) */
} kg_relation_t;

/**
 * @brief List of entities (for query results)
 */
typedef struct {
    kg_entity_t** entities;                     /**< Array of entity pointers */
    uint32_t count;                             /**< Number of entities */
    uint32_t capacity;                          /**< Allocated capacity */
} kg_entity_list_t;

/**
 * @brief List of relations (for query results)
 */
typedef struct {
    kg_relation_t** relations;                  /**< Array of relation pointers */
    uint32_t count;                             /**< Number of relations */
    uint32_t capacity;                          /**< Allocated capacity */
} kg_relation_list_t;

/**
 * @brief Knowledge graph reader handle
 */
typedef struct kg_reader kg_reader_t;

/**
 * @brief KG reader statistics
 */
typedef struct {
    uint32_t total_entities;                    /**< Total entities loaded */
    uint32_t total_relations;                   /**< Total relations loaded */
    uint32_t total_observations;                /**< Total observations across all entities */
    uint64_t load_time_us;                      /**< Time to load KG in microseconds */
    uint64_t last_reload_time;                  /**< Timestamp of last reload */
    char file_path[512];                        /**< Path to loaded KG file */
} kg_reader_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create knowledge graph reader
 *
 * @return Reader handle or NULL on error
 */
kg_reader_t* kg_reader_create(void);

/**
 * @brief Destroy knowledge graph reader
 *
 * Frees all loaded entities, relations, and internal structures.
 *
 * @param reader Reader handle (NULL safe)
 */
void kg_reader_destroy(kg_reader_t* reader);

/**
 * @brief Load knowledge graph from JSONL file
 *
 * Parses the .aim/memory-*.jsonl file and loads entities and relations.
 * Can be called multiple times to reload (previous data is cleared).
 *
 * @param reader Reader handle
 * @param file_path Path to JSONL file (NULL for default: .aim/memory-nimcp.jsonl)
 * @return 0 on success, -1 on error
 */
int kg_reader_load(kg_reader_t* reader, const char* file_path);

/**
 * @brief Reload knowledge graph from previously loaded file
 *
 * Useful for detecting runtime changes to the KG.
 *
 * @param reader Reader handle
 * @return 0 on success, -1 on error
 */
int kg_reader_reload(kg_reader_t* reader);

/**
 * @brief Check if KG file has been modified since last load
 *
 * @param reader Reader handle
 * @return true if file was modified
 */
bool kg_reader_is_modified(const kg_reader_t* reader);

/* ============================================================================
 * ENTITY QUERY API
 * ============================================================================ */

/**
 * @brief Get entity by exact name
 *
 * @param reader Reader handle
 * @param name Entity name
 * @return Entity pointer or NULL if not found
 */
const kg_entity_t* kg_reader_get_entity(const kg_reader_t* reader, const char* name);

/**
 * @brief Get all entities of a given type
 *
 * @param reader Reader handle
 * @param entity_type Type to filter by (e.g., "Module", "Integration")
 * @return Entity list (caller must free with kg_entity_list_destroy)
 */
kg_entity_list_t* kg_reader_get_entities_by_type(const kg_reader_t* reader, const char* entity_type);

/**
 * @brief Get all entities
 *
 * @param reader Reader handle
 * @return Entity list (caller must free with kg_entity_list_destroy)
 */
kg_entity_list_t* kg_reader_get_all_entities(const kg_reader_t* reader);

/**
 * @brief Search entities by observation content
 *
 * Finds entities that have observations containing the search string.
 *
 * @param reader Reader handle
 * @param search_text Text to search for (case-insensitive)
 * @return Entity list (caller must free with kg_entity_list_destroy)
 */
kg_entity_list_t* kg_reader_search_entities(const kg_reader_t* reader, const char* search_text);

/**
 * @brief Free entity list
 *
 * @param list Entity list to free (NULL safe)
 */
void kg_entity_list_destroy(kg_entity_list_t* list);

/* ============================================================================
 * RELATION QUERY API
 * ============================================================================ */

/**
 * @brief Get all relations from a source entity
 *
 * @param reader Reader handle
 * @param from_entity Source entity name
 * @return Relation list (caller must free with kg_relation_list_destroy)
 */
kg_relation_list_t* kg_reader_get_relations_from(const kg_reader_t* reader, const char* from_entity);

/**
 * @brief Get all relations to a target entity
 *
 * @param reader Reader handle
 * @param to_entity Target entity name
 * @return Relation list (caller must free with kg_relation_list_destroy)
 */
kg_relation_list_t* kg_reader_get_relations_to(const kg_reader_t* reader, const char* to_entity);

/**
 * @brief Get all relations of a given type
 *
 * @param reader Reader handle
 * @param relation_type Relation type to filter by
 * @return Relation list (caller must free with kg_relation_list_destroy)
 */
kg_relation_list_t* kg_reader_get_relations_by_type(const kg_reader_t* reader, const char* relation_type);

/**
 * @brief Check if two entities are directly connected
 *
 * @param reader Reader handle
 * @param from_entity Source entity name
 * @param to_entity Target entity name
 * @return Relation type string if connected, NULL if not
 */
const char* kg_reader_are_connected(const kg_reader_t* reader, const char* from_entity, const char* to_entity);

/**
 * @brief Free relation list
 *
 * @param list Relation list to free (NULL safe)
 */
void kg_relation_list_destroy(kg_relation_list_t* list);

/* ============================================================================
 * OBSERVATION QUERY API
 * ============================================================================ */

/**
 * @brief Get specific observation from entity by keyword
 *
 * Searches observations for one containing the keyword.
 *
 * @param reader Reader handle
 * @param entity_name Entity name
 * @param keyword Keyword to search for in observations
 * @return Matching observation string or NULL
 */
const char* kg_reader_get_observation(const kg_reader_t* reader, const char* entity_name, const char* keyword);

/**
 * @brief Get all observations for an entity
 *
 * @param reader Reader handle
 * @param entity_name Entity name
 * @param out_count Output: number of observations
 * @return Array of observation strings or NULL
 */
const char* const* kg_reader_get_observations(const kg_reader_t* reader, const char* entity_name, uint32_t* out_count);

/* ============================================================================
 * INTROSPECTION API (Self-Awareness Helpers)
 * ============================================================================ */

/**
 * @brief Get list of all module names
 *
 * Convenience function for self-model: "What modules do I have?"
 *
 * @param reader Reader handle
 * @param out_count Output: number of modules
 * @return Array of module names (caller must free array, not strings)
 */
const char** kg_reader_get_module_names(const kg_reader_t* reader, uint32_t* out_count);

/**
 * @brief Get capabilities of a module
 *
 * Searches for observations containing "capabilities" or "provides".
 *
 * @param reader Reader handle
 * @param module_name Module entity name
 * @return Capability description or NULL
 */
const char* kg_reader_get_module_capabilities(const kg_reader_t* reader, const char* module_name);

/**
 * @brief Get file locations for a module
 *
 * Searches for observations containing "Located at" or file paths.
 *
 * @param reader Reader handle
 * @param module_name Module entity name
 * @return Location description or NULL
 */
const char* kg_reader_get_module_location(const kg_reader_t* reader, const char* module_name);

/**
 * @brief Get integration points for a module
 *
 * Returns entities that the module connects to.
 *
 * @param reader Reader handle
 * @param module_name Module entity name
 * @return Relation list showing integrations (caller must free)
 */
kg_relation_list_t* kg_reader_get_module_integrations(const kg_reader_t* reader, const char* module_name);

/**
 * @brief Generate self-description string
 *
 * Creates a human-readable description of the system based on KG contents.
 * Useful for self-model verbalization.
 *
 * @param reader Reader handle
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written
 */
int kg_reader_generate_self_description(const kg_reader_t* reader, char* buffer, size_t buffer_size);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get reader statistics
 *
 * @param reader Reader handle
 * @param stats Output statistics
 * @return 0 on success
 */
int kg_reader_get_stats(const kg_reader_t* reader, kg_reader_stats_t* stats);

/**
 * @brief Get last error message
 *
 * @return Thread-local error message
 */
const char* kg_reader_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_READER_H */
