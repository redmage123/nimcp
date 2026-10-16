// nimcp_working_memory_part_lifecycle.c - lifecycle functions
// Part of nimcp_working_memory.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_working_memory.c


/**
 * @brief Create working memory with default configuration
 *
 * WHAT: Allocate and initialize working memory buffer
 * WHY:  Provide simple creation for standard use cases
 * HOW:  Delegate to custom creation with default config
 *
 * COMPLEXITY: O(capacity) for array allocation
 * MEMORY: ~capacity × (ptr + uint32 + float + uint64 + bool) bytes
 *
 * @return New working memory instance, or NULL on allocation failure
 */
working_memory_t* working_memory_create(void) {
    working_memory_config_t config = working_memory_default_config();
    return working_memory_create_custom(&config);
}


/**
 * @brief Create working memory with custom configuration
 *
 * WHAT: Allocate and initialize working memory with custom parameters
 * WHY:  Allow experimentation with non-standard capacities and decay
 * HOW:  Validate config → Allocate struct → Allocate arrays → Initialize
 *
 * COMPLEXITY: O(capacity) for array allocation
 * MEMORY: capacity × (24 bytes per item + item data)
 *
 * @param config Configuration parameters (non-NULL)
 * @return New working memory instance, or NULL on error
 */
working_memory_t* working_memory_create_custom(
    const working_memory_config_t* config
)
{
    // Guard: NULL config
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_create_custom: config is NULL");
        set_error("NULL config");
        LOG_ERROR("NULL config provided to working_memory_create_custom");
        return NULL;
    }

    // Guard: Invalid capacity
    if (config->capacity < MIN_CAPACITY || config->capacity > MAX_CAPACITY) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_create_custom: invalid capacity");
        set_error("Invalid capacity (must be 1-32)");
        LOG_ERROR("Invalid capacity: %u (must be %d-%d)", config->capacity, MIN_CAPACITY, MAX_CAPACITY);
        return NULL;
    }

    // Guard: Invalid decay tau
    if (config->decay_tau_ms <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_create_custom: invalid decay_tau_ms");
        set_error("Invalid decay_tau_ms (must be > 0)");
        LOG_ERROR("Invalid decay_tau_ms: %.2f (must be > 0)", config->decay_tau_ms);
        return NULL;
    }

    LOG_INFO("Creating working memory: capacity=%u, decay_tau_ms=%.2f",
             config->capacity, config->decay_tau_ms);

    // Allocate main structure
    working_memory_t* wm = nimcp_calloc(1, sizeof(working_memory_t));
    if (!wm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "working_memory_create_custom: failed to allocate wm");
        set_error("Failed to allocate working_memory_t");
        LOG_ERROR("Failed to allocate working_memory_t (%zu bytes)", sizeof(working_memory_t));
        return NULL;
    }

    // Allocate arrays — do NOT early-return on individual failures.
    // Each calloc may return NULL; the combined check below handles cleanup.
    wm->items = nimcp_calloc(config->capacity, sizeof(float*));
    wm->item_sizes = nimcp_calloc(config->capacity, sizeof(uint32_t));
    wm->salience = nimcp_calloc(config->capacity, sizeof(float));
    wm->timestamps = nimcp_calloc(config->capacity, sizeof(uint64_t));
    wm->attention_refreshed = nimcp_calloc(config->capacity, sizeof(bool));
    wm->emotions = nimcp_calloc(config->capacity, sizeof(emotional_tag_t));  // Phase 10.3
    wm->has_emotion = nimcp_calloc(config->capacity, sizeof(bool));          // Phase 10.3

    // Check all allocations — working_memory_destroy handles partially-allocated state
    if (!wm->items || !wm->item_sizes || !wm->salience ||
        !wm->timestamps || !wm->attention_refreshed ||
        !wm->emotions || !wm->has_emotion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "working_memory_create_custom: failed to allocate arrays");
        set_error("Failed to allocate arrays");
        working_memory_destroy(wm);
        return NULL;
    }

    // Initialize configuration
    wm->capacity = config->capacity;
    wm->current_size = 0;
    wm->decay_tau_ms = config->decay_tau_ms;
    wm->min_salience = config->min_salience;
    wm->enable_attention_refresh = config->enable_attention_refresh;
    wm->enable_temporal_decay = config->enable_temporal_decay;

    // Initialize statistics
    wm->total_additions = 0;
    wm->total_evictions = 0;
    wm->total_refreshes = 0;
    wm->total_decay_removals = 0;

    // Initialize mutex for thread safety
    if (nimcp_platform_mutex_init(&wm->mutex, false) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "working_memory_create_custom: failed to init mutex");
        set_error("Failed to initialize mutex");
        working_memory_destroy(wm);
        return NULL;
    }

    // Bio-async registration
    wm->bio_ctx = NULL;
    wm->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_WORKING_MEMORY,
            .module_name = "working_memory",
            .inbox_capacity = 32,
            .user_data = wm
        };
        wm->bio_ctx = bio_router_register_module(&bio_info);
        if (wm->bio_ctx) {
            wm->bio_async_enabled = true;

            // Try KG-driven wiring callback registration first
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_WORKING_MEMORY,
                (void*)working_memory_wiring_handler_callback,
                wm
            );

            if (wiring_result == NIMCP_SUCCESS) {
                LOG_INFO("Working memory: KG-driven wiring callback registered");
            } else {
                // Legacy fallback - register handlers directly
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(wm->bio_ctx, BIO_MSG_WORKING_MEMORY_STORE, handle_wm_store_request);
                    bio_router_register_handler(wm->bio_ctx, BIO_MSG_WORKING_MEMORY_RETRIEVE, handle_wm_retrieve_request);
                    bio_router_register_handler(wm->bio_ctx, BIO_MSG_NEUROMODULATOR_RELEASE, handle_wm_store_request)
                );
                LOG_INFO("Working memory: legacy handler registration");
            }
        }
    }

    // Second messenger integration (optional, must be set via working_memory_integrate_second_messengers)
    wm->sm_system = NULL;
    wm->enable_second_messengers = false;
    wm->num_neurons = 0;

    // Initialize positional encoding
    wm->pos_encoder = NULL;
    wm->pe_buffer = NULL;
    wm->enable_positional_encoding = config->enable_positional_encoding;
    wm->pe_type = config->pe_type;
    wm->pe_embedding_dim = config->pe_embedding_dim;

    // Initialize sleep state (default: awake)
    wm->current_sleep_state = SLEEP_STATE_AWAKE;

    if (wm->enable_positional_encoding) {
        // Create positional encoder configuration
        nimcp_pos_config_t pe_config;
        pe_config.type = config->pe_type;

        // Configure based on type
        if (config->pe_type == NIMCP_POS_SINUSOIDAL) {
            pe_config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
            pe_config.config.sinusoidal.base.max_seq_length = config->capacity;
            pe_config.config.sinusoidal.base.embedding_dim = config->pe_embedding_dim;
            pe_config.config.sinusoidal.base.cache_enabled = true;
        } else if (config->pe_type == NIMCP_POS_RELATIVE) {
            pe_config.config.relative = nimcp_pos_relative_default_config();
            pe_config.config.relative.base.max_seq_length = config->capacity;
            pe_config.config.relative.base.embedding_dim = config->pe_embedding_dim;
            pe_config.config.relative.base.cache_enabled = true;
            pe_config.config.relative.max_relative_pos = config->capacity;
        } else {
            // For other types, use sinusoidal as fallback
            LOG_WARN("Unsupported PE type %d, using SINUSOIDAL", config->pe_type);
            pe_config.type = NIMCP_POS_SINUSOIDAL;
            pe_config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
            pe_config.config.sinusoidal.base.max_seq_length = config->capacity;
            pe_config.config.sinusoidal.base.embedding_dim = config->pe_embedding_dim;
            pe_config.config.sinusoidal.base.cache_enabled = true;
            wm->pe_type = NIMCP_POS_SINUSOIDAL;
        }

        // Create encoder
        wm->pos_encoder = nimcp_pos_encoder_create(&pe_config);
        if (!wm->pos_encoder) {
            LOG_ERROR("Failed to create positional encoder");
            working_memory_destroy(wm);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "working_memory_create_custom: wm->pos_encoder is NULL");
            return NULL;
        }

        // Allocate PE buffer for temporary position encodings
        wm->pe_buffer = nimcp_calloc(config->pe_embedding_dim, sizeof(float));
        if (!wm->pe_buffer) {
            LOG_ERROR("Failed to allocate PE buffer");
            working_memory_destroy(wm);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "working_memory_create_custom: wm->pe_buffer is NULL");
            return NULL;
        }

        LOG_INFO("Positional encoding enabled: type=%d, dim=%u",
                 wm->pe_type, wm->pe_embedding_dim);
    }

    // Initialize quantum retrieval bridge
    wm->quantum_bridge = NULL;
    wm->enable_quantum_wm = config->enable_quantum_wm;

    if (wm->enable_quantum_wm) {
        working_memory_quantum_config_t qconfig = working_memory_quantum_default_config();
        qconfig.max_items = config->capacity;
        qconfig.item_embedding_dim = config->pe_embedding_dim;  // Use same dim as PE

        wm->quantum_bridge = working_memory_quantum_bridge_create(&qconfig);
        if (!wm->quantum_bridge) {
            LOG_ERROR("Failed to create quantum bridge");
            working_memory_destroy(wm);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "working_memory_create_custom: wm->quantum_bridge is NULL");
            return NULL;
        }

        LOG_INFO("Quantum working memory bridge enabled: max_items=%u, embedding_dim=%u",
                 qconfig.max_items, qconfig.item_embedding_dim);
    }

    // Initialize SNN and Plasticity bridges
    wm->snn_bridge = NULL;
    wm->plasticity_bridge = NULL;
    wm->bridges_enabled = false;

    // Create SNN bridge with default config
    wm_snn_config_t snn_config = wm_snn_config_default();
    snn_config.max_slots = config->capacity;
    wm->snn_bridge = wm_snn_create(&snn_config);
    if (!wm->snn_bridge) {
        LOG_WARN("Failed to create SNN bridge - continuing without SNN integration");
    }

    // Create Plasticity bridge with default config
    wm_plasticity_config_t plasticity_config = wm_plasticity_config_default();
    wm->plasticity_bridge = wm_plasticity_create(&plasticity_config);
    if (!wm->plasticity_bridge) {
        LOG_WARN("Failed to create Plasticity bridge - continuing without plasticity integration");
    }

    // Mark bridges as enabled if at least one succeeded
    if (wm->snn_bridge || wm->plasticity_bridge) {
        wm->bridges_enabled = true;
        LOG_INFO("SNN/Plasticity bridges enabled: snn=%s, plasticity=%s",
                 wm->snn_bridge ? "yes" : "no",
                 wm->plasticity_bridge ? "yes" : "no");
    }

    return wm;
}


/**
 * @brief Destroy working memory and free all resources
 *
 * WHAT: Free all allocated memory
 * WHY:  Prevent memory leaks
 * HOW:  Free items → Free arrays → Free struct
 *
 * COMPLEXITY: O(n) where n = current_size
 *
 * @param wm Working memory instance (nullable)
 */
void working_memory_destroy(working_memory_t* wm) {
    // Guard: NULL pointer (safe to call on NULL)
    if (!wm) {
        return;
    }

    // Free all items
    if (wm->items) {
        for (uint32_t i = 0; i < wm->current_size; i++) {
            nimcp_free(wm->items[i]);
        }
        nimcp_free(wm->items);
    }

    // Free arrays
    nimcp_free(wm->item_sizes);
    nimcp_free(wm->salience);
    nimcp_free(wm->timestamps);
    nimcp_free(wm->attention_refreshed);
    nimcp_free(wm->emotions);      // Phase 10.3
    nimcp_free(wm->has_emotion);   // Phase 10.3

    // Unregister from bio-router
    if (wm->bio_async_enabled && wm->bio_ctx) {
        bio_router_unregister_module(wm->bio_ctx);
        wm->bio_ctx = NULL;
        wm->bio_async_enabled = false;
    }

    // Destroy positional encoder
    if (wm->pos_encoder) {
        nimcp_pos_encoder_destroy(wm->pos_encoder);
        wm->pos_encoder = NULL;
    }
    if (wm->pe_buffer) {
        nimcp_free(wm->pe_buffer);
        wm->pe_buffer = NULL;
    }

    // Destroy quantum bridge
    if (wm->quantum_bridge) {
        working_memory_quantum_bridge_destroy(wm->quantum_bridge);
        wm->quantum_bridge = NULL;
    }

    // Destroy SNN and Plasticity bridges
    if (wm->snn_bridge) {
        wm_snn_destroy(wm->snn_bridge);
        wm->snn_bridge = NULL;
    }
    if (wm->plasticity_bridge) {
        wm_plasticity_destroy(wm->plasticity_bridge);
        wm->plasticity_bridge = NULL;
    }
    wm->bridges_enabled = false;

    // Destroy mutex
    nimcp_platform_mutex_destroy(&wm->mutex);

    // Free main structure
    nimcp_free(wm);
    wm = NULL;
}
