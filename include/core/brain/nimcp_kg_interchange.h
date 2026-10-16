/**
 * @file nimcp_kg_interchange.h
 * @brief Import/Export & Interoperability for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Full import/export and external ontology integration for brain KG
 * WHY:  Enable data exchange, backup, migration, and integration with external knowledge bases
 * HOW:  Support multiple formats (JSON, RDF, GraphML, etc.) with flexible filtering and options
 *
 * INTERCHANGE ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    KG INTERCHANGE SYSTEM                                   |
 * +===========================================================================+
 * |                                                                            |
 * |   EXPORT PIPELINE                                                          |
 * |   ----------------                                                         |
 * |   ┌────────────┐   ┌────────────┐   ┌────────────┐   ┌────────────┐       |
 * |   │ Brain KG   │ → │ Filter     │ → │ Transform  │ → │ Serialize  │       |
 * |   │ (in-mem)   │   │ (nodes/    │   │ (format    │   │ (file/     │       |
 * |   │            │   │  edges)    │   │  specific) │   │  buffer)   │       |
 * |   └────────────┘   └────────────┘   └────────────┘   └────────────┘       |
 * |                                                                            |
 * |   IMPORT PIPELINE                                                          |
 * |   ----------------                                                         |
 * |   ┌────────────┐   ┌────────────┐   ┌────────────┐   ┌────────────┐       |
 * |   │ Parse      │ → │ Validate   │ → │ ID Remap   │ → │ Merge/     │       |
 * |   │ (file/     │   │ (schema/   │   │ (conflict  │   │ Insert     │       |
 * |   │  buffer)   │   │  types)    │   │  resolve)  │   │ (KG)       │       |
 * |   └────────────┘   └────────────┘   └────────────┘   └────────────┘       |
 * |                                                                            |
 * |   SUPPORTED FORMATS                                                        |
 * |   ------------------                                                       |
 * |   JSON       - Standard nodes/edges representation                         |
 * |   JSON-LD    - Linked data with @context                                   |
 * |   RDF/XML    - W3C RDF XML serialization                                   |
 * |   RDF Turtle - Human-readable RDF                                          |
 * |   N-Triples  - Line-based RDF                                              |
 * |   GraphML    - XML-based graph format                                      |
 * |   GEXF       - Gephi exchange format                                       |
 * |   CSV        - Separate node/edge files                                    |
 * |   Parquet    - Columnar analytics format                                   |
 * |   NIMCP      - Native binary (fastest, full fidelity)                      |
 * |                                                                            |
 * |   EXTERNAL ONTOLOGY INTEGRATION                                            |
 * |   --------------------------------                                         |
 * |   WordNet     - Lexical database (synsets, relations)                      |
 * |   ConceptNet  - Commonsense knowledge graph                                |
 * |   Wikidata    - Structured Wikipedia data                                  |
 * |   DBpedia     - Wikipedia in RDF                                           |
 * |   Schema.org  - Web vocabulary                                             |
 * |   Custom      - User-provided ontology files                               |
 * |                                                                            |
 * +===========================================================================+
 * ```
 *
 * THREAD SAFETY: All import/export operations are thread-safe
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_INTERCHANGE_H
#define NIMCP_KG_INTERCHANGE_H

#include "core/brain/nimcp_brain_kg.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum length for filter expressions */
#define KG_INTERCHANGE_MAX_FILTER_LEN       512

/** Maximum length for file path */
#define KG_INTERCHANGE_MAX_PATH_LEN         1024

/** Maximum length for classification string */
#define KG_INTERCHANGE_MAX_CLASS_LEN        64

/** Maximum length for external ID */
#define KG_INTERCHANGE_MAX_EXTERNAL_ID_LEN  256

/** Maximum length for ontology term */
#define KG_INTERCHANGE_MAX_TERM_LEN         256

/* ============================================================================
 * Export Format Enumeration
 * ============================================================================ */

/**
 * @brief Export format types
 *
 * WHAT: Enumeration of supported export/import formats
 * WHY:  Enable interoperability with various graph tools and standards
 * HOW:  Each format has specialized serialization/deserialization logic
 */
typedef enum {
    KG_FORMAT_JSON = 0,              /**< JSON (nodes and edges arrays) */
    KG_FORMAT_JSON_LD,               /**< JSON-LD (linked data with @context) */
    KG_FORMAT_RDF_XML,               /**< RDF/XML (W3C standard) */
    KG_FORMAT_RDF_TURTLE,            /**< RDF Turtle (human-readable RDF) */
    KG_FORMAT_RDF_NTRIPLES,          /**< RDF N-Triples (line-based RDF) */
    KG_FORMAT_GRAPHML,               /**< GraphML (XML-based graph) */
    KG_FORMAT_GEXF,                  /**< GEXF (Gephi exchange format) */
    KG_FORMAT_CSV,                   /**< CSV (separate files for nodes/edges) */
    KG_FORMAT_PARQUET,               /**< Apache Parquet (columnar format) */
    KG_FORMAT_NIMCP_BINARY,          /**< Native NIMCP binary (full fidelity) */
    KG_FORMAT_COUNT
} kg_export_format_t;

/* ============================================================================
 * External Ontology Enumeration
 * ============================================================================ */

/**
 * @brief External ontology types for knowledge integration
 *
 * WHAT: Enumeration of supported external ontologies
 * WHY:  Enable linking brain KG nodes to established knowledge bases
 * HOW:  Import ontology data and create cross-references
 */
typedef enum {
    KG_ONTOLOGY_WORDNET = 0,         /**< WordNet lexical database */
    KG_ONTOLOGY_CONCEPTNET,          /**< ConceptNet knowledge graph */
    KG_ONTOLOGY_WIKIDATA,            /**< Wikidata structured knowledge */
    KG_ONTOLOGY_DBPEDIA,             /**< DBpedia (Wikipedia as RDF) */
    KG_ONTOLOGY_SCHEMA_ORG,          /**< Schema.org vocabulary */
    KG_ONTOLOGY_CUSTOM,              /**< Custom ontology file */
    KG_ONTOLOGY_COUNT
} kg_ontology_type_t;

/* ============================================================================
 * Export Options Structure
 * ============================================================================ */

/**
 * @brief Export configuration options
 *
 * WHAT: Configuration for KG export operations
 * WHY:  Control what data is exported and how it's formatted
 * HOW:  Set flags and filters before calling export functions
 */
typedef struct {
    kg_export_format_t format;       /**< Output format */
    bool include_metadata;           /**< Include node metadata */
    bool include_weights;            /**< Include edge weight snapshots */
    bool include_history;            /**< Include version history */
    bool compress_output;            /**< Compress output file (gzip) */
    bool encrypt_output;             /**< Encrypt output file (AES-256) */
    char* node_filter;               /**< Filter expression for nodes (NULL = all) */
    char* edge_filter;               /**< Filter expression for edges (NULL = all) */
    uint32_t max_depth;              /**< Max traversal depth (0 = unlimited) */
} kg_export_options_t;

/* ============================================================================
 * Import Options Structure
 * ============================================================================ */

/**
 * @brief Import configuration options
 *
 * WHAT: Configuration for KG import operations
 * WHY:  Control how imported data merges with existing KG
 * HOW:  Set conflict resolution and validation options
 */
typedef struct {
    kg_export_format_t format;       /**< Input format */
    bool merge_existing;             /**< Merge with existing nodes (vs replace) */
    bool overwrite_conflicts;        /**< Overwrite on ID conflict */
    bool validate_schema;            /**< Validate against KG schema */
    bool dry_run;                    /**< Preview without applying changes */
    char* id_mapping_file;           /**< File for ID remapping (NULL = auto) */
    char* default_classification;    /**< Default data classification for new nodes */
} kg_import_options_t;

/* ============================================================================
 * Import Result Structure
 * ============================================================================ */

/**
 * @brief Import operation result statistics
 *
 * WHAT: Summary of import operation outcomes
 * WHY:  Provide visibility into what was imported/updated/skipped
 * HOW:  Populated by import functions after completion
 */
typedef struct {
    uint32_t nodes_imported;         /**< New nodes created */
    uint32_t nodes_updated;          /**< Existing nodes updated */
    uint32_t nodes_skipped;          /**< Nodes skipped (conflict/filter) */
    uint32_t edges_imported;         /**< New edges created */
    uint32_t edges_updated;          /**< Existing edges updated */
    uint32_t edges_skipped;          /**< Edges skipped (conflict/filter) */
    uint32_t errors;                 /**< Error count during import */
    char* error_log_path;            /**< Path to detailed error log (heap allocated) */
    uint64_t duration_ms;            /**< Total operation duration in ms */
} kg_import_result_t;

/* ============================================================================
 * Default Configuration Functions
 * ============================================================================ */

/**
 * @brief Get default export options
 *
 * WHAT: Initialize export options with sensible defaults
 * WHY:  Provide safe starting configuration
 * HOW:  Sets format to JSON, includes metadata, no compression
 *
 * @param options Output options structure
 * @return 0 on success, -1 on NULL input
 */
int kg_export_options_default(kg_export_options_t* options);

/**
 * @brief Get default import options
 *
 * WHAT: Initialize import options with sensible defaults
 * WHY:  Provide safe starting configuration
 * HOW:  Sets format to JSON, merge mode, validation enabled
 *
 * @param options Output options structure
 * @return 0 on success, -1 on NULL input
 */
int kg_import_options_default(kg_import_options_t* options);

/**
 * @brief Free import result resources
 *
 * WHAT: Release heap-allocated fields in import result
 * WHY:  Clean resource management
 * HOW:  Frees error_log_path if allocated
 *
 * @param result Result to free (NULL safe)
 */
void kg_import_result_free(kg_import_result_t* result);

/* ============================================================================
 * Export Operations API
 * ============================================================================ */

/**
 * @brief Export full knowledge graph to file
 *
 * WHAT: Serialize entire KG to specified format
 * WHY:  Backup, migration, or external analysis
 * HOW:  Iterate all nodes/edges, apply filters, write to file
 *
 * @param kg Brain knowledge graph to export
 * @param output_path Destination file path
 * @param options Export configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int kg_export_full(
    const brain_kg_t* kg,
    const char* output_path,
    const kg_export_options_t* options
);

/**
 * @brief Export subgraph rooted at specified node
 *
 * WHAT: Serialize portion of KG reachable from root
 * WHY:  Export specific module clusters or subsystems
 * HOW:  BFS/DFS from root to depth limit, then serialize
 *
 * @param kg Brain knowledge graph
 * @param root Root node ID for subgraph
 * @param depth Maximum traversal depth from root
 * @param output_path Destination file path
 * @param options Export configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int kg_export_subgraph(
    const brain_kg_t* kg,
    brain_kg_node_id_t root,
    uint32_t depth,
    const char* output_path,
    const kg_export_options_t* options
);

/**
 * @brief Export knowledge graph to memory buffer
 *
 * WHAT: Serialize KG to in-memory buffer
 * WHY:  Network transmission or in-memory processing
 * HOW:  Same as file export but to allocated buffer
 *
 * @param kg Brain knowledge graph to export
 * @param options Export configuration (NULL for defaults)
 * @param buffer Output: pointer to allocated buffer (caller must free)
 * @param size Output: size of buffer in bytes
 * @return 0 on success, -1 on error
 */
int kg_export_to_buffer(
    const brain_kg_t* kg,
    const kg_export_options_t* options,
    void** buffer,
    size_t* size
);

/* ============================================================================
 * Import Operations API
 * ============================================================================ */

/**
 * @brief Import knowledge graph from file
 *
 * WHAT: Load and merge KG data from file
 * WHY:  Restore backups, merge external data
 * HOW:  Parse file, validate, resolve conflicts, apply to KG
 *
 * @param kg Target brain knowledge graph
 * @param input_path Source file path
 * @param options Import configuration (NULL for defaults)
 * @param result Output: import statistics (NULL to skip)
 * @return 0 on success, -1 on error
 */
int kg_import_from_file(
    brain_kg_t* kg,
    const char* input_path,
    const kg_import_options_t* options,
    kg_import_result_t* result
);

/**
 * @brief Import knowledge graph from memory buffer
 *
 * WHAT: Load and merge KG data from buffer
 * WHY:  Network receive or in-memory data
 * HOW:  Parse buffer, validate, resolve conflicts, apply to KG
 *
 * @param kg Target brain knowledge graph
 * @param buffer Source data buffer
 * @param size Buffer size in bytes
 * @param options Import configuration (NULL for defaults)
 * @param result Output: import statistics (NULL to skip)
 * @return 0 on success, -1 on error
 */
int kg_import_from_buffer(
    brain_kg_t* kg,
    const void* buffer,
    size_t size,
    const kg_import_options_t* options,
    kg_import_result_t* result
);

/**
 * @brief Validate import file without applying changes
 *
 * WHAT: Dry-run validation of import file
 * WHY:  Preview import effects before committing
 * HOW:  Parse and validate, populate preview result
 *
 * @param input_path Source file path
 * @param options Import configuration (format must match file)
 * @param preview Output: what would be imported (NULL to skip)
 * @return 0 if valid, -1 on validation error
 */
int kg_import_validate(
    const char* input_path,
    const kg_import_options_t* options,
    kg_import_result_t* preview
);

/* ============================================================================
 * Incremental Sync API
 * ============================================================================ */

/**
 * @brief Export changes since timestamp
 *
 * WHAT: Export only nodes/edges modified after timestamp
 * WHY:  Incremental backup and synchronization
 * HOW:  Filter by updated_at timestamp, export delta
 *
 * @param kg Brain knowledge graph
 * @param since_timestamp Export changes after this timestamp (ms since epoch)
 * @param output_path Destination file path
 * @param options Export configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int kg_sync_export_changes(
    const brain_kg_t* kg,
    uint64_t since_timestamp,
    const char* output_path,
    const kg_export_options_t* options
);

/**
 * @brief Import incremental changes
 *
 * WHAT: Apply delta import (changes only)
 * WHY:  Efficient synchronization between systems
 * HOW:  Parse delta file, apply updates/additions
 *
 * @param kg Target brain knowledge graph
 * @param input_path Source delta file path
 * @param options Import configuration (NULL for defaults)
 * @param result Output: import statistics (NULL to skip)
 * @return 0 on success, -1 on error
 */
int kg_sync_import_changes(
    brain_kg_t* kg,
    const char* input_path,
    const kg_import_options_t* options,
    kg_import_result_t* result
);

/* ============================================================================
 * External Ontology Integration API
 * ============================================================================ */

/**
 * @brief Import external ontology
 *
 * WHAT: Load external ontology into KG as reference nodes
 * WHY:  Enrich brain KG with external knowledge
 * HOW:  Parse ontology, create reference nodes with external IDs
 *
 * @param kg Target brain knowledge graph
 * @param type Ontology type (WordNet, ConceptNet, etc.)
 * @param path_or_url Path to ontology file or URL to fetch
 * @return 0 on success, -1 on error
 */
int kg_ontology_import(
    brain_kg_t* kg,
    kg_ontology_type_t type,
    const char* path_or_url
);

/**
 * @brief Link node to external ontology entry
 *
 * WHAT: Create cross-reference from KG node to ontology entry
 * WHY:  Enable semantic enrichment and lookup
 * HOW:  Add edge from node to ontology reference node
 *
 * @param kg Brain knowledge graph
 * @param node_id Node to link
 * @param type Ontology type
 * @param external_id External identifier (synset ID, Wikidata Q-ID, etc.)
 * @return 0 on success, -1 on error
 */
int kg_ontology_link(
    brain_kg_t* kg,
    brain_kg_node_id_t node_id,
    kg_ontology_type_t type,
    const char* external_id
);

/**
 * @brief Lookup term in external ontology
 *
 * WHAT: Search ontology for matching nodes
 * WHY:  Find relevant ontology entries to link
 * HOW:  Query ontology index, return matching node IDs
 *
 * @param kg Brain knowledge graph
 * @param type Ontology type to search
 * @param term Search term
 * @param matches Output: array of matching node IDs (caller allocated)
 * @param max Maximum number of matches to return
 * @param count Output: actual number of matches found
 * @return 0 on success, -1 on error
 */
int kg_ontology_lookup(
    const brain_kg_t* kg,
    kg_ontology_type_t type,
    const char* term,
    brain_kg_node_id_t* matches,
    uint32_t max,
    uint32_t* count
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert export format to string
 *
 * @param format Export format enum value
 * @return String representation (e.g., "JSON", "RDF_TURTLE")
 */
const char* kg_export_format_to_string(kg_export_format_t format);

/**
 * @brief Convert ontology type to string
 *
 * @param type Ontology type enum value
 * @return String representation (e.g., "WordNet", "ConceptNet")
 */
const char* kg_ontology_type_to_string(kg_ontology_type_t type);

/**
 * @brief Infer format from file extension
 *
 * WHAT: Detect export format from filename
 * WHY:  Convenience for auto-detection
 * HOW:  Parse extension and map to format enum
 *
 * @param path File path
 * @return Detected format or KG_FORMAT_JSON as default
 */
kg_export_format_t kg_infer_format_from_path(const char* path);

/**
 * @brief Get file extension for format
 *
 * @param format Export format
 * @return Recommended file extension (e.g., ".json", ".ttl")
 */
const char* kg_format_extension(kg_export_format_t format);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_INTERCHANGE_H */
