// nimcp_swarm_consciousness_part_processing.c - processing functions
// Part of nimcp_swarm_consciousness.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_swarm_consciousness.c


/**
 * WHAT: Publish consciousness update via bio-async
 * WHY:  Notify other modules of consciousness changes
 * HOW:  Send message through bio-async system
 */
static void publish_consciousness_update(swarm_consciousness_ctx_t* ctx,
                                        const swarm_consciousness_metrics_t* metrics)
{
    if (!ctx || !metrics || !ctx->bio_async_registered) {
        return;
    }

    // In real implementation, would use bio-async message publishing
    // This is a placeholder
    LOG_DEBUG("Publishing consciousness update: phi=%.3f state=%s",
                   metrics->collective_phi,
                   swarm_consciousness_state_name(metrics->consciousness_state));
}


//=============================================================================
// Imagination Engine Integration
//=============================================================================
// BIOLOGICAL BASIS: Collective imagination enables distributed creativity
// across the swarm, similar to cultural transmission of imaginative content
// in social species. Individual nodes can share novel scenarios and insights,
// enabling emergent collective creativity beyond individual capabilities.
//
// NOTE: bio_msg_imagination_collective_t is defined in nimcp_bio_messages.h

/**
 * WHAT: Handler for incoming collective imagination messages
 * WHY:  Process imagination content from other swarm nodes
 * HOW:  Validate, evaluate relevance, invoke user callback
 */
static nimcp_error_t imagination_collective_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)response_promise;  // No response expected for broadcasts

    swarm_consciousness_ctx_t* ctx = (swarm_consciousness_ctx_t*)user_data;

    // Guard: Validate context
    if (!ctx || ctx->magic != SWARM_CONSCIOUSNESS_MAGIC) {
        return NIMCP_INVALID_PARAM;
    }

    // Guard: Validate message size
    if (!msg || msg_size < sizeof(bio_msg_imagination_collective_t)) {
        LOG_WARN("Invalid imagination collective message size");
        return NIMCP_INVALID_PARAM;
    }

    const bio_msg_imagination_collective_t* imag_msg =
        (const bio_msg_imagination_collective_t*)msg;

    // Log received imagination
    LOG_DEBUG("Received collective imagination from node %lu: scenario=%u, relevance=%.3f",
              (unsigned long)imag_msg->source_node,
              imag_msg->scenario_id,
              imag_msg->relevance);

    // Invoke user callback if registered
    nimcp_mutex_lock(&ctx->lock);
    imagination_collective_receive_callback_t callback = ctx->collective_imagination_callback;
    void* callback_data = ctx->collective_imagination_user_data;
    nimcp_mutex_unlock(&ctx->lock);

    if (callback) {
        // Note: In full implementation, would reconstruct imagination_scenario_t
        // from message payload. For now, we pass NULL scenario with metadata.
        callback(NULL,  // scenario pointer (would be reconstructed)
                 imag_msg->source_node,
                 imag_msg->relevance,
                 callback_data);
    }

    // BBB audit for collective imagination
    bbb_audit_log(BBB_AUDIT_INFO, "swarm_consciousness", "imagination_received",
                  "source=%lu scenario=%u relevance=%.3f",
                  (unsigned long)imag_msg->source_node,
                  imag_msg->scenario_id,
                  imag_msg->relevance);

    return NIMCP_SUCCESS;
}


/**
 * WHAT: Register handler for collective imagination sharing
 * WHY:  Enable reception of imagination from other swarm nodes
 * HOW:  Register bio-async handler for BIO_MSG_IMAGINATION_COLLECTIVE_SHARE
 *
 * BIOLOGICAL BASIS: Establishes neural pathways for receiving and
 * processing culturally transmitted imaginative content from the swarm.
 *
 * @param ctx Swarm consciousness context
 * @param callback Callback for received imagination
 * @param user_data User data passed to callback
 * @return 0 on success, -1 on error
 */
int swarm_consciousness_register_imagination_handler(
    swarm_consciousness_ctx_t* ctx,
    imagination_collective_receive_callback_t callback,
    void* user_data)
{
    // Guard: Validate context
    if (!ctx) {
        LOG_ERROR("Null context in register_imagination_handler");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Null context in register_imagination_handler");
        return -1;
    }

    if (ctx->magic != SWARM_CONSCIOUSNESS_MAGIC) {
        LOG_ERROR("Invalid context magic in register_imagination_handler");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Invalid context magic in register_imagination_handler");
        return -1;
    }

    nimcp_mutex_lock(&ctx->lock);

    // Store callback
    ctx->collective_imagination_callback = callback;
    ctx->collective_imagination_user_data = user_data;

    // Register bio-async handler if not already done
    if (!ctx->imagination_handler_registered && ctx->bio_async_registered) {
        // Register module with bio-router if not yet registered
        if (!ctx->bio_module_ctx) {
            bio_module_info_t mod_info = {
                .module_id = BIO_MODULE_SWARM_CONSCIOUSNESS,
                .module_name = "swarm_consciousness",
                .inbox_capacity = 0,  // Use default
                .user_data = ctx
            };
            ctx->bio_module_ctx = bio_router_register_module(&mod_info);
            if (!ctx->bio_module_ctx) {
                LOG_WARN("Failed to register swarm_consciousness with bio-router");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to register swarm_consciousness with bio-router");
                nimcp_mutex_unlock(&ctx->lock);
                return -1;
            }
        }

        /* Register handlers via KG-driven wiring callback */
        nimcp_error_t err = bio_router_register_wiring_callback(
            BIO_MODULE_SWARM_CONSCIOUSNESS,
            (void*)swarm_consciousness_handler_callback,
            ctx
        );

        if (err != NIMCP_SUCCESS) {
            /* Legacy fallback: direct handler registration */
            LOG_DEBUG("KG wiring unavailable, using legacy registration");
            err = LEGACY_HANDLER_REGISTRATION(bio_router_register_handler(
                ctx->bio_module_ctx,
                BIO_MSG_IMAGINATION_COLLECTIVE_SHARE,
                imagination_collective_handler));

            if (err != NIMCP_SUCCESS) {
                LOG_WARN("Failed to register imagination handler: error=%d", err);
                nimcp_mutex_unlock(&ctx->lock);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_register_imagination_handler: validation failed");
                return -1;
            }

            // Also register for collective insight messages
            LEGACY_HANDLER_REGISTRATION(bio_router_register_handler(
                ctx->bio_module_ctx,
                BIO_MSG_IMAGINATION_COLLECTIVE_INSIGHT,
                imagination_collective_handler));
        }

        ctx->imagination_handler_registered = true;
        LOG_INFO("Registered imagination collective handler");
    }

    nimcp_mutex_unlock(&ctx->lock);

    bbb_audit_log(BBB_AUDIT_INFO, "swarm_consciousness", "imagination_handler_registered",
                  "callback=%p", (void*)callback);

    return 0;
}
