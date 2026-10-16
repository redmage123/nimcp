/**
 * @file nimcp_hypergraph.c
 * @brief Implementation of Hypergraph Data Structure
 *
 * Implements n-ary relation hypergraph with efficient incidence indexing,
 * dual computation, and transversal algorithms.
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#include "cognitive/neuro_symbolic/nimcp_hypergraph.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/nimcp_brain_internal.h"    /* W7: brain->internal_kg fields */
#include "core/brain/nimcp_brain_kg.h"          /* W7: KG API */

#define LOG_MODULE "HYPERGRAPH"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(hypergraph, MESH_ADAPTER_CATEGORY_COGNITIVE)


#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Internal hypergraph state
 */
struct nimcp_hypergraph {
    /* Vertices */
    nimcp_hypervertex_t* vertices;
    uint32_t vertex_count;
    uint32_t vertex_capacity;
    uint32_t next_vertex_id;

    /* Edges */
    nimcp_hyperedge_t* edges;
    uint32_t edge_count;
    uint32_t edge_capacity;
    uint32_t next_edge_id;

    /* Incidence index (hash table) */
    incidence_entry_t** incidence_table;
    uint32_t incidence_hash_size;
    bool incidence_enabled;

    /* Configuration */
    hypergraph_config_t config;

    /* Statistics */
    hypergraph_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bio_router_t* bio_router;
    bool bio_async_enabled;

    /* Thread safety */
    nimcp_mutex_t* mutex;
    bool thread_safe;

    /* Module identification */
    uint32_t module_id;
    const char* module_name;
};

/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static uint32_t hash_vertex_edge(uint32_t vertex_id, uint32_t edge_id,
    uint32_t table_size);
static nimcp_error_t add_incidence(nimcp_hypergraph_t* hg, uint32_t vertex_id,
    uint32_t edge_id, uint32_t position);
static nimcp_error_t remove_incidence(nimcp_hypergraph_t* hg, uint32_t vertex_id,
    uint32_t edge_id);
static nimcp_error_t grow_vertices(nimcp_hypergraph_t* hg);
static nimcp_error_t grow_edges(nimcp_hypergraph_t* hg);
static void update_stats(nimcp_hypergraph_t* hg);
static int find_vertex_index(const nimcp_hypergraph_t* hg, uint32_t vertex_id);
static int find_edge_index(const nimcp_hypergraph_t* hg, uint32_t edge_id);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

NIMCP_API nimcp_hypergraph_t* nimcp_hypergraph_create(void)
{
    return nimcp_hypergraph_create_with_config(NULL);
}

NIMCP_API nimcp_hypergraph_t* nimcp_hypergraph_create_with_config(
    const hypergraph_config_t* config)
{
    nimcp_hypergraph_t* hg = NULL;

    /* Allocate hypergraph */
    hg = (nimcp_hypergraph_t*)nimcp_calloc(1, sizeof(nimcp_hypergraph_t));
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate hypergraph");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        memcpy(&hg->config, config, sizeof(hypergraph_config_t));
    } else {
        nimcp_hypergraph_get_default_config(&hg->config);
    }

    /* Allocate vertices */
    hg->vertex_capacity = hg->config.initial_vertex_capacity;
    hg->vertices = (nimcp_hypervertex_t*)nimcp_calloc(
        hg->vertex_capacity, sizeof(nimcp_hypervertex_t));
    if (!hg->vertices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate hypergraph vertices");
        nimcp_free(hg);
        hg = NULL;
        return NULL;
    }

    /* Allocate edges */
    hg->edge_capacity = hg->config.initial_edge_capacity;
    hg->edges = (nimcp_hyperedge_t*)nimcp_calloc(
        hg->edge_capacity, sizeof(nimcp_hyperedge_t));
    if (!hg->edges) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate hypergraph edges");
        nimcp_free(hg->vertices);
        nimcp_free(hg);
        hg = NULL;
        return NULL;
    }

    /* Allocate incidence table if enabled */
    if (hg->config.enable_incidence_index) {
        hg->incidence_hash_size = hg->config.incidence_hash_size;
        hg->incidence_table = (incidence_entry_t**)nimcp_calloc(
            hg->incidence_hash_size, sizeof(incidence_entry_t*));
        if (!hg->incidence_table) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "Failed to allocate hypergraph incidence table");
            nimcp_free(hg->edges);
            nimcp_free(hg->vertices);
            nimcp_free(hg);
            hg = NULL;
            return NULL;
        }
        hg->incidence_enabled = true;
    }

    /* Create mutex if thread safety enabled */
    if (hg->config.enable_thread_safety) {
        mutex_attr_t mutex_attr = {
            .type = MUTEX_TYPE_RECURSIVE
        };
        hg->mutex = nimcp_mutex_create(&mutex_attr);
        if (!hg->mutex) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
                "Failed to create hypergraph mutex");
            if (hg->incidence_table) nimcp_free(hg->incidence_table);
            nimcp_free(hg->edges);
            nimcp_free(hg->vertices);
            nimcp_free(hg);
            hg = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hypergraph_create_with_config: validation failed");
            return NULL;
        }
        hg->thread_safe = true;
    }

    /* Initialize counters */
    hg->vertex_count = 0;
    hg->edge_count = 0;
    hg->next_vertex_id = 1;
    hg->next_edge_id = 1;

    /* Initialize statistics */
    memset(&hg->stats, 0, sizeof(hypergraph_stats_t));

    /* Module identification */
    hg->module_id = BIO_MODULE_HYPERGRAPH;
    hg->module_name = "hypergraph";
    hg->bio_async_enabled = false;

    return hg;
}

NIMCP_API void nimcp_hypergraph_destroy(nimcp_hypergraph_t* hg)
{
    if (!hg) {
        return;
    }

    /* Unregister from bio-async */
    if (hg->bio_async_enabled) {
        nimcp_hypergraph_unregister_bio_async(hg);
    }

    /* Free incidence table */
    if (hg->incidence_table) {
        for (uint32_t i = 0; i < hg->incidence_hash_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && hg->incidence_hash_size > 256) {
                hypergraph_heartbeat("hypergraph_loop",
                                 (float)(i + 1) / (float)hg->incidence_hash_size);
            }

            incidence_entry_t* entry = hg->incidence_table[i];
            while (entry) {
                incidence_entry_t* next = entry->next;
                nimcp_free(entry);
                entry = NULL;
                entry = next;
            }
        }
        nimcp_free(hg->incidence_table);
    }

    /* Free vertex incident edges arrays and data */
    for (uint32_t i = 0; i < hg->vertex_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hg->vertex_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)hg->vertex_count);
        }

        if (hg->vertices[i].incident_edges) {
            nimcp_free(hg->vertices[i].incident_edges);
        }
        if (hg->vertices[i].data) {
            nimcp_free(hg->vertices[i].data);
        }
    }

    /* Free edge vertex arrays */
    for (uint32_t i = 0; i < hg->edge_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hg->edge_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)hg->edge_count);
        }

        if (hg->edges[i].vertices) {
            nimcp_free(hg->edges[i].vertices);
        }
    }

    /* Free main arrays */
    if (hg->vertices) {
        nimcp_free(hg->vertices);
    }
    if (hg->edges) {
        nimcp_free(hg->edges);
    }

    /* Free mutex */
    if (hg->mutex) {
        nimcp_mutex_destroy(hg->mutex);
    }

    /* Free hypergraph */
    nimcp_free(hg);
    hg = NULL;
}

NIMCP_API nimcp_error_t nimcp_hypergraph_clear(nimcp_hypergraph_t* hg)
{
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_hypergraph_clear: hg is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (hg->thread_safe) {
        nimcp_mutex_lock(hg->mutex);
    }

    /* Clear incidence table */
    if (hg->incidence_table) {
        for (uint32_t i = 0; i < hg->incidence_hash_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && hg->incidence_hash_size > 256) {
                hypergraph_heartbeat("hypergraph_loop",
                                 (float)(i + 1) / (float)hg->incidence_hash_size);
            }

            incidence_entry_t* entry = hg->incidence_table[i];
            while (entry) {
                incidence_entry_t* next = entry->next;
                nimcp_free(entry);
                entry = NULL;
                entry = next;
            }
            hg->incidence_table[i] = NULL;
        }
    }

    /* Clear vertex data */
    for (uint32_t i = 0; i < hg->vertex_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hg->vertex_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)hg->vertex_count);
        }

        if (hg->vertices[i].incident_edges) {
            nimcp_free(hg->vertices[i].incident_edges);
        }
        if (hg->vertices[i].data) {
            nimcp_free(hg->vertices[i].data);
        }
    }
    memset(hg->vertices, 0, hg->vertex_capacity * sizeof(nimcp_hypervertex_t));

    /* Clear edge data */
    for (uint32_t i = 0; i < hg->edge_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hg->edge_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)hg->edge_count);
        }

        if (hg->edges[i].vertices) {
            nimcp_free(hg->edges[i].vertices);
        }
    }
    memset(hg->edges, 0, hg->edge_capacity * sizeof(nimcp_hyperedge_t));

    /* Reset counters */
    hg->vertex_count = 0;
    hg->edge_count = 0;
    hg->next_vertex_id = 1;
    hg->next_edge_id = 1;

    /* Reset statistics */
    memset(&hg->stats, 0, sizeof(hypergraph_stats_t));

    if (hg->thread_safe) {
        nimcp_mutex_unlock(hg->mutex);
    }

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t nimcp_hypergraph_get_default_config(
    hypergraph_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_hypergraph_get_default_config: config is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    config->initial_vertex_capacity = HYPERGRAPH_DEFAULT_CAPACITY;
    config->initial_edge_capacity = HYPERGRAPH_DEFAULT_CAPACITY;
    config->incidence_hash_size = HYPERGRAPH_DEFAULT_CAPACITY * 4;
    config->enable_incidence_index = true;
    config->enable_thread_safety = true;
    config->enable_bio_async = true;
    config->default_confidence = 1.0f;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Vertex Operations
 * ============================================================================ */

NIMCP_API uint32_t nimcp_hypergraph_add_vertex(
    nimcp_hypergraph_t* hg,
    hypervertex_type_t type,
    const char* label,
    float confidence)
{
    return nimcp_hypergraph_add_vertex_with_data(hg, type, label,
        NULL, 0, confidence);
}

NIMCP_API uint32_t nimcp_hypergraph_add_vertex_with_data(
    nimcp_hypergraph_t* hg,
    hypervertex_type_t type,
    const char* label,
    const void* data,
    uint32_t data_size,
    float confidence)
{
    if (!hg) {
        return UINT32_MAX;
    }

    if (hg->thread_safe) {
        nimcp_mutex_lock(hg->mutex);
    }

    /* Check capacity */
    if (hg->vertex_count >= hg->vertex_capacity) {
        if (grow_vertices(hg) != NIMCP_SUCCESS) {
            if (hg->thread_safe) {
                nimcp_mutex_unlock(hg->mutex);
            }
            return UINT32_MAX;
        }
    }

    /* Check limits */
    if (hg->vertex_count >= HYPERGRAPH_MAX_VERTICES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE,
            "Hypergraph vertex limit exceeded");
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        return UINT32_MAX;
    }

    /* Initialize vertex */
    nimcp_hypervertex_t* vertex = &hg->vertices[hg->vertex_count];
    vertex->id = hg->next_vertex_id++;
    vertex->type = type;
    vertex->confidence = confidence;
    vertex->value = 0.0f;

    if (label) {
        strncpy(vertex->label, label, HYPERGRAPH_MAX_LABEL_LEN - 1);
        vertex->label[HYPERGRAPH_MAX_LABEL_LEN - 1] = '\0';
    } else {
        vertex->label[0] = '\0';
    }

    /* Copy data if provided */
    if (data && data_size > 0) {
        vertex->data = nimcp_malloc(data_size);
        if (vertex->data) {
            memcpy(vertex->data, data, data_size);
            vertex->data_size = data_size;
        }
    } else {
        vertex->data = NULL;
        vertex->data_size = 0;
    }

    /* Initialize incident edges array */
    vertex->incident_capacity = 16;
    vertex->incident_edges = (uint32_t*)nimcp_calloc(
        vertex->incident_capacity, sizeof(uint32_t));
    vertex->incident_count = 0;

    /* Timestamps */
    uint64_t now = nimcp_time_monotonic_us();
    vertex->created_time_us = now;
    vertex->modified_time_us = now;

    uint32_t id = vertex->id;
    hg->vertex_count++;

    /* Update statistics */
    if (type < HYPERVERTEX_TYPE_COUNT) {
        hg->stats.vertex_type_counts[type]++;
    }
    hg->stats.vertex_count = hg->vertex_count;

    if (hg->thread_safe) {
        nimcp_mutex_unlock(hg->mutex);
    }

    return id;
}

NIMCP_API nimcp_error_t nimcp_hypergraph_remove_vertex(
    nimcp_hypergraph_t* hg,
    uint32_t vertex_id)
{
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_hypergraph_remove_vertex: hg is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (hg->thread_safe) {
        nimcp_mutex_lock(hg->mutex);
    }

    int idx = find_vertex_index(hg, vertex_id);
    if (idx < 0) {
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND,
            "nimcp_hypergraph_remove_vertex: vertex not found");
        return NIMCP_ERROR_NOT_FOUND;
    }

    nimcp_hypervertex_t* vertex = &hg->vertices[idx];

    /* Remove from all incident edges */
    for (uint32_t i = 0; i < vertex->incident_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && vertex->incident_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)vertex->incident_count);
        }

        uint32_t edge_id = vertex->incident_edges[i];
        /* Remove this vertex from the edge */
        nimcp_hypergraph_shrink_edge(hg, edge_id, vertex_id);
    }

    /* Free vertex resources */
    if (vertex->incident_edges) {
        nimcp_free(vertex->incident_edges);
    }
    if (vertex->data) {
        nimcp_free(vertex->data);
    }

    /* Update statistics */
    if (vertex->type < HYPERVERTEX_TYPE_COUNT) {
        hg->stats.vertex_type_counts[vertex->type]--;
    }

    /* Move last vertex to this position */
    if ((uint32_t)idx < hg->vertex_count - 1) {
        memcpy(&hg->vertices[idx], &hg->vertices[hg->vertex_count - 1],
            sizeof(nimcp_hypervertex_t));
    }
    hg->vertex_count--;
    hg->stats.vertex_count = hg->vertex_count;

    if (hg->thread_safe) {
        nimcp_mutex_unlock(hg->mutex);
    }

    return NIMCP_SUCCESS;
}

NIMCP_API const nimcp_hypervertex_t* nimcp_hypergraph_get_vertex(
    const nimcp_hypergraph_t* hg,
    uint32_t vertex_id)
{
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_hypergraph_get_vertex: hg is NULL");
        return NULL;
    }

    int idx = find_vertex_index(hg, vertex_id);
    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_hypergraph_get_vertex: validation failed");
        return NULL;
    }

    return &hg->vertices[idx];
}

NIMCP_API uint32_t nimcp_hypergraph_find_vertex(
    const nimcp_hypergraph_t* hg,
    const char* label)
{
    if (!hg || !label) {
        return UINT32_MAX;
    }

    for (uint32_t i = 0; i < hg->vertex_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hg->vertex_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)hg->vertex_count);
        }

        if (strcmp(hg->vertices[i].label, label) == 0) {
            return hg->vertices[i].id;
        }
    }

    return UINT32_MAX;
}

NIMCP_API nimcp_error_t nimcp_hypergraph_update_vertex_confidence(
    nimcp_hypergraph_t* hg,
    uint32_t vertex_id,
    float confidence)
{
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_hypergraph_update_vertex_confidence: hg is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (hg->thread_safe) {
        nimcp_mutex_lock(hg->mutex);
    }

    int idx = find_vertex_index(hg, vertex_id);
    if (idx < 0) {
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND,
            "nimcp_hypergraph_update_vertex_confidence: vertex not found");
        return NIMCP_ERROR_NOT_FOUND;
    }

    hg->vertices[idx].confidence = confidence;
    hg->vertices[idx].modified_time_us = nimcp_time_monotonic_us();

    if (hg->thread_safe) {
        nimcp_mutex_unlock(hg->mutex);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Edge Operations
 * ============================================================================ */

NIMCP_API uint32_t nimcp_hypergraph_add_edge(
    nimcp_hypergraph_t* hg,
    hyperedge_type_t type,
    const uint32_t* vertices,
    uint32_t count,
    trit_t weight,
    const char* label)
{
    return nimcp_hypergraph_add_edge_full(hg, type, vertices, count,
        weight, 1.0f, false, label);
}

NIMCP_API uint32_t nimcp_hypergraph_add_edge_full(
    nimcp_hypergraph_t* hg,
    hyperedge_type_t type,
    const uint32_t* vertices,
    uint32_t count,
    trit_t weight,
    float confidence,
    bool is_directed,
    const char* label)
{
    if (!hg || !vertices || count == 0) {
        return UINT32_MAX;
    }

    if (count > HYPERGRAPH_MAX_EDGE_VERTICES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "Hyperedge exceeds maximum vertex count");
        return UINT32_MAX;
    }

    if (hg->thread_safe) {
        nimcp_mutex_lock(hg->mutex);
    }

    /* Check capacity */
    if (hg->edge_count >= hg->edge_capacity) {
        if (grow_edges(hg) != NIMCP_SUCCESS) {
            if (hg->thread_safe) {
                nimcp_mutex_unlock(hg->mutex);
            }
            return UINT32_MAX;
        }
    }

    /* Check limits */
    if (hg->edge_count >= HYPERGRAPH_MAX_EDGES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE,
            "Hypergraph edge limit exceeded");
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        return UINT32_MAX;
    }

    /* Verify all vertices exist */
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)count);
        }

        if (find_vertex_index(hg, vertices[i]) < 0) {
            if (hg->thread_safe) {
                nimcp_mutex_unlock(hg->mutex);
            }
            return UINT32_MAX;
        }
    }

    /* Initialize edge */
    nimcp_hyperedge_t* edge = &hg->edges[hg->edge_count];
    edge->id = hg->next_edge_id++;
    edge->type = type;
    edge->weight = weight;
    edge->confidence = confidence;
    edge->is_directed = is_directed;
    edge->is_symmetric = false;
    edge->is_reflexive = false;
    edge->is_transitive = false;
    edge->is_antisymmetric = false;

    if (label) {
        strncpy(edge->label, label, HYPERGRAPH_MAX_LABEL_LEN - 1);
        edge->label[HYPERGRAPH_MAX_LABEL_LEN - 1] = '\0';
    } else {
        edge->label[0] = '\0';
    }

    /* Allocate and copy vertices */
    edge->vertex_capacity = count;
    edge->vertices = (uint32_t*)nimcp_calloc(count, sizeof(uint32_t));
    if (!edge->vertices) {
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        return UINT32_MAX;
    }
    memcpy(edge->vertices, vertices, count * sizeof(uint32_t));
    edge->vertex_count = count;

    /* Timestamps */
    uint64_t now = nimcp_time_monotonic_us();
    edge->created_time_us = now;
    edge->modified_time_us = now;

    uint32_t edge_id = edge->id;

    /* Add to incidence structures */
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)count);
        }

        /* Add to incidence index */
        if (hg->incidence_enabled) {
            add_incidence(hg, vertices[i], edge_id, i);
        }

        /* Add to vertex's incident edges list */
        int vidx = find_vertex_index(hg, vertices[i]);
        if (vidx >= 0) {
            nimcp_hypervertex_t* v = &hg->vertices[vidx];

            /* Grow if needed */
            if (v->incident_count >= v->incident_capacity) {
                uint32_t new_cap = v->incident_capacity * 2;
                uint32_t* new_edges = (uint32_t*)nimcp_realloc(
                    v->incident_edges, new_cap * sizeof(uint32_t));
                if (new_edges) {
                    v->incident_edges = new_edges;
                    v->incident_capacity = new_cap;
                }
            }

            if (v->incident_count < v->incident_capacity) {
                v->incident_edges[v->incident_count++] = edge_id;
            }
        }
    }

    hg->edge_count++;

    /* Update statistics */
    if (type < HYPEREDGE_TYPE_COUNT) {
        hg->stats.edge_type_counts[type]++;
    }
    hg->stats.edge_count = hg->edge_count;
    hg->stats.total_incidences += count;

    if (count > hg->stats.max_edge_arity) {
        hg->stats.max_edge_arity = count;
    }

    /* Recompute average arity */
    float total_arity = 0;
    for (uint32_t i = 0; i < hg->edge_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hg->edge_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)hg->edge_count);
        }

        total_arity += hg->edges[i].vertex_count;
    }
    hg->stats.avg_edge_arity = total_arity / hg->edge_count;

    if (hg->thread_safe) {
        nimcp_mutex_unlock(hg->mutex);
    }

    return edge_id;
}

NIMCP_API nimcp_error_t nimcp_hypergraph_remove_edge(
    nimcp_hypergraph_t* hg,
    uint32_t edge_id)
{
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_hypergraph_remove_edge: hg is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (hg->thread_safe) {
        nimcp_mutex_lock(hg->mutex);
    }

    int idx = find_edge_index(hg, edge_id);
    if (idx < 0) {
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND,
            "nimcp_hypergraph_remove_edge: edge not found");
        return NIMCP_ERROR_NOT_FOUND;
    }

    nimcp_hyperedge_t* edge = &hg->edges[idx];

    /* Remove from incidence structures */
    for (uint32_t i = 0; i < edge->vertex_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && edge->vertex_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)edge->vertex_count);
        }

        uint32_t vid = edge->vertices[i];

        /* Remove from incidence index */
        if (hg->incidence_enabled) {
            remove_incidence(hg, vid, edge_id);
        }

        /* Remove from vertex's incident edges list */
        int vidx = find_vertex_index(hg, vid);
        if (vidx >= 0) {
            nimcp_hypervertex_t* v = &hg->vertices[vidx];
            for (uint32_t j = 0; j < v->incident_count; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && v->incident_count > 256) {
                    hypergraph_heartbeat("hypergraph_loop",
                                     (float)(j + 1) / (float)v->incident_count);
                }

                if (v->incident_edges[j] == edge_id) {
                    /* Shift remaining edges */
                    memmove(&v->incident_edges[j],
                            &v->incident_edges[j + 1],
                            (v->incident_count - j - 1) * sizeof(uint32_t));
                    v->incident_count--;
                    break;
                }
            }
        }
    }

    /* Update statistics */
    if (edge->type < HYPEREDGE_TYPE_COUNT) {
        hg->stats.edge_type_counts[edge->type]--;
    }
    hg->stats.total_incidences -= edge->vertex_count;

    /* Free edge resources */
    if (edge->vertices) {
        nimcp_free(edge->vertices);
    }

    /* Move last edge to this position */
    if ((uint32_t)idx < hg->edge_count - 1) {
        memcpy(&hg->edges[idx], &hg->edges[hg->edge_count - 1],
            sizeof(nimcp_hyperedge_t));
    }
    hg->edge_count--;
    hg->stats.edge_count = hg->edge_count;

    if (hg->thread_safe) {
        nimcp_mutex_unlock(hg->mutex);
    }

    return NIMCP_SUCCESS;
}

NIMCP_API const nimcp_hyperedge_t* nimcp_hypergraph_get_edge(
    const nimcp_hypergraph_t* hg,
    uint32_t edge_id)
{
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_hypergraph_get_edge: hg is NULL");
        return NULL;
    }

    int idx = find_edge_index(hg, edge_id);
    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_hypergraph_get_edge: validation failed");
        return NULL;
    }

    return &hg->edges[idx];
}

NIMCP_API uint32_t nimcp_hypergraph_get_incident_edges(
    const nimcp_hypergraph_t* hg,
    uint32_t vertex_id,
    uint32_t* edges,
    uint32_t max_edges)
{
    if (!hg || !edges || max_edges == 0) {
        return 0;
    }

    int vidx = find_vertex_index(hg, vertex_id);
    if (vidx < 0) {
        return 0;
    }

    const nimcp_hypervertex_t* v = &hg->vertices[vidx];
    uint32_t count = (v->incident_count < max_edges) ?
        v->incident_count : max_edges;

    memcpy(edges, v->incident_edges, count * sizeof(uint32_t));

    return count;
}

NIMCP_API uint32_t nimcp_hypergraph_get_edges_by_type(
    const nimcp_hypergraph_t* hg,
    hyperedge_type_t type,
    uint32_t* edges,
    uint32_t max_edges)
{
    if (!hg || !edges || max_edges == 0) {
        return 0;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < hg->edge_count && count < max_edges; i++) {
        if (hg->edges[i].type == type) {
            edges[count++] = hg->edges[i].id;
        }
    }

    return count;
}

NIMCP_API nimcp_error_t nimcp_hypergraph_update_edge_weight(
    nimcp_hypergraph_t* hg,
    uint32_t edge_id,
    trit_t weight)
{
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_hypergraph_update_edge_weight: hg is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (hg->thread_safe) {
        nimcp_mutex_lock(hg->mutex);
    }

    int idx = find_edge_index(hg, edge_id);
    if (idx < 0) {
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND,
            "nimcp_hypergraph_update_edge_weight: edge not found");
        return NIMCP_ERROR_NOT_FOUND;
    }

    hg->edges[idx].weight = weight;
    hg->edges[idx].modified_time_us = nimcp_time_monotonic_us();

    if (hg->thread_safe) {
        nimcp_mutex_unlock(hg->mutex);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Edge Transformation
 * ============================================================================ */

NIMCP_API uint32_t nimcp_hypergraph_contract_edge(
    nimcp_hypergraph_t* hg,
    uint32_t edge_id)
{
    if (!hg) {
        return UINT32_MAX;
    }

    if (hg->thread_safe) {
        nimcp_mutex_lock(hg->mutex);
    }

    int eidx = find_edge_index(hg, edge_id);
    if (eidx < 0) {
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        return UINT32_MAX;
    }

    nimcp_hyperedge_t* edge = &hg->edges[eidx];

    if (edge->vertex_count == 0) {
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        return UINT32_MAX;
    }

    /* Keep first vertex as the merged vertex */
    uint32_t merged_id = edge->vertices[0];
    int merged_idx = find_vertex_index(hg, merged_id);
    if (merged_idx < 0) {
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        return UINT32_MAX;
    }

    /* Merge all other vertices into the first one */
    for (uint32_t i = 1; i < edge->vertex_count; i++) {
        uint32_t vid = edge->vertices[i];
        int vidx = find_vertex_index(hg, vid);
        if (vidx < 0) continue;

        nimcp_hypervertex_t* v = &hg->vertices[vidx];

        /* For each edge incident to this vertex */
        for (uint32_t j = 0; j < v->incident_count; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && v->incident_count > 256) {
                hypergraph_heartbeat("hypergraph_loop",
                                 (float)(j + 1) / (float)v->incident_count);
            }

            uint32_t incident_edge_id = v->incident_edges[j];
            if (incident_edge_id == edge_id) continue;

            int ie_idx = find_edge_index(hg, incident_edge_id);
            if (ie_idx < 0) continue;

            nimcp_hyperedge_t* ie = &hg->edges[ie_idx];

            /* Replace vid with merged_id in this edge */
            for (uint32_t k = 0; k < ie->vertex_count; k++) {
                /* Phase 8: Loop progress heartbeat */
                if ((k & 0xFF) == 0 && ie->vertex_count > 256) {
                    hypergraph_heartbeat("hypergraph_loop",
                                     (float)(k + 1) / (float)ie->vertex_count);
                }

                if (ie->vertices[k] == vid) {
                    ie->vertices[k] = merged_id;
                }
            }
        }
    }

    /* Remove the contracted edge and merged vertices */
    nimcp_hypergraph_remove_edge(hg, edge_id);

    for (uint32_t i = 1; i < edge->vertex_count; i++) {
        nimcp_hypergraph_remove_vertex(hg, edge->vertices[i]);
    }

    if (hg->thread_safe) {
        nimcp_mutex_unlock(hg->mutex);
    }

    return merged_id;
}

NIMCP_API nimcp_error_t nimcp_hypergraph_extend_edge(
    nimcp_hypergraph_t* hg,
    uint32_t edge_id,
    uint32_t vertex_id)
{
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_hypergraph_extend_edge: hg is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (hg->thread_safe) {
        nimcp_mutex_lock(hg->mutex);
    }

    int eidx = find_edge_index(hg, edge_id);
    int vidx = find_vertex_index(hg, vertex_id);

    if (eidx < 0 || vidx < 0) {
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND,
            "nimcp_hypergraph_extend_edge: edge or vertex not found");
        return NIMCP_ERROR_NOT_FOUND;
    }

    nimcp_hyperedge_t* edge = &hg->edges[eidx];

    /* Check if already in edge */
    for (uint32_t i = 0; i < edge->vertex_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && edge->vertex_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)edge->vertex_count);
        }

        if (edge->vertices[i] == vertex_id) {
            if (hg->thread_safe) {
                nimcp_mutex_unlock(hg->mutex);
            }
            return NIMCP_SUCCESS;  /* Already present */
        }
    }

    /* Check arity limit */
    if (edge->vertex_count >= HYPERGRAPH_MAX_EDGE_VERTICES) {
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE,
            "nimcp_hypergraph_extend_edge: max edge vertices exceeded");
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    /* Grow if needed */
    if (edge->vertex_count >= edge->vertex_capacity) {
        uint32_t new_cap = edge->vertex_capacity * 2;
        if (new_cap > HYPERGRAPH_MAX_EDGE_VERTICES) {
            new_cap = HYPERGRAPH_MAX_EDGE_VERTICES;
        }
        uint32_t* new_vertices = (uint32_t*)nimcp_realloc(
            edge->vertices, new_cap * sizeof(uint32_t));
        if (!new_vertices) {
            if (hg->thread_safe) {
                nimcp_mutex_unlock(hg->mutex);
            }
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "nimcp_hypergraph_extend_edge: failed to grow vertices array");
            return NIMCP_ERROR_NO_MEMORY;
        }
        edge->vertices = new_vertices;
        edge->vertex_capacity = new_cap;
    }

    /* Add vertex */
    edge->vertices[edge->vertex_count++] = vertex_id;
    edge->modified_time_us = nimcp_time_monotonic_us();

    /* Update incidence */
    if (hg->incidence_enabled) {
        add_incidence(hg, vertex_id, edge_id, edge->vertex_count - 1);
    }

    /* Update vertex's incident list */
    nimcp_hypervertex_t* v = &hg->vertices[vidx];
    if (v->incident_count >= v->incident_capacity) {
        uint32_t new_cap = v->incident_capacity * 2;
        uint32_t* new_edges = (uint32_t*)nimcp_realloc(
            v->incident_edges, new_cap * sizeof(uint32_t));
        if (new_edges) {
            v->incident_edges = new_edges;
            v->incident_capacity = new_cap;
        }
    }
    if (v->incident_count < v->incident_capacity) {
        v->incident_edges[v->incident_count++] = edge_id;
    }

    hg->stats.total_incidences++;

    if (hg->thread_safe) {
        nimcp_mutex_unlock(hg->mutex);
    }

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t nimcp_hypergraph_shrink_edge(
    nimcp_hypergraph_t* hg,
    uint32_t edge_id,
    uint32_t vertex_id)
{
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_hypergraph_shrink_edge: hg is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (hg->thread_safe) {
        nimcp_mutex_lock(hg->mutex);
    }

    int eidx = find_edge_index(hg, edge_id);
    if (eidx < 0) {
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND,
            "nimcp_hypergraph_shrink_edge: edge not found");
        return NIMCP_ERROR_NOT_FOUND;
    }

    nimcp_hyperedge_t* edge = &hg->edges[eidx];

    /* Find vertex in edge */
    int vpos = -1;
    for (uint32_t i = 0; i < edge->vertex_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && edge->vertex_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)edge->vertex_count);
        }

        if (edge->vertices[i] == vertex_id) {
            vpos = (int)i;
            break;
        }
    }

    if (vpos < 0) {
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND,
            "nimcp_hypergraph_shrink_edge: vertex not in edge");
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Remove from incidence */
    if (hg->incidence_enabled) {
        remove_incidence(hg, vertex_id, edge_id);
    }

    /* Remove from vertex's incident list */
    int vidx = find_vertex_index(hg, vertex_id);
    if (vidx >= 0) {
        nimcp_hypervertex_t* v = &hg->vertices[vidx];
        for (uint32_t i = 0; i < v->incident_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && v->incident_count > 256) {
                hypergraph_heartbeat("hypergraph_loop",
                                 (float)(i + 1) / (float)v->incident_count);
            }

            if (v->incident_edges[i] == edge_id) {
                memmove(&v->incident_edges[i],
                        &v->incident_edges[i + 1],
                        (v->incident_count - i - 1) * sizeof(uint32_t));
                v->incident_count--;
                break;
            }
        }
    }

    /* Remove from edge's vertex list */
    memmove(&edge->vertices[vpos],
            &edge->vertices[vpos + 1],
            (edge->vertex_count - vpos - 1) * sizeof(uint32_t));
    edge->vertex_count--;
    edge->modified_time_us = nimcp_time_monotonic_us();

    hg->stats.total_incidences--;

    if (hg->thread_safe) {
        nimcp_mutex_unlock(hg->mutex);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Dual Hypergraph
 * ============================================================================ */

NIMCP_API nimcp_hypergraph_dual_t* nimcp_hypergraph_compute_dual(
    const nimcp_hypergraph_t* hg)
{
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_hypergraph_compute_dual: hg is NULL");
        return NULL;
    }

    nimcp_hypergraph_dual_t* dual_result =
        (nimcp_hypergraph_dual_t*)nimcp_calloc(1, sizeof(nimcp_hypergraph_dual_t));
    if (!dual_result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hypergraph_compute_dual: dual_result is NULL");
        return NULL;
    }

    /* Create the dual hypergraph */
    dual_result->dual = nimcp_hypergraph_create();
    if (!dual_result->dual) {
        nimcp_free(dual_result);
        dual_result = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hypergraph_compute_dual: dual_result->dual is NULL");
        return NULL;
    }

    /* Allocate mapping arrays */
    dual_result->vertex_to_edge_map =
        (uint32_t*)nimcp_calloc(hg->vertex_count, sizeof(uint32_t));
    dual_result->edge_to_vertex_map =
        (uint32_t*)nimcp_calloc(hg->edge_count, sizeof(uint32_t));

    if (!dual_result->vertex_to_edge_map || !dual_result->edge_to_vertex_map) {
        nimcp_hypergraph_dual_destroy(dual_result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hypergraph_compute_dual: required parameter is NULL (dual_result->vertex_to_edge_map, dual_result->edge_to_vertex_map)");
        return NULL;
    }

    /* In the dual: original edges become vertices */
    for (uint32_t i = 0; i < hg->edge_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hg->edge_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)hg->edge_count);
        }

        const nimcp_hyperedge_t* edge = &hg->edges[i];
        uint32_t dual_vid = nimcp_hypergraph_add_vertex(
            dual_result->dual,
            HYPERVERTEX_SET,
            edge->label,
            edge->confidence);

        if (dual_vid != UINT32_MAX) {
            dual_result->edge_to_vertex_map[i] = dual_vid;
        }
    }

    /* In the dual: original vertices become edges
     * Two dual vertices (original edges) are connected by a dual edge
     * if they share a common original vertex */
    for (uint32_t i = 0; i < hg->vertex_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hg->vertex_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)hg->vertex_count);
        }

        const nimcp_hypervertex_t* v = &hg->vertices[i];

        if (v->incident_count > 1) {
            /* Create a dual edge connecting all dual vertices
             * corresponding to edges incident to this vertex */
            uint32_t* dual_vertices = (uint32_t*)nimcp_malloc(
                v->incident_count * sizeof(uint32_t));
            if (!dual_vertices) continue;

            uint32_t count = 0;
            for (uint32_t j = 0; j < v->incident_count; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && v->incident_count > 256) {
                    hypergraph_heartbeat("hypergraph_loop",
                                     (float)(j + 1) / (float)v->incident_count);
                }

                int eidx = find_edge_index(hg, v->incident_edges[j]);
                if (eidx >= 0) {
                    dual_vertices[count++] = dual_result->edge_to_vertex_map[eidx];
                }
            }

            if (count > 0) {
                uint32_t dual_eid = nimcp_hypergraph_add_edge(
                    dual_result->dual,
                    HYPEREDGE_RELATION,
                    dual_vertices,
                    count,
                    TRIT_POSITIVE,
                    v->label);

                if (dual_eid != UINT32_MAX) {
                    dual_result->vertex_to_edge_map[i] = dual_eid;
                }
            }

            nimcp_free(dual_vertices);
            dual_vertices = NULL;
        }
    }

    return dual_result;
}

NIMCP_API void nimcp_hypergraph_dual_destroy(nimcp_hypergraph_dual_t* dual)
{
    if (!dual) {
        return;
    }

    if (dual->dual) {
        nimcp_hypergraph_destroy(dual->dual);
    }
    if (dual->vertex_to_edge_map) {
        nimcp_free(dual->vertex_to_edge_map);
    }
    if (dual->edge_to_vertex_map) {
        nimcp_free(dual->edge_to_vertex_map);
    }

    nimcp_free(dual);
    dual = NULL;
}

/* ============================================================================
 * Transversal Computation
 * ============================================================================ */

NIMCP_API uint32_t nimcp_hypergraph_transversal(
    const nimcp_hypergraph_t* hg,
    uint32_t* vertices,
    uint32_t max_vertices)
{
    if (!hg || !vertices || max_vertices == 0) {
        return 0;
    }

    if (hg->edge_count == 0) {
        return 0;  /* Empty hypergraph has empty transversal */
    }

    /* Greedy algorithm: repeatedly pick vertex that covers most uncovered edges */
    bool* covered_edges = (bool*)nimcp_calloc(hg->edge_count, sizeof(bool));
    bool* used_vertices = (bool*)nimcp_calloc(hg->vertex_count, sizeof(bool));

    if (!covered_edges || !used_vertices) {
        if (covered_edges) nimcp_free(covered_edges);
        if (used_vertices) nimcp_free(used_vertices);
        return 0;
    }

    uint32_t transversal_size = 0;
    uint32_t uncovered_count = hg->edge_count;

    while (uncovered_count > 0 && transversal_size < max_vertices) {
        /* Find vertex covering most uncovered edges */
        uint32_t best_vertex = UINT32_MAX;
        uint32_t best_cover_count = 0;

        for (uint32_t i = 0; i < hg->vertex_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && hg->vertex_count > 256) {
                hypergraph_heartbeat("hypergraph_loop",
                                 (float)(i + 1) / (float)hg->vertex_count);
            }

            if (used_vertices[i]) continue;

            const nimcp_hypervertex_t* v = &hg->vertices[i];
            uint32_t cover_count = 0;

            for (uint32_t j = 0; j < v->incident_count; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && v->incident_count > 256) {
                    hypergraph_heartbeat("hypergraph_loop",
                                     (float)(j + 1) / (float)v->incident_count);
                }

                int eidx = find_edge_index(hg, v->incident_edges[j]);
                if (eidx >= 0 && !covered_edges[eidx]) {
                    cover_count++;
                }
            }

            if (cover_count > best_cover_count) {
                best_cover_count = cover_count;
                best_vertex = i;
            }
        }

        if (best_vertex == UINT32_MAX) {
            break;  /* No vertex can cover more edges */
        }

        /* Add vertex to transversal */
        vertices[transversal_size++] = hg->vertices[best_vertex].id;
        used_vertices[best_vertex] = true;

        /* Mark edges as covered */
        const nimcp_hypervertex_t* v = &hg->vertices[best_vertex];
        for (uint32_t j = 0; j < v->incident_count; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && v->incident_count > 256) {
                hypergraph_heartbeat("hypergraph_loop",
                                 (float)(j + 1) / (float)v->incident_count);
            }

            int eidx = find_edge_index(hg, v->incident_edges[j]);
            if (eidx >= 0 && !covered_edges[eidx]) {
                covered_edges[eidx] = true;
                uncovered_count--;
            }
        }
    }

    nimcp_free(covered_edges);
    covered_edges = NULL;
    nimcp_free(used_vertices);
    used_vertices = NULL;

    return transversal_size;
}

NIMCP_API uint32_t nimcp_hypergraph_all_transversals(
    const nimcp_hypergraph_t* hg,
    uint32_t** transversals,
    uint32_t* sizes,
    uint32_t max_transversals)
{
    /* Simplified implementation - just return one transversal */
    if (!hg || !transversals || !sizes || max_transversals == 0) {
        return 0;
    }

    transversals[0] = (uint32_t*)nimcp_calloc(hg->vertex_count, sizeof(uint32_t));
    if (!transversals[0]) {
        return 0;
    }

    sizes[0] = nimcp_hypergraph_transversal(hg, transversals[0], hg->vertex_count);

    return 1;
}

/* ============================================================================
 * Query Operations
 * ============================================================================ */

NIMCP_API uint32_t nimcp_hypergraph_find_edges_containing(
    const nimcp_hypergraph_t* hg,
    const uint32_t* vertices,
    uint32_t vertex_count,
    uint32_t* edges,
    uint32_t max_edges)
{
    if (!hg || !vertices || vertex_count == 0 || !edges || max_edges == 0) {
        return 0;
    }

    uint32_t count = 0;

    for (uint32_t i = 0; i < hg->edge_count && count < max_edges; i++) {
        const nimcp_hyperedge_t* edge = &hg->edges[i];
        bool contains_all = true;

        for (uint32_t j = 0; j < vertex_count; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && vertex_count > 256) {
                hypergraph_heartbeat("hypergraph_loop",
                                 (float)(j + 1) / (float)vertex_count);
            }

            bool found = false;
            for (uint32_t k = 0; k < edge->vertex_count; k++) {
                /* Phase 8: Loop progress heartbeat */
                if ((k & 0xFF) == 0 && edge->vertex_count > 256) {
                    hypergraph_heartbeat("hypergraph_loop",
                                     (float)(k + 1) / (float)edge->vertex_count);
                }

                if (edge->vertices[k] == vertices[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                contains_all = false;
                break;
            }
        }

        if (contains_all) {
            edges[count++] = edge->id;
        }
    }

    return count;
}

NIMCP_API uint32_t nimcp_hypergraph_get_neighbors(
    const nimcp_hypergraph_t* hg,
    uint32_t vertex_id,
    uint32_t* neighbors,
    uint32_t max_neighbors)
{
    if (!hg || !neighbors || max_neighbors == 0) {
        return 0;
    }

    int vidx = find_vertex_index(hg, vertex_id);
    if (vidx < 0) {
        return 0;
    }

    const nimcp_hypervertex_t* v = &hg->vertices[vidx];

    /* Use a simple array to track unique neighbors */
    bool* seen = (bool*)nimcp_calloc(hg->next_vertex_id, sizeof(bool));
    if (!seen) {
        return 0;
    }

    uint32_t count = 0;

    for (uint32_t i = 0; i < v->incident_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && v->incident_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)v->incident_count);
        }

        int eidx = find_edge_index(hg, v->incident_edges[i]);
        if (eidx < 0) continue;

        const nimcp_hyperedge_t* edge = &hg->edges[eidx];

        for (uint32_t j = 0; j < edge->vertex_count; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && edge->vertex_count > 256) {
                hypergraph_heartbeat("hypergraph_loop",
                                 (float)(j + 1) / (float)edge->vertex_count);
            }

            uint32_t neighbor_id = edge->vertices[j];
            if (neighbor_id == vertex_id) continue;
            if (neighbor_id < hg->next_vertex_id && !seen[neighbor_id]) {
                seen[neighbor_id] = true;
                if (count < max_neighbors) {
                    neighbors[count++] = neighbor_id;
                }
            }
        }
    }

    nimcp_free(seen);
    seen = NULL;

    return count;
}

NIMCP_API bool nimcp_hypergraph_are_connected(
    const nimcp_hypergraph_t* hg,
    uint32_t vertex_a,
    uint32_t vertex_b)
{
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_hypergraph_are_connected: hg is NULL");
        return false;
    }

    int vidx_a = find_vertex_index(hg, vertex_a);
    if (vidx_a < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_hypergraph_are_connected: validation failed");
        return false;
    }

    const nimcp_hypervertex_t* v = &hg->vertices[vidx_a];

    for (uint32_t i = 0; i < v->incident_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && v->incident_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)v->incident_count);
        }

        int eidx = find_edge_index(hg, v->incident_edges[i]);
        if (eidx < 0) continue;

        const nimcp_hyperedge_t* edge = &hg->edges[eidx];

        for (uint32_t j = 0; j < edge->vertex_count; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && edge->vertex_count > 256) {
                hypergraph_heartbeat("hypergraph_loop",
                                 (float)(j + 1) / (float)edge->vertex_count);
            }

            if (edge->vertices[j] == vertex_b) {
                return true;
            }
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_hypergraph_are_connected: validation failed");
    return false;
}

NIMCP_API uint32_t nimcp_hypergraph_pattern_query(
    const nimcp_hypergraph_t* hg,
    const nimcp_hypergraph_t* pattern,
    hypergraph_query_result_t* result,
    uint32_t max_matches)
{
    if (!hg || !pattern || !result) {
        return 0;
    }

    memset(result, 0, sizeof(hypergraph_query_result_t));

    uint32_t pat_vc = nimcp_hypergraph_vertex_count(pattern);
    uint32_t pat_ec = nimcp_hypergraph_edge_count(pattern);
    uint32_t hg_vc = nimcp_hypergraph_vertex_count(hg);

    if (pat_vc == 0 || hg_vc == 0) {
        return 0;
    }

    /* Simple pattern matching: find target vertices that match each pattern
       vertex by type. For single-vertex patterns, return all type-matching
       vertices. For multi-vertex patterns with edges, verify edge connectivity. */

    /* Allocate result arrays */
    uint32_t cap = (max_matches > 0 && max_matches < hg_vc) ? max_matches : hg_vc;
    result->vertex_ids = nimcp_calloc(cap, sizeof(uint32_t));
    if (!result->vertex_ids) return 0;

    if (pat_ec > 0) {
        result->edge_ids = nimcp_calloc(cap, sizeof(uint32_t));
        if (!result->edge_ids) {
            nimcp_free(result->vertex_ids);
            result->vertex_ids = NULL;
            return 0;
        }
    }

    /* Get the first pattern vertex as the anchor type to match.
       Access internal arrays directly since we're in the same compilation unit. */
    if (pattern->vertex_count == 0) {
        nimcp_free(result->vertex_ids);
        result->vertex_ids = NULL;
        if (result->edge_ids) { nimcp_free(result->edge_ids); result->edge_ids = NULL; }
        return 0;
    }
    const nimcp_hypervertex_t* pat_v0 = &pattern->vertices[0];

    uint32_t matches = 0;
    float total_conf = 0.0f;

    for (uint32_t i = 0; i < hg_vc && matches < cap; i++) {
        const nimcp_hypervertex_t* v = &hg->vertices[i];

        /* Match by type */
        if (v->type != pat_v0->type) continue;

        /* Match by label prefix if pattern has a non-empty label */
        if (pat_v0->label[0] != '\0') {
            size_t plen = strlen(pat_v0->label);
            if (strncmp(v->label, pat_v0->label, plen) != 0) continue;
        }

        /* For multi-vertex patterns, verify the vertex has enough edges */
        if (pat_ec > 0 && v->incident_count < pat_ec) continue;

        result->vertex_ids[matches] = v->id;
        total_conf += v->confidence;
        matches++;
    }

    result->vertex_count = matches;
    result->avg_confidence = (matches > 0) ? total_conf / (float)matches : 0.0f;
    return matches;
}

/* ============================================================================
 * Conversion Functions
 * ============================================================================ */

NIMCP_API nimcp_hypergraph_t* nimcp_hypergraph_from_knowledge_base(
    const void* logic)
{
    if (!logic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_hypergraph_from_knowledge_base: logic is NULL");
        return NULL;
    }

    /* Create hypergraph with a root vertex representing the knowledge base.
       Full KB→hypergraph conversion would require iterating KB facts/rules,
       which requires the typed symbolic_logic_t API. For now, create a
       structured graph with a KB source vertex that downstream code can
       extend with vertices/edges from query results. */
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    if (!hg) {
        return NULL;
    }

    /* Add a root vertex representing the knowledge base source */
    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_SET, "knowledge_base_root", 1.0f);
    return hg;
}

NIMCP_API nimcp_hypergraph_t* nimcp_hypergraph_from_ternary(
    const NimcpTernaryGraph* ternary)
{
    if (!ternary) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_hypergraph_from_ternary: ternary is NULL");
        return NULL;
    }

    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hypergraph_from_ternary: hg is NULL");
        return NULL;
    }

    /* Add vertices */
    for (uint32_t i = 0; i < ternary->vertex_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ternary->vertex_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)ternary->vertex_count);
        }

        nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT,
            NULL, 1.0f);
    }

    /* Add edges (convert binary edges to 2-vertex hyperedges) */
    for (uint32_t i = 0; i < ternary->vertex_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ternary->vertex_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)ternary->vertex_count);
        }

        const NimcpTernaryVertex* v = &ternary->vertices[i];
        NimcpTernaryEdge* edge = v->edges;

        while (edge) {
            if (i < edge->dest) {  /* Avoid duplicates */
                uint32_t verts[2] = { i + 1, edge->dest + 1 };
                nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION,
                    verts, 2, edge->weight, NULL);
            }
            edge = edge->next;
        }
    }

    return hg;
}

NIMCP_API NimcpTernaryGraph* nimcp_hypergraph_to_ternary(
    const nimcp_hypergraph_t* hg)
{
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_hypergraph_to_ternary: hg is NULL");
        return NULL;
    }

    NimcpTernaryGraph* ternary = nimcp_ternary_graph_create();
    if (!ternary) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hypergraph_to_ternary: ternary is NULL");
        return NULL;
    }

    /* Add vertices */
    for (uint32_t i = 0; i < hg->vertex_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hg->vertex_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)hg->vertex_count);
        }

        nimcp_ternary_graph_add_vertex(ternary, i, 0.0f, 0.0f, 0.0f, 0);
    }

    /* Create 2-section: connect vertices that share a hyperedge */
    for (uint32_t i = 0; i < hg->edge_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hg->edge_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)hg->edge_count);
        }

        const nimcp_hyperedge_t* edge = &hg->edges[i];

        for (uint32_t j = 0; j < edge->vertex_count; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && edge->vertex_count > 256) {
                hypergraph_heartbeat("hypergraph_loop",
                                 (float)(j + 1) / (float)edge->vertex_count);
            }

            for (uint32_t k = j + 1; k < edge->vertex_count; k++) {
                int idx_j = find_vertex_index(hg, edge->vertices[j]);
                int idx_k = find_vertex_index(hg, edge->vertices[k]);

                if (idx_j >= 0 && idx_k >= 0) {
                    nimcp_ternary_graph_add_edge(ternary, idx_j, idx_k,
                        edge->weight);
                }
            }
        }
    }

    return ternary;
}

NIMCP_API nimcp_error_t nimcp_hypergraph_to_tensor(
    const nimcp_hypergraph_t* hg,
    float* tensor,
    uint32_t tensor_size)
{
    if (!hg || !tensor || tensor_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_hypergraph_to_tensor: hg, tensor is NULL or tensor_size is 0");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Fill tensor with adjacency-like representation */
    memset(tensor, 0, tensor_size * sizeof(float));

    /* For each pair of vertices, set 1 if they share an edge */
    uint32_t n = hg->vertex_count;
    if (tensor_size < n * n) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE,
            "nimcp_hypergraph_to_tensor: tensor_size too small");
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    for (uint32_t i = 0; i < hg->edge_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hg->edge_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)hg->edge_count);
        }

        const nimcp_hyperedge_t* edge = &hg->edges[i];

        for (uint32_t j = 0; j < edge->vertex_count; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && edge->vertex_count > 256) {
                hypergraph_heartbeat("hypergraph_loop",
                                 (float)(j + 1) / (float)edge->vertex_count);
            }

            for (uint32_t k = j + 1; k < edge->vertex_count; k++) {
                int idx_j = find_vertex_index(hg, edge->vertices[j]);
                int idx_k = find_vertex_index(hg, edge->vertices[k]);

                if (idx_j >= 0 && idx_k >= 0) {
                    tensor[idx_j * n + idx_k] = 1.0f;
                    tensor[idx_k * n + idx_j] = 1.0f;
                }
            }
        }
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connected Components
 * ============================================================================ */

NIMCP_API nimcp_error_t nimcp_hypergraph_connected_components(
    const nimcp_hypergraph_t* hg,
    uint32_t* vertex_components,
    uint32_t* num_components)
{
    if (!hg || !vertex_components || !num_components) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_hypergraph_connected_components: hg, vertex_components, or num_components is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (hg->vertex_count == 0) {
        *num_components = 0;
        return NIMCP_SUCCESS;
    }

    /* Initialize component IDs */
    for (uint32_t i = 0; i < hg->vertex_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hg->vertex_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)hg->vertex_count);
        }

        vertex_components[i] = UINT32_MAX;
    }

    uint32_t current_component = 0;
    uint32_t* stack = (uint32_t*)nimcp_calloc(hg->vertex_count, sizeof(uint32_t));
    if (!stack) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_hypergraph_connected_components: failed to allocate stack");
        return NIMCP_ERROR_NO_MEMORY;
    }

    for (uint32_t start = 0; start < hg->vertex_count; start++) {
        /* Phase 8: Loop progress heartbeat */
        if ((start & 0xFF) == 0 && hg->vertex_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(start + 1) / (float)hg->vertex_count);
        }

        if (vertex_components[start] != UINT32_MAX) {
            continue;  /* Already assigned */
        }

        /* BFS/DFS from this vertex */
        uint32_t stack_top = 0;
        stack[stack_top++] = start;
        vertex_components[start] = current_component;

        while (stack_top > 0) {
            uint32_t vidx = stack[--stack_top];
            const nimcp_hypervertex_t* v = &hg->vertices[vidx];

            /* Visit all neighbors through incident edges */
            for (uint32_t i = 0; i < v->incident_count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && v->incident_count > 256) {
                    hypergraph_heartbeat("hypergraph_loop",
                                     (float)(i + 1) / (float)v->incident_count);
                }

                int eidx = find_edge_index(hg, v->incident_edges[i]);
                if (eidx < 0) continue;

                const nimcp_hyperedge_t* edge = &hg->edges[eidx];

                for (uint32_t j = 0; j < edge->vertex_count; j++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((j & 0xFF) == 0 && edge->vertex_count > 256) {
                        hypergraph_heartbeat("hypergraph_loop",
                                         (float)(j + 1) / (float)edge->vertex_count);
                    }

                    int nidx = find_vertex_index(hg, edge->vertices[j]);
                    if (nidx >= 0 && vertex_components[nidx] == UINT32_MAX) {
                        vertex_components[nidx] = current_component;
                        if (stack_top < hg->vertex_count) {
                            stack[stack_top++] = nidx;
                        }
                    }
                }
            }
        }

        current_component++;
    }

    nimcp_free(stack);
    stack = NULL;

    *num_components = current_component;
    return NIMCP_SUCCESS;
}

NIMCP_API bool nimcp_hypergraph_is_connected(const nimcp_hypergraph_t* hg)
{
    if (!hg || hg->vertex_count == 0) {
        return true;  /* Empty graph is trivially connected */
    }

    uint32_t* components = (uint32_t*)nimcp_malloc(
        hg->vertex_count * sizeof(uint32_t));
    if (!components) {
        return false;
    }

    uint32_t num_components = 0;
    nimcp_error_t result = nimcp_hypergraph_connected_components(
        hg, components, &num_components);

    nimcp_free(components);
    components = NULL;

    return (result == NIMCP_SUCCESS && num_components <= 1);
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

NIMCP_API nimcp_error_t nimcp_hypergraph_register_bio_async(
    nimcp_hypergraph_t* hg)
{
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_hypergraph_register_bio_async: hg is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (hg->thread_safe) {
        nimcp_mutex_lock(hg->mutex);
    }

    if (hg->bio_async_enabled) {
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        return NIMCP_SUCCESS;
    }

    /* Check if router is available */
    if (!bio_router_is_initialized()) {
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        return NIMCP_SUCCESS;  /* No router, skip */
    }

    /* Register with router */
    bio_module_info_t info = {
        .module_id = hg->module_id,
        .module_name = hg->module_name,
        .inbox_capacity = 32,
        .user_data = hg
    };

    hg->bio_ctx = bio_router_register_module(&info);
    if (hg->bio_ctx) {
        hg->bio_async_enabled = true;
    }

    if (hg->thread_safe) {
        nimcp_mutex_unlock(hg->mutex);
    }

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t nimcp_hypergraph_unregister_bio_async(
    nimcp_hypergraph_t* hg)
{
    if (!hg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_hypergraph_unregister_bio_async: hg is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (hg->thread_safe) {
        nimcp_mutex_lock(hg->mutex);
    }

    if (!hg->bio_async_enabled) {
        if (hg->thread_safe) {
            nimcp_mutex_unlock(hg->mutex);
        }
        return NIMCP_SUCCESS;
    }

    if (hg->bio_ctx) {
        bio_router_unregister_module(hg->bio_ctx);
        hg->bio_ctx = NULL;
    }

    hg->bio_async_enabled = false;

    if (hg->thread_safe) {
        nimcp_mutex_unlock(hg->mutex);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics and Diagnostics
 * ============================================================================ */

NIMCP_API nimcp_error_t nimcp_hypergraph_get_stats(
    const nimcp_hypergraph_t* hg,
    hypergraph_stats_t* stats)
{
    if (!hg || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_hypergraph_get_stats: hg or stats is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memcpy(stats, &hg->stats, sizeof(hypergraph_stats_t));
    return NIMCP_SUCCESS;
}

NIMCP_API uint32_t nimcp_hypergraph_vertex_count(const nimcp_hypergraph_t* hg)
{
    return hg ? hg->vertex_count : 0;
}

NIMCP_API uint32_t nimcp_hypergraph_edge_count(const nimcp_hypergraph_t* hg)
{
    return hg ? hg->edge_count : 0;
}

NIMCP_API void nimcp_hypergraph_print_diagnostics(const nimcp_hypergraph_t* hg)
{
    if (!hg) {
        return;
    }

    /* Would use NIMCP logging infrastructure */
}

/* ============================================================================
 * Query Result Management
 * ============================================================================ */

NIMCP_API nimcp_error_t nimcp_hypergraph_query_result_init(
    hypergraph_query_result_t* result,
    uint32_t max_vertices,
    uint32_t max_edges)
{
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_hypergraph_query_result_init: result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(result, 0, sizeof(hypergraph_query_result_t));

    if (max_vertices > 0) {
        result->vertex_ids = (uint32_t*)nimcp_calloc(max_vertices, sizeof(uint32_t));
        if (!result->vertex_ids) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "nimcp_hypergraph_query_result_init: failed to allocate vertex_ids");
            return NIMCP_ERROR_NO_MEMORY;
        }
    }

    if (max_edges > 0) {
        result->edge_ids = (uint32_t*)nimcp_calloc(max_edges, sizeof(uint32_t));
        if (!result->edge_ids) {
            if (result->vertex_ids) nimcp_free(result->vertex_ids);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "nimcp_hypergraph_query_result_init: failed to allocate edge_ids");
            return NIMCP_ERROR_NO_MEMORY;
        }
    }

    return NIMCP_SUCCESS;
}

NIMCP_API void nimcp_hypergraph_query_result_cleanup(
    hypergraph_query_result_t* result)
{
    if (!result) {
        return;
    }

    if (result->vertex_ids) {
        nimcp_free(result->vertex_ids);
    }
    if (result->edge_ids) {
        nimcp_free(result->edge_ids);
    }

    memset(result, 0, sizeof(hypergraph_query_result_t));
}


//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_hypergraph_part_helpers.c"  // 9 functions: helpers
#include "nimcp_hypergraph_part_core.c"  // 2 functions: core
