// nimcp_ethics_part_processing.c - processing functions
// Part of nimcp_ethics.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_ethics.c


//=============================================================================
// KG-Driven Wiring Callback (Phase 2: KG-Based Runtime Module Assembly)
//=============================================================================

/**
 * @brief KG-driven wiring handler callback
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Ethics engine pointer
 * @return 0 on success, -1 on error
 */
static int ethics_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    LOG_INFO("ethics_wiring_handler_callback: registering %u handlers from KG",
             message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            ethics_heartbeat("ethics_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_ETHICS_EVALUATION_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_ethics_request);
                LOG_DEBUG("  Registered handler for BIO_MSG_ETHICS_EVALUATION_REQUEST");
                break;

            /* Add additional message types as wiring is discovered from KG */
            default:
                LOG_DEBUG("  Unknown message type %u - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}


int ethics_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ethics_heartbeat_instance(NULL, "ethics_training_step", progress);
    return 0;
}
