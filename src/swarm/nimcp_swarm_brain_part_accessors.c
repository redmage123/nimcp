// nimcp_swarm_brain_part_accessors.c - accessors functions
// Part of nimcp_swarm_brain.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_swarm_brain.c


//=============================================================================
// Public API Implementation
//=============================================================================

swarm_brain_config_t swarm_brain_default_config(void) {
    swarm_brain_config_t config;
    memset(&config, 0, sizeof(config));

    config.drone_id = 1;
    strncpy(config.swarm_name, "default_swarm", SWARM_MAX_NAME_LEN - 1);
    config.heartbeat_ms = SWARM_DEFAULT_HEARTBEAT_MS;
    config.sync_ms = SWARM_DEFAULT_SYNC_MS;
    config.vote_timeout_ms = SWARM_DEFAULT_VOTE_TIMEOUT_MS;
    config.coherence_threshold = SWARM_DEFAULT_COHERENCE_THRESHOLD;
    config.critical_mass = SWARM_DEFAULT_CRITICAL_MASS;
    config.workspace_size = SWARM_DEFAULT_WORKSPACE_SIZE;
    config.broadcast_threshold = SWARM_DEFAULT_BROADCAST_THRESHOLD;
    config.neuromod_diffusion = SWARM_DEFAULT_NEUROMOD_DIFFUSION;
    config.enable_reward_sharing = true;
    config.enable_bio_async = true;

    return config;
}


swarm_emergence_tier_t swarm_brain_get_emergence_tier(const swarm_brain_t* swarm) {
    if (!swarm || !swarm->emergence) return SWARM_TIER_INDIVIDUAL;
    return swarm->emergence->current_tier;
}


const workspace_entry_t* swarm_brain_get_workspace(const swarm_brain_t* swarm, uint32_t* workspace_size) {
    if (!swarm || !swarm->workspace || !workspace_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_get_workspace: required parameter is NULL (swarm, swarm->workspace, workspace_size)");
        return NULL;
    }

    *workspace_size = swarm->workspace->size;
    return swarm->workspace->entries;
}


const swarm_peer_info_t* swarm_brain_get_peers(const swarm_brain_t* swarm, uint32_t* peer_count) {
    if (!swarm || !peer_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_get_peers: required parameter is NULL (swarm, peer_count)");
        return NULL;
    }

    *peer_count = swarm->peer_count;
    return swarm->peers;
}


bool swarm_brain_get_stats(const swarm_brain_t* swarm, swarm_stats_t* stats) {
    if (!swarm || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_get_stats: required parameter is NULL (swarm, stats)");
        return false;
    }

    nimcp_platform_mutex_lock(swarm->stats_lock);
    *stats = swarm->stats;
    nimcp_platform_mutex_unlock(swarm->stats_lock);

    return true;
}


brain_t swarm_brain_get_local_brain(swarm_brain_t* swarm) {
    if (!swarm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm is NULL");

        return NULL;

    }
    return swarm->local_brain;
}


bool swarm_brain_is_operational(const swarm_brain_t* swarm) {
    return swarm && swarm->operational;
}


/**
 * @brief Get swarm capabilities from knowledge graph
 *
 * @param kg Knowledge graph reader instance
 * @return Capability description string or NULL
 */
const char* swarm_brain_get_capabilities(kg_reader_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return kg_reader_get_module_capabilities(kg, "Swarm_Brain");
}


/**
 * @brief Get swarm integrations from knowledge graph
 *
 * @param kg Knowledge graph reader instance
 * @return Relation list showing integrations (caller must free)
 */
kg_relation_list_t* swarm_brain_get_integrations(kg_reader_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return kg_reader_get_module_integrations(kg, "Swarm_Brain");
}
