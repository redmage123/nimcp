// nimcp_systems_consolidation_part_core.c - core functions
// Part of nimcp_systems_consolidation.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_systems_consolidation.c


//=============================================================================
// Sleep Replay API
//=============================================================================

bool systems_consolidation_schedule_replay(
    systems_consolidation_system_t* system,
    uint64_t engram_id,
    float priority)
{
    // WHAT: Guard against invalid input
    if (!system || engram_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "systems_consolidation_schedule_replay: system is NULL");
        return false;
    }

    // WHAT: Check queue capacity
    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_heartbeat("consolidatio_systems_consolidatio", 0.0f);


    if (system->replay_queue_size >= system->replay_queue_capacity) {
        return false;  // Queue full — normal operating condition, not an error
    }

    // WHAT: Create replay event
    replay_event_t event = {
        .engram_id = engram_id,
        .cortical_node_id = 0,  // Will be determined during replay
        .priority = priority,
        .emotional_salience = priority,  // Simplified: use priority as salience
        .scheduled_time_ms = nimcp_platform_time_monotonic_ms(),
        .is_completed = false
    };

    // WHAT: Add to queue
    system->replay_queue[system->replay_queue_size] = event;
    system->replay_queue_size++;

    return true;
}


//=============================================================================
// Cortical Transfer API
//=============================================================================

uint64_t systems_consolidation_transfer_to_cortex(
    systems_consolidation_system_t* system,
    uint64_t engram_id,
    float replay_strength)
{
    // WHAT: Guard against invalid input
    if (!system || engram_id == 0 || replay_strength <= 0.0F) {
        return 0;
    }

    // WHAT: Guard against missing engram system (currently optional for testing)
    // WHY: During testing, we may not have a full engram system
    // TODO: Make required once integration phase is complete
    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_heartbeat("consolidatio_systems_consolidatio", 0.0f);


    bool have_engram_system = (system->engram_system != NULL);

    // WHAT: Extract semantic features (simplified implementation)
    // WHY: Cortex stores abstracted/semantic representations
    // HOW: Create semantic feature vector from engram ID
    // NOTE: In full integration, would query engram_system for actual neurons/activations
    const uint32_t SEMANTIC_DIM = 32;  // Semantic feature dimensionality
    float semantic_features[SEMANTIC_DIM];

    if (have_engram_system) {
        // TODO: Replace with actual engram_get_neurons() call
        // For now, use deterministic features based on engram ID
        for (uint32_t i = 0; i < SEMANTIC_DIM; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && SEMANTIC_DIM > 256) {
                systems_consolidation_heartbeat("consolidatio_loop",
                                 (float)(i + 1) / (float)SEMANTIC_DIM);
            }

            semantic_features[i] = ((float)(engram_id % 100) / 100.0F) + (i * 0.01F);
        }
    } else {
        // Simplified mode for testing without engram system
        for (uint32_t i = 0; i < SEMANTIC_DIM; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && SEMANTIC_DIM > 256) {
                systems_consolidation_heartbeat("consolidatio_loop",
                                 (float)(i + 1) / (float)SEMANTIC_DIM);
            }

            semantic_features[i] = ((float)(engram_id % 100) / 100.0F) + (i * 0.01F);
        }
    }

    // WHAT: Check if cortical node already exists for this engram
    cortical_memory_node_t* existing_node = NULL;
    for (uint32_t i = 0; i < system->node_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->node_count > 256) {
            systems_consolidation_heartbeat("consolidatio_loop",
                             (float)(i + 1) / (float)system->node_count);
        }

        if (system->cortical_nodes[i]->source_engram_id == engram_id) {
            existing_node = system->cortical_nodes[i];
            break;
        }
    }

    if (existing_node) {
        // WHAT: Update existing node (strengthening)
        // WHY: Repeated replay consolidates memory
        existing_node->consolidation_strength += replay_strength * 0.1F;
        if (existing_node->consolidation_strength > 1.0F) {
            existing_node->consolidation_strength = 1.0F;
        }

        existing_node->hippocampal_dependency -= replay_strength * 0.05F;
        if (existing_node->hippocampal_dependency < 0.0F) {
            existing_node->hippocampal_dependency = 0.0F;
            existing_node->is_transferred = true;
            system->total_transfers++;
        }

        existing_node->last_activation_ms = nimcp_platform_time_monotonic_ms();

        /* W6: emit KG consolidation event (strengthen path) */
        memory_kg_emit_consolidation_transfer(
            memory_kg_events_get_registered_brain(),
            engram_id, existing_node->id,
            existing_node->consolidation_strength);

        return existing_node->id;
    }

    // WHAT: Create new cortical node
    // WHY: First transfer of this engram to cortex
    cortical_memory_node_t* new_node = cortical_node_create(
        semantic_features,
        SEMANTIC_DIM,
        engram_id
    );

    if (!new_node) {
        return 0;  // Allocation failed
    }

    // WHAT: Check capacity
    if (system->node_count >= system->node_capacity) {
        cortical_node_destroy(new_node);
        return 0;  // Cortex full
    }

    // WHAT: Apply initial replay strength to new node
    // WHY: First replay establishes initial consolidation
    new_node->consolidation_strength = replay_strength * 0.1F;
    new_node->hippocampal_dependency -= replay_strength * 0.05F;

    // WHAT: Add to system
    system->cortical_nodes[system->node_count] = new_node;
    system->node_count++;

    // WHAT: Link to similar nodes (semantic clustering)
    // WHY: Cortex groups related concepts (lateral connections)
    for (uint32_t i = 0; i < system->node_count - 1; i++) {
        cortical_memory_node_t* other_node = system->cortical_nodes[i];
        float similarity = nimcp_vector_cosine_similarity(
            new_node->features,
            other_node->features,
            SEMANTIC_DIM
        );

        // WHAT: Link if sufficiently similar
        if (similarity > 0.7F) {
            cortical_node_add_neighbor(new_node, other_node, similarity);
            cortical_node_add_neighbor(other_node, new_node, similarity);
        }
    }

    /* W6: emit KG consolidation transfer event (new node path) */
    memory_kg_emit_consolidation_transfer(
        memory_kg_events_get_registered_brain(),
        engram_id, new_node->id,
        new_node->consolidation_strength);

    return new_node->id;
}


uint32_t systems_consolidation_find_similar(
    const systems_consolidation_system_t* system,
    const float* query_features,
    uint32_t feature_dim,
    uint32_t max_results,
    uint64_t* results_out,
    float* similarities_out)
{
    // WHAT: Guard against invalid input
    if (!system || !query_features || feature_dim == 0 || max_results == 0 ||
        !results_out || !similarities_out) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_heartbeat("consolidatio_systems_consolidatio", 0.0f);

    // WHAT: Compute similarities for all nodes
    uint32_t results_found = 0;

    for (uint32_t i = 0; i < system->node_count && results_found < max_results; i++) {
        cortical_memory_node_t* node = system->cortical_nodes[i];

        // WHAT: Skip if dimensionality mismatch
        if (node->feature_dim != feature_dim) {
            continue;
        }

        // WHAT: Compute similarity
        float similarity = nimcp_vector_cosine_similarity(
            query_features,
            node->features,
            feature_dim
        );

        // WHAT: Insert into results (simple linear insertion)
        // TODO: Use heap for large result sets
        uint32_t insert_pos = results_found;
        for (uint32_t j = 0; j < results_found; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && results_found > 256) {
                systems_consolidation_heartbeat("consolidatio_loop",
                                 (float)(j + 1) / (float)results_found);
            }

            if (similarity > similarities_out[j]) {
                insert_pos = j;
                break;
            }
        }

        // WHAT: Shift results if needed
        if (insert_pos < max_results) {
            for (uint32_t j = results_found; j > insert_pos && j > 0; j--) {
                if (j < max_results) {
                    results_out[j] = results_out[j - 1];
                    similarities_out[j] = similarities_out[j - 1];
                }
            }

            results_out[insert_pos] = node->id;
            similarities_out[insert_pos] = similarity;

            if (results_found < max_results) {
                results_found++;
            }
        }
    }

    return results_found;
}


//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

/**
 * @brief Query self-knowledge from knowledge graph
 * WHAT: Retrieve module's self-awareness information from KG
 * WHY:  Enable introspection about module capabilities and connections
 * HOW:  Query KG reader for entity and relations
 */
int systems_consolidation_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_heartbeat("consolidatio_systems_consolidatio", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Systems_Consolidation_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                systems_consolidation_heartbeat("consolidatio_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Systems consolidation self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Systems_Consolidation_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Systems_Consolidation_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}


/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int systems_consolidation_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "systems_consolidation_training_begin: NULL argument");
        return -1;
    }
    systems_consolidation_heartbeat_instance(NULL, "systems_consolidation_training_begin", 0.0f);
    return 0;
}


int systems_consolidation_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "systems_consolidation_training_end: NULL argument");
        return -1;
    }
    systems_consolidation_heartbeat_instance(NULL, "systems_consolidation_training_end", 1.0f);
    return 0;
}
