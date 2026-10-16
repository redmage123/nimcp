// nimcp_introspection_part_lifecycle.c - lifecycle functions
// Part of nimcp_introspection.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_introspection.c


/* ========================================================================
 * CONTEXT MANAGEMENT (Factory Pattern)
 * ======================================================================== */

/**
 * WHAT: Create introspection context
 * WHY: Initialize introspection subsystem for a brain
 * HOW: Allocate context, initialize pattern registry, history buffer
 *
 * COMPLEXITY: O(n) where n = network size (topology analysis)
 */
introspection_context_t introspection_context_create(brain_t brain,
                                                     const introspection_config_t* config)
{
    /* WHAT: Validate inputs */
    if (brain == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_context_create: brain is NULL");
        return NULL;
    }

    /* WHAT: Allocate context */
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_context_create", 0.0f);


    introspection_context_t context =
        (introspection_context_t) nimcp_calloc(1, sizeof(struct introspection_context_struct));
    if (context == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "introspection_context_create: failed to allocate context");
        return NULL;
    }

    /* WHAT: Initialize basic fields */
    context->brain = brain;
    context->config = config ? *config : introspection_default_config();
    context->topology_cached = false;
    context->bio_ctx = NULL;
    context->bio_async_enabled = false;
    context->ensemble = NULL;  /* Ensemble created on-demand or via introspection_set_ensemble */
    context->immune_system = NULL;  /* Immune system connected via introspection_connect_immune */
    nimcp_mutex_init(&context->lock, NULL);

    /* WHAT: Initialize auto activity history state */
    context->last_sample_time_ms = 0;
    context->sample_callback = NULL;
    context->sample_callback_context = NULL;
    context->last_avg_activation = 0.0F;
    /* P1-COG-02: Initialize thread-safe RNG seed */
    context->rand_seed = mc_seed_from_time();

    /* WHAT: Register with bio-async router if enabled */
    /* WHY: Enable asynchronous introspection queries and responses */
    if (context->config.enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_INTROSPECTION,
            .module_name = "introspection",
            .inbox_capacity = 64,
            .user_data = context
        };
        context->bio_ctx = bio_router_register_module(&bio_info);
        if (context->bio_ctx) {
            context->bio_async_enabled = true;

            /* Try KG-driven wiring callback registration first */
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_INTROSPECTION,
                (void*)introspection_wiring_handler_callback,
                context
            );

            if (wiring_result == NIMCP_SUCCESS) {
                LOG_INFO(LOG_MODULE, "Introspection: KG-driven wiring callback registered");
            } else {
                /* Legacy fallback - register handlers directly */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(context->bio_ctx, BIO_MSG_INTROSPECTION_QUERY, handle_introspection_query)
                );
                LOG_INFO(LOG_MODULE, "Introspection: legacy handler registration");
            }
        } else {
            LOG_WARN(LOG_MODULE, "Bio-async registration failed");
        }
    }

    /* WHAT: Create pattern registry if enabled */
    /* WHY: Track learned patterns for queries */
    if (context->config.enable_pattern_tracking) {
        context->pattern_registry =
            (pattern_registry_t*) nimcp_calloc(1, sizeof(pattern_registry_t));
        if (context->pattern_registry == NULL) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "introspection_context_create: failed to allocate pattern_registry");
            nimcp_free(context);
            context = NULL;
            return NULL;
        }
        nimcp_mutex_init(&context->pattern_registry->lock, NULL);
    }

    /* WHAT: Create activity history queue using nimcp_queue utility */
    /* WHY: Track state evolution over time with standardized queue API */
    /* HOW: Create blocking queue with capacity from config */
    nimcp_queue_config_t queue_config = {.max_size = context->config.history_size,
                                         .item_size = sizeof(activity_history_entry_t),
                                         .is_blocking =
                                             false,  // Don't block - drop oldest on overflow
                                         .timeout_ms = 0};

    nimcp_result_t result = nimcp_queue_create(&queue_config, &context->activity_queue);
    if (result != NIMCP_SUCCESS) {
        /* P2-COG-03: Remove redundant second throw - one is sufficient */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "introspection_context_create: failed to create activity_queue");
        if (context->pattern_registry) {
            nimcp_mutex_destroy(&context->pattern_registry->lock);
            nimcp_free(context->pattern_registry);
        }
        nimcp_mutex_destroy(&context->lock);
        nimcp_free(context);
        context = NULL;
        return NULL;
    }

    /* WHAT: Initialize statistics */
    memset(&context->stats, 0, sizeof(introspection_stats_t));
    context->stats.memory_used_bytes =
        sizeof(struct introspection_context_struct) + sizeof(pattern_registry_t) +
        (context->config.history_size * sizeof(activity_history_entry_t));

    /* WHAT: Initialize SNN and Plasticity bridges */
    /* WHY: Enable biologically-plausible metacognitive processing */
    context->snn_bridge = NULL;
    context->plasticity_bridge = NULL;
    context->bridges_enabled = false;

    introspection_snn_config_t snn_config = introspection_snn_config_default();
    context->snn_bridge = introspection_snn_create(&snn_config);

    introspection_plasticity_config_t plasticity_config = introspection_plasticity_config_default();
    context->plasticity_bridge = introspection_plasticity_create(&plasticity_config);

    if (context->snn_bridge && context->plasticity_bridge) {
        context->bridges_enabled = true;
    }

    return context;
}


/**
 * WHAT: Destroy introspection context
 * WHY: Free all resources and prevent memory leaks
 * HOW: Free pattern registry, history, topology, context itself
 */
void introspection_context_destroy(introspection_context_t context)
{
    if (context == NULL) {
        return;
    }

    /* WHAT: Unregister from bio-async router */
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_context_destroy", 0.0f);


    if (context->bio_async_enabled && context->bio_ctx) {
        bio_router_unregister_module(context->bio_ctx);
        context->bio_ctx = NULL;
        context->bio_async_enabled = false;
        LOG_INFO(LOG_MODULE, "Bio-async communication disabled");
    }

    /* WHAT: Free pattern registry */
    if (context->pattern_registry) {
        nimcp_mutex_lock(&context->pattern_registry->lock);
        for (uint32_t i = 0; i < 256; i++) {
            /* P3-COG-04: Removed dead heartbeat code (256 > 256 is always false) */
            pattern_entry_t* entry = context->pattern_registry->buckets[i];
            while (entry) {
                pattern_entry_t* next = entry->next;
                nimcp_free(entry->name);
                nimcp_free(entry);
                entry = NULL;
                entry = next;
            }
        }
        nimcp_mutex_unlock(&context->pattern_registry->lock);
        nimcp_mutex_destroy(&context->pattern_registry->lock);
        nimcp_free(context->pattern_registry);
    }

    /* WHAT: Destroy activity history queue */
    if (context->activity_queue) {
        nimcp_queue_destroy(context->activity_queue);
    }

    /* WHAT: Free topology cache */
    if (context->topology_cached) {
        network_topology_free(&context->topology);
    }

    /* WHAT: Destroy ensemble if owned */
    /* NOTE: ensemble is NOT owned by introspection context,
     * caller must destroy it separately. We just NULL the pointer here. */
    context->ensemble = NULL;

    /* WHAT: Destroy SNN and Plasticity bridges */
    if (context->snn_bridge) {
        introspection_snn_destroy(context->snn_bridge);
        context->snn_bridge = NULL;
    }
    if (context->plasticity_bridge) {
        introspection_plasticity_destroy(context->plasticity_bridge);
        context->plasticity_bridge = NULL;
    }

    /* WHAT: Destroy mutex and free context */
    nimcp_mutex_destroy(&context->lock);
    nimcp_free(context);
    context = NULL;
}


/**
 * WHAT: Free neuron population structure
 * WHY: Release allocated arrays
 * HOW: Free arrays, zero struct
 */
void neuron_population_free(neuron_population_t* population)
{
    if (population == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_neuron_population_fr", 0.0f);


    nimcp_free(population->neuron_ids);
    nimcp_free(population->activation_levels);
    memset(population, 0, sizeof(neuron_population_t));
}


/**
 * WHAT: Free brain state structure
 * WHY: Release allocated memory
 * HOW: Free vector and interpretation
 */
void brain_state_free(brain_state_t* state)
{
    if (state == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_brain_state_free", 0.0f);


    nimcp_free(state->state_vector);
    nimcp_free(state->interpretation);
    memset(state, 0, sizeof(brain_state_t));
}


/**
 * WHAT: Free uncertainty structure
 * WHY: Release ensemble predictions array
 * HOW: Free array, zero struct
 */
void brain_uncertainty_free(brain_uncertainty_t* uncertainty)
{
    if (uncertainty == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_brain_uncertainty_fr", 0.0f);


    nimcp_free(uncertainty->ensemble_predictions);
    memset(uncertainty, 0, sizeof(brain_uncertainty_t));
}


/**
 * WHAT: Free pattern info structure
 * WHY: Release memory
 * HOW: Free name string and struct
 */
void pattern_info_free(pattern_info_t* info)
{
    if (info == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_pattern_info_free", 0.0f);


    nimcp_free(info->pattern_name);
    nimcp_free(info);
    info = NULL;
}


/**
 * WHAT: Free pattern list
 * WHY: Release memory from brain_list_patterns
 * HOW: Free each string, free array
 */
void pattern_list_free(char** pattern_list, uint32_t num_patterns)
{
    if (pattern_list == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_pattern_list_free", 0.0f);


    for (uint32_t i = 0; i < num_patterns; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_patterns > 256) {
            introspection_heartbeat("introspectio_loop",
                             (float)(i + 1) / (float)num_patterns);
        }

        nimcp_free(pattern_list[i]);
    }
    nimcp_free(pattern_list);
    pattern_list = NULL;
}


/**
 * WHAT: Free topology structure
 * WHY: Release neurons_per_layer array
 * HOW: Free array, zero struct
 */
void network_topology_free(network_topology_t* topology)
{
    if (topology == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_network_topology_fre", 0.0f);


    nimcp_free(topology->neurons_per_layer);
    memset(topology, 0, sizeof(network_topology_t));
}


/**
 * WHAT: Reset introspection statistics
 * WHY: Clear counters for new measurement
 * HOW: Zero all counters except memory usage
 *
 * COMPLEXITY: O(1)
 */
void introspection_reset_stats(introspection_context_t context)
{
    if (context == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_reset_stats", 0.0f);


    nimcp_mutex_lock(&context->lock);

    size_t memory_used = context->stats.memory_used_bytes;
    memset(&context->stats, 0, sizeof(introspection_stats_t));
    context->stats.memory_used_bytes = memory_used;

    nimcp_mutex_unlock(&context->lock);
}
