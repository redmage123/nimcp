// nimcp_collective_cognition_part_core.c - core functions
// Part of nimcp_collective_cognition.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_collective_cognition.c


/*=============================================================================
 * Instance Management API
 *===========================================================================*/

int collective_cognition_register_instance(
    collective_cognition_t* cc,
    uint32_t instance_id,
    brain_t* brain
) {
    if (!cc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_register_instance: cc is NULL");
        return -1;
    }

    /* Check if already registered */
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_register_instance", 0.0f);


    if (find_instance(cc, instance_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_cognition_register_instance: validation failed");
        return -1;  /* Already exists */
    }

    /* Find free slot */
    registered_instance_t* slot = find_free_slot(cc);
    if (!slot) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_register_instance: slot is NULL");
        return -1;  /* No free slots */
    }

    /* Initialize instance */
    slot->instance_id = instance_id;
    slot->brain = brain;
    slot->active = true;
    slot->local_phi = 0.3f;
    slot->atp_level = 1.0f;
    slot->fatigue_level = 0.0f;
    slot->last_heartbeat_us = get_timestamp_us();
    slot->missed_heartbeats = 0;

    /* Initialize band states with some variation */
    for (int b = 0; b < SYNC_BAND_COUNT; b++) {
        /* Phase 8: Loop progress heartbeat */
        if ((b & 0xFF) == 0 && SYNC_BAND_COUNT > 256) {
            collective_cognition_heartbeat("collective_c_loop",
                             (float)(b + 1) / (float)SYNC_BAND_COUNT);
        }

        slot->band_power[b] = 0.5f + (instance_id % 10) * 0.01f;
        slot->band_phase[b] = (instance_id * 0.5f);  /* Different starting phases */
    }

    cc->instance_count++;
    cc->stats.instances_joined++;

    /* W10 (2026-04-24): emit collective level change + query phi bias.
     * Uses the newly-registered instance's brain if available. */
    if (slot->brain) {
        int lvl = (int)cc->state.consciousness_level;
        wave10_collective_emit_level_change((struct brain_struct*)slot->brain,
                                            lvl, slot->local_phi);
        (void)wave10_collective_query_phi_bias((struct brain_struct*)slot->brain);
    }

    /* Register with subsystems so they can be accessed directly */
    if (cc->hyperscanning) {
        hyperscanning_register_instance(
            (hyperscanning_t*)cc->hyperscanning,
            instance_id,
            NULL,  /* No callback for now */
            NULL
        );
    }

    if (cc->intentionality) {
        shared_intentionality_register_instance(
            (shared_intentionality_t*)cc->intentionality,
            instance_id
        );
    }

    /* Note: extended_mind doesn't register instances, it registers extensions */
    /* Note: phi_system doesn't need instance registration */

    return 0;
}


int collective_cognition_unregister_instance(
    collective_cognition_t* cc,
    uint32_t instance_id
) {
    if (!cc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_unregister_instance: cc is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_unregister_instance", 0.0f);


    registered_instance_t* inst = find_instance(cc, instance_id);
    if (!inst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_unregister_instance: inst is NULL");
        return -1;  /* Not found */
    }

    inst->active = false;
    inst->brain = NULL;
    cc->instance_count--;
    cc->stats.instances_left++;

    /* Unregister from subsystems */
    if (cc->hyperscanning) {
        hyperscanning_unregister_instance(
            (hyperscanning_t*)cc->hyperscanning,
            instance_id
        );
    }

    if (cc->intentionality) {
        shared_intentionality_unregister_instance(
            (shared_intentionality_t*)cc->intentionality,
            instance_id
        );
    }

    return 0;
}


/*=============================================================================
 * Bio-Async Integration API
 *===========================================================================*/

int collective_cognition_connect_bio_async(
    collective_cognition_t* cc,
    bio_router_t* router
) {
    if (!cc || !router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_connect_bio_async: required parameter is NULL (cc, router)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_connect_bio_async", 0.0f);


    cc->bio_router = router;
    cc->bio_async_connected = true;

    /* TODO: Register message handlers when bio-async integration is implemented */

    return 0;
}


int collective_cognition_disconnect_bio_async(collective_cognition_t* cc) {
    if (!cc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_disconnect_bio_async: cc is NULL");
        return -1;
    }

    /* TODO: Unregister message handlers */

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_disconnect_bio_async", 0.0f);


    cc->bio_router = NULL;
    cc->bio_async_connected = false;

    return 0;
}


int collective_cognition_offload_task(
    collective_cognition_t* cc,
    uint32_t from_instance,
    uint32_t to_instance,
    const void* task_data,
    size_t task_size
) {
    if (!cc || !task_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_offload_task: required parameter is NULL (cc, task_data)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_offload_task", 0.0f);


    registered_instance_t* from = find_instance(cc, from_instance);
    registered_instance_t* to = find_instance(cc, to_instance);

    if (!from || !to) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_offload_task: required parameter is NULL (from, to)");
        return -1;
    }

    /* Update load estimates */
    float task_load = task_size * 0.0001f;  /* Estimate load from size */
    from->fatigue_level -= task_load;
    to->fatigue_level += task_load;

    if (from->fatigue_level < 0.0f) from->fatigue_level = 0.0f;
    if (to->fatigue_level > 1.0f) to->fatigue_level = 1.0f;

    cc->stats.bytes_transferred += task_size;

    return 0;
}


/*=============================================================================
 * Utility API
 *===========================================================================*/

const char* collective_consciousness_level_name(collective_consciousness_level_t level) {
    switch (level) {
        case COLLECTIVE_CONSCIOUSNESS_NONE:        return "NONE";
        case COLLECTIVE_CONSCIOUSNESS_MINIMAL:     return "MINIMAL";
        case COLLECTIVE_CONSCIOUSNESS_EMERGING:    return "EMERGING";
        case COLLECTIVE_CONSCIOUSNESS_PARTIAL:     return "PARTIAL";
        case COLLECTIVE_CONSCIOUSNESS_UNIFIED:     return "UNIFIED";
        case COLLECTIVE_CONSCIOUSNESS_TRANSCENDENT: return "TRANSCENDENT";
        default: return "UNKNOWN";
    }
}


const char* sync_band_name(sync_band_t band) {
    switch (band) {
        case SYNC_BAND_DELTA: return "DELTA";
        case SYNC_BAND_THETA: return "THETA";
        case SYNC_BAND_ALPHA: return "ALPHA";
        case SYNC_BAND_BETA:  return "BETA";
        case SYNC_BAND_GAMMA: return "GAMMA";
        default: return "UNKNOWN";
    }
}


const char* extension_type_name(extension_type_t type) {
    switch (type) {
        case EXT_TYPE_MEMORY:        return "MEMORY";
        case EXT_TYPE_PERCEPTION:    return "PERCEPTION";
        case EXT_TYPE_REASONING:     return "REASONING";
        case EXT_TYPE_ACTION:        return "ACTION";
        case EXT_TYPE_COMMUNICATION: return "COMMUNICATION";
        default: return "UNKNOWN";
    }
}


/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Collective Cognition self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int collective_cognition_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    collective_cognition_heartbeat("collective_c_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Collective_Cognition");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                collective_cognition_heartbeat("collective_c_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            printf("Collective Cognition self-knowledge: %s\n", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Collective_Cognition");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Collective_Cognition");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

int collective_cognition_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_cognition_training_begin: NULL argument");
        return -1;
    }
    collective_cognition_heartbeat_instance(g_collective_cognition_instance_health_agent, "coll_cog_train_begin", 0.0f);
    collective_cognition_t* ctx = (collective_cognition_t*)instance;

    /* Reset training counters */
    g_collective_cognition_training_steps = 0;
    g_collective_cognition_training_total_error = 0.0;
    g_collective_cognition_training_best_error = 1e30;
    g_collective_cognition_training_active = true;

    /* Reset module stats */
    memset(&ctx->stats, 0, sizeof(ctx->stats));

    NIMCP_LOGGING_INFO("collective_cognition training begin: counters reset");
    return 0;
}


int collective_cognition_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_cognition_training_end: NULL argument");
        return -1;
    }
    collective_cognition_heartbeat_instance(g_collective_cognition_instance_health_agent, "coll_cog_train_end", 1.0f);

    collective_cognition_t* ctx = (collective_cognition_t*)instance;
    /* Compute final averages */
    double avg_error = (g_collective_cognition_training_steps > 0)
        ? g_collective_cognition_training_total_error / (double)g_collective_cognition_training_steps
        : 0.0;

    uint64_t total_updates = ctx->stats.total_updates;

    /* Clear training flag */
    g_collective_cognition_training_active = false;

    NIMCP_LOGGING_INFO("collective_cognition training end: %lu steps, avg_error=%.6f, best_error=%.6f, total_updates=%lu",
                       (unsigned long)g_collective_cognition_training_steps,
                       avg_error, g_collective_cognition_training_best_error,
                       (unsigned long)total_updates);
    return 0;
}
