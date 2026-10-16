// nimcp_bio_router_part_processing.c - processing functions
// Part of nimcp_bio_router.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_bio_router.c


nimcp_error_t bio_router_register_handler(bio_module_context_t ctx,
                                           bio_message_type_t msg_type,
                                           bio_message_handler_t handler) {
    NIMCP_CHECK_THROW(ctx && ctx->magic == BIO_MODULE_MAGIC && handler,
                      NIMCP_ERROR_INVALID_PARAM,
                      "bio_router_register_handler: invalid context or handler");

    bio_module_entry_t* entry = ctx->entry;
    NIMCP_CHECK_THROW(entry && entry->magic == BIO_MODULE_MAGIC,
                      NIMCP_ERROR_INVALID_STATE,
                      "bio_router_register_handler: invalid module entry");

    /* G5 SECURITY: Access control on handler registration
     * WHAT: Validate module ID and handler are legitimate before registration
     * WHY:  Prevent unauthorized modules from registering handlers for
     *       message types they should not intercept. Without this, any code
     *       with a bio_module_context could hijack another module's messages.
     * HOW:  Basic validation: module_id must be non-zero, handler must not be NULL,
     *       and module must be in the global router table.
     * TODO: Full RBAC — maintain per-module capability bitmask for allowed msg types. */
    if (entry->module_id == 0) {
        LOG_WARN("bio_router: rejecting handler registration from module_id=0 (uninitialized)");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_PERMISSION_DENIED,
                          "bio_router_register_handler: module_id 0 is reserved — registration denied");
    }
    if (msg_type == 0) {
        LOG_WARN("bio_router: rejecting handler registration for msg_type=0 (reserved) by module %s",
                 entry->module_name);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM,
                          "bio_router_register_handler: msg_type 0 is reserved — registration denied");
    }

    nimcp_platform_mutex_lock(&entry->handler_mutex);

    if (entry->handler_count >= MAX_HANDLERS_PER_MODULE) {
        nimcp_platform_mutex_unlock(&entry->handler_mutex);
        LOG_ERROR("Handler table full for module %s", entry->module_name);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE,
                          "bio_router_register_handler: handler table full");
    }

    // Check for duplicate - if same handler already registered, succeed silently (idempotent)
    for (uint32_t i = 0; i < entry->handler_count; i++) {
        if (entry->handlers[i].msg_type == msg_type &&
            !entry->handlers[i].is_category_handler) {
            // If same handler function, just succeed (idempotent re-registration)
            if (entry->handlers[i].handler == handler) {
                nimcp_platform_mutex_unlock(&entry->handler_mutex);
                LOG_DEBUG("Handler for message type 0x%04X already registered in module %s (same handler)",
                          msg_type, entry->module_name);
                return NIMCP_SUCCESS;
            }
            // Different handler for same type - this is a conflict
            nimcp_platform_mutex_unlock(&entry->handler_mutex);
            LOG_WARN("Handler for message type 0x%04X already registered in module %s (different handler)",
                     msg_type, entry->module_name);
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_ALREADY_EXISTS,
                              "bio_router_register_handler: duplicate handler for message type");
        }
    }

    // Add handler
    bio_handler_entry_t* h = &entry->handlers[entry->handler_count];
    h->msg_type = msg_type;
    h->category_mask = 0;
    h->handler = handler;
    h->is_category_handler = false;

    entry->handler_count++;

    nimcp_platform_mutex_unlock(&entry->handler_mutex);

    LOG_DEBUG("Registered handler for message type 0x%04X in module %s",
              msg_type, entry->module_name);

    return NIMCP_SUCCESS;
}


nimcp_error_t bio_router_register_category_handler(bio_module_context_t ctx,
                                                     uint32_t category_base,
                                                     bio_message_handler_t handler) {
    NIMCP_CHECK_THROW(ctx && ctx->magic == BIO_MODULE_MAGIC && handler,
                      NIMCP_ERROR_INVALID_PARAM,
                      "bio_router_register_category_handler: invalid context or handler");

    bio_module_entry_t* entry = ctx->entry;
    NIMCP_CHECK_THROW(entry && entry->magic == BIO_MODULE_MAGIC,
                      NIMCP_ERROR_INVALID_STATE,
                      "bio_router_register_category_handler: invalid module entry");

    nimcp_platform_mutex_lock(&entry->handler_mutex);

    if (entry->handler_count >= MAX_HANDLERS_PER_MODULE) {
        nimcp_platform_mutex_unlock(&entry->handler_mutex);
        LOG_ERROR("Handler table full for module %s", entry->module_name);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE,
                          "bio_router_register_category_handler: handler table full");
    }

    // Check for duplicate category handler - if same handler already registered, succeed silently
    for (uint32_t i = 0; i < entry->handler_count; i++) {
        if (entry->handlers[i].msg_type == category_base &&
            entry->handlers[i].is_category_handler) {
            if (entry->handlers[i].handler == handler) {
                nimcp_platform_mutex_unlock(&entry->handler_mutex);
                LOG_DEBUG("Category handler for 0x%04X already registered in module %s (same handler)",
                          category_base, entry->module_name);
                return NIMCP_SUCCESS;
            }
            nimcp_platform_mutex_unlock(&entry->handler_mutex);
            LOG_WARN("Category handler for 0x%04X already registered in module %s (different handler)",
                     category_base, entry->module_name);
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_ALREADY_EXISTS,
                              "bio_router_register_category_handler: duplicate handler");
        }
    }

    // Add category handler
    bio_handler_entry_t* h = &entry->handlers[entry->handler_count];
    h->msg_type = category_base;
    h->category_mask = 0xFF00;  // Match top byte
    h->handler = handler;
    h->is_category_handler = true;

    entry->handler_count++;

    nimcp_platform_mutex_unlock(&entry->handler_mutex);

    LOG_DEBUG("Registered category handler for 0x%04X in module %s",
              category_base, entry->module_name);

    return NIMCP_SUCCESS;
}


nimcp_error_t bio_router_unregister_handler(bio_module_context_t ctx,
                                             bio_message_type_t msg_type) {
    NIMCP_CHECK_THROW(ctx && ctx->magic == BIO_MODULE_MAGIC,
                      NIMCP_ERROR_INVALID_PARAM,
                      "bio_router_unregister_handler: invalid context");

    bio_module_entry_t* entry = ctx->entry;
    NIMCP_CHECK_THROW(entry && entry->magic == BIO_MODULE_MAGIC,
                      NIMCP_ERROR_INVALID_STATE,
                      "bio_router_unregister_handler: invalid module entry");

    nimcp_platform_mutex_lock(&entry->handler_mutex);

    // Find handler for this message type
    bool found = false;
    for (uint32_t i = 0; i < entry->handler_count; i++) {
        if (entry->handlers[i].msg_type == msg_type &&
            !entry->handlers[i].is_category_handler) {
            // Found it - shift remaining handlers down
            for (uint32_t j = i; j < entry->handler_count - 1; j++) {
                entry->handlers[j] = entry->handlers[j + 1];
            }
            entry->handler_count--;
            found = true;
            break;
        }
    }

    nimcp_platform_mutex_unlock(&entry->handler_mutex);

    if (found) {
        LOG_DEBUG("Unregistered handler for message type 0x%04X in module %s",
                  msg_type, entry->module_name);
        return NIMCP_SUCCESS;
    } else {
        LOG_WARN("Handler for message type 0x%04X not found in module %s",
                 msg_type, entry->module_name);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND,
                          "bio_router_unregister_handler: handler not found");
    }
}


nimcp_error_t bio_router_clear_handlers(bio_module_context_t ctx) {
    NIMCP_CHECK_THROW(ctx && ctx->magic == BIO_MODULE_MAGIC,
                      NIMCP_ERROR_INVALID_PARAM,
                      "bio_router_clear_handlers: invalid context");

    bio_module_entry_t* entry = ctx->entry;
    NIMCP_CHECK_THROW(entry && entry->magic == BIO_MODULE_MAGIC,
                      NIMCP_ERROR_INVALID_STATE,
                      "bio_router_clear_handlers: invalid module entry");

    nimcp_platform_mutex_lock(&entry->handler_mutex);

    uint32_t old_count = entry->handler_count;
    entry->handler_count = 0;
    memset(entry->handlers, 0, sizeof(entry->handlers));

    nimcp_platform_mutex_unlock(&entry->handler_mutex);

    LOG_DEBUG("Cleared %u handlers from module %s", old_count, entry->module_name);

    return NIMCP_SUCCESS;
}


/*=============================================================================
 * MESSAGE RECEIVING
 *============================================================================*/

/**
 * WHAT: Find handler for message type
 * WHY:  Dispatch message to appropriate handler
 * HOW:  Search handlers for exact match or category match
 */
static bio_message_handler_t bio_router_find_handler(bio_module_entry_t* entry,
                                                       bio_message_type_t msg_type) {
    nimcp_platform_mutex_lock(&entry->handler_mutex);

    // First try exact match
    for (uint32_t i = 0; i < entry->handler_count; i++) {
        if (!entry->handlers[i].is_category_handler &&
            entry->handlers[i].msg_type == msg_type) {
            bio_message_handler_t handler = entry->handlers[i].handler;
            nimcp_platform_mutex_unlock(&entry->handler_mutex);
            return handler;
        }
    }

    // Try category match
    for (uint32_t i = 0; i < entry->handler_count; i++) {
        if (entry->handlers[i].is_category_handler) {
            uint32_t masked_type = msg_type & entry->handlers[i].category_mask;
            uint32_t masked_handler = entry->handlers[i].msg_type & entry->handlers[i].category_mask;
            if (masked_type == masked_handler) {
                bio_message_handler_t handler = entry->handlers[i].handler;
                nimcp_platform_mutex_unlock(&entry->handler_mutex);
                return handler;
            }
        }
    }

    nimcp_platform_mutex_unlock(&entry->handler_mutex);
    /* No handler found - normal for messages without registered handlers */
    return NULL;
}


uint32_t bio_router_process_inbox(bio_module_context_t ctx, uint32_t max_messages) {
    if (!ctx || ctx->magic != BIO_MODULE_MAGIC) return 0;

    bio_module_entry_t* entry = ctx->entry;
    if (!entry || entry->magic != BIO_MODULE_MAGIC) return 0;

    /* Phase 8: Send heartbeat at start of inbox processing */
    bio_router_heartbeat("process_inbox", 0.0f);

    uint32_t processed = 0;
    uint32_t limit = max_messages > 0 ? max_messages : UINT32_MAX;

    while (processed < limit) {
        /* Phase 8: Send progress heartbeat for batch message processing */
        if (limit != UINT32_MAX && limit > 0) {
            bio_router_heartbeat("process_inbox", (float)processed / (float)limit);
        }
        void* msg_data = NULL;
        size_t msg_size = 0;
        nimcp_bio_promise_t response_promise = NULL;

        // Dequeue next message (non-blocking)
        nimcp_error_t deq_result = bio_msg_queue_dequeue(&entry->inbox,
                                                          &msg_data,
                                                          &msg_size,
                                                          &response_promise,
                                                          0);
        if (deq_result != NIMCP_SUCCESS || !msg_data) {
            break;  // No more messages
        }

        const bio_message_header_t* header = (const bio_message_header_t*)msg_data;

        // Find handler
        bio_message_handler_t handler = bio_router_find_handler(entry, header->type);

        if (handler) {
            /* DEADLOCK PREVENTION: Check for re-entrant message delivery.
             * If this module is already processing a handler (in_handler=true),
             * a synchronous send from the current handler's source back to this
             * module would deadlock. Log a warning but still process (message
             * was already dequeued). */
            if (atomic_load(&entry->in_handler)) {
                LOG_WARN("bio_router: re-entrant message to module %u (%s) while handler "
                         "active (current source=%u, new msg_type=0x%04X) — potential deadlock",
                         entry->module_id, entry->module_name,
                         entry->in_handler_source, header->type);
            }

            // Set re-entrancy guard
            atomic_store(&entry->in_handler, true);
            entry->in_handler_source = header->source_module;

            // Invoke handler
            __atomic_fetch_add(&entry->handler_invocations, 1, __ATOMIC_RELAXED);

            nimcp_error_t handler_result = handler(msg_data, msg_size,
                                                    response_promise,
                                                    entry->user_data);

            // Clear re-entrancy guard
            atomic_store(&entry->in_handler, false);
            entry->in_handler_source = 0;

            if (handler_result != NIMCP_SUCCESS) {
                __atomic_fetch_add(&entry->handler_errors, 1, __ATOMIC_RELAXED);

                nimcp_platform_mutex_lock(&g_router->stats_mutex);
                g_router->stats.handler_errors++;
                nimcp_platform_mutex_unlock(&g_router->stats_mutex);

                LOG_WARN("Handler error for message type 0x%04X in module %s",
                         header->type, entry->module_name);
            }
        } else {
            /* Complete orphaned promise so caller's future doesn't block forever */
            if (response_promise) {
                nimcp_bio_promise_complete(response_promise, NULL);
            }
            LOG_DEBUG("No handler for message type 0x%04X in module %s",
                      header->type, entry->module_name);
        }

        // Free message data (allocated via nimcp_malloc in bio_msg_queue_enqueue)
        nimcp_free(msg_data);

        processed++;
    }

    return processed;
}
