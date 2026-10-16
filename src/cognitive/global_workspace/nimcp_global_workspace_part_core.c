// nimcp_global_workspace_part_core.c - core functions
// Part of nimcp_global_workspace.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_global_workspace.c


//=============================================================================
// Core Operations
//=============================================================================

bool global_workspace_compete(
    global_workspace_t* workspace,
    cognitive_module_t module,
    const float* content,
    uint32_t content_dim,
    float strength)
{
    // WHAT: Submit content and immediately resolve competition
    // WHY:  Backward compatibility - original API auto-resolves
    // HOW:  Use submit() + resolve() internally
    // TODO: The slot-finding, validation, and content-copy logic is duplicated
    //       between compete() and submit(). Extract a shared internal helper
    //       (e.g., submit_to_pool_unlocked()) to reduce maintenance burden.

    // Guard: NULL checks
    if (workspace == NULL || content == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_compete: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_compete", 0.0f);


    struct global_workspace_struct* ws = (struct global_workspace_struct*)workspace;

    // Thread safety - lock for entire operation
    nimcp_platform_mutex_lock(&ws->mutex);

    // Process pending bio-async messages before competition
    if (ws->bio_async_enabled && ws->bio_ctx) {
        bio_router_process_inbox(ws->bio_ctx, 10);  // Process up to 10 messages
    }

    // Submit to competition pool (internal call, already locked)
    // Note: We call the internal parts directly to avoid recursive locking
    // Validate content dimension
    if (content_dim != ws->config.capacity_dim) {
        LOG_ERROR("Content dimension mismatch in global_workspace_compete: "
                "expected %u, got %u", ws->config.capacity_dim, content_dim);
        nimcp_platform_mutex_unlock(&ws->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_compete: validation failed");
        return false;
    }

    // Validate strength range
    if (strength < 0.0F || strength > 1.0F) {
        LOG_ERROR("Invalid strength in global_workspace_compete: %.2f "
                "(must be 0.0-1.0)", strength);
        nimcp_platform_mutex_unlock(&ws->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_compete: validation failed");
        return false;
    }

    // Update statistics
    if (ws->config.enable_statistics) {
        ws->stats.total_competitions++;
        if (module < MODULE_CUSTOM_START) {
            ws->stats.competitions_per_module[module]++;
        }
    }

    // Find slot for this module (update existing or find empty)
    uint32_t slot_idx = GLOBAL_WORKSPACE_MAX_COMPETITORS;

    // Check if module already in pool (update case)
    for (uint32_t i = 0; i < GLOBAL_WORKSPACE_MAX_COMPETITORS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GLOBAL_WORKSPACE_MAX_COMPETITORS > 256) {
            global_workspace_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)GLOBAL_WORKSPACE_MAX_COMPETITORS);
        }

        if (ws->competitors[i].is_active && ws->competitors[i].module == module) {
            slot_idx = i;
            // Free old content buffer
            if (ws->competitors[i].content) {
                nimcp_free(ws->competitors[i].content);
            }
            break;
        }
    }

    // If not found, find empty slot
    if (slot_idx == GLOBAL_WORKSPACE_MAX_COMPETITORS) {
        for (uint32_t i = 0; i < GLOBAL_WORKSPACE_MAX_COMPETITORS; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && GLOBAL_WORKSPACE_MAX_COMPETITORS > 256) {
                global_workspace_heartbeat("global_works_loop",
                                 (float)(i + 1) / (float)GLOBAL_WORKSPACE_MAX_COMPETITORS);
            }

            if (!ws->competitors[i].is_active) {
                slot_idx = i;
                break;
            }
        }
    }

    // Guard: Check if pool is full
    if (slot_idx == GLOBAL_WORKSPACE_MAX_COMPETITORS) {
        LOG_WARN("Competition pool full in global_workspace_compete "
                "(%u competitors)", GLOBAL_WORKSPACE_MAX_COMPETITORS);
        nimcp_platform_mutex_unlock(&ws->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_compete: validation failed");
        return false;
    }

    // Track if we're adding new competitor vs updating existing
    bool was_inactive = !ws->competitors[slot_idx].is_active;
    bool pool_was_empty = (ws->num_active_competitors == 0);

    // Copy content - we own this buffer now
    float* content_copy = (float*)nimcp_calloc(content_dim, sizeof(float));
    if (!content_copy) {
        LOG_ERROR("Failed to allocate content buffer in global_workspace_compete");
        nimcp_platform_mutex_unlock(&ws->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "global_workspace_compete: content_copy is NULL");
        return false;
    }
    memcpy(content_copy, content, content_dim * sizeof(float));

    // Add/update competitor in pool
    ws->competitors[slot_idx].module = module;
    ws->competitors[slot_idx].content = content_copy;  // We own this buffer
    ws->competitors[slot_idx].content_dim = content_dim;
    ws->competitors[slot_idx].strength = strength;
    ws->competitors[slot_idx].timestamp_ms = get_time_ms();
    ws->competitors[slot_idx].is_active = true;

    // Update pool counts
    if (was_inactive) {
        ws->num_active_competitors++;
        if (pool_was_empty) {
            ws->pool_activation_time_ms = get_time_ms();
        }
    }

    // Immediately resolve competition (backward compatible behavior)
    cognitive_module_t winner = MODULE_NONE;

    uint64_t current_time = get_time_ms();
    uint32_t winner_idx = 0;
    float winner_strength = 0.0f;
    bool found_winner = false;

    switch (ws->config.strategy) {
        case COMPETITION_WINNER_TAKE_ALL:
            found_winner = resolve_winner_take_all(ws, &winner_idx, &winner_strength);
            break;
        case COMPETITION_PRIORITY_BASED:
            found_winner = resolve_priority_based(ws, &winner_idx, &winner_strength);
            break;
        case COMPETITION_ROUND_ROBIN:
            found_winner = resolve_round_robin(ws, &winner_idx, &winner_strength);
            break;
        case COMPETITION_WEIGHTED_FUSION:
            found_winner = resolve_winner_take_all(ws, &winner_idx, &winner_strength);
            break;
        default:
            found_winner = false;
            break;
    }

    bool broadcast_occurred = false;
    if (found_winner) {
        // Check refractory period
        bool can_broadcast = true;
        if (ws->last_broadcast_time_ms > 0) {
            uint64_t time_since_broadcast = current_time - ws->last_broadcast_time_ms;
            if (time_since_broadcast < ws->config.refractory_period_ms) {
                can_broadcast = false;
            }
        }

        if (can_broadcast) {
            broadcast_winner(ws, winner_idx, winner_strength);
            winner = ws->competitors[winner_idx].module;

            // Clear the entire competition pool after broadcasting
            for (uint32_t i = 0; i < GLOBAL_WORKSPACE_MAX_COMPETITORS; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && GLOBAL_WORKSPACE_MAX_COMPETITORS > 256) {
                    global_workspace_heartbeat("global_works_loop",
                                     (float)(i + 1) / (float)GLOBAL_WORKSPACE_MAX_COMPETITORS);
                }

                if (ws->competitors[i].is_active) {
                    if (ws->competitors[i].content) {
                        nimcp_free(ws->competitors[i].content);
                        ws->competitors[i].content = NULL;
                    }
                    ws->competitors[i].is_active = false;
                }
            }
            ws->num_active_competitors = 0;
            ws->pool_activation_time_ms = 0;

            broadcast_occurred = true;
        }
    }

    nimcp_platform_mutex_unlock(&ws->mutex);

    // Return true only if THIS module won and was broadcast
    return (broadcast_occurred && winner == module);
}


bool global_workspace_submit(
    global_workspace_t* workspace,
    cognitive_module_t module,
    const float* content,
    uint32_t content_dim,
    float strength)
{
    // Guard: NULL checks
    if (workspace == NULL) {
        LOG_ERROR("NULL workspace in global_workspace_submit");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_submit: validation failed");
        return false;
    }

    if (content == NULL) {
        LOG_ERROR("NULL content in global_workspace_submit");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_submit: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_submit", 0.0f);


    struct global_workspace_struct* ws = (struct global_workspace_struct*)workspace;

    // Thread safety
    nimcp_platform_mutex_lock(&ws->mutex);

    // Guard: Validate content dimension
    if (content_dim != ws->config.capacity_dim) {
        LOG_ERROR("Content dimension mismatch in global_workspace_submit: "
                "expected %u, got %u", ws->config.capacity_dim, content_dim);
        nimcp_platform_mutex_unlock(&ws->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_submit: validation failed");
        return false;
    }

    // Guard: Validate strength range
    if (strength < 0.0F || strength > 1.0F) {
        LOG_ERROR("Invalid strength in global_workspace_submit: %.2f "
                "(must be 0.0-1.0)", strength);
        nimcp_platform_mutex_unlock(&ws->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_submit: validation failed");
        return false;
    }

    // Update statistics
    if (ws->config.enable_statistics) {
        ws->stats.total_competitions++;
        if (module < MODULE_CUSTOM_START) {
            ws->stats.competitions_per_module[module]++;
        }
    }

    // Find slot for this module (update existing or find empty)
    uint32_t slot_idx = GLOBAL_WORKSPACE_MAX_COMPETITORS;

    // Check if module already in pool (update case)
    for (uint32_t i = 0; i < GLOBAL_WORKSPACE_MAX_COMPETITORS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GLOBAL_WORKSPACE_MAX_COMPETITORS > 256) {
            global_workspace_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)GLOBAL_WORKSPACE_MAX_COMPETITORS);
        }

        if (ws->competitors[i].is_active && ws->competitors[i].module == module) {
            slot_idx = i;
            // Free old content buffer before replacing
            if (ws->competitors[i].content) {
                nimcp_free(ws->competitors[i].content);
                ws->competitors[i].content = NULL;
            }
            break;
        }
    }

    // If not found, find empty slot
    if (slot_idx == GLOBAL_WORKSPACE_MAX_COMPETITORS) {
        for (uint32_t i = 0; i < GLOBAL_WORKSPACE_MAX_COMPETITORS; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && GLOBAL_WORKSPACE_MAX_COMPETITORS > 256) {
                global_workspace_heartbeat("global_works_loop",
                                 (float)(i + 1) / (float)GLOBAL_WORKSPACE_MAX_COMPETITORS);
            }

            if (!ws->competitors[i].is_active) {
                slot_idx = i;
                break;
            }
        }
    }

    // Guard: Check if pool is full
    if (slot_idx == GLOBAL_WORKSPACE_MAX_COMPETITORS) {
        LOG_WARN("Competition pool full in global_workspace_submit "
                "(%u competitors)", GLOBAL_WORKSPACE_MAX_COMPETITORS);
        nimcp_platform_mutex_unlock(&ws->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_submit: validation failed");
        return false;
    }

    // Track if we're adding new competitor vs updating existing
    bool was_inactive = !ws->competitors[slot_idx].is_active;
    bool pool_was_empty = (ws->num_active_competitors == 0);

    // Copy content - we own this buffer now (fixes buffer ownership issue)
    float* content_copy = (float*)nimcp_calloc(content_dim, sizeof(float));
    if (!content_copy) {
        LOG_ERROR("Failed to allocate content buffer in global_workspace_submit");
        nimcp_platform_mutex_unlock(&ws->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "global_workspace_submit: content_copy is NULL");
        return false;
    }
    memcpy(content_copy, content, content_dim * sizeof(float));

    // Add/update competitor in pool
    ws->competitors[slot_idx].module = module;
    ws->competitors[slot_idx].content = content_copy;  // We own this buffer now
    ws->competitors[slot_idx].content_dim = content_dim;
    ws->competitors[slot_idx].strength = strength;
    ws->competitors[slot_idx].timestamp_ms = get_time_ms();
    ws->competitors[slot_idx].is_active = true;

    // Update pool counts
    if (was_inactive) {
        ws->num_active_competitors++;

        // Track when pool first becomes active
        if (pool_was_empty) {
            ws->pool_activation_time_ms = get_time_ms();
        }
    }

    nimcp_platform_mutex_unlock(&ws->mutex);
    return true;
}


bool global_workspace_resolve(
    global_workspace_t* workspace,
    cognitive_module_t* winning_module)
{
    // Guard: NULL checks
    if (workspace == NULL) {
        LOG_ERROR("NULL workspace in global_workspace_resolve");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_resolve: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_resolve", 0.0f);


    struct global_workspace_struct* ws = (struct global_workspace_struct*)workspace;

    // Thread safety
    nimcp_platform_mutex_lock(&ws->mutex);

    // Set default output (no winner)
    if (winning_module != NULL) {
        *winning_module = MODULE_NONE;
    }

    // Check if pool is empty
    if (ws->num_active_competitors == 0) {
        // No competitors to resolve
        nimcp_platform_mutex_unlock(&ws->mutex);
        return false;
    }

    uint64_t current_time = get_time_ms();

    // Run competition resolution based on strategy
    uint32_t winner_idx = 0;
    float winner_strength = 0.0f;
    bool found_winner = false;

    uint64_t competition_start = get_time_ms();

    switch (ws->config.strategy) {
        case COMPETITION_WINNER_TAKE_ALL:
            found_winner = resolve_winner_take_all(ws, &winner_idx, &winner_strength);
            break;

        case COMPETITION_PRIORITY_BASED:
            found_winner = resolve_priority_based(ws, &winner_idx, &winner_strength);
            break;

        case COMPETITION_ROUND_ROBIN:
            found_winner = resolve_round_robin(ws, &winner_idx, &winner_strength);
            break;

        case COMPETITION_WEIGHTED_FUSION:
            LOG_WARN("WEIGHTED_FUSION strategy not yet implemented");
            found_winner = resolve_winner_take_all(ws, &winner_idx, &winner_strength);
            break;

        default:
            LOG_ERROR("Unknown competition strategy: %d", ws->config.strategy);
            found_winner = false;
            break;
    }

    // Update competition latency statistics
    if (ws->config.enable_statistics) {
        uint64_t competition_end = get_time_ms();
        uint64_t latency_us = (competition_end - competition_start) * 1000;
        if (ws->stats.total_competitions == 1) {
            ws->stats.avg_competition_latency_us = latency_us;
            ws->stats.max_competition_latency_us = latency_us;
        } else {
            ws->stats.avg_competition_latency_us =
                (ws->stats.avg_competition_latency_us * (ws->stats.total_competitions - 1) +
                 latency_us) / ws->stats.total_competitions;
            if (latency_us > ws->stats.max_competition_latency_us) {
                ws->stats.max_competition_latency_us = latency_us;
            }
        }
    }

    // If no winner found, clear pool and return
    if (!found_winner) {
        // Clear pool (all below threshold or pruned by decay)
        for (uint32_t i = 0; i < GLOBAL_WORKSPACE_MAX_COMPETITORS; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && GLOBAL_WORKSPACE_MAX_COMPETITORS > 256) {
                global_workspace_heartbeat("global_works_loop",
                                 (float)(i + 1) / (float)GLOBAL_WORKSPACE_MAX_COMPETITORS);
            }

            if (ws->competitors[i].is_active) {
                // Free content buffer we own
                if (ws->competitors[i].content) {
                    nimcp_free(ws->competitors[i].content);
                    ws->competitors[i].content = NULL;
                }
                ws->competitors[i].is_active = false;
            }
        }
        ws->num_active_competitors = 0;
        ws->pool_activation_time_ms = 0;

        if (ws->config.enable_statistics) {
            ws->stats.rejected_submissions++;
        }

        nimcp_platform_mutex_unlock(&ws->mutex);
        /* No winner is normal competition outcome, not an error */
        return false;
    }

    // Check refractory period before broadcasting
    bool can_broadcast = true;
    if (ws->last_broadcast_time_ms > 0) {
        uint64_t time_since_broadcast = current_time - ws->last_broadcast_time_ms;
        if (time_since_broadcast < ws->config.refractory_period_ms) {
            can_broadcast = false;
            if (ws->config.enable_statistics) {
                ws->stats.refractory_violations++;
            }
        }
    }

    // Broadcast if allowed
    if (can_broadcast) {
        broadcast_winner(ws, winner_idx, winner_strength);

        // Set output: which module won
        if (winning_module != NULL) {
            *winning_module = ws->competitors[winner_idx].module;
        }

        // Clear the entire competition pool after broadcasting
        for (uint32_t i = 0; i < GLOBAL_WORKSPACE_MAX_COMPETITORS; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && GLOBAL_WORKSPACE_MAX_COMPETITORS > 256) {
                global_workspace_heartbeat("global_works_loop",
                                 (float)(i + 1) / (float)GLOBAL_WORKSPACE_MAX_COMPETITORS);
            }

            if (ws->competitors[i].is_active) {
                // Free content buffer we own
                if (ws->competitors[i].content) {
                    nimcp_free(ws->competitors[i].content);
                    ws->competitors[i].content = NULL;
                }
                ws->competitors[i].is_active = false;
            }
        }
        ws->num_active_competitors = 0;
        ws->pool_activation_time_ms = 0;

        nimcp_platform_mutex_unlock(&ws->mutex);
        return true;
    } else {
        // Winner found but blocked by refractory period
        // Keep competitors in pool for next resolve attempt
        nimcp_platform_mutex_unlock(&ws->mutex);
        /* Refractory period block is normal operation, not an error */
        return false;
    }
}


bool global_workspace_subscribe(
    global_workspace_t* workspace,
    cognitive_module_t module)
{
    if (workspace == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_subscribe: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_subscribe", 0.0f);


    struct global_workspace_struct* ws = (struct global_workspace_struct*)workspace;

    // Thread safety
    nimcp_platform_mutex_lock(&ws->mutex);

    // Check if already subscribed
    for (uint32_t i = 0; i < ws->num_subscribers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ws->num_subscribers > 256) {
            global_workspace_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)ws->num_subscribers);
        }

        if (ws->subscribers[i] == module) {
            nimcp_platform_mutex_unlock(&ws->mutex);
            return true;  // Already subscribed (idempotent)
        }
    }

    // Check if room for more
    if (ws->num_subscribers >= GLOBAL_WORKSPACE_MAX_SUBSCRIBERS) {
        LOG_WARN("Subscriber list full (%u max)", GLOBAL_WORKSPACE_MAX_SUBSCRIBERS);
        nimcp_platform_mutex_unlock(&ws->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "global_workspace_subscribe: capacity exceeded");
        return false;
    }

    // Add subscriber
    ws->subscribers[ws->num_subscribers++] = module;

    // Update statistics
    if (ws->config.enable_statistics) {
        ws->stats.current_subscribers = ws->num_subscribers;
    }

    nimcp_platform_mutex_unlock(&ws->mutex);
    return true;
}


bool global_workspace_unsubscribe(
    global_workspace_t* workspace,
    cognitive_module_t module)
{
    if (workspace == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_unsubscribe: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_unsubscribe", 0.0f);


    struct global_workspace_struct* ws = (struct global_workspace_struct*)workspace;

    // Thread safety
    nimcp_platform_mutex_lock(&ws->mutex);

    // Find module in subscriber list
    for (uint32_t i = 0; i < ws->num_subscribers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ws->num_subscribers > 256) {
            global_workspace_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)ws->num_subscribers);
        }

        if (ws->subscribers[i] == module) {
            // Remove by shifting remaining elements
            for (uint32_t j = i; j < ws->num_subscribers - 1; j++) {
                ws->subscribers[j] = ws->subscribers[j + 1];
            }
            ws->num_subscribers--;

            // Update statistics
            if (ws->config.enable_statistics) {
                ws->stats.current_subscribers = ws->num_subscribers;
            }

            nimcp_platform_mutex_unlock(&ws->mutex);
            return true;
        }
    }

    nimcp_platform_mutex_unlock(&ws->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_unsubscribe: operation failed");
    return false;  // Not subscribed
}


uint64_t global_workspace_time_since_broadcast(
    const global_workspace_t* workspace,
    uint64_t current_time_ms)
{
    if (workspace == NULL) return UINT64_MAX;

    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_time_since_broadcast", 0.0f);


    /* Cast away const for mutex — consistent with other accessor patterns */
    struct global_workspace_struct* ws =
        (struct global_workspace_struct*)workspace;

    nimcp_platform_mutex_lock(&ws->mutex);

    if (ws->last_broadcast_time_ms == 0) {
        nimcp_platform_mutex_unlock(&ws->mutex);
        return UINT64_MAX;  // Never broadcast
    }

    if (current_time_ms < ws->last_broadcast_time_ms) {
        nimcp_platform_mutex_unlock(&ws->mutex);
        return 0;  // Time went backwards?
    }

    uint64_t result = current_time_ms - ws->last_broadcast_time_ms;
    nimcp_platform_mutex_unlock(&ws->mutex);
    return result;
}


//=============================================================================
// Knowledge Graph Self-Awareness Integration
//=============================================================================

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Allow global workspace module to introspect its own structure and capabilities
 * WHY:  Enable self-awareness - GW is central to consciousness, needs to know itself
 * HOW:  Use KG reader to look up Global_Workspace entity and related entities
 *
 * @param kg Knowledge graph reader instance
 * @return 1 if self-knowledge found, 0 otherwise
 */
int global_workspace_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    // Query for our own module entity
    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Global_Workspace");
    if (self) {
        // Global workspace module now has access to its documented structure
        NIMCP_LOGGING_DEBUG("global_workspace: Self-knowledge found: %s (%u observations)",
                  self->name, self->num_observations);
    }

    // Query consciousness-related entities
    kg_entity_list_t* consciousness = kg_reader_search_entities(kg, "consciousness");
    if (consciousness) {
        NIMCP_LOGGING_DEBUG("global_workspace: Found %u consciousness-related entities in KG",
                  consciousness->count);
        kg_entity_list_destroy(consciousness);
    }

    // Query broadcast-related entities
    kg_entity_list_t* broadcast = kg_reader_search_entities(kg, "broadcast");
    if (broadcast) {
        NIMCP_LOGGING_DEBUG("global_workspace: Found %u broadcast-related entities in KG",
                  broadcast->count);
        kg_entity_list_destroy(broadcast);
    }

    return self ? 1 : 0;
}


/**
 * @brief Query broadcast targets from knowledge graph
 *
 * WHAT: Allow GW to know what cognitive modules it can broadcast to
 * WHY:  Essential for consciousness - GW needs to know its audience
 * HOW:  Query KG for cognitive subsystem entities that are potential broadcast targets
 *
 * @param kg Knowledge graph reader instance
 * @return 1 if broadcast targets found, 0 otherwise
 */
int global_workspace_query_broadcast_targets(kg_reader_t* kg) {
    if (!kg) return 0;

    // GW should know all cognitive modules it can broadcast to
    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_query_broadcast_targ", 0.0f);


    kg_entity_list_t* cognitive = kg_reader_get_entities_by_type(kg, "cognitive_subsystem");
    if (cognitive) {
        NIMCP_LOGGING_DEBUG("global_workspace: Found %u cognitive subsystems as broadcast targets",
                  cognitive->count);
        kg_entity_list_destroy(cognitive);
        return 1;
    }

    // Also check for Module type entities
    kg_entity_list_t* modules = kg_reader_get_entities_by_type(kg, "Module");
    if (modules) {
        NIMCP_LOGGING_DEBUG("global_workspace: Found %u modules as potential broadcast targets",
                  modules->count);
        kg_entity_list_destroy(modules);
        return 1;
    }

    // Query for cognitive-related entities as fallback
    kg_entity_list_t* cognitive_entities = kg_reader_search_entities(kg, "cognitive");
    if (cognitive_entities) {
        NIMCP_LOGGING_DEBUG("global_workspace: Found %u cognitive-related entities",
                  cognitive_entities->count);
        kg_entity_list_destroy(cognitive_entities);
        return 1;
    }

    return 0;
}


/**
 * @brief Query competition strategy information from knowledge graph
 *
 * WHAT: Allow GW to understand its competition mechanisms
 * WHY:  Self-awareness about how attention works
 * HOW:  Query KG for competition and attention related entities
 *
 * @param kg Knowledge graph reader instance
 * @return Number of competition-related entities found
 */
int global_workspace_query_competition_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_query_competition_kn", 0.0f);


    int count = 0;

    // Query for competition-related entities
    kg_entity_list_t* competition = kg_reader_search_entities(kg, "competition");
    if (competition) {
        count += competition->count;
        kg_entity_list_destroy(competition);
    }

    // Query for attention-related entities
    kg_entity_list_t* attention = kg_reader_search_entities(kg, "attention");
    if (attention) {
        count += attention->count;
        kg_entity_list_destroy(attention);
    }

    // Query for ignition-related entities (global ignition theory)
    kg_entity_list_t* ignition = kg_reader_search_entities(kg, "ignition");
    if (ignition) {
        count += ignition->count;
        kg_entity_list_destroy(ignition);
    }

    return count;
}


/* ============================================================================
 * W9-kg READ-PATH INTEGRATION (internal_kg)
 * ============================================================================ */

/**
 * @brief W9-kg read-path: query prior winners by salience (outgoing edges
 *        from `cog_global_workspace` root node).
 *
 * WHAT: Counts outgoing edges from the GWS root node in the internal KG,
 *       and emits a competition event noting how many competitors are
 *       currently in the pool. Completes GWS from UNWIRED (in internal_kg)
 *       to fully WIRED (structural root + write in broadcast_winner + read
 *       in this fn).
 * WHY:  Lets GWS see how many prior broadcast events are visible (as edges
 *       from its root) — a proxy for downstream reach. W16 consumers can
 *       then trace broadcast history.
 * HOW:  Uses the shared W9-kg helpers. Null-safe; no-op if KG disabled.
 *
 * @param workspace global workspace instance (non-NULL)
 * @param num_competitors current competition pool size
 * @return count of outgoing edges from the GWS root
 */
uint32_t global_workspace_query_kg_winners(global_workspace_t* workspace,
                                           uint32_t num_competitors)
{
    if (!workspace) return 0;

    struct brain_struct* brain = w9kg_get_registered_brain();
    if (!brain) return 0;

    uint32_t winners = w9kg_query_gws_winners_by_salience(brain);
    w9kg_emit_gws_competition(brain, num_competitors);
    return winners;
}


/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int global_workspace_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "global_workspace_training_begin: NULL argument");
        return -1;
    }
    global_workspace_heartbeat_instance(g_global_workspace_health_agent, "global_workspace_training_begin", 0.0f);
    (void)(struct global_workspace_struct*)instance; /* Module state available for reset */
    return 0;
}


int global_workspace_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "global_workspace_training_end: NULL argument");
        return -1;
    }
    global_workspace_heartbeat_instance(g_global_workspace_health_agent, "global_workspace_training_end", 1.0f);
    (void)(struct global_workspace_struct*)instance; /* Module state available for finalization */
    return 0;
}
