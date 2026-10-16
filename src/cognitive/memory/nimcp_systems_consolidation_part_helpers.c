// nimcp_systems_consolidation_part_helpers.c - helpers functions
// Part of nimcp_systems_consolidation.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_systems_consolidation.c


//=============================================================================
// Internal Helper Functions
//=============================================================================

/* Cosine similarity uses nimcp_vector_cosine_similarity from utils/containers/nimcp_vector.h */

/**
 * @brief Generate unique ID for cortical memory node
 *
 * WHAT: Creates unique identifier for cortical nodes
 * WHY:  Needed to track and reference nodes
 * HOW:  Uses timestamp + counter for uniqueness
 *
 * @return Unique 64-bit ID
 */
static uint64_t generate_node_id(void)
{
    static _Atomic uint64_t counter = 0;
    uint64_t timestamp = nimcp_platform_time_monotonic_ms();
    return (timestamp << 16) | (atomic_fetch_add(&counter, 1) & 0xFFFF);
}


/*=============================================================================
 * BIO-ASYNC HANDLER IMPLEMENTATIONS
 *============================================================================*/

static nimcp_error_t handle_consolidation_trigger(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)msg_size;
    (void)response_promise;
    NIMCP_CHECK_THROW(msg && user_data, NIMCP_ERROR_NULL_ARG, "msg or user_data is NULL");
    const bio_msg_salience_response_t* trigger = (const bio_msg_salience_response_t*)msg;
    systems_consolidation_system_t* system = (systems_consolidation_system_t*)user_data;
    LOG_DEBUG(LOG_MODULE, "Received consolidation trigger: stimulus_id=%u, strength=%.2f, node_count=%u",
              trigger->stimulus_id, trigger->salience_score, system->node_count);

    // Schedule replay for the triggered engram
    systems_consolidation_schedule_replay(system, trigger->stimulus_id, trigger->salience_score);

    return NIMCP_SUCCESS;
}


static void bio_broadcast_consolidation_complete(systems_consolidation_system_t* system, uint32_t engram_id, float strength) {
    if (!system || !system->bio_async_enabled || !system->bio_ctx) { return; }
    // Use salience response for consolidation complete notification
    bio_msg_salience_response_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(system->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.stimulus_id = engram_id;
    msg.salience_score = strength;
    msg.attention_priority = strength;
    msg.requires_immediate_attention = false;
    bio_router_broadcast(system->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG(LOG_MODULE, "Broadcast consolidation complete: engram=%u, strength=%.2f", engram_id, strength);
}


/**
 * @brief Add a neighbor link to cortical node
 *
 * WHAT: Creates semantic similarity connection between nodes
 * WHY:  Models lateral cortical connections (concepts cluster)
 * HOW:  Adds neighbor pointer and connection strength
 *
 * @param node Node to add neighbor to
 * @param neighbor Neighboring node
 * @param strength Connection strength (0.0-1.0)
 * @return true if added, false if capacity reached
 */
static bool cortical_node_add_neighbor(
    cortical_memory_node_t* node,
    cortical_memory_node_t* neighbor,
    float strength)
{
    // WHAT: Guard against invalid input
    if (!node || !neighbor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_node_add_neighbor: required parameter is NULL (node, neighbor)");
        return false;
    }

    // WHAT: Check capacity
    if (node->neighbor_count >= node->neighbor_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "cortical_node_add_neighbor: capacity exceeded");
        return false;  // Max neighbors reached
    }

    // WHAT: Add neighbor and strength
    node->neighbors[node->neighbor_count] = neighbor;
    node->neighbor_strengths[node->neighbor_count] = strength;
    node->neighbor_count++;

    return true;
}
