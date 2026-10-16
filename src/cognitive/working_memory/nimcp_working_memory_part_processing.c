// nimcp_working_memory_part_processing.c - processing functions
// Part of nimcp_working_memory.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_working_memory.c


/* ============================================================================
 * KG-Driven Wiring Callback
 * ============================================================================ */

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * Called by the orchestrator with discovered message types from the knowledge graph.
 * Registers handlers based on message types discovered at runtime.
 */
static int working_memory_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_WORKING_MEMORY_STORE:
                bio_router_register_handler(ctx, message_types[i], handle_wm_store_request);
                registered++;
                break;
            case BIO_MSG_WORKING_MEMORY_RETRIEVE:
                bio_router_register_handler(ctx, message_types[i], handle_wm_retrieve_request);
                registered++;
                break;
            case BIO_MSG_NEUROMODULATOR_RELEASE:
                bio_router_register_handler(ctx, message_types[i], handle_wm_store_request);
                registered++;
                break;
            default:
                LOG_DEBUG("Working memory: unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    return (registered > 0) ? 0 : -1;
}


int working_memory_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "working_memory_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    working_memory_heartbeat_instance(NULL, "working_memory_training_step", progress);
    (void)instance;
    return 0;
}
