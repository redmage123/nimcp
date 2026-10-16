// nimcp_salience_part_lifecycle.c - lifecycle functions
// Part of nimcp_salience.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_salience.c

static history_buffer_t* history_buffer_create(uint32_t capacity)
{
    history_buffer_t* hist = nimcp_calloc(1, sizeof(history_buffer_t));
    if (!hist) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "history_buffer_create: hist is NULL");
        return NULL;
    }

    hist->entries = nimcp_calloc(capacity, sizeof(history_entry_t));
    if (!hist->entries) {
        nimcp_free(hist);
        hist = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "history_buffer_create: hist->entries is NULL");
        return NULL;
    }

    hist->capacity = capacity;
    hist->head = 0;
    hist->count = 0;
    nimcp_mutex_init(&hist->lock, NULL);

    return hist;
}


/**
 * WHAT: Destroy history buffer
 * WHY: Free all resources
 * HOW: Free entries array, then structure (features are stack-allocated in entries)
 */
static void history_buffer_destroy(history_buffer_t* hist)
{
    if (!hist)
        return;

    /**
     * WHAT: Free entries array
     * WHY: No per-entry cleanup needed - features are fixed-size arrays
     * NOTE: Simpler than before - no malloc/free per entry
     */
    if (hist->entries) {
        nimcp_free(hist->entries);
    }

    nimcp_mutex_destroy(&hist->lock);
    nimcp_free(hist);
    hist = NULL;
}

static predictor_t* predictor_create(uint32_t num_features)
{
    predictor_t* pred = nimcp_calloc(1, sizeof(predictor_t));
    if (!pred) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictor_create: pred is NULL");
        return NULL;
    }

    pred->prediction = nimcp_calloc(num_features, sizeof(float));
    if (!pred->prediction) {
        nimcp_free(pred);
        pred = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictor_create: pred->prediction is NULL");
        return NULL;
    }

    pred->num_features = num_features;
    pred->alpha = 0.3F;  // 30% new, 70% old
    pred->initialized = false;
    nimcp_mutex_init(&pred->lock, NULL);

    return pred;
}


/**
 * WHAT: Destroy predictor
 */
static void predictor_destroy(predictor_t* pred)
{
    if (!pred)
        return;

    if (pred->prediction) {
        nimcp_free(pred->prediction);
    }

    nimcp_mutex_destroy(&pred->lock);
    nimcp_free(pred);
    pred = NULL;
}


salience_evaluator_t salience_evaluator_create(brain_t brain, const salience_config_t* config)
{
    // Guard clauses
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_evaluator_create: brain is NULL");
        salience_set_error("NULL brain");
        return NULL;
    }

    if (!validate_salience_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "salience_evaluator_create: invalid config");
        return NULL;
    }

    // Allocate evaluator
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_evaluator_create", 0.0f);


    salience_evaluator_t eval = nimcp_calloc(1, sizeof(struct salience_evaluator_struct));
    if (!eval) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "salience_evaluator_create: failed to allocate eval");
        salience_set_error("Failed to allocate evaluator");
        return NULL;
    }

    eval->brain = brain;
    memcpy(&eval->config, config, sizeof(salience_config_t));
    eval->bio_ctx = NULL;
    eval->bio_async_enabled = false;

    // Register with bio-async router if enabled
    if (config->enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_SALIENCE,
            .module_name = "salience",
            .inbox_capacity = 64,
            .user_data = eval
        };
        eval->bio_ctx = bio_router_register_module(&bio_info);
        if (eval->bio_ctx) {
            eval->bio_async_enabled = true;

            // Try KG-driven wiring callback registration first
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_SALIENCE,
                (void*)salience_wiring_handler_callback,
                eval
            );

            if (wiring_result == NIMCP_SUCCESS) {
                LOG_INFO("Salience: KG-driven wiring callback registered");
            } else {
                // Legacy fallback - register handlers directly
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(eval->bio_ctx, BIO_MSG_SALIENCE_QUERY, handle_salience_query)
                );
                LOG_INFO("Salience: legacy handler registration");
            }
        } else {
            LOG_WARN("Bio-async registration failed");
        }
    }

    // Create history buffer if novelty enabled
    if (config->enable_novelty && config->history_size > 0) {
        eval->history = history_buffer_create(config->history_size);
        if (!eval->history) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "salience_evaluator_create: failed to create history");
            salience_set_error("Failed to create history buffer");
            nimcp_free(eval);
            eval = NULL;
            return NULL;
        }
    }

    // Create predictor if surprise enabled
    if (config->enable_surprise) {
        // Get num_features from brain configuration via getter
        uint32_t num_features = brain_get_num_inputs(brain);
        eval->predictor = predictor_create(num_features);
        if (!eval->predictor) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "salience_evaluator_create: failed to create predictor");
            salience_set_error("Failed to create predictor");
            if (eval->history)
                history_buffer_destroy(eval->history);
            nimcp_free(eval);
            eval = NULL;
            return NULL;
        }
    }

    // Initialize statistics
    eval->stats_evaluations = 0;
    eval->stats_high_salience = 0;
    eval->stats_high_novelty = 0;
    eval->stats_high_surprise = 0;
    eval->stats_high_urgency = 0;

    eval->running_avg_salience = 0.0F;
    eval->running_avg_novelty = 0.0F;
    eval->running_avg_surprise = 0.0F;
    eval->running_avg_urgency = 0.0F;

    eval->total_eval_time_us = 0;

    // Initialize mutex
    nimcp_mutex_init(&eval->eval_lock, NULL);

    /**
     * WHAT: Create thread pool for batch processing
     * WHY: Parallel evaluation significantly improves batch performance
     * HOW: 4 worker threads for optimal CPU utilization
     */
    eval->thread_pool = nimcp_pool_create(4);
    if (!eval->thread_pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "salience_evaluator_create: failed to create thread_pool");
        salience_set_error("Failed to create thread pool");
        if (eval->history)
            history_buffer_destroy(eval->history);
        if (eval->predictor)
            predictor_destroy(eval->predictor);
        nimcp_mutex_destroy(&eval->eval_lock);
        nimcp_free(eval);
        eval = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_evaluator_create: validation failed");
        return NULL;
    }

    // Phase 1.5: Initialize memory pool for history entries
    memory_pool_config_t history_pool_config = {
        .block_size = sizeof(history_entry_t),
        .num_blocks = config->history_size > 0 ? config->history_size : 32,
        .alignment = 16,  // SIMD alignment
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    eval->history_entry_pool = memory_pool_create(&history_pool_config);

    // Initialize cross-modal fusion state
    for (int i = 0; i < SALIENCE_MODALITY_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && SALIENCE_MODALITY_COUNT > 256) {
            salience_heartbeat("salience_loop",
                             (float)(i + 1) / (float)SALIENCE_MODALITY_COUNT);
        }

        eval->modalities[i].salience = (brain_salience_t){0};
        eval->modalities[i].weight = config->default_modality_weight;
        eval->modalities[i].active = false;
        eval->modalities[i].has_data = false;
    }
    eval->fusion_strategy = config->fusion_strategy;

    // Initialize SNN and Plasticity bridges
    eval->snn_bridge = NULL;
    eval->plasticity_bridge = NULL;
    eval->bridges_enabled = false;

    // Create SNN bridge with default config
    salience_snn_config_t snn_config = salience_snn_config_default();
    eval->snn_bridge = salience_snn_create(&snn_config);

    // Create Plasticity bridge with default config
    salience_plasticity_config_t plasticity_config = salience_plasticity_config_default();
    eval->plasticity_bridge = salience_plasticity_create(&plasticity_config);

    // Mark bridges enabled if both created successfully
    if (eval->snn_bridge && eval->plasticity_bridge) {
        eval->bridges_enabled = true;
        LOG_INFO("salience: SNN and Plasticity bridges enabled");
    } else {
        LOG_WARN("salience: Bridges partially or not created (SNN=%p, Plasticity=%p)",
                 (void*)eval->snn_bridge, (void*)eval->plasticity_bridge);
    }

    return eval;
}


void salience_evaluator_destroy(salience_evaluator_t eval)
{
    if (!eval)
        return;

    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_evaluator_destroy", 0.0f);

    // Unregister from bio-async router
    if (eval->bio_async_enabled && eval->bio_ctx) {
        bio_router_unregister_module(eval->bio_ctx);
        eval->bio_ctx = NULL;
        eval->bio_async_enabled = false;
        LOG_INFO("Bio-async communication disabled");
    }

    /**
     * WHAT: Destroy thread pool first
     * WHY: Ensures all pending tasks complete before cleanup
     * HOW: nimcp_pool_destroy waits for completion
     */
    if (eval->thread_pool) {
        nimcp_pool_destroy(eval->thread_pool);
    }

    if (eval->history) {
        history_buffer_destroy(eval->history);
    }

    if (eval->predictor) {
        predictor_destroy(eval->predictor);
    }

    // Phase 1.5: Destroy memory pool
    if (eval->history_entry_pool) {
        memory_pool_destroy(eval->history_entry_pool);
    }

    // Destroy SNN and Plasticity bridges
    if (eval->snn_bridge) {
        salience_snn_destroy(eval->snn_bridge);
        eval->snn_bridge = NULL;
    }
    if (eval->plasticity_bridge) {
        salience_plasticity_destroy(eval->plasticity_bridge);
        eval->plasticity_bridge = NULL;
    }
    eval->bridges_enabled = false;

    nimcp_mutex_destroy(&eval->eval_lock);

    nimcp_free(eval);
    eval = NULL;
}


/**
 * WHAT: Reset salience statistics
 * WHY: Allow clearing counters for fresh measurements
 * HOW: Resets all statistics counters to zero
 *
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param eval Evaluator handle
 * @return true on success, false on error
 */
bool salience_reset_stats(salience_evaluator_t eval)
{
    if (!eval) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_reset_stats: eval is NULL");
        return false;
    }

    /**
     * WHAT: Reset all statistic counters
     * WHY: Provide clean slate for new measurement period
     * HOW: Mutex-protected zero assignment to all counters
     */
    /* Phase 8: Heartbeat at operation start */
    salience_heartbeat("salience_reset_stats", 0.0f);


    nimcp_mutex_lock(&eval->eval_lock);

    eval->stats_evaluations = 0;
    eval->stats_high_salience = 0;
    eval->stats_high_novelty = 0;
    eval->stats_high_surprise = 0;
    eval->stats_high_urgency = 0;

    eval->running_avg_salience = 0.0F;
    eval->running_avg_novelty = 0.0F;
    eval->running_avg_surprise = 0.0F;
    eval->running_avg_urgency = 0.0F;

    eval->total_eval_time_us = 0;

    nimcp_mutex_unlock(&eval->eval_lock);

    return true;
}
