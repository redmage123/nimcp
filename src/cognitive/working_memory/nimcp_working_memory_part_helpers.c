// nimcp_working_memory_part_helpers.c - helpers functions
// Part of nimcp_working_memory.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_working_memory.c


//=============================================================================
// BIO-ASYNC MESSAGE HANDLERS
//=============================================================================

/**
 * @brief Handle working memory store request via bio-async
 *
 * WHAT: Process incoming request to store data in working memory
 * WHY:  Enable distributed systems to add items to working memory via bio-async
 * HOW:  Extract payload data, call working_memory_add with salience from priority
 *
 * NOTE: Data follows immediately after bio_msg_wm_store_t in message buffer
 */
static nimcp_error_t handle_wm_store_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)response_promise;

    if (!msg || !user_data) {
        return NIMCP_ERROR_NULL_ARG;
    }

    if (msg_size < sizeof(bio_msg_wm_store_t)) {
        LOG_ERROR("Store request too small: %zu bytes", msg_size);
        return NIMCP_ERROR_INVALID;
    }

    const bio_msg_wm_store_t* store_msg = (const bio_msg_wm_store_t*)msg;
    working_memory_t* wm = (working_memory_t*)user_data;

    LOG_DEBUG("Received WM store request: slot=%u, size=%u, priority=%.2f",
              store_msg->slot_id, store_msg->data_size, store_msg->priority);

    // Extract payload data (follows immediately after header)
    const uint8_t* payload = (const uint8_t*)msg + sizeof(bio_msg_wm_store_t);
    size_t expected_size = sizeof(bio_msg_wm_store_t) + store_msg->data_size;

    if (msg_size < expected_size) {
        LOG_ERROR("Incomplete store request: expected %zu, got %zu", expected_size, msg_size);
        return NIMCP_ERROR_INVALID;
    }

    // Convert byte data to float array (assuming data_size is in bytes)
    uint32_t num_floats = store_msg->data_size / sizeof(float);
    if (num_floats == 0) {
        LOG_WARN("Empty data in store request");
        return NIMCP_ERROR_INVALID;
    }

    const float* data = (const float*)payload;

    // Add to working memory with priority as salience
    bool success = working_memory_add(wm, data, num_floats, store_msg->priority);

    if (!success) {
        LOG_ERROR("Failed to add item to working memory");
        return NIMCP_ERROR_MEMORY;
    }

    LOG_DEBUG("Successfully stored %u floats in working memory", num_floats);
    return NIMCP_SUCCESS;
}


/**
 * @brief Handle working memory retrieve request via bio-async
 *
 * WHAT: Process incoming request to retrieve data from working memory
 * WHY:  Enable distributed systems to query working memory contents via bio-async
 * HOW:  Lookup item by slot_id, send response with data via promise
 *
 * NOTE: Response includes retrieved data in payload
 */
static nimcp_error_t handle_wm_retrieve_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)msg_size;

    if (!msg || !user_data) {
        return NIMCP_ERROR_NULL_ARG;
    }

    const bio_msg_wm_retrieve_t* retrieve_msg = (const bio_msg_wm_retrieve_t*)msg;
    working_memory_t* wm = (working_memory_t*)user_data;

    LOG_DEBUG("Received WM retrieve request: slot=%u, min_confidence=%.2f",
              retrieve_msg->slot_id, retrieve_msg->min_confidence);

    // Get item from working memory
    uint32_t item_size = 0;
    const float* item = working_memory_get(wm, retrieve_msg->slot_id, &item_size);

    if (!item) {
        LOG_WARN("Item not found at slot %u", retrieve_msg->slot_id);
        // Send empty response via promise to indicate failure
        if (response_promise) {
            bio_msg_wm_store_t response = {0};
            bio_msg_init_header(&response.header, BIO_MSG_WORKING_MEMORY_STORE,
                                BIO_MODULE_WORKING_MEMORY, 0, sizeof(response));
            nimcp_bio_promise_complete_sized(response_promise, &response, sizeof(response));
        }
        return NIMCP_ERROR_INVALID;
    }

    // Get salience for confidence check
    float salience = 0.0F;
    working_memory_get_total_salience(wm, retrieve_msg->slot_id, &salience);

    if (salience < retrieve_msg->min_confidence) {
        LOG_DEBUG("Item salience %.2f below threshold %.2f",
                  salience, retrieve_msg->min_confidence);
        // Send empty response to indicate confidence too low
        if (response_promise) {
            bio_msg_wm_store_t response = {0};
            bio_msg_init_header(&response.header, BIO_MSG_WORKING_MEMORY_STORE,
                                BIO_MODULE_WORKING_MEMORY, 0, sizeof(response));
            nimcp_bio_promise_complete_sized(response_promise, &response, sizeof(response));
        }
        return NIMCP_ERROR_INVALID;
    }

    // Send response with retrieved data via promise
    if (response_promise) {
        size_t response_size = sizeof(bio_msg_wm_store_t) + (item_size * sizeof(float));
        uint8_t* response_buf = nimcp_malloc(response_size);
        if (!response_buf) {
            LOG_ERROR("Failed to allocate response buffer");
            return NIMCP_ERROR_MEMORY;
        }

        bio_msg_wm_store_t* response = (bio_msg_wm_store_t*)response_buf;
        bio_msg_init_header(&response->header, BIO_MSG_WORKING_MEMORY_STORE,
                            BIO_MODULE_WORKING_MEMORY, 0, response_size);
        response->slot_id = retrieve_msg->slot_id;
        response->data_size = item_size * sizeof(float);
        response->priority = salience;

        // Copy item data to response payload
        memcpy(response_buf + sizeof(bio_msg_wm_store_t), item, item_size * sizeof(float));

        nimcp_bio_promise_complete_sized(response_promise, response_buf, response_size);
        nimcp_free(response_buf);
        response_buf = NULL;

        LOG_DEBUG("Retrieved %u floats from slot %u", item_size, retrieve_msg->slot_id);
    }

    return NIMCP_SUCCESS;
}


/**
 * @brief Broadcast item stored event
 */
static void bio_broadcast_item_stored(working_memory_t* wm, uint32_t slot_id, float salience) {
    if (!wm || !wm->bio_async_enabled || !wm->bio_ctx) {
        return;
    }

    bio_msg_wm_store_t msg = {};
    bio_msg_init_header(&msg.header, BIO_MSG_WORKING_MEMORY_STORE,
                        bio_module_context_get_id(wm->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.slot_id = slot_id;
    msg.priority = salience;

    bio_router_broadcast(wm->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG("Broadcast: item stored in slot %u", slot_id);
}


/**
 * @brief Broadcast item evicted event (attention shift)
 */
static void bio_broadcast_item_evicted(working_memory_t* wm, uint32_t slot_id) {
    if (!wm || !wm->bio_async_enabled || !wm->bio_ctx) {
        return;
    }

    bio_msg_attention_shift_t msg = {};
    bio_msg_init_header(&msg.header, BIO_MSG_ATTENTION_SHIFT,
                        bio_module_context_get_id(wm->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.target_id = slot_id;
    msg.attention_weight = 0.0F;  // Item is gone
    msg.preemptive = false;

    bio_router_broadcast(wm->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG("Broadcast: item evicted from slot %u", slot_id);
}

static void set_error(const char* msg) {
    snprintf(last_error, sizeof(last_error), "%s", msg);
}


// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Find index of item with lowest salience
 *
 * WHAT: Search for lowest-priority item for eviction
 * WHY:  Evict least important item when buffer is full
 * HOW:  Linear scan with min tracking
 *
 * COMPLEXITY: O(n) where n = current_size
 *
 * @param wm Working memory instance
 * @return Index of lowest salience item, or -1 if empty
 */
static int find_lowest_salience_index(const working_memory_t* wm) {
    // Guard: Empty buffer
    if (wm->current_size == 0) {
        return -1;
    }

    int min_index = 0;
    float min_salience = wm->salience[0];

    for (uint32_t i = 1; i < wm->current_size; i++) {
        if (wm->salience[i] < min_salience) {
            min_salience = wm->salience[i];
            min_index = i;
        }
    }

    return min_index;
}


/**
 * @brief Evict item at specific index
 *
 * WHAT: Remove item and compact buffer
 * WHY:  Make space for new item
 * HOW:  Free memory → Shift arrays left → Decrement size → NULL last slot
 *
 * COMPLEXITY: O(n) where n = current_size (due to shift)
 *
 * @param wm Working memory instance
 * @param index Index to evict
 */
static void evict_item_at_index(working_memory_t* wm, uint32_t index) {
    // Guard: Invalid index
    if (index >= wm->current_size) {
        return;
    }

    // Remove from quantum bridge before eviction
    if (wm->enable_quantum_wm && wm->quantum_bridge) {
        working_memory_quantum_remove(wm->quantum_bridge, index);
    }

    // Free item memory
    nimcp_free(wm->items[index]);

    // Shift arrays left
    uint32_t shift_count = wm->current_size - index - 1;
    if (shift_count > 0) {
        memmove(&wm->items[index], &wm->items[index + 1],
                shift_count * sizeof(float*));
        memmove(&wm->item_sizes[index], &wm->item_sizes[index + 1],
                shift_count * sizeof(uint32_t));
        memmove(&wm->salience[index], &wm->salience[index + 1],
                shift_count * sizeof(float));
        memmove(&wm->timestamps[index], &wm->timestamps[index + 1],
                shift_count * sizeof(uint64_t));
        memmove(&wm->attention_refreshed[index],
                &wm->attention_refreshed[index + 1],
                shift_count * sizeof(bool));
        memmove(&wm->emotions[index], &wm->emotions[index + 1],  // Phase 10.3
                shift_count * sizeof(emotional_tag_t));
        memmove(&wm->has_emotion[index], &wm->has_emotion[index + 1],  // Phase 10.3
                shift_count * sizeof(bool));
    }

    wm->current_size--;

    // NULL out the last slot to prevent double-free
    // After memmove and size decrement, items[current_size] contains a stale pointer
    wm->items[wm->current_size] = NULL;

    wm->total_evictions++;

    // Signal immune system on eviction (TNF-alpha for failure)
    if (wm->immune_integration_enabled && wm->immune) {
        // Release TNF-alpha (eviction = resource failure)
        uint32_t cytokine_id = 0;
        brain_immune_release_cytokine(
            wm->immune,
            CYTOKINE_TNFA,
            0,  // Working memory module
            0.5f,  // Moderate signal
            0,  // Broadcast
            &cytokine_id
        );
    }

    // Broadcast eviction event via bio-async
    bio_broadcast_item_evicted(wm, index);
}


/**
 * @brief Get current time in milliseconds
 *
 * WHAT: System clock for temporal decay
 * WHY:  Track item age for exponential decay
 * HOW:  Use monotonic clock for consistent timing
 *
 * @return Current time in milliseconds
 *
 * COMPLEXITY: O(1) - direct system call
 */
static uint64_t get_current_time_ms(void) {
    return nimcp_time_monotonic_ms();
}


//=============================================================================
// BRAIN IMMUNE INTEGRATION
//=============================================================================

/**
 * @brief Callback for inflammation state changes
 *
 * WHAT: Update working memory capacity based on inflammation level
 * WHY:  Model cytokine impairment of prefrontal cortex function
 * HOW:  Apply capacity penalty based on inflammation severity
 *
 * BIOLOGICAL BASIS:
 * IL-6 and TNF-alpha impair prefrontal working memory representations
 * during systemic inflammation (illness, stress, immune activation)
 */
static void wm_inflammation_callback(
    brain_immune_system_t* system,
    const brain_inflammation_site_t* site,
    void* user_data)
{
    (void)system;

    if (!user_data || !site) {
        return;
    }

    working_memory_t* wm = (working_memory_t*)user_data;

    nimcp_platform_mutex_lock(&wm->mutex);

    // Map inflammation level to capacity penalty
    uint32_t penalty = 0;
    switch (site->level) {
        case INFLAMMATION_NONE:
            penalty = 0;
            break;
        case INFLAMMATION_LOCAL:
            penalty = 1;  // -1 item (7 → 6)
            break;
        case INFLAMMATION_REGIONAL:
            penalty = 2;  // -2 items (7 → 5)
            break;
        case INFLAMMATION_SYSTEMIC:
            penalty = 3;  // -3 items (7 → 4)
            break;
        case INFLAMMATION_STORM:
            penalty = 4;  // -4 items (7 → 3, minimum)
            break;
    }

    wm->inflammation_capacity_penalty = penalty;

    LOG_INFO("WM inflammation callback: level=%s, penalty=%u, effective_capacity=%u",
             brain_immune_inflammation_to_string(site->level),
             penalty,
             wm->capacity > penalty ? wm->capacity - penalty : 3);

    // If current size exceeds new effective capacity, evict lowest-salience items
    uint32_t effective_capacity = wm->capacity > penalty ? wm->capacity - penalty : 3;
    if (effective_capacity < 3) {
        effective_capacity = 3;  // Minimum 3 items even under cytokine storm
    }

    while (wm->current_size > effective_capacity) {
        // Use internal unlocked helpers (mutex already held by this callback)
        int lowest_idx = find_lowest_salience_index(wm);
        if (lowest_idx < 0) {
            break;  // No more items
        }

        LOG_DEBUG("Evicting item %d due to inflammation (size=%u, effective_cap=%u)",
                  lowest_idx, wm->current_size, effective_capacity);

        evict_item_at_index(wm, (uint32_t)lowest_idx);
    }

    nimcp_platform_mutex_unlock(&wm->mutex);
}
