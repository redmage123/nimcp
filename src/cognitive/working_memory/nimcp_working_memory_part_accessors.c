// nimcp_working_memory_part_accessors.c - accessors functions
// Part of nimcp_working_memory.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_working_memory.c


const char* working_memory_get_last_error(void) {
    return last_error;
}


// ============================================================================
// LIFECYCLE FUNCTIONS
// ============================================================================

/**
 * @brief Get default working memory configuration
 *
 * WHAT: Standard configuration matching biological constraints
 * WHY:  Provide sensible defaults (Miller's 7±2, 1s decay)
 * HOW:  Initialize struct with empirically validated values
 *
 * @return Default configuration
 */
working_memory_config_t working_memory_default_config(void) {
    working_memory_config_t config = {
        .capacity = WORKING_MEMORY_DEFAULT_CAPACITY,  // 7
        .decay_tau_ms = WORKING_MEMORY_DECAY_TAU_MS,  // 1000ms
        .min_salience = WORKING_MEMORY_MIN_SALIENCE,  // 0.01
        .enable_attention_refresh = true,
        .enable_temporal_decay = true,
        .enable_positional_encoding = true,           // Enable position encoding
        .pe_type = NIMCP_POS_SINUSOIDAL,              // Sinusoidal (no training needed)
        .pe_embedding_dim = NIMCP_SMALL_EMBEDDING_DIM,                       // 64-dim position embeddings
        .enable_quantum_wm = true                     // Enable quantum retrieval
    };
    return config;
}


/**
 * @brief Get emotional tag of item (Phase 10.3)
 *
 * WHAT: Retrieve emotional context attached to working memory item
 * WHY:  Access emotional state for decision-making and memory retrieval
 * HOW:  Validate → Copy emotional tag to output
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @param index Item index [0, current_size)
 * @param emotion Output: emotional tag (non-NULL)
 * @return true on success, false on invalid index
 */
bool working_memory_get_emotion(
    const working_memory_t* wm,
    uint32_t index,
    emotional_tag_t* emotion
)
{
    // Guard: NULL working memory
    if (!wm) {
        set_error("NULL working_memory_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_get_emotion: wm is NULL");
        return false;
    }

    // Guard: NULL output
    if (!emotion) {
        set_error("NULL emotion output");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_get_emotion: emotion is NULL");
        return false;
    }

    // Lock mutex for thread-safe access
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&wm->mutex);

    // Guard: Invalid index
    if (index >= wm->current_size) {
        set_error("Index out of bounds");
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&wm->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_get_emotion: capacity exceeded");
        return false;
    }

    // Copy emotional tag (or neutral if none)
    if (wm->has_emotion[index]) {
        *emotion = wm->emotions[index];
    } else {
        *emotion = emotional_tag_neutral();
    }

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&wm->mutex);
    return true;
}


/**
 * @brief Get total salience including emotional boost (Phase 10.3)
 *
 * WHAT: Return effective salience (already boosted if emotion present)
 * WHY:  Priority decisions should use final salience value
 * HOW:  Return stored salience (pre-boosted during add_with_emotion)
 *
 * COMPLEXITY: O(1)
 *
 * DESIGN NOTE:
 * When items are added with working_memory_add_with_emotion(), the
 * emotional boost is computed and applied ONCE during insertion.
 * The stored salience is already the "total" salience.
 * This function is a convenience accessor that returns the stored value.
 *
 * @param wm Working memory instance (non-NULL)
 * @param index Item index [0, current_size)
 * @param total_salience Output: total salience value (non-NULL)
 * @return true on success, false on invalid index
 */
bool working_memory_get_total_salience(
    const working_memory_t* wm,
    uint32_t index,
    float* total_salience
)
{
    // Guard: NULL working memory
    if (!wm) {
        set_error("NULL working_memory_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_get_total_salience: wm is NULL");
        return false;
    }

    // Guard: NULL output
    if (!total_salience) {
        set_error("NULL total_salience output");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_get_total_salience: total_salience is NULL");
        return false;
    }

    // Lock mutex for thread-safe access
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&wm->mutex);

    // Guard: Invalid index
    if (index >= wm->current_size) {
        set_error("Index out of bounds");
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&wm->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_get_total_salience: capacity exceeded");
        return false;
    }

    // Return stored salience (already boosted if emotion present)
    *total_salience = wm->salience[index];

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&wm->mutex);
    return true;
}


// ============================================================================
// QUERY AND STATISTICS
// ============================================================================

/**
 * @brief Get current size of working memory
 *
 * WHAT: Return number of items currently stored
 * WHY:  Monitor buffer utilization
 * HOW:  Return current_size field
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @return Current item count, or 0 on error
 */
uint32_t working_memory_get_size(const working_memory_t* wm) {
    // Guard: NULL working memory
    if (!wm) {
        return 0;
    }

    // Lock mutex for thread-safe access (cast-away-const for mutex lock; size read is logically const)
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&wm->mutex);
    uint32_t size = wm->current_size;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&wm->mutex);

    return size;
}


/**
 * @brief Get current number of items (alias for get_size)
 *
 * WHAT: Return count of currently stored items
 * WHY:  Provide alternative naming for consistency with other APIs
 * HOW:  Call working_memory_get_size
 *
 * COMPLEXITY: O(1)
 */
uint32_t working_memory_get_count(const working_memory_t* wm) {
    return working_memory_get_size(wm);
}


/**
 * @brief Get working memory utilization percentage
 *
 * WHAT: Return percentage of capacity currently in use
 * WHY:  Monitor memory pressure and capacity usage
 * HOW:  Return (current_size / capacity) as float [0.0, 1.0]
 *
 * COMPLEXITY: O(1)
 */
float working_memory_get_utilization(working_memory_t* wm) {
    // Guard: NULL working memory
    if (!wm) {
        return 0.0F;
    }

    // Guard: Zero capacity (shouldn't happen but guard anyway)
    if (wm->capacity == 0) {
        return 0.0F;
    }

    // Lock mutex for thread-safe access
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&wm->mutex);
    float utilization = (float)wm->current_size / (float)wm->capacity;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&wm->mutex);

    return utilization;
}


/**
 * @brief Get capacity of working memory
 *
 * WHAT: Return maximum item capacity
 * WHY:  Determine buffer limits
 * HOW:  Return capacity field
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @return Maximum capacity, or 0 on error
 */
uint32_t working_memory_get_capacity(working_memory_t* wm) {
    // Guard: NULL working memory
    if (!wm) {
        return 0;
    }

    // Lock mutex for thread-safe access
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&wm->mutex);
    uint32_t capacity = wm->capacity;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&wm->mutex);

    return capacity;
}


/**
 * @brief Check if working memory is full
 *
 * WHAT: Test if buffer has reached capacity
 * WHY:  Determine if next add will trigger eviction
 * HOW:  Compare current_size to capacity
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @return true if full, false otherwise
 */
bool working_memory_is_full(working_memory_t* wm) {
    // Guard: NULL working memory
    if (!wm) {
        return false;
    }

    // Lock mutex for thread-safe access
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&wm->mutex);
    bool is_full = wm->current_size >= wm->capacity;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&wm->mutex);

    return is_full;
}


/**
 * @brief Get working memory statistics
 *
 * WHAT: Retrieve lifetime usage statistics
 * WHY:  Monitor performance and utilization patterns
 * HOW:  Copy internal statistics to output struct
 *
 * COMPLEXITY: O(n) for avg salience calculation
 *
 * @param wm Working memory instance (non-NULL)
 * @param stats Output statistics structure (non-NULL)
 */
void working_memory_get_stats(
    const working_memory_t* wm,
    working_memory_stats_t* stats
)
{
    // Guard: NULL pointers
    if (!wm || !stats) {
        return;
    }

    // Lock mutex for thread-safe access
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&wm->mutex);

    stats->current_size = wm->current_size;
    stats->capacity = wm->capacity;
    stats->total_additions = wm->total_additions;
    stats->total_evictions = wm->total_evictions;
    stats->total_refreshes = wm->total_refreshes;

    // Calculate average salience
    stats->avg_salience = 0.0F;
    if (wm->current_size > 0) {
        float sum = 0.0F;
        for (uint32_t i = 0; i < wm->current_size; i++) {
            sum += wm->salience[i];
        }
        stats->avg_salience = sum / wm->current_size;
    }

    // Find oldest item age
    stats->oldest_item_age_ms = 0.0F;
    if (wm->current_size > 0) {
        uint64_t current_time = get_current_time_ms();
        uint64_t oldest_time = wm->timestamps[0];
        for (uint32_t i = 1; i < wm->current_size; i++) {
            if (wm->timestamps[i] < oldest_time) {
                oldest_time = wm->timestamps[i];
            }
        }
        stats->oldest_item_age_ms = (float)(current_time - oldest_time);
    }

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&wm->mutex);
}


/**
 * @brief Get positional embedding for specific slot
 *
 * WHAT: Retrieve position encoding vector for a working memory slot
 * WHY:  Inspect position information, external position-aware processing
 * HOW:  Query internal positional encoder for slot's position encoding
 *
 * COMPLEXITY: O(1) if cached, O(d) if computed where d = embedding_dim
 *
 * BIOLOGICAL BASIS:
 * - Position codes in prefrontal working memory representations
 * - Enables comparison of relative positions between items
 * - Supports temporal reasoning over working memory contents
 *
 * @param wm Working memory instance (non-NULL)
 * @param slot_index Slot position [0, capacity)
 * @param output Output buffer for position embedding (pe_embedding_dim floats)
 * @return true on success, false on invalid slot or PE disabled
 */
bool working_memory_get_position_embedding(
    const working_memory_t* wm,
    uint32_t slot_index,
    float* output
)
{
    // Guard: NULL working memory
    if (!wm) {
        set_error("NULL working_memory_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_get_position_embedding: wm is NULL");
        return false;
    }

    // Guard: NULL output
    if (!output) {
        set_error("NULL output buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_get_position_embedding: output is NULL");
        return false;
    }

    // Guard: Positional encoding disabled
    if (!wm->enable_positional_encoding || !wm->pos_encoder) {
        set_error("Positional encoding disabled");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_get_position_embedding: required parameter is NULL (wm->enable_positional_encoding, wm->pos_encoder)");
        return false;
    }

    // Guard: Invalid slot index
    if (slot_index >= wm->capacity) {
        set_error("Slot index out of bounds");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "working_memory_get_position_embedding: capacity exceeded");
        return false;
    }

    // Get position encoding (no lock needed, encoder is thread-safe)
    int result = nimcp_pos_encode_position(wm->pos_encoder, slot_index, output);
    if (result != NIMCP_POS_SUCCESS) {
        set_error("Failed to encode position");
        LOG_ERROR("Position encoding failed for slot %u: error %d", slot_index, result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_get_position_embedding: validation failed");
        return false;
    }

    return true;
}


/**
 * @brief Configure positional encoding type
 *
 * WHAT: Change the type of positional encoding used
 * WHY:  Allow runtime switching between encoding strategies
 * HOW:  Destroy old encoder, create new encoder with specified type
 *
 * ALGORITHM:
 * 1. Validate new PE type
 * 2. Lock mutex for thread safety
 * 3. Destroy existing encoder
 * 4. Create new encoder with specified type
 * 5. Reapply encodings to existing items
 * 6. Unlock mutex
 *
 * COMPLEXITY: O(capacity × embedding_dim) - must rebuild encoder cache
 *
 * @param wm Working memory instance (non-NULL)
 * @param pe_type New positional encoding type
 * @return true on success, false on invalid type or allocation failure
 */
bool working_memory_set_pe_type(
    working_memory_t* wm,
    nimcp_pos_encoding_type_t pe_type
)
{
    // Guard: NULL working memory
    if (!wm) {
        set_error("NULL working_memory_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_set_pe_type: wm is NULL");
        return false;
    }

    // Guard: Positional encoding disabled
    if (!wm->enable_positional_encoding) {
        set_error("Positional encoding disabled");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_set_pe_type: wm->enable_positional_encoding is NULL");
        return false;
    }

    // Guard: Invalid PE type
    if (pe_type >= NIMCP_POS_TYPE_COUNT) {
        set_error("Invalid positional encoding type");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "working_memory_set_pe_type: capacity exceeded");
        return false;
    }

    // Lock mutex for thread-safe access
    nimcp_platform_mutex_lock(&wm->mutex);

    // If already using this type, nothing to do
    if (wm->pe_type == pe_type) {
        nimcp_platform_mutex_unlock(&wm->mutex);
        return true;
    }

    LOG_INFO("Switching PE type from %d to %d", wm->pe_type, pe_type);

    // Destroy old encoder
    if (wm->pos_encoder) {
        nimcp_pos_encoder_destroy(wm->pos_encoder);
        wm->pos_encoder = NULL;
    }

    // Create new encoder configuration
    nimcp_pos_config_t pe_config;
    pe_config.type = pe_type;

    // Configure based on type
    if (pe_type == NIMCP_POS_SINUSOIDAL) {
        pe_config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
        pe_config.config.sinusoidal.base.max_seq_length = wm->capacity;
        pe_config.config.sinusoidal.base.embedding_dim = wm->pe_embedding_dim;
        pe_config.config.sinusoidal.base.cache_enabled = true;
    } else if (pe_type == NIMCP_POS_RELATIVE) {
        pe_config.config.relative = nimcp_pos_relative_default_config();
        pe_config.config.relative.base.max_seq_length = wm->capacity;
        pe_config.config.relative.base.embedding_dim = wm->pe_embedding_dim;
        pe_config.config.relative.base.cache_enabled = true;
        pe_config.config.relative.max_relative_pos = wm->capacity;
    } else if (pe_type == NIMCP_POS_LEARNED) {
        pe_config.config.learned = nimcp_pos_learned_default_config();
        pe_config.config.learned.base.max_seq_length = wm->capacity;
        pe_config.config.learned.base.embedding_dim = wm->pe_embedding_dim;
        pe_config.config.learned.base.cache_enabled = true;
    } else if (pe_type == NIMCP_POS_ROTARY) {
        pe_config.config.rope = nimcp_pos_rope_default_config();
        pe_config.config.rope.base.max_seq_length = wm->capacity;
        pe_config.config.rope.base.embedding_dim = wm->pe_embedding_dim;
        pe_config.config.rope.base.cache_enabled = true;
    } else if (pe_type == NIMCP_POS_ALIBI) {
        pe_config.config.alibi = nimcp_pos_alibi_default_config();
        pe_config.config.alibi.base.max_seq_length = wm->capacity;
        pe_config.config.alibi.base.embedding_dim = wm->pe_embedding_dim;
        pe_config.config.alibi.base.cache_enabled = true;
    } else {
        // Unsupported type, use sinusoidal as fallback
        LOG_WARN("Unsupported PE type %d, using SINUSOIDAL", pe_type);
        pe_config.type = NIMCP_POS_SINUSOIDAL;
        pe_config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
        pe_config.config.sinusoidal.base.max_seq_length = wm->capacity;
        pe_config.config.sinusoidal.base.embedding_dim = wm->pe_embedding_dim;
        pe_config.config.sinusoidal.base.cache_enabled = true;
        pe_type = NIMCP_POS_SINUSOIDAL;
    }

    // Create new encoder
    wm->pos_encoder = nimcp_pos_encoder_create(&pe_config);
    if (!wm->pos_encoder) {
        set_error("Failed to create new positional encoder");
        LOG_ERROR("Failed to create PE encoder for type %d", pe_type);
        wm->enable_positional_encoding = false;
        nimcp_platform_mutex_unlock(&wm->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_set_pe_type: wm->pos_encoder is NULL");
        return false;
    }

    // Update PE type
    wm->pe_type = pe_type;

    LOG_INFO("Successfully switched to PE type %d", pe_type);

    nimcp_platform_mutex_unlock(&wm->mutex);
    return true;
}


uint32_t working_memory_get_effective_capacity(const working_memory_t* wm)
{
    // WHAT: Get current capacity after inflammation and sleep penalties
    // WHY:  Check available slots for new items
    // HOW:  base_capacity × sleep_factor - inflammation_penalty, minimum 3

    if (!wm) {
        return 0;
    }

    // Start with base capacity
    float effective = (float)wm->capacity;

    // Apply sleep state modulation
    float sleep_capacity_factor = working_memory_sleep_capacity_for_state(wm->current_sleep_state);
    effective *= sleep_capacity_factor;

    // Apply inflammation penalty
    if (wm->immune_integration_enabled) {
        effective -= (float)wm->inflammation_capacity_penalty;
    }

    // Floor to integer, minimum 3 items
    uint32_t final_capacity = (uint32_t)effective;
    if (final_capacity < 3) {
        final_capacity = 3;
    }

    return final_capacity;
}


bool working_memory_is_immune_impaired(const working_memory_t* wm)
{
    // WHAT: Check if inflammation is reducing capacity
    // WHY:  Detect cognitive impairment
    // HOW:  Compare effective to base capacity

    if (!wm) {
        return false;
    }

    return wm->immune_integration_enabled && wm->inflammation_capacity_penalty > 0;
}


//=============================================================================
// SLEEP STATE INTEGRATION
//=============================================================================

/**
 * @brief Set sleep state for working memory modulation
 *
 * WHAT: Update current sleep state to modulate WM capacity and decay
 * WHY:  Sleep state affects working memory performance (biological)
 * HOW:  Store state, apply modulation factors from sleep bridge
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full WM capacity (~7±2 items), normal decay
 * - DROWSY: Reduced capacity (~5 items), faster decay
 * - NREM: Minimal capacity (offline processing)
 * - REM: Limited capacity (dream narrative)
 *
 * @param wm Working memory instance (non-NULL)
 * @param state New sleep state
 * @return true on success, false on NULL pointer
 */
bool working_memory_set_sleep_state(working_memory_t* wm, sleep_state_t state)
{
    if (!wm) {
        set_error("NULL working_memory_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_set_sleep_state: wm is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(&wm->mutex);
    wm->current_sleep_state = state;
    nimcp_platform_mutex_unlock(&wm->mutex);

    LOG_DEBUG("WM sleep state changed to %d", state);
    return true;
}


/**
 * @brief Get current sleep state
 *
 * WHAT: Query current sleep/wake state
 * WHY:  Check what modulation is being applied
 * HOW:  Return current_sleep_state field
 *
 * @param wm Working memory instance (non-NULL)
 * @return Current sleep state, or SLEEP_STATE_AWAKE if NULL
 */
sleep_state_t working_memory_get_sleep_state(working_memory_t* wm)
{
    if (!wm) {
        return SLEEP_STATE_AWAKE;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&wm->mutex);
    sleep_state_t state = wm->current_sleep_state;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&wm->mutex);

    return state;
}


//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * @brief Check if working memory is empty
 *
 * WHAT: Returns true if no items are stored
 * WHY:  Useful for quick emptiness checks before operations
 * HOW:  Check current_size == 0
 *
 * @param wm Working memory instance
 * @return true if empty, false if has items or NULL
 */
bool working_memory_is_empty(const working_memory_t* wm)
{
    if (!wm) {
        return true;  // NULL is considered empty
    }

    return wm->current_size == 0;
}


/**
 * @brief Get salience value for an item at given index
 *
 * WHAT: Retrieve the salience (importance) score for a specific item
 * WHY:  Allow external modules to query item priorities
 * HOW:  Direct array access with bounds checking
 *
 * @param wm Working memory instance (non-NULL)
 * @param index Item index [0, current_size)
 * @param salience Output: salience value [0.0, 1.0] (non-NULL)
 * @return true on success, false on invalid index or NULL params
 */
bool working_memory_get_salience(
    const working_memory_t* wm,
    uint32_t index,
    float* salience
)
{
    // Guard: NULL working memory
    if (!wm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_get_salience: wm is NULL");
        return false;
    }

    // Guard: NULL output
    if (!salience) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_get_salience: salience is NULL");
        return false;
    }

    // Thread-safe access — bounds check INSIDE lock to prevent TOCTOU
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&wm->mutex);
    if (index >= wm->current_size) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&wm->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_get_salience: capacity exceeded");
        return false;
    }
    *salience = wm->salience[index];
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&wm->mutex);

    return true;
}


/**
 * @brief Set salience of item at index
 *
 * WHAT: Update importance score for an item
 * WHY:  Allow dynamic priority adjustment based on relevance
 * HOW:  Direct array access with bounds checking and clamping
 *
 * @param wm Working memory instance (non-NULL)
 * @param index Item index [0, current_size)
 * @param new_salience New salience value (clamped to [0.0, 1.0])
 * @return true on success, false on invalid index or NULL params
 */
bool working_memory_set_salience(
    working_memory_t* wm,
    uint32_t index,
    float new_salience
)
{
    // Guard: NULL working memory
    if (!wm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_set_salience: wm is NULL");
        return false;
    }

    // Clamp salience to valid range
    if (new_salience < 0.0f) {
        new_salience = 0.0f;
    } else if (new_salience > 1.0f) {
        new_salience = 1.0f;
    }

    // Thread-safe write — bounds check INSIDE lock to prevent TOCTOU
    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&wm->mutex);
    if (index >= wm->current_size) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&wm->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_set_salience: capacity exceeded");
        return false;
    }
    wm->salience[index] = new_salience;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&wm->mutex);

    return true;
}


/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void working_memory_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_working_memory_health_agent = agent;
    }
}
