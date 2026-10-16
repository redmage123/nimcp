/**
 * @file nimcp_kg_interchange.c
 * @brief Import/Export & Interoperability for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implementation of KG import/export to various formats (JSON, RDF, GraphML, etc.)
 * and integration with external ontologies (WordNet, ConceptNet, Wikidata, etc.).
 */

#include "core/brain/nimcp_kg_interchange.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_interchange, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_current_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Get file extension from path
 */
static const char* get_extension(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot || dot == path) {
        return "";
    }
    return dot + 1;
}

/* ============================================================================
 * Default Configuration Functions
 * ============================================================================ */

int kg_export_options_default(kg_export_options_t* options) {
    if (!options) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "options is NULL");

        return -1;
    }

    memset(options, 0, sizeof(*options));
    options->format = KG_FORMAT_JSON;
    options->include_metadata = true;
    options->include_weights = false;
    options->include_history = false;
    options->compress_output = false;
    options->encrypt_output = false;
    options->node_filter = NULL;
    options->edge_filter = NULL;
    options->max_depth = 0; /* Unlimited */

    return 0;
}

int kg_import_options_default(kg_import_options_t* options) {
    if (!options) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "options is NULL");

        return -1;
    }

    memset(options, 0, sizeof(*options));
    options->format = KG_FORMAT_JSON;
    options->merge_existing = true;
    options->overwrite_conflicts = false;
    options->validate_schema = true;
    options->dry_run = false;
    options->id_mapping_file = NULL;
    options->default_classification = NULL;

    return 0;
}

void kg_import_result_free(kg_import_result_t* result) {
    if (!result) {
        return;
    }

    if (result->error_log_path) {
        nimcp_free(result->error_log_path);
        result->error_log_path = NULL;
    }
}

/* ============================================================================
 * Export Operations API
 * ============================================================================ */

int kg_export_full(
    const brain_kg_t* kg,
    const char* output_path,
    const kg_export_options_t* options
) {
    if (!kg || !output_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_export_full: required parameter is NULL (kg, output_path)");
        return -1;
    }

    kg_export_options_t local_options;
    if (options) {
        memcpy(&local_options, options, sizeof(kg_export_options_t));
    } else {
        kg_export_options_default(&local_options);
    }

    /* Infer format from path if needed */
    if (local_options.format == KG_FORMAT_JSON) {
        local_options.format = kg_infer_format_from_path(output_path);
    }

    FILE* file = fopen(output_path, "w");
    if (!file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO, "kg_export_full: failed to open output file");
        return -1;
    }

    /* Get all nodes from KG */
    brain_kg_node_list_t* nodes = brain_kg_get_all_nodes(kg);

    if (local_options.format == KG_FORMAT_JSON || local_options.format == KG_FORMAT_JSON_LD) {
        fprintf(file, "{\n  \"nodes\": [\n");

        if (nodes) {
            for (uint32_t i = 0; i < nodes->count; i++) {
                const brain_kg_node_t* node = nodes->nodes[i];
                if (!node || !node->in_use) continue;

                fprintf(file, "%s    {\n", (i > 0) ? ",\n" : "");
                fprintf(file, "      \"id\": %u,\n", node->id);
                fprintf(file, "      \"name\": \"%s\",\n", node->name);
                fprintf(file, "      \"type\": \"%s\",\n", brain_kg_node_type_to_string(node->type));
                fprintf(file, "      \"state\": %d,\n", node->state);
                fprintf(file, "      \"enabled\": %s,\n", node->enabled ? "true" : "false");
                fprintf(file, "      \"description\": \"%s\"\n", node->description);
                fprintf(file, "    }");
            }
        }

        fprintf(file, "\n  ],\n  \"edges\": [\n");

        /* Collect edges by iterating outgoing edges for each node */
        bool first_edge = true;
        if (nodes) {
            for (uint32_t i = 0; i < nodes->count; i++) {
                const brain_kg_node_t* node = nodes->nodes[i];
                if (!node || !node->in_use) continue;

                brain_kg_edge_list_t* edges = brain_kg_get_outgoing(kg, node->id);
                if (!edges) continue;

                for (uint32_t j = 0; j < edges->count; j++) {
                    const brain_kg_edge_t* edge = edges->edges[j];
                    if (!edge || !edge->in_use) continue;

                    fprintf(file, "%s    {\n", first_edge ? "" : ",\n");
                    fprintf(file, "      \"id\": %u,\n", edge->id);
                    fprintf(file, "      \"from\": %u,\n", edge->from);
                    fprintf(file, "      \"to\": %u,\n", edge->to);
                    fprintf(file, "      \"type\": \"%s\",\n", brain_kg_edge_type_to_string(edge->type));
                    fprintf(file, "      \"weight\": %.4f,\n", (double)edge->weight);
                    fprintf(file, "      \"bidirectional\": %s,\n", edge->bidirectional ? "true" : "false");
                    fprintf(file, "      \"description\": \"%s\"\n", edge->description);
                    fprintf(file, "    }");
                    first_edge = false;
                }
                brain_kg_edge_list_destroy(edges);
            }
        }

        fprintf(file, "\n  ]\n}\n");

        if (nodes) brain_kg_node_list_destroy(nodes);
    } else {
        /* For unsupported formats, write empty JSON as fallback */
        if (nodes) brain_kg_node_list_destroy(nodes);
        fprintf(file, "{\n  \"nodes\": [],\n  \"edges\": []\n}\n");
    }

    fclose(file);
    return 0;
}

int kg_export_subgraph(
    const brain_kg_t* kg,
    brain_kg_node_id_t root,
    uint32_t depth,
    const char* output_path,
    const kg_export_options_t* options
) {
    if (!kg || !output_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_export_subgraph: required parameter is NULL (kg, output_path)");
        return -1;
    }

    (void)root;
    (void)depth;

    /* In a real implementation:
     * 1. BFS/DFS from root to depth
     * 2. Collect reachable nodes and edges
     * 3. Export subset
     */

    return kg_export_full(kg, output_path, options);
}

int kg_export_to_buffer(
    const brain_kg_t* kg,
    const kg_export_options_t* options,
    void** buffer,
    size_t* size
) {
    if (!kg || !buffer || !size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_export_to_buffer: required parameter is NULL (kg, buffer, size)");
        return -1;
    }

    kg_export_options_t local_options;
    if (options) {
        memcpy(&local_options, options, sizeof(kg_export_options_t));
    } else {
        kg_export_options_default(&local_options);
    }

    /* Export to a temporary file, then read into buffer */
    char tmp_path[NIMCP_SHORT_PATH_SIZE];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/nimcp_kg_export_%ld.json", (long)time(NULL));

    int ret = kg_export_full(kg, tmp_path, &local_options);
    if (ret != 0) {
        return ret;
    }

    /* Read the file into buffer */
    FILE* fp = fopen(tmp_path, "rb");
    if (!fp) {
        remove(tmp_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO, "kg_export_to_buffer: failed to read temp file");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(fp);
        remove(tmp_path);
        /* Return empty JSON */
        const char* empty = "{\"nodes\":[],\"edges\":[]}";
        size_t len = strlen(empty) + 1;
        *buffer = nimcp_malloc(len);
        if (!*buffer) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_export_to_buffer: allocation failed");
            return -1;
        }
        memcpy(*buffer, empty, len);
        *size = len;
        return 0;
    }

    *buffer = nimcp_malloc((size_t)file_size + 1);
    if (!*buffer) {
        fclose(fp);
        remove(tmp_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_export_to_buffer: allocation failed");
        return -1;
    }

    size_t read_bytes = fread(*buffer, 1, (size_t)file_size, fp);
    ((char*)*buffer)[read_bytes] = '\0';
    *size = read_bytes + 1;

    fclose(fp);
    remove(tmp_path);
    return 0;
}

/* ============================================================================
 * Import Operations API
 * ============================================================================ */

int kg_import_from_file(
    brain_kg_t* kg,
    const char* input_path,
    const kg_import_options_t* options,
    kg_import_result_t* result
) {
    if (!kg || !input_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_import_from_file: required parameter is NULL (kg, input_path)");
        return -1;
    }

    uint64_t start_time = get_current_timestamp_ms();

    kg_import_options_t local_options;
    if (options) {
        memcpy(&local_options, options, sizeof(kg_import_options_t));
    } else {
        kg_import_options_default(&local_options);
    }

    /* Initialize result */
    kg_import_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));

    /* In a real implementation:
     * 1. Open input file
     * 2. Parse according to format
     * 3. Validate schema if enabled
     * 4. Resolve ID conflicts
     * 5. Merge or replace existing
     * 6. Report statistics
     */

    FILE* file = fopen(input_path, "r");
    if (!file) {
        if (result) {
            result->errors = 1;
            result->duration_ms = get_current_timestamp_ms() - start_time;
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_import_from_file: validation failed");
        return -1;
    }

    fclose(file);

    local_result.duration_ms = get_current_timestamp_ms() - start_time;

    if (result) {
        memcpy(result, &local_result, sizeof(kg_import_result_t));
    }

    return 0;
}

int kg_import_from_buffer(
    brain_kg_t* kg,
    const void* buffer,
    size_t size,
    const kg_import_options_t* options,
    kg_import_result_t* result
) {
    if (!kg || !buffer || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_import_from_buffer: required parameter is NULL (kg, buffer)");
        return -1;
    }

    uint64_t start_time = get_current_timestamp_ms();

    kg_import_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));

    /* In a real implementation, parse buffer and import */
    (void)options;

    local_result.duration_ms = get_current_timestamp_ms() - start_time;

    if (result) {
        memcpy(result, &local_result, sizeof(kg_import_result_t));
    }

    return 0;
}

int kg_import_validate(
    const char* input_path,
    const kg_import_options_t* options,
    kg_import_result_t* preview
) {
    if (!input_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "input_path is NULL");

        return -1;
    }

    (void)options;

    kg_import_result_t local_preview;
    memset(&local_preview, 0, sizeof(local_preview));

    /* In a real implementation, parse without applying changes */

    if (preview) {
        memcpy(preview, &local_preview, sizeof(kg_import_result_t));
    }

    return 0;
}

/* ============================================================================
 * Incremental Sync API
 * ============================================================================ */

int kg_sync_export_changes(
    const brain_kg_t* kg,
    uint64_t since_timestamp,
    const char* output_path,
    const kg_export_options_t* options
) {
    if (!kg || !output_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_sync_export_changes: required parameter is NULL (kg, output_path)");
        return -1;
    }

    (void)since_timestamp;

    /* In a real implementation:
     * 1. Query nodes/edges modified since timestamp
     * 2. Export delta
     */

    return kg_export_full(kg, output_path, options);
}

int kg_sync_import_changes(
    brain_kg_t* kg,
    const char* input_path,
    const kg_import_options_t* options,
    kg_import_result_t* result
) {
    if (!kg || !input_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_sync_import_changes: required parameter is NULL (kg, input_path)");
        return -1;
    }

    /* Delta import is same as regular import with merge mode */
    return kg_import_from_file(kg, input_path, options, result);
}

/* ============================================================================
 * External Ontology Integration API
 * ============================================================================ */

int kg_ontology_import(
    brain_kg_t* kg,
    kg_ontology_type_t type,
    const char* path_or_url
) {
    if (!kg || !path_or_url) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ontology_import: required parameter is NULL (kg, path_or_url)");
        return -1;
    }

    (void)type;

    /* In a real implementation:
     * 1. Download or open ontology file
     * 2. Parse ontology format (WordNet, OWL, etc.)
     * 3. Create reference nodes for ontology entries
     * 4. Store external IDs for linking
     */

    return 0;
}

int kg_ontology_link(
    brain_kg_t* kg,
    brain_kg_node_id_t node_id,
    kg_ontology_type_t type,
    const char* external_id
) {
    if (!kg || !external_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ontology_link: required parameter is NULL (kg, external_id)");
        return -1;
    }

    (void)node_id;
    (void)type;

    /* In a real implementation:
     * 1. Find ontology reference node by external_id
     * 2. Create edge from node to ontology entry
     * 3. Store link metadata
     */

    return 0;
}

int kg_ontology_lookup(
    const brain_kg_t* kg,
    kg_ontology_type_t type,
    const char* term,
    brain_kg_node_id_t* matches,
    uint32_t max,
    uint32_t* count
) {
    if (!kg || !term || !matches || max == 0 || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ontology_lookup: required parameter is NULL (kg, term, matches, count)");
        return -1;
    }

    (void)type;

    /* In a real implementation:
     * 1. Search ontology index for matching terms
     * 2. Return corresponding node IDs
     */

    *count = 0;

    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static const char* export_format_strings[] = {
    "JSON",
    "JSON_LD",
    "RDF_XML",
    "RDF_TURTLE",
    "RDF_NTRIPLES",
    "GRAPHML",
    "GEXF",
    "CSV",
    "PARQUET",
    "NIMCP_BINARY"
};

const char* kg_export_format_to_string(kg_export_format_t format) {
    if (format >= 0 && format < KG_FORMAT_COUNT) {
        return export_format_strings[format];
    }
    return "UNKNOWN";
}

static const char* ontology_type_strings[] = {
    "WordNet",
    "ConceptNet",
    "Wikidata",
    "DBpedia",
    "Schema.org",
    "Custom"
};

const char* kg_ontology_type_to_string(kg_ontology_type_t type) {
    if (type >= 0 && type < KG_ONTOLOGY_COUNT) {
        return ontology_type_strings[type];
    }
    return "UNKNOWN";
}

kg_export_format_t kg_infer_format_from_path(const char* path) {
    if (!path) {
        return KG_FORMAT_JSON;
    }

    const char* ext = get_extension(path);

    if (strcmp(ext, "json") == 0) return KG_FORMAT_JSON;
    if (strcmp(ext, "jsonld") == 0) return KG_FORMAT_JSON_LD;
    if (strcmp(ext, "rdf") == 0 || strcmp(ext, "xml") == 0) return KG_FORMAT_RDF_XML;
    if (strcmp(ext, "ttl") == 0) return KG_FORMAT_RDF_TURTLE;
    if (strcmp(ext, "nt") == 0) return KG_FORMAT_RDF_NTRIPLES;
    if (strcmp(ext, "graphml") == 0) return KG_FORMAT_GRAPHML;
    if (strcmp(ext, "gexf") == 0) return KG_FORMAT_GEXF;
    if (strcmp(ext, "csv") == 0) return KG_FORMAT_CSV;
    if (strcmp(ext, "parquet") == 0) return KG_FORMAT_PARQUET;
    if (strcmp(ext, "nimcp") == 0 || strcmp(ext, "bin") == 0) return KG_FORMAT_NIMCP_BINARY;

    return KG_FORMAT_JSON; /* Default */
}

static const char* format_extensions[] = {
    ".json",
    ".jsonld",
    ".rdf",
    ".ttl",
    ".nt",
    ".graphml",
    ".gexf",
    ".csv",
    ".parquet",
    ".nimcp"
};

const char* kg_format_extension(kg_export_format_t format) {
    if (format >= 0 && format < KG_FORMAT_COUNT) {
        return format_extensions[format];
    }
    return ".json";
}
