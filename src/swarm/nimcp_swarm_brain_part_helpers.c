// nimcp_swarm_brain_part_helpers.c - helpers functions
// Part of nimcp_swarm_brain.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_swarm_brain.c


//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_time_ms(void) {
    return nimcp_time_get_ms();
}


/**
 * @brief Simple random number generator (0-max) - thread-safe
 *
 * Uses thread-local storage for the seed to prevent race conditions
 * when multiple threads call this function concurrently.
 */
static uint32_t simple_rand(uint32_t max) {
    static __thread uint32_t seed = 12345;
    seed = (1103515245 * seed + 12345) & 0x7fffffff;
    return seed % (max + 1);
}


/**
 * @brief Find peer by ID
 */
static swarm_peer_info_t* find_peer(swarm_brain_t* swarm, uint16_t drone_id) {
    if (!swarm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < swarm->peer_count; i++) {
        if (swarm->peers[i].active && swarm->peers[i].drone_id == drone_id) {
            return &swarm->peers[i];
        }
    }
    return NULL;  /* Not found - normal search miss */
}


/**
 * @brief Add or update peer
 */
static bool update_peer(swarm_brain_t* swarm, uint16_t drone_id) {
    if (!swarm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "update_peer: swarm is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(swarm->peer_lock);

    swarm_peer_info_t* peer = find_peer(swarm, drone_id);
    if (peer) {
        // Update existing peer
        peer->last_seen_ms = get_time_ms();
        peer->message_count++;
    } else {
        // Add new peer if space available
        if (swarm->peer_count < SWARM_MAX_PEERS) {
            peer = &swarm->peers[swarm->peer_count];
            peer->drone_id = drone_id;
            peer->last_seen_ms = get_time_ms();
            peer->coherence = 0.5F;
            peer->message_count = 1;
            peer->active = true;
            swarm->peer_count++;

            LOG_INFO("New peer joined: drone_id=%u, total_peers=%u",
                     drone_id, swarm->peer_count);
        } else {
            LOG_WARN("Peer list full, cannot add drone_id=%u", drone_id);
            nimcp_platform_mutex_unlock(swarm->peer_lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "update_peer: operation failed");
            return false;
        }
    }

    nimcp_platform_mutex_unlock(swarm->peer_lock);
    return true;
}


/**
 * @brief Remove inactive peers
 */
static void remove_inactive_peers(swarm_brain_t* swarm) {
    if (!swarm) return;

    uint64_t now = get_time_ms();
    nimcp_platform_mutex_lock(swarm->peer_lock);

    uint32_t removed = 0;
    for (uint32_t i = 0; i < swarm->peer_count; ) {
        if (swarm->peers[i].active &&
            (now - swarm->peers[i].last_seen_ms) > PEER_TIMEOUT_MS) {
            LOG_INFO("Peer timeout: drone_id=%u", swarm->peers[i].drone_id);
            swarm->peers[i].active = false;
            removed++;

            // Compact array by moving last element here
            if (i < swarm->peer_count - 1) {
                swarm->peers[i] = swarm->peers[swarm->peer_count - 1];
            }
            swarm->peer_count--;
        } else {
            i++;
        }
    }

    if (removed > 0) {
        LOG_DEBUG("Removed %u inactive peers, remaining=%u", removed, swarm->peer_count);
    }

    nimcp_platform_mutex_unlock(swarm->peer_lock);
}


//=============================================================================
// Collective Workspace Functions
//=============================================================================

/**
 * @brief Create collective workspace
 */
static collective_workspace_t* create_workspace(uint32_t size) {
    collective_workspace_t* ws = (collective_workspace_t*)nimcp_malloc(sizeof(collective_workspace_t));
    if (!ws) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_workspace: ws allocation failed");

        return NULL;

    }

    ws->entries = (workspace_entry_t*)nimcp_calloc(size, sizeof(workspace_entry_t));
    if (!ws->entries) {
        nimcp_free(ws);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_workspace: ws->entries is NULL");
        return NULL;
    }

    ws->size = size;
    ws->last_update_ms = get_time_ms();
    ws->coherence = 0.0F;

    // Allocate and initialize mutex
    ws->lock = (nimcp_mutex_t*)nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!ws->lock || nimcp_platform_mutex_init(ws->lock, false) != 0) {
        if (ws->lock) nimcp_free(ws->lock);
        nimcp_free(ws->entries);
        nimcp_free(ws);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "create_workspace: validation failed");
        return NULL;
    }

    return ws;
}


/**
 * @brief Destroy collective workspace
 */
static void destroy_workspace(collective_workspace_t* ws) {
    if (!ws) return;

    if (ws->lock) {
        nimcp_platform_mutex_destroy(ws->lock);
        nimcp_free(ws->lock);
    }
    if (ws->entries) nimcp_free(ws->entries);
    nimcp_free(ws);
}


/**
 * @brief Decay workspace attention over time
 */
static void workspace_decay(collective_workspace_t* ws) {
    if (!ws || !ws->entries) return;

    uint64_t now = get_time_ms();
    nimcp_platform_mutex_lock(ws->lock);

    float dt = (now - ws->last_update_ms) / 1000.0F; // Convert to seconds
    float decay = powf(WORKSPACE_DECAY_RATE, dt);

    for (uint32_t i = 0; i < ws->size; i++) {
        ws->entries[i].attention *= decay;
        if (ws->entries[i].attention < WORKSPACE_MIN_ATTENTION) {
            ws->entries[i].attention = 0.0F;
            ws->entries[i].contributor_count = 0;
        }
    }

    ws->last_update_ms = now;

    nimcp_platform_mutex_unlock(ws->lock);
}


/**
 * @brief Calculate workspace coherence
 */
static float workspace_calculate_coherence(collective_workspace_t* ws) {
    if (!ws || !ws->entries) return 0.0F;

    nimcp_platform_mutex_lock(ws->lock);

    float total_attention = 0.0F;
    float max_attention = 0.0F;

    for (uint32_t i = 0; i < ws->size; i++) {
        total_attention += ws->entries[i].attention;
        if (ws->entries[i].attention > max_attention) {
            max_attention = ws->entries[i].attention;
        }
    }

    // Coherence is ratio of max attention to total (high = focused)
    float coherence = (total_attention > 0.0F) ? (max_attention / total_attention) : 0.0F;
    ws->coherence = coherence;

    nimcp_platform_mutex_unlock(ws->lock);

    return coherence;
}


//=============================================================================
// Emergence Functions
//=============================================================================

/**
 * @brief Create emergence context
 */
static emergence_context_t* create_emergence_context(void) {
    emergence_context_t* ctx = (emergence_context_t*)nimcp_calloc(1, sizeof(emergence_context_t));
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_emergence_context: allocation failed");

        return NULL;

    }

    ctx->current_tier = SWARM_TIER_INDIVIDUAL;
    ctx->tier_enter_time_ms = get_time_ms();
    return ctx;
}


/**
 * @brief Destroy emergence context
 */
static void destroy_emergence_context(emergence_context_t* ctx) {
    if (ctx) nimcp_free(ctx);
}


//=============================================================================
// Consensus Functions
//=============================================================================

/**
 * @brief Create consensus context
 */
static consensus_context_t* create_consensus_context(void) {
    consensus_context_t* ctx = (consensus_context_t*)nimcp_calloc(1, sizeof(consensus_context_t));
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_consensus_context: allocation failed");

        return NULL;

    }

    ctx->lock = (nimcp_mutex_t*)nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!ctx->lock || nimcp_platform_mutex_init(ctx->lock, false) != 0) {
        if (ctx->lock) nimcp_free(ctx->lock);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "create_consensus_context: validation failed");
        return NULL;
    }

    return ctx;
}


/**
 * @brief Destroy consensus context
 */
static void destroy_consensus_context(consensus_context_t* ctx) {
    if (!ctx) return;
    if (ctx->lock) {
        nimcp_platform_mutex_destroy(ctx->lock);
        nimcp_free(ctx->lock);
    }
    nimcp_free(ctx);
}


/**
 * @brief Start new vote
 */
static bool consensus_start_vote(consensus_context_t* ctx, const vote_proposal_t* proposal) {
    if (!ctx || !proposal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consensus_start_vote: required parameter is NULL (ctx, proposal)");
        return false;
    }

    nimcp_platform_mutex_lock(ctx->lock);

    // Find free vote slot
    active_vote_t* vote = NULL;
    for (uint32_t i = 0; i < MAX_ACTIVE_VOTES; i++) {
        if (!ctx->votes[i].active) {
            vote = &ctx->votes[i];
            break;
        }
    }

    if (!vote) {
        LOG_WARN("No free vote slots available");
        nimcp_platform_mutex_unlock(ctx->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consensus_start_vote: vote is NULL");
        return false;
    }

    // Initialize vote
    vote->proposal = *proposal;
    vote->votes_for = 0;
    vote->votes_against = 0;
    vote->votes_abstain = 0;
    vote->expiry_ms = proposal->expiry_ms;
    vote->active = true;
    vote->completed = false;
    ctx->vote_count++;

    LOG_DEBUG("Started vote: proposal_id=%u, action_type=%u",
              proposal->proposal_id, proposal->action_type);

    nimcp_platform_mutex_unlock(ctx->lock);
    return true;
}


/**
 * @brief Cast vote on proposal
 */
static bool consensus_cast_vote(consensus_context_t* ctx, uint32_t proposal_id, vote_decision_t decision) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consensus_cast_vote: ctx is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(ctx->lock);

    // Find vote
    active_vote_t* vote = NULL;
    for (uint32_t i = 0; i < MAX_ACTIVE_VOTES; i++) {
        if (ctx->votes[i].active && ctx->votes[i].proposal.proposal_id == proposal_id) {
            vote = &ctx->votes[i];
            break;
        }
    }

    if (!vote) {
        nimcp_platform_mutex_unlock(ctx->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consensus_cast_vote: vote is NULL");
        return false;
    }

    // Record vote
    switch (decision) {
        case VOTE_APPROVE:
            vote->votes_for++;
            break;
        case VOTE_REJECT:
            vote->votes_against++;
            break;
        case VOTE_ABSTAIN:
            vote->votes_abstain++;
            break;
    }

    nimcp_platform_mutex_unlock(ctx->lock);
    return true;
}


//=============================================================================
// Message Handlers
//=============================================================================

static void handle_heartbeat(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id) {
    if (!swarm || !data) return;

    // Heartbeat just updates peer presence
    update_peer(swarm, source_id);
}


static void handle_perception(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id) {
    if (!swarm || !data || len < sizeof(perception_data_t)) return;

    perception_data_t* perception = (perception_data_t*)data;
    update_peer(swarm, source_id);

    LOG_DEBUG("Received perception from drone %u: sensor_type=%u, confidence=%.3f",
              source_id, perception->sensor_type, perception->confidence);

    // Update workspace with perception - concept_id derived from sensor_type
    // Attention based on confidence (weighted for peer input)
    uint32_t concept_id = perception->sensor_type;
    float attention = perception->confidence * 0.5F;  // Weight peer perceptions
    workspace_update_entry(swarm->workspace, concept_id, attention);
}


static void handle_threat(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id) {
    if (!swarm || !data || len < sizeof(threat_data_t)) return;

    threat_data_t* threat = (threat_data_t*)data;
    update_peer(swarm, source_id);

    LOG_WARN("THREAT from drone %u: type=%u, severity=%.3f, desc=%s",
             source_id, threat->threat_type, threat->severity, threat->description);

    // TODO: Urgent processing - alert local brain, update workspace
}


/**
 * @brief Broadcast a vote decision for a proposal
 */
static bool broadcast_vote(swarm_brain_t* swarm, uint32_t proposal_id, vote_decision_t decision) {
    if (!swarm || !swarm->signal_adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broadcast_vote: required parameter is NULL (swarm, swarm->signal_adapter)");
        return false;
    }

    uint8_t message[16];
    message[0] = SWARM_MSG_VOTE_CAST;
    memcpy(message + 1, &proposal_id, sizeof(proposal_id));
    memcpy(message + 1 + sizeof(proposal_id), &decision, sizeof(decision));

    bool success = swarm_signal_broadcast(swarm->signal_adapter, message,
                                          1 + sizeof(proposal_id) + sizeof(decision));
    if (success) {
        // Record our own vote locally
        consensus_cast_vote(swarm->consensus, proposal_id, decision);

        nimcp_platform_mutex_lock(swarm->stats_lock);
        swarm->stats.messages_sent++;
        nimcp_platform_mutex_unlock(swarm->stats_lock);

        LOG_DEBUG("Cast vote: proposal_id=%u, decision=%d", proposal_id, decision);
    }
    return success;
}


static void handle_vote_propose(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id) {
    if (!swarm || !data || len < sizeof(vote_proposal_t)) return;

    vote_proposal_t* proposal = (vote_proposal_t*)data;
    update_peer(swarm, source_id);

    LOG_DEBUG("Vote proposal from drone %u: proposal_id=%u, action_type=%u",
              source_id, proposal->proposal_id, proposal->action_type);

    consensus_start_vote(swarm->consensus, proposal);

    // Evaluate proposal locally and cast vote
    // For now, simple heuristic: approve if from known peer and not expired
    uint64_t now = get_time_ms();
    vote_decision_t decision = VOTE_APPROVE;

    if (proposal->expiry_ms < now) {
        decision = VOTE_REJECT;  // Already expired
    }
    // Could add more sophisticated evaluation based on action_type, parameters, etc.

    broadcast_vote(swarm, proposal->proposal_id, decision);
}


static void handle_vote_cast(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id) {
    if (!swarm || !data || len < 8) return;

    uint32_t proposal_id = *(uint32_t*)data;
    vote_decision_t decision = *(vote_decision_t*)(data + 4);
    update_peer(swarm, source_id);

    consensus_cast_vote(swarm->consensus, proposal_id, decision);
}


static void handle_neuromod_sync(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id) {
    if (!swarm || !data || len < sizeof(neuromod_state_t)) return;

    neuromod_state_t* state = (neuromod_state_t*)data;
    update_peer(swarm, source_id);

    LOG_DEBUG("Neuromod sync from drone %u: DA=%.3f, 5HT=%.3f, NE=%.3f, ACh=%.3f",
              source_id, state->dopamine, state->serotonin,
              state->norepinephrine, state->acetylcholine);

    // Update workspace with emotional state - high arousal (NE) indicates salience
    // Use a concept ID for "emotional state" updates
    float arousal = state->norepinephrine;
    float attention = arousal * 0.3F;  // Weight emotional states lower than perceptions
    workspace_update_entry(swarm->workspace, 1000 + source_id, attention);
}


static void handle_goodbye(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id) {
    if (!swarm) return;

    LOG_INFO("Peer leaving: drone_id=%u", source_id);

    // Mark peer as inactive immediately
    nimcp_platform_mutex_lock(swarm->peer_lock);
    swarm_peer_info_t* peer = find_peer(swarm, source_id);
    if (peer) {
        peer->active = false;
        // Remove from array
        for (uint32_t i = 0; i < swarm->peer_count; i++) {
            if (swarm->peers[i].drone_id == source_id) {
                if (i < swarm->peer_count - 1) {
                    swarm->peers[i] = swarm->peers[swarm->peer_count - 1];
                }
                swarm->peer_count--;
                break;
            }
        }
    }
    nimcp_platform_mutex_unlock(swarm->peer_lock);
}


/**
 * @brief Main message dispatcher
 */
static void handle_message(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id) {
    if (!swarm || !data || len < 1) return;

    swarm_message_type_t msg_type = (swarm_message_type_t)data[0];
    uint8_t* payload = data + 1;
    uint32_t payload_len = len - 1;

    // Dispatch based on message type
    switch (msg_type) {
        case SWARM_MSG_HEARTBEAT:
            handle_heartbeat(swarm, payload, payload_len, source_id);
            break;
        case SWARM_MSG_PERCEPTION:
            handle_perception(swarm, payload, payload_len, source_id);
            break;
        case SWARM_MSG_THREAT:
            handle_threat(swarm, payload, payload_len, source_id);
            break;
        case SWARM_MSG_VOTE_PROPOSE:
            handle_vote_propose(swarm, payload, payload_len, source_id);
            break;
        case SWARM_MSG_VOTE_CAST:
            handle_vote_cast(swarm, payload, payload_len, source_id);
            break;
        case SWARM_MSG_NEUROMOD_SYNC:
            handle_neuromod_sync(swarm, payload, payload_len, source_id);
            break;
        case SWARM_MSG_WORKSPACE_UPDATE:
            handle_workspace_update(swarm, payload, payload_len, source_id);
            break;
        case SWARM_MSG_GOODBYE:
            handle_goodbye(swarm, payload, payload_len, source_id);
            break;
        default:
            LOG_WARN("Unknown message type: %u from drone %u", msg_type, source_id);
            break;
    }

    // Update stats
    nimcp_platform_mutex_lock(swarm->stats_lock);
    swarm->stats.messages_received++;
    nimcp_platform_mutex_unlock(swarm->stats_lock);
}


//=============================================================================
// Processing Functions
//=============================================================================

static bool process_incoming_messages(swarm_brain_t* swarm) {
    if (!swarm || !swarm->signal_adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "process_incoming_messages: required parameter is NULL (swarm, swarm->signal_adapter)");
        return false;
    }

    uint8_t buffer[SWARM_MAX_MESSAGE_SIZE];
    uint32_t received_len;
    uint32_t source_id;

    // Process all available messages (non-blocking)
    while (swarm_signal_receive(swarm->signal_adapter, buffer, SWARM_MAX_MESSAGE_SIZE,
                                &received_len, &source_id)) {
        if (received_len > 0 && source_id != swarm->config.drone_id) {
            handle_message(swarm, buffer, received_len, source_id);
        }
    }

    return true;
}


static bool send_heartbeat(swarm_brain_t* swarm) {
    if (!swarm || !swarm->signal_adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "send_heartbeat: required parameter is NULL (swarm, swarm->signal_adapter)");
        return false;
    }

    uint64_t now = get_time_ms();
    uint32_t jitter = simple_rand(HEARTBEAT_JITTER_MS);

    if ((now - swarm->last_heartbeat_ms) < (swarm->config.heartbeat_ms + jitter)) {
        return true; // Not time yet
    }

    uint8_t message[2];
    message[0] = SWARM_MSG_HEARTBEAT;
    message[1] = 0; // No payload

    bool success = swarm_signal_broadcast(swarm->signal_adapter, message, 2);
    if (success) {
        swarm->last_heartbeat_ms = now;

        nimcp_platform_mutex_lock(swarm->stats_lock);
        swarm->stats.messages_sent++;
        nimcp_platform_mutex_unlock(swarm->stats_lock);
    }

    return success;
}


static bool update_peers(swarm_brain_t* swarm) {
    if (!swarm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "update_peers: swarm is NULL");
        return false;
    }

    remove_inactive_peers(swarm);

    nimcp_platform_mutex_lock(swarm->stats_lock);
    swarm->stats.peers_connected = swarm->peer_count;
    nimcp_platform_mutex_unlock(swarm->stats_lock);

    return true;
}


static bool update_workspace(swarm_brain_t* swarm) {
    if (!swarm || !swarm->workspace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "update_workspace: required parameter is NULL (swarm, swarm->workspace)");
        return false;
    }

    workspace_decay(swarm->workspace);
    float coherence = workspace_calculate_coherence(swarm->workspace);

    nimcp_platform_mutex_lock(swarm->stats_lock);
    swarm->stats.workspace_coherence = coherence;
    nimcp_platform_mutex_unlock(swarm->stats_lock);

    return true;
}


static bool process_votes(swarm_brain_t* swarm) {
    if (!swarm || !swarm->consensus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "process_votes: required parameter is NULL (swarm, swarm->consensus)");
        return false;
    }

    consensus_process_votes(swarm->consensus, swarm->peer_count);

    nimcp_platform_mutex_lock(swarm->stats_lock);
    swarm->stats.votes_completed = swarm->consensus->total_votes_completed;
    nimcp_platform_mutex_unlock(swarm->stats_lock);

    return true;
}


static bool update_emergence_tier(swarm_brain_t* swarm) {
    if (!swarm || !swarm->emergence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "update_emergence_tier: required parameter is NULL (swarm, swarm->emergence)");
        return false;
    }

    float coherence = swarm->workspace ? swarm->workspace->coherence : 0.0F;
    emergence_update_tier(swarm->emergence, swarm->peer_count, coherence);

    nimcp_platform_mutex_lock(swarm->stats_lock);
    swarm->stats.emergence_tier_changes = swarm->emergence->tier_change_count;
    nimcp_platform_mutex_unlock(swarm->stats_lock);

    return true;
}

static local_brain_instance_t* get_local_brains(swarm_brain_t* swarm) {
    // TODO(P2): Move to per-instance struct for full isolation
    // Static array shared between all swarm instances - protected by callers' locks
    static local_brain_instance_t local_brains[MAX_LOCAL_BRAINS] = {0};
    (void)swarm;  // Currently unused - would key into per-instance storage
    return local_brains;
}


/**
 * @brief Find local brain by agent ID
 */
static local_brain_instance_t* find_local_brain(swarm_brain_t* swarm, uint16_t agent_id) {
    if (!swarm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm is NULL");

        return NULL;

    }

    local_brain_instance_t* brains = get_local_brains(swarm);
    for (uint32_t i = 0; i < MAX_LOCAL_BRAINS; i++) {
        if (brains[i].active && brains[i].agent_id == agent_id) {
            return &brains[i];
        }
    }
    return NULL;  /* Not found - normal search miss */
}
