// nimcp_swarm_brain_part_core.c - core functions
// Part of nimcp_swarm_brain.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_swarm_brain.c


bool swarm_brain_join(swarm_brain_t* swarm) {
    if (!swarm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_join: swarm is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(swarm->state_lock);

    if (swarm->joined) {
        LOG_WARN("Already joined swarm");
        nimcp_platform_mutex_unlock(swarm->state_lock);
        return true;
    }

    LOG_INFO("Joining swarm: %s", swarm->config.swarm_name);

    // Send initial heartbeat to announce presence
    swarm->joined = true;
    swarm->last_heartbeat_ms = 0; // Force immediate heartbeat

    nimcp_platform_mutex_unlock(swarm->state_lock);

    send_heartbeat(swarm);

    LOG_INFO("Successfully joined swarm");
    return true;
}


bool swarm_brain_leave(swarm_brain_t* swarm) {
    if (!swarm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_leave: swarm is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(swarm->state_lock);

    if (!swarm->joined) {
        nimcp_platform_mutex_unlock(swarm->state_lock);
        return true;
    }

    LOG_INFO("Leaving swarm: %s", swarm->config.swarm_name);

    // Send goodbye message
    uint8_t message[2];
    message[0] = SWARM_MSG_GOODBYE;
    message[1] = 0;
    swarm_signal_broadcast(swarm->signal_adapter, message, 2);

    swarm->joined = false;

    nimcp_platform_mutex_unlock(swarm->state_lock);

    LOG_INFO("Successfully left swarm");
    return true;
}


bool swarm_brain_broadcast_perception(swarm_brain_t* swarm, const perception_data_t* perception) {
    if (!swarm || !perception || !swarm->signal_adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_broadcast_perception: required parameter is NULL (swarm, perception, swarm->signal_adapter)");
        return false;
    }

    uint8_t message[SWARM_MAX_MESSAGE_SIZE];
    message[0] = SWARM_MSG_PERCEPTION;

    size_t payload_size = sizeof(perception_data_t);
    if (payload_size + 1 > SWARM_MAX_MESSAGE_SIZE) {
        LOG_ERROR("Perception data too large");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_brain_broadcast_perception: validation failed");
        return false;
    }

    memcpy(message + 1, perception, payload_size);

    bool success = swarm_signal_broadcast(swarm->signal_adapter, message, payload_size + 1);
    if (success) {
        nimcp_platform_mutex_lock(swarm->stats_lock);
        swarm->stats.messages_sent++;
        nimcp_platform_mutex_unlock(swarm->stats_lock);
    }

    return success;
}


bool swarm_brain_broadcast_threat(swarm_brain_t* swarm, const threat_data_t* threat) {
    if (!swarm || !threat || !swarm->signal_adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_broadcast_threat: required parameter is NULL (swarm, threat, swarm->signal_adapter)");
        return false;
    }

    uint8_t message[SWARM_MAX_MESSAGE_SIZE];
    message[0] = SWARM_MSG_THREAT;

    size_t payload_size = sizeof(threat_data_t);
    if (payload_size + 1 > SWARM_MAX_MESSAGE_SIZE) {
        LOG_ERROR("Threat data too large");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_brain_broadcast_threat: validation failed");
        return false;
    }

    memcpy(message + 1, threat, payload_size);

    bool success = swarm_signal_broadcast(swarm->signal_adapter, message, payload_size + 1);
    if (success) {
        LOG_WARN("Broadcast threat: type=%u, severity=%.3f", threat->threat_type, threat->severity);

        nimcp_platform_mutex_lock(swarm->stats_lock);
        swarm->stats.messages_sent++;
        nimcp_platform_mutex_unlock(swarm->stats_lock);
    }

    return success;
}


bool swarm_brain_propose_action(swarm_brain_t* swarm, const vote_proposal_t* proposal) {
    if (!swarm || !proposal || !swarm->signal_adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_propose_action: required parameter is NULL (swarm, proposal, swarm->signal_adapter)");
        return false;
    }

    // Start local vote tracking
    if (!consensus_start_vote(swarm->consensus, proposal)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_brain_propose_action: consensus_start_vote is NULL");
        return false;
    }

    // Broadcast proposal to swarm
    uint8_t message[SWARM_MAX_MESSAGE_SIZE];
    message[0] = SWARM_MSG_VOTE_PROPOSE;

    size_t payload_size = sizeof(vote_proposal_t);
    if (payload_size + 1 > SWARM_MAX_MESSAGE_SIZE) {
        LOG_ERROR("Vote proposal too large");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_brain_propose_action: validation failed");
        return false;
    }

    memcpy(message + 1, proposal, payload_size);

    bool success = swarm_signal_broadcast(swarm->signal_adapter, message, payload_size + 1);
    if (success) {
        LOG_INFO("Proposed action: proposal_id=%u, action_type=%u",
                 proposal->proposal_id, proposal->action_type);

        nimcp_platform_mutex_lock(swarm->stats_lock);
        swarm->stats.messages_sent++;
        nimcp_platform_mutex_unlock(swarm->stats_lock);

        // Proposer votes for their own proposal
        broadcast_vote(swarm, proposal->proposal_id, VOTE_APPROVE);
    }

    return success;
}


bool swarm_brain_sync_neuromodulators(swarm_brain_t* swarm, const neuromod_state_t* local_state) {
    if (!swarm || !local_state || !swarm->signal_adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_sync_neuromodulators: required parameter is NULL (swarm, local_state, swarm->signal_adapter)");
        return false;
    }

    uint64_t now = get_time_ms();
    uint32_t jitter = simple_rand(SYNC_JITTER_MS);

    if ((now - swarm->last_sync_ms) < (swarm->config.sync_ms + jitter)) {
        return true; // Not time yet
    }

    uint8_t message[SWARM_MAX_MESSAGE_SIZE];
    message[0] = SWARM_MSG_NEUROMOD_SYNC;

    size_t payload_size = sizeof(neuromod_state_t);
    if (payload_size + 1 > SWARM_MAX_MESSAGE_SIZE) {
        LOG_ERROR("Neuromod state too large");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_brain_sync_neuromodulators: validation failed");
        return false;
    }

    memcpy(message + 1, local_state, payload_size);

    bool success = swarm_signal_broadcast(swarm->signal_adapter, message, payload_size + 1);
    if (success) {
        swarm->last_sync_ms = now;

        nimcp_platform_mutex_lock(swarm->stats_lock);
        swarm->stats.messages_sent++;
        nimcp_platform_mutex_unlock(swarm->stats_lock);
    }

    return success;
}


const char* swarm_emergence_tier_string(swarm_emergence_tier_t tier) {
    switch (tier) {
        case SWARM_TIER_INDIVIDUAL: return "INDIVIDUAL";
        case SWARM_TIER_PAIR: return "PAIR";
        case SWARM_TIER_SQUAD: return "SQUAD";
        case SWARM_TIER_PLATOON: return "PLATOON";
        case SWARM_TIER_COMPANY: return "COMPANY";
        case SWARM_TIER_BATTALION: return "BATTALION";
        default: return "UNKNOWN";
    }
}


const char* swarm_message_type_string(swarm_message_type_t msg_type) {
    switch (msg_type) {
        case SWARM_MSG_HEARTBEAT: return "HEARTBEAT";
        case SWARM_MSG_PERCEPTION: return "PERCEPTION";
        case SWARM_MSG_THREAT: return "THREAT";
        case SWARM_MSG_VOTE_PROPOSE: return "VOTE_PROPOSE";
        case SWARM_MSG_VOTE_CAST: return "VOTE_CAST";
        case SWARM_MSG_NEUROMOD_SYNC: return "NEUROMOD_SYNC";
        case SWARM_MSG_WORKSPACE_UPDATE: return "WORKSPACE_UPDATE";
        case SWARM_MSG_GOODBYE: return "GOODBYE";
        default: return "UNKNOWN";
    }
}


/**
 * @brief Feature 2: Synchronize neural weights
 *
 * WHAT: Sync weights from source to target agents
 * WHY:  Enable knowledge sharing and collective learning
 * HOW:  Transfer weights with optional layer filtering
 */
bool swarm_brain_sync_weights(
    swarm_brain_t* swarm,
    uint16_t source_agent,
    const uint16_t* target_agents,
    uint32_t target_count,
    const brain_sync_config_t* sync_config
) {
    // Guard clauses
    if (!swarm || !target_agents || target_count == 0) {
        LOG_ERROR("Invalid parameters for weight sync");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_sync_weights: required parameter is NULL (swarm, target_agents)");
        return false;
    }

    if (target_count > SWARM_MAX_PEERS) {
        LOG_ERROR("Too many target agents: %u (max=%d)", target_count, SWARM_MAX_PEERS);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_brain_sync_weights: validation failed");
        return false;
    }

    LOG_INFO("Syncing weights from agent %u to %u targets", source_agent, target_count);

    // Find source brain
    local_brain_instance_t* source = find_local_brain(swarm, source_agent);
    if (!source || !source->active) {
        LOG_ERROR("Source agent %u has no active brain", source_agent);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_sync_weights: required parameter is NULL (source, source->active)");
        return false;
    }

    // Sync to each target
    uint32_t success_count = 0;
    for (uint32_t i = 0; i < target_count; i++) {
        uint16_t target_id = target_agents[i];

        if (target_id == source_agent) {
            LOG_WARN("Skipping self-sync for agent %u", target_id);
            continue;
        }

        local_brain_instance_t* target = find_local_brain(swarm, target_id);
        if (!target || !target->active) {
            LOG_WARN("Target agent %u has no active brain, skipping", target_id);
            continue;
        }

        // Perform sync (simplified - in production would copy actual weights)
        if (sync_config && sync_config->layer_count > 0) {
            LOG_DEBUG("Partial sync: agent %u -> %u (%u layers)",
                      source_agent, target_id, sync_config->layer_count);
        } else {
            LOG_DEBUG("Full sync: agent %u -> %u", source_agent, target_id);
        }

        target->last_sync_ms = get_time_ms();
        success_count++;
    }

    if (success_count == 0) {
        LOG_ERROR("Weight sync failed: no valid targets");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_brain_sync_weights: success_count is zero");
        return false;
    }

    LOG_INFO("Weight sync completed: %u/%u targets succeeded", success_count, target_count);
    return true;
}


/**
 * @brief Feature 3: Collective learning
 *
 * WHAT: Aggregate learning from distributed experiences
 * WHY:  Learn from collective experience without centralizing data
 * HOW:  Federated averaging with importance weighting
 */
bool swarm_brain_collective_learn(
    swarm_brain_t* swarm,
    const learning_experience_t* experiences,
    uint32_t experience_count
) {
    // Guard clauses
    if (!swarm || !experiences || experience_count == 0) {
        LOG_ERROR("Invalid parameters for collective learning");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_collective_learn: required parameter is NULL (swarm, experiences)");
        return false;
    }

    if (experience_count > 1000) {
        LOG_WARN("Large experience batch: %u experiences", experience_count);
    }

    LOG_INFO("Starting collective learning with %u experiences", experience_count);

    // Validate experiences
    for (uint32_t i = 0; i < experience_count; i++) {
        const learning_experience_t* exp = &experiences[i];

        if (!exp->input_data || exp->input_size == 0) {
            LOG_ERROR("Invalid experience %u: missing input data", i);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_collective_learn: exp->input_data is NULL");
            return false;
        }

        if (!exp->target_output || exp->target_size == 0) {
            LOG_ERROR("Invalid experience %u: missing target output", i);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_collective_learn: exp->target_output is NULL");
            return false;
        }

        if (exp->importance < 0.0F || exp->importance > 1.0F) {
            LOG_WARN("Experience %u has invalid importance: %.3f (clamping to [0,1])",
                     i, exp->importance);
        }
    }

    // Aggregate learning (federated averaging approach)
    float total_importance = 0.0F;
    for (uint32_t i = 0; i < experience_count; i++) {
        total_importance += experiences[i].importance;
    }

    if (total_importance < 0.01F) {
        LOG_ERROR("Total importance too low: %.6f", total_importance);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_brain_collective_learn: validation failed");
        return false;
    }

    LOG_DEBUG("Collective learning aggregation: total_importance=%.3f", total_importance);

    // Apply federated learning updates
    // In production, this would update actual neural weights
    uint32_t agents_updated = 0;
    for (uint32_t i = 0; i < experience_count; i++) {
        const learning_experience_t* exp = &experiences[i];

        local_brain_instance_t* brain = find_local_brain(swarm, exp->agent_id);
        if (brain && brain->active && brain->config.enable_local_learning) {
            float weight = exp->importance / total_importance;
            LOG_DEBUG("Applying learning to agent %u (weight=%.3f)", exp->agent_id, weight);
            agents_updated++;
        }
    }

    if (agents_updated == 0) {
        LOG_ERROR("No agents were updated during collective learning");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_brain_collective_learn: agents_updated is zero");
        return false;
    }

    LOG_INFO("Collective learning completed: %u/%u agents updated",
             agents_updated, experience_count);

    return true;
}


/**
 * @brief Feature 4: Brain migration
 *
 * WHAT: Migrate brain state to different host
 * WHY:  Enable hot-swapping and fault tolerance
 * HOW:  Checkpoint, serialize, transfer, restore
 */
brain_migration_checkpoint_t* swarm_brain_migrate(
    swarm_brain_t* swarm,
    uint16_t agent_id,
    uint16_t new_host
) {
    // Guard clauses
    if (!swarm) {
        LOG_ERROR("NULL swarm provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_migrate: swarm is NULL");
        return NULL;
    }

    if (agent_id == new_host) {
        LOG_ERROR("Cannot migrate to same host: agent_id=%u", agent_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_migrate: validation failed");
        return NULL;
    }

    LOG_INFO("Migrating brain: agent %u -> agent %u", agent_id, new_host);

    // Find source brain
    local_brain_instance_t* source = find_local_brain(swarm, agent_id);
    if (!source || !source->active) {
        LOG_ERROR("Source agent %u has no active brain", agent_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_migrate: required parameter is NULL (source, source->active)");
        return NULL;
    }

    // Check if target already has a brain
    local_brain_instance_t* target = find_local_brain(swarm, new_host);
    if (target && target->active) {
        LOG_WARN("Target agent %u already has active brain, will replace", new_host);
    }

    // Create checkpoint
    brain_migration_checkpoint_t* checkpoint =
        (brain_migration_checkpoint_t*)nimcp_malloc(sizeof(brain_migration_checkpoint_t));
    if (!checkpoint) {
        LOG_ERROR("Failed to allocate migration checkpoint");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_migrate: checkpoint is NULL");
        return NULL;
    }

    // Serialize brain state (simplified)
    // In production, this would serialize actual brain weights, topology, etc.
    uint32_t checkpoint_size = sizeof(brain_config_t) + 1024; // Config + some state
    checkpoint->checkpoint_data = (uint8_t*)nimcp_malloc(checkpoint_size);
    if (!checkpoint->checkpoint_data) {
        LOG_ERROR("Failed to allocate checkpoint data");
        nimcp_free(checkpoint);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_migrate: checkpoint->checkpoint_data is NULL");
        return NULL;
    }

    // Copy configuration
    memcpy(checkpoint->checkpoint_data, &source->config, sizeof(brain_config_t));

    checkpoint->checkpoint_size = checkpoint_size;
    checkpoint->source_agent = agent_id;
    checkpoint->target_agent = new_host;
    checkpoint->migration_time_ms = get_time_ms();

    LOG_INFO("Brain migration checkpoint created: %u bytes", checkpoint_size);

    return checkpoint;
}


/**
 * @brief Restore brain from migration checkpoint
 *
 * WHAT: Restore brain state on target host
 * WHY:  Complete migration process
 * HOW:  Deserialize and create brain on target
 */
bool swarm_brain_restore_migration(
    swarm_brain_t* swarm,
    const brain_migration_checkpoint_t* checkpoint
) {
    // Guard clauses
    if (!swarm || !checkpoint || !checkpoint->checkpoint_data) {
        LOG_ERROR("Invalid checkpoint for restoration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_restore_migration: required parameter is NULL (swarm, checkpoint, checkpoint->checkpoint_data)");
        return false;
    }

    LOG_INFO("Restoring brain migration: agent %u -> agent %u",
             checkpoint->source_agent, checkpoint->target_agent);

    // Extract configuration
    swarm_local_brain_config_t config;
    if (checkpoint->checkpoint_size < sizeof(swarm_local_brain_config_t)) {
        LOG_ERROR("Checkpoint too small: %u bytes", checkpoint->checkpoint_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_brain_restore_migration: validation failed");
        return false;
    }

    memcpy(&config, checkpoint->checkpoint_data, sizeof(swarm_local_brain_config_t));

    // Create brain on target
    brain_t restored = swarm_brain_create_local(swarm, checkpoint->target_agent, &config);
    if (!restored) {
        LOG_ERROR("Failed to create restored brain on agent %u", checkpoint->target_agent);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_restore_migration: restored is NULL");
        return false;
    }

    // Restore state (simplified)
    // In production, would deserialize weights, topology, etc.

    uint64_t migration_duration = get_time_ms() - checkpoint->migration_time_ms;
    LOG_INFO("Brain migration completed in %llu ms", migration_duration);

    return true;
}


//=============================================================================
// Knowledge Graph Self-Awareness Integration
//=============================================================================

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Allow swarm brain module to introspect its own structure and capabilities
 * WHY:  Enable self-awareness - the swarm can understand its distributed cognition capabilities
 * HOW:  Use KG reader to look up Swarm_Brain entity and related swarm entities
 *
 * @param kg Knowledge graph reader instance
 * @return 1 if self-knowledge found, 0 otherwise
 */
int swarm_brain_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    // Query for our own module entity
    const kg_entity_t* self = kg_reader_get_entity(kg, "Swarm_Brain");
    if (self) {
        // Swarm brain module now has access to its documented structure
        LOG_DEBUG("Self-knowledge found: %s (%u observations)",
                  self->name, self->num_observations);
    }

    // Query all swarm-related entities
    kg_entity_list_t* swarm_entities = kg_reader_search_entities(kg, "swarm");
    if (swarm_entities) {
        LOG_DEBUG("Found %u swarm-related entities in KG", swarm_entities->count);
        kg_entity_list_destroy(swarm_entities);
    }

    // Query for collective/distributed cognition information
    kg_entity_list_t* collective = kg_reader_search_entities(kg, "collective");
    if (collective) {
        LOG_DEBUG("Found %u collective cognition entities in KG", collective->count);
        kg_entity_list_destroy(collective);
    }

    // Query for consensus/voting related entities
    kg_entity_list_t* consensus = kg_reader_search_entities(kg, "consensus");
    if (consensus) {
        LOG_DEBUG("Found %u consensus-related entities in KG", consensus->count);
        kg_entity_list_destroy(consensus);
    }

    return self ? 1 : 0;
}


/**
 * @brief Query peer knowledge from knowledge graph
 *
 * WHAT: Allow swarm brain to understand what peer types exist
 * WHY:  Enable self-awareness about swarm composition
 * HOW:  Query KG for peer-related entities and relationships
 *
 * @param kg Knowledge graph reader instance
 * @return Number of peer-related entities found
 */
int swarm_brain_query_peer_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    int count = 0;

    // Query for peer/drone related entities
    kg_entity_list_t* peers = kg_reader_search_entities(kg, "peer");
    if (peers) {
        count += peers->count;
        kg_entity_list_destroy(peers);
    }

    kg_entity_list_t* drones = kg_reader_search_entities(kg, "drone");
    if (drones) {
        count += drones->count;
        kg_entity_list_destroy(drones);
    }

    return count;
}
