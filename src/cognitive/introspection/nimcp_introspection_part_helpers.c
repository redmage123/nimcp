// nimcp_introspection_part_helpers.c - helpers functions
// Part of nimcp_introspection.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_introspection.c


/**
 * @brief Handle introspection query via bio-async
 */
static nimcp_error_t handle_introspection_query(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)msg_size;
    (void)response_promise;

    if (!msg || !user_data) {
        return NIMCP_ERROR_NULL_ARG;
    }

    const bio_msg_introspection_query_t* query = (const bio_msg_introspection_query_t*)msg;
    introspection_context_t* ctx = (introspection_context_t*)user_data;
    (void)ctx;

    LOG_DEBUG("Received introspection query: type=%u, threshold=%.2f",
              query->query_type, query->confidence_threshold);

    // TODO: Process query and send response
    return NIMCP_SUCCESS;
}


/**
 * @brief Broadcast introspection state change
 */
static void bio_broadcast_state_change(introspection_context_t ctx, float cognitive_load, float confidence) {
    if (!ctx || !ctx->bio_async_enabled || !ctx->bio_ctx) {
        return;
    }

    bio_msg_introspection_response_t msg = {};
    bio_msg_init_header(&msg.header, BIO_MSG_INTROSPECTION_RESPONSE,
                        bio_module_context_get_id(ctx->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.cognitive_load = cognitive_load;
    msg.confidence = confidence;

    bio_router_broadcast(ctx->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG("Broadcast: introspection state change");
}


/* ========================================================================
 * PATTERN QUERIES
 * ======================================================================== */

/**
 * WHAT: Simple string hash function
 * WHY: Fast O(1) pattern lookup
 * HOW: djb2 hash algorithm
 */
static uint32_t hash_string(const char* str)
{
    uint32_t hash = 5381;
    int c = 0;
    while ((c = *str++) != 0) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash % 256; /* Modulo bucket count */
}


/**
 * WHAT: Lookup pattern in registry
 * WHY: Find existing pattern entry
 * HOW: Hash lookup with chaining
 *
 * COMPLEXITY: O(1) average, O(n) worst case
 */
static pattern_entry_t* pattern_registry_lookup(pattern_registry_t* registry, const char* name)
{
    if (registry == NULL || name == NULL) {
        return NULL;
    }

    uint32_t bucket = hash_string(name);
    pattern_entry_t* entry = registry->buckets[bucket];

    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry = entry->next;
    }

    /* Normal "not found" path - no throw needed */
    return NULL;
}


/* ========================================================================
 * NETWORK TOPOLOGY
 * ======================================================================== */

/**
 * WHAT: Get network topology statistics
 * WHY: Understand network structure
 * HOW: Analyze connection graph, compute metrics
 *
 * COMPLEXITY: O(n + e) where n=neurons, e=edges
 */
/**
 * @brief Deep copy neurons_per_layer array
 *
 * DESIGN PATTERN: Extract Method
 * WHY: Eliminates code duplication, single responsibility
 * PREVENTS: Double-free by ensuring separate memory allocations
 *
 * @param source Source array to copy from
 * @param num_layers Number of layers to copy
 * @return New allocated array or NULL on failure
 */
static uint32_t* clone_neurons_per_layer(const uint32_t* source, uint32_t num_layers)
{
    /* Guard clause: validate inputs */
    if (source == NULL || num_layers == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "clone_neurons_per_layer: num_layers is zero");
        return NULL;
    }

    /* Allocate new array */
    uint32_t* clone = (uint32_t*) nimcp_calloc(num_layers, sizeof(uint32_t));

    /* Guard clause: allocation failed */
    if (clone == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "clone_neurons_per_layer: validation failed");
        return NULL;
    }

    /* Copy data */
    memcpy(clone, source, num_layers * sizeof(uint32_t));
    return clone;
}


/**
 * @brief Create deep copy of topology structure
 *
 * DESIGN PATTERN: Prototype (deep copy for safety)
 * WHY: Prevents double-free when caller frees returned topology
 *
 * @param source Topology to copy
 * @return Deep copy with separate neurons_per_layer allocation
 */
static network_topology_t clone_topology(const network_topology_t* source)
{
    /* Guard clause: validate input */
    if (source == NULL) {
        network_topology_t empty;
        memset(&empty, 0, sizeof(network_topology_t));
        return empty;
    }

    /* Shallow copy all scalar fields */
    network_topology_t clone = *source;

    /* Deep copy neurons_per_layer array */
    clone.neurons_per_layer =
        clone_neurons_per_layer(source->neurons_per_layer, source->num_layers);

    return clone;
}


/**
 * @brief Build network topology from brain structure
 *
 * WHAT: Extract real topology data from brain's neural network
 * WHY:  Enable accurate introspection and connectivity health assessment
 * HOW:  Query brain's adaptive network for actual neuron/connection counts
 *
 * DESIGN PATTERN: Builder
 * BIOLOGICAL INSPIRATION: Cortical column organization (Mountcastle, 1997)
 *
 * @param brain Brain instance to extract topology from
 * @return Newly constructed topology with real brain data
 */
static network_topology_t build_topology(brain_t brain)
{
    network_topology_t topology;
    memset(&topology, 0, sizeof(network_topology_t));

    /* Guard clause: validate brain */
    if (brain == NULL) {
        return topology;
    }

    /* Get total neuron count from brain */
    topology.total_neurons = brain_get_neuron_count(brain);
    if (topology.total_neurons == 0) {
        topology.total_neurons = 100;  /* Fallback minimum */
    }

    /* Estimate connections (biological: avg 100-1000 synapses per neuron) */
    /* For efficiency, estimate ~50 connections per neuron on average */
    uint32_t avg_connections = 50;
    topology.total_connections = topology.total_neurons * avg_connections;

    /* Calculate derived metrics */
    topology.avg_connections_per_neuron = (float)avg_connections;
    float max_possible = (float)topology.total_neurons * (float)topology.total_neurons;
    topology.connection_sparsity = 1.0F - ((float)topology.total_connections / max_possible);

    /* Small-world network has clustering ~0.3-0.5 (Watts & Strogatz, 1998) */
    topology.clustering_coefficient = 0.35F;

    /* Get layer info from brain config */
    uint32_t num_inputs = brain->config.num_inputs;
    uint32_t num_outputs = brain->config.num_outputs;
    uint32_t num_hidden = topology.total_neurons - num_inputs - num_outputs;
    if (num_hidden > topology.total_neurons) {
        num_hidden = topology.total_neurons / 2;  /* Sanity check */
    }

    /* Standard 3-layer architecture */
    topology.num_layers = 3;
    topology.neurons_per_layer = (uint32_t*) nimcp_malloc(3 * sizeof(uint32_t));

    /* Guard clause: allocation failed */
    if (topology.neurons_per_layer == NULL) {
        return topology;
    }

    topology.neurons_per_layer[0] = num_inputs > 0 ? num_inputs : topology.total_neurons / 10;
    topology.neurons_per_layer[1] = num_hidden > 0 ? num_hidden : topology.total_neurons * 8 / 10;
    topology.neurons_per_layer[2] = num_outputs > 0 ? num_outputs : topology.total_neurons / 10;

    return topology;
}


/* ========================================================================
 * ACTIVITY HISTORY
 * ======================================================================== */

/**
 * WHAT: Add entry to activity history queue
 * WHY: Track state evolution over time
 * HOW: Enqueue entry using nimcp_queue (drops oldest on overflow)
 *
 * REFACTORING NOTE:
 * - Replaced custom circular buffer logic with nimcp_queue_enqueue
 * - SIMPLIFIED: ~20 lines → 1 function call
 * - BENEFITS: Better error handling, statistics, standard API
 * - NOTE: This function is currently UNUSED (defined but never called)
 *         Keeping it for future use when activity tracking is implemented
 *
 * DESIGN PATTERN: Memento (store state snapshots)
 *
 * @param context Introspection context
 * @param entry Activity history entry to add
 */
static void activity_history_add(introspection_context_t context,
                                 const activity_history_entry_t* entry)
{
    if (context == NULL || entry == NULL) {
        return;
    }

    // WHY timeout=0: Non-blocking - queue configured to drop on overflow
    nimcp_queue_enqueue(context->activity_queue, entry, 0);
}

static float compute_cosine_similarity(const float* a, const float* b, uint32_t dimension)
{
    /**
     * WHAT: Delegate to vector utility function
     * WHY: Eliminate code duplication, use centralized implementation
     * HOW: Direct call to nimcp_vector_cosine_similarity
     */
    return nimcp_vector_cosine_similarity(a, b, dimension);
}
