// nimcp_knowledge_part_processing.c - processing functions
// Part of nimcp_knowledge.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_knowledge.c

static int knowledge_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data)
{
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            knowledge_heartbeat("knowledge_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_KNOWLEDGE_QUERY:
                bio_router_register_handler(ctx, message_types[i], handle_knowledge_query);
                registered++;
                break;
            default:
                LOG_DEBUG(LOG_MODULE, "knowledge: unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_INFO(LOG_MODULE, "Knowledge: registered %d handlers via KG wiring", registered);
    return 0;
}


/**
 * @brief Broadcast knowledge update event via bio-async
 */
static void bio_broadcast_knowledge_update(knowledge_system_t system,
                                           uint32_t concepts_learned,
                                           knowledge_domain_t domain) {
    // Process pending bio-async messages
    if (system && system->bio_async_enabled && system->bio_ctx) {
        bio_router_process_inbox(system->bio_ctx, 5);
    }

    if (!system || !system->bio_async_enabled || !system->bio_ctx) {
        return;
    }

    bio_msg_introspection_response_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_KNOWLEDGE_RESPONSE,
                        bio_module_context_get_id(system->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.query_type = 0;
    msg.confidence = 1.0F;
    msg.matched_pattern_count = concepts_learned;

    bio_router_broadcast(system->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG(LOG_MODULE, "Broadcast knowledge update: %u concepts learned in domain %u",
              concepts_learned, domain);
}


int knowledge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    knowledge_heartbeat_instance(NULL, "knowledge_training_step", progress);
    (void)instance;
    return 0;
}
