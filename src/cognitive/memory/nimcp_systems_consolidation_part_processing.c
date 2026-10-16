// nimcp_systems_consolidation_part_processing.c - processing functions
// Part of nimcp_systems_consolidation.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_systems_consolidation.c


/*=============================================================================
 * KG-Driven Wiring Callback
 *============================================================================*/

/**
 * @brief KG-driven wiring handler callback
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 */
static int systems_consolidation_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    LOG_INFO(LOG_MODULE, "systems_consolidation_wiring_handler_callback: registering %u handlers from KG",
             message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            systems_consolidation_heartbeat("consolidatio_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_CONSOLIDATION_TRIGGER:
                bio_router_register_handler(ctx, message_types[i], handle_consolidation_trigger);
                LOG_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_CONSOLIDATION_TRIGGER");
                break;

            default:
                LOG_DEBUG(LOG_MODULE, "  Unknown message type %u - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}


uint32_t systems_consolidation_execute_replays(
    systems_consolidation_system_t* system,
    float time_delta_seconds,
    bool is_sws,
    bool is_rem)
{
    // WHAT: Guard against invalid input
    if (!system || time_delta_seconds <= 0.0F) {
        return 0;
    }

    /* Phase 8: Send heartbeat at start of replay execution */
    systems_consolidation_heartbeat("consolidation_replays", 0.0f);

    // WHAT: Determine replay rate based on sleep state
    // WHY: SWS has highest replay rate (Born & Wilhelm, 2012)
    float replay_rate_hz = 0.0f;
    if (is_sws) {
        replay_rate_hz = CONSOLIDATION_REPLAY_FREQUENCY_SWS;  // 10 Hz in SWS
    } else if (is_rem) {
        replay_rate_hz = CONSOLIDATION_REPLAY_FREQUENCY_SWS * 0.5F;  // 5 Hz in REM
    } else {
        replay_rate_hz = 0.1F;  // Minimal awake replay
    }

    // WHAT: Calculate number of replays to execute this cycle
    // WHY: Replay frequency determines consolidation speed
    float replays_to_execute_float = replay_rate_hz * time_delta_seconds;
    uint32_t replays_to_execute = (uint32_t)replays_to_execute_float;

    // WHAT: Cap by queue size
    if (replays_to_execute > system->replay_queue_size) {
        replays_to_execute = system->replay_queue_size;
    }

    // WHAT: Execute replays
    uint32_t executed_count = 0;
    for (uint32_t i = 0; i < replays_to_execute && i < system->replay_queue_size; i++) {
        /* Phase 8: Send progress heartbeat for long replay sequences */
        if (replays_to_execute > 0) {
            systems_consolidation_heartbeat("consolidation_replays", (float)i / (float)replays_to_execute);
        }
        replay_event_t* event = &system->replay_queue[i];

        // WHAT: Transfer engram to cortex
        // WHY: Replay drives cortical plasticity
        float replay_strength = is_sws ? 0.8F : (is_rem ? 0.5F : 0.1F);
        uint64_t cortical_node_id = systems_consolidation_transfer_to_cortex(
            system,
            event->engram_id,
            replay_strength
        );

        if (cortical_node_id != 0) {
            event->is_completed = true;
            executed_count++;
            system->total_replays++;
        }
    }

    // WHAT: Remove completed events from queue
    // WHY: Keep queue compact
    uint32_t write_idx = 0;
    for (uint32_t read_idx = 0; read_idx < system->replay_queue_size; read_idx++) {
        /* Phase 8: Loop progress heartbeat */
        if ((read_idx & 0xFF) == 0 && system->replay_queue_size > 256) {
            systems_consolidation_heartbeat("consolidatio_loop",
                             (float)(read_idx + 1) / (float)system->replay_queue_size);
        }

        if (!system->replay_queue[read_idx].is_completed) {
            system->replay_queue[write_idx] = system->replay_queue[read_idx];
            write_idx++;
        }
    }
    system->replay_queue_size = write_idx;

    system->last_replay_time_ms = nimcp_platform_time_monotonic_ms();
    return executed_count;
}


void systems_consolidation_update(
    systems_consolidation_system_t* system,
    float time_delta_seconds,
    bool is_sleeping)
{
    // WHAT: Guard against invalid input
    if (!system || time_delta_seconds <= 0.0F) {
        return;
    }

    /* Phase 8: Send heartbeat at start of consolidation update */
    systems_consolidation_heartbeat("consolidation_update", 0.0f);

    // Process pending bio-async messages
    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_process_inbox(system->bio_ctx, 5);
    }

    // WHAT: Determine consolidation rate based on sleep state
    // WHY: Sleep accelerates consolidation (Born & Wilhelm, 2012)
    float consolidation_rate = is_sleeping
        ? CONSOLIDATION_TRANSFER_RATE_SWS
        : CONSOLIDATION_TRANSFER_RATE_AWAKE;

    float time_delta_hours = time_delta_seconds / 3600.0F;
    float consolidation_increment = consolidation_rate * time_delta_hours;
    float forgetting_decrement = system->forgetting_rate * time_delta_hours;

    // WHAT: Update all cortical nodes
    for (uint32_t i = 0; i < system->node_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->node_count > 256) {
            systems_consolidation_heartbeat("consolidatio_loop",
                             (float)(i + 1) / (float)system->node_count);
        }

        cortical_memory_node_t* node = system->cortical_nodes[i];

        // WHAT: Increase consolidation strength
        // WHY: Gradual strengthening over time
        node->consolidation_strength += consolidation_increment;
        if (node->consolidation_strength > 1.0F) {
            node->consolidation_strength = 1.0F;
        }

        // WHAT: Decrease hippocampal dependency
        // WHY: Cortex becomes independent over time
        node->hippocampal_dependency -= consolidation_increment;
        if (node->hippocampal_dependency < 0.0F) {
            node->hippocampal_dependency = 0.0F;
            if (!node->is_transferred) {
                node->is_transferred = true;
                system->total_transfers++;
            }
        }

        // WHAT: Check for episodic → semantic transition
        // WHY: Details fade, gist remains (Winocur & Moscovitch, 2011)
        if (node->consolidation_strength >= system->semantic_threshold &&
            node->type == CORTICAL_MEMORY_EPISODIC) {
            node->type = CORTICAL_MEMORY_SEMANTIC;
        }

        // WHAT: Apply forgetting to unrehearsed memories
        // WHY: Memories decay without rehearsal (Ebbinghaus forgetting curve)
        uint64_t time_since_activation = nimcp_platform_time_monotonic_ms() - node->last_activation_ms;
        if (time_since_activation > 3600000) {  // >1 hour
            node->consolidation_strength -= forgetting_decrement;
            if (node->consolidation_strength < 0.0F) {
                node->consolidation_strength = 0.0F;
            }
        }
    }
}


int systems_consolidation_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "systems_consolidation_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    systems_consolidation_heartbeat_instance(NULL, "systems_consolidation_training_step", progress);
    return 0;
}
