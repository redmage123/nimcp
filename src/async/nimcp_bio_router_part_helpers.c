// nimcp_bio_router_part_helpers.c - helpers functions
// Part of nimcp_bio_router.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_bio_router.c


/**
 * @brief Thread-safe accessor for brain KG pointer
 *
 * TOCTOU FIX: Use atomic_load for the initialized flag to ensure we read
 * a consistent value. The flag is set atomically in init_router_mutex_once()
 * and cleared atomically in bio_router_shutdown(). The atomic_load with
 * memory_order_acquire ensures the mutex init is visible before we lock it.
 * We also re-check the flag after acquiring the lock to handle the case where
 * shutdown runs between our check and our lock acquisition.
 */
static struct brain_kg* get_router_brain_kg_safe(void) {
    if (!atomic_load_explicit(&g_router_brain_kg_mutex_initialized, memory_order_acquire)) {
        return NULL;  /* Normal pre-init state, not an error */
    }
    nimcp_platform_mutex_lock(&g_router_brain_kg_mutex);
    /* TOCTOU FIX: Re-check after acquiring lock. If shutdown cleared the flag
     * between our check above and the lock acquisition, we must bail out.
     * The mutex is still valid here because shutdown destroys it AFTER setting
     * the flag to false, and we hold the lock so shutdown is blocked. */
    if (!atomic_load_explicit(&g_router_brain_kg_mutex_initialized, memory_order_acquire)) {
        nimcp_platform_mutex_unlock(&g_router_brain_kg_mutex);
        return NULL;
    }
    struct brain_kg* kg = g_router_brain_kg;
    nimcp_platform_mutex_unlock(&g_router_brain_kg_mutex);
    return kg;
}


/**
 * WHAT: One-time initialization of router init mutex
 * WHY:  Fix TOCTOU race condition in bio_router_init
 * HOW:  Called exactly once via pthread_once before any mutex operations
 */
static void init_router_mutex_once(void) {
    nimcp_platform_mutex_init(&g_router_init_mutex, false);
    /* RACE FIX: Only create the brain KG mutex once per process lifetime.
     * Shutdown no longer destroys it (to avoid destroying a mutex while threads
     * are still blocked on it), so we must not re-initialize it on subsequent
     * init cycles. The static bool tracks whether the mutex has ever been created;
     * the atomic flag tracks whether the mutex is currently "open for business". */
    if (!g_router_brain_kg_mutex_created) {
        nimcp_platform_mutex_init(&g_router_brain_kg_mutex, false);
        g_router_brain_kg_mutex_created = true;
    }
    /* TOCTOU FIX: Use atomic store with release ordering to ensure the mutex
     * initialization is visible to all threads before they see initialized=true. */
    atomic_store_explicit(&g_router_brain_kg_mutex_initialized, true, memory_order_release);
}


/**
 * WHAT: Grow message queue capacity
 * WHY:  Dynamically handle traffic spikes without dropping messages
 * HOW:  Double capacity (capped at MAX_INBOX_MESSAGES), linearize ring buffer
 *
 * THREAD SAFETY:
 *   - MUST be called with queue->mutex held by the calling thread
 *   - The mutex ensures atomic visibility of all queue state updates
 *   - All readers (dequeue, count) also acquire the mutex before access
 *   - The grow operation is atomic from the perspective of other threads
 *     because no other thread can access the queue while mutex is held
 *
 * MEMORY ORDERING:
 *   - The mutex unlock after grow provides a release barrier
 *   - The mutex lock before any subsequent access provides an acquire barrier
 *   - This guarantees that all writes (entries, capacity, read_idx, write_idx)
 *     are visible to any thread that later acquires the mutex
 */
static nimcp_error_t bio_msg_queue_grow(bio_msg_queue_t* queue) {
    NIMCP_CHECK_THROW(queue && queue->entries, NIMCP_ERROR_NULL_POINTER,
                      "bio_msg_queue_grow: queue or entries is NULL");

    // DEBUG: Verify mutex is held by attempting trylock (should fail if held)
    // This assertion helps catch incorrect usage during development
    /* BUG-14 note: This assertion is ONLY valid for non-recursive mutexes.
     * If queue->mutex is a recursive (reentrant) mutex, trylock will SUCCEED
     * even when the calling thread already holds the lock (incrementing the
     * recursion count). In that case, this assertion would fire a false positive.
     * The current queue mutexes are non-recursive, so this is correct for now. */
#ifndef NDEBUG
    int trylock_result = nimcp_platform_mutex_trylock(&queue->mutex);
    if (trylock_result == 0) {
        // Trylock succeeded means mutex was NOT held - this is a bug!
        nimcp_platform_mutex_unlock(&queue->mutex);
        LOG_ERROR("bio_msg_queue_grow called without holding mutex!");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE,
                          "bio_msg_queue_grow: called without holding mutex");
    }
    // trylock returning non-zero (EBUSY) is expected - mutex is properly held
#endif

    // Check if already at max capacity — return error, don't THROW (this is normal backpressure)
    if (queue->capacity >= MAX_INBOX_MESSAGES) {
        LOG_DEBUG("Queue at max capacity %u, cannot grow further", MAX_INBOX_MESSAGES);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    // Calculate new capacity (double, but cap at max)
    uint32_t new_capacity = queue->capacity * 2;
    if (new_capacity > MAX_INBOX_MESSAGES) {
        new_capacity = MAX_INBOX_MESSAGES;
    }

    // Allocate new entries array
    bio_msg_queue_entry_t* new_entries = nimcp_calloc(new_capacity, sizeof(bio_msg_queue_entry_t));
    if (!new_entries) {
        LOG_ERROR("Failed to allocate %u queue entries for resize", new_capacity);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NO_MEMORY,
                          "bio_msg_queue_grow: failed to allocate new entries");
    }

    // Copy existing entries, linearizing the ring buffer
    // Old: entries[read_idx], entries[(read_idx+1)%cap], ..., entries[(read_idx+count-1)%cap]
    // New: entries[0], entries[1], ..., entries[count-1]
    for (uint32_t i = 0; i < queue->count; i++) {
        uint32_t old_idx = (queue->read_idx + i) % queue->capacity;
        new_entries[i] = queue->entries[old_idx];
    }

    // Free old entries array
    nimcp_free(queue->entries);

    // Update queue state
    queue->entries = new_entries;
    queue->capacity = new_capacity;
    queue->read_idx = 0;
    queue->write_idx = queue->count;

    LOG_DEBUG("Queue grown to capacity %u (count=%u)", new_capacity, queue->count);
    return NIMCP_SUCCESS;
}


/**
 * WHAT: Enqueue message to queue
 * WHY:  Add message to module inbox
 * HOW:  Blocking if full, copy message data, signal waiters
 *
 * DEADLOCK PREVENTION: Checks shutdown_requested flag after waking from
 * condition variable wait to allow clean shutdown without deadlock.
 */
static nimcp_error_t bio_msg_queue_enqueue(bio_msg_queue_t* queue,
                                            const void* msg,
                                            size_t msg_size,
                                            nimcp_bio_promise_t response_promise,
                                            uint32_t timeout_ms) {
    NIMCP_CHECK_THROW(queue && msg && msg_size > 0, NIMCP_ERROR_INVALID_PARAM,
                      "bio_msg_queue_enqueue: invalid queue, msg, or msg_size");

    /* TOCTOU FIX: Acquire the queue lock FIRST, then check the shutdown flag.
     * The previous code checked shutdown_requested without holding any lock,
     * creating a race where shutdown could begin between the check and the
     * lock acquisition, potentially enqueuing a message during teardown.
     * By checking inside the lock, we ensure atomicity of the check-and-enqueue
     * sequence. The shutdown path acquires queue->mutex before broadcasting
     * wakeups, so holding this lock prevents the shutdown from progressing
     * past our check. */
    nimcp_platform_mutex_lock(&queue->mutex);

    // Check shutdown after acquiring lock (TOCTOU-safe)
    if (g_router && g_router->shutdown_requested) {
        nimcp_platform_mutex_unlock(&queue->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_CANCELLED,
                          "bio_msg_queue_enqueue: shutdown requested");
    }

    // Handle full queue - try to grow or wait
    while (queue->count >= queue->capacity) {
        // DEADLOCK FIX: Check shutdown flag after waking from wait
        if (g_router && g_router->shutdown_requested) {
            nimcp_platform_mutex_unlock(&queue->mutex);
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_CANCELLED,
                              "bio_msg_queue_enqueue: shutdown requested during wait");
        }

        if (timeout_ms == 0) {
            // Non-blocking mode: try to grow the queue instead of failing
            if (bio_msg_queue_grow(queue) == NIMCP_SUCCESS) {
                // Successfully grew, can now enqueue
                break;
            }
            // Growth failed (at max capacity) — return error, don't THROW (normal backpressure)
            nimcp_platform_mutex_unlock(&queue->mutex);
            return NIMCP_ERROR_QUEUE_FULL;
        }

        // Blocking mode: wait for space
        int wait_result = nimcp_platform_cond_timedwait(&queue->not_full,
                                                          &queue->mutex,
                                                          timeout_ms);

        // DEADLOCK FIX: Check shutdown after waking from condition wait
        if (g_router && g_router->shutdown_requested) {
            nimcp_platform_mutex_unlock(&queue->mutex);
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_CANCELLED,
                              "bio_msg_queue_enqueue: shutdown requested after wait");
        }

        if (wait_result != 0) {
            // Timeout: try to grow as last resort
            if (bio_msg_queue_grow(queue) == NIMCP_SUCCESS) {
                break;
            }
            nimcp_platform_mutex_unlock(&queue->mutex);
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_TIMEOUT,
                              "bio_msg_queue_enqueue: timeout waiting for space");
        }

        // Loop will re-check condition to handle spurious wakeups
    }

    // Copy message data
    void* msg_copy = nimcp_malloc(msg_size);
    if (!msg_copy) {
        nimcp_platform_mutex_unlock(&queue->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NO_MEMORY,
                          "bio_msg_queue_enqueue: failed to allocate message copy");
    }
    memcpy(msg_copy, msg, msg_size);

    // Add to queue
    bio_msg_queue_entry_t* entry = &queue->entries[queue->write_idx];
    entry->msg_data = msg_copy;
    entry->msg_size = msg_size;
    entry->response_promise = response_promise;
    entry->enqueue_time_us = nimcp_platform_time_monotonic_us();

    queue->write_idx = (queue->write_idx + 1) % queue->capacity;
    queue->count++;

    // Signal waiting consumers
    nimcp_platform_cond_signal(&queue->not_empty);

    nimcp_platform_mutex_unlock(&queue->mutex);
    return NIMCP_SUCCESS;
}


/**
 * WHAT: Dequeue message from queue
 * WHY:  Process next message from inbox
 * HOW:  Blocking if empty, return message data
 *
 * DEADLOCK PREVENTION: Checks shutdown_requested flag after waking from
 * condition variable wait to allow clean shutdown without deadlock.
 */
static nimcp_error_t bio_msg_queue_dequeue(bio_msg_queue_t* queue,
                                            void** out_msg,
                                            size_t* out_size,
                                            nimcp_bio_promise_t* out_promise,
                                            uint32_t timeout_ms) {
    NIMCP_CHECK_THROW(queue && out_msg && out_size, NIMCP_ERROR_INVALID_PARAM,
                      "bio_msg_queue_dequeue: invalid queue or output params");

    /* TOCTOU FIX: Acquire the queue lock FIRST, then check the shutdown flag.
     * Same fix as bio_msg_queue_enqueue - checking shutdown_requested outside
     * the lock creates a race where shutdown can progress between check and lock. */
    nimcp_platform_mutex_lock(&queue->mutex);

    // Check shutdown after acquiring lock (TOCTOU-safe)
    if (g_router && g_router->shutdown_requested) {
        nimcp_platform_mutex_unlock(&queue->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_CANCELLED,
                          "bio_msg_queue_dequeue: shutdown requested");
    }

    // Wait for message if empty
    while (queue->count == 0) {
        // DEADLOCK FIX: Check shutdown flag in wait loop
        if (g_router && g_router->shutdown_requested) {
            nimcp_platform_mutex_unlock(&queue->mutex);
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_CANCELLED,
                              "bio_msg_queue_dequeue: shutdown requested during wait");
        }

        if (timeout_ms == 0) {
            // Non-blocking mode: return NOT_FOUND silently (not an error condition)
            nimcp_platform_mutex_unlock(&queue->mutex);
            return NIMCP_ERROR_NOT_FOUND;
        }

        int wait_result = nimcp_platform_cond_timedwait(&queue->not_empty,
                                                          &queue->mutex,
                                                          timeout_ms);

        // DEADLOCK FIX: Check shutdown after waking from condition wait
        if (g_router && g_router->shutdown_requested) {
            nimcp_platform_mutex_unlock(&queue->mutex);
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_CANCELLED,
                              "bio_msg_queue_dequeue: shutdown requested after wait");
        }

        if (wait_result != 0) {
            nimcp_platform_mutex_unlock(&queue->mutex);
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_TIMEOUT,
                              "bio_msg_queue_dequeue: timeout waiting for message");
        }

        // Loop will re-check condition (count == 0) to handle spurious wakeups
    }

    // Remove from queue
    bio_msg_queue_entry_t* entry = &queue->entries[queue->read_idx];
    *out_msg = entry->msg_data;
    *out_size = entry->msg_size;
    if (out_promise) {
        *out_promise = entry->response_promise;
    }

    entry->msg_data = NULL;
    entry->response_promise = NULL;

    queue->read_idx = (queue->read_idx + 1) % queue->capacity;
    queue->count--;

    // Signal waiting producers
    nimcp_platform_cond_signal(&queue->not_full);

    nimcp_platform_mutex_unlock(&queue->mutex);
    return NIMCP_SUCCESS;
}


/*=============================================================================
 * MESSAGE SENDING
 *============================================================================*/

/**
 * WHAT: Find module entry by ID
 * WHY:  Lookup target module for routing
 * HOW:  Linear search through registry (modules_mutex must be held)
 */
static bio_module_entry_t* bio_router_find_module(bio_module_id_t module_id) {
    for (uint32_t i = 0; i < g_router->module_count; i++) {
        if (g_router->modules[i].module_id == module_id &&
            g_router->modules[i].magic == BIO_MODULE_MAGIC) {
            return &g_router->modules[i];
        }
    }
    /* Module not found - normal case for unregistered targets */
    return NULL;
}


/**
 * @brief Internal send with optional response promise
 */
static nimcp_error_t bio_router_send_with_promise(bio_module_context_t ctx,
                                                   const void* msg,
                                                   size_t msg_size,
                                                   nimcp_bio_promise_t response_promise,
                                                   uint32_t timeout_ms) {
    NIMCP_CHECK_THROW(g_router && ctx && ctx->magic == BIO_MODULE_MAGIC && msg && msg_size > 0,
                      NIMCP_ERROR_INVALID_PARAM,
                      "bio_router_send_with_promise: invalid router, context, or message");

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    uint32_t target_id = header->target_module;

    LOG_TRACE("Routing message (with promise) type=0x%04X from=%u to=%u size=%zu",
              header->type, header->source_module, target_id, msg_size);

    // BBB Security Gate: Validate message content before dispatch
    // WHAT: Apply Blood-Brain Barrier validation to all messages
    // WHY:  Prevent malicious or malformed messages from affecting modules
    // HOW:  Use global BBB system to validate raw message bytes
    bbb_system_t bbb = nimcp_bbb_get_global_system();
    if (bbb && bbb_system_is_enabled(bbb)) {
        bbb_validation_result_t bbb_result;
        if (!bbb_validate_input(bbb, msg, msg_size, &bbb_result)) {
            LOG_WARN("BBB validation failed for async message type=0x%04X from=%u: threat=%d severity=%d",
                     header->type, header->source_module, bbb_result.threat, bbb_result.severity);

            /* Dead-letter tracking: record dropped message type */
            nimcp_platform_mutex_lock(&g_router->stats_mutex);
            g_router->stats.messages_dropped++;
            g_router->dead_letter_types[g_router->dead_letter_idx] = header->type;
            g_router->dead_letter_idx = (g_router->dead_letter_idx + 1) % 16;
            nimcp_platform_mutex_unlock(&g_router->stats_mutex);

            /* Broadcast BIO_MSG_SECURITY_THREAT_DETECTED on BBB rejection
             * (same logic as bio_router_send — see that function for comments). */
            if (header->type != BIO_MSG_SECURITY_THREAT_DETECTED &&
                header->type != BIO_MSG_SECURITY_ALERT) {
                struct {
                    bio_message_header_t hdr;
                    uint32_t rejected_msg_type;
                    uint32_t source_module;
                    int      threat_type;
                    int      severity;
                } threat_alert = {0};
                threat_alert.hdr.type          = BIO_MSG_SECURITY_THREAT_DETECTED;
                threat_alert.hdr.source_module  = BIO_MODULE_SECURITY_BBB_FEP;
                threat_alert.hdr.target_module  = 0;
                threat_alert.hdr.timestamp_us   = nimcp_platform_time_monotonic_us();
                threat_alert.hdr.payload_size   = sizeof(threat_alert) - sizeof(bio_message_header_t);
                threat_alert.hdr.flags          = BIO_MSG_FLAG_URGENT | BIO_MSG_FLAG_BROADCAST;
                threat_alert.rejected_msg_type  = (uint32_t)header->type;
                threat_alert.source_module      = header->source_module;
                threat_alert.threat_type        = bbb_result.threat;
                threat_alert.severity           = bbb_result.severity;
                (void)bio_router_broadcast(ctx, &threat_alert, sizeof(threat_alert));
            }

            NIMCP_CHECK_THROW(false, NIMCP_ERROR_PERMISSION_DENIED,
                              "bio_router_send_with_promise: BBB validation failed");
        }
        LOG_TRACE("BBB validation passed for async message type=0x%04X", header->type);
    }

    if (target_id == 0) {
        LOG_ERROR("Cannot use async send with response for broadcasts");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM,
                          "bio_router_send_with_promise: cannot use promise for broadcasts");
    }

    /* BUG-10 fix: Look up the target entry under modules_mutex, copy the inbox
     * pointer, then release modules_mutex BEFORE calling bio_msg_queue_enqueue.
     * Holding modules_mutex while blocking on a queue causes deadlocks. */
    nimcp_platform_mutex_lock(&g_router->modules_mutex);
    bio_module_entry_t* target = bio_router_find_module(target_id);
    if (!target) {
        nimcp_platform_mutex_unlock(&g_router->modules_mutex);
        LOG_ERROR("Target module %u not found", target_id);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND,
                          "bio_router_send_with_promise: target module not found");
    }
    bio_msg_queue_t* target_inbox = &target->inbox;
    nimcp_platform_mutex_unlock(&g_router->modules_mutex);

    nimcp_error_t result = bio_msg_queue_enqueue(target_inbox, msg, msg_size,
                                                  response_promise, timeout_ms);
    if (result == NIMCP_SUCCESS) {
        atomic_fetch_add(&target->messages_received, 1);
    } else {
        LOG_WARN("Failed to enqueue to module %u inbox (type=0x%04X)", target_id, header->type);

        /* Dead-letter tracking: record dropped message type */
        nimcp_platform_mutex_lock(&g_router->stats_mutex);
        g_router->stats.messages_dropped++;
        g_router->dead_letter_types[g_router->dead_letter_idx] = header->type;
        g_router->dead_letter_idx = (g_router->dead_letter_idx + 1) % 16;
        nimcp_platform_mutex_unlock(&g_router->stats_mutex);
    }

    if (result == NIMCP_SUCCESS) {
        nimcp_platform_mutex_lock(&g_router->stats_mutex);
        g_router->stats.messages_routed++;
        nimcp_platform_mutex_unlock(&g_router->stats_mutex);
    }

    return result;
}

static void init_signal_mutex_once(void) {
    nimcp_platform_mutex_init(&g_signal_mutex, false);
}

static void init_wave_mutex_once(void) {
    nimcp_platform_mutex_init(&g_wave_mutex, false);
}

static int bio_router_kg_dispatch_internal(
    const void* msg,
    size_t msg_size,
    uint32_t timeout_ms
) {
    /* Internal function returns -1 on error, >= 0 for handler count */
    struct brain_kg* kg = get_router_brain_kg_safe();
    if (!kg || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_router_kg_dispatch_internal: required parameter is NULL (kg, msg)");
        return -1;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    /* Query KG for handlers of this message type */
    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(
        kg,
        header->type
    );

    if (!handlers) {
        LOG_WARN("KG dispatch: failed to query handlers for msg type 0x%04X", header->type);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_router_kg_dispatch_internal: handlers is NULL");
        return -1;
    }

    if (handlers->count == 0) {
        LOG_DEBUG("KG dispatch: no handlers found for msg type 0x%04X", header->type);
        brain_kg_handler_list_destroy(handlers);
        return 0;
    }

    LOG_DEBUG("KG dispatch: found %u handlers for msg type 0x%04X",
              handlers->count, header->type);

    int dispatched = 0;

    /* Dispatch to each handler module.
     * BUG-21 fix: Collect target inbox pointers under modules_mutex, then release
     * the lock BEFORE calling bio_msg_queue_enqueue. This prevents deadlock when
     * timeout_ms > 0 (blocking enqueue holds modules_mutex, starving all other
     * module operations). Same pattern as BUG-10 fix in bio_router_send. */
    uint32_t handler_count = handlers->count;

    /* Collect targets under lock */
    bio_msg_queue_t* target_inboxes[handler_count];
    bio_module_entry_t* target_entries[handler_count];
    uint32_t target_count = 0;

    nimcp_platform_mutex_lock(&g_router->modules_mutex);

    for (uint32_t i = 0; i < handler_count; i++) {
        brain_kg_node_id_t handler_node = handlers->handlers[i];

        /* Find module by KG node ID
         * Note: For simplicity, we assume handler_node == module_id.
         * In a full implementation, we'd look up the module_id from the KG node.
         */
        bio_module_entry_t* target = bio_router_find_module((bio_module_id_t)handler_node);
        if (!target) {
            LOG_DEBUG("KG dispatch: module for node %u not found", handler_node);
            continue;
        }

        /* Skip sender to avoid self-delivery */
        if (target->module_id == header->source_module) {
            continue;
        }

        target_inboxes[target_count] = &target->inbox;
        target_entries[target_count] = target;
        target_count++;
    }

    nimcp_platform_mutex_unlock(&g_router->modules_mutex);

    /* Enqueue to each target OUTSIDE the modules_mutex lock */
    for (uint32_t i = 0; i < target_count; i++) {
        nimcp_error_t enq_result = bio_msg_queue_enqueue(target_inboxes[i],
                                                          msg, msg_size,
                                                          NULL, timeout_ms);
        if (enq_result == NIMCP_SUCCESS) {
            atomic_fetch_add(&target_entries[i]->messages_received, 1);
            dispatched++;
        } else {
            LOG_DEBUG("KG dispatch: failed to enqueue to module %u", target_entries[i]->module_id);
        }
    }
    brain_kg_handler_list_destroy(handlers);

    LOG_DEBUG("KG dispatch: delivered to %d/%u handlers", dispatched, handler_count);

    /* Update stats */
    if (dispatched > 0) {
        nimcp_platform_mutex_lock(&g_router->stats_mutex);
        g_router->stats.messages_routed += dispatched;
        nimcp_platform_mutex_unlock(&g_router->stats_mutex);
    }

    return dispatched;
}
