// nimcp_global_workspace_part_helpers.c - helpers functions
// Part of nimcp_global_workspace.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_global_workspace.c


/**
 * @brief Handle attention shift via bio-async
 */
static nimcp_error_t handle_attention_shift(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)msg_size;

    if (!msg || !user_data) {
        if (response_promise) {
            nimcp_bio_promise_complete(response_promise, NULL);
        }
        return NIMCP_ERROR_NULL_ARG;
    }

    const bio_msg_attention_shift_t* shift = (const bio_msg_attention_shift_t*)msg;
    global_workspace_t* ws = (global_workspace_t*)user_data;
    (void)ws;

    LOG_DEBUG("Received attention shift: target=%u, weight=%.2f",
              shift->target_id, shift->attention_weight);

    // TODO: Process attention shift — currently a no-op acknowledgment

    // Complete the promise so callers waiting on a response are unblocked
    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, NULL);
    }

    return NIMCP_SUCCESS;
}


/**
 * @brief Broadcast workspace broadcast event
 * Note: Using internal struct pointer (struct global_workspace_struct*) instead of typedef
 */
static void bio_broadcast_workspace_event(struct global_workspace_struct* ws, uint32_t broadcast_id, float strength) {
    if (!ws || !ws->bio_async_enabled || !ws->bio_ctx) {
        return;
    }

    bio_msg_attention_shift_t msg = {};
    bio_msg_init_header(&msg.header, BIO_MSG_ATTENTION_SHIFT,
                        bio_module_context_get_id(ws->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.target_id = broadcast_id;
    msg.attention_weight = strength;

    bio_router_broadcast(ws->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG("Broadcast: workspace event %u", broadcast_id);
}


//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 *
 * WHAT: Monotonic timestamp for timing operations
 * WHY:  Need consistent time source for refractory period, decay
 * HOW:  clock_gettime(CLOCK_MONOTONIC)
 *
 * @return Milliseconds since arbitrary epoch
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}


/**
 * @brief Apply exponential decay to competitor signal
 *
 * WHAT: Reduce strength of stale competition entries
 * WHY:  Prevent old submissions from lingering indefinitely
 * HOW:  strength *= exp(-dt / tau)
 *
 * @param original_strength Initial strength
 * @param age_ms How long since submission
 * @param tau_ms Time constant
 * @return Decayed strength
 */
static float apply_decay(float original_strength, uint64_t age_ms, float tau_ms) {
    if (tau_ms <= 0.0F) return original_strength;
    float decay_factor = expf(-(float)age_ms / tau_ms);
    return original_strength * decay_factor;
}


/**
 * @brief Resolve winner-take-all competition
 *
 * WHAT: Find strongest competitor above threshold
 * WHY:  Most biologically realistic competition mechanism
 * HOW:  argmax(strengths) where strength >= ignition_threshold
 *
 * @param workspace Workspace instance
 * @param winner_idx Output: index of winner in competitors array
 * @param winner_strength Output: strength of winner (after decay)
 * @return true if winner found (above threshold), false otherwise
 */
static bool resolve_winner_take_all(
    struct global_workspace_struct* workspace,
    uint32_t* winner_idx,
    float* winner_strength)
{
    float max_strength = 0.0F;
    uint32_t max_idx = 0;
    bool found_any = false;
    uint64_t current_time = get_time_ms();

    // Find strongest competitor (with decay applied)
    for (uint32_t i = 0; i < GLOBAL_WORKSPACE_MAX_COMPETITORS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GLOBAL_WORKSPACE_MAX_COMPETITORS > 256) {
            global_workspace_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)GLOBAL_WORKSPACE_MAX_COMPETITORS);
        }

        if (!workspace->competitors[i].is_active) continue;

        // Apply decay
        uint64_t age_ms = current_time - workspace->competitors[i].timestamp_ms;
        float decayed_strength = apply_decay(
            workspace->competitors[i].strength,
            age_ms,
            workspace->config.competition_decay_tau_ms
        );

        // Prune if decayed below minimum
        if (decayed_strength < GLOBAL_WORKSPACE_MIN_IGNITION_THRESHOLD) {
            // Free content buffer before deactivating to prevent leak
            if (workspace->competitors[i].content) {
                nimcp_free(workspace->competitors[i].content);
                workspace->competitors[i].content = NULL;
            }
            workspace->competitors[i].is_active = false;
            workspace->num_active_competitors--;
            continue;
        }

        // Track maximum
        if (decayed_strength > max_strength) {
            max_strength = decayed_strength;
            max_idx = i;
            found_any = true;
        }
    }

    // Check if winner exceeds ignition threshold
    if (found_any && max_strength >= workspace->config.ignition_threshold) {
        *winner_idx = max_idx;
        *winner_strength = max_strength;
        return true;
    }

    return false;  // No winner above threshold
}


/**
 * @brief Resolve priority-based competition
 *
 * WHAT: Select highest priority module, then strongest
 * WHY:  Some modules are more important (safety, pain, etc.)
 * HOW:  argmax(priorities), tiebreak with strength
 *
 * @param workspace Workspace instance
 * @param winner_idx Output: index of winner
 * @param winner_strength Output: strength of winner
 * @return true if winner found
 */
static bool resolve_priority_based(
    struct global_workspace_struct* workspace,
    uint32_t* winner_idx,
    float* winner_strength)
{
    float max_priority = -1.0F;
    float max_strength_at_priority = 0.0F;
    uint32_t max_idx = 0;
    bool found_any = false;
    uint64_t current_time = get_time_ms();

    for (uint32_t i = 0; i < GLOBAL_WORKSPACE_MAX_COMPETITORS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GLOBAL_WORKSPACE_MAX_COMPETITORS > 256) {
            global_workspace_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)GLOBAL_WORKSPACE_MAX_COMPETITORS);
        }

        if (!workspace->competitors[i].is_active) continue;

        // Apply decay
        uint64_t age_ms = current_time - workspace->competitors[i].timestamp_ms;
        float decayed_strength = apply_decay(
            workspace->competitors[i].strength,
            age_ms,
            workspace->config.competition_decay_tau_ms
        );

        if (decayed_strength < GLOBAL_WORKSPACE_MIN_IGNITION_THRESHOLD) {
            // Free content buffer before deactivating to prevent leak
            if (workspace->competitors[i].content) {
                nimcp_free(workspace->competitors[i].content);
                workspace->competitors[i].content = NULL;
            }
            workspace->competitors[i].is_active = false;
            workspace->num_active_competitors--;
            continue;
        }

        // Get module priority
        cognitive_module_t module = workspace->competitors[i].module;
        float priority = (module < MODULE_CUSTOM_START) ?
                          workspace->config.module_priorities[module] : 0.5F;

        // Compare priority first, strength second
        if (priority > max_priority ||
            (priority == max_priority && decayed_strength > max_strength_at_priority)) {
            max_priority = priority;
            max_strength_at_priority = decayed_strength;
            max_idx = i;
            found_any = true;
        }
    }

    // Check ignition threshold
    if (found_any && max_strength_at_priority >= workspace->config.ignition_threshold) {
        *winner_idx = max_idx;
        *winner_strength = max_strength_at_priority;
        return true;
    }

    /* P2-COG-01: No winner found is normal resolution behavior, not an error */
    return false;
}


/**
 * @brief Resolve round-robin competition
 *
 * WHAT: Take turns among competitors
 * WHY:  Ensure fairness, prevent starvation
 * HOW:  Select next in sequence after last winner
 *
 * @param workspace Workspace instance
 * @param winner_idx Output: index of winner
 * @param winner_strength Output: strength of winner
 * @return true if winner found
 */
static bool resolve_round_robin(
    struct global_workspace_struct* workspace,
    uint32_t* winner_idx,
    float* winner_strength)
{
    if (workspace->num_active_competitors == 0) {
        return false;
    }

    uint64_t current_time = get_time_ms();
    uint32_t start_idx = (workspace->last_winner_idx + 1) % GLOBAL_WORKSPACE_MAX_COMPETITORS;

    // Find next active competitor after last winner
    for (uint32_t offset = 0; offset < GLOBAL_WORKSPACE_MAX_COMPETITORS; offset++) {
        /* Phase 8: Loop progress heartbeat */
        if ((offset & 0xFF) == 0 && GLOBAL_WORKSPACE_MAX_COMPETITORS > 256) {
            global_workspace_heartbeat("global_works_loop",
                             (float)(offset + 1) / (float)GLOBAL_WORKSPACE_MAX_COMPETITORS);
        }

        uint32_t idx = (start_idx + offset) % GLOBAL_WORKSPACE_MAX_COMPETITORS;
        if (!workspace->competitors[idx].is_active) continue;

        // Apply decay
        uint64_t age_ms = current_time - workspace->competitors[idx].timestamp_ms;
        float decayed_strength = apply_decay(
            workspace->competitors[idx].strength,
            age_ms,
            workspace->config.competition_decay_tau_ms
        );

        if (decayed_strength < GLOBAL_WORKSPACE_MIN_IGNITION_THRESHOLD) {
            // Free content buffer before deactivating to prevent leak
            if (workspace->competitors[idx].content) {
                nimcp_free(workspace->competitors[idx].content);
                workspace->competitors[idx].content = NULL;
            }
            workspace->competitors[idx].is_active = false;
            workspace->num_active_competitors--;
            continue;
        }

        // Found next active competitor
        *winner_idx = idx;
        *winner_strength = decayed_strength;
        workspace->last_winner_idx = idx;
        return true;
    }

    /* P2-COG-02: No active competitor found is normal, not an error */
    return false;
}


/**
 * @brief Broadcast winner's content to workspace
 *
 * WHAT: Update workspace with winning content, notify subscribers
 * WHY:  Make content globally available
 * HOW:  Copy content, update broadcast state, add to history
 *
 * @param workspace Workspace instance
 * @param winner_idx Index of winning competitor
 * @param winner_strength Decayed strength of winner
 */
static void broadcast_winner(
    struct global_workspace_struct* workspace,
    uint32_t winner_idx,
    float winner_strength)
{
    competitor_entry_t* winner = &workspace->competitors[winner_idx];

    // Copy content to broadcast buffer
    memcpy(workspace->broadcast_content, winner->content,
           workspace->config.capacity_dim * sizeof(float));

    // Update broadcast state
    workspace->current_broadcast.content = workspace->broadcast_content;
    workspace->current_broadcast.content_dim = workspace->config.capacity_dim;
    workspace->current_broadcast.source_module = winner->module;
    workspace->current_broadcast.source_strength = winner_strength;
    workspace->current_broadcast.broadcast_timestamp_ms = get_time_ms();
    workspace->current_broadcast.broadcast_id = workspace->next_broadcast_id++;
    workspace->current_broadcast.num_competitors = workspace->num_active_competitors;
    workspace->current_broadcast.is_valid = true;

    // Find runner-up strength (for statistics)
    float runner_up = 0.0F;
    for (uint32_t i = 0; i < GLOBAL_WORKSPACE_MAX_COMPETITORS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GLOBAL_WORKSPACE_MAX_COMPETITORS > 256) {
            global_workspace_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)GLOBAL_WORKSPACE_MAX_COMPETITORS);
        }

        if (i == winner_idx || !workspace->competitors[i].is_active) continue;
        if (workspace->competitors[i].strength > runner_up) {
            runner_up = workspace->competitors[i].strength;
        }
    }
    workspace->current_broadcast.runner_up_strength = runner_up;

    // Update timing
    workspace->last_broadcast_time_ms = workspace->current_broadcast.broadcast_timestamp_ms;

    // Add to history (if enabled)
    if (workspace->config.enable_history && workspace->history != NULL) {
        uint32_t hist_idx = workspace->history_head;

        // Copy broadcast metadata
        workspace->history[hist_idx] = workspace->current_broadcast;

        // Copy content
        memcpy(workspace->history_content[hist_idx], workspace->broadcast_content,
               workspace->config.capacity_dim * sizeof(float));

        // Update history to point to copied content
        workspace->history[hist_idx].content = workspace->history_content[hist_idx];

        // Advance circular buffer
        workspace->history_head = (workspace->history_head + 1) % workspace->config.history_depth;
        if (workspace->history_count < workspace->config.history_depth) {
            workspace->history_count++;
        }
    }

    // Update statistics
    if (workspace->config.enable_statistics) {
        workspace->stats.total_broadcasts++;
        if (winner->module < MODULE_CUSTOM_START) {
            workspace->stats.broadcasts_per_module[winner->module]++;
        }
    }

    /* W9-kg: publish broadcast event to internal KG so W16 consumers
     * (brain_decide, middleware attention_focus) can see the winner. */
    {
        struct brain_struct* brain = w9kg_get_registered_brain();
        if (brain) {
            w9kg_emit_gws_broadcast(brain,
                                    (uint32_t)winner->module,
                                    winner_strength,
                                    workspace->current_broadcast.broadcast_id);
        }
    }

    // Note: Winner removal now happens in global_workspace_compete() after broadcast
}
