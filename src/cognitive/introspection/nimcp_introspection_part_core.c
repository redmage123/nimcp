// nimcp_introspection_part_core.c - core functions
// Part of nimcp_introspection.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_introspection.c


/**
 * WHAT: Compare two brain states for similarity
 * WHY: Detect state changes, measure drift
 * HOW: Cosine similarity between state vectors
 *
 * COMPLEXITY: O(d) where d = dimension
 */
float brain_state_similarity(const brain_state_t* state1, const brain_state_t* state2)
{
    if (state1 == NULL || state2 == NULL) {
        return 0.0F;
    }

    /* WHAT: States must have same dimension */
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_brain_state_similari", 0.0f);


    if (state1->dimension != state2->dimension) {
        return 0.0F;
    }

    float similarity = compute_cosine_similarity(state1->state_vector, state2->state_vector, state1->dimension);

    /* WHAT: Clamp to [0,1] to handle floating-point rounding errors */
    /* WHY: Cosine similarity can exceed 1.0 slightly due to FP arithmetic */
    /* HOW: Use fminf/fmaxf to clamp the value */
    if (similarity > 1.0F) {
        similarity = 1.0F;
    } else if (similarity < 0.0F) {
        similarity = 0.0F;
    }

    return similarity;
}


/**
 * WHAT: Get list of all registered patterns
 * WHY: Discover learned patterns
 * HOW: Scan hash table, collect names
 *
 * COMPLEXITY: O(p) where p = number of patterns
 */
char** brain_list_patterns(introspection_context_t context, uint32_t* num_patterns)
{
    if (context == NULL || num_patterns == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_list_patterns: validation failed");
        return NULL;
    }

    if (!context->config.enable_pattern_tracking || context->pattern_registry == NULL) {
        *num_patterns = 0;
        /* Feature disabled or registry not initialized - normal behavior, not an error */
        return NULL;
    }

    nimcp_mutex_lock(&context->pattern_registry->lock);

    *num_patterns = context->pattern_registry->num_patterns;
    if (*num_patterns == 0) {
        nimcp_mutex_unlock(&context->pattern_registry->lock);
        /* Empty registry is normal, not an error */
        return NULL;
    }

    /* WHAT: Allocate array of strings */
    char** pattern_list = (char**) nimcp_calloc(*num_patterns, sizeof(char*));
    if (pattern_list == NULL) {
        nimcp_mutex_unlock(&context->pattern_registry->lock);
        *num_patterns = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_list_patterns: validation failed");
        return NULL;
    }

    /* WHAT: Collect pattern names from all buckets */
    uint32_t index = 0;
    for (uint32_t bucket = 0; bucket < 256; bucket++) {
        /* Phase 8: Loop progress heartbeat */
        if ((bucket & 0xFF) == 0 && 256 > 256) {
            introspection_heartbeat("introspectio_loop",
                             (float)(bucket + 1) / (float)256);
        }

        pattern_entry_t* entry = context->pattern_registry->buckets[bucket];
        while (entry) {
            pattern_list[index++] = nimcp_strdup(entry->name);
            entry = entry->next;
        }
    }

    nimcp_mutex_unlock(&context->pattern_registry->lock);

    return pattern_list;
}


/* ========================================================================
 * AUTO ACTIVITY HISTORY IMPLEMENTATION
 * ======================================================================== */

/**
 * WHAT: Sample current brain activity and add to history
 * WHY: Enable auto-population of activity history from brain processing loop
 * HOW: Computes metrics from network, enqueues entry, invokes callback, sends bio-async message
 *
 * BIOLOGICAL BASIS:
 * - Simulates metacognitive monitoring (self-awareness of processing load)
 * - Energy estimation based on spiking activity (biological ATP consumption)
 * - Threshold-based activation counting (neuron firing vs resting states)
 *
 * COMPLEXITY: O(n) where n = network size
 * THREAD-SAFE: Yes (mutex protected)
 */
bool introspection_sample_activity(introspection_context_t context)
{
    /* WHAT: Validate input */
    if (context == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_sample_activity: validation failed");
        return false;
    }

    /* WHAT: Get brain's underlying network */
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_sample_activity", 0.0f);


    adaptive_network_t network = brain_get_network(context->brain);
    if (network == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_sample_activity: validation failed");
        return false;
    }

    /* WHAT: Get network size */
    uint32_t total_neurons = adaptive_network_get_neuron_count(network);
    if (total_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_sample_activity: total_neurons is zero");
        return false;
    }

    /* WHAT: Compute activity metrics */
    /* WHY: Provide snapshot of current neural state */

    /* Biological potential normalization constants */
    const float REST_POTENTIAL = -65.0F;
    const float PEAK_POTENTIAL = 30.0F;

    float sum_activation = 0.0F;
    float max_activation = 0.0F;
    uint32_t num_active = 0;

    /* WHAT: Scan all neurons to compute statistics */
    /* HOW: Sample activations, normalize to [0,1], compute aggregates */
    for (uint32_t i = 0; i < total_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && total_neurons > 256) {
            introspection_heartbeat("introspectio_loop",
                             (float)(i + 1) / (float)total_neurons);
        }

        float raw_activation = 0.0f;
        if (adaptive_network_get_neuron_activation(network, i, &raw_activation)) {
            /* Normalize biological potential to [0,1] */
            float normalized = (raw_activation - REST_POTENTIAL) / (PEAK_POTENTIAL - REST_POTENTIAL);
            if (normalized < 0.0F)
                normalized = 0.0F;
            if (normalized > 1.0F)
                normalized = 1.0F;

            sum_activation += normalized;

            if (normalized > max_activation) {
                max_activation = normalized;
            }

            if (normalized >= context->config.activity_threshold) {
                num_active++;
            }
        }
    }

    float avg_activation = total_neurons > 0 ? sum_activation / total_neurons : 0.0F;

    /* WHAT: Estimate energy consumption */
    /* WHY: Biological brains consume ATP proportional to spiking activity */
    /* HOW: Energy ≈ spike_rate * num_active + baseline_metabolism */
    /* BIOLOGICAL BASIS: ~10^8 ATP molecules per action potential (Attwell & Laughlin, 2001) */
    float baseline_energy = 0.1F;  /* Baseline metabolic cost */
    float spike_energy = avg_activation * 0.9F;  /* Activity-dependent cost */
    float energy_consumption = baseline_energy + spike_energy;

    /* WHAT: Create activity history entry */
    activity_history_entry_t entry = {
        .timestamp = nimcp_time_monotonic_ms(),
        .avg_activation = avg_activation,
        .max_activation = max_activation,
        .num_active = num_active,
        .energy_consumption = energy_consumption
    };

    /* WHAT: Apply change threshold filter */
    /* WHY: Avoid redundant samples when activity is stable */
    /* HOW: Only record if change exceeds threshold */
    if (context->config.history_change_threshold > 0.0F) {
        float change = fabsf(avg_activation - context->last_avg_activation);
        if (change < context->config.history_change_threshold) {
            /* Activity hasn't changed significantly - skip this sample */
            return true;  /* Success, but didn't record */
        }
    }

    /* WHAT: Update last activation for next comparison */
    context->last_avg_activation = avg_activation;

    /* WHAT: Add to activity history queue (circular buffer behavior) */
    /* WHY: Maintain temporal record of brain activity */
    /* HOW: If queue is full, drop oldest entry to make room */
    nimcp_result_t result = nimcp_queue_enqueue(context->activity_queue, &entry, 0);
    if (result != NIMCP_SUCCESS) {
        /* Queue is full - implement circular buffer behavior */
        /* Dequeue oldest entry to make room */
        activity_history_entry_t discarded;
        nimcp_result_t dequeue_result = nimcp_queue_dequeue(context->activity_queue, &discarded, 0);
        if (dequeue_result == NIMCP_SUCCESS) {
            /* Try enqueue again after making room */
            result = nimcp_queue_enqueue(context->activity_queue, &entry, 0);
        }
        if (result != NIMCP_SUCCESS) {
            /* Still failed - log but don't fail the sample operation */
            LOG_DEBUG(LOG_MODULE, "Activity queue full, sample dropped");
            /* Return true since the sampling itself succeeded, just storage failed */
            return true;
        }
    }

    /* WHAT: Invoke registered callback if present */
    /* WHY: Allow external observers to react to activity snapshots */
    if (context->sample_callback != NULL) {
        context->sample_callback(&entry, context->sample_callback_context);
    }

    /* WHAT: Send bio-async message if enabled */
    /* WHY: Notify other brain modules of activity snapshot */
    if (context->bio_async_enabled && context->bio_ctx) {
        bio_msg_introspection_response_t msg = {};
        bio_msg_init_header(&msg.header, BIO_MSG_INTROSPECTION_RESPONSE,
                            bio_module_context_get_id(context->bio_ctx), 0, sizeof(msg));
        msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
        msg.cognitive_load = avg_activation;
        msg.confidence = 1.0F - avg_activation;  /* Simple confidence estimate */
        bio_router_broadcast(context->bio_ctx, &msg, sizeof(msg));
    }

    return true;
}


/**
 * WHAT: Clear activity history queue
 * WHY: Reset history tracking
 * HOW: Clear all entries from queue
 *
 * COMPLEXITY: O(h) where h = current history size
 * THREAD-SAFE: Yes
 */
bool introspection_clear_history(introspection_context_t context)
{
    /* WHAT: Validate input */
    if (context == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_clear_history: validation failed");
        return false;
    }

    /* WHAT: Clear queue */
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_clear_history", 0.0f);


    nimcp_result_t result = nimcp_queue_clear(context->activity_queue);
    if (result != NIMCP_SUCCESS) {
        LOG_WARN(LOG_MODULE, "Failed to clear activity history: %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_clear_history: validation failed");
        return false;
    }

    /* WHAT: Reset last activation */
    nimcp_mutex_lock(&context->lock);
    context->last_avg_activation = 0.0F;
    nimcp_mutex_unlock(&context->lock);

    return true;
}


/* ========================================================================
 * IMMUNE SYSTEM INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Connect brain immune system to introspection module
 * WHY: Enable immune state to influence consciousness and metacognition
 * HOW: Store reference, register callbacks for immune events
 *
 * BIOLOGICAL BASIS:
 * - Systemic inflammation reduces consciousness (cytokine fatigue)
 * - Immune activation affects cognitive performance
 * - Memory cell formation is metacognitive learning
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool introspection_connect_immune(introspection_context_t context,
                                   struct brain_immune_system* immune_system)
{
    /* WHAT: Validate inputs */
    if (context == NULL || immune_system == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_connect_immune: validation failed");
        return false;
    }

    /* WHAT: Store immune system reference with thread safety */
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_connect_immune", 0.0f);


    nimcp_mutex_lock(&context->lock);
    context->immune_system = immune_system;
    nimcp_mutex_unlock(&context->lock);

    LOG_INFO(LOG_MODULE, "Brain immune system connected to introspection");

    return true;
}


/* ========================================================================
 * INTERNAL KNOWLEDGE GRAPH INTEGRATION
 * ======================================================================== */

bool introspection_connect_internal_kg(introspection_context_t context, brain_t brain)
{
    if (context == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_connect_internal_kg: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_connect_internal_kg", 0.0f);


    nimcp_mutex_lock(&context->lock);

    /* Initialize KG context */
    int result = kg_module_init(&context->kg_context, brain, "introspection");

    if (result != 0) {
        nimcp_mutex_unlock(&context->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_connect_internal_kg: validation failed");
        return false;
    }

    /* Check if KG is available */
    if (!kg_is_available(&context->kg_context)) {
        context->kg_connected = false;
        LOG_INFO(LOG_MODULE, "KG disabled, graceful degradation");
        nimcp_mutex_unlock(&context->lock);
        return true;  /* Success - just no KG */
    }

    context->kg_connected = true;
    LOG_INFO(LOG_MODULE, "Connected to internal KG");

    nimcp_mutex_unlock(&context->lock);
    return true;
}


void introspection_disconnect_internal_kg(introspection_context_t context)
{
    if (context == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_disconnect_internal_", 0.0f);


    nimcp_mutex_lock(&context->lock);

    context->kg_context.kg = NULL;
    context->kg_context.kg_available = false;
    context->kg_context.self_node_id = BRAIN_KG_INVALID_NODE;
    context->kg_connected = false;

    LOG_INFO(LOG_MODULE, "Disconnected from internal KG");

    nimcp_mutex_unlock(&context->lock);
}


/* ========================================================================
 * W9-kg READ-PATH INTEGRATION
 * ======================================================================== */

/**
 * @brief W9-kg read-path: query wellbeing events from the KG.
 *
 * WHAT: Counts wellbeing event nodes in the KG and emits an introspection
 *       report. Introspection reports include a health score derived from
 *       wellbeing context when available.
 * WHY:  Previously introspection had a self-node only. Now it reads
 *       wellbeing context and writes its report so W16 consumers see
 *       introspection reports.
 * HOW:  Uses the shared W9-kg helpers. Falls back to baseline (1.0) health
 *       when wellbeing evidence is absent.
 *
 * @param context introspection context (non-NULL)
 * @param report_id logical report id for the emitted event
 * @return count of wellbeing event nodes in the KG
 */
uint32_t introspection_query_wellbeing_context(introspection_context_t context,
                                               uint64_t report_id)
{
    if (context == NULL) return 0;

    struct brain_struct* brain = w9kg_get_registered_brain();
    if (!brain) return 0;

    uint32_t wellbeing_count = w9kg_query_introspection_wellbeing(brain);
    /* Health score: baseline 1.0 nudged down if KG reports nothing
     * (no introspective context available). */
    float health = (wellbeing_count > 0) ? 1.0f : 0.5f;
    w9kg_emit_introspection_report(brain, report_id, health);
    return wellbeing_count;
}


/* ========================================================================
 * KG SELF-AWARENESS INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Query knowledge graph for self-knowledge about introspection module
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int introspection_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    introspection_heartbeat("introspectio_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Introspection_Module");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                introspection_heartbeat("introspectio_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Introspection self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Introspection_Module");
    if (connections) {
        LOG_DEBUG("Introspection has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Introspection_Module");
    if (incoming) {
        LOG_DEBUG("Introspection has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}


/**
 * @brief Begin training - reset counters, set flags, log start
 */
int introspection_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_training_begin: ctx is NULL");
        return -1;
    }
    introspection_heartbeat_instance(g_introspection_instance_health_agent,
                                     "introspection_training_begin", 0.0f);
    /* Cast to opaque context and reset via public API */
    introspection_context_t context = (introspection_context_t)ctx;
    introspection_reset_stats(context);
    NIMCP_LOGGING_INFO("[INTROSPECTION] Training begin: stats reset, baseline initialized");
    return 0;
}


/**
 * @brief End training - compute final metrics, log results
 */
int introspection_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_training_end: ctx is NULL");
        return -1;
    }
    introspection_heartbeat_instance(g_introspection_instance_health_agent,
                                     "introspection_training_end", 1.0f);
    introspection_context_t context = (introspection_context_t)ctx;
    introspection_stats_t stats;
    if (introspection_get_stats(context, &stats)) {
        NIMCP_LOGGING_INFO("[INTROSPECTION] Training end: queries=%lu, memory=%zu bytes",
                           (unsigned long)stats.queries_total, stats.memory_used_bytes);
    }
    return 0;
}
