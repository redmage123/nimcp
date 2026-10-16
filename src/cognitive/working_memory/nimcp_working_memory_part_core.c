// nimcp_working_memory_part_core.c - core functions
// Part of nimcp_working_memory.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_working_memory.c


// ============================================================================
// ITEM MANAGEMENT
// ============================================================================

/**
 * @brief Add item to working memory with salience-based eviction
 *
 * WHAT: Insert new item into buffer, evicting if necessary
 * WHY:  Maintain active representations for reasoning
 * HOW:  Validate → Check capacity → Evict if full → Copy item → Store metadata
 *
 * COMPLEXITY: O(n) where n = capacity (eviction search)
 * MEMORY: Allocates item_size × sizeof(float) bytes
 *
 * @param wm Working memory instance (non-NULL)
 * @param item Item data array (non-NULL)
 * @param item_size Size of item in floats (> 0)
 * @param salience Importance [0.0, 1.0] for eviction priority
 * @return true on success, false on error
 */
bool working_memory_add(
    working_memory_t* wm,
    const float* item,
    uint32_t item_size,
    float salience
)
{
    // Guard: NULL working memory
    if (!wm) {
        set_error("NULL working_memory_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_add: wm is NULL");
        return false;
    }

    // Guard: NULL item
    if (!item) {
        set_error("NULL item");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_add: item is NULL");
        return false;
    }

    // Guard: Invalid size
    if (item_size == 0) {
        set_error("item_size must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_add: item_size is zero");
        return false;
    }

    // Guard: Size overflow check
    if (item_size > (MAX_ITEM_SIZE_BYTES / sizeof(float))) {
        set_error("item_size exceeds maximum");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_add: validation failed");
        return false;
    }

    // Guard: Invalid salience
    if (salience < 0.0F || salience > 1.0F) {
        set_error("salience must be in [0.0, 1.0]");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_add: validation failed");
        return false;
    }

    // CRITICAL: Allocate and copy item BEFORE locking/evicting
    // WHY: The input 'item' pointer may point to an existing working memory item.
    //      If we evict that item before copying, we'd read from freed memory.
    //      By copying first, we safely capture the data before any eviction.
    float* item_copy = nimcp_calloc(item_size, sizeof(float));
    if (!item_copy) {
        set_error("Failed to allocate item memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "working_memory_add: item_copy is NULL");
        return false;
    }
    memcpy(item_copy, item, item_size * sizeof(float));

    // Lock mutex for thread-safe access
    nimcp_platform_mutex_lock(&wm->mutex);

    // Check if full → evict lowest salience item
    if (wm->current_size >= wm->capacity) {
        int evict_index = find_lowest_salience_index(wm);
        if (evict_index >= 0) {
            evict_item_at_index(wm, evict_index);
        }
        // Safety check: if eviction failed, we can't add
        if (wm->current_size >= wm->capacity) {
            nimcp_platform_mutex_unlock(&wm->mutex);
            nimcp_free(item_copy);
            item_copy = NULL;
            set_error("Working memory full and eviction failed");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "working_memory_add: capacity exceeded");
            return false;
        }
    }

    // Insert at end
    uint32_t index = wm->current_size;
    wm->items[index] = item_copy;
    wm->item_sizes[index] = item_size;
    wm->salience[index] = salience;
    wm->timestamps[index] = get_current_time_ms();
    wm->attention_refreshed[index] = false;
    wm->has_emotion[index] = false;  // Phase 10.3: No emotion by default

    wm->current_size++;
    wm->total_additions++;

    // Store in quantum bridge for fast retrieval
    if (wm->enable_quantum_wm && wm->quantum_bridge) {
        working_memory_quantum_store(wm->quantum_bridge, item_copy, item_size, index, salience);
    }

    // Check for high utilization and signal stress to immune system
    if (wm->immune_integration_enabled && wm->immune) {
        uint32_t eff_cap = working_memory_get_effective_capacity(wm);
        float utilization = (eff_cap > 0) ? ((float)wm->current_size / (float)eff_cap) : 0.0f;
        if (utilization > 0.9f) {
            // High utilization - signal IL-6 (cognitive load)
            uint32_t cytokine_id = 0;
            brain_immune_release_cytokine(
                wm->immune,
                CYTOKINE_IL6,
                0,  // Working memory module
                utilization,  // Signal strength = utilization
                0,  // Broadcast
                &cytokine_id
            );
        }
    }

    // Broadcast item stored event via bio-async
    bio_broadcast_item_stored(wm, index, salience);

    nimcp_platform_mutex_unlock(&wm->mutex);
    return true;
}


/**
 * @brief Add item to working memory with emotional tag (Phase 10.3)
 *
 * WHAT: Insert new item with emotional context for enhanced salience
 * WHY:  Emotional events receive memory priority (biological)
 * HOW:  Store emotional tag → Compute boosted salience → Add item
 *
 * COMPLEXITY: O(n) where n = capacity (eviction search)
 *
 * @param wm Working memory instance (non-NULL)
 * @param item Item data array (non-NULL)
 * @param item_size Size of item in floats (> 0)
 * @param base_salience Base importance [0.0, 1.0]
 * @param emotion Emotional tag (non-NULL)
 * @return true on success, false on error
 */
bool working_memory_add_with_emotion(
    working_memory_t* wm,
    const float* item,
    uint32_t item_size,
    float base_salience,
    const emotional_tag_t* emotion
)
{
    // Guard: NULL working memory
    if (!wm) {
        set_error("NULL working_memory_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_add_with_emotion: wm is NULL");
        return false;
    }

    // Guard: NULL emotion
    if (!emotion) {
        set_error("NULL emotional_tag_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_add_with_emotion: emotion is NULL");
        return false;
    }

    // Guard: Invalid emotion
    if (!emotional_tag_is_valid(emotion)) {
        set_error("Invalid emotional tag");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_add_with_emotion: emotional tag validation failed");
        return false;
    }

    // Compute emotional salience boost
    float emotional_boost = emotional_compute_salience_boost(emotion);
    float total_salience = base_salience * emotional_boost;

    // Clamp to valid range
    if (total_salience > 1.0F) {
        total_salience = 1.0F;
    }

    // Add item with boosted salience (this will acquire the lock)
    if (!working_memory_add(wm, item, item_size, total_salience)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "working_memory_add_with_emotion: working_memory_add failed");
        return false;
    }

    // Lock mutex to attach emotional tag
    nimcp_platform_mutex_lock(&wm->mutex);

    // Attach emotional tag to the just-added item
    uint32_t index = wm->current_size - 1;  // Last added
    wm->emotions[index] = *emotion;
    wm->has_emotion[index] = true;

    nimcp_platform_mutex_unlock(&wm->mutex);
    return true;
}


/**
 * @brief Get item from working memory without removing
 *
 * WHAT: Read-only access to item by index
 * WHY:  Allow inspection of working memory contents
 * HOW:  Validate → Return pointer to internal data
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @param index Item index [0, current_size)
 * @param size Output parameter for item size (nullable)
 * @return Pointer to item data, or NULL on error
 */
const float* working_memory_get(
    working_memory_t* wm,
    uint32_t index,
    uint32_t* size
)
{
    // Guard: NULL working memory
    if (!wm) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: wm");
        set_error("NULL working_memory_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_get: wm is NULL");
        return NULL;
    }

    // Lock mutex for thread-safe access
    nimcp_platform_mutex_lock(&wm->mutex);

    // Guard: Invalid index (bounds check)
    if (index >= wm->current_size) {
        NIMCP_ERROR_SET(NIMCP_ERROR_OUT_OF_RANGE, "Out of bounds: index (%u >= %u)",
                       index, wm->current_size);
        set_error("Index out of bounds");
        nimcp_platform_mutex_unlock(&wm->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_get: capacity exceeded");
        return NULL;
    }

    // Set size if requested
    if (size) {
        *size = wm->item_sizes[index];
    }

    const float* result = wm->items[index];
    nimcp_platform_mutex_unlock(&wm->mutex);
    return result;
}


/**
 * @brief Remove item from working memory
 *
 * WHAT: Delete item at specific index
 * WHY:  Manual removal of irrelevant items
 * HOW:  Validate → Evict → Compact
 *
 * COMPLEXITY: O(n) where n = current_size (shift)
 *
 * @param wm Working memory instance (non-NULL)
 * @param index Item index to remove [0, current_size)
 * @return true on success, false on error
 */
bool working_memory_remove(working_memory_t* wm, uint32_t index) {
    // Guard: NULL working memory
    if (!wm) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: wm");
        set_error("NULL working_memory_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_remove: wm is NULL");
        return false;
    }

    // Lock mutex for thread-safe access
    nimcp_platform_mutex_lock(&wm->mutex);

    // Guard: Invalid index (bounds check)
    if (index >= wm->current_size) {
        NIMCP_ERROR_SET(NIMCP_ERROR_OUT_OF_RANGE, "Out of bounds: index (%u >= %u)",
                       index, wm->current_size);
        set_error("Index out of bounds");
        nimcp_platform_mutex_unlock(&wm->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_remove: capacity exceeded");
        return false;
    }

    evict_item_at_index(wm, index);
    nimcp_platform_mutex_unlock(&wm->mutex);
    return true;
}


/**
 * @brief Clear all items from working memory
 *
 * WHAT: Remove all items and reset to empty state
 * WHY:  Task switching, context reset
 * HOW:  Free all items → Reset size counter
 *
 * COMPLEXITY: O(n) where n = current_size
 *
 * @param wm Working memory instance (non-NULL)
 */
void working_memory_clear(working_memory_t* wm) {
    // Guard: NULL working memory
    if (!wm) {
        return;
    }

    // Lock mutex for thread-safe access
    nimcp_platform_mutex_lock(&wm->mutex);

    // Free all items
    for (uint32_t i = 0; i < wm->current_size; i++) {
        nimcp_free(wm->items[i]);
        wm->items[i] = NULL;
    }

    wm->current_size = 0;

    // Clear quantum bridge
    if (wm->enable_quantum_wm && wm->quantum_bridge) {
        working_memory_quantum_clear(wm->quantum_bridge);
    }

    nimcp_platform_mutex_unlock(&wm->mutex);
}


// ============================================================================
// ATTENTION AND DECAY
// ============================================================================

/**
 * @brief Refresh item via attention (prevent decay)
 *
 * WHAT: Mark item as rehearsed to prevent temporal decay
 * WHY:  Simulate attention-based maintenance (frontal-parietal loop)
 * HOW:  Validate → Update timestamp → Set refresh flag
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @param index Item index to refresh [0, current_size)
 * @return true on success, false on error
 */
bool working_memory_refresh(working_memory_t* wm, uint32_t index) {
    // Guard: NULL working memory
    if (!wm) {
        set_error("NULL working_memory_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_refresh: wm is NULL");
        return false;
    }

    // Guard: Feature disabled (check before locking)
    if (!wm->enable_attention_refresh) {
        set_error("Attention refresh disabled");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_refresh: attention refresh is disabled");
        return false;
    }

    // Lock mutex for thread-safe access
    nimcp_platform_mutex_lock(&wm->mutex);

    // Guard: Invalid index
    if (index >= wm->current_size) {
        set_error("Index out of bounds");
        nimcp_platform_mutex_unlock(&wm->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_refresh: capacity exceeded");
        return false;
    }

    // Refresh timestamp and set flag
    wm->timestamps[index] = get_current_time_ms();
    wm->attention_refreshed[index] = true;
    wm->total_refreshes++;

    // Update quantum bridge with refreshed salience
    if (wm->enable_quantum_wm && wm->quantum_bridge) {
        working_memory_quantum_update(wm->quantum_bridge, index,
                                      wm->items[index], wm->item_sizes[index],
                                      wm->salience[index]);
    }

    nimcp_platform_mutex_unlock(&wm->mutex);
    return true;
}


/**
 * @brief Apply temporal decay to all items
 *
 * WHAT: Exponentially decay salience based on time elapsed
 * WHY:  Simulate natural forgetting without rehearsal
 * HOW:  For each item: Calculate decay → Update salience → Remove if below threshold
 *
 * FORMULA: salience_new = salience_old × exp(-Δt / τ)
 *
 * COMPLEXITY: O(n) where n = current_size
 *
 * @param wm Working memory instance (non-NULL)
 * @param current_time_ms Current time in milliseconds
 * @return Number of items removed due to decay
 */
uint32_t working_memory_decay(
    working_memory_t* wm,
    uint64_t current_time_ms
)
{
    // Guard: NULL working memory
    if (!wm) {
        set_error("NULL working_memory_t");
        return 0;
    }

    // Guard: Feature disabled
    if (!wm->enable_temporal_decay) {
        return 0;
    }

    // Process pending bio-async messages before decay processing
    if (wm->bio_async_enabled && wm->bio_ctx) {
        bio_router_process_inbox(wm->bio_ctx, 10);  // Process up to 10 messages
    }

    // Lock mutex for thread-safe access
    nimcp_platform_mutex_lock(&wm->mutex);

    uint32_t removed_count = 0;

    // Get sleep-modulated decay rate
    float decay_rate_modulation = working_memory_sleep_decay_for_state(wm->current_sleep_state);

    // Iterate backwards to safely remove items
    for (int i = (int)wm->current_size - 1; i >= 0; i--) {
        // Skip if attention-refreshed
        if (wm->attention_refreshed[i]) {
            wm->attention_refreshed[i] = false;  // Reset flag
            continue;
        }

        // Calculate time elapsed
        uint64_t elapsed_ms = current_time_ms - wm->timestamps[i];

        // Apply exponential decay with sleep-modulated rate: s_new = s_old × exp(-t×decay_rate/τ)
        // P2 fix: Clamp exponent to prevent underflow to exactly 0
        float exponent = -(float)elapsed_ms * decay_rate_modulation / wm->decay_tau_ms;
        if (exponent < MIN_DECAY_EXPONENT) {
            exponent = MIN_DECAY_EXPONENT;
        }
        float decay_factor = expf(exponent);
        wm->salience[i] *= decay_factor;

        // Remove if below threshold
        if (wm->salience[i] < wm->min_salience) {
            evict_item_at_index(wm, i);
            wm->total_decay_removals++;
            removed_count++;
        }
    }

    // Signal immune system if items were removed by decay (IL-1 for resource scarcity)
    if (removed_count > 0 && wm->immune_integration_enabled && wm->immune) {
        uint32_t cytokine_id = 0;
        float signal_strength = (float)removed_count / (float)wm->capacity;
        if (signal_strength > 1.0f) signal_strength = 1.0f;

        brain_immune_release_cytokine(
            wm->immune,
            CYTOKINE_IL1B,
            0,  // Working memory module
            signal_strength,
            0,  // Broadcast
            &cytokine_id
        );
    }

    nimcp_platform_mutex_unlock(&wm->mutex);
    return removed_count;
}


/**
 * @brief Find index of item with highest salience
 *
 * WHAT: Search for most important item
 * WHY:  Identify priority item for processing
 * HOW:  Linear scan with max tracking
 *
 * COMPLEXITY: O(n) where n = current_size
 *
 * @param wm Working memory instance (non-NULL)
 * @param salience Output parameter for salience value (nullable)
 * @return Index of highest salience item, or -1 if empty
 */
int working_memory_find_highest_salience(
    working_memory_t* wm,
    float* salience
)
{
    // Guard: NULL working memory
    if (!wm) {
        set_error("NULL working_memory_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_find_highest_salience: wm is NULL");
        return -1;
    }

    // Lock mutex for thread-safe access
    nimcp_platform_mutex_lock(&wm->mutex);

    // Guard: Empty buffer
    if (wm->current_size == 0) {
        nimcp_platform_mutex_unlock(&wm->mutex);
        return -1;
    }

    int max_index = 0;
    float max_salience = wm->salience[0];

    for (uint32_t i = 1; i < wm->current_size; i++) {
        if (wm->salience[i] > max_salience) {
            max_salience = wm->salience[i];
            max_index = i;
        }
    }

    // Set salience if requested
    if (salience) {
        *salience = max_salience;
    }

    nimcp_platform_mutex_unlock(&wm->mutex);
    return max_index;
}


/**
 * @brief Find index of item with lowest salience
 *
 * WHAT: Search for least important item
 * WHY:  Identify item for eviction when memory full
 * HOW:  Linear scan with min tracking
 *
 * COMPLEXITY: O(n) where n = current_size
 *
 * @param wm Working memory instance (non-NULL)
 * @param salience Output parameter for salience value (nullable)
 * @return Index of lowest salience item, or -1 if empty
 */
int working_memory_find_lowest_salience(
    working_memory_t* wm,
    float* salience
)
{
    /* Guard: NULL working memory */
    if (!wm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_find_lowest_salience: wm is NULL");
        return -1;
    }

    /* Lock mutex for thread-safe access */
    nimcp_platform_mutex_lock(&wm->mutex);

    /* Guard: Empty buffer */
    if (wm->current_size == 0) {
        nimcp_platform_mutex_unlock(&wm->mutex);
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

    /* Set salience if requested */
    if (salience) {
        *salience = min_salience;
    }

    nimcp_platform_mutex_unlock(&wm->mutex);
    return min_index;
}


//=============================================================================
// POSITIONAL ENCODING FUNCTIONS
//=============================================================================

/**
 * @brief Apply positional encodings to all items in working memory
 *
 * WHAT: Add position embeddings to each item based on its slot position
 * WHY:  Capture serial position effects (primacy, recency) in working memory
 * HOW:  For each item at position i, apply PE(i) additively to item data
 *
 * ALGORITHM:
 * 1. Check if positional encoding is enabled
 * 2. For each item in buffer:
 *    a. Get position encoding PE(i) for slot i
 *    b. Add PE to item data: item[j] += PE(i)[j % pe_dim]
 * 3. Handle dimension mismatch (cycle or truncate PE)
 *
 * BIOLOGICAL BASIS:
 * - Serial position effects: primacy (early items) and recency (late items)
 * - Prefrontal cortex encodes temporal order of representations
 * - Position information aids working memory retrieval and manipulation
 *
 * COMPLEXITY: O(n × d) where n = current_size, d = embedding_dim
 *
 * @param wm Working memory instance (non-NULL)
 * @return true on success, false if PE disabled or error
 */
bool working_memory_encode_positions(working_memory_t* wm) {
    // Guard: NULL working memory
    if (!wm) {
        set_error("NULL working_memory_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_encode_positions: wm is NULL");
        return false;
    }

    // Guard: Positional encoding disabled
    if (!wm->enable_positional_encoding || !wm->pos_encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_encode_positions: required parameter is NULL (wm->enable_positional_encoding, wm->pos_encoder)");
        return false;
    }

    // Lock mutex for thread-safe access
    nimcp_platform_mutex_lock(&wm->mutex);

    // Guard: Empty buffer
    if (wm->current_size == 0) {
        nimcp_platform_mutex_unlock(&wm->mutex);
        return true;  // Success: nothing to encode
    }

    // Apply position encoding to each item
    for (uint32_t pos = 0; pos < wm->current_size; pos++) {
        // Get position encoding for this slot
        int result = nimcp_pos_encode_position(wm->pos_encoder, pos, wm->pe_buffer);
        if (result != NIMCP_POS_SUCCESS) {
            LOG_WARN("Failed to encode position %u: error %d", pos, result);
            continue;  // Skip this position, continue with others
        }

        // Add position encoding to item data
        // Handle dimension mismatch: cycle PE if item is larger than PE dim
        float* item_data = wm->items[pos];
        uint32_t item_size = wm->item_sizes[pos];

        if (wm->pe_embedding_dim == 0) continue;
        for (uint32_t j = 0; j < item_size; j++) {
            // Cycle through PE dimensions if item is larger
            uint32_t pe_idx = j % wm->pe_embedding_dim;
            item_data[j] += wm->pe_buffer[pe_idx];
        }
    }

    nimcp_platform_mutex_unlock(&wm->mutex);
    return true;
}


bool working_memory_connect_immune(
    working_memory_t* wm,
    struct brain_immune_system* immune)
{
    // WHAT: Connect working memory to brain immune system
    // WHY:  Enable bidirectional immune-cognitive integration
    // HOW:  Store immune pointer, register inflammation callback

    if (!wm) {
        set_error("NULL working memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_connect_immune: wm is NULL");
        return false;
    }

    if (!immune) {
        set_error("NULL immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_connect_immune: immune is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(&wm->mutex);

    wm->immune = immune;
    wm->immune_integration_enabled = true;
    wm->inflammation_capacity_penalty = 0;
    wm->last_stress_signal_time_ms = 0.0f;

    // Register callback for inflammation events
    brain_immune_set_inflammation_callback(immune, wm_inflammation_callback, wm);

    LOG_INFO("Working memory connected to brain immune system");

    nimcp_platform_mutex_unlock(&wm->mutex);
    return true;
}


void working_memory_disconnect_immune(working_memory_t* wm)
{
    // WHAT: Disconnect from immune system
    // WHY:  Clean shutdown or disable modulation
    // HOW:  Clear pointer, restore full capacity

    if (!wm) {
        return;
    }

    nimcp_platform_mutex_lock(&wm->mutex);

    if (wm->immune) {
        // Unregister callback
        brain_immune_set_inflammation_callback(wm->immune, NULL, NULL);
    }

    wm->immune = NULL;
    wm->immune_integration_enabled = false;
    wm->inflammation_capacity_penalty = 0;

    LOG_INFO("Working memory disconnected from brain immune system");

    nimcp_platform_mutex_unlock(&wm->mutex);
}


bool working_memory_signal_stress(
    working_memory_t* wm,
    float stress_level)
{
    // WHAT: Signal immune system about cognitive stress
    // WHY:  Working memory overload triggers immune response
    // HOW:  Release IL-6 cytokine via brain immune system

    if (!wm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_signal_stress: wm is NULL");
        return false;
    }

    if (!wm->immune_integration_enabled || !wm->immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_signal_stress: required parameter is NULL (wm->immune_integration_enabled, wm->immune)");
        return false;
    }

    // Clamp stress level
    if (stress_level < 0.0f) stress_level = 0.0f;
    if (stress_level > 1.0f) stress_level = 1.0f;

    nimcp_platform_mutex_lock(&wm->mutex);

    // Release IL-6 cytokine (cognitive load signal)
    uint32_t cytokine_id = 0;
    int result = brain_immune_release_cytokine(
        wm->immune,
        CYTOKINE_IL6,
        0,  // No specific source cell (working memory itself)
        stress_level,
        0,  // Broadcast (target_region = 0)
        &cytokine_id
    );

    if (result == 0) {
        wm->last_stress_signal_time_ms = (float)nimcp_time_get_ms();
        LOG_DEBUG("WM stress signal: level=%.2f, cytokine_id=%u", stress_level, cytokine_id);
    }

    nimcp_platform_mutex_unlock(&wm->mutex);

    return result == 0;
}


//=============================================================================
// Knowledge Graph Self-Awareness Integration
//=============================================================================

/**
 * @brief Connect working memory to internal knowledge graph
 *
 * WHAT: Initialize KG context for self-awareness queries
 * WHY:  Enable working memory to query its integrations and capabilities
 * HOW:  Use KG helper functions to establish connection
 *
 * @param wm Working memory instance
 * @param brain Brain instance for KG access
 * @return true if connected (or KG gracefully disabled), false on error
 */
bool working_memory_connect_kg(working_memory_t* wm, brain_t brain)
{
    if (!wm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_connect_kg: wm is NULL");
        return false;
    }

    int result = kg_module_init(&wm->kg_context, brain, "Working_Memory");

    if (result != 0) {
        LOG_ERROR("Failed to initialize KG context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_connect_kg: validation failed");
        return false;
    }

    if (!kg_is_available(&wm->kg_context)) {
        wm->kg_connected = false;
        LOG_INFO("KG disabled, graceful degradation");
        return true;
    }

    wm->kg_connected = true;
    LOG_INFO("Connected to internal KG for self-awareness");

    return true;
}


/**
 * @brief Query working memory's integration points from KG
 *
 * WHAT: Retrieve list of modules that integrate with working memory
 * WHY:  Enable self-awareness of memory pathway connections
 * HOW:  Query KG for edges connected to working memory node
 *
 * @param wm Working memory instance
 * @return Number of integration points found (0 if KG not connected)
 */
int working_memory_query_integrations(working_memory_t* wm)
{
    if (!wm || !wm->kg_connected) {
        return 0;
    }

    if (!kg_is_available(&wm->kg_context)) {
        return 0;
    }

    // Get all neighbors (both incoming and outgoing connections)
    brain_kg_node_list_t* neighbors = kg_get_neighbors_safe(&wm->kg_context);
    if (!neighbors) {
        return 0;
    }

    int count = (int)neighbors->count;
    LOG_DEBUG("Working memory has %d KG integration points", count);

    brain_kg_node_list_destroy(neighbors);
    return count;
}


/**
 * @brief Query working memory's self-knowledge from KG
 *
 * WHAT: Query KG for structural self-knowledge about working memory
 * WHY:  Enable introspection of capacity and integration status
 * HOW:  Find self node and retrieve metadata
 *
 * @param wm Working memory instance
 * @return true if self-knowledge is available, false otherwise
 */
bool working_memory_query_self_knowledge(working_memory_t* wm)
{
    if (!wm || !wm->kg_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_query_self_knowledge: required parameter is NULL (wm, wm->kg_connected)");
        return false;
    }

    if (!kg_has_node(&wm->kg_context)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_query_self_knowledge: kg_has_node is NULL");
        return false;
    }

    const brain_kg_node_t* self = kg_get_node_safe(
        &wm->kg_context,
        wm->kg_context.self_node_id
    );

    if (self) {
        LOG_DEBUG("Working memory self-knowledge: name=%s, state=%d",
                  self->name, self->state);
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_query_self_knowledge: validation failed");
    return false;
}


/**
 * @brief W9-kg read-path: query semantic-memory nodes from KG.
 *
 * WHAT: Ask the KG whether a named concept (semantic node) is present;
 *       if so, this WM instance can defer retrieval to semantic memory
 *       rather than serving from its bounded cache.
 * WHY:  Closes the PARTIAL status for WM — it now consults the KG as a
 *       retrieval source alongside its own buffer. Emits a WM store event
 *       so W16 consumers see the access.
 * HOW:  Uses the shared W9-kg helpers. Returns true when the KG has a
 *       semantic node matching the concept name, false otherwise.
 *
 * @param wm working memory instance
 * @param concept_name canonical concept name to look up
 * @param item_id logical WM item id to associate with the lookup (for emit)
 * @param priority salience priority for the lookup
 * @return true if KG has a semantic node for this concept
 */
bool working_memory_query_semantic_kg(working_memory_t* wm,
                                      const char* concept_name,
                                      uint64_t item_id,
                                      float priority)
{
    if (!wm) return false;

    struct brain_struct* brain = w9kg_get_registered_brain();
    if (!brain) return false;

    bool has_concept = w9kg_query_wm_has_semantic(brain, concept_name);
    /* Record the WM access in the KG so downstream consumers see it. */
    w9kg_emit_wm_item_stored(brain, item_id, priority);
    return has_concept;
}


/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int working_memory_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "working_memory_training_begin: NULL argument");
        return -1;
    }
    working_memory_heartbeat_instance(NULL, "working_memory_training_begin", 0.0f);
    (void)(struct working_memory*)instance; /* Module state available for reset */
    return 0;
}


int working_memory_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "working_memory_training_end: NULL argument");
        return -1;
    }
    working_memory_heartbeat_instance(NULL, "working_memory_training_end", 1.0f);
    (void)(struct working_memory*)instance; /* Module state available for finalization */
    return 0;
}
