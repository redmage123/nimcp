// nimcp_systems_consolidation_part_lifecycle.c - lifecycle functions
// Part of nimcp_systems_consolidation.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_systems_consolidation.c


//=============================================================================
// Cortical Node Management
//=============================================================================

/**
 * @brief Create a new cortical memory node
 *
 * WHAT: Allocates and initializes a cortical memory node
 * WHY:  Represents abstracted memory in cortex
 * HOW:  Allocates structure and feature vector
 *
 * @param features Semantic feature vector (will be copied)
 * @param feature_dim Dimensionality of features
 * @param source_engram_id Original hippocampal engram
 * @return Pointer to new node, or NULL on failure
 */
static cortical_memory_node_t* cortical_node_create(
    const float* features,
    uint32_t feature_dim,
    uint64_t source_engram_id)
{
    // WHAT: Guard against invalid input
    if (!features || feature_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_node_create: features is NULL");
        return NULL;
    }

    // WHAT: Allocate node structure
    cortical_memory_node_t* node = nimcp_calloc(1, sizeof(cortical_memory_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate node");

        return NULL;
    }

    // WHAT: Allocate and copy feature vector
    node->features = nimcp_calloc(feature_dim, sizeof(float));
    if (!node->features) {
        nimcp_free(node);
        node = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_node_create: node->features is NULL");
        return NULL;
    }
    memcpy(node->features, features, feature_dim * sizeof(float));

    // WHAT: Allocate neighbor arrays
    node->neighbor_capacity = CONSOLIDATION_DEFAULT_NEIGHBORS_PER_NODE;
    node->neighbors = nimcp_calloc(node->neighbor_capacity, sizeof(cortical_memory_node_t*));
    node->neighbor_strengths = nimcp_calloc(node->neighbor_capacity, sizeof(float));

    if (!node->neighbors || !node->neighbor_strengths) {
        nimcp_free(node->features);
        nimcp_free(node->neighbors);
        nimcp_free(node->neighbor_strengths);
        nimcp_free(node);
        node = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_node_create: required parameter is NULL (node->neighbors, node->neighbor_strengths)");
        return NULL;
    }

    // WHAT: Initialize node state
    node->id = generate_node_id();
    node->feature_dim = feature_dim;
    node->type = CORTICAL_MEMORY_EPISODIC;  // Starts episodic
    node->consolidation_strength = 0.0F;    // Starts weak
    node->hippocampal_dependency = 1.0F;    // Fully dependent on hippocampus initially
    node->creation_time_ms = nimcp_platform_time_monotonic_ms();
    node->last_activation_ms = node->creation_time_ms;
    node->source_engram_id = source_engram_id;
    node->is_transferred = false;
    node->neighbor_count = 0;

    // Note: Bio-async is now handled at the system level, not per-node

    return node;
}


/**
 * @brief Destroy cortical memory node and free resources
 *
 * WHAT: Frees all memory associated with cortical node
 * WHY:  Prevents memory leaks
 * HOW:  Frees arrays and structure in correct order
 *
 * @param node Node to destroy (can be NULL)
 */
static void cortical_node_destroy(cortical_memory_node_t* node)
{
    // WHAT: Guard against NULL
    if (!node) {
        return;
    }

    // WHAT: Free allocated arrays
    if (node->features) {
        nimcp_free(node->features);
    }
    if (node->neighbors) {
        nimcp_free(node->neighbors);
    }
    if (node->neighbor_strengths) {
        nimcp_free(node->neighbor_strengths);
    }

    // Note: Bio-async is now handled at the system level, not per-node

    // WHAT: Free node structure
    nimcp_free(node);
    node = NULL;
}


//=============================================================================
// System Management API
//=============================================================================

systems_consolidation_system_t* systems_consolidation_create(void)
{
    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_heartbeat("consolidatio_systems_consolidatio", 0.0f);


    LOG_INFO("Creating systems consolidation system");

    // WHAT: Allocate main system structure
    systems_consolidation_system_t* system =
        nimcp_calloc(1, sizeof(systems_consolidation_system_t));
    if (!system) {
        LOG_ERROR("Failed to allocate systems consolidation system (%zu bytes)", sizeof(systems_consolidation_system_t));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "systems_consolidation_create: system is NULL");
        return NULL;
    }

    // WHAT: Allocate cortical node storage
    system->node_capacity = CONSOLIDATION_DEFAULT_CORTICAL_CAPACITY;
    system->cortical_nodes =
        nimcp_calloc(system->node_capacity, sizeof(cortical_memory_node_t*));
    if (!system->cortical_nodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "systems_consolidation_create: system->cortical_nodes is NULL");
        goto cleanup;
    }

    // WHAT: Allocate replay queue
    system->replay_queue_capacity = CONSOLIDATION_DEFAULT_REPLAY_QUEUE_SIZE;
    system->replay_queue =
        nimcp_calloc(system->replay_queue_capacity, sizeof(replay_event_t));
    if (!system->replay_queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "systems_consolidation_create: system->replay_queue is NULL");
        goto cleanup;
    }

    // WHAT: Initialize parameters with biological defaults
    system->node_count = 0;
    system->replay_queue_size = 0;
    system->replay_frequency_hz = CONSOLIDATION_REPLAY_FREQUENCY_SWS;
    system->last_replay_time_ms = nimcp_platform_time_monotonic_ms();
    system->transfer_rate = CONSOLIDATION_TRANSFER_RATE_SWS;
    system->forgetting_rate = CONSOLIDATION_FORGETTING_RATE;
    system->semantic_threshold = CONSOLIDATION_SEMANTIC_THRESHOLD;
    system->engram_system = NULL;  // Set via integration API
    system->sleep_system = NULL;   // Set via integration API
    system->total_replays = 0;
    system->total_transfers = 0;
    system->total_forgotten = 0;

    // Phase 1.5: Initialize memory pools for hot-path allocations
    memory_pool_config_t node_pool_config = {
        .block_size = sizeof(cortical_memory_node_t),
        .num_blocks = 64,  // Pre-allocate typical usage
        .alignment = 16,   // SIMD alignment
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    system->node_pool = memory_pool_create(&node_pool_config);

    memory_pool_config_t feature_pool_config = {
        .block_size = 32 * sizeof(float),  // Default feature dimension
        .num_blocks = 64,
        .alignment = 16,
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    system->feature_pool = memory_pool_create(&feature_pool_config);

    memory_pool_config_t neighbor_pool_config = {
        .block_size = CONSOLIDATION_DEFAULT_NEIGHBORS_PER_NODE * sizeof(void*),
        .num_blocks = 64,
        .alignment = 16,
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    system->neighbor_pool = memory_pool_create(&neighbor_pool_config);

    // Bio-async registration at system level
    system->bio_ctx = NULL;
    system->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_SYSTEMS_CONSOLIDATION,
            .module_name = "systems_consolidation",
            .inbox_capacity = 64,
            .user_data = system
        };
        system->bio_ctx = bio_router_register_module(&bio_info);
        if (system->bio_ctx) {
            system->bio_async_enabled = true;

            // Try KG-driven wiring callback first
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_SYSTEMS_CONSOLIDATION,
                (void*)systems_consolidation_wiring_handler_callback,
                system
            );

            if (wiring_result != NIMCP_SUCCESS) {
                // Fallback to legacy hardcoded registration
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(system->bio_ctx, BIO_MSG_CONSOLIDATION_TRIGGER,
                                                handle_consolidation_trigger)
                );
                LOG_INFO("Bio-async registered with legacy handlers (module_id=0x%04X)",
                         BIO_MODULE_SYSTEMS_CONSOLIDATION);
            } else {
                LOG_INFO("Bio-async registered with KG wiring callback (module_id=0x%04X)",
                         BIO_MODULE_SYSTEMS_CONSOLIDATION);
            }
        } else {
            LOG_WARN("Failed to register with bio_router (async disabled)");
        }
    }

    return system;

cleanup:
    nimcp_free(system->replay_queue);
    nimcp_free(system->cortical_nodes);
    nimcp_free(system);
    system = NULL;
    return NULL;
}


void systems_consolidation_destroy(systems_consolidation_system_t* system)
{
    // WHAT: Guard against NULL
    if (!system) {
        return;
    }

    // Unregister from bio-router
    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_heartbeat("consolidatio_systems_consolidatio", 0.0f);


    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
        system->bio_async_enabled = false;
        LOG_DEBUG("Unregistered from bio_router");
    }

    // WHAT: Free all cortical nodes
    if (system->cortical_nodes) {
        for (uint32_t i = 0; i < system->node_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->node_count > 256) {
                systems_consolidation_heartbeat("consolidatio_loop",
                                 (float)(i + 1) / (float)system->node_count);
            }

            cortical_node_destroy(system->cortical_nodes[i]);
        }
        nimcp_free(system->cortical_nodes);
    }

    // WHAT: Free replay queue
    if (system->replay_queue) {
        nimcp_free(system->replay_queue);
    }

    // Phase 1.5: Destroy memory pools
    if (system->node_pool) {
        memory_pool_destroy(system->node_pool);
    }
    if (system->feature_pool) {
        memory_pool_destroy(system->feature_pool);
    }
    if (system->neighbor_pool) {
        memory_pool_destroy(system->neighbor_pool);
    }

    // WHAT: Free system structure
    nimcp_free(system);
    system = NULL;
}


void systems_consolidation_reset(systems_consolidation_system_t* system)
{
    // WHAT: Guard against NULL
    if (!system) {
        return;
    }

    // WHAT: Free all cortical nodes
    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_heartbeat("consolidatio_systems_consolidatio", 0.0f);


    for (uint32_t i = 0; i < system->node_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->node_count > 256) {
            systems_consolidation_heartbeat("consolidatio_loop",
                             (float)(i + 1) / (float)system->node_count);
        }

        cortical_node_destroy(system->cortical_nodes[i]);
        system->cortical_nodes[i] = NULL;
    }

    // WHAT: Reset counters (keep allocated capacity)
    system->node_count = 0;
    system->replay_queue_size = 0;
    system->total_replays = 0;
    system->total_transfers = 0;
    system->total_forgotten = 0;
    system->last_replay_time_ms = nimcp_platform_time_monotonic_ms();
}
