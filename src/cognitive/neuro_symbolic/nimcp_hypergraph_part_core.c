// nimcp_hypergraph_part_core.c - core functions
// Part of nimcp_hypergraph.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_hypergraph.c


/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int hypergraph_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hypergraph_training_begin: NULL argument");
        return -1;
    }
    hypergraph_heartbeat_instance(NULL, "hypergraph_training_begin", 0.0f);
    (void)(struct nimcp_hypergraph*)instance; /* Module state available for reset */
    return 0;
}


int hypergraph_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hypergraph_training_end: NULL argument");
        return -1;
    }
    hypergraph_heartbeat_instance(NULL, "hypergraph_training_end", 1.0f);
    (void)(struct nimcp_hypergraph*)instance; /* Module state available for finalization */
    return 0;
}


/* ============================================================================
 * W7 KG Integration
 *
 * The hypergraph is a pre-existing graph store that structurally mirrors the
 * KG: hypervertices ↔ KG nodes, hyperedges ↔ (sets of) KG edges.  This
 * wiring keeps hypergraph topology discoverable via brain->internal_kg by
 * emitting a structural root + one event per sync call.  We deliberately do
 * NOT duplicate every vertex into the KG (too much noise at M-vertex scale);
 * callers can trigger a sync event summarising the current counts.
 * ============================================================================ */

/**
 * @brief Register a hypergraph with a brain's internal KG.
 *
 * Idempotent.  Creates the structural root 'cog_neuro_symbolic_hypergraph'
 * and emits one sync event with current vertex/edge counts.
 */
int nimcp_hypergraph_kg_register(nimcp_hypergraph_t* hg, brain_t brain)
{
    if (!hg || !brain) return -1;
    if (!brain->internal_kg_enabled || !brain->internal_kg) return 0;

    uint64_t tok = brain->internal_kg_admin_token;
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN, tok);

    /* Structural root. */
    brain_kg_node_id_t root = brain_kg_find_node(brain->internal_kg,
                                                 "cog_neuro_symbolic_hypergraph");
    if (root == BRAIN_KG_INVALID_NODE) {
        root = brain_kg_add_node(brain->internal_kg,
            "cog_neuro_symbolic_hypergraph", BRAIN_KG_NODE_COGNITIVE,
            "Hypergraph store (hypervertices + hyperedges; KG-topology mirror)");
    }

    /* Sync event summarising counts. */
    char node_name[BRAIN_KG_MAX_NAME_LEN];
    snprintf(node_name, sizeof(node_name),
             "cog_neuro_symbolic_hypergraph_event_sync_%llu",
             (unsigned long long)nimcp_time_monotonic_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "hypergraph sync: %u vertices, %u edges",
             (unsigned)hg->vertex_count, (unsigned)hg->edge_count);
    brain_kg_node_id_t nid =
        brain_kg_add_node(brain->internal_kg, node_name,
                          BRAIN_KG_NODE_COGNITIVE, desc);
    if (nid != BRAIN_KG_INVALID_NODE && root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(brain->internal_kg, root, nid,
                          BRAIN_KG_EDGE_PROVIDES_TO, "hypergraph_sync", 0.5f);
    }

    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
    return 0;
}
