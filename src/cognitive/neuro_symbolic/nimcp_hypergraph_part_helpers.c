// nimcp_hypergraph_part_helpers.c - helpers functions
// Part of nimcp_hypergraph.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_hypergraph.c


/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

static uint32_t hash_vertex_edge(uint32_t vertex_id, uint32_t edge_id,
    uint32_t table_size)
{
    /* Simple hash combining vertex and edge IDs */
    uint32_t h = vertex_id * 31 + edge_id;
    return h % table_size;
}


static nimcp_error_t add_incidence(nimcp_hypergraph_t* hg, uint32_t vertex_id,
    uint32_t edge_id, uint32_t position)
{
    if (!hg->incidence_table) {
        return NIMCP_SUCCESS;
    }

    uint32_t hash = hash_vertex_edge(vertex_id, edge_id, hg->incidence_hash_size);

    incidence_entry_t* entry = (incidence_entry_t*)nimcp_malloc(
        sizeof(incidence_entry_t));
    if (!entry) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    entry->vertex_id = vertex_id;
    entry->edge_id = edge_id;
    entry->position = position;
    entry->next = hg->incidence_table[hash];
    hg->incidence_table[hash] = entry;

    return NIMCP_SUCCESS;
}


static nimcp_error_t remove_incidence(nimcp_hypergraph_t* hg, uint32_t vertex_id,
    uint32_t edge_id)
{
    if (!hg->incidence_table) {
        return NIMCP_SUCCESS;
    }

    uint32_t hash = hash_vertex_edge(vertex_id, edge_id, hg->incidence_hash_size);

    incidence_entry_t** pp = &hg->incidence_table[hash];
    while (*pp) {
        if ((*pp)->vertex_id == vertex_id && (*pp)->edge_id == edge_id) {
            incidence_entry_t* to_free = *pp;
            *pp = (*pp)->next;
            nimcp_free(to_free);
            to_free = NULL;
            return NIMCP_SUCCESS;
        }
        pp = &(*pp)->next;
    }

    return NIMCP_ERROR_NOT_FOUND;
}


static nimcp_error_t grow_vertices(nimcp_hypergraph_t* hg)
{
    uint32_t new_capacity = hg->vertex_capacity * 2;
    if (new_capacity > HYPERGRAPH_MAX_VERTICES) {
        new_capacity = HYPERGRAPH_MAX_VERTICES;
    }

    nimcp_hypervertex_t* new_vertices = (nimcp_hypervertex_t*)nimcp_realloc(
        hg->vertices, new_capacity * sizeof(nimcp_hypervertex_t));
    if (!new_vertices) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    hg->vertices = new_vertices;
    hg->vertex_capacity = new_capacity;

    return NIMCP_SUCCESS;
}


static nimcp_error_t grow_edges(nimcp_hypergraph_t* hg)
{
    uint32_t new_capacity = hg->edge_capacity * 2;
    if (new_capacity > HYPERGRAPH_MAX_EDGES) {
        new_capacity = HYPERGRAPH_MAX_EDGES;
    }

    nimcp_hyperedge_t* new_edges = (nimcp_hyperedge_t*)nimcp_realloc(
        hg->edges, new_capacity * sizeof(nimcp_hyperedge_t));
    if (!new_edges) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    hg->edges = new_edges;
    hg->edge_capacity = new_capacity;

    return NIMCP_SUCCESS;
}


static int find_vertex_index(const nimcp_hypergraph_t* hg, uint32_t vertex_id)
{
    for (uint32_t i = 0; i < hg->vertex_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hg->vertex_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)hg->vertex_count);
        }

        if (hg->vertices[i].id == vertex_id) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_vertex_index: validation failed");
    return -1;
}


static int find_edge_index(const nimcp_hypergraph_t* hg, uint32_t edge_id)
{
    for (uint32_t i = 0; i < hg->edge_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hg->edge_count > 256) {
            hypergraph_heartbeat("hypergraph_loop",
                             (float)(i + 1) / (float)hg->edge_count);
        }

        if (hg->edges[i].id == edge_id) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_edge_index: validation failed");
    return -1;
}


/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void hypergraph_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_hypergraph_health_agent = agent;
    }
}


int hypergraph_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hypergraph_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    hypergraph_heartbeat_instance(NULL, "hypergraph_training_step", progress);
    (void)(struct nimcp_hypergraph*)instance; /* Module state available for step adaptation */
    return 0;
}
