// nimcp_swarm_brain_part_lifecycle.c - lifecycle functions
// Part of nimcp_swarm_brain.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_swarm_brain.c


swarm_brain_t* swarm_brain_create(const swarm_brain_config_t* config) {
    if (!config) {
        LOG_ERROR("NULL configuration provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_create: config is NULL");
        return NULL;
    }

    LOG_INFO("Creating swarm brain: drone_id=%u, swarm=%s",
             config->drone_id, config->swarm_name);

    swarm_brain_t* swarm = (swarm_brain_t*)nimcp_calloc(1, sizeof(swarm_brain_t));
    if (!swarm) {
        LOG_ERROR("Failed to allocate swarm brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_create: swarm is NULL");
        return NULL;
    }

    // Copy configuration
    swarm->config = *config;
    swarm->creation_time_ms = get_time_ms();

    // Create and initialize mutexes
    swarm->state_lock = (nimcp_mutex_t*)nimcp_malloc(sizeof(nimcp_mutex_t));
    swarm->peer_lock = (nimcp_mutex_t*)nimcp_malloc(sizeof(nimcp_mutex_t));
    swarm->stats_lock = (nimcp_mutex_t*)nimcp_malloc(sizeof(nimcp_mutex_t));

    if (!swarm->state_lock || !swarm->peer_lock || !swarm->stats_lock) {
        LOG_ERROR("Failed to allocate mutexes");
        swarm_brain_destroy(swarm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_create: required parameter is NULL (swarm->state_lock, swarm->peer_lock, swarm->stats_lock)");
        return NULL;
    }

    if (nimcp_platform_mutex_init(swarm->state_lock, false) != 0 ||
        nimcp_platform_mutex_init(swarm->peer_lock, false) != 0 ||
        nimcp_platform_mutex_init(swarm->stats_lock, false) != 0) {
        LOG_ERROR("Failed to initialize mutexes");
        swarm_brain_destroy(swarm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "swarm_brain_create: validation failed");
        return NULL;
    }

    // Create signal adapter (using simulation mode for now)
    swarm_signal_config_t signal_config = {
        .radio_type = SWARM_RADIO_SIMULATION,
        .max_packet_size = SWARM_MAX_MESSAGE_SIZE,
        .retry_count = 3,
        .timeout_ms = NIMCP_FAST_HEARTBEAT_MS,
        .node_id = config->drone_id  // Use drone_id as network node identifier
    };
    swarm->signal_adapter = swarm_signal_adapter_create(&signal_config);
    if (!swarm->signal_adapter) {
        LOG_ERROR("Failed to create signal adapter");
        swarm_brain_destroy(swarm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_create: swarm->signal_adapter is NULL");
        return NULL;
    }

    // Create collective workspace
    swarm->workspace = create_workspace(config->workspace_size);
    if (!swarm->workspace) {
        LOG_ERROR("Failed to create workspace");
        swarm_brain_destroy(swarm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_create: swarm->workspace is NULL");
        return NULL;
    }

    // Create emergence context
    swarm->emergence = create_emergence_context();
    if (!swarm->emergence) {
        LOG_ERROR("Failed to create emergence context");
        swarm_brain_destroy(swarm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_create: swarm->emergence is NULL");
        return NULL;
    }

    // Create consensus context
    swarm->consensus = create_consensus_context();
    if (!swarm->consensus) {
        LOG_ERROR("Failed to create consensus context");
        swarm_brain_destroy(swarm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_create: swarm->consensus is NULL");
        return NULL;
    }

    // Create constrained local brain for drone
    char brain_name[NIMCP_ID_BUFFER_SIZE];
    snprintf(brain_name, sizeof(brain_name), "swarm_drone_%u", config->drone_id);
    swarm->local_brain = brain_create(
        brain_name,
        BRAIN_SIZE_TINY,         // Smallest brain size for drones
        BRAIN_TASK_CLASSIFICATION,
        10,                       // 10 inputs (sensors)
        5                         // 5 outputs (actions)
    );
    if (!swarm->local_brain) {
        LOG_WARN("Failed to create local brain - swarm will operate without local processing");
        // Not fatal - swarm can still coordinate without local brain
    }

    // Setup bio-async if enabled
    if (config->enable_bio_async) {
        swarm->bio_async_enabled = true;
        // Bio-async context setup handled by brain initialization
    }

    swarm->operational = true;

    LOG_INFO("Swarm brain created successfully");
    return swarm;
}


void swarm_brain_destroy(swarm_brain_t* swarm) {
    if (!swarm) return;

    LOG_INFO("Destroying swarm brain: drone_id=%u", swarm->config.drone_id);

    // Leave swarm if joined
    if (swarm->joined) {
        swarm_brain_leave(swarm);
    }

    // Cleanup components
    if (swarm->consensus) destroy_consensus_context(swarm->consensus);
    if (swarm->emergence) destroy_emergence_context(swarm->emergence);
    if (swarm->workspace) destroy_workspace(swarm->workspace);
    if (swarm->signal_adapter) swarm_signal_adapter_destroy(swarm->signal_adapter);

    // Destroy local brain if created
    if (swarm->local_brain) {
        brain_destroy(swarm->local_brain);
        swarm->local_brain = NULL;
    }

    // Destroy mutexes
    if (swarm->state_lock) {
        nimcp_platform_mutex_destroy(swarm->state_lock);
        nimcp_free(swarm->state_lock);
    }
    if (swarm->peer_lock) {
        nimcp_platform_mutex_destroy(swarm->peer_lock);
        nimcp_free(swarm->peer_lock);
    }
    if (swarm->stats_lock) {
        nimcp_platform_mutex_destroy(swarm->stats_lock);
        nimcp_free(swarm->stats_lock);
    }

    nimcp_free(swarm);
    LOG_INFO("Swarm brain destroyed");
}


bool swarm_brain_reset_stats(swarm_brain_t* swarm) {
    if (!swarm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_reset_stats: swarm is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(swarm->stats_lock);
    memset(&swarm->stats, 0, sizeof(swarm_stats_t));
    nimcp_platform_mutex_unlock(swarm->stats_lock);

    return true;
}


/**
 * @brief Feature 1: Create local brain instance
 *
 * WHAT: Creates lightweight brain instance for swarm agent
 * WHY:  Enable distributed cognition with local processing
 * HOW:  Allocates brain with shared structures, local state
 */
brain_t swarm_brain_create_local(
    swarm_brain_t* swarm,
    uint16_t agent_id,
    const swarm_local_brain_config_t* config
) {
    // Guard clauses
    if (!swarm || !config) {
        LOG_ERROR("NULL swarm or config provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_create_local: required parameter is NULL (swarm, config)");
        return NULL;
    }

    if (config->neuron_count == 0 || config->synapse_count == 0) {
        LOG_ERROR("Invalid brain configuration: neuron_count=%u, synapse_count=%u",
                  config->neuron_count, config->synapse_count);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_brain_create_local: config->neuron_count is zero");
        return NULL;
    }

    LOG_INFO("Creating local brain for agent %u: neurons=%u, synapses=%u",
             agent_id, config->neuron_count, config->synapse_count);

    // Check if brain already exists
    local_brain_instance_t* existing = find_local_brain(swarm, agent_id);
    if (existing) {
        LOG_WARN("Local brain already exists for agent %u", agent_id);
        return existing->brain;
    }

    // Find free slot
    local_brain_instance_t* brains = get_local_brains(swarm);
    local_brain_instance_t* slot = NULL;
    for (uint32_t i = 0; i < MAX_LOCAL_BRAINS; i++) {
        if (!brains[i].active) {
            slot = &brains[i];
            break;
        }
    }

    if (!slot) {
        LOG_ERROR("No free local brain slots available (max=%d)", MAX_LOCAL_BRAINS);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "swarm_brain_create_local: no free local brain slots available");
        return NULL;
    }

    // Create brain using the proper brain API
    // Use brain_create with configuration derived from the local config
    char task_name[NIMCP_ID_BUFFER_SIZE];
    snprintf(task_name, sizeof(task_name), "swarm_agent_%u", agent_id);

    brain_t brain = brain_create(
        task_name,
        BRAIN_SIZE_TINY,  // Use small brain for swarm agents
        BRAIN_TASK_CLASSIFICATION,
        config->neuron_count > 0 ? config->neuron_count : 10,  // inputs
        config->synapse_count > 0 ? config->synapse_count / 10 : 2  // outputs
    );

    if (!brain) {
        LOG_ERROR("Failed to create local brain for agent %u", agent_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_create_local: brain is NULL");
        return NULL;
    }

    // Initialize slot
    slot->brain = brain;
    slot->agent_id = agent_id;
    slot->config = *config;
    slot->active = true;
    slot->creation_time_ms = get_time_ms();
    slot->last_sync_ms = 0;

    LOG_DEBUG("Local brain created successfully for agent %u", agent_id);

    return brain;
}


/**
 * @brief Destroy migration checkpoint
 *
 * WHAT: Free migration checkpoint resources
 * WHY:  Prevent memory leaks
 * HOW:  Free data and structure
 */
void swarm_brain_migration_checkpoint_destroy(
    brain_migration_checkpoint_t* checkpoint
) {
    if (!checkpoint) return;

    if (checkpoint->checkpoint_data) {
        nimcp_free(checkpoint->checkpoint_data);
    }

    nimcp_free(checkpoint);
    LOG_DEBUG("Migration checkpoint destroyed");
}
