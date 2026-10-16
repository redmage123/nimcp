// nimcp_global_workspace_part_processing.c - processing functions
// Part of nimcp_global_workspace.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_global_workspace.c


/**
 * @brief KG-driven wiring callback for global workspace module
 */
static int global_workspace_wiring_handler_callback(
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
            global_workspace_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_ATTENTION_SHIFT:
                bio_router_register_handler(ctx, message_types[i], handle_attention_shift);
                registered++;
                break;
            default:
                NIMCP_LOGGING_DEBUG("global_workspace: unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    NIMCP_LOGGING_INFO("global_workspace: registered %d handlers via KG wiring", registered);
    return 0;
}


int global_workspace_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "global_workspace_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    global_workspace_heartbeat_instance(g_global_workspace_health_agent, "global_workspace_training_step", progress);
    (void)(struct global_workspace_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
