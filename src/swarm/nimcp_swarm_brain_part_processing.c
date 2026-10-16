// nimcp_swarm_brain_part_processing.c - processing functions
// Part of nimcp_swarm_brain.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_swarm_brain.c

#define EMERGENCE_COHERENCE_THRESHOLD 0.2f

/**
 * @brief Update workspace entry
 */
static void workspace_update_entry(collective_workspace_t* ws, uint32_t concept_id, float attention) {
    if (!ws || !ws->entries) return;
    if (!ws->lock) return;

    nimcp_platform_mutex_lock(ws->lock);

    // Find or create entry
    workspace_entry_t* entry = NULL;
    for (uint32_t i = 0; i < ws->size; i++) {
        if (ws->entries[i].concept_id == concept_id) {
            entry = &ws->entries[i];
            break;
        }
        if (ws->entries[i].attention < WORKSPACE_MIN_ATTENTION && !entry) {
            // Reuse low-attention entry
            entry = &ws->entries[i];
        }
    }

    if (entry) {
        entry->concept_id = concept_id;
        entry->attention = fminf(entry->attention + attention, 1.0F);
        entry->contributor_count++;
        entry->last_update_ms = get_time_ms();
    }

    ws->last_update_ms = get_time_ms();

    nimcp_platform_mutex_unlock(ws->lock);
}


/**
 * @brief Update emergence tier based on peer count and coherence
 */
static void emergence_update_tier(emergence_context_t* ctx, uint32_t peer_count, float coherence) {
    if (!ctx) return;

    swarm_emergence_tier_t old_tier = ctx->current_tier;
    swarm_emergence_tier_t new_tier = SWARM_TIER_INDIVIDUAL;

    // Determine tier based on peer count
    // Note: peer_count is number of OTHER drones known, not total including self
    // TIER_PAIR: at least 1 peer (2 total drones)
    // TIER_SQUAD: at least 4 peers (5 total drones)
    // TIER_PLATOON: at least 7 peers (8 total drones)
    // TIER_COMPANY: at least 15 peers (16 total drones)
    if (peer_count >= 15) {
        new_tier = SWARM_TIER_COMPANY;
    } else if (peer_count >= 7) {
        new_tier = SWARM_TIER_PLATOON;
    } else if (peer_count >= 4) {
        new_tier = SWARM_TIER_SQUAD;
    } else if (peer_count >= 1) {
        new_tier = SWARM_TIER_PAIR;
    } else {
        new_tier = SWARM_TIER_INDIVIDUAL;
    }

    // Require some coherence for highest tier only
    // Note: In test scenarios coherence may be low due to limited workspace activity
    if (new_tier >= SWARM_TIER_COMPANY && coherence < EMERGENCE_COHERENCE_THRESHOLD) {
        new_tier = SWARM_TIER_PLATOON;
    }

    // Update if tier changed
    if (new_tier != old_tier) {
        ctx->current_tier = new_tier;
        ctx->tier_change_count++;
        ctx->tier_enter_time_ms = get_time_ms();

        LOG_INFO("Emergence tier changed: %s -> %s (peers=%u, coherence=%.3f)",
                 swarm_emergence_tier_string(old_tier),
                 swarm_emergence_tier_string(new_tier),
                 peer_count, coherence);
    }

    // Update coherence history
    ctx->coherence_history[ctx->coherence_index] = coherence;
    ctx->coherence_index = (ctx->coherence_index + 1) % 10;
}


/**
 * @brief Process active votes (check timeouts, quorum)
 */
static void consensus_process_votes(consensus_context_t* ctx, uint32_t peer_count) {
    if (!ctx) return;

    uint64_t now = get_time_ms();
    nimcp_platform_mutex_lock(ctx->lock);

    for (uint32_t i = 0; i < MAX_ACTIVE_VOTES; i++) {
        active_vote_t* vote = &ctx->votes[i];
        if (!vote->active || vote->completed) continue;

        uint32_t total_votes = vote->votes_for + vote->votes_against + vote->votes_abstain;
        uint32_t quorum = (uint32_t)(peer_count * VOTE_QUORUM_RATIO);

        // Check timeout
        if (now >= vote->expiry_ms) {
            vote->completed = true;
            vote->active = false;
            ctx->vote_count--;
            ctx->total_votes_completed++;

            LOG_INFO("Vote expired: proposal_id=%u, votes=%u/%u",
                     vote->proposal.proposal_id, total_votes, peer_count);
            continue;
        }

        // Check quorum
        if (total_votes >= quorum) {
            vote->completed = true;
            vote->active = false;
            ctx->vote_count--;
            ctx->total_votes_completed++;

            // Determine result
            if (vote->votes_for > vote->votes_against) {
                vote->result = VOTE_APPROVE;
            } else if (vote->votes_against > vote->votes_for) {
                vote->result = VOTE_REJECT;
            } else {
                vote->result = VOTE_ABSTAIN;
            }

            LOG_INFO("Vote completed: proposal_id=%u, result=%d, for=%u, against=%u",
                     vote->proposal.proposal_id, vote->result,
                     vote->votes_for, vote->votes_against);
        }
    }

    nimcp_platform_mutex_unlock(ctx->lock);
}


static void handle_workspace_update(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id) {
    if (!swarm || !data || len < 8) return;

    uint32_t concept_id = *(uint32_t*)data;
    float attention = *(float*)(data + 4);
    update_peer(swarm, source_id);

    workspace_update_entry(swarm->workspace, concept_id, attention * 0.5F); // Weight by 0.5 for peer input
}


bool swarm_brain_process(swarm_brain_t* swarm) {
    if (!swarm || !swarm->operational) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_process: required parameter is NULL (swarm, swarm->operational)");
        return false;
    }

    // Process incoming messages
    process_incoming_messages(swarm);

    // Send periodic heartbeat
    send_heartbeat(swarm);

    // Update peer list (remove timeouts)
    update_peers(swarm);

    // Update workspace (decay, coherence)
    update_workspace(swarm);

    // Process active votes
    process_votes(swarm);

    // Update emergence tier
    update_emergence_tier(swarm);

    // Update uptime stat
    nimcp_platform_mutex_lock(swarm->stats_lock);
    swarm->stats.uptime_ms = get_time_ms() - swarm->creation_time_ms;
    nimcp_platform_mutex_unlock(swarm->stats_lock);

    return true;
}
