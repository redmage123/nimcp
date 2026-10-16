// nimcp_global_workspace_part_io.c - io functions
// Part of nimcp_global_workspace.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_global_workspace.c


bool global_workspace_read_broadcast(
    global_workspace_t* workspace,
    float* content,
    uint32_t max_dim,
    uint32_t* actual_dim,
    cognitive_module_t* source)
{
    if (workspace == NULL || content == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_read_broadcast: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_read_broadcast", 0.0f);


    struct global_workspace_struct* ws = (struct global_workspace_struct*)workspace;

    // Thread safety
    nimcp_platform_mutex_lock(&ws->mutex);

    // Check if broadcast available (no broadcast is normal, not an error)
    if (!ws->current_broadcast.is_valid) {
        nimcp_platform_mutex_unlock(&ws->mutex);
        return false;
    }

    // Check buffer size
    if (max_dim < ws->current_broadcast.content_dim) {
        LOG_ERROR("Buffer too small: need %u, have %u",
                ws->current_broadcast.content_dim, max_dim);
        nimcp_platform_mutex_unlock(&ws->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_read_broadcast: validation failed");
        return false;
    }

    // Copy content
    memcpy(content, ws->current_broadcast.content,
           ws->current_broadcast.content_dim * sizeof(float));

    // Return metadata
    if (actual_dim != NULL) {
        *actual_dim = ws->current_broadcast.content_dim;
    }
    if (source != NULL) {
        *source = ws->current_broadcast.source_module;
    }

    nimcp_platform_mutex_unlock(&ws->mutex);
    return true;
}


void global_workspace_print_state(
    const global_workspace_t* workspace,
    bool verbose)
{
    if (workspace == NULL) {
        fprintf(stderr, "Workspace is NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_print_state", 0.0f);


    /* Cast away const for mutex — debug function, large lock scope is acceptable */
    struct global_workspace_struct* ws =
        (struct global_workspace_struct*)workspace;

    nimcp_platform_mutex_lock(&ws->mutex);

    fprintf(stderr, "=== Global Workspace State ===\n");

    // Current broadcast
    if (ws->current_broadcast.is_valid) {
        fprintf(stderr, "Broadcast: %s (strength=%.2f, dim=%u, id=%u)\n",
                cognitive_module_to_string(ws->current_broadcast.source_module),
                ws->current_broadcast.source_strength,
                ws->current_broadcast.content_dim,
                ws->current_broadcast.broadcast_id);
        fprintf(stderr, "  Time: %lu ms ago\n",
                get_time_ms() - ws->current_broadcast.broadcast_timestamp_ms);
        fprintf(stderr, "  Competitors: %u (runner-up: %.2f)\n",
                ws->current_broadcast.num_competitors,
                ws->current_broadcast.runner_up_strength);

        if (verbose && ws->current_broadcast.content != NULL) {
            fprintf(stderr, "  Content (first 10): ");
            uint32_t print_count = (ws->current_broadcast.content_dim < 10) ?
                                    ws->current_broadcast.content_dim : 10;
            for (uint32_t i = 0; i < print_count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && print_count > 256) {
                    global_workspace_heartbeat("global_works_loop",
                                     (float)(i + 1) / (float)print_count);
                }

                fprintf(stderr, "%.3f ", ws->current_broadcast.content[i]);
            }
            fprintf(stderr, "...\n");
        }
    } else {
        fprintf(stderr, "Broadcast: (none)\n");
    }

    // Competitors
    fprintf(stderr, "Competitors (%u active):\n", ws->num_active_competitors);
    for (uint32_t i = 0; i < GLOBAL_WORKSPACE_MAX_COMPETITORS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GLOBAL_WORKSPACE_MAX_COMPETITORS > 256) {
            global_workspace_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)GLOBAL_WORKSPACE_MAX_COMPETITORS);
        }

        if (ws->competitors[i].is_active) {
            uint64_t age = get_time_ms() - ws->competitors[i].timestamp_ms;
            fprintf(stderr, "  %s (strength=%.2f, age=%lu ms)\n",
                    cognitive_module_to_string(ws->competitors[i].module),
                    ws->competitors[i].strength,
                    age);
        }
    }

    // Subscribers
    fprintf(stderr, "Subscribers (%u):\n", ws->num_subscribers);
    for (uint32_t i = 0; i < ws->num_subscribers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ws->num_subscribers > 256) {
            global_workspace_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)ws->num_subscribers);
        }

        fprintf(stderr, "  %s\n", cognitive_module_to_string(ws->subscribers[i]));
    }

    // Configuration
    fprintf(stderr, "Configuration:\n");
    fprintf(stderr, "  Strategy: %s\n",
            competition_strategy_to_string(ws->config.strategy));
    fprintf(stderr, "  Ignition threshold: %.2f\n", ws->config.ignition_threshold);
    fprintf(stderr, "  Refractory period: %u ms\n", ws->config.refractory_period_ms);
    fprintf(stderr, "  Capacity: %u floats\n", ws->config.capacity_dim);

    // Statistics
    if (ws->config.enable_statistics) {
        fprintf(stderr, "Statistics:\n");
        fprintf(stderr, "  Total broadcasts: %lu\n", ws->stats.total_broadcasts);
        fprintf(stderr, "  Total competitions: %lu\n", ws->stats.total_competitions);
        fprintf(stderr, "  Rejected: %lu\n", ws->stats.rejected_submissions);
        fprintf(stderr, "  Refractory violations: %lu\n", ws->stats.refractory_violations);
        fprintf(stderr, "  Avg competition latency: %lu us\n",
                ws->stats.avg_competition_latency_us);
        fprintf(stderr, "  Max competition latency: %lu us\n",
                ws->stats.max_competition_latency_us);

        if (verbose) {
            fprintf(stderr, "  Per-module broadcasts:\n");
            for (cognitive_module_t m = MODULE_PERCEPTION; m < MODULE_CUSTOM_START; m++) {
                if (ws->stats.broadcasts_per_module[m] > 0) {
                    fprintf(stderr, "    %s: %lu\n",
                            cognitive_module_to_string(m),
                            ws->stats.broadcasts_per_module[m]);
                }
            }
        }
    }

    fprintf(stderr, "==============================\n");

    nimcp_platform_mutex_unlock(&ws->mutex);
}


//=============================================================================
// Utility Functions
//=============================================================================

const char* cognitive_module_to_string(cognitive_module_t module) {
    switch (module) {
        case MODULE_NONE: return "NONE";
        case MODULE_PERCEPTION: return "PERCEPTION";
        case MODULE_WORKING_MEMORY: return "WORKING_MEMORY";
        case MODULE_EXECUTIVE: return "EXECUTIVE";
        case MODULE_THEORY_OF_MIND: return "THEORY_OF_MIND";
        case MODULE_ETHICS: return "ETHICS";
        case MODULE_EPISODIC_MEMORY: return "EPISODIC_MEMORY";
        case MODULE_SEMANTIC_MEMORY: return "SEMANTIC_MEMORY";
        case MODULE_LANGUAGE: return "LANGUAGE";
        case MODULE_EMOTION: return "EMOTION";
        case MODULE_SALIENCE: return "SALIENCE";
        case MODULE_MOTOR: return "MOTOR";
        case MODULE_ATTENTION: return "ATTENTION";
        case MODULE_METACOGNITION: return "METACOGNITION";
        case MODULE_CURIOSITY: return "CURIOSITY";
        case MODULE_INTROSPECTION: return "INTROSPECTION";
        case MODULE_PREDICTIVE: return "PREDICTIVE";
        case MODULE_CONSOLIDATION: return "CONSOLIDATION";
        case MODULE_WELLBEING: return "WELLBEING";
        case MODULE_MENTAL_HEALTH: return "MENTAL_HEALTH";
        case MODULE_GOAL_MOTIVATION: return "GOAL_MOTIVATION";
        case MODULE_COGNITIVE_CONTROL: return "COGNITIVE_CONTROL";
        default:
            // Custom modules in reasonable range
            if (module >= MODULE_CUSTOM_START && module < MODULE_CUSTOM_START + 1000) {
                return "CUSTOM";
            }
            return "UNKNOWN";
    }
}


const char* competition_strategy_to_string(competition_strategy_t strategy) {
    switch (strategy) {
        case COMPETITION_WINNER_TAKE_ALL: return "WINNER_TAKE_ALL";
        case COMPETITION_WEIGHTED_FUSION: return "WEIGHTED_FUSION";
        case COMPETITION_PRIORITY_BASED: return "PRIORITY_BASED";
        case COMPETITION_ROUND_ROBIN: return "ROUND_ROBIN";
        default: return "UNKNOWN";
    }
}
